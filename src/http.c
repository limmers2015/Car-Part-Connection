// src/http.c
#define _POSIX_C_SOURCE 200809L
#include "http.h"
#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

static const char* status_line(int code) {
    switch (code) {
      case 200: return "HTTP/1.1 200 OK\r\n";
      case 201: return "HTTP/1.1 201 Created\r\n";
      case 204: return "HTTP/1.1 204 No Content\r\n";
      case 400: return "HTTP/1.1 400 Bad Request\r\n";
      case 401: return "HTTP/1.1 401 Unauthorized\r\n";
      case 403: return "HTTP/1.1 403 Forbidden\r\n";
      case 404: return "HTTP/1.1 404 Not Found\r\n";
      case 405: return "HTTP/1.1 405 Method Not Allowed\r\n";
      case 409: return "HTTP/1.1 409 Conflict\r\n";
      case 500: return "HTTP/1.1 500 Internal Server Error\r\n";
      default:  return "HTTP/1.1 200 OK\r\n";
    }
}

int http_listen(int port) {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) return -1;
    int opt = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET; addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons((uint16_t)port);
    if (bind(fd, (struct sockaddr*)&addr, sizeof addr) < 0) { close(fd); return -1; }
    if (listen(fd, 64) < 0) { close(fd); return -1; }
    return fd;
}

int http_accept(int server_fd, struct sockaddr_in* client_addr) {
    socklen_t len = sizeof *client_addr;
    int c = accept(server_fd, (struct sockaddr*)client_addr, &len);
    return c;
}

static char* strnstr_local(const char* haystack, const char* needle, size_t len) {
    size_t nlen = strlen(needle);
    if (!nlen) return (char*)haystack;
    for (size_t i=0; i + nlen <= len; i++) {
        if (memcmp(haystack + i, needle, nlen) == 0) return (char*)(haystack + i);
    }
    return NULL;
}

int http_read_request(int client_fd, http_request* req, http_ctx* ctx) {
    memset(req, 0, sizeof *req);
    char buf[8192];
    ssize_t n = recv(client_fd, buf, sizeof(buf)-1, 0);
    if (n <= 0) return -1;
    buf[n] = '\0';

    // request line
    if (sscanf(buf, "%7s %1023s", req->method, req->path) != 2) return -1;

    // headers
    char* header_end = strnstr_local(buf, "\r\n\r\n", (size_t)n);
    if (!header_end) return -1;
    size_t header_len = (size_t)(header_end - buf) + 4;

    // scan for interested headers
    const char* p = buf;
    while (p < header_end) {
        char* line_end = strnstr_local(p, "\r\n", (size_t)(header_end - p));
        if (!line_end) break;
        size_t line_len = (size_t)(line_end - p);
        if (line_len >= 14 && !strncasecmp(p, "Content-Type:", 13)) {
            size_t copy = line_len - 13;
            while (copy && (p[13 + (line_len - 13 - copy)]==' ')) copy--;
            snprintf(req->content_type, sizeof req->content_type, "%.*s", (int)(line_len-13), p+13);
        }
        if (line_len >= 16 && !strncasecmp(p, "Content-Length:", 15)) {
            char tmp[32] = {0};
            snprintf(tmp, sizeof tmp, "%.*s", (int)(line_len-15), p+15);
            req->content_length = (size_t)strtoul(tmp, NULL, 10);
        }
        if (line_len >= 7 && !strncasecmp(p, "Cookie:", 7)) {
            size_t copy = line_len - 7;
            snprintf(req->cookie, sizeof req->cookie, "%.*s", (int)(line_len-7), p+7);
        }
        p = line_end + 2;
    }

    // body
    size_t have = (size_t)n - header_len;
    if (have < req->content_length) {
        // read remaining
        size_t remaining = req->content_length - have;
        char* body = malloc(req->content_length + 1);
        if (!body) return -1;
        memcpy(body, buf + header_len, have);
        size_t off = have;
        while (remaining > 0) {
            ssize_t r = recv(client_fd, body + off, remaining, 0);
            if (r <= 0) { free(body); return -1; }
            off += (size_t)r; remaining -= (size_t)r;
        }
        body[req->content_length] = '\0';
        req->body = body;
    } else {
        // body fully present
        if (req->content_length > 0) {
            req->body = malloc(req->content_length + 1);
            if (!req->body) return -1;
            memcpy(req->body, buf + header_len, req->content_length);
            req->body[req->content_length] = '\0';
        } else {
            req->body = NULL;
        }
    }
    return 0;
}

void http_free_request(http_request* req) {
    if (req->body) free(req->body);
    memset(req, 0, sizeof *req);
}

static void send_common(http_response* res, int code, const char* content_type, const char* body) {
    char header[256];
    int n = snprintf(header, sizeof header,
        "%sContent-Type: %s\r\n"
        "Connection: close\r\n"
        "Content-Length: %zu\r\n\r\n",
        status_line(code), content_type, body ? strlen(body) : 0);
    send(res->fd, header, n, 0);
    if (body && *body) send(res->fd, body, strlen(body), 0);
}

void http_send_json(http_response* res, int status_code, const char* json) {
    send_common(res, status_code, "application/json; charset=utf-8", json ? json : "");
}

void http_send_405(http_response* res) {
    http_send_json(res, 405, "{\"error\":\"method_not_allowed\"}\n");
}

void http_send_404(http_response* res) {
    http_send_json(res, 404, "{\"error\":\"not_found\"}\n");
}
