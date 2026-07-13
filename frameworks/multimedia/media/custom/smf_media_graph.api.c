#include "smf_media_graph.api.h"
#include "smf_debug_ext.h"
#include "kfifo.h"
#include "smf_media_audio_path_bt.h"
#include "smf_pool.h"
#include "smf_media_arg_parse.h"
#include "smf_stream_api.h"
#include "smf_stream_api_default.h"
#include "pthread.h"

static smf_media_policy_t _smf_policy[SMF_POLICY_LIST];

static uint64_t string_hash(const char *str) {
    const uint64_t prime = 0x100000001B3ull;
    uint64_t hash = 0xCBF29CE484222325ull;
    while (*str) {
        hash ^= (uint8_t)*str++;
        hash *= prime;
    }
    return hash;
}

static int smf_media_output_switch_thread_create(void){
    pthread_t pid;
    pthread_attr_t pattr;
    struct sched_param sparam;
    pthread_attr_init(&pattr);
    pthread_attr_setstacksize(&pattr, 4096);
    sparam.sched_priority = sched_get_priority_max(SCHED_FIFO) - 9;
    pthread_attr_setschedparam(&pattr, &sparam);

    int ret = pthread_create(&pid, &pattr, smf_media_audio_output_switch, 0);
    if (ret){
        dbgErrPXL("create smf media thread error %d", ret);
        return -1;
    }
    pthread_setname_np(pid, "output_switch");
    pthread_attr_destroy(&pattr);
    pthread_detach(pid);
    dbgNxPXL("output_switch create");
    return 0;
}

void smf_media_policy_list_clean(void){
    dbgNxPL();
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
        if( (memcmp(target, "SelPlay", strlen("SelPlay")) == 0)  && (memcmp(cmd, "a2dp", strlen("a2dp")) == 0) ){
            const char* defplay = "DefPlay";
            smf_media_policy_t* def_policy = (smf_media_policy_t*)smf_media_policy_list_get(defplay);
            if(def_policy){
                if(strlen(policy->cmd)){
                    memset(def_policy->cmd, 0, SMF_POLICY_DATA_LEN);
                    memcpy(def_policy->cmd, policy->cmd, strlen(policy->cmd));
                }
                if(strlen(policy->arg)){
                    memset(def_policy->arg, 0, SMF_POLICY_DATA_LEN);
                    memcpy(def_policy->arg, policy->arg, strlen(policy->arg));
                }
                dbgNxPXL("change target %s cmd %s arg %s", def_policy->target, def_policy->cmd ,def_policy->arg);

            }else{
                for(int i = 0; i<SMF_POLICY_LIST; i++){
                    if(_smf_policy[i].hash == 0){
                        _smf_policy[i].hash = string_hash(defplay);
                        memcpy(_smf_policy[i].target, defplay, strlen(defplay));
                        memcpy(_smf_policy[i].cmd, policy->cmd, strlen(policy->cmd));
                        memcpy(_smf_policy[i].arg, policy->arg, strlen(policy->arg));
                        dbgNxPXL("join target %s cmd %s arg %s", _smf_policy[i].target, _smf_policy[i].cmd ,_smf_policy[i].arg);
                        break;
                    }
                }
            }
        }
             
        if(cmd){
            memset(policy->cmd, 0, SMF_POLICY_DATA_LEN);
            memcpy(policy->cmd, cmd, strlen(cmd));
        }
        if(arg){
            memset(policy->arg, 0, SMF_POLICY_DATA_LEN);
            memcpy(policy->arg, arg, strlen(arg));
        }
        dbgNxPXL("change target %s cmd %s arg %s", policy->target, policy->cmd ,policy->arg);
        if( (memcmp(target, "SelPlay", strlen("SelPlay")) == 0) ){
            smf_media_output_switch_thread_create();
        }
        return true;
    }else{
        for(int i = 0; i<SMF_POLICY_LIST; i++){
            if(_smf_policy[i].hash == 0){
                if(target)_smf_policy[i].hash = string_hash(target);
                if(target)memcpy(_smf_policy[i].target, target, strlen(target));
                if(cmd)memcpy(_smf_policy[i].cmd, cmd, strlen(cmd));
                if(arg)memcpy(_smf_policy[i].arg, arg, strlen(arg));
                dbgNxPXL("join target %s cmd %s arg %s", _smf_policy[i].target, _smf_policy[i].cmd ,_smf_policy[i].arg);
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
            dbgNxPXL("target %s cmd %s arg %s", _smf_policy[i].target, _smf_policy[i].cmd ,_smf_policy[i].arg);
            return &_smf_policy[i];
        }
    }
    // dbgWarnPXL("policy list no find %s", target);
    return NULL;
}

