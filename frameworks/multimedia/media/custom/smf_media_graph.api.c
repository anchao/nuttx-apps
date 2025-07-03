#include "smf_media_graph.api.h"
#include "smf_debug.h"
#include "kfifo.h"
#include "smf_media_audio_path_bt.h"
#include "smf_pool.h"
#include "smf_media_arg_parse.h"

static smf_media_policy_t _smf_policy[SMF_POLICY_LIST];
static struct kfifo* _kfifo = 0;
static void* g_kfifo_buffer = 0;
static bool agsco = false;

static uint64_t string_hash(const char *str) {
    const uint64_t prime = 0x100000001B3ull;
    uint64_t hash = 0xCBF29CE484222325ull;
    while (*str) {
        hash ^= (uint8_t)*str++;
        hash *= prime;
    }
    return hash;
}

static bool smf_media_kfifo_init(void){
    _kfifo = smf_find_pool_alloc("psramnc", sizeof(struct kfifo));
    g_kfifo_buffer = smf_find_pool_alloc("psramnc", SMF_AUDIO_KFIFO_SIZE);
    if(!_kfifo || !g_kfifo_buffer){
        dbgErrPXL("kfifo malloc failed");
        return false;
    }
    memset(g_kfifo_buffer, 0, SMF_AUDIO_KFIFO_SIZE);
    kfifo_init(_kfifo, g_kfifo_buffer, SMF_AUDIO_KFIFO_SIZE);
    return true;
}
static void smf_media_kfifo_deinit(void){
    if(g_kfifo_buffer)smf_find_pool_free("psramnc", g_kfifo_buffer);
    if(_kfifo)smf_find_pool_free("psramnc", _kfifo);
}
static bool smf_media_audio_output_spksink(bool is_monopoly, int rate, int ch, int bits){
    dbgTestPXL("%d %d %d %d", is_monopoly, rate, ch, bits);
    smf_audio_output_config_t spk_cfg;
    memset(&spk_cfg, 0, sizeof(smf_audio_output_config_t));
    spk_cfg.outputType = SMF_AUDIO_OUTPUT_SPK;
    spk_cfg.outputSpk.params.rate = rate;
    spk_cfg.outputSpk.params.channel = ch;
    spk_cfg.outputSpk.params.bits = bits;
    spk_cfg.outputSpk.params.is_monopoly = is_monopoly;
    return smf_audio_player_load_output(&spk_cfg);
}
static bool smf_media_audio_output_i2ssink(bool is_monopoly,int devid, bool master, int rate, int ch, int bits){
    dbgTestPXL("%d %d %d %d %d %d", is_monopoly, devid, master, rate, ch, bits);
    smf_audio_output_config_t i2s_cfg;
    memset(&i2s_cfg, 0, sizeof(smf_audio_output_config_t));
    i2s_cfg.outputType = SMF_AUDIO_OUTPUT_I2S;
    i2s_cfg.outputI2s.id = devid;
    i2s_cfg.outputI2s.master = master;
    i2s_cfg.outputI2s.params.rate = rate;
    i2s_cfg.outputI2s.params.channel = ch;
    i2s_cfg.outputI2s.params.bits = bits;
    i2s_cfg.outputI2s.params.is_monopoly = is_monopoly;
    return smf_audio_player_load_output(&i2s_cfg);
}
static bool smf_media_audio_output_a2dpsink(const void* info, bool is_monopoly){
    if(!smf_media_kfifo_init())return false;
    const smf_media_audio_bt_codec_cfg_t* cfg = (smf_media_audio_bt_codec_cfg_t*)info;
    smf_audio_output_config_t a2dp_cfg;
    memset(&a2dp_cfg, 0, sizeof(smf_audio_output_config_t));
    a2dp_cfg.outputType = SMF_AUDIO_OUTPUT_A2DP;
    a2dp_cfg.outputA2dp.params.rate = cfg->codec_param.a2dp.sample_rate;

    if(cfg->codec_param.a2dp.codec_type == 0){//sbc
        a2dp_cfg.outputA2dp.codec = "sbc";
        a2dp_cfg.outputA2dp.enc.sbc.allocMethod = 1; //SMF_SBC_ALLOC_METHOD_SNR
        a2dp_cfg.outputA2dp.enc.sbc.bitPool = cfg->codec_param.a2dp.diff_param.sbc.s16BitPool;
        a2dp_cfg.outputA2dp.enc.sbc.channelMode =  cfg->codec_param.a2dp.diff_param.sbc.s16ChannelMode;
        a2dp_cfg.outputA2dp.enc.sbc.numBlocks =  cfg->codec_param.a2dp.diff_param.sbc.s16NumOfBlocks;
        a2dp_cfg.outputA2dp.enc.sbc.numSubBands =  cfg->codec_param.a2dp.diff_param.sbc.s16NumOfSubBands;

    }else if(cfg->codec_param.a2dp.codec_type == 2){//aac
        switch (cfg->codec_param.a2dp.diff_param.aac.u16ObjectType)
        {
        case 0x80://A2DP_AAC_OBJECT_TYPE_MPEG2_LC:
            a2dp_cfg.outputA2dp.enc.aac.aot = 129;
            break;
        case 0x40://A2DP_AAC_OBJECT_TYPE_MPEG4_LC:
            a2dp_cfg.outputA2dp.enc.aac.aot = 2;
            break;
        case 0x20://A2DP_AAC_OBJECT_TYPE_MPEG4_LTP:
            /* code */
        case 0x10://A2DP_AAC_OBJECT_TYPE_MPEG4_SCALABLE:
            /* code */
        default:
            dbgWarnPXL("aac ObjectType Not Supported %d, default 2(MPEG4_LC)", cfg->codec_param.a2dp.diff_param.aac.u16ObjectType);
            a2dp_cfg.outputA2dp.enc.aac.aot = 2;
            break;
        }
        a2dp_cfg.outputA2dp.codec = "aac";
        a2dp_cfg.outputA2dp.enc.aac.bitrate = cfg->codec_param.a2dp.bit_rate;
        a2dp_cfg.outputA2dp.enc.aac.layer = 0;
        a2dp_cfg.outputA2dp.enc.aac.package = 6;
        a2dp_cfg.outputA2dp.enc.aac.vbr = cfg->codec_param.a2dp.diff_param.aac.u16VariableBitRate;
    }
    a2dp_cfg.outputA2dp.params.bits = 16;
    a2dp_cfg.outputA2dp.params.channel = 2;
    a2dp_cfg.outputA2dp.params.is_monopoly = is_monopoly;
    a2dp_cfg.outputA2dp.kfifo = (uint32_t)_kfifo;
    return smf_audio_player_load_output(&a2dp_cfg);
}
static bool smf_media_audio_input_mic(int chamap, int rate, int ch, int bits, uint32_t dll_addr){
    dbgTestPL();
    smf_audio_input_config_t mic_cfg;
    memset(&mic_cfg, 0, sizeof(smf_audio_input_config_t));

    if(dll_addr){
        dbgTestPXL("dll addr 0x%x", dll_addr);
        smf_media_dl_config_t dl_config;
        memset(&dl_config, 0, sizeof(smf_media_dl_config_t));
        dl_config.text_pool = SMF_MEDIA_DLLPOOL_TCM;
        dl_config.data_pool = SMF_MEDIA_DLLPOOL_TCM;
        dl_config.dll_type[0] = SMF_MEDIA_DLL_ADDR;
        dl_config.dll_name[0] = "pil_algo_normal_demo";
        dl_config.dll_parameter[0].dll_addr = dll_addr;
        smf_media_dl_config(&dl_config);
        mic_cfg.inputExtra.algo_enable = 1;
        mic_cfg.inputExtra.dyn_path = "pil_algo_normal_demo";
    }
    mic_cfg.inputType = SMF_AUDIO_INPUT_MIC;
    mic_cfg.inputMic.params.rate = rate;
    mic_cfg.inputMic.params.channel = ch;
    mic_cfg.inputMic.params.bits = bits;
    mic_cfg.inputMic.mic_channel_map = chamap;
    return smf_audio_recorder_load_input(&mic_cfg);
}
static bool smf_media_audio_input_i2s(void){
    dbgTestPL();
    // smf_audio_input_i2s_t   inputI2s;
    return true;
}
static bool smf_media_audio_input_tdm(void){
    dbgTestPL();
    // smf_audio_input_tdm_t   inputTdm;
    return true;
}

