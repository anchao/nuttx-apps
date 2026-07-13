#include "smf_media_graph.api.h"
#include "smf_debug_ext.h"
#include "kfifo.h"
#include "smf_media_audio_path_bt.h"
#include "smf_pool.h"
#include "smf_media_arg_parse.h"
#include "smf_stream_api.h"
#include "smf_stream_api_default.h"

static struct kfifo* _kfifo = 0;
static void* g_kfifo_buffer = 0;

static bool smf_media_kfifo_init(void){
    dbgNxPL();
    _kfifo = smf_find_pool_alloc("psramnc", sizeof(struct kfifo));
    g_kfifo_buffer = smf_find_pool_alloc("psramnc", SMF_AUDIO_KFIFO_SIZE);
    if(!_kfifo || !g_kfifo_buffer){
        dbgErrPXL("kfifo malloc failed");
        return false;
    }
    memset(g_kfifo_buffer, 0, SMF_AUDIO_KFIFO_SIZE);
    kfifo_init(_kfifo, g_kfifo_buffer, SMF_AUDIO_KFIFO_SIZE);
    _kfifo->in = _kfifo->out = 0;
    return true;
}
void smf_media_kfifo_deinit(void){
    dbgNxPL();
    if(g_kfifo_buffer)smf_find_pool_free("psramnc", g_kfifo_buffer);
    if(_kfifo)smf_find_pool_free("psramnc", _kfifo);
    g_kfifo_buffer = NULL;
    _kfifo = NULL;
}
//a2dp_source_send_info_t
uint32_t smf_media_kfifo_data_pull(void* buffer, uint32_t len) {
    if(!_kfifo)return 0;
    return kfifo_get(_kfifo, (uint8_t*)buffer, len);
}
uint32_t smf_media_kfifo_data_push(void* buffer, uint32_t len) {
    if(!_kfifo)return 0;
    uint32_t free_size = kfifo_get_free_space(_kfifo);
    int ret = 0;
    if (len <= free_size) {
        ret = kfifo_put(_kfifo, (uint8_t*)buffer, len);
    }
    else {
        dbgNxPXL("kfifo push failed: %u<%u", free_size, len);
    }
    return ret;
}
uint32_t smf_media_kfifo_data_peek(void* buffer, uint32_t len) {
    if(!_kfifo)return 0;
    return kfifo_peek_to_buf(_kfifo, (uint8_t*)buffer, len);
}
uint32_t smf_media_kfifo_data_size(void) {
    if(!_kfifo)return 0;
    return kfifo_len(_kfifo);
}
uint32_t smf_media_kfifo_data_free_size(void) {
    if(!_kfifo)return 0;
    return kfifo_get_free_space(_kfifo);
}

bool smf_media_audio_output_a2dpsink(smf_media_priv_t* priv, const void* info){
    dbgNxPL();
    const smf_media_audio_bt_codec_cfg_t* cfg = (smf_media_audio_bt_codec_cfg_t*)info;
    char param[256];
    int n = sprintf(param, "fmt=[rate=#%u,ch=#%u,bits=#%u]"
        ",a2dp=[kfifo=#%p]"
        , cfg->codec_param.a2dp.sample_rate, 2, 16
        , _kfifo
    );
    if (cfg->codec_param.a2dp.codec_type == 0) {//sbc
        sprintf(param + n, ",enc=[keys=sbc,allocMethod=#1,bitPool=#%u,channelMode=#%u,numBlocks=#%u,numSubBands=#%u]"
            , cfg->codec_param.a2dp.diff_param.sbc.s16BitPool
            , cfg->codec_param.a2dp.diff_param.sbc.s16ChannelMode
            , cfg->codec_param.a2dp.diff_param.sbc.s16NumOfBlocks
            , cfg->codec_param.a2dp.diff_param.sbc.s16NumOfSubBands
            );
    }
    else if (cfg->codec_param.a2dp.codec_type == 2) {//aac
        int aot = 129;//A2DP_AAC_OBJECT_TYPE_MPEG2_LC
        //switch (cfg->codec_param.a2dp.diff_param.aac.u16ObjectType)
        //{
        //case 0x80://A2DP_AAC_OBJECT_TYPE_MPEG2_LC:
        //    aot = 129;
        //    break;
        //case 0x40://A2DP_AAC_OBJECT_TYPE_MPEG4_LC:
        //    aot = 2;
        //    break;
        //default:
        //    dbgWarnPXL("aac ObjectType Not Supported %d, default 2(MPEG4_LC)", cfg->codec_param.a2dp.diff_param.aac.u16ObjectType);
        //    aot = 2;
        //    break;
        //}
        sprintf(param + n, ",enc=[keys=aac,aot=#%u,br=#%u,vbr=#%u,layer=#0,package=#6]"
            , aot
            , cfg->codec_param.a2dp.bit_rate
            , cfg->codec_param.a2dp.diff_param.aac.u16VariableBitRate
        );
    }
    else {
        dbgWarnPXL("unsupport codec", cfg->codec_param.a2dp.codec_type);
        return false;
    }
    returnIfErrC(false, !smf_media_ids_start(priv, SMF_STREAM_A2DP_UP, param));
    return true;
}

