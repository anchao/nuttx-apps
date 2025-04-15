#include "smf_media_graph.api.h"
#include "smf_debug.h"
#include "kfifo.h"
#include "smf_media_audio_path_bt.h"
#include "smf_pool.h"

static smf_media_policy_t _smf_policy[SMF_POLICY_LIST];
static struct kfifo* _kfifo = 0;
static void* g_kfifo_buffer = 0;

static uint64_t string_hash(const char *str) {
    const uint64_t prime = 0x100000001B3ull;
    uint64_t hash = 0xCBF29CE484222325ull;
    while (*str) {
        hash ^= (uint8_t)*str++;
        hash *= prime;
    }
    return hash;
}

static void parse_arg(const char* arg, int* numbers, int count){
    char tmp[SMF_POLICY_DATA_LEN] = {0};
    memcpy(tmp, arg, SMF_POLICY_DATA_LEN);
    char *token = strtok(tmp, " ");
    int index = 0;
    while (token != NULL && index<count ) {
        numbers[index++] = atoi(token);
        token = strtok(NULL, " ");
    }
    for (int i = 0; i < index; i++) {
        dbgTestPDL(numbers[i]);
    }
}

static bool smf_media_kfifo_init(){
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
static void smf_media_kfifo_deinit(){
    if(g_kfifo_buffer)smf_find_pool_free("psramnc", g_kfifo_buffer);
    if(_kfifo)smf_find_pool_free("psramnc", _kfifo);
}
static bool smf_media_audio_output_spksink(bool is_monopoly){
    dbgTestPL();
    smf_audio_output_config_t spk_cfg;
    memset(&spk_cfg, 0, sizeof(smf_audio_output_config_t));
    spk_cfg.outputType = SMF_AUDIO_OUTPUT_SPK;
    spk_cfg.outputSpk.params.rate = 48000;
    spk_cfg.outputSpk.params.channel = 2;
    spk_cfg.outputSpk.params.bits = 16;
    spk_cfg.outputSpk.params.is_monopoly = is_monopoly;
    return smf_audio_player_load_output(&spk_cfg);
}
static bool smf_media_audio_output_i2ssink(bool is_monopoly){
    dbgTestPL();
    smf_audio_output_config_t i2s_cfg;
    memset(&i2s_cfg, 0, sizeof(smf_audio_output_config_t));
    i2s_cfg.outputType = SMF_AUDIO_OUTPUT_I2S;
    i2s_cfg.outputI2s.id = 0;
    i2s_cfg.outputI2s.master = true;
    i2s_cfg.outputI2s.params.rate = 48000;
    i2s_cfg.outputI2s.params.channel = 2;
    i2s_cfg.outputI2s.params.bits = 16;
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
static bool smf_media_audio_input_mic(int chamap, int rate, int ch, int bits){
    dbgTestPL();
    smf_audio_input_config_t mic_cfg;
    memset(&mic_cfg, 0, sizeof(smf_audio_input_config_t));
    mic_cfg.inputType = SMF_AUDIO_INPUT_MIC;
    mic_cfg.inputMic.params.rate = rate;
    mic_cfg.inputMic.params.channel = ch;
    mic_cfg.inputMic.params.bits = bits;
    mic_cfg.inputMic.mic_channel_map = chamap;
    return smf_audio_recorder_load_input(&mic_cfg);
}
static bool smf_media_audio_input_i2s(){
    dbgTestPL();
    // smf_audio_input_i2s_t   inputI2s;
    return true;
}
static bool smf_media_audio_input_tdm(){
    dbgTestPL();
    // smf_audio_input_tdm_t   inputTdm;
    return true;
}

bool smf_media_audio_output_config(){
    dbgTestPL();
    void* policy = smf_media_policy_list_get("SelPlay");
    if(!policy){
        dbgErrPXL("audio output SelPlay is NULL, use incodec");
        return smf_media_audio_output_spksink(false);
    }
    const char* cmd = ((smf_media_policy_t*)policy)->cmd;
    // dbgTestPSL(cmd);
    if( memcmp(cmd, "speaker", strlen(cmd)) == 0 ){
        return smf_media_audio_output_spksink(false);
    }else if( memcmp(cmd, "a2dp", strlen(cmd)) == 0 ){
        const smf_media_audio_bt_codec_cfg_t* info = smf_media_audio_bt_get_codec_info();
        if(info){
            if (info->type == SMF_MEDIA_AUDIO_BT_A2DP) {
                smf_media_audio_bt_ctrl_send(SMF_MEDIA_AUDIO_BT_CTRL_START);
                return true;//smf_media_audio_output_a2dpsink(info, false);
            }
            else {
                dbgErrPXL("unsport type=%d", info->type);
            }
        }else{
            return smf_media_audio_output_spksink(false);
        }
    }else if( memcmp(cmd, "i2s", strlen(cmd)) == 0 ){
        return smf_media_audio_output_i2ssink(false);

    }else{
        dbgErrPXL("audio output type unsupport");
    }
    return false;
}

bool smf_media_audio_output_remove(SMF_AUDIO_OUTPUT_TYPE type){
    dbgTestPL();
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
    smf_audio_player_file_t file;
    memset(&file ,0, sizeof(smf_audio_player_file_t));
    file.filename = (const char*)url;
    file.callback = player_func;
    file.volume = vol;
    dbgTestPDL(file.volume);
    file.priv = priv;
    return smf_audio_player_start("music", SMF_AUDIO_PLAYER_FILE, &file);
}

uint64_t smf_media_audio_player_buffer_start(int vol){
    dbgTestPL();
    smf_audio_player_stream_t params;
    memset(&params, 0, sizeof(smf_audio_player_stream_t));
    params.codec = "pcm";
    params.rate = 48000;
    params.channel = 2;
    params.bits = 16;
    params.volume = vol;
    return smf_audio_player_start("music", SMF_AUDIO_PLAYER_STREAM, (void*)&params);
}

uint64_t smf_media_audio_player_a2dp_start(const char* codec, int vol){
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
}
bool smf_media_audio_output_a2dpsink_start(){
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
bool smf_media_audio_input_config(){
    dbgTestPL();
    smf_media_policy_t* policy = (smf_media_policy_t*)smf_media_policy_list_get("SelCap");
    if(!policy){
        dbgErrPXL("audio input SelCap is NULL, use incodec");
        return smf_media_audio_input_mic(1<<8, 16000, 1, 16);
    }
    const char* cmd = policy->cmd;
    if( memcmp(cmd, "mic", strlen(cmd)) == 0 ){
        int numbers[4] = {0};
        int chamap=0,rate=0,ch=0,bits=0;
        parse_arg(policy->arg, &numbers, 4);
        chamap = numbers[0];
        rate = numbers[1];
        ch = numbers[2];
        bits = numbers[3]; 
        if(!chamap || !rate || !ch || !bits){
            dbgErrPXL("%d %d %d %d",chamap,rate,ch,bits);
            return false;
        }
        return smf_media_audio_input_mic(chamap, rate, ch, bits);
        
    }else if( memcmp(cmd, "i2s", strlen(cmd)) == 0 ){
        return smf_media_audio_input_i2s();
    }else if( memcmp(cmd, "tdm", strlen(cmd)) == 0 ){
        return smf_media_audio_input_tdm();
    }else{
        dbgErrPXL("audio input type unsupport");
    }
    return false;
}
bool smf_media_audio_input_remove(SMF_AUDIO_INPUT_TYPE type){
    dbgTestPL();
    smf_audio_input_config_t config;
    memset(&config, 0, sizeof(smf_audio_input_config_t));
    config.inputType = type;
    return smf_audio_recorder_unload_input(&config);
}

uint64_t smf_media_audio_recorder_url_start(char* url, const char* codec){
    dbgTestPL();
    returnIfErrC(0, !url || !codec);
    smf_audio_recorder_file_t params;
    memset(&params, 0, sizeof(smf_audio_recorder_file_t));
    params.params.rate = 16000;
    params.params.bits = 16;
    params.params.channel = 1;
    params.codec = codec;
    params.filename = (const char*)url;
    return smf_audio_recorder_start("record", SMF_AUDIO_RECORDER_FILE, &params);
}

uint64_t smf_media_audio_recorder_buffer_start(SmfAudioRecordCallback* record_func, const char* codec, void* priv){
    dbgTestPL();
    smf_audio_recorder_func_t params;
    memset(&params, 0, sizeof(smf_audio_recorder_func_t));
    params.params.rate = 16000;
    params.params.bits = 16;
    params.params.channel = 1;
    params.codec = codec;
    params.callback = record_func;
    params.fsize = 1024;//params.params.rate * params.params.channel * (params.params.bits>>3) *50 /1000;
    params.priv = priv;
    return smf_audio_recorder_start("record", SMF_AUDIO_RECORDER_FUNC, &params);
}

uint64_t smf_media_audio_btsco_start(uint8_t type, uint32_t vol){
    dbgTestPL();
    const char* format = 0;
    if(type==1)format="cvsd";
    else if(type==2)format="msbc";
    else return 0;
    if(!smf_media_audio_output_config())return 0;
    if(!smf_media_audio_input_config())return 0;

    uint64_t id = smf_audio_btsco_start(format);
    if(id){
        if(!smf_audio_btsco_set_down_volume(id, vol))return 0;
    }
    return id;
}
bool smf_media_audio_btsco_stop(uint64_t id){
    return smf_audio_btsco_stop(id);
}

//a2dp_source_send_info_t
uint32_t smf_media_kfifo_data_pull(void* buffer, uint32_t len){
    return kfifo_get(_kfifo, (uint8_t*)buffer, len);
}
uint32_t smf_media_kfifo_data_push(void* buffer, uint32_t len){
    return kfifo_put(_kfifo, (uint8_t*)buffer, len);
}
uint32_t smf_media_kfifo_data_peek(void* buffer, uint32_t len){
    return kfifo_peek_to_buf(_kfifo, (uint8_t*)buffer ,len);
}
uint32_t smf_media_kfifo_data_size(){
    return kfifo_len(_kfifo);
}
uint32_t smf_media_kfifo_data_free_size(){
    return kfifo_get_free_space(_kfifo);
}

void smf_media_policy_list_clean(){
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
