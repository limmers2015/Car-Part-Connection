// src/sessions.c
#define _POSIX_C_SOURCE 200809L
#include "sessions.h"
#include "util.h"
#include <hiredis/hiredis.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static redisContext* rc = NULL;
static char COOKIE_NAME[64] = "cpc_session";
static int  TTL = 604800;

static const char* getenv_or(const char* k, const char* d){const char* v=getenv(k);return(v&&*v)?v:d;}

int sessions_init(void) {
    const char* host = getenv_or("REDIS_HOST","127.0.0.1");
    int port = atoi(getenv_or("REDIS_PORT","6379"));
    int db   = atoi(getenv_or("REDIS_DB","0"));

    rc = redisConnect(host, port);
    if (!rc || rc->err) return -1;
    if (db > 0) {
        redisReply* r = redisCommand(rc, "SELECT %d", db);
        if (!r) return -1; freeReplyObject(r);
    }
    const char* name = getenv("SESSION_COOKIE_NAME");
    if (name && *name) snprintf(COOKIE_NAME, sizeof COOKIE_NAME, "%s", name);

    const char* ttl = getenv("SESSION_TTL_SECONDS");
    if (ttl && *ttl) TTL = atoi(ttl);
    return 0;
}

void sessions_close(void) {
    if (rc) redisFree(rc);
    rc = NULL;
}

bool sessions_create(const char* user_id, char out_session_id[37], int ttl_seconds) {
    if (!rc) return false;
    char sid[37]; uuid4(sid);
    redisReply* r = redisCommand(rc, "SETEX session:%s %d %s", sid, ttl_seconds>0?ttl_seconds:TTL, user_id);
    if (!r) return false;
    int ok = (r->type == REDIS_REPLY_STATUS && strcasecmp(r->str,"OK")==0);
    freeReplyObject(r);
    if (!ok) return false;
    strncpy(out_session_id, sid, 37);
    return true;
}

bool sessions_get_user(const char* session_id, char out_user_id[37]) {
    if (!rc) return false;
    redisReply* r = redisCommand(rc, "GET session:%s", session_id);
    if (!r) return false;
    bool ok = false;
    if (r->type == REDIS_REPLY_STRING && r->len > 0) {
        snprintf(out_user_id, 37, "%.*s", r->len>36?36:r->len, r->str);
        ok = true;
    }
    freeReplyObject(r);
    return ok;
}

bool sessions_delete(const char* session_id) {
    if (!rc) return false;
    redisReply* r = redisCommand(rc, "DEL session:%s", session_id);
    if (!r) return false;
    freeReplyObject(r);
    return true;
}

const char* sessions_cookie_name(void){ return COOKIE_NAME; }
int sessions_ttl_seconds(void){ return TTL; }
