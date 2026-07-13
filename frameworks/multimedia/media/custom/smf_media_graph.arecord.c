#include "smf_media_graph.api.h"
#include "smf_debug_ext.h"
#include "smf_stream_api.h"
#include "smf_stream_api_default.h"

bool smf_media_audio_recorder_url_start(smf_media_priv_t* priv){
    dbgNxPL();
    uint8_t stream = smf_stream_get_free(SMF_STREAM_RECORD0, SMF_STREAM_RECORD1);
    returnIfErrC(false, !smf_stream_check(stream));
#ifdef SMF_SINGLE_PIPELINE
#else
    returnIfErrC(false, !smf_media_audio_input_config(priv));
#endif
    priv->smf_media_id = stream;
    char param[256];
    int n = snprintf(param, 255, "mux=[url=[%s]]", priv->smf_url);
    const char* opt = priv->smf_opt;
    if (strlen(opt) == 0) {
        dbgNxPXL("opt is null, use default pcm");
        //opt = "fmt=[rate=#16000,ch=#1,bits=#16,width=#2],enc=[keys=opus,br=#32000,dms=#200,haveHead=#1]";
        opt = "fmt=[rate=#16000,ch=#1,bits=#16,width=#2],enc=[keys=pcm,imin=#1920]";
    }
    n += snprintf(param + n, 255 - n, ",%s", opt);

    //dbgNxPXL("[%02x]%s", stream, param);
    bool ret = smf_media_ids_start_wait(priv, stream, param, 1000);
    if(!ret){
        priv->smf_media_id = 0;
#ifdef SMF_SINGLE_PIPELINE
#else
        smf_media_audio_input_remove(priv);
#endif
        dbgErrPXL("smf_media_ids_start_wait error");
        return false;
    }
    return true;
}

bool smf_media_audio_recorder_buffer_start(smf_media_priv_t* priv){
    dbgNxPL();
    uint8_t stream = smf_stream_get_free(SMF_STREAM_RECORD2, SMF_STREAM_RECORD3);
    returnIfErrC(false, !smf_stream_check(stream));
#ifdef SMF_SINGLE_PIPELINE
#else
    returnIfErrC(false, !smf_media_audio_input_config(priv));
#endif
    priv->smf_media_id = stream;
    char param[256];
    int n = snprintf(param, 255, "sink=[cboutput=#%p,priv=#%p]", priv->callback, priv->callback_priv);
    const char* opt = priv->smf_opt;
    if (strlen(opt) == 0) {
        dbgNxPXL("opt is null, use default pcm");
        //opt = "fmt=[rate=#16000,ch=#1,bits=#16,width=#2],enc=[keys=opus,br=#32000,dms=#200,haveHead=#1]";
        opt = "fmt=[rate=#16000,ch=#1,bits=#16,width=#2],enc=[keys=pcm,imin=#1920]";
    }
    n += snprintf(param + n, 255 - n, ",%s", opt);

    //dbgNxPXL("[%02x]%s", stream, param);
    bool ret = smf_media_ids_start_wait(priv, stream, param, 1000);
    if(!ret){
        priv->smf_media_id = 0;
#ifdef SMF_SINGLE_PIPELINE
#else
        smf_media_audio_input_remove(priv);
#endif
        dbgErrPXL("smf_media_ids_start_wait error");
        return false;
    }
    return true;
}

bool smf_media_audio_recorder_i2s_start(smf_media_priv_t* priv){
    dbgNxPL();
    returnIfErrC(false, !smf_media_audio_input_config(priv));
    uint8_t stream = SMF_STREAM_RECORD5;
    returnIfErrC(false, !smf_stream_check(stream));
    priv->smf_media_id = stream;
    bool ret = smf_media_ids_start_wait(priv, stream, 0, 1000);
    if(!ret){
        priv->smf_media_id = 0;
        smf_media_audio_input_remove(priv);
        dbgErrPXL("smf_media_ids_start_wait error");
        return false;
    }
    return true;
}

bool smf_media_audio_recorder_tdm_start(smf_media_priv_t* priv){
    uint8_t stream = SMF_STREAM_AUDIO_BRIDGE0;
    returnIfErrC(false, !smf_stream_check(stream));
    priv->smf_media_id = stream;
    bool ret = smf_media_ids_start_wait(priv, stream, 0, 1000);
    if(!ret){
        priv->smf_media_id = 0;
        return false;
    }
    return true;
}

bool smf_media_audio_recorder_voice_rec_start(smf_media_priv_t* priv){
    dbgNxPL();
    returnIfErrC(false, !smf_media_audio_input_config(priv));
    uint8_t stream = SMF_STREAM_RECORD4;
    returnIfErrC(false, !smf_stream_check(stream));
    priv->smf_media_id = stream;
    char param[256];
    int n = snprintf(param, 255, "sink=[cboutput=#%p,priv=#%p]", priv->callback, priv->callback_priv);
    const char* opt = priv->smf_opt;
    if (strlen(opt) == 0) {
        dbgNxPXL("opt is null, use default pcm");
        //opt = "fmt=[rate=#16000,ch=#1,bits=#16,width=#2],enc=[keys=opus,br=#32000,dms=#200,haveHead=#1]";
        opt = "fmt=[rate=#16000,ch=#1,bits=#16,width=#2],enc=[keys=pcm,imin=#1920]";
    }
    n += snprintf(param + n, 255 - n, ",%s", opt);

    //dbgNxPXL("[%02x]%s", stream, param);
    bool ret = smf_media_ids_start_wait(priv, stream, param, 1000);
    if(!ret){
        priv->smf_media_id = 0;
        smf_media_audio_input_remove(priv);
        dbgErrPXL("smf_media_ids_start_wait error");
        return false;
    }
    return true;
}

bool smf_media_audio_recorder_tdm_stop(smf_media_priv_t* priv){
    smf_media_ids_stop_all(priv);
    return true;
}

bool smf_media_audio_recorder_stop(smf_media_priv_t* priv){
    smf_media_ids_stop_all(priv);
#ifdef SMF_SINGLE_PIPELINE
#else
    smf_media_audio_input_remove(priv);
#endif
    return true;
}

bool smf_media_audio_recorder_set_volume(smf_media_priv_t* priv, int vol) {
    dbgNxPL();
    returnIfErrC(false, !priv);
    returnIfErrC(false, !priv->smf_media_id);
    dbgNxPDL(vol);
    if(vol == 0){
        return smf_stream_set_mute(priv->smf_media_id, true);
    }else{
        return smf_stream_set_volume(priv->smf_media_id, vol);
    }
}
bool smf_media_audio_recorder_set_mute(smf_media_priv_t* priv, bool mute) {
    dbgNxPL();
    returnIfErrC(false, !priv);
    returnIfErrC(false, !priv->smf_media_id);
    return smf_stream_set_mute(priv->smf_media_id, mute);
}
bool smf_media_audio_recorder_pause(smf_media_priv_t* priv){
    dbgNxPL();
    returnIfErrC(false, !priv);
    returnIfErrC(false, !priv->smf_media_id);
    if(smf_stream_pause(priv->smf_media_id)){
        priv->status = SMF_STREAM_STATUS_pause;
        return true;
    }
    return false;
}
bool smf_media_audio_recorder_resume(smf_media_priv_t* priv){
    dbgNxPL();
    returnIfErrC(false, !priv);
    returnIfErrC(false, !priv->smf_media_id);
    if(smf_stream_resume(priv->smf_media_id)){
        priv->status = SMF_STREAM_STATUS_play;
        return true;
    }
    return false;
}