bool smf_media_audio_output_config(void){
    dbgTestPL();
    smf_media_policy_t* policy = (smf_media_policy_t*)smf_media_policy_list_get("SelPlay");
    if(!policy){
        dbgErrPXL("audio output SelPlay is NULL");
        return false;//smf_media_audio_output_spksink(false, 48000, 2, 16);
    }
    const char* cmd = policy->cmd;
    if( memcmp(cmd, "speaker", strlen(cmd)) == 0 ){
        int rate=0,ch=0,bits=0;
        smf_media_kv_pair_t pairs[3];
        int count = smf_media_parse_string(policy->arg, pairs, 3, " ");
        if(count != 3){
            dbgErrPXL("smf_media_parse_string error");
            return false;
        }
        rate = atoi(smf_media_get_value(pairs, count, "rate"));
        ch = atoi(smf_media_get_value(pairs, count, "ch"));
        bits = atoi(smf_media_get_value(pairs, count, "bits"));
        dbgTestPXL("%d %d %d",rate,ch,bits);
        if(!rate || !ch || !bits){
            dbgErrPXL("%d %d %d",rate,ch,bits);
            return false;
        }
        return smf_media_audio_output_spksink(false, rate, ch, bits);

    }else if( memcmp(cmd, "a2dp", strlen(cmd)) == 0 ){
        const smf_media_audio_bt_codec_cfg_t* info = smf_media_audio_bt_get_codec_info();
        if(info){
            if (info->type == SMF_MEDIA_AUDIO_BT_A2DP) {
                smf_media_audio_bt_ctrl_send(SMF_MEDIA_AUDIO_BT_CTRL_START);
                return true;
            }
            else {
                dbgErrPXL("unsport type=%d", info->type);
            }
        }else{
            return smf_media_audio_output_spksink(false, 48000, 2, 16);
        }
    }else if( (memcmp(cmd, "i2s", strlen(cmd)) == 0) ){
        int rate=0,ch=0,bits=0,master = 0;
        smf_media_kv_pair_t pairs[4];
        int count = smf_media_parse_string(policy->arg, pairs, 4, " ");
        if(count != 4){
            dbgErrPXL("smf_media_parse_string error");
            return false;
        }
        rate = atoi(smf_media_get_value(pairs, count, "rate"));
        ch = atoi(smf_media_get_value(pairs, count, "ch"));
        bits = atoi(smf_media_get_value(pairs, count, "bits"));
        master = atoi(smf_media_get_value(pairs, count, "master"));
        dbgTestPXL("%d %d %d %d",master,rate,ch,bits);
        if(!rate || !ch || !bits){
            dbgErrPXL("%d %d %d",rate,ch,bits);
            return false;
        }
        int id = 0; //i2s0
        if( memcmp(cmd, "i2s1", strlen(cmd)) == 0 ){
            id = 1;
        }
        return smf_media_audio_output_i2ssink(false, id, (bool)master, rate, ch, bits);
    }else{
        dbgErrPXL("audio output type unsupport");
    }
    return false;
}

