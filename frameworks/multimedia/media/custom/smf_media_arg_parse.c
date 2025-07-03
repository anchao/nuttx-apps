#include "smf_media_arg_parse.h"
#include <string.h>
#include <stdlib.h>

int smf_media_parse_string(const char* input, smf_media_kv_pair_t* pairs, int max_pairs, const char* flag) {
    char buffer[256];
    strncpy(buffer, input, sizeof(buffer)-1);
    buffer[sizeof(buffer)-1] = '\0';

    int count = 0;
    char* token = strtok(buffer, flag);
    
    while (token && count < max_pairs) {
        char* equal_pos = strchr(token, '=');
        if (equal_pos) {
            *equal_pos = '\0';
            strncpy(pairs[count].key, token, MAX_KEY_LEN-1);
            strncpy(pairs[count].value, equal_pos+1, MAX_VALUE_LEN-1);
            count++;
        }
        token = strtok(NULL, flag);
    }
    return count;
}

const char* smf_media_get_value(const smf_media_kv_pair_t* pairs, int count, const char* key) {
    for (int i = 0; i < count; i++) {
        if (strcmp(pairs[i].key, key) == 0) {
            return pairs[i].value;
        }
    }
    return NULL;
}
