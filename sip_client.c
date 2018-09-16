#include <stdbool.h>

#include "sip_client.h"
#include "voipms.h"
#include "constify.h"

typedef struct {
    pjsua_acc_id aid;
    pjsua_transport_id tid;
    bool did_create;
    bool did_transport;
    bool did_account;
} global_pj_state;

global_pj_state gpj;

void global_pj_state_reset(void){
    gpj.did_create = false;
    gpj.did_transport = false;
    gpj.did_account = false;
}

inline bool pj_str_match(const pj_str_t* a, const pj_str_t* b){
    return (a->slen == b->slen) && (strncmp(a->ptr, b->ptr, a->slen) == 0);
}

void pager_cb(pjsua_call_id call_id,
              const pj_str_t *from,
              const pj_str_t *to,
              const pj_str_t *contact,
              const pj_str_t *mime,
              const pj_str_t *body){
    // print plain text messages to the buffer
    pj_str_t text_plain = pj_str("text/plain");
    if(pj_str_match(mime, &text_plain)){
        // TODO: error handling
        voip_plugin_handle_sms(from->ptr, from->slen,
                               body->ptr, body->slen);
    }
    // for non-text messages, save to a file and show an alert
    else{
        // TODO: error handling
        voip_plugin_handle_mms(from->ptr, from->slen,
                               mime->ptr, mime->slen,
                               body->ptr, body->slen);
    }
}

int sip_client_send_sms(const char* contact, const char* msg){
    pj_str_t to = constify(contact, strlen(contact));
    pj_str_t mime = pj_str("text/plain");
    pj_str_t content = constify(msg, strlen(msg));

    pjsua_im_send(gpj.aid, &to, &mime, &content, NULL, NULL);
    // TODO: error handling
    return 0;
}

// log to a weechat buffer
void pj_log_cb(int level, const char* data, int len){
    if(voip_buffer) weechat_printf(voip_buffer, "%.*s", len, data);
}

int sip_setup(void){
    // set global_pj_state to default values
    global_pj_state_reset();

    // CREATE
    pj_status_t pret = pjsua_create();
    if(pret != PJ_SUCCESS){
        // TODO print error message
        return 1;
    }
    gpj.did_create = true;


    // INIT
    pjsua_config pc;
    pjsua_config_default(&pc);
    pc.cb.on_pager = &pager_cb;
    //pc.cb.on_incoming_call = &incoming_call_cb;
    //pc.cb.on_call_state = &on_call_state;
    //pc.cb.on_call_media_state = &on_call_media_state;
    // handle our own logging
    pjsua_logging_config lc;
    pjsua_logging_config_default(&lc);
    lc.console_level = 5;
    lc.level = 5;
    lc.cb = pj_log_cb;
    // configure media
    pjsua_media_config mc;
    pjsua_media_config_default(&mc);
    // disable auto-closing of snd device
    //// mc.snd_auto_close_time = -1;
    pret = pjsua_init(&pc, &lc, &mc);
    if(pret != PJ_SUCCESS){
        // TODO print error message
        sip_teardown();
        return 2;
    }

    // TRANSPORT
    pjsua_transport_config tc;
    pjsua_transport_config_default(&tc);
    pjsip_transport_type_e type = PJSIP_TRANSPORT_UDP;
    pret = pjsua_transport_create(type, &tc, &gpj.tid);
    if(pret != PJ_SUCCESS){
        // TODO print error message
        sip_teardown();
        return 10;
    }
    gpj.did_transport = true;

    // START
    pret = pjsua_start();
    if(pret != PJ_SUCCESS){
        // TODO print error message
        sip_teardown();
        return 20;
    }

    // ACCOUNT ADD
    pjsip_cred_info creds = {
        .realm = pj_str(REALM),
        .scheme = pj_str("digest"),
        .username = pj_str(USERNAME),
        .data_type = PJSIP_CRED_DATA_PLAIN_PASSWD,
        .data = pj_str(PASSWORD),
    };
    pjsua_acc_config ac;
    pjsua_acc_config_default(&ac);
    ac.id = pj_str(ACCOUNT_ID);
    ac.reg_uri = pj_str(REGISTER_URI);
    ac.cred_info[0] = creds;
    ac.cred_count = 1;
    int make_default_account = 1;
    pret = pjsua_acc_add(&ac, make_default_account, &gpj.aid);
    if(pret != PJ_SUCCESS){
        // TODO print error message
        sip_teardown();
        return 30;
    }
    gpj.did_account = true;
    return 0;
}

int sip_teardown(){
    pj_status_t pret;
    int retval = 0;

    // ACCOUNT DELETE
    if(gpj.did_account){
        pret = pjsua_acc_del(gpj.aid);
        if(pret != PJ_SUCCESS){
            // TODO print error message
            retval = 1;
        }
    }

    // TRANSPORT
    if(gpj.did_transport){
        pret = pjsua_transport_close(gpj.tid, 0);
        if(pret != PJ_SUCCESS){
            // TODO print error message
            retval = 2;
        }
    }

    // DESTROY
    if(gpj.did_create){
        pj_status_t pret = pjsua_destroy();
        if(pret != PJ_SUCCESS){
            // TODO log this
            retval = 3;
        }
    }

    return retval;
}
