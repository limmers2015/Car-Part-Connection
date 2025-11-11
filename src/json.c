// src/json.c
#include "json.h"

json_t* json_parse_strict(const char* s) {
    json_error_t err;
    json_t* root = json_loads(s, JSON_REJECT_DUPLICATES, &err);
    return root;
}

const char* json_get_string(json_t* obj, const char* key) {
    json_t* v = json_object_get(obj, key);
    if (!v || !json_is_string(v)) return NULL;
    return json_string_value(v);
}

int json_get_int(json_t* obj, const char* key, int* out) {
    json_t* v = json_object_get(obj, key);
    if (!v || !json_is_integer(v)) return -1;
    *out = (int)json_integer_value(v);
    return 0;
}
