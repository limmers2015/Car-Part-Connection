// src/auth.c
#define _POSIX_C_SOURCE 200809L
#include "auth.h"
#include "json.h"
#include "db.h"
#include "sessions.h"
#include "util.h"
#include <jansson.h>
#include <sodium.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static int cookie_secure_flag(void) {
    const char* v = getenv("SESSION_COOKIE_SECURE");
    if (!v) return 0;
    return (!strcasecmp(v,"1")||!strcasecmp(v,"true")||!strcasecmp(v,"yes"));
}
static const char* cookie_samesite_attr(void) {
    const char* v = getenv("SESSION_COOKIE_SAMESITE");
    if (!v) return "Lax";
    return v;
}

/* Helpers */
static void set_session_cookie(http_response* res, const char* sid) {
    char header[512];
    const char* name = sessions_cookie_name();
    int secure = cookie_secure_flag();
    const char* samesite = cookie_samesite_attr();
    int n = snprintf(header, sizeof header,
        "Set-Cookie: %s=%s; Path=/; HttpOnly; SameSite=%s%s\r\n",
        name, sid, samesite, secure?"; Secure":"");
    send(res->fd, header, n, 0);
}

static void clear_session_cookie(http_response* res) {
    char header[512];
    const char* name = sessions_cookie_name();
    int secure = cookie_secure_flag();
    const char* samesite = cookie_samesite_attr();
    int n = snprintf(header, sizeof header,
        "Set-Cookie: %s=deleted; Path=/; HttpOnly; Max-Age=0; SameSite=%s%s\r\n",
        name, samesite, secure?"; Secure":"");
    send(res->fd, header, n, 0);
}

static int parse_cookie_for_session(const char* cookie_header, char out_sid[128]) {
    if (!cookie_header || !*cookie_header) return -1;
    const char* name = sessions_cookie_name();
    char needle[256]; snprintf(needle, sizeof needle, "%s=", name);
    const char* p = strstr(cookie_header, needle);
    if (!p) return -1;
    p += strlen(needle);
    const char* end = strchr(p, ';'); size_t len = end ? (size_t)(end - p) : strlen(p);
    if (len >= 127) len = 127;
    snprintf(out_sid, 128, "%.*s", (int)len, p);
    return 0;
}

void handle_signup(const http_ctx* ctx, http_request* req, http_response* res) {
    (void)ctx;
    if (strcmp(req->method,"POST")) return http_send_405(res);
    if (!req->body) return http_send_json(res,400,"{\"error\":\"invalid_json\"}\n");
    json_t* root = json_parse_strict(req->body);
    if (!root) return http_send_json(res,400,"{\"error\":\"invalid_json\"}\n");
    const char* email = json_get_string(root,"email");
    const char* password = json_get_string(root,"password");
    if (!email || !password || strlen(password)<8) {
        json_decref(root);
        return http_send_json(res,400,"{\"error\":\"invalid_input\"}\n");
    }

    char hash[crypto_pwhash_STRBYTES];
    if (crypto_pwhash_str(hash, password, strlen(password),
                          crypto_pwhash_OPSLIMIT_MODERATE,
                          crypto_pwhash_MEMLIMIT_MODERATE) != 0) {
        json_decref(root);
        return http_send_json(res,500,"{\"error\":\"hash_failed\"}\n");
    }

    char user_id[37];
    if (db_user_create(email, hash, "user", user_id) != 0) {
        json_decref(root);
        return http_send_json(res,409,"{\"error\":\"email_exists\"}\n");
    }
    json_decref(root);

    // auto-login
    char sid[37];
    if (!sessions_create(user_id, sid, sessions_ttl_seconds())) {
        return http_send_json(res,500,"{\"error\":\"session_failed\"}\n");
    }
    // Send Set-Cookie header manually before body:
    set_session_cookie(res, sid);
    http_send_json(res,201,"{\"ok\":true}\n");
}

void handle_login(const http_ctx* ctx, http_request* req, http_response* res) {
    (void)ctx;
    if (strcmp(req->method,"POST")) return http_send_405(res);
    if (!req->body) return http_send_json(res,400,"{\"error\":\"invalid_json\"}\n");
    json_t* root = json_parse_strict(req->body);
    if (!root) return http_send_json(res,400,"{\"error\":\"invalid_json\"}\n");
    const char* email = json_get_string(root,"email");
    const char* password = json_get_string(root,"password");
    if (!email || !password) { json_decref(root); return http_send_json(res,400,"{\"error\":\"invalid_input\"}\n"); }

    char user_id[37]={0}, stored_hash[256]={0}, role[16]={0};
    if (db_user_find_by_email(email, user_id, stored_hash, sizeof stored_hash, role, sizeof role) != 0) {
        json_decref(root);
        return http_send_json(res,401,"{\"error\":\"bad_credentials\"}\n");
    }
    if (crypto_pwhash_str_verify(stored_hash, password, strlen(password)) != 0) {
        json_decref(root);
        return http_send_json(res,401,"{\"error\":\"bad_credentials\"}\n");
    }
    json_decref(root);

    char sid[37];
    if (!sessions_create(user_id, sid, sessions_ttl_seconds())) {
        return http_send_json(res,500,"{\"error\":\"session_failed\"}\n");
    }
    set_session_cookie(res, sid);
    http_send_json(res,200,"{\"ok\":true}\n");
}

void handle_logout(const http_ctx* ctx, http_request* req, http_response* res) {
    (void)ctx;
    if (strcmp(req->method,"POST")) return http_send_405(res);
    char sid[128]={0};
    if (parse_cookie_for_session(req->cookie, sid)==0) {
        sessions_delete(sid);
    }
    clear_session_cookie(res);
    http_send_json(res,200,"{\"ok\":true}\n");
}

void handle_me(const http_ctx* ctx, http_request* req, http_response* res) {
    (void)ctx;
    if (strcmp(req->method,"GET")) return http_send_405(res);
    char sid[128]={0}, uid[37]={0};
    if (parse_cookie_for_session(req->cookie, sid)!=0 || !sessions_get_user(sid, uid)) {
        return http_send_json(res,401,"{\"error\":\"unauthorized\"}\n");
    }
    char body[128];
    snprintf(body, sizeof body, "{\"user_id\":\"%s\"}\n", uid);
    http_send_json(res,200,body);
}
