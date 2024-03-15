#ifndef USERPROG_PAGEDIR_H
#define USERPROG_PAGEDIR_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

uint32_t *pagedir_create (void);
void pagedir_destroy (uint32_t *pd);
bool pagedir_set_page (uint32_t *pd, void *upage, void *kpage, bool rw);
void *pagedir_get_page (uint32_t *pd, const void *upage);
void pagedir_clear_page (uint32_t *pd, void *upage);
bool pagedir_is_dirty (uint32_t *pd, const void *upage);
void pagedir_set_dirty (uint32_t *pd, const void *upage, bool dirty);
bool pagedir_is_accessed (uint32_t *pd, const void *upage);
void pagedir_set_accessed (uint32_t *pd, const void *upage, bool accessed);
void pagedir_activate (uint32_t *pd);
bool read_from_user (void *uaddr, void *dst, size_t bytes);
bool write_to_user (void *uaddr, void *src, size_t bytes);

struct intr_frame;
bool check_user_pointer(const void *uaddr, size_t bytes, bool writeable, struct intr_frame *f);

void pin_user_pointer(const void *uaddr, size_t bytes);
void unpin_user_pointer(const void *uaddr, size_t bytes);

#endif /**< userprog/pagedir.h */
