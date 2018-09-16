#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <regex.h>

#include "buffers.h"
#include "voipms.h"

struct buffers {
    // number of elements in each list:
    size_t len;
    // maximum number of elements:
    size_t maxlen;
    // buffers
    struct t_gui_buffer** buffers;
    // sip uri associated with each buffer
    char** contacts;
};


// global variables
struct buffers sip_buffers;

void sip_buffers_init(void){
    return;
    sip_buffers.buffers = NULL;
    sip_buffers.contacts = NULL;
    sip_buffers.len = 0;
    sip_buffers.maxlen = 0;
}

int sip_buffers_allocate(void){
    const size_t ini_max = 32;
    // init
    struct t_gui_buffer** buffers = NULL;
    char** contacts = NULL;
    // allocate
    buffers = malloc(ini_max * sizeof(*buffers));
    if(!buffers) goto fail;
    contacts = malloc(ini_max * sizeof(*contacts));
    if(!contacts) goto fail;
    // done allocating
    sip_buffers.buffers = buffers;
    sip_buffers.contacts = contacts;
    sip_buffers.maxlen = ini_max;
    return 0;
fail:
    if(buffers) free(buffers);
    if(contacts) free(contacts);
    return 1;
}

void sip_buffers_free(void){
    // close all of the buffers
    for(size_t i = 0; i < sip_buffers.len; i++){
        // free the buffer
        weechat_buffer_close(sip_buffers.buffers[i]);
        // free the "contact" string
        free(sip_buffers.contacts[i]);
    }
    // free the buffer list
    if(sip_buffers.buffers){
        free(sip_buffers.buffers);
    }
    // free the string list
    if(sip_buffers.contacts){
        free(sip_buffers.contacts);
    }
    sip_buffers.len = 0;
    sip_buffers.maxlen = 0;
}

char* dup_only_sip_uri(const char* contact, size_t len){
    regex_t reg;
    bool regex_allocated = false;
    char* out = NULL;

    // regex needs a null-terminated string, so we will put it in out
    out = malloc(len + 1);
    if(!out) goto fail;
    memcpy(out, contact, len);
    out[len] = '\0';

    // try and get just the sip uri
    int ret = regcomp(&reg, "sip:[^\\s@]*@[-a-zA-Z0-9.]*", REG_EXTENDED);
    if(ret) goto fail; else regex_allocated = true;
    regmatch_t match;
    ret = regexec(&reg, out, 1, &match, 0);
    if(ret == 0){
        int siplen = match.rm_eo - match.rm_so;
        memmove(out, out + match.rm_so, siplen);
        out[siplen] = '\0';
    }else{
        char ebuf[256];
        size_t len = regerror(ret, &reg, ebuf, sizeof(ebuf));
        weechat_printf(NULL, "regex says: %.*s", (int)len, ebuf);
    }
    regfree(&reg);
    // if it can't do it, try and just create a new buffer
    return out;
fail:
    if(out) free(out);
    if(regex_allocated) regfree(&reg);
    return NULL;
}


char* dup_only_phone_number(const char* contact){
    regex_t reg;
    bool regex_allocated = false;
    char* out = NULL;
    size_t len = strlen(contact);

    // regex needs a null-terminated string, so we will put it in out
    out = malloc(len + 1);
    if(!out) goto fail;
    memcpy(out, contact, len);
    out[len] = '\0';

    // try and get just the sip uri
    int ret = regcomp(&reg, "sip:([0-9]{10})@[-a-zA-Z0-9]", REG_EXTENDED);
    if(ret) goto fail; else regex_allocated = true;
    regmatch_t matches[2];
    ret = regexec(&reg, out, 2, matches, 0);
    if(ret == 0){
        // regex match! set *out to be the 10-digit substring
        memmove(out, out + matches[1].rm_so, 10);
        out[10] = '\0';
    }
    regfree(&reg);
    // if it can't do it, try and just create a new buffer
    return out;
fail:
    if(out) free(out);
    if(regex_allocated) regfree(&reg);
    return NULL;
}

// add a sip uri (and corresponding weechat buffer) to sip_buffers
/* *contact should be an allocated string, and this function will free it if
   there is an error */
struct t_gui_buffer* sip_buffers_new(char* contact){
    // check if we need to grow our list first
    if(sip_buffers.len == sip_buffers.maxlen){
        // double the size of the buffers
        size_t new_max = 2 * sip_buffers.maxlen;
        // realloc buffers
        struct t_gui_buffer** buffers;
        buffers = realloc(sip_buffers.buffers, new_max * sizeof(*buffers));
        if(!buffers) return NULL;
        sip_buffers.buffers = buffers;
        // realloc contact
        char** contacts;
        contacts = realloc(sip_buffers.contacts, new_max * sizeof(*contacts));
        if(!contacts) return NULL;
        sip_buffers.contacts = contacts;
        sip_buffers.maxlen = new_max;
    }
    struct t_gui_buffer* buffer = NULL;
    char* buffername = NULL;

    buffername = dup_only_phone_number(contact);
    if(!buffername) goto fail;
    buffer = weechat_buffer_new(buffername,
                                sip_buffer_input_cb, contact, NULL,
                                sip_buffer_close_cb, contact, NULL);
    if(!buffer) goto fail;

    sip_buffers.contacts[sip_buffers.len] = contact;
    sip_buffers.buffers[sip_buffers.len] = buffer;
    sip_buffers.len += 1;

    free(buffername);
    return buffer;
fail:
    if(buffername) free(buffername);
    if(buffer) weechat_buffer_close(buffer);
    free(contact);
    return NULL;
}

// for when you recv a msg: returns an existing buffer or allocates a new one
struct t_gui_buffer* sip_buffers_get(const char* contact_in, size_t len){
    /* this is where we will allocate the memory that gets stored into
       sip_buffers.contacts.  If we already have the buffer we need to free it.
       Otherwise sip_buffers_new() will handle freeing it on errors. */
    char* contact = dup_only_sip_uri(contact_in, len);
    if(!contact) return NULL;
    // check if we already have a matching buffer
    for(size_t i = 0; i < sip_buffers.len; i++){
        if(strcmp(contact, sip_buffers.contacts[i]) == 0){
            // found matching buffer
            free(contact);
            return sip_buffers.buffers[i];
        }
    }
    // if we didn't find anything, allocated it now
    return sip_buffers_new(contact);
}

// for when a buffer is closed: remove it from sip_buffers
void sip_buffers_delete(const char* contact){
    for(size_t i = 0; i < sip_buffers.len; i++){
        if(strcmp(contact, sip_buffers.contacts[i]) == 0){
            /* found matching buffer; weechat will free the buffer we
               allocated, but we need to free the string we allocated. Then we
               need to leftshift both arrays. */
            free(sip_buffers.contacts[i]);
            memmove(&sip_buffers.contacts[i],
                    &sip_buffers.contacts[i+1],
                    (sip_buffers.len - i - 1) * sizeof(*sip_buffers.contacts));
            memmove(&sip_buffers.buffers[i],
                    &sip_buffers.buffers[i+1],
                    (sip_buffers.len - i - 1) * sizeof(*sip_buffers.buffers));
            return;
        }
    }
}