bool smf_media_audio_output_remove(void){
    dbgTestPL();
    SMF_AUDIO_OUTPUT_TYPE type = SMF_AUDIO_OUTPUT_NULL;
    void* policy = smf_media_policy_list_get("SelPlay");
    if(!policy){
        dbgErrPXL("audio output SelPlay is NULL");
        return true;
    }
    const char* cmd = ((smf_media_policy_t*)policy)->cmd;
    if( memcmp(cmd, "speaker", strlen(cmd)) == 0 ){
        type = SMF_AUDIO_OUTPUT_SPK;
    }else if( memcmp(cmd, "a2dp", strlen(cmd)) == 0 ){
        type = SMF_AUDIO_OUTPUT_A2DP;
    }else if( memcmp(cmd, "i2s", 3) == 0 ){
        type = SMF_AUDIO_OUTPUT_I2S;
    }else{
        dbgErrPXL("audio output type unsupport");
    }
    dbgTestPDL(type);
    smf_audio_output_config_t config;
    memset(&config, 0, sizeof(smf_audio_output_config_t));
    config.outputType = type;
    if(type == SMF_AUDIO_OUTPUT_A2DP){
        const void* info = smf_media_audio_bt_get_codec_info();
        if(info)smf_media_audio_bt_ctrl_send(SMF_MEDIA_AUDIO_BT_CTRL_STOP);
        smf_media_kfifo_deinit();
    }
    return smf_audio_player_unload_output(&config);
}

