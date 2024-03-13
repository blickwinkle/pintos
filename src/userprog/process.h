#ifndef USERPROG_PROCESS_H
#define USERPROG_PROCESS_H

#include "threads/thread.h"

#define MAX_ARGS_LEN 128

tid_t process_execute (const char *arguments);
int process_wait (tid_t);
void process_exit (void);
void process_activate (void);
bool lazy_load_segment (struct page *page, void *aux);

#endif /**< userprog/process.h */
