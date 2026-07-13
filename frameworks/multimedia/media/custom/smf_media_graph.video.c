#include "smf_media_graph.api.h"
#include "smf_debug_ext.h"
#include "smf_stream_api.h"
#include "smf_stream_api_default.h"

//video output
bool smf_media_video_output_config(smf_media_priv_t* priv) {
    returnIfErrC(false, !priv);
    return smf_media_ids_start(priv, SMF_STREAM_DISPLAY, 0);
}
bool smf_media_video_output_remove(smf_media_priv_t* priv) {
    returnIfErrC(false, !priv);
    return smf_media_ids_stop(priv, SMF_STREAM_DISPLAY);
}

//video input
bool smf_media_video_input_config(smf_media_priv_t* priv) {
    returnIfErrC(false, !priv);
    return smf_media_ids_start(priv, SMF_STREAM_CAMERA, 0);
}
bool smf_media_video_input_remove(smf_media_priv_t* priv) {
    returnIfErrC(false, !priv);
    return smf_media_ids_stop(priv, SMF_STREAM_CAMERA);
}

//video player
bool smf_media_video_player_url_start(smf_media_priv_t* priv){
    dbgNxPL();
    if (strlen(priv->smf_opt) == 0) {
        dbgErrPXL("video player arg is null");
        return false;
    }
    char param[256];
    snprintf(param, 256, "dem=[url=[%s],vol=#%u,cache=#8192]", priv->smf_url, priv->volume);

    if (memcmp(priv->smf_opt, "mp4-h264-aac", strlen(priv->smf_opt)) == 0) {
        returnIfErrC(false, !smf_media_audio_output_config(priv));
        returnIfErrC(false, !smf_media_video_output_config(priv));
        dbgTestPXL("[%02x,%02x,%02x]%s", SMF_STREAM_AV_PLAY, SMF_STREAM_AUDIO_PLAY, SMF_STREAM_VIDEO_PLAY, param);
        returnIfErrC(false, !smf_media_ids_start(priv, SMF_STREAM_AV_PLAY, param));
        returnIfErrC(false, !smf_media_ids_start(priv, SMF_STREAM_AUDIO_PLAY, 0));
        returnIfErrC(false, !smf_media_ids_start(priv, SMF_STREAM_VIDEO_PLAY, 0));
    }else if(memcmp(priv->smf_opt, "mp4-jpeg-aac", strlen(priv->smf_opt)) == 0) {
        returnIfErrC(false, !smf_media_audio_output_config(priv));
        returnIfErrC(false, !smf_media_video_output_config(priv));
        returnIfErrC(false, !smf_media_ids_start(priv, SMF_STREAM_AV_PLAY1, param));
        returnIfErrC(false, !smf_media_ids_start(priv, SMF_STREAM_AUDIO_PLAY, 0));
    }else if(memcmp(priv->smf_opt, "mp4-jpeg", strlen(priv->smf_opt)) == 0) {
        returnIfErrC(false, !smf_media_video_output_config(priv));
        returnIfErrC(false, !smf_media_ids_start(priv, SMF_STREAM_AV_PLAY1, param));
    }else if(memcmp(priv->smf_opt, "gif", strlen(priv->smf_opt)) == 0) {

    }else if(memcmp(priv->smf_opt, "mjpeg", strlen(priv->smf_opt)) == 0) {

    }else if(memcmp(priv->smf_opt, "h264", strlen(priv->smf_opt)) == 0) {

    }else{
        dbgErrPXL("arg not supported");
        return false;
    }
    return true;
}
bool smf_media_video_player_buffer_start(smf_media_priv_t* priv){
    dbgNxPL();
    if (strlen(priv->smf_opt) == 0) {
        dbgErrPXL("video player arg is null");
        return false;
    }
    else{
        char param[256];
        dbgNxPXL("%s", priv->smf_opt);
        snprintf(param, sizeof(param), "dem=[oMediaScript=[%s]]", priv->smf_opt);
        returnIfErrC(false, !smf_media_video_output_config(priv));
        returnIfErrC(false, !smf_media_ids_start(priv, SMF_STREAM_VIDEO_STREAM_PLAY, param));
    }
    return true;
}
bool smf_media_video_player_stop(smf_media_priv_t* priv){
    dbgNxPL();
    //if(id)smf_video_player_file_stop(id);
    smf_media_audio_output_remove(priv);
    smf_media_video_output_remove(priv);
    smf_media_ids_stop_all(priv);
    return true;
}

