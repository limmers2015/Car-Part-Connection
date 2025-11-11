// src/vehicles.c
#define _POSIX_C_SOURCE 200809L
#include "vehicles.h"
#include "sessions.h"
#include "json.h"
#include "db.h"
#include <jansson.h>
#include <stdio.h>
#include <string.h>

static int get_user_from_cookie(http_request* req, char out_uid[37]) {
    char cookie[2048]; snprintf(cookie, sizeof cookie, "%s", req->cookie);
    const char* name = sessions_cookie_name();
    char needle[256]; snprintf(needle, sizeof needle, "%s=", name);
    char* p = strstr(cookie, needle);
    if (!p) return -1;
    p += strlen(needle);
    char* end = strchr(p,';');
    size_t len = end ? (size_t)(end-p) : strlen(p);
    char sid[128]={0}; snprintf(sid, sizeof sid, "%.*s", (int)(len>120?120:len), p);
    if (!sessions_get_user(sid, out_uid)) return -1;
    return 0;
}

void handle_vehicles_list(const http_ctx* ctx, http_request* req, http_response* res) {
    (void)ctx;
    if (strcmp(req->method,"GET")) return http_send_405(res);
    char uid[37]={0};
    if (get_user_from_cookie(req, uid)!=0) return http_send_json(res,401,"{\"error\":\"unauthorized\"}\n");
    char* json = NULL;
    if (db_vehicles_list(uid, &json)!=0) return http_send_json(res,500,"{\"error\":\"db_error\"}\n");
    http_send_json(res,200,json);
    free(json);
}

void handle_vehicles_create(const http_ctx* ctx, http_request* req, http_response* res) {
    (void)ctx;
    if (strcmp(req->method,"POST")) return http_send_405(res);
    char uid[37]={0};
    if (get_user_from_cookie(req, uid)!=0) return http_send_json(res,401,"{\"error\":\"unauthorized\"}\n");
    if (!req->body) return http_send_json(res,400,"{\"error\":\"invalid_json\"}\n");
    json_t* root = json_parse_strict(req->body);
    if (!root) return http_send_json(res,400,"{\"error\":\"invalid_json\"}\n");
    int year=0; const char* make=NULL; const char* model=NULL; const char* nickname="";
    if (json_get_int(root,"year",&year)!=0 || !(make=json_get_string(root,"make")) || !(model=json_get_string(root,"model"))) {
        json_decref(root); return http_send_json(res,400,"{\"error\":\"invalid_input\"}\n");
    }
    const char* nn = json_get_string(root,"nickname");
    if (nn) nickname = nn;
    char* out = NULL;
    int rc = db_vehicle_insert(uid, year, make, model, nickname, &out);
    json_decref(root);
    if (rc!=0) return http_send_json(res,500,"{\"error\":\"db_error\"}\n");
    http_send_json(res,201,out);
    free(out);
}
