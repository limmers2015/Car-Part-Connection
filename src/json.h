// src/json.h
#pragma once
#include <jansson.h>

json_t* json_parse_strict(const char* s);
const char* json_get_string(json_t* obj, const char* key);
int json_get_int(json_t* obj, const char* key, int* out);
