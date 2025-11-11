// src/sessions.h
#pragma once
#include <stdbool.h>

int  sessions_init(void);
void sessions_close(void);
bool sessions_create(const char* user_id, char out_session_id[37], int ttl_seconds);
bool sessions_get_user(const char* session_id, char out_user_id[37]);
bool sessions_delete(const char* session_id);
const char* sessions_cookie_name(void);
int  sessions_ttl_seconds(void);
