
#pragma once

#define MAX_KEY_LEN 32
#define MAX_VALUE_LEN 64

typedef struct {
    char key[MAX_KEY_LEN];
    char value[MAX_VALUE_LEN];
} smf_media_kv_pair_t;

int smf_media_parse_string(const char* input, smf_media_kv_pair_t* pairs, int max_pairs, const char* flag);
const char* smf_media_get_value(const smf_media_kv_pair_t* pairs, int count, const char* key);