uint64_t smf_media_audio_player_url_start(SmfAudioPlayerCallback* player_func, char* url, void* priv, int vol){
    dbgTestPL();
    returnIfErrC(0, !url);

    bool ret = smf_media_audio_output_config();
    if(!ret){
        dbgErrPXL("audio player set output failed");
        return 0;
    }

    smf_audio_player_file_t file;
    memset(&file ,0, sizeof(smf_audio_player_file_t));
    file.filename = (const char*)url;
    file.callback = player_func;
    file.volume = vol;
    dbgTestPDL(file.volume);
    file.priv = priv;

#if 0
    extern uint32_t __audio_file_start[];
    extern uint32_t __audio_file_end[];
    int buff_size = (int32_t)__audio_file_end - (int32_t)__audio_file_start;
    dbgTestPXL("%p~%p,%d", __audio_file_start, __audio_file_end, buff_size);
    char buff_url[50];
    sprintf(buff_url, "buff://%p_%d.mp3", __audio_file_start, buff_size);
    file.filename = (const char*)buff_url;
#endif

    return smf_audio_player_start("music", SMF_AUDIO_PLAYER_FILE, &file);
}

uint64_t smf_media_audio_player_buffer_start(int vol, char* opt){
    dbgTestPL();
    bool ret = smf_media_audio_output_config();
    if(!ret){
        dbgErrPXL("audio player set output failed");
        return 0;
    }

    if(strlen(opt) == 0){
        dbgTestPXL("opt is null, use default pcm 48000 2 16");
        opt = "format=pcm,rate=48000,ch=2,bits=16";
    }
    dbgTestPSL(opt);
    const char* codec = 0;
    int rate=0,ch=0,bits=0;
    smf_media_kv_pair_t pairs[4];
    int count = smf_media_parse_string(opt, pairs, 6, ",");
    if(count < 4){
        dbgErrPXL("smf_media_parse_string error");
        return 0;
    }
    rate = atoi(smf_media_get_value(pairs, count, "rate"));
    ch = atoi(smf_media_get_value(pairs, count, "ch"));
    bits = atoi(smf_media_get_value(pairs, count, "bits"));
    codec = smf_media_get_value(pairs, count, "format");

    dbgTestPXL("%d %d %d %s",rate,ch,bits,codec);
    if(!rate || !ch || !bits){
        dbgErrPXL("%d %d %d %d",rate,ch,bits);
        return 0;
    }

    smf_audio_player_stream_t params;
    memset(&params, 0, sizeof(smf_audio_player_stream_t));
    params.codec = codec;
    params.rate = rate;
    params.channel = ch;
    params.bits = bits;
    params.volume = vol;
    return smf_audio_player_start("stream", SMF_AUDIO_PLAYER_STREAM, (void*)&params);
}

uint64_t smf_media_audio_player_a2dp_start(const char* codec, int vol){
    dbgTestPL();
    bool ret = smf_media_audio_output_config();
    if(!ret){
        dbgErrPXL("audio player set output failed");
        return 0;
    }

    if(!smf_media_kfifo_init())return 0;
    smf_audio_player_a2dpsink_t params;
    memset(&params, 0, sizeof(smf_audio_player_a2dpsink_t));
    params.codec = codec;
    params.kfifo = (uint32_t)_kfifo;
    params.volume = vol;
    return smf_audio_player_start("a2dp", SMF_AUDIO_PLAYER_A2DP, (void*)&params);
}

void smf_media_audio_player_stop(uint64_t id){
    dbgTestPL();
    const void* info = smf_media_audio_bt_get_codec_info();
    if(info){
        smf_media_audio_bt_ctrl_send(SMF_MEDIA_AUDIO_BT_CTRL_STOP);
        smf_media_kfifo_deinit();
    }
    if(id)smf_audio_player_stop(id);
    smf_media_audio_output_remove();
}
bool smf_media_audio_output_a2dpsink_start(void){
    const smf_media_audio_bt_codec_cfg_t* info = smf_media_audio_bt_get_codec_info();
    if(info){
        return smf_media_audio_output_a2dpsink(info, false);
    }
    return false;
}

