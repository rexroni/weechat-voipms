#ifndef SIP_CLIENT_H
#define SIP_CLIENT_H

#include <pjlib.h>
#include <pjlib-util.h>
#include <pjmedia.h>
#include <pjmedia-codec.h>
#include <pjsip.h>
#include <pjsip_simple.h>
#include <pjsip_ua.h>
#include <pjsua-lib/pjsua.h>

int sip_setup();
int sip_teardown();

int sip_client_send_sms(const char* contact, const char* msg);

#endif // SIP_CLIENT_H
