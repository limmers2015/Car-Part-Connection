// src/main.c
#define _POSIX_C_SOURCE 200809L
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <stdbool.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

/* -------------------- Config helpers -------------------- */

static const char* getenv_or(const char* key, const char* def) {
    const char* v = getenv(key);
    return (v && *v) ? v : def;
}

static int getenv_int_or(const char* key, int def) {
    const char* v = getenv(key);
    if (!v || !*v) return def;
    char* end = NULL;
    long n = strtol(v, &end, 10);
    if (end == v || n <= 0 || n > 65535) return def;
    return (int)n;
}

static bool getenv_bool_or(const char* key, bool def) {
    const char* v = getenv(key);
    if (!v) return def;
    if (!strcasecmp(v, "1") || !strcasecmp(v, "true") || !strcasecmp(v, "yes")) return true;
    if (!strcasecmp(v, "0") || !strcasecmp(v, "false") || !strcasecmp(v, "no")) return false;
    return def;
}

/* -------------------- UUID v4 (best-effort) -------------------- */

static void uuid4(char out[37]) {
    // Try /dev/urandom; if not, fall back to rand().
    unsigned char r[16];
    FILE* f = fopen("/dev/urandom", "rb");
    if (f) {
        fread(r, 1, 16, f);
        fclose(f);
    } else {
        srand((unsigned)time(NULL));
        for (int i = 0; i < 16; i++) r[i] = (unsigned char)(rand() & 0xFF);
    }
    // Set version (4) and variant (10b)
    r[6] = (r[6] & 0x0F) | 0x40;
    r[8] = (r[8] & 0x3F) | 0x80;

    snprintf(out, 37,
             "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
             r[0], r[1], r[2], r[3], r[4], r[5], r[6], r[7],
             r[8], r[9], r[10], r[11], r[12], r[13], r[14], r[15]);
}

/* -------------------- Logging -------------------- */

static bool LOG_JSON = true;

static void log_json(const char* level, const char* req_id, const char* fmt, ...) {
    char msg[1024];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(msg, sizeof msg, fmt, ap);
    va_end(ap);

    time_t now = time(NULL);
    struct tm t;
    gmtime_r(&now, &t);
    char ts[64];
    strftime(ts, sizeof ts, "%Y-%m-%dT%H:%M:%SZ", &t);

    fprintf(stderr,
        "{\"ts\":\"%s\",\"level\":\"%s\",\"request_id\":\"%s\",\"msg\":\"",
        ts, level, req_id ? req_id : "");
    // escape quotes very lightly
    for (const char* p = msg; *p; ++p) {
        if (*p == '"' || *p == '\\') fputc('\\', stderr);
        fputc(*p, stderr);
    }
    fprintf(stderr, "\"}\n");
}

static void log_text(const char* level, const char* req_id, const char* fmt, ...) {
    char msg[1024];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(msg, sizeof msg, fmt, ap);
    va_end(ap);

    fprintf(stderr, "[%s] %s %s\n", level, req_id ? req_id : "", msg);
}

static void log_info(const char* req_id, const char* fmt, ...) {
    char buf[1024];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);
    if (LOG_JSON) log_json("INFO", req_id, "%s", buf);
    else log_text("INFO", req_id, "%s", buf);
}

static void log_error(const char* req_id, const char* fmt, ...) {
    char buf[1024];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);
    if (LOG_JSON) log_json("ERROR", req_id, "%s", buf);
    else log_text("ERROR", req_id, "%s", buf);
}

/* -------------------- HTTP helpers -------------------- */

static const char* http_200 =
    "HTTP/1.1 200 OK\r\n"
    "Content-Type: application/json; charset=utf-8\r\n"
    "Connection: close\r\n"
    "\r\n";

static const char* http_404 =
    "HTTP/1.1 404 Not Found\r\n"
    "Content-Type: application/json; charset=utf-8\r\n"
    "Connection: close\r\n"
    "\r\n";

static const char* http_405 =
    "HTTP/1.1 405 Method Not Allowed\r\n"
    "Content-Type: application/json; charset=utf-8\r\n"
    "Allow: GET\r\n"
    "Connection: close\r\n"
    "\r\n";

static void send_health(int client_fd, const char* req_id) {
    char body[256];
    const char* app_env = getenv_or("APP_ENV", "dev");
    snprintf(body, sizeof body,
        "{\"status\":\"ok\",\"request_id\":\"%s\",\"env\":\"%s\"}\n",
        req_id, app_env);

    send(client_fd, http_200, strlen(http_200), 0);
    send(client_fd, body, strlen(body), 0);
}

static void send_404(int client_fd, const char* req_id) {
    (void)req_id;
    const char* body = "{\"error\":\"not_found\"}\n";
    send(client_fd, http_404, strlen(http_404), 0);
    send(client_fd, body, strlen(body), 0);
}

static void send_405(int client_fd, const char* req_id) {
    (void)req_id;
    const char* body = "{\"error\":\"method_not_allowed\"}\n";
    send(client_fd, http_405, strlen(http_405), 0);
    send(client_fd, body, strlen(body), 0);
}

/* Very small request line parser: "GET /path HTTP/1.1" */
static void handle_client(int client_fd, struct sockaddr_in* addr) {
    char req_id[37]; uuid4(req_id);

    char buf[4096];
    ssize_t n = recv(client_fd, buf, sizeof(buf)-1, 0);
    if (n <= 0) {
        log_error(req_id, "recv failed: %s", strerror(errno));
        return;
    }
    buf[n] = '\0';

    // Extract first line
    char method[8] = {0}, path[1024] = {0};
    if (sscanf(buf, "%7s %1023s", method, path) != 2) {
        log_error(req_id, "bad request line");
        send_404(client_fd, req_id);
        return;
    }

    char ip[64];
    inet_ntop(AF_INET, &addr->sin_addr, ip, sizeof ip);
    log_info(req_id, "request %s %s from %s", method, path, ip);

    // Route: GET /api/health
    if (strcmp(method, "GET") == 0) {
        if (strcmp(path, "/api/health") == 0) {
            send_health(client_fd, req_id);
            return;
        }
        send_404(client_fd, req_id);
        return;
    }

    send_405(client_fd, req_id);
}

/* -------------------- Server bootstrap -------------------- */

static volatile sig_atomic_t keep_running = 1;
static void handle_sigint(int sig) {
    (void)sig;
    keep_running = 0;
}

int main(void) {
    int port = getenv_int_or("PORT", 8080);
    LOG_JSON = getenv_bool_or("LOG_JSON", true);

    signal(SIGINT, handle_sigint);
    signal(SIGTERM, handle_sigint);

    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("socket");
        return 1;
    }

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof addr);
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons((uint16_t)port);

    if (bind(server_fd, (struct sockaddr*)&addr, sizeof addr) < 0) {
        perror("bind");
        close(server_fd);
        return 1;
    }
    if (listen(server_fd, 64) < 0) {
        perror("listen");
        close(server_fd);
        return 1;
    }

    fprintf(stderr, "API listening on :%d\n", port);

    while (keep_running) {
        struct sockaddr_in client_addr;
        socklen_t len = sizeof client_addr;
        int client_fd = accept(server_fd, (struct sockaddr*)&client_addr, &len);
        if (client_fd < 0) {
            if (errno == EINTR) break;
            perror("accept");
            continue;
        }
        handle_client(client_fd, &client_addr);
        close(client_fd);
    }

    close(server_fd);
    fprintf(stderr, "API shut down.\n");
    return 0;
}