void smf_media_audio_player_a2dp_stop(uint64_t id){
    dbgTestPL();
    smf_media_kfifo_deinit();
    if(id)smf_media_audio_player_stop(id);
}
bool smf_media_audio_input_config(uint32_t dll_addr){
    dbgTestPL();
    smf_media_policy_t* policy = (smf_media_policy_t*)smf_media_policy_list_get("SelCap");
    if(!policy){
        dbgErrPXL("audio input SelCap is NULL");
        return false;
    }
    const char* cmd = policy->cmd;
    if( memcmp(cmd, "mic", strlen(cmd)) == 0 ){
        uint32_t chamap=0,rate=0,ch=0,bits=0;
        smf_media_kv_pair_t pairs[4];
        int count = smf_media_parse_string(policy->arg, pairs, 4, " ");
        if(count != 4){
            dbgErrPXL("smf_media_parse_string error");
            return false;
        }
        rate = atoi(smf_media_get_value(pairs, count, "rate"));
        ch = atoi(smf_media_get_value(pairs, count, "ch"));
        bits = atoi(smf_media_get_value(pairs, count, "bits"));
        chamap = atoi(smf_media_get_value(pairs, count, "chmap"));
        dbgTestPXL("%d %d %d %d",chamap,rate,ch,bits);
        if(!rate || !ch || !bits){
            dbgErrPXL("%d %d %d %d",chamap,rate,ch,bits);
            return false;
        }
        return smf_media_audio_input_mic(chamap, rate, ch, bits, dll_addr);
        
    }else if( memcmp(cmd, "i2s0", strlen(cmd)) == 0 ){
        return smf_media_audio_input_i2s();

    }else if( memcmp(cmd, "i2s1", strlen(cmd)) == 0 ){
        return smf_media_audio_input_i2s();

    }else if( memcmp(cmd, "tdm", strlen(cmd)) == 0 ){
        return smf_media_audio_input_tdm();
    }else{
        dbgErrPXL("audio input type unsupport");
    }
    return false;
}
bool smf_media_audio_input_remove(void){
    dbgTestPL();
    SMF_AUDIO_INPUT_TYPE type = SMF_AUDIO_INPUT;
    smf_media_policy_t* policy = (smf_media_policy_t*)smf_media_policy_list_get("SelCap");
    if(!policy){
        dbgErrPXL("audio input SelCap is NULL");
        return true;
    }
    const char* cmd = policy->cmd;
    if( memcmp(cmd, "mic", strlen(cmd)) == 0 ){
        type = SMF_AUDIO_INPUT_MIC;
    }else if( memcmp(cmd, "i2s", 3) == 0 ){
        type = SMF_AUDIO_INPUT_I2S;
    }else if( memcmp(cmd, "tdm", strlen(cmd)) == 0 ){
        type = SMF_AUDIO_INPUT_TDM;
    }else{
        dbgErrPXL("audio input type unsupport");
    }

    smf_audio_input_config_t config;
    memset(&config, 0, sizeof(smf_audio_input_config_t));
    config.inputType = type;
    return smf_audio_recorder_unload_input(&config);
}
//opt "format=pcm:rate=16000:ch=1:bits=16:addr=0x30E00000"
//opt "format=opus:rate=48000:ch=2:bits=16:br=128000"
//opt "format=silk:rate=16000:ch=1:bits=16"
//opt "format=mp3:rate=16000:ch=1:bits=16:br=80000"
uint64_t smf_media_audio_recorder_url_start(char* url, char* opt){
    dbgTestPL();
    returnIfErrC(0, !url);
    smf_audio_recorder_file_t params;
    memset(&params, 0, sizeof(smf_audio_recorder_file_t));
    if(strlen(opt) == 0){
        dbgTestPXL("opt is null, use default pcm");
        opt = "format=pcm:rate=16000:ch=1:bits=16";
    }
    dbgTestPSL(opt);
    const char* codec = 0;
    int rate=0,ch=0,bits=0;
    smf_media_kv_pair_t pairs[6];
    int count = smf_media_parse_string(opt, pairs, 6, ":");
    if(count < 4){
        dbgErrPXL("smf_media_parse_string error");
        return 0;
    }
    rate = atoi(smf_media_get_value(pairs, count, "rate"));
    ch = atoi(smf_media_get_value(pairs, count, "ch"));
    bits = atoi(smf_media_get_value(pairs, count, "bits"));
    codec = smf_media_get_value(pairs, count, "format");

    dbgTestPXL("%d %d %d %s",rate,ch,bits,codec);
    if(!rate || !ch || !bits){
        dbgErrPXL("%d %d %d %d",rate,ch,bits);
        return 0;
    }
    char* value = smf_media_get_value(pairs, count, "addr");
    uint32_t dll_addr = strtol(value, NULL, 16);
    bool ret = smf_media_audio_input_config(dll_addr);
    if(!ret){
        dbgErrPXL("audio recorder set input failed\n");
        return 0;
    }
    if( memcmp(codec, "pcm", strlen(codec)) == 0 ){
        codec = "pcm";
    }else if( memcmp(codec, "opus", strlen(codec)) == 0 ){
        codec = "opus";
        uint32_t bitrate = atoi(smf_media_get_value(pairs, count, "br"));
        dbgTestPDL(bitrate);
        params.enc.opus.bitrate = bitrate?bitrate:128000;
        params.enc.opus.frameDMs = 200;
        params.enc.opus.have_head = 1;
    }else if( memcmp(codec, "silk", strlen(codec)) == 0 ){
        codec = "silk";
        params.enc.silk.rev = 0;
    }else if( memcmp(codec, "aac", strlen(codec)) == 0 ){
        codec = "aac";
        params.enc.aac.aot = 0;
        params.enc.aac.bitrate = 0;
        params.enc.aac.layer = 0;
        params.enc.aac.package = 0;
        params.enc.aac.vbr = 0;
    }else if( memcmp(codec, "mp3", strlen(codec)) == 0 ){
        codec = "mp3";
        params.enc.silk.rev = 0;
    }else if( memcmp(codec, "sbc", strlen(codec)) == 0 ){
        codec = "sbc";
        params.enc.sbc.allocMethod = 0;
        params.enc.sbc.bitPool = 0;
        params.enc.sbc.channelMode = 0;
        params.enc.sbc.numBlocks = 0;
        params.enc.sbc.numSubBands = 0;
    }else{
        dbgErrPXL("audio recorder type unsupport %s", codec);
        return 0;
    }

    params.params.rate = rate;
    params.params.bits = bits;
    params.params.channel = ch;
    params.codec = codec;
    params.filename = (const char*)url;
    return smf_audio_recorder_start("record", SMF_AUDIO_RECORDER_FILE, &params);
}

