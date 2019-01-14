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
    char** sip_uris;
};


// global variables
struct buffers sip_buffers;

void sip_buffers_init(void){
    sip_buffers.buffers = NULL;
    sip_buffers.sip_uris = NULL;
    sip_buffers.len = 0;
    sip_buffers.maxlen = 0;
    return;
}

int sip_buffers_allocate(void){
    const size_t ini_max = 32;
    // init
    struct t_gui_buffer** buffers = NULL;
    char** sip_uris = NULL;
    // allocate
    buffers = malloc(ini_max * sizeof(*buffers));
    if(!buffers) goto fail;
    sip_uris = malloc(ini_max * sizeof(*sip_uris));
    if(!sip_uris) goto fail;
    // done allocating
    sip_buffers.buffers = buffers;
    sip_buffers.sip_uris = sip_uris;
    sip_buffers.maxlen = ini_max;
    return 0;
fail:
    if(buffers) free(buffers);
    if(sip_uris) free(sip_uris);
    return 1;
}

void sip_buffers_free(void){
    // close all of the buffers
    for(size_t i = 0; i < sip_buffers.len; i++){
        // free the buffer
        weechat_buffer_close(sip_buffers.buffers[i]);
        // free the "contact" string
        free(sip_buffers.sip_uris[i]);
    }
    // free the buffer list
    if(sip_buffers.buffers){
        free(sip_buffers.buffers);
    }
    // free the string list
    if(sip_buffers.sip_uris){
        free(sip_buffers.sip_uris);
    }
    sip_buffers.len = 0;
    sip_buffers.maxlen = 0;
}

char* dup_only_sip_uri(const char* from, size_t len){
    regex_t reg;
    bool regex_allocated = false;
    char* out = NULL;

    // regex needs a null-terminated string, so we will put it in out
    out = malloc(len + 1);
    if(!out) goto fail;
    memcpy(out, from, len);
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


char* dup_only_phone_number(const char* sip_uri){
    regex_t reg;
    bool regex_allocated = false;
    char* out = NULL;
    size_t len = strlen(sip_uri);

    // we put the original string in out. If there's no match, return original
    out = malloc(len + 1);
    if(!out) goto fail;
    memcpy(out, sip_uri, len);
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
    return out;
fail:
    if(out) free(out);
    if(regex_allocated) regfree(&reg);
    return NULL;
}

// add a sip uri (and corresponding weechat buffer) to sip_buffers
/* *contact should be an allocated string, and this function will free it if
   there is an error */
struct t_gui_buffer* sip_buffers_new(char* sip_uri){
    // check if we need to grow our list first
    if(sip_buffers.len == sip_buffers.maxlen){
        // double the size of the buffers
        size_t new_max = 2 * sip_buffers.maxlen;
        // realloc buffers
        struct t_gui_buffer** buffers;
        buffers = realloc(sip_buffers.buffers, new_max * sizeof(*buffers));
        if(!buffers) return NULL;
        sip_buffers.buffers = buffers;
        // realloc sip_uris
        char** sip_uris;
        sip_uris = realloc(sip_buffers.sip_uris, new_max * sizeof(*sip_uris));
        if(!sip_uris) return NULL;
        sip_buffers.sip_uris = sip_uris;
        sip_buffers.maxlen = new_max;
    }
    struct t_gui_buffer* buffer = NULL;
    char* buffername = NULL;

    buffername = dup_only_phone_number(sip_uri);
    if(!buffername) goto fail;
    buffer = weechat_buffer_new(buffername,
                                sip_buffer_input_cb, sip_uri, NULL,
                                sip_buffer_close_cb, sip_uri, NULL);
    if(!buffer) goto fail;

    sip_buffers.sip_uris[sip_buffers.len] = sip_uri;
    sip_buffers.buffers[sip_buffers.len] = buffer;
    sip_buffers.len += 1;

    free(buffername);
    return buffer;
fail:
    if(buffername) free(buffername);
    if(buffer) weechat_buffer_close(buffer);
    free(sip_uri);
    return NULL;
}

// for when you recv a msg: returns an existing buffer or allocates a new one
struct t_gui_buffer* sip_buffers_get(const char* from, size_t flen){
    /* this is where we will allocate the memory that gets stored into
       sip_buffers.sip_uris.  If we already have the buffer we need to free it.
       Otherwise sip_buffers_new() will handle freeing it on errors. */
    char* sip_uri = dup_only_sip_uri(from, flen);
    if(!sip_uri) return NULL;
    // check if we already have a matching buffer
    for(size_t i = 0; i < sip_buffers.len; i++){
        if(strcmp(sip_uri, sip_buffers.sip_uris[i]) == 0){
            // found matching buffer
            free(sip_uri);
            return sip_buffers.buffers[i];
        }
    }
    // if we didn't find anything, allocated it now
    return sip_buffers_new(sip_uri);
}

// for when a buffer is closed: remove it from sip_buffers
void sip_buffers_delete(const char* sip_uri){
    for(size_t i = 0; i < sip_buffers.len; i++){
        if(strcmp(sip_uri, sip_buffers.sip_uris[i]) == 0){
            /* found matching buffer; weechat will free the buffer we
               allocated, but we need to free the string we allocated. Then we
               need to leftshift both arrays. */
            free(sip_buffers.sip_uris[i]);
            memmove(&sip_buffers.sip_uris[i],
                    &sip_buffers.sip_uris[i+1],
                    (sip_buffers.len - i - 1) * sizeof(*sip_buffers.sip_uris));
            memmove(&sip_buffers.buffers[i],
                    &sip_buffers.buffers[i+1],
                    (sip_buffers.len - i - 1) * sizeof(*sip_buffers.buffers));
            return;
        }
    }
}
