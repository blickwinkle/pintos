/* vm.c: Generic interface for virtual memory objects. */

#include "threads/malloc.h"
#include "vm/vm.h"
#include "userprog/pagedir.h"
#include "threads/synch.h"
#include "threads/vaddr.h"
#include "swap.h"
#include "lib/kernel/list.h"
#include "threads/interrupt.h"

/* Locks */
static struct lock frame_lock;

static struct list frame_table;

/* Initializes the virtual memory subsystem by invoking each subsystem's
 * intialize codes. */
void
vm_init (void) {
	vm_anon_init ();
	vm_file_init ();
#ifdef EFILESYS  /* For project 4 */
	pagecache_init ();
#endif
	lock_init (&frame_lock);
	list_init (&frame_table);

	

	// register_inspect_intr ();
	/* DO NOT MODIFY UPPER LINES. */
	/* TODO: Your code goes here. */
}

/* Get the type of the page. This function is useful if you want to know the
 * type of the page after it will be initialized.
 * This function is fully implemented now. */
enum vm_type
page_get_type (struct page *page) {
	int ty = VM_TYPE (page->operations->type);
	switch (ty) {
		case VM_UNINIT:
			return VM_TYPE (page->uninit.type);
		default:
			return ty;
	}
}



/* Helpers */
static struct frame *vm_get_victim (void);
static bool vm_do_claim_page (struct page *page);
static struct frame *vm_evict_frame (void);
static bool
vm_stack_growth (void *addr);
/* Create the pending page object with initializer. If you want to create a
 * page, do not create it directly and make it through this function or
 * `vm_alloc_page`. */
bool
vm_alloc_page_with_initializer (enum vm_type type, void *upage, bool writable,
		vm_initializer *init, void *aux) {

	ASSERT (VM_TYPE(type) != VM_UNINIT)

	struct supplemental_page_table *spt = &thread_current ()->spt;
	
	// 
	ASSERT(pg_round_down(upage) == upage);
	/* Check wheter the upage is already occupied or not. */
	if (spt_find_page (spt, upage) == NULL) {
		/* TODO: Create the page, fetch the initialier according to the VM type,
		 * TODO: and then create "uninit" page struct by calling uninit_new. You
		 * TODO: should modify the field after calling the uninit_new. */
        struct page *page = malloc (sizeof *page);
        if (page == NULL)
            goto err;

        uninit_new (page, upage, init, type, aux, writable, spt, VM_TYPE (type) == VM_ANON ? anon_initializer : file_backed_initializer);

		/* TODO: Insert the page into the spt. */
        if (!spt_insert_page (spt, page))
            goto err;
        return true;
	}
err:
	return false;
}



/* Find VA from spt and return page. On error, return NULL. */
struct page *
spt_find_page (struct supplemental_page_table *spt, void *va) {
	/* TODO: Fill this function. */
    struct page tmp = {.va = va};

    // lock_acquire(&spt->lock);
    struct hash_elem *res = hash_find(&spt->pages, &tmp.elem);
    // lock_release(&spt->lock);

	return res != NULL ? hash_entry (res, struct page, elem) : NULL;
}

/* Insert PAGE into spt with validation. */
bool
spt_insert_page (struct supplemental_page_table *spt,
		struct page *page) {
	bool succ = false;
	/* TODO: Fill this function. */
    // lock_acquire(&spt->lock);
    succ = (hash_insert(&spt->pages, &page->elem) == NULL);
    // lock_release(&spt->lock);

	return succ;
}

void
spt_remove_page (struct supplemental_page_table *spt, struct page *page) {
	ASSERT(lock_held_by_current_thread(&spt->lock));

	if (page->frame != NULL) {
		lock_acquire(&frame_lock);
		list_remove(&page->frame->elem);
		lock_release(&frame_lock);
	}
	
	hash_delete(&spt->pages, &page->elem);
	vm_dealloc_page (page);
	// return true;
}

/* Get the struct frame, that will be evicted. Get the usr spt lock and Del it from frame list*/
static struct frame *
vm_get_victim (void) {
	struct frame *victim = NULL;
	 /* TODO: The policy for eviction is up to you. */

	lock_acquire(&frame_lock);
	
	for (struct list_elem *e = list_begin(&frame_table); e != list_end(&frame_table); e = list_next(e)) {
		struct frame *tmp_v = list_entry(e, struct frame, elem);
		if (tmp_v->page->pin_count == 0 && lock_try_acquire(&tmp_v->page->spt->lock)) {
			list_remove(e);
			victim = tmp_v;
			break;
		}
	}
	lock_release(&frame_lock);

	return victim;
}

/* Evict one page and return the corresponding frame.
 * Return NULL on error.*/
static struct frame *
vm_evict_frame (void) {
	struct frame *victim = vm_get_victim ();
	
	for (int i = 0; i < 6 && victim == NULL; i++) {
		timer_msleep(5 + i * 3);
		victim = vm_get_victim();
	}
	if (victim == NULL) {
		return NULL;
	}

	ASSERT(lock_held_by_current_thread(&victim->page->spt->lock));

	/* TODO: swap out the victim and return the evicted frame. */
	if (!swap_out(victim->page)) {
		// PANIC("Swap out failed");
		lock_acquire(&frame_lock);
		list_push_back(&frame_table, &victim->elem);
		lock_release(&frame_lock);
		lock_release(&victim->page->spt->lock);
		return NULL;
	}

	victim->page->frame = NULL;

	if (victim->page->spt->thread->pagedir == NULL) {
		printf("bad!\n");
	}

	pagedir_clear_page(victim->page->spt->thread->pagedir, victim->page->va);

	lock_release(&victim->page->spt->lock);
	return victim;
}

/* palloc() and get frame. If there is no available page, evict the page
 * and return it. This always return valid address. That is, if the user pool
 * memory is full, this function evicts the frame to get the available memory
 * space.*/
static struct frame *
vm_get_frame (void) {
	struct frame *frame = NULL;
	/* TODO: Fill this function. */

	/* Get the frame. */
	
	// ASSERT (frame != NULL);
	// frame->page = NULL;
	void *kva = palloc_get_page (PAL_USER);
	
	/* If the memory is full, evict the page. */
	if (kva == NULL) {
        // PANIC("Not implemented");
		frame = vm_evict_frame ();
		return frame;
	}
	frame = malloc (sizeof (struct frame));
	if (frame == NULL) {
		palloc_free_page (kva);
		return NULL;
	}
	frame->kva = kva;

	// ASSERT (frame->page == NULL);
	return frame;
}

/* Growing the stack. */
static bool
vm_stack_growth (void *addr) {
	// addr = pg_round_down (addr);
	bool success =  vm_claim_page (addr, true);
	if (success) {
		memset(pg_round_down(addr), 0, PGSIZE);
	}
	return success;
}

/* Handle the fault on write_protected page */
static bool
vm_handle_wp (struct page *page UNUSED) {
}

static bool
is_stack_growth (struct intr_frame *f, void *addr, bool user, bool write, bool not_present) {
	if (user && not_present) {
		void *esp = f->esp;
		
		if ((addr >= (esp - 32)) && addr >= (((uint8_t *)PHYS_BASE) - USR_STACK_MAX)) {
			return true;
		}
	}
	return false;
}

/* Return true on success */
bool vm_try_handle_fault (struct intr_frame *f, void *addr, bool user, bool write, bool not_present) {
	if (!not_present) return false;

	struct supplemental_page_table *spt = &thread_current ()->spt;
	struct page *page = NULL;
	
	/* TODO: Validate the fault */
	/* TODO: Your code goes here */
	void *old_addr = addr;
	addr = pg_round_down (addr);

	page = spt_find_page(spt, addr);
	if (page == NULL) {
		if (is_stack_growth(f, old_addr, user, write, not_present)) {
			return vm_stack_growth(old_addr);
		}
		/* The page is not found in the spt. Or page have kva*/
		return false;
	}

	ASSERT(page->frame == NULL);

	return vm_do_claim_page (page);
}

/* Free the page.
 * DO NOT MODIFY THIS FUNCTION. */
void
vm_dealloc_page (struct page *page) {
	destroy (page);
	free (page);
}

/* Claim the page that allocate on VA. */
bool
vm_claim_page (void *va, bool writable) {
	struct page *page = NULL;
	/* TODO: Fill this function */
	va = pg_round_down (va);

	if (!vm_alloc_page(VM_ANON, va, writable)) {
		return false;
	}

	page = spt_find_page(&thread_current ()->spt, va);

	ASSERT(page != NULL);

	return vm_do_claim_page (page);
}