uint64_t smf_media_audio_recorder_buffer_start(SmfAudioRecordCallback* record_func, void* priv, char* opt){
    dbgTestPL();
    returnIfErrC(0, !priv);
    smf_audio_recorder_func_t params;
    memset(&params, 0, sizeof(smf_audio_recorder_func_t));

    if(strlen(opt) == 0){
        dbgTestPXL("opt is null, use default pcm");
        opt = "format=pcm:rate=16000:ch=1:bits=16";
    }
    dbgTestPSL(opt);
    const char* codec = 0;
    int rate=0,ch=0,bits=0;
    smf_media_kv_pair_t pairs[6];
    int count = smf_media_parse_string(opt, pairs, 6, ":");
    if(count < 4){
        dbgErrPXL("smf_media_parse_string error");
        return 0;
    }
    rate = atoi(smf_media_get_value(pairs, count, "rate"));
    ch = atoi(smf_media_get_value(pairs, count, "ch"));
    bits = atoi(smf_media_get_value(pairs, count, "bits"));
    codec = smf_media_get_value(pairs, count, "format");

    dbgTestPXL("%d %d %d %s",rate,ch,bits,codec);
    if(!rate || !ch || !bits){
        dbgErrPXL("%d %d %d %d",rate,ch,bits);
        return 0;
    }
    char* value = smf_media_get_value(pairs, count, "addr");
    uint32_t dll_addr = strtol(value, NULL, 16);
    bool ret = smf_media_audio_input_config(dll_addr);
    if(!ret){
        dbgErrPXL("audio recorder set input failed\n");
        return 0;
    }
    if( memcmp(codec, "pcm", strlen(codec)) == 0 ){
        codec = "pcm";
    }else if( memcmp(codec, "opus", strlen(codec)) == 0 ){
        codec = "opus";
        uint32_t bitrate = atoi(smf_media_get_value(pairs, count, "br"));
        dbgTestPDL(bitrate);
        params.enc.opus.bitrate = bitrate?bitrate:128000;
        params.enc.opus.frameDMs = 200;
        params.enc.opus.have_head = 1;
    }else if( memcmp(codec, "silk", strlen(codec)) == 0 ){
        codec = "silk";
        params.enc.silk.rev = 0;
    }else if( memcmp(codec, "aac", strlen(codec)) == 0 ){
        codec = "aac";
        params.enc.aac.aot = 0;
        params.enc.aac.bitrate = 0;
        params.enc.aac.layer = 0;
        params.enc.aac.package = 0;
        params.enc.aac.vbr = 0;
    }else if( memcmp(codec, "mp3", strlen(codec)) == 0 ){
        codec = "mp3";
        params.enc.silk.rev = 0;
    }else if( memcmp(codec, "sbc", strlen(codec)) == 0 ){
        codec = "sbc";
        params.enc.sbc.allocMethod = 0;
        params.enc.sbc.bitPool = 0;
        params.enc.sbc.channelMode = 0;
        params.enc.sbc.numBlocks = 0;
        params.enc.sbc.numSubBands = 0;
    }else{
        dbgErrPXL("audio recorder type unsupport %s", codec);
        return 0;
    }

    params.params.rate = rate;
    params.params.bits = bits;
    params.params.channel = ch;
    params.codec = codec;
    params.callback = record_func;
    // params.fsize = 1024;//params.params.rate * params.params.channel * (params.params.bits>>3) *50 /1000;
    params.priv = priv;
    return smf_audio_recorder_start("record", SMF_AUDIO_RECORDER_FUNC, &params);
}

