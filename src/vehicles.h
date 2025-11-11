// src/vehicles.h
#pragma once
#include "http.h"

void handle_vehicles_list(const http_ctx* ctx, http_request* req, http_response* res);
void handle_vehicles_create(const http_ctx* ctx, http_request* req, http_response* res);
