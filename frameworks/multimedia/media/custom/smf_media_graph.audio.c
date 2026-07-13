#include "smf_media_graph.api.h"
#include "smf_debug_ext.h"
#include "smf_stream_api.h"
#include "smf_stream_api_default.h"
#include "smf_media_audio_path_bt.h"

static smf_media_priv_t _default_priv;
static smf_media_priv_t* _output_priv[8];
static char _output_type[16];
static bool smf_media_audio_output_config_x(smf_media_priv_t* priv, const char* cmd) {
    returnIfErrC(false, !priv);
    const char* cmdx = cmd;
    if (memcmp(cmdx, "a2dp", strlen(cmdx)) == 0){
        const smf_media_audio_bt_codec_cfg_t* info = smf_media_audio_bt_get_codec_info();
        if(info){
            bool ret = smf_media_audio_a2dp_output_start_send_bt();
            if(!ret)return false;
            return smf_media_audio_output_a2dpsink(priv, info);  
        }else{
            smf_media_policy_t* policy = (smf_media_policy_t*)smf_media_policy_list_get("DefPlay");
            if(!policy){
                dbgErrPXL("get DefPlay error!");
                return false;
            }
            cmdx = policy->cmd;
        }
    }

    if (memcmp(cmdx, "spk0", strlen(cmdx)) == 0) {
        returnIfErrC(false, !smf_media_ids_start(priv, SMF_STREAM_SPK0, 0));
    }
    else if (memcmp(cmdx, "spk1", strlen(cmdx)) == 0) {
        returnIfErrC(false, !smf_media_ids_start(priv, SMF_STREAM_SPK1, 0));
    }
    else if (memcmp(cmdx, "spk2", strlen(cmdx)) == 0) {
        returnIfErrC(false, !smf_media_ids_start(priv, SMF_STREAM_SPK2, 0));
    }
    else {
        dbgErrPXL("audio output type unsupport");
        return false;
    }
    return true;
}
static bool smf_media_audio_output_remove_x(smf_media_priv_t* priv, const char* cmd) {
    returnIfErrC(false, !priv);
    smf_media_ids_stop(priv, SMF_STREAM_A2DP_UP);
    smf_media_ids_stop(priv, SMF_STREAM_SPK0);
    smf_media_ids_stop(priv, SMF_STREAM_SPK1);
    smf_media_ids_stop(priv, SMF_STREAM_SPK2);
    return true;
}

bool smf_media_audio_output_config(smf_media_priv_t* priv){
    dbgNxPL();
    returnIfErrC(false, !priv);
    smf_media_policy_t* policy = (smf_media_policy_t*)smf_media_policy_list_get("SelPlay");
    if(!policy){
        dbgErrPXL("audio output SelPlay is NULL");
        return false;
    }
    const char* cmd = policy->cmd;
    returnIfErrC(false, !smf_media_audio_output_config_x(priv, cmd));
    strcpy(_output_type, cmd);
    bool flag = false;
    for (int i = 1; i < 8; i++) {
        if (!_output_priv[i]) {
            _output_priv[i] = priv;
            priv->device_id = i;
            flag = true;
            break;
        }
    }
    if(!flag){
        dbgErrPXL("output_priv error");
        return false;
    }
    return true;
}

bool smf_media_audio_output_remove(smf_media_priv_t* priv){
    dbgNxPL();
    returnIfErrC(false, !priv);
    smf_media_policy_t* policy = (smf_media_policy_t*)smf_media_policy_list_get("SelPlay");
    if(!policy){
        dbgErrPXL("audio output SelPlay is NULL");
        return true;
    }
    const char* cmd = policy->cmd;
    smf_media_audio_output_remove_x(priv, cmd);
    //
    _output_priv[priv->device_id] = 0;
    priv->device_id = 0;
    bool flag = false;
    for (int i = 1; i < 8; i++) {
        if (_output_priv[i]) {
            flag = true;
            break;
        }
    }
    if(!flag){
        memset(_output_type, 0, sizeof(_output_type));
    }
    if (!flag && memcmp(cmd, "a2dp", strlen(cmd)) == 0){
        smf_media_audio_a2dp_output_close_send_bt();
    }
    return true;
}