void smf_media_audio_recorder_stop(uint64_t id){
    if(id)smf_audio_recorder_stop(id);
    smf_media_audio_input_remove();
}
uint64_t smf_media_audio_btsco_start(uint8_t type, uint32_t vol){
    dbgTestPL();
    const char* format = 0;
    if(type==1)format="cvsd";
    else if(type==2)format="msbc";
    else return 0;
    // if(!smf_media_audio_output_config())return 0;
    smf_media_audio_output_spksink(false, 48000, 2, 16);
    if(!smf_media_audio_input_config(0))return 0;
    uint64_t id = smf_audio_btsco_start(format);
    if(id){
        if(!smf_audio_btsco_set_down_volume(id, vol))return 0;
    }
    return id;
}
bool smf_media_audio_btsco_stop(uint64_t id){
    dbgTestPL();
    if(agsco){
        return smf_media_audio_agsco_stop();
    }else{
        if(id)smf_audio_btsco_stop(id);
        smf_audio_output_config_t config;
        memset(&config, 0, sizeof(smf_audio_output_config_t));
        config.outputType = SMF_AUDIO_OUTPUT_SPK;
        smf_audio_player_unload_output(&config);
        // smf_media_audio_output_remove();
        smf_media_audio_input_remove();
    }
    return true;
}

uint64_t smf_media_audio_agsco_start(uint8_t type, uint32_t vol){
    dbgTestPL();
    const char* format = 0;
    if(type==1)format="cvsd";
    else if(type==2)format="msbc";
    else return 0;

    smf_media_add_dn_btpcm_sink(format);
    smf_media_add_up_btpcm_source(format);
    agsco = true;
    return true;
}
bool smf_media_audio_agsco_stop(void){
    dbgTestPL();
    smf_media_remove_dn_btpcm_sink();
    smf_media_remove_up_btpcm_source();
    agsco = false;
    return true;
}

bool smf_media_audio_default_config(void){
    dbgTestPL();
    smf_media_audio_input_config(0);
    smf_media_audio_output_config();
    return true;
}
bool smf_media_audio_default_remove(void){
    dbgTestPL();
    smf_media_audio_input_remove();
    smf_media_audio_output_remove();
    return true;
}

//video
uint64_t smf_media_video_player_url_start(char* url, char* opt){
    dbgTestPL();
    smf_media_kv_pair_t pairs[1];
    int count = smf_media_parse_string(opt, pairs, 1, " ");
    if(count != 1){
        dbgErrPXL("smf_media_parse_string error");
        return 0;
    }
    int type = atoi(smf_media_get_value(pairs, count, "type"));
    if(type == 0){
        dbgErrPXL("type is 0");
        return 0;
    }
    dbgTestPXL("url %s type %d", url, type);
    smf_video_player_file_param_t param;
    memset(&param, 0, sizeof(smf_video_player_file_param_t));
    param.filename = (const char*)url;
    param.type = (SMF_VIDEO_PLAYER_TYPE)type;
    param.startpos = 0;

    smf_audio_output_config_t out_cfg;
    memset(&out_cfg, 0, sizeof(smf_audio_output_config_t));
    out_cfg.outputType = SMF_AUDIO_OUTPUT_SPK;
    out_cfg.outputSpk.params.rate = 48000;
    out_cfg.outputSpk.params.channel = 2;
    out_cfg.outputSpk.params.bits = 16;
    out_cfg.outputSpk.params.is_monopoly = true;

    param.acfg = &out_cfg;

    return smf_video_player_file_start(&param);
}
uint64_t smf_media_video_player_buffer_start(void){
    dbgTestPL();
    return 0;
}
void smf_media_video_player_stop(uint64_t id){
    dbgTestPL();
    if(id)smf_video_player_file_stop(id);
}

