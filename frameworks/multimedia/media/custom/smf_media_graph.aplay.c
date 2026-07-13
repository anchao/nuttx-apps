#include "smf_media_graph.api.h"
#include "smf_debug_ext.h"
#include "smf_stream_api.h"
#include "smf_stream_api_default.h"

bool smf_media_audio_player_url_start(smf_media_priv_t* priv){
    dbgNxPL();
    uint8_t stream = smf_stream_get_free(SMF_STREAM_PLAY1, SMF_STREAM_PLAY3);
    returnIfErrC(false, !smf_stream_check(stream));
#ifdef SMF_SINGLE_PIPELINE
#else
    returnIfErrC(false, !smf_media_audio_output_config(priv));
#endif
    priv->smf_media_id = stream;

    #ifdef SMF_USE_IO_BUFFER
    extern uint32_t __audio_file_start[];
    extern uint32_t __audio_file_end[];
    char arr[50];
    int32_t size = (int32_t)__audio_file_end - (int32_t)__audio_file_start;
    dbgNxPXL("%p~%p,%d", __audio_file_start, __audio_file_end, size);
    sprintf(arr, "buff://%p_%d.mp3", __audio_file_start, size);
    int len = strlen(arr);
    memset(priv->smf_url, 0, sizeof(priv->smf_url));
    memcpy(priv->smf_url, arr, len);
    priv->smf_url[len] = '\0';
	#endif

    char param[256];
    memset(param, 0, 256);
    const char* opt = priv->smf_opt;
    if (strlen(opt) != 0) {
        //sprintf(param, "dem=[url=[%s],vol=#%u,oMediaScript=[%s],fsize=#%u,meta=#%p,cache=#4096]", priv->smf_url, priv->volume, opt, 1920, &priv->meta); //maybe need modify fsize
        snprintf(param, 256, "dem=[url=[%s],vol=#%u,meta=#%p,cache=#4096,%s]", priv->smf_url, priv->volume, &priv->meta, opt);
    }else{
        snprintf(param, 256, "dem=[url=[%s],vol=#%u,meta=#%p,cache=#4096]", priv->smf_url, priv->volume, &priv->meta);
    }
    // dbgNxPXL("[%02x]%s", stream, param);
    bool ret = smf_media_ids_start_wait(priv, stream, param, 10000);
    if(!ret){
        priv->smf_media_id = 0;
#ifdef SMF_SINGLE_PIPELINE
#else
        smf_media_audio_output_remove(priv);
#endif
        priv->status = SMF_STREAM_STATUS_null;
        dbgErrPXL("smf_media_ids_start_wait error");
        return false;
    }
    priv->status = SMF_STREAM_STATUS_play;
    // dbgNxPXL("%p %02x %d", priv, priv->smf_media_id, priv->status);
    return true;
}

bool smf_media_audio_player_buffer_start(smf_media_priv_t* priv){
    dbgNxPL();
    uint8_t stream = smf_stream_get_free(SMF_STREAM_PLAY4, SMF_STREAM_PLAY5);
    returnIfErrC(false, !smf_stream_check(stream));
#ifdef SMF_SINGLE_PIPELINE
#else
    returnIfErrC(false, !smf_media_audio_output_config(priv));
#endif
    priv->smf_media_id = stream;
    const char* opt = priv->smf_opt;
    if (strlen(opt) == 0) {
        dbgNxPXL("opt is null, use default pcm 16000 1 16");
        opt = "oMediaScript=[codec=pcm,rate=#16000,ch=#1,bits=#16]";
        //opt = "codec=opus";
        //opt = "codec=aac,package=2";//0:raw,2:adts,6:mcp1
    }
    //dbgNxPSL(opt);
    char param[256];
    memset(param, 0, 256);
    //sprintf(param, "dem=[oMediaScript=[%s],vol=#%u,thread=#0]", opt, priv->volume);
    snprintf(param, 256, "dem=[vol=#%u,thread=#0,%s]", priv->volume, opt);
    //dbgNxPXL("[%02x]%s", stream, param);
    bool ret = smf_media_ids_start_wait(priv, stream, param, 1000);
    if(!ret){
        priv->smf_media_id = 0;
#ifdef SMF_SINGLE_PIPELINE
#else
        smf_media_audio_output_remove(priv);
#endif
        priv->status = SMF_STREAM_STATUS_null;
        dbgErrPXL("smf_media_ids_start_wait error");
        return false;
    }
    priv->status = SMF_STREAM_STATUS_play;
    return true;
}

