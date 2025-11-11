// src/http.h
#pragma once
#include <stddef.h>
#include <stdbool.h>
#include <netinet/in.h>

typedef struct {
    char method[8];
    char path[1024];
    char content_type[128];
    char cookie[2048];
    size_t content_length;
    char* body;
} http_request;

typedef struct {
    int fd;
} http_response;

typedef struct {
    int server_fd;
    int port;
} http_server;

typedef struct {
    char request_id[37];
    char remote_ip[64];
} http_ctx;

int  http_listen(int port);
int  http_accept(int server_fd, struct sockaddr_in* client_addr);
int  http_read_request(int client_fd, http_request* req, http_ctx* ctx);
void http_free_request(http_request* req);

void http_send_json(http_response* res, int status_code, const char* json);
void http_send_405(http_response* res);
void http_send_404(http_response* res);
