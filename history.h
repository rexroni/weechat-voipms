#ifndef HISTORY_H
#define HISTORY_H

#include <stdbool.h>

// we are going to do a quick-and-easy (malloc-heavy) design here

// per-buffer history, (file name)
typedef struct hist_buf_t {
    // filename is of format "contact:[name]"
    char* filename;
    char* contact;
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
int list_hist_bufs(const char* voipms_dir, hist_buf_t **out);

// get a linked list of messages from a history file
int get_hist_msg(const char* voipms_dir, hist_buf_t *buf, hist_msg_t **out);

#endif // HISTORY_H