bool smf_media_audio_player_i2s_start(smf_media_priv_t* priv){
    dbgNxPL();
    returnIfErrC(false, !smf_media_audio_output_config(priv));
    uint8_t stream = SMF_STREAM_PLAY7;
    returnIfErrC(false, !smf_stream_check(stream));
    priv->smf_media_id = stream;
    bool ret = smf_media_ids_start_wait(priv, stream, 0, 1000);
    if(!ret){
        priv->smf_media_id = 0;
        smf_media_audio_output_remove(priv);
        priv->status = SMF_STREAM_STATUS_null;
        dbgErrPXL("smf_media_ids_start_wait error");
        return false;
    }
    priv->status = SMF_STREAM_STATUS_play;
    return true;
}
bool smf_media_audio_player_i2s_set_volume(smf_media_priv_t* priv){
    return true;
}

bool smf_media_audio_player_prompt_start(smf_media_priv_t* priv){
    dbgNxPL();
    uint8_t stream = smf_stream_get_free(SMF_STREAM_PLAY6, SMF_STREAM_PLAY6);
    returnIfErrC(false, !smf_stream_check(stream));
#ifdef SMF_SINGLE_PIPELINE
#else
    returnIfErrC(false, !smf_media_audio_output_config(priv));
#endif
    priv->smf_media_id = stream;
    char param[256];
    memset(param, 0, 256);
    const char* opt = priv->smf_opt;
    if (strlen(opt) != 0) {
        //sprintf(param, "dem=[url=[%s],vol=#%u,oMediaScript=[%s],fsize=#%u,meta=#%p,cache=#4096]", priv->smf_url, priv->volume, opt, 1920, &priv->meta); //maybe need modify fsize
        snprintf(param, 255, "dem=[url=[%s],vol=#%u,meta=#%p,cache=#4096,%s]", priv->smf_url, priv->volume, &priv->meta, opt);
    }else{
        snprintf(param, 255, "dem=[url=[%s],vol=#%u,meta=#%p,cache=#4096]", priv->smf_url, priv->volume, &priv->meta);
    }
    // dbgNxPXL("[%02x]%s", stream, param);
    bool ret = smf_media_ids_start_wait(priv, stream, param, 1000);
    if(!ret){
        priv->smf_media_id = 0;
#ifdef SMF_SINGLE_PIPELINE
#else
        smf_media_audio_output_remove(priv);
#endif
        priv->status = SMF_STREAM_STATUS_null;
        dbgErrPXL("smf_media_ids_start_wait error");
        return false;
    }
    priv->status = SMF_STREAM_STATUS_play;
    return true;
}

bool smf_media_audio_player_stop(smf_media_priv_t* priv){
    dbgNxPL();
    priv->status = SMF_STREAM_STATUS_null;
    smf_media_ids_stop_all(priv);
#ifdef SMF_SINGLE_PIPELINE
#else
    smf_media_audio_output_remove(priv);
#endif
    return true;
}