static int smf_media_audio_player_a2dp_get_volume(void){
    smf_media_policy_t* policy = (smf_media_policy_t*)smf_media_policy_list_get("VolMedia");
    if(!policy){
        dbgErrPXL("VolMedia is NULL");
        return -1;
    }
    const char* cmd = policy->cmd;
    int vol = 0;
    if( memcmp(cmd, "volume", strlen(cmd)) == 0 ){
        vol = atoi(policy->arg);
        return (int)( (float)vol/15*SMF_VOLUME_MAX );
    }else{
        dbgErrPXL("VolMedia cmd unsupport %s", cmd);
        return -1;
    }
}

smf_media_priv_t* smf_media_audio_player_a2dp_start(const char* codec){
    dbgNxPL();
    smf_media_priv_t* priv = zalloc(sizeof(smf_media_priv_t));
    returnIfErrC(0, !priv);
    bool ret = smf_media_audio_output_config(priv);
    if(!ret){
        dbgErrPXL("audio player set output failed");
        free(priv);
        return 0;
    }
    int vol = smf_media_audio_player_a2dp_get_volume();
    if(vol < 0){
        dbgWarnPDL(vol);
        vol = SMF_VOLUME_MAX;
    }
    dbgNxPDL(vol);
    ret = smf_media_kfifo_init();
    if(!ret)goto err;

    uint8_t stream = SMF_STREAM_A2DP_DOWN0;
    ret = smf_stream_check(stream);
    if(!ret)goto err;
    priv->smf_media_id = stream;
    
    char param[256] = {0};
    sprintf(param, "a2dp=[omediaScript=[codec=%s,pkg=#6],kfifo=#%u,vol=#%u]", codec, (uint32_t)_kfifo, vol);
    dbgNxPXL("[%02x]%s", stream, param);
    ret = smf_media_ids_start_wait(priv, stream, param, 1000);
    if(!ret){
        priv->smf_media_id = 0;
        dbgErrPXL("smf_media_ids_start_wait timeout");
        goto err;
    }
    return priv;

err:
    dbgErrPXL("goto error");
    smf_media_kfifo_deinit();
    smf_media_audio_output_remove(priv);
    free(priv);
    return 0;
}

void smf_media_audio_player_a2dp_stop(smf_media_priv_t* priv){
    dbgNxPL();
    if(!priv)return;
    smf_media_kfifo_deinit();
    smf_media_audio_player_stop(priv);
    free(priv);
}

bool smf_media_audio_player_a2dp_set_volume(smf_media_priv_t* priv, uint32_t volume) {
    if(!priv)return false;
    dbgNxPDL(volume);
    return smf_stream_set_volume(priv->smf_media_id, volume);
}

bool smf_media_audio_a2dp_output_start_send_bt(void){
    dbgNxPL();
    const void* info = smf_media_audio_bt_get_codec_info();
    if (info && !g_kfifo_buffer && !_kfifo){
        dbgNxPL();
        smf_media_audio_bt_ctrl_send(SMF_MEDIA_AUDIO_BT_CTRL_START);
        if(!smf_media_kfifo_init())return false;
    }
    return true;
}

bool smf_media_audio_a2dp_output_close_send_bt(void){
    dbgNxPL();
    const void* info = smf_media_audio_bt_get_codec_info();
    if (info){
        dbgNxPL();
        smf_media_audio_bt_ctrl_send(SMF_MEDIA_AUDIO_BT_CTRL_STOP);
        smf_media_kfifo_deinit();
    }
    return true;
}