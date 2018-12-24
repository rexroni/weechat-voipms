#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "history.h"

int main(){
    int retval = 1;
    hist_buf_t *hist = NULL;
    hist_msg_t *msg = NULL;

    // add a message to a file
    int ret = hist_add_msg("testfiles", "1234567890", 10, "my added msg", 12, true);
    if(ret){
        perror("hist_add_msg");
        printf("ret %d\n", ret);
        goto fail;
    }

    ret = hist_add_msg("testfiles", "1234567890", 10, "their added msg", 15, false);
    if(ret){
        perror("hist_add_msg");
        printf("ret %d\n", ret);
        goto fail;
    }


    // get all history buffers
    ret = list_hist_bufs("testfiles", &hist);
    if(ret){
        perror("list_hist_bufs");
        printf("ret %d\n", ret);
        goto fail;
    }

    printf("history files:\n");
    hist_buf_t *p, *next = hist;
    while( (p = next) ){
        printf("  %s:%s\n", p->contact, p->name);
        // get all messages in this buffer
        hist_msg_t *msg;
        ret = get_hist_msg("testfiles", p->filename, &msg);
        if(ret){
            printf("%d\n", ret);
            perror("get_hist_msg");
            goto fail;
        }
        hist_msg_t *mp, *mnext = msg;
        while( (mp = mnext) ){
            printf("    %lu:%u:%zu:%*s\n", mp->time, mp->me, mp->len,
                                           (int)mp->len, mp->msg);
            mnext = mp->next;
        }
        // done with this message history
        free_hist_msg(msg);
        msg = NULL;
        next = p->next;
    }

    // success!
    retval = 0;

fail:
    free_hist_msg(msg);
    free_hist_buf(hist);
    return retval;
}