bool smf_media_audio_player_set_volume(smf_media_priv_t* priv, int vol){
    returnIfErrC(false, !priv);
    returnIfErrC(false, !priv->smf_media_id);
    // dbgNxPDL(vol);
    return smf_stream_set_volume(priv->smf_media_id, vol);
}
bool smf_media_audio_player_set_mute(smf_media_priv_t* priv, bool mute){
    dbgNxPL();
    returnIfErrC(false, !priv);
    returnIfErrC(false, !priv->smf_media_id);
    return smf_stream_set_mute(priv->smf_media_id, mute);
}
bool smf_media_audio_player_pause(smf_media_priv_t* priv){
    dbgNxPL();
    returnIfErrC(false, !priv);
    returnIfErrC(false, !priv->smf_media_id);
    if(smf_stream_pause(priv->smf_media_id)){
        priv->status = SMF_STREAM_STATUS_pause;
        return true;
    }
    return false; 
}
bool smf_media_audio_player_resume(smf_media_priv_t* priv){
    dbgNxPL();
    returnIfErrC(false, !priv);
    returnIfErrC(false, !priv->smf_media_id);
    if(smf_stream_resume(priv->smf_media_id)){
        priv->status = SMF_STREAM_STATUS_play;
        return true;
    }
    return false;
}
bool smf_media_audio_player_seek(smf_media_priv_t* priv, uint32_t timepoint) {
    returnIfErrC(false, !priv);
    returnIfErrC(false, !priv->smf_media_id);
    dbgNxPXL("%02x,%ums", priv->smf_media_id, timepoint);
    return smf_stream_seek(priv->smf_media_id, timepoint);
}
// bool smf_media_audio_player_set_all_volume(int vol){
//     return smf_stream_set_volume(SMF_STREAM_MIX,vol);
// }
// bool smf_media_audio_player_set_all_mute(bool mute){
//     return smf_stream_set_mute(SMF_STREAM_MIX,1);
// }

bool smf_media_volume_set(uint64_t type, const char* arg){
    dbgNxPL();
    int vol = 0;
    int num_int = atoi(arg);
    if(( (type == FCC3('s','c','o')) || (type == FCC4('a','2','d','p')) )){
        vol = (int)( (float)num_int/15*SMF_VOLUME_MAX );
    }else{
        vol = (int)( (float)num_int/10*SMF_VOLUME_MAX );
    }
    dbgNxPXL("type:%s arg:%s vol:%d \n",(const char*)&type, arg, vol);

    switch (type)
    {
    case FCC3('s','c','o'):{
#if SMF_SCO_INTERSYS
       smf_media_priv_t* priv1 = smf_media_ids_get(SMF_STREAM_SCO_INTERSYS_DOWN);
#else
        smf_media_priv_t* priv1 = smf_media_ids_get(SMF_STREAM_SCO_DOWN);
#endif
        if(priv1)smf_media_audio_btsco_set_downvolme(priv1, vol);
        break;
    }
    case FCC4('a','2','d','p'):{
        smf_media_priv_t* priv1 = smf_media_ids_get(SMF_STREAM_A2DP_DOWN0);
        if(priv1)smf_media_audio_player_a2dp_set_volume(priv1, vol);
        break;
    }
    case FCC5('m','u','s','i','c'):{
        for(int i=SMF_STREAM_PLAY1;i<=SMF_STREAM_PLAY5;i++){
            smf_media_priv_t* priv = smf_media_ids_get((enum smf_stream_e)i);
            if(priv)smf_media_audio_player_set_volume(priv, vol);
        }
        break;
    }
    case FCC6('r','e','c','o','r','d'):{
        smf_media_priv_t* priv0 = smf_media_ids_get(SMF_STREAM_RECORD0);
        smf_media_priv_t* priv1 = smf_media_ids_get(SMF_STREAM_RECORD1);
        if(priv0)smf_media_audio_recorder_set_volume(priv0, vol);
        if(priv1)smf_media_audio_recorder_set_volume(priv1, vol);
        break;
    }
    case FCC5('a','l','a','r','m'):
        /* code */
        break;
    case FCC3('t','t','s'):
        /* code */
        break;
    case FCC4('r','i','n','g'):{
        smf_media_priv_t* priv1 = smf_media_ids_get(SMF_STREAM_PLAY6);
        if(priv1)smf_media_audio_player_set_volume(priv1, vol);
        break;
    }
    case FCC3('c','u','s'):{//i2splay
        smf_media_priv_t* priv1 = smf_media_ids_get(SMF_STREAM_PLAY7);
        if(priv1)smf_media_audio_player_set_volume(priv1, vol);
        break;
    }
    default:
        dbgWarnPXL("Unsupported type");
        break;
    }
    return true;
}



