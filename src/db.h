// src/db.h
#pragma once
#include <libpq-fe.h>

int  db_init(void);
void db_close(void);
PGconn* db_conn(void);

/* Users */
int  db_user_create(const char* email, const char* password_hash, const char* role, char out_id[37]);
int  db_user_find_by_email(const char* email, char out_id[37], char* out_hash, size_t hash_len, char* out_role, size_t role_len);

/* Vehicles */
int  db_vehicles_list(const char* user_id, char** out_json);
int  db_vehicle_insert(const char* user_id, int year, const char* make, const char* model, const char* nickname, char** out_json);
