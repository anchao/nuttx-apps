#include "smf_media_graph.api.h"
#include "smf_debug_ext.h"
#include "smf_media_audio_path_bt.h"
#include "smf_stream_api.h"
#include "smf_stream_api_default.h"
#include "media_server.h"

static smf_media_priv_t _privs_bak[SMF_STREAM_END];
static void smf_message_callback(smf_stream_message_t* msg) {
    smf_media_priv_t* priv = smf_media_ids_get(msg->stream);
    if(priv){
        memcpy( &_privs_bak[msg->stream], priv, sizeof(smf_media_priv_t) );
    }
    dbgNxPXL("[%02x]%02x %02x", msg->msgid, msg->stream, _privs_bak[msg->stream].smf_media_id);

    switch (msg->msgid) {
    case SMF_STREAM_MESSAGE_eos:		//0x01
        if(priv && priv->cookie)media_stub_notify_event(priv->cookie, MEDIA_EVENT_COMPLETED, 0, 0);
        break;
    case SMF_STREAM_MESSAGE_error:	//0x02
        break;
    case SMF_STREAM_MESSAGE_done:	//0x03
        break;
    case SMF_STREAM_MESSAGE_status:	//0x04
        break;
    case SMF_STREAM_MESSAGE_progress://0x05
        break;
    case SMF_STREAM_MESSAGE_meta:	//0x06
        break;
    case SMF_STREAM_MESSAGE_media:	//0x07
        break;
    case SMF_STREAM_MESSAGE_start_sucess:	//0x08
        if( _privs_bak[msg->stream].cookie && (msg->stream==_privs_bak[msg->stream].smf_media_id) ) media_stub_notify_event(_privs_bak[msg->stream].cookie, MEDIA_EVENT_STARTED, 0, 0);
        break;
    case SMF_STREAM_MESSAGE_start_fail:	//0x09
        if( _privs_bak[msg->stream].cookie && (msg->stream==_privs_bak[msg->stream].smf_media_id) ) media_stub_notify_event(_privs_bak[msg->stream].cookie, MEDIA_EVENT_STARTED, -1, 0);
        break;
    case SMF_STREAM_MESSAGE_vad: //0x10
        //smf_media_vad_ready();
        smf_invoke(&smf_media_vad_ready, 0, 0);
        if(priv && priv->cookie) media_stub_notify_event(priv->cookie, MEDIA_EVENT_COMPLETED, 0, 0);
        break;
    case SMF_STREAM_MESSAGE_wakeup:	//0x11
        smf_invoke(&smf_media_vad_predata_done, 0, 0);
        break;
    case SMF_STREAM_MESSAGE_aivioce:	//0x12
        break;
    case SMF_STREAM_MESSAGE_kws:
        break;
    case SMF_STREAM_MESSAGE_stop_sucess:
        if( _privs_bak[msg->stream].cookie && (msg->stream==_privs_bak[msg->stream].smf_media_id) ) media_stub_notify_event(_privs_bak[msg->stream].cookie, MEDIA_EVENT_STOPPED, 0, 0);
        break;
    case SMF_STREAM_MESSAGE_stop_fail:
        if( _privs_bak[msg->stream].cookie && (msg->stream==_privs_bak[msg->stream].smf_media_id) ) media_stub_notify_event(_privs_bak[msg->stream].cookie, MEDIA_EVENT_STOPPED, -1, 0);
        break;
    default:
        dbgWarnPXL("unknow msg");
        break;
    }
}

bool smf_media_callback_register(void) {
    return smf_stream_register_async(SMF_STREAM_MESSAGE_CALLBACK_0, &smf_message_callback, 0);
}