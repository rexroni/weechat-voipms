#ifndef HISTORY_H
#define HISTORY_H

#include <stdbool.h>

// we are going to do a quick-and-easy (malloc-heavy) design here

// per-buffer history, (file name)
typedef struct hist_buf_t {
    // filename is of format "<sip_uri>name"
    char* filename;
    char* sip_uri;
    char* name;
    struct hist_buf_t* next;
} hist_buf_t;

// per-message history, (file contents)
typedef struct hist_msg_t {
    // messages of format "epochtime:[0|1]:msg_len:msg_bytes\n"
    time_t time;
    bool me;
    char* msg;
    size_t len;
    struct hist_msg_t* next;
} hist_msg_t;

void free_hist_buf(hist_buf_t *hist);
void free_hist_msg(hist_msg_t *msg);

// get a linked list of all the available buffer history files
int list_hist_bufs(const char* wc_dir, hist_buf_t **out);

// get a linked list of messages from a history file
int get_hist_msg(const char* wc_dir, const char* fname, hist_msg_t **out);

// add a message to the history
int hist_add_msg(const char* wc_dir, const char* sip_uri, const char* name,
                 const char* msg, size_t msg_len, bool me);

#endif // HISTORY_H
