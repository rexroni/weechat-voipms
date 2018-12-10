#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <dirent.h>
#include <regex.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>

#include "history.h"

#define OPEN_FLAGS O_RDONLY | O_DIRECTORY | O_CLOEXEC

#define FAIL(n) { retval = n; goto fail; }

void free_hist_buf(hist_buf_t *hist){
    hist_buf_t *p, *next = hist;
    while( (p = next) ){
        if(p->filename) free(p->filename);
        if(p->contact) free(p->contact);
        if(p->name) free(p->name);
        next = p->next;
        free(p);
    }
    hist = NULL;
}

void free_hist_msg(hist_msg_t *msg){
    hist_msg_t *p, *next = msg;
    while( (p = next) ){
        if(p->msg) free(p->msg);
        next = p->next;
        free(p);
    }
    msg = NULL;
}


// get a linked list of all the available buffer history files
int list_hist_bufs(const char* voipms_dir, hist_buf_t **out){
    // .weechat/voipms directory (file descriptor)
    int vdir_fd = -1;
    // .weechat/voipms/history directory (file descriptor)
    int hdir_fd = -1;
    // history directory
    DIR* hdir = NULL;
    // a temporary history entry
    hist_buf_t *hist = NULL;
    // regex for pattern matching
    regex_t reg;
    bool regex_allocated = false;
    // return values
    int retval = -1; // indicate error if we return early
    *out = NULL;

    // (mkdir and) open the voipms_dir
    vdir_fd = open(voipms_dir, OPEN_FLAGS, 0777);
    if(vdir_fd < 0) FAIL(1);

    // open the history directory
    hdir_fd = openat(vdir_fd, "history", OPEN_FLAGS, 0777);
    if(hdir_fd < 0) FAIL(2);

    // make a DIR* out of the hdir_fd
    hdir = fdopendir(hdir_fd);
    if(!hdir) FAIL(3);
    // don't use hdir_fd anymore
    hdir_fd = -1;

    // prepare the regex for pattern matching
    int ret = regcomp(&reg, "(.+):(.*)", REG_EXTENDED);
    if(ret) FAIL(4);
    regex_allocated = true;

    struct dirent* entry;
    // end of the *out linked list
    hist_buf_t **out_end = out;

    while( (entry = readdir(hdir)) ){
        // skip directories
        if(entry->d_type == DT_DIR) continue;

        // skip invalid filenames
        regmatch_t match[3];
        size_t nmatch = sizeof(match) / sizeof(*match);
        ret = regexec(&reg, entry->d_name, nmatch, match, 0);
        if(ret == REG_NOMATCH) continue;

        // allocate a new hist_buf_t entry
        hist_buf_t *hist = malloc(sizeof(*hist));
        if(!hist) FAIL(5);

        // init the hist_buf_t
        *hist = (hist_buf_t){0};

        // duplicate the filename
        hist->filename = strdup(entry->d_name);
        if(!hist->filename) FAIL(6);

        // duplicate the contact name
        // TODO: do all libc implementations add a null byte to this?
        hist->contact = strndup(entry->d_name, match[1].rm_eo);
        if(!hist->contact) FAIL(7);

        // duplicate the buffer name (contact's user-readable name)
        if(match[2].rm_eo - match[2].rm_so > 0){
            hist->name = strdup(entry->d_name + match[2].rm_so);
            if(!hist->name) FAIL(8);
        }

        // store it at the end of the linked list
        *out_end = hist;
        out_end = &hist->next;
        hist = NULL;
    }

    // success!
    retval = 0;

fail:
    if(vdir_fd >= 0) close(vdir_fd);
    if(hdir_fd >= 0) close(hdir_fd);
    if(hdir) closedir(hdir);
    if(regex_allocated) regfree(&reg);
    free_hist_buf(hist);
    // free *out, but only if we are about to return an error
    if(retval != 0) free_hist_buf(*out);
    return retval;
}


