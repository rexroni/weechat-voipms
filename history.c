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
#include <time.h>

#include "history.h"

#define OPENDIR_FLAGS O_RDONLY | O_DIRECTORY | O_CLOEXEC
#define OPEN_RD_FLAGS O_RDONLY | O_CLOEXEC
#define OPEN_WR_FLAGS O_WRONLY | O_APPEND | O_CLOEXEC | O_CREAT , 0666

#define FAIL(n) { retval = n; goto fail; }

void free_hist_buf(hist_buf_t *hist){
    hist_buf_t *p, *next = hist;
    while( (p = next) ){
        if(p->filename) free(p->filename);
        if(p->sip_uri) free(p->sip_uri);
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


/* returns a file descriptor at hdir_fd if hdir_fd is not NULL,
   otherwise returns a DIR* at hdir */
static int open_hist_dir(const char* wc_dir, int *hdir_fd, DIR **hdir){
    // .weechat directory (file descriptor)
    int wdir_fd = -1;
    // .weechat/voipms directory (file descriptor)
    int vdir_fd = -1;
    // .weechat/voipms/history directory (file descriptor)
    int hdir_fd_temp = -1;
    // return values
    if(hdir_fd) *hdir_fd = -1;
    if(hdir) *hdir = NULL;
    int retval = -1;

    // (mkdir and) open the weechat dir
    wdir_fd = open(wc_dir, OPENDIR_FLAGS);
    if(wdir_fd < 0) FAIL(1);

    // attempt to make the directory, ignoring errors
    mkdirat(wdir_fd, "voipms", 0777);
    errno = 0;

    // open the history directory
    vdir_fd = openat(wdir_fd, "voipms", OPENDIR_FLAGS);
    if(vdir_fd < 0) FAIL(2);

    // attempt to make the directory, ignoring errors
    mkdirat(vdir_fd, "history", 0777);
    errno = 0;

    // open the history directory
    hdir_fd_temp = openat(vdir_fd, "history", OPENDIR_FLAGS);
    if(hdir_fd < 0) FAIL(3);

    // if hdir is not NULL, make a DIR* out of the hdir_fd
    if(hdir_fd){
        *hdir_fd = hdir_fd_temp;
    }else{
        *hdir = fdopendir(hdir_fd_temp);
        if(!*hdir) FAIL(4);
        // don't use hdir_fd_temp anymore
        hdir_fd_temp = -1;
    }

    // success!
    retval = 0;

fail:
    if(wdir_fd >= 0) close(wdir_fd);
    if(vdir_fd >= 0) close(vdir_fd);
    if(retval != 0 && hdir_fd_temp >= 0) close(hdir_fd_temp);
    return retval;
}


// get a linked list of all the available buffer history files
int list_hist_bufs(const char* wc_dir, hist_buf_t **out){
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

    int ret = open_hist_dir(wc_dir, NULL, &hdir);
    if(ret) FAIL(4);

    // prepare the regex for pattern matching
    ret = regcomp(&reg, "^<(.+)>(.+)", REG_EXTENDED);
    if(ret) FAIL(5);
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
        if(!hist) FAIL(6);

        // init the hist_buf_t
        *hist = (hist_buf_t){0};

        // duplicate the filename
        hist->filename = strdup(entry->d_name);
        if(!hist->filename) FAIL(7);

        // duplicate the sip_uri
        // TODO: do all libc implementations add a null byte to this?
        hist->sip_uri = strndup(entry->d_name + match[1].rm_so,
                                match[1].rm_eo - match[1].rm_so);
        if(!hist->sip_uri) FAIL(8);

        // duplicate the buffer name
        hist->name = strdup(entry->d_name + match[2].rm_so);
        if(!hist->name) FAIL(9);

        // store it at the end of the linked list
        *out_end = hist;
        out_end = &hist->next;
        hist = NULL;
    }

    // success!
    retval = 0;

fail:
    if(hdir) closedir(hdir);
    if(regex_allocated) regfree(&reg);
    free_hist_buf(hist);
    // free *out, but only if we are about to return an error
    if(retval != 0 && *out) free_hist_buf(*out);
    return retval;
}


// get a message history from a buffer history
int get_hist_msg(const char* wc_dir, const char* fname, hist_msg_t **out){
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

    int ret = open_hist_dir(wc_dir, &hdir_fd, NULL);
    if(ret) FAIL(3);

    // open the message file
    msg_fd = openat(hdir_fd, fname, OPEN_RD_FLAGS);
    if(msg_fd < 0) FAIL(4);

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
    if(hdir_fd >= 0) close(hdir_fd);
    if(msg_fd >= 0) close(msg_fd);
    if(mem) free(mem);
    free_hist_msg(hist);
    // free *out, but only if we are about to return an error
    if(retval) free_hist_msg(*out);
    return retval;
}


static int check_name(int hdir_fd, const char* filename, size_t uri_len){
    // a dup of hdir_fd for making DIR *hdir
    int hdir_fd_dup = -1;
    // history directory
    DIR* hdir = NULL;
    // return values
    int retval = -1; // indicate error if we return early

    // dup hdir_fd
    hdir_fd_dup = dup(hdir_fd);
    if(hdir_fd_dup < 0) FAIL(1);

    // make it a DIR*
    hdir = fdopendir(hdir_fd_dup);
    if(!hdir) FAIL(2);
    // don't close hdir_fd_dup now
    hdir_fd_dup = -1;

    struct dirent* entry;

    while( (entry = readdir(hdir)) ){
        // skip directories
        if(entry->d_type == DT_DIR) continue;

        // skip short filenames (need <, >, and at least 1 name char)
        if(strlen(entry->d_name) < uri_len + 3) continue;

        // match the <sip_uri> portion of the filename
        if(strncmp(entry->d_name, filename, uri_len + 2) != 0) continue;

        // now check if a rematch is necessary
        if(strcmp(entry->d_name, filename) != 0){
            int ret = renameat(hdir_fd, entry->d_name, hdir_fd, filename);
            if(ret) FAIL(3);
        }
        break;
    }

    retval = 0;

fail:
    if(hdir_fd_dup >= 0) close(hdir_fd_dup);
    if(hdir) closedir(hdir);
    return retval;
}

// add a message to the history
int hist_add_msg(const char* wc_dir, const char* sip_uri, const char* name,
                 const char* msg, size_t msg_len, bool me){
    // filename of the history buffer for this sip_uri
    char *fname = NULL;
    int fd = -1;
    // history directory (file descriptor)
    int hdir_fd = -1;
    // return values
    int retval = -1; // indicate error if we return early

    size_t uri_len = strlen(sip_uri);
    size_t name_len = strlen(name);

    // allocate the filename (two strings and < and > and \0)
    fname = malloc(uri_len + strlen(name) + 3);
    if(!fname) FAIL(1);

    // build the filename
    sprintf(fname, "<%.*s>%.*s", (int)uri_len, sip_uri, (int)name_len, name);

    // open the history directory
    int ret = open_hist_dir(wc_dir, &hdir_fd, NULL);
    if(ret) FAIL(2);

    // check if the same file exists but under a different name
    ret = check_name(hdir_fd, fname, uri_len);
    if(ret) FAIL(3);

    // open the file for appending
    fd = openat(hdir_fd, fname, OPEN_WR_FLAGS);
    if(fd < 0) FAIL(4);

    // get the time
    time_t t = time(NULL);
    if(t == ((time_t)-1)) FAIL(5);

    // append the message to the file
    ret = dprintf(fd, "%ld:%d:%zu:%.*s\n",t, me, msg_len, (int)msg_len, msg);
    if(ret < 0) FAIL(6);

    // success!
    retval = 0;

fail:
    if(fname) free(fname);
    if(fd >= 0) close(fd);
    if(hdir_fd >= 0) close(hdir_fd);
    return retval;
}

