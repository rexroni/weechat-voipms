#include <unistd.h>
#include <string.h>
#include <fcntl.h>

#include <weechat/weechat-plugin.h>

#include "voipms.h"
#include "buffers.h"
#include "sip_client.h"

WEECHAT_PLUGIN_NAME("voipms")
WEECHAT_PLUGIN_DESCRIPTION("send and receive sms from your voip.ms account")
WEECHAT_PLUGIN_AUTHOR("Rex Roni")
WEECHAT_PLUGIN_VERSION("0.1")
WEECHAT_PLUGIN_LICENSE("UNLICENSE")

// optional
WEECHAT_PLUGIN_PRIORITY(1000)

// global variables
struct t_weechat_plugin *weechat_plugin;
struct t_gui_buffer* voip_buffer;

static int do_sms(const void* ptr, void* data, struct t_gui_buffer* cmd_buffer,
                  int argc, char** argv, char** argv_eol){
    (void)ptr;
    (void)data;

    if(argc < 3){
        weechat_printf(cmd_buffer, "/sms needs a number and a message");
        return WEECHAT_RC_ERROR;
    }

    // build a SIP uri from the phone number that was given
    char contact[256];
    sprintf(contact, "sip:%.32s@" REALM, argv[1]);

    // ignore what buffer this was called from
    struct t_gui_buffer* buffer = sip_buffers_get(contact, strlen(contact));
    if(!buffer) return WEECHAT_RC_ERROR;

    return voip_plugin_send_sms(buffer, contact, argv_eol[2]);
}

int voip_plugin_send_sms(struct t_gui_buffer* buffer, const char* contact,
                         const char* msg){
    // echo the input data for the user
    weechat_printf_date_tags (buffer, 0, "self_msg", "me:\t%s", msg);

    // send via sip
    sip_client_send_sms(contact, msg);
    // TODO: error handling

    return WEECHAT_RC_OK;
}

int voip_buffer_close_cb(const void* ptr, void* data,
                         struct t_gui_buffer* buffer){
    // don't try to print to voip_buffer any more
    voip_buffer = NULL;
    return WEECHAT_RC_OK;
}

int voip_plugin_handle_sms(const char* from, size_t flen,
                           const char* body, size_t blen){
    // just print to the appropriate weechat buffer
    struct t_gui_buffer* buffer = sip_buffers_get(from, flen);
    if(!buffer) return WEECHAT_RC_ERROR;
    weechat_printf_date_tags(buffer, 0, "notify_highlight", "%s%.*s",
                             weechat_color("green"), (int)blen, body);
    return WEECHAT_RC_OK;
}

int voip_plugin_handle_mms(const char* from, size_t flen,
                           const char* mime, size_t mlen,
                           const char* body, size_t blen){
    // we are mime-stoopid for now
    (void)mime;
    (void)mlen;
    // getthe appropriate weechat buffer
    struct t_gui_buffer* buffer = sip_buffers_get(from, flen);
    if(!buffer) return WEECHAT_RC_ERROR;
    // save to a unique filename based on the time
    char d[128];
    time_t epoch = time(NULL);
    struct tm* tnow = localtime(&epoch);
    strftime(d, sizeof(d), "%Y_%H:%M:%S", tnow);
    char path[256];
    sprintf(path, "%s.mms", d);
    weechat_printf_date_tags(buffer, 0, "notify_highlight",
                             "%sGot message of type %.*s, saving to %s",
                             weechat_color("green"), (int)mlen, mime, path);
    int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if(fd >= 0){
        write(fd, body, blen);
        close(fd);
    }
    return WEECHAT_RC_OK;
}

void voip_plugin_init(void){
    voip_buffer = NULL;
    sip_buffers_init();

}

void voip_plugin_cleanup(void){
    sip_teardown();
    sip_buffers_free();
}

int weechat_plugin_init (struct t_weechat_plugin *plugin,
                         int argc, char *argv[]){
    weechat_plugin = plugin;
    voip_plugin_init();

    // create a "/sms" command
    weechat_hook_command("sms",
                         "send an sms message",
                         "number message...",
                         "number: a 10-digit phone number\n"
                         "message: the message to send",
                         NULL,
                         do_sms, NULL, NULL);

    // open the VOIP buffer
    voip_buffer = weechat_buffer_new("voipms",
                                     NULL, NULL, NULL,
                                     voip_buffer_close_cb, NULL, NULL);
    if(!voip_buffer){
        voip_plugin_cleanup();
        return WEECHAT_RC_ERROR;
    }

    // allocate sip_buffers
    if(sip_buffers_allocate()){
        voip_plugin_cleanup();
        return WEECHAT_RC_ERROR;
    }

    // launch the pjsip client
    if(sip_setup()){
        voip_plugin_cleanup();
        return WEECHAT_RC_ERROR;
    }

    return WEECHAT_RC_OK;
}

int weechat_plugin_end (struct t_weechat_plugin *plugin){

    voip_plugin_cleanup();

    return WEECHAT_RC_OK;
}

int sip_buffer_input_cb(const void* ptr, void* data,
                        struct t_gui_buffer* buffer, const char* input_data){
    (void)data;
    // dereference the contact associated with this buffer
    const char* contact = (const char*)ptr;
    return voip_plugin_send_sms(buffer, contact, input_data);
}

int sip_buffer_close_cb(const void* ptr, void* data,
                        struct t_gui_buffer* buffer){
    (void)data;
    // dereference the contact associated with this buffer
    const char* contact = (const char*)ptr;
    // delete this buffer from our list
    sip_buffers_delete(contact);
    return WEECHAT_RC_OK;
}
