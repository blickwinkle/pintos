#ifndef VM_SWAP_H
#define VM_SWAP_H
#include "vm/vm.h"
#define DISK_SWAP_ERROR SIZE_MAX

void disk_swap_init (void);
void disk_swap_in (size_t slot, void *kva);
size_t disk_swap_out (void *kva);
void disk_swap_free(size_t slot);
// bool 

#endif