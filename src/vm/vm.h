#ifndef VM_VM_H
#define VM_VM_H
#include <stdbool.h>
#include "threads/palloc.h"

enum vm_type {
	/* page not initialized */
	VM_UNINIT = 0,
	/* page not related to the file, aka anonymous page */
	VM_ANON = 1,
	/* page that realated to the file */
	VM_FILE = 2,
	/* page that hold the page cache, for project 4 */
	VM_PAGE_CACHE = 3,

	/* Bit flags to store state */

	/* Auxillary bit flag marker for store information. You can add more
	 * markers, until the value is fit in the int. */
	VM_PINED = (1 << 3),
	VM_WRITEBALE = (1 << 4),

	/* DO NOT EXCEED THIS VALUE. */
	VM_MARKER_END = (1 << 31),
};

#include "vm/uninit.h"
#include "vm/anon.h"
#include "vm/file.h"
#include "lib/kernel/hash.h"
#include "threads/synch.h"
#ifdef EFILESYS
#include "filesys/page_cache.h"
#endif

struct page_operations;
struct thread;

#define VM_TYPE(type) ((type) & 7)



// #define VM_PINED(type) ((type) & VM_PINED)
// #define VM_SET_PINED(type) ((type) |= VM_PINED)
// #define VM_UNSET_PINED(type) ((type) &= ~VM_PINED)

// #define VM_WRITEABLE(type) ((type) & VM_WRITEBALE)
// #define VM_SET_WRITEABLE(type) ((type) |= VM_WRITEBALE)
// #define VM_UNSET_WRITEABLE(type) ((type) &= ~VM_WRITEBALE)

/* The representation of "page".
 * This is kind of "parent class", which has four "child class"es, which are
 * uninit_page, file_page, anon_page, and page cache (project4).
 * DO NOT REMOVE/MODIFY PREDEFINED MEMBER OF THIS STRUCTURE. */
 struct supplemental_page_table;
struct page {
	const struct page_operations *operations;
	void *va;              /* Address in terms of user space */
	struct frame *frame;   /* Back reference for frame */

	/* Your implementation */
    struct hash_elem elem; /* For supplemental page table */
	int pin_count;         /* Pin count for eviction */
	bool writable;         /* Writable or not */
	struct supplemental_page_table *spt; /* Back reference for spt */

	struct list_elem mmap_elem; /* For mmap list */

	/* Per-type data are binded into the union.
	 * Each function automatically detects the current union */
	union {
		struct uninit_page uninit;
		struct anon_page anon;
		struct file_page file;
#ifdef EFILESYS
		struct page_cache page_cache;
#endif
	};
};

/* The representation of "frame" */
struct frame {
	void *kva;
	struct page *page;
	struct list_elem elem;
};

/* The function table for page operations.
 * This is one way of implementing "interface" in C.
 * Put the table of "method" into the struct's member, and
 * call it whenever you needed. */
struct page_operations {
	bool (*swap_in) (struct page *, void *);
	bool (*swap_out) (struct page *);
	void (*destroy) (struct page *);
	enum vm_type type;
};

#define swap_in(page, v) (page)->operations->swap_in ((page), v)
#define swap_out(page) (page)->operations->swap_out (page)
#define destroy(page) \
	if ((page)->operations->destroy) (page)->operations->destroy (page)

struct mmap_file {
	int mapid;
	struct file *file;
	struct list_elem elem;
	void *start;
	int len;
	int offset;
	bool writable;
	struct list pages;
};

/* Representation of current process's memory space.
 * We don't want to force you to obey any specific design for this struct.
 * All designs up to you for this. */
struct supplemental_page_table {
    struct hash pages;
	struct list mmap_table;
    struct thread *thread;
    struct lock lock;
};

#include "threads/thread.h"
void supplemental_page_table_init (struct supplemental_page_table *spt, struct thread *t);
bool supplemental_page_table_copy (struct supplemental_page_table *dst,
		struct supplemental_page_table *src);
void supplemental_page_table_kill (struct supplemental_page_table *spt);
struct page *spt_find_page (struct supplemental_page_table *spt,
		void *va);
bool spt_insert_page (struct supplemental_page_table *spt, struct page *page);
void spt_remove_page (struct supplemental_page_table *spt, struct page *page);

void vm_init (void);

struct intr_frame;
bool vm_try_handle_fault (struct intr_frame *f, void *addr, bool user, bool write, bool not_present);

#define vm_alloc_page(type, upage, writable) \
	vm_alloc_page_with_initializer ((type), (upage), (writable), NULL, NULL)
bool vm_alloc_page_with_initializer (enum vm_type type, void *upage,
		bool writable, vm_initializer *init, void *aux);
void vm_dealloc_page (struct page *page);
bool vm_claim_page (void *va, bool writable);
enum vm_type page_get_type (struct page *page);

bool vm_page_exist(void *va, bool writable, struct intr_frame *f);
bool vm_pin_page(void *va);
bool vm_unpin_page(void *va);


#endif  /* VM_VM_H */