// get a message history from a buffer history
int get_hist_msg(const char* voipms_dir, hist_buf_t *buf, hist_msg_t **out){
    // .weechat/voipms directory (file descriptor)
    int vdir_fd = -1;
    // .weechat/voipms/history directory (file descriptor)
    int hdir_fd = -1;
    // message file
    int msg_fd = -1;
    // a temporary msg entry
    hist_msg_t *hist = NULL;
    // memory for storing the entire file in memory
    char *mem = NULL;
    // return values
    int retval = -1;
    *out = NULL;

    // (mkdir and) open the voipms_dir
    vdir_fd = open(voipms_dir, OPEN_FLAGS, 0777);
    if(vdir_fd < 0) FAIL(1);

    // open the history directory
    hdir_fd = openat(vdir_fd, "history", OPEN_FLAGS, 0777);
    if(hdir_fd < 0) FAIL(2);

    // open the message file
    msg_fd = openat(hdir_fd, buf->filename, O_RDONLY, 0777);
    if(msg_fd < 0) FAIL(3);

    // read the entire file into memory
    size_t msize = 8192;
    mem = malloc(msize);
    size_t mlen = 0;
    if(!mem) FAIL(5);

    // read until end of file
    ssize_t amnt_read;
    while( (amnt_read = read(msg_fd, mem + mlen, 4096)) ){
        // check for error
        if(amnt_read < 0) FAIL(6);
        // add to length
        mlen += (size_t)amnt_read;
        // check if we need to reallocate before next read
        if(msize - mlen < 4096){
            msize *= 2;
            char *new = realloc(mem, msize);
            if(!new) FAIL(7);
            mem = new;
        }
    }

    // read every entry in the file
    char *c = mem;
    // end of the *out linked list
    hist_msg_t **out_end = out;
    while(c - mem < mlen){
        // semicolon marks
        int semis = 0;
        char* nums[3] = {c, NULL, NULL};
        for(; c - mem < mlen ; c++){
            if(*c >= '0' && *c <= '9') continue;
            if(*c == ':'){
                if(++semis == 3) break;
                nums[semis] = c + 1;
                continue;
            }
            // syntax error if we got here
            FAIL(8);
        }
        // if we got here without 3 semicolons, it's an error
        if(semis != 3) FAIL(9);

        // advance *c past the last semicolon
        c++;

        // interpret the three values we got
        errno = 0;
        size_t time = strtoul(nums[0], NULL, 10);
        size_t me_val = strtoul(nums[1], NULL, 10);
        size_t bytes_len = strtoul(nums[2], NULL, 10);
        if(errno) FAIL(10);

        // me_val should be 0 (the other person) or 1 (me)
        if(me_val > 1) FAIL(11);

        // make sure we have the whole message loaded, plus the ending newline
        if((c - mem) + bytes_len + 1 > mlen) FAIL(12);

        // start building the temporary entry
        hist = malloc(sizeof(*hist));
        if(!hist) FAIL(13);
        hist->time = time;
        hist->me = me_val;
        hist->len = bytes_len;
        hist->next = NULL;

        // allocate for this message's bytes
        hist->msg = malloc(hist->len + 1);
        if(!hist->msg) FAIL(14);

        // copy the message bytes
        memcpy(hist->msg, c, bytes_len);
        // null-terminate
        hist->msg[hist->len] = '\0';

        // move *c to end of message entry plus ending newline
        c += hist->len + 1;

        // save to end of *out linked list
        *out_end = hist;
        out_end = &hist->next;
        hist = NULL;
    }

    // success!
    retval = 0;

fail:
    if(vdir_fd >= 0) close(vdir_fd);
    if(hdir_fd >= 0) close(hdir_fd);
    if(msg_fd >= 0) close(msg_fd);
    if(mem) free(mem);
    free_hist_msg(hist);
    // free *out, but only if we are about to return an error
    if(retval) free_hist_msg(*out);
    return retval;
}