//a2dp_source_send_info_t
uint32_t smf_media_kfifo_data_pull(void* buffer, uint32_t len){
    return kfifo_get(_kfifo, (uint8_t*)buffer, len);
}
uint32_t smf_media_kfifo_data_push(void* buffer, uint32_t len){
    uint32_t free_size = kfifo_get_free_space(_kfifo);
    int ret = 0;
    if (len <= free_size) {
        ret = kfifo_put(_kfifo, (uint8_t*)buffer, len);
    }
    else {
        dbgTestPXL("kfifo push failed: %u<%u", free_size, len);
    }
    return ret;
}
uint32_t smf_media_kfifo_data_peek(void* buffer, uint32_t len){
    return kfifo_peek_to_buf(_kfifo, (uint8_t*)buffer ,len);
}
uint32_t smf_media_kfifo_data_size(void){
    return kfifo_len(_kfifo);
}
uint32_t smf_media_kfifo_data_free_size(void){
    return kfifo_get_free_space(_kfifo);
}

void smf_media_policy_list_clean(void){
    dbgTestPL();
    memset(&_smf_policy[0], 0, sizeof(smf_media_policy_t)*SMF_POLICY_LIST);
}
bool smf_media_policy_list_join(const char* target, const char* cmd, const char* arg ){
    if(!target || !cmd)return false;
    if( strlen(target)>(SMF_POLICY_DATA_LEN-1) ){
        dbgErrPXL("target len %d", strlen(target));
        return false;
    }
    if( strlen(cmd)>(SMF_POLICY_DATA_LEN-1) ){
        dbgErrPXL("target len %d", strlen(cmd));
        return false;
    }
    if(arg && strlen(arg)>(SMF_POLICY_DATA_LEN-1) ){
        dbgErrPXL("target len %d", strlen(arg));
        return false;
    }
    smf_media_policy_t* policy = (smf_media_policy_t*)smf_media_policy_list_get(target);
    if(policy){
        if(cmd){
            memset(policy->cmd, 0, SMF_POLICY_DATA_LEN);
            memcpy(policy->cmd, cmd, strlen(cmd));
        }
        if(arg){
            memset(policy->arg, 0, SMF_POLICY_DATA_LEN);
            memcpy(policy->arg, arg, strlen(arg));
        }
        dbgTestPXL("change target %s cmd %s arg %s", policy->target, policy->cmd ,policy->arg);
        return true;
    }else{
        for(int i = 0; i<SMF_POLICY_LIST; i++){
            if(_smf_policy[i].hash == 0){
                if(target)_smf_policy[i].hash = string_hash(target);
                if(target)memcpy(_smf_policy[i].target, target, strlen(target));
                if(cmd)memcpy(_smf_policy[i].cmd, cmd, strlen(cmd));
                if(arg)memcpy(_smf_policy[i].arg, arg, strlen(arg));
                dbgTestPXL("join target %s cmd %s arg %s", _smf_policy[i].target, _smf_policy[i].cmd ,_smf_policy[i].arg);
                return true;
            }
        }
    }
    return false;
}
void* smf_media_policy_list_get(const char* target){
    if(!target)return NULL;
    uint64_t thash = string_hash(target);
    for(int i = 0; i<SMF_POLICY_LIST; i++){
        if(_smf_policy[i].hash == thash){
            dbgTestPXL("target %s cmd %s arg %s", _smf_policy[i].target, _smf_policy[i].cmd ,_smf_policy[i].arg);
            return &_smf_policy[i];
        }
    }
    dbgErrPXL("policy list no find %s", target);
    return NULL;
}
