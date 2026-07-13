#include "smf_media_graph.api.h"
#include "smf_debug_ext.h"
#include "smf_stream_api.h"
#include "smf_stream_api_default.h"

bool smf_media_rtsp_server_start(smf_media_priv_t* priv) {
    dbgNxPL();
    uint8_t stream = smf_stream_get_free(SMF_STREAM_RTSP_SERVER, SMF_STREAM_RTSP_SERVER);
    returnIfErrC(false, !smf_stream_check(stream));
    priv->smf_media_id = stream;
    // dbgNxPXL("[%02x]", stream);
    bool ret = smf_media_ids_start(priv, stream, 0);
    if(!ret){
        priv->smf_media_id = 0;
        dbgErrPXL("[%02x] start failed", stream);
        return false;
    }
    priv->status = SMF_STREAM_STATUS_play;
    return true;
}

bool smf_media_rtsp_server_stop(smf_media_priv_t* priv) {
    dbgNxPL();
    smf_media_ids_stop_all(priv);

    priv->status = SMF_STREAM_STATUS_null;
    return true;
}

bool smf_media_rtsp_client_start(smf_media_priv_t* priv){
    dbgNxPL();
    const char* url = priv->smf_opt;
    returnIfErrC(false, !url);

    returnIfErrC(false, !smf_media_audio_output_config(priv));
    returnIfErrC(false, !smf_media_video_output_config(priv));

    uint8_t stream = smf_stream_get_free(SMF_STREAM_RTSP_CLIENT, SMF_STREAM_RTSP_CLIENT);
    returnIfErrC(false, !smf_stream_check(stream));
    priv->smf_media_id = stream;
    // dbgNxPXL("[%02x]%s", stream, url);
    bool ret = smf_media_ids_start_param(priv, stream, (void*)url, strlen(url)+1);
    if(!ret){
        priv->smf_media_id = 0;
        dbgErrPXL("[%02x] start failed", stream);
        return false;
    }
    priv->status = SMF_STREAM_STATUS_play;
    return true;
}

bool smf_media_rtsp_client_stop(smf_media_priv_t* priv){
    dbgNxPL();
    smf_media_ids_stop_all(priv);

    smf_media_audio_output_remove(priv);
    smf_media_video_output_remove(priv);

    priv->status = SMF_STREAM_STATUS_null;
    return true;
}

bool smf_media_rtsp_client_set_volume(smf_media_priv_t* priv, int vol){
    returnIfErrC(false, !priv);
    returnIfErrC(false, !priv->smf_media_id);
    dbgNxPDL(vol);
    return smf_stream_set_volume(priv->smf_media_id, vol);
}
bool smf_media_rtsp_client_set_mute(smf_media_priv_t* priv, bool mute){
    dbgNxPL();
    returnIfErrC(false, !priv);
    returnIfErrC(false, !priv->smf_media_id);
    return smf_stream_set_mute(priv->smf_media_id, mute);
}
bool smf_media_rtsp_client_pause(smf_media_priv_t* priv){
    dbgNxPL();
    returnIfErrC(false, !priv);
    returnIfErrC(false, !priv->smf_media_id);
    if(smf_stream_pause(priv->smf_media_id)){
        priv->status = SMF_STREAM_STATUS_pause;
        return true;
    }
    return false; 
}
bool smf_media_rtsp_client_resume(smf_media_priv_t* priv){
    dbgNxPL();
    returnIfErrC(false, !priv);
    returnIfErrC(false, !priv->smf_media_id);
    if(smf_stream_resume(priv->smf_media_id)){
        priv->status = SMF_STREAM_STATUS_play;
        return true;
    }
    return false;
}