/* Claim the PAGE and set up the mmu. */
static bool
vm_do_claim_page (struct page *page) {
	struct frame *frame = vm_get_frame ();
	if (frame == NULL) {
		return false;
	}
	/* Set links */
	frame->page = page;
	page->frame = frame;

	/* TODO: Insert page table entry to map page's VA to frame's PA. */
	struct thread *t = thread_current();
	lock_acquire(&t->spt.lock);
	ASSERT(pagedir_get_page (t->pagedir, page->va) == NULL); 

	

	pagedir_set_page (t->pagedir, page->va, frame->kva, page->writable);
	lock_release(&t->spt.lock);

	bool ret = swap_in (page, frame->kva);

	lock_acquire(&t->spt.lock);
	
	pagedir_set_accessed (t->pagedir, page->va, false);
	pagedir_set_dirty (t->pagedir, page->va, false);

	// if (!ret) {
		
	// 	free (frame);
	// 	return false;
	// }
	lock_acquire(&frame_lock);
	list_push_back (&frame_table, &frame->elem);
	lock_release(&frame_lock);

	return ret;
}


/* Returns a hash value for page p. */
static unsigned
page_hash (const struct hash_elem *p_, void *aux UNUSED)
{
  const struct page *p = hash_entry (p_, struct page, elem);
  return hash_bytes (&p->va, sizeof p->va);
}

/* Returns true if page a precedes page b. */
static bool
page_less (const struct hash_elem *a_, const struct hash_elem *b_,
           void *aux UNUSED)
{
  const struct page *a = hash_entry (a_, struct page, elem);
  const struct page *b = hash_entry (b_, struct page, elem);

  return a->va < b->va;
}

/* Initialize new supplemental page table */
void
supplemental_page_table_init (struct supplemental_page_table *spt, struct thread *t) {
    hash_init (&spt->pages, page_hash, page_less, NULL);
    spt->thread = t;
    lock_init (&spt->lock);
	list_init(&spt->mmap_table);
}

/* Copy supplemental page table from src to dst */
bool
supplemental_page_table_copy (struct supplemental_page_table *dst UNUSED,
		struct supplemental_page_table *src UNUSED) {
}

static void 
spt_remove_frame_from_list(struct supplemental_page_table *spt) {
	struct list_elem *e;
	struct frame *frame;
	lock_acquire(&frame_lock);
	for (e = list_begin(&frame_table); e != list_end(&frame_table); ) {
		frame = list_entry(e, struct frame, elem);
		if (frame->page->spt == spt) {
			e = list_remove(e);
			// list_remove(e);
		} else {
			e = list_next(e);
		}
	}
	lock_release(&frame_lock);
}

/** Performs some operation on hash element E, given auxiliary
   data AUX. */
 static void 
 page_dealloc_helper (struct hash_elem *e, void *aux UNUSED) {
	struct page *page = hash_entry (e, struct page, elem);
	vm_dealloc_page (page);
 }

/* Free the resource hold by the supplemental page table */
void
supplemental_page_table_kill (struct supplemental_page_table *spt) {
	/* TODO: Destroy all the supplemental_page_table hold by thread and
	 * TODO: writeback all the modified contents to the storage. */
	lock_acquire(&spt->lock);
	spt_remove_frame_from_list(spt);
	lock_release(&spt->lock);

	hash_destroy(&spt->pages, page_dealloc_helper);
}

bool
vm_page_exist(void *va, bool writable, struct intr_frame *f) {
	ASSERT(pg_round_down(va) == va);
	struct supplemental_page_table *spt = &thread_current ()->spt;
	struct page *page = spt_find_page(spt, va);
	if (page == NULL) {
		if (is_stack_growth(f, va, true, writable, true)) {
			return vm_stack_growth(va);
		}
		return false;
	}
	return page->writable || !writable;
}

bool
vm_pin_page(void *va) {
	ASSERT(pg_round_down(va) == va);
	struct supplemental_page_table *spt = &thread_current ()->spt;
	struct page *page = spt_find_page(spt, va);
	if (page == NULL) {
		return false;
	}

	lock_acquire(&page->spt->lock);
	page->pin_count++;
	lock_release(&page->spt->lock);
	
	if (page->frame == NULL) {
		return vm_do_claim_page(page);
	}
	return true;
}

bool
vm_unpin_page(void *va) {
	ASSERT(pg_round_down(va) == va);
	struct supplemental_page_table *spt = &thread_current ()->spt;
	struct page *page = spt_find_page(spt, va);
	if (page == NULL) {
		return false;
	}
	lock_acquire(&page->spt->lock);
	page->pin_count--;
	lock_release(&page->spt->lock);
	return true;
}