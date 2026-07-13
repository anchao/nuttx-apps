#include "smf_media_graph.api.h"
#include "smf_debug_ext.h"
#include "smf_stream_api.h"
#include "smf_stream_api_default.h"

//photo take
bool smf_media_photo_take_url_start(smf_media_priv_t* priv) {
    dbgTestPL();
    char param[256];
    char url[128];
    char *endptr;
    uint32_t cnt = (uint32_t)strtol(priv->smf_opt, &endptr, 10);
    if (cnt > 100) dbgWarnPXL("photo cnt too large");
    char *prefix;
    char *postfix;
    char *saveptr;
    memcpy(url, priv->smf_url, strlen(priv->smf_url) + 1);
    prefix = strtok_r(url, ".", &saveptr);
    postfix = strtok_r(NULL, ".", &saveptr);
    if (cnt > 1) {
        if (postfix)
            snprintf(param, 256, "mux=[cnt=#%d,url=[fileU://%s_%%04u.%s]],enc=[keys=bypass]", cnt, prefix, postfix);
        else
            snprintf(param, 256, "mux=[cnt=#%d,url=[fileU://%s_%%04u]],enc=[keys=bypass]", cnt, prefix);
    } else {
        snprintf(param, 256, "mux=[cnt=#%d,url=[%s]],enc=[keys=bypass]", cnt, priv->smf_url);
    }

    dbgTestPXL("[%02x]%s", SMF_STREAM_TAKE_PHOTO, param);
    returnIfErrC(false, !smf_media_ids_start(priv, SMF_STREAM_TAKE_PHOTO, param));
    return true;
}

bool smf_media_photo_take_start(smf_media_priv_t* priv) {
    dbgTestPL();
    return smf_media_photo_take_url_start(priv);
}

bool smf_media_photo_take_stop(smf_media_priv_t* priv) {
    dbgNxPL();
    smf_media_ids_stop_all(priv);
    return true;
}