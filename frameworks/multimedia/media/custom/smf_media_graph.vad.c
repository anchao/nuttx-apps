#include "smf_media_graph.api.h"
#include "smf_debug_ext.h"
#include "smf_pool.h"
#include "smf_stream_api.h"
#include "smf_stream_api_default.h"

static smf_media_priv_t* _vad = 0;
bool smf_media_vad_start(smf_media_priv_t* priv) {
    returnIfErrC(0, _vad);
    if(priv->smf_media_stream_type == SMF_MEDIA_STREAM_HWVAD){
        returnIfErrC(0, !smf_media_ids_start(priv, SMF_STREAM_HWVAD, 0));
    }else{
        returnIfErrC(0, !smf_media_ids_start(priv, SMF_STREAM_VAD, 0));
    }
    _vad = priv;
    return true;
}
bool smf_media_vad_stop(smf_media_priv_t* priv) {
    _vad = 0;
    smf_media_ids_stop_all(priv);
    smf_media_audio_input_remove(priv);
    return true;
}
bool smf_media_vad_ready(void *_param) {
    dbgNxPL();
    smf_media_priv_t* priv = _vad;
    returnIfErrC(true, !priv);
    char param[256];
    int n = 0;
    if (priv->callback) {
        n = snprintf(param, 255, "sink=[cboutput=#%p,priv=#%p]", priv->callback, priv->callback_priv);
    }
    else if (strlen(priv->smf_url)) {
        n = snprintf(param, 255, "mux=[url=[%s]]", priv->smf_url);
    }
    else {
        dbgErrPL();
        return false;
    }
    const char* opt = priv->smf_opt;
    if (strlen(opt) == 0) {
        dbgNxPXL("opt is null, use default pcm");
        opt = "fmt=[rate=#16000,ch=#1,bits=#16,width=#2],enc=[keys=pcm,imin=#1920]";
    }
    n += snprintf(param + n, 255 - n, ",%s", opt);
    returnIfErrC(true, !smf_media_audio_input_config(priv));
    returnIfErrC(true, !smf_media_ids_start(priv, SMF_STREAM_WAKEUP, param));
    //returnIfErrC(0, !smf_media_ids_start_wait(priv, SMF_STREAM_WAKEUP, param, 1000));
    return true;
}

bool smf_media_vad_predata_done(void *param) {
    dbgNxPL();
    smf_media_priv_t* priv = _vad;
    returnIfErrC(true, !priv);
    if(priv->smf_media_stream_type == SMF_MEDIA_STREAM_HWVAD){
        smf_media_ids_stop(priv, SMF_STREAM_HWVAD);
    }else{
        smf_media_ids_stop(priv, SMF_STREAM_VAD);
    }
    return true;
}

static bool smf_vad_record_callback(void* priv, struct smf_frame_t*frm){
    void* buffer = frm->buff;
    uint32_t buffer_size = frm->size;
    dbgNxPDL(frm->size);
    frm->size -= buffer_size;
    return true;
}
bool smf_media_vad_record_start(void *_param){
    dbgNxPL();
    smf_media_priv_t* priv = _vad;
    returnIfErrC(true, !priv);
    char param[256];
    int n = 0;
    if (&smf_vad_record_callback) {
        n = snprintf(param, 255, "sink=[cboutput=#%p,priv=#%p]", &smf_vad_record_callback, NULL);
    }
    else {
        dbgErrPL();
        return false;
    }
    const char* opt = "fmt=[rate=#16000,ch=#1,bits=#16,width=#2],enc=[keys=opus,br=#32000,dms=#200,haveHead=#0]";
    n += snprintf(param + n, 255 - n, ",%s", opt);
    returnIfErrC(true, !smf_media_ids_start(priv, SMF_STREAM_VAD_RECORD, param));
    return true;
}

bool smf_media_vad_record_stop(void *param){
    dbgNxPL();
    smf_media_priv_t* priv = _vad;
    returnIfErrC(true, !priv);
    smf_media_ids_stop(priv, SMF_STREAM_VAD_RECORD);
    return true;
}
