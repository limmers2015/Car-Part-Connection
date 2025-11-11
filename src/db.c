// src/db.c
#define _POSIX_C_SOURCE 200809L
#include "db.h"
#include "util.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static PGconn* g_conn = NULL;

static const char* getenv_or(const char* k, const char* d) {
    const char* v = getenv(k); return (v && *v) ? v : d;
}

int db_init(void) {
    const char* host = getenv_or("PGHOST", NULL);
    const char* port = getenv_or("PGPORT", NULL);
    const char* db   = getenv_or("PGDATABASE", NULL);
    const char* user = getenv_or("PGUSER", NULL);
    const char* pass = getenv_or("PGPASSWORD", NULL);

    char conninfo[1024] = {0};
    snprintf(conninfo, sizeof conninfo,
        "host=%s port=%s dbname=%s user=%s password=%s",
        host?host:"", port?port:"", db?db:"", user?user:"", pass?pass:"");

    g_conn = PQconnectdb(conninfo);
    if (PQstatus(g_conn) != CONNECTION_OK) {
        fprintf(stderr, "Postgres connect failed: %s\n", PQerrorMessage(g_conn));
        return -1;
    }
    return 0;
}

void db_close(void) {
    if (g_conn) PQfinish(g_conn);
    g_conn = NULL;
}

PGconn* db_conn(void) { return g_conn; }

int db_user_create(const char* email, const char* password_hash, const char* role, char out_id[37]) {
    char uuid[37]; uuid4(uuid);
    const char* params[4] = { uuid, email, password_hash, role };
    PGresult* r = PQexecParams(g_conn,
        "insert into users(id,email,password_hash,role) values($1,$2,$3,$4) returning id",
        4, NULL, params, NULL, NULL, 0);
    if (PQresultStatus(r) != PGRES_TUPLES_OK) {
        PQclear(r);
        return -1;
    }
    strncpy(out_id, PQgetvalue(r, 0, 0), 36);
    out_id[36] = '\0';
    PQclear(r);
    return 0;
}

int db_user_find_by_email(const char* email, char out_id[37], char* out_hash, size_t hash_len, char* out_role, size_t role_len) {
    const char* params[1] = { email };
    PGresult* r = PQexecParams(g_conn,
        "select id, password_hash, role from users where email=$1 limit 1",
        1, NULL, params, NULL, NULL, 0);
    if (PQresultStatus(r) != PGRES_TUPLES_OK || PQntuples(r) == 0) {
        PQclear(r); return -1;
    }
    strncpy(out_id, PQgetvalue(r, 0, 0), 36); out_id[36]='\0';
    strncpy(out_hash, PQgetvalue(r, 0, 1), hash_len-1); out_hash[hash_len-1]='\0';
    strncpy(out_role, PQgetvalue(r, 0, 2), role_len-1); out_role[role_len-1]='\0';
    PQclear(r);
    return 0;
}

int db_vehicles_list(const char* user_id, char** out_json) {
    const char* params[1] = { user_id };
    PGresult* r = PQexecParams(g_conn,
        "select id,year,make,model,coalesce(nickname,'') as nickname,created_at "
        "from vehicles where user_id=$1 order by created_at desc",
        1, NULL, params, NULL, NULL, 0);
    if (PQresultStatus(r) != PGRES_TUPLES_OK) { PQclear(r); return -1; }
    int rows = PQntuples(r);
    // small JSON build by hand (safe because we control data; for more safety, escape)
    size_t cap = 1024 + rows*256;
    char* buf = malloc(cap); if (!buf) { PQclear(r); return -1; }
    size_t off = 0; off += snprintf(buf+off, cap-off, "{\"items\":[");
    for (int i=0;i<rows;i++) {
        if (i) off += snprintf(buf+off, cap-off, ",");
        off += snprintf(buf+off, cap-off,
            "{\"id\":\"%s\",\"year\":%s,\"make\":\"%s\",\"model\":\"%s\",\"nickname\":\"%s\"}",
            PQgetvalue(r,i,0), PQgetvalue(r,i,1), PQgetvalue(r,i,2), PQgetvalue(r,i,3), PQgetvalue(r,i,4));
    }
    off += snprintf(buf+off, cap-off, "]}");
    PQclear(r);
    *out_json = buf;
    return 0;
}

int db_vehicle_insert(const char* user_id, int year, const char* make, const char* model, const char* nickname, char** out_json) {
    char uuid[37]; uuid4(uuid);
    char year_s[16]; snprintf(year_s, sizeof year_s, "%d", year);
    const char* params[6] = { uuid, user_id, year_s, make, model, nickname ? nickname : "" };
    PGresult* r = PQexecParams(g_conn,
        "insert into vehicles(id,user_id,year,make,model,nickname) values($1,$2,$3,$4,$5,$6) "
        "returning id,year,make,model,coalesce(nickname,'')",
        6, NULL, params, NULL, NULL, 0);
    if (PQresultStatus(r) != PGRES_TUPLES_OK) { PQclear(r); return -1; }
    // produce JSON
    const char* id   = PQgetvalue(r,0,0);
    const char* y    = PQgetvalue(r,0,1);
    const char* mk   = PQgetvalue(r,0,2);
    const char* mdl  = PQgetvalue(r,0,3);
    const char* nick = PQgetvalue(r,0,4);
    size_t cap = 256; char* buf = malloc(cap);
    snprintf(buf, cap, "{\"id\":\"%s\",\"year\":%s,\"make\":\"%s\",\"model\":\"%s\",\"nickname\":\"%s\"}",
             id, y, mk, mdl, nick);
    PQclear(r);
    *out_json = buf;
    return 0;
}
