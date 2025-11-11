// src/auth.h
#pragma once
#include "http.h"

void handle_signup(const http_ctx* ctx, http_request* req, http_response* res);
void handle_login(const http_ctx* ctx, http_request* req, http_response* res);
void handle_logout(const http_ctx* ctx, http_request* req, http_response* res);
void handle_me(const http_ctx* ctx, http_request* req, http_response* res);