void* smf_media_audio_output_switch(void* arg) {
    dbgNxPL();
    bool output = false;
    for (int i = 0; i < strlen(_output_type); i++) {
        if (_output_type[i] != 0) {
            output = true;
            break;
        }
    }
    if(!output)return NULL;
    smf_media_policy_t* policy = (smf_media_policy_t*)smf_media_policy_list_get("SelPlay");
    if (!policy) {
        dbgErrPXL("audio output SelPlay is NULL");
        return NULL;
    }
    const char* cmd = policy->cmd;
    if (memcmp(cmd, "a2dp", strlen(cmd)) == 0){
        dbgNxPL();
        int timecut = 40;
        void* info = 0;
        while(timecut--){
            info = (void *)smf_media_audio_bt_get_codec_info();
            if(info)break;
            usleep(50*1000);
        }
        if (!info){
            dbgErrPXL("audio a2dp info is NULL, swtich to a2dp failed");
            return NULL;
        }
    }
    if (memcmp(cmd, _output_type, strlen(cmd)) == 0) {
        dbgWarnPXL("the same output.");
        return NULL;
    }
    for (int i = 1; i < 8; i++) {
        smf_media_priv_t* priv = _output_priv[i];
        if (priv) {
            smf_media_audio_output_remove_x(priv, cmd);
            priv->device_id = 0;
        }
    }
    for (int i = 1; i < 8; i++) {
        smf_media_priv_t* priv = _output_priv[i];
        if (priv) {
            returnIfErrC(NULL, !smf_media_audio_output_config_x(priv, cmd));
            priv->device_id = i;
        }
    }
    memset(_output_type, 0, strlen(_output_type));
    strcpy(_output_type, cmd);
    return NULL;
}

bool smf_media_audio_output_volume(smf_media_priv_t* priv, uint32_t volume) {
    return smf_stream_set_volume(SMF_STREAM_MIX, volume);
}

bool smf_media_audio_output_mute(smf_media_priv_t* priv, bool mute) {
    return smf_stream_set_mute(SMF_STREAM_MIX, mute);
}

bool smf_media_audio_input_config(smf_media_priv_t* priv){
    dbgNxPL();
    returnIfErrC(false, !priv);
    smf_media_policy_t* policy = (smf_media_policy_t*)smf_media_policy_list_get("SelCap");
    if(!policy){
        dbgErrPXL("audio input SelCap is NULL");
        return false;
    }
    const char* cmd = policy->cmd;
    uint32_t stream = 0;
    if( memcmp(cmd, "mic0", strlen(cmd)) == 0 ){
        stream = SMF_STREAM_MIC0;
    }else if( memcmp(cmd, "mic1", strlen(cmd)) == 0 ){
        stream = SMF_STREAM_MIC1;
    }else if( memcmp(cmd, "mic2", strlen(cmd)) == 0 ){
        stream = SMF_STREAM_MIC2;
    }else if( memcmp(cmd, "mic3", strlen(cmd)) == 0 ){
        stream = SMF_STREAM_MIC3;
    }else{
        dbgErrPXL("audio input type unsupport");
        return false;
    }
    returnIfErrC(false, !smf_media_ids_start(priv, stream, 0));
    priv->smf_media_id_ctrl = stream;
    return true;
}
bool smf_media_audio_input_remove(smf_media_priv_t* priv){
    dbgNxPL();
    returnIfErrC(false, !priv);
    smf_media_ids_stop(priv, SMF_STREAM_MIC0);
    smf_media_ids_stop(priv, SMF_STREAM_MIC1);
    smf_media_ids_stop(priv, SMF_STREAM_MIC2);
    smf_media_ids_stop(priv, SMF_STREAM_MIC3);
    return true;
}

bool smf_media_audio_default_config(void) {
    dbgNxPL();
    smf_media_audio_input_config(&_default_priv);
    smf_media_audio_output_config(&_default_priv);
    return true;
}
bool smf_media_audio_default_remove(void) {
    dbgNxPL();
    smf_media_audio_input_remove(&_default_priv);
    smf_media_audio_output_remove(&_default_priv);
    return true;
}