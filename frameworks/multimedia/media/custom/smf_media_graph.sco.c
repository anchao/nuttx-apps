#include "smf_media_graph.api.h"
#include "smf_debug_ext.h"
#include "smf_media_audio_path_bt.h"
#include "smf_stream_api.h"
#include "smf_stream_api_default.h"

static bool agsco = false;

static int smf_media_audio_btsco_get_downvol(void){
    smf_media_policy_t* policy = (smf_media_policy_t*)smf_media_policy_list_get("VolSCO");
    if(!policy){
        dbgErrPXL("VolSCO is NULL");
        return -1;
    }
    const char* cmd = policy->cmd;
    int vol = 0;
    if( memcmp(cmd, "volume", strlen(cmd)) == 0 ){
        vol = atoi(policy->arg);
        return (int)( (float)vol/15*SMF_VOLUME_MAX );
    }else{
        dbgErrPXL("VolSCO cmd unsupport %s", cmd);
        return -1;
    }
}

smf_media_priv_t* smf_media_audio_btsco_start(uint8_t type) {
    dbgNxPL();
    int rate = 0;
    const char* codec = 0;
    switch (type) {
    case 1:codec = "cvsd"; rate = 8000; break;
    case 2:codec = "msbc"; rate = 16000;  break;
    default:dbgErrPXL("%u", type); return 0;
    }
    //
    smf_media_priv_t* priv = (smf_media_priv_t*)zalloc(sizeof(smf_media_priv_t));
    returnIfErrC(0, !priv);
    bool ret = false;

#ifdef SMF_SINGLE_PIPELINE
#else
    ret = smf_media_audio_output_config(priv);
    if(!ret){
        free(priv);
        return 0;
    }
    ret = smf_media_audio_input_config(priv);
    if(!ret){
        smf_media_audio_output_remove(priv);
        free(priv);
        return 0;
    }
#endif
    //
    int vol = smf_media_audio_btsco_get_downvol();
    if(vol<0){
        dbgWarnPDL(vol);
        vol = SMF_VOLUME_MAX;
    }
    dbgNxPDL(vol);
    char param[256];
    //
    uint8_t scodn = SMF_STREAM_SCO_DOWN;
    uint8_t scoup = SMF_STREAM_SCO_UP;
#if SMF_SCO_INTERSYS
    //memset(param, 0, 256);
    sprintf(param, "svc=[codec=%s,volume=#%u]", codec, vol);
    ret = smf_media_ids_start(priv, SMF_STREAM_SCO_SERVICE, param);
    if (!ret)goto err;
    scodn = SMF_STREAM_SCO_INTERSYS_DOWN;
    scoup = SMF_STREAM_SCO_INTERSYS_UP;
#endif
    //
    //memset(param, 0, 256);
    sprintf(param, "src=[btcodec=%s,vol=#%u],dec=[keys=%s]", codec, vol, codec);
    ret = smf_media_ids_start(priv, scodn, param);
    if (!ret)goto err;
    priv->smf_media_id = scodn;
    //
    //memset(param, 0, 256);
    sprintf(param, "enc=[keys=%s],fmt=[rate=#%u,ch=#1,bits=#16]", codec, rate);
    ret = smf_media_ids_start(priv, scoup, param);
    if (!ret)goto err;
    priv->smf_media_id_ctrl = scoup;
    return priv;

err:
    smf_media_audio_btsco_stop(priv);
    return 0;

}
bool smf_media_audio_btsco_stop(smf_media_priv_t* priv){
    dbgNxPL();
    if(agsco){
        return smf_media_audio_agsco_stop(priv);
    }else{
        smf_media_ids_stop_all(priv);
#ifdef SMF_SINGLE_PIPELINE
#else
        smf_media_audio_output_remove(priv);
        smf_media_audio_input_remove(priv);
#endif
    }
    free(priv);
    return true;
}

smf_media_priv_t* smf_media_audio_agsco_start(uint8_t type, uint32_t vol){
    dbgNxPL();
    const char* codec = 0;
    switch (type) {
    case 1:codec = "cvsd"; break;
    case 2:codec = "msbc"; break;
    default:dbgErrPXL("%u", type); return 0;
    }
    //
    smf_media_priv_t* priv = (smf_media_priv_t*)zalloc(sizeof(smf_media_priv_t));
    returnIfErrC(0, !priv);
    //
    char param[256];
    sprintf(param, "src=[vol=#%u]", vol);
    returnIfErrC(0, !smf_media_ids_start(priv, SMF_STREAM_IMS_DOWN_TO_SCO, param));
    //
    sprintf(param, "enc=[keys=%s]", codec);
    returnIfErrC(0, !smf_media_ids_start(priv, SMF_STREAM_IMS_UP_FROM_SCO, param));
    agsco = true;
    return priv;
}
bool smf_media_audio_agsco_stop(smf_media_priv_t* priv){
    dbgNxPL();
    returnIfErrC(0, !priv);
    returnIfErrC(0, !smf_media_ids_stop_all(priv));
    return true;
#if 0
    smf_media_remove_dn_btpcm_sink();
    smf_media_remove_up_btpcm_source();
    agsco = false;
    return true;
#endif
}

bool smf_media_audio_btsco_set_downvolme(smf_media_priv_t* priv, uint32_t vol){
    returnIfErrC(false, !priv);
    dbgNxPDL(vol);
#if SMF_SCO_INTERSYS
    return smf_stream_set_volume(SMF_STREAM_SCO_SERVICE, vol);
#else
    return smf_stream_set_volume(priv->smf_media_id, vol);
#endif
}
bool smf_media_audio_btsco_set_mic_mute(smf_media_priv_t* priv, bool mute){
    returnIfErrC(false, !priv);
    dbgNxPDL(mute);
    return smf_stream_set_mute(priv->smf_media_id_ctrl, mute);
}

bool smf_media_audio_agsco_set_downvolme(smf_media_priv_t* priv){
    return false;
}
bool smf_media_audio_agsco_set_mic_mute(smf_media_priv_t* priv){
    return false;
}