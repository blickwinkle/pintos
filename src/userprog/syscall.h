#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

#include <stdint.h>
#include <stdbool.h>

struct intr_frame;

void syscall_init (void);
bool argraw(int n, struct intr_frame *f, uint32_t *ret);
#endif /**< userprog/syscall.h */
