#include "smf_media_graph.api.h"
#include "smf_debug_ext.h"
#include "smf_stream_api.h"
#include "smf_stream_api_default.h"

static smf_media_priv_t* _privs[SMF_STREAM_END];
bool smf_media_ids_add(smf_media_priv_t* priv, uint8_t id) {
    returnIfErrC(false, !priv);
    for (int i = 0; i < 8; i++) {
        if (!priv->smf_media_ids[i]) {
            priv->smf_media_ids[i] = id;
            _privs[id] = priv;
            return true;
        }
    }
    return false;
}
bool smf_media_ids_remove(smf_media_priv_t* priv, uint8_t id) {
    returnIfErrC(false, !priv);
    for (int i = 0; i < 8; i++) {
        if (priv->smf_media_ids[i] == id) {
            priv->smf_media_ids[i] = 0;
            _privs[id] = 0;
            return true;
        }
    }
    return false;
}
bool smf_media_ids_clear(smf_media_priv_t* priv) {
    returnIfErrC(false, !priv);
    memset(priv->smf_media_ids, 0, sizeof(priv->smf_media_ids));
    priv->smf_media_id = 0;
    return true;
}
bool smf_media_ids_stop_all(smf_media_priv_t* priv) {
    returnIfErrC(false, !priv);
    for (int i = 7; i >= 0; i--) {
        if (priv->smf_media_ids[i]) {
            _privs[priv->smf_media_ids[i]] = 0;
            returnIfErrC(false,!smf_stream_stop(priv->smf_media_ids[i]));
            priv->smf_media_ids[i] = 0;
        }
    }
    return true;
}
bool smf_media_ids_stop(smf_media_priv_t* priv, uint8_t id) {
    returnIfErrC(false, !priv);
    if (smf_media_ids_remove(priv, id)) {
        return smf_stream_stop(id);
    }
    return false;
}
bool smf_media_ids_start(smf_media_priv_t* priv, uint8_t id, const char* param) {
    returnIfErrC(false, !priv);
    dbgNxPXL("[%02x]%s", (uint8_t)id, param);
    if (smf_media_ids_add(priv, id)) {
        return smf_stream_start_string(id, param);
    }
    return false;
}
bool smf_media_ids_start_param(smf_media_priv_t* priv, uint8_t id, void* param, uint32_t size) {
    dbgNxPXL("[%02x]%s", (uint8_t)id, param);
    if (smf_media_ids_add(priv, id)) {
        return smf_stream_start_params(id, param, size);
    }
    return false;
}
bool smf_media_ids_start_wait(smf_media_priv_t* priv, uint8_t id, const char* param, uint32_t timeout) {
    returnIfErrC(false, !priv);
    dbgNxPXL("[%02x]%s", (uint8_t)id, param);
    if (smf_media_ids_add(priv, id)) {
        return smf_stream_start_string_wait(id, param, timeout);
    }
    return false;
}
smf_media_priv_t* smf_media_ids_get(uint8_t id) {
    return _privs[id];
}