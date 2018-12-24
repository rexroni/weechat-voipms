#ifndef VOIPMS_H
#define VOIPMS_H

#include <weechat/weechat-plugin.h>

#include "config.h"

extern struct t_weechat_plugin *weechat_plugin;
extern struct t_gui_buffer* voip_buffer;
// name of .weechat dir
extern const char *wc_dir;

int voip_plugin_send_sms(struct t_gui_buffer* buffer, const char* contact,
                         const char* msg);
int voip_plugin_handle_sms(const char* from, size_t flen,
                           const char* body, size_t blen);
int voip_plugin_handle_mms(const char* from, size_t flen,
                           const char* mime, size_t mlen,
                           const char* body, size_t blen);
int sip_buffer_input_cb(const void* ptr, void* data,
                        struct t_gui_buffer* buffer, const char* input_data);
int sip_buffer_close_cb(const void* ptr, void* data,
                        struct t_gui_buffer* buffer);

#endif // VOIPMS_H