bool smf_media_video_player_buffer_stop(smf_media_priv_t* priv){
    dbgNxPL();
    smf_media_video_output_remove(priv);
    smf_media_ids_stop_all(priv);
    return true;
}
//video record
bool smf_media_video_recorder_url_start(smf_media_priv_t* priv) {
    dbgTestPL();
    returnIfErrC(false, !smf_media_audio_input_config(priv));
    returnIfErrC(false, !smf_media_video_input_config(priv));

    char param[256];
    char param1[256];
    snprintf(param, 256, "mux=[url=[%s]]", priv->smf_url);
    snprintf(param1, 256, "fmt=[rate=#16000,ch=#1,bits=#16,width=#2],enc=[keys=aac,br=#12800,pkg=#2]");
    dbgTestPXL("[%02x,%02x,%02x]%s %s", SMF_STREAM_AV_PLAY, SMF_STREAM_AUDIO_PLAY, SMF_STREAM_VIDEO_PLAY, param, param1);
    returnIfErrC(false, !smf_media_ids_start(priv, SMF_STREAM_AUDIO_RECORD, param1));
    returnIfErrC(false, !smf_media_ids_start(priv, SMF_STREAM_AV_RECORD, param));
    // returnIfErrC(false, !smf_media_ids_start(priv, SMF_STREAM_CAMERA, 0));
    return true;
}

bool smf_media_video_recorder_isp_start(smf_media_priv_t* priv) {
    dbgTestPL();
    char param[256];
    snprintf(param, 256, "sink=[url=[%s]", priv->smf_url);
    returnIfErrC(false, !smf_media_ids_start(priv, SMF_STREAM_VIDEO_RECORD1, param));
    return true;
}

bool smf_media_video_recorder_buffer_start(smf_media_priv_t* priv) {
    dbgTestPL();
    return true;
}
// bool smf_media_video_recorder_url_stop(smf_media_priv_t* priv) {
//     dbgTestPL();
//     smf_media_audio_input_remove(priv);
//     smf_media_video_input_remove(priv);
//     smf_media_ids_stop_all(priv);
//     return true;
// }

// bool smf_media_video_recorder_isp_stop(smf_media_priv_t* priv) {
//     dbgTestPL();
//     smf_media_ids_stop_all(priv);
//     return true;
// }

bool smf_media_video_recorder_start(smf_media_priv_t* priv) {
    dbgTestPL();
    if (strlen(priv->smf_opt) == 0) {
        dbgErrPXL("video player arg is null");
        return false;
    }
    if (memcmp(priv->smf_opt, "url", strlen(priv->smf_opt)) == 0) {
        return smf_media_video_recorder_url_start(priv);
    }else if(memcmp(priv->smf_opt, "isp", strlen(priv->smf_opt)) == 0){
        return smf_media_video_recorder_isp_start(priv);
    }else if(memcmp(priv->smf_opt, "buffer", strlen(priv->smf_opt)) == 0){
        return smf_media_video_recorder_buffer_start(priv);
    }else{
        dbgErrPXL("arg not supported");
        return false;
    }
}

bool smf_media_video_recorder_stop(smf_media_priv_t* priv) {
    dbgNxPL();
    smf_media_audio_input_remove(priv);
    smf_media_video_input_remove(priv);
    smf_media_ids_stop_all(priv);
    return true;
}