// src/main.c
#define _POSIX_C_SOURCE 200809L
#include "http.h"
#include "db.h"
#include "sessions.h"
#include "auth.h"
#include "vehicles.h"
#include "util.h"
#include <arpa/inet.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sodium.h>

static int getenv_int_or(const char* k, int def){const char* v=getenv(k);if(!v||!*v)return def;return atoi(v);}

static volatile sig_atomic_t keep_running = 1;
static void handle_sigint(int s){(void)s; keep_running=0;}

static void route_request(const http_ctx* ctx, http_request* req, http_response* res) {
    if (!strcmp(req->method,"GET") && !strcmp(req->path,"/api/health")) {
        char body[256];
        snprintf(body,sizeof body,"{\"status\":\"ok\",\"request_id\":\"%s\",\"env\":\"%s\"}\n",
                 ctx->request_id, getenv("APP_ENV")?getenv("APP_ENV"):"dev");
        return http_send_json(res,200,body);
    }
    // Auth
    if (!strncmp(req->path,"/api/signup",11))  return handle_signup(ctx,req,res);
    if (!strncmp(req->path,"/api/login",10))   return handle_login(ctx,req,res);
    if (!strncmp(req->path,"/api/logout",11))  return handle_logout(ctx,req,res);
    if (!strncmp(req->path,"/api/me",7))       return handle_me(ctx,req,res);

    // Vehicles
    if (!strcmp(req->path,"/api/vehicles") && !strcmp(req->method,"GET"))
        return handle_vehicles_list(ctx,req,res);
    if (!strcmp(req->path,"/api/vehicles") && !strcmp(req->method,"POST"))
        return handle_vehicles_create(ctx,req,res);

    http_send_404(res);
}

int main(void) {
    if (sodium_init() < 0) {
        fprintf(stderr,"libsodium init failed\n"); return 1;
    }

    int port = getenv_int_or("PORT",8080);

    if (db_init()!=0) { fprintf(stderr,"db_init failed\n"); return 1; }
    if (sessions_init()!=0) { fprintf(stderr,"sessions_init failed\n"); return 1; }

    int server_fd = http_listen(port);
    if (server_fd < 0) { perror("listen"); return 1; }
    fprintf(stderr,"API listening on :%d\n", port);

    signal(SIGINT, handle_sigint);
    signal(SIGTERM, handle_sigint);

    while (keep_running) {
        struct sockaddr_in client_addr;
        int cfd = http_accept(server_fd, &client_addr);
        if (cfd < 0) { if (!keep_running) break; continue; }
        http_request req; http_response res = {.fd=cfd}; http_ctx ctx = {0};
        inet_ntop(AF_INET, &client_addr.sin_addr, ctx.remote_ip, sizeof ctx.remote_ip);
        uuid4(ctx.request_id);

        if (http_read_request(cfd, &req, &ctx)==0) {
            route_request(&ctx, &req, &res);
            http_free_request(&req);
        }
        close(cfd);
    }

    sessions_close();
    db_close();
    close(server_fd);
    fprintf(stderr,"API shut down.\n");
    return 0;
}
