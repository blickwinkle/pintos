/* anon.c: Implementation of page for non-disk image (a.k.a. anonymous page). */

#include "vm/vm.h"
#include "vm/swap.h"
#include "userprog/process.h"
#include "userprog/pagedir.h"
// #include "devices/disk.h"

/* DO NOT MODIFY BELOW LINE */
static bool anon_swap_in (struct page *page, void *kva);
static bool anon_swap_out (struct page *page);
static void anon_destroy (struct page *page);

/* DO NOT MODIFY this struct */
static const struct page_operations anon_ops = {
	.swap_in = anon_swap_in,
	.swap_out = anon_swap_out,
	.destroy = anon_destroy,
	.type = VM_ANON,
};

/* Initialize the data for anonymous pages */
void
vm_anon_init (void) {
	/* TODO: Set up the swap_disk. */
	disk_swap_init();
}

/* Initialize the file mapping */
bool
anon_initializer (struct page *page, enum vm_type type, void *kva) {
	/* Set up the handler */
	page->operations = &anon_ops;
	struct uninit_page *uninit = &page->uninit;

	vm_initializer *init = uninit->init;
	void *aux = uninit->aux;

	struct anon_page *anon_page = &page->anon;
	anon_page->init = init;
	anon_page->aux = aux;
	anon_page->slot = DISK_SWAP_ERROR;
	anon_page->isDirty = false;
}

static bool
isLoadSegPage(struct page *page) {
	struct anon_page *anon_page = &page->anon;
	return anon_page->init == lazy_load_segment && !anon_page->isDirty;
}

/* Swap in the page by read contents from the swap disk. */
static bool
anon_swap_in (struct page *page, void *kva) {
	struct anon_page *anon_page = &page->anon;
	if (isLoadSegPage(page)) {
		return anon_page->init(page, anon_page->aux);
	}

	disk_swap_in(anon_page->slot, kva);
	anon_page->slot = DISK_SWAP_ERROR;

	

	return true;
}

/* Swap out the page by writing contents to the swap disk. */
static bool
anon_swap_out (struct page *page) {
	struct anon_page *anon_page = &page->anon;
	if (!anon_page->isDirty && page->writable) {
		anon_page->isDirty = pagedir_is_dirty(page->spt->thread->pagedir, page->va);
	}
	if (isLoadSegPage(page)) {
		return true;
	}

	size_t slot =  disk_swap_out(page->frame->kva);
	anon_page->slot = slot;
	return slot != DISK_SWAP_ERROR;
}

/* Destroy the anonymous page. PAGE will be freed by the caller. */
static void
anon_destroy (struct page *page) {

	struct anon_page *anon_page = &page->anon;

	if (anon_page->aux != NULL) {
		free(anon_page->aux);
	}

	if (!isLoadSegPage(page) && anon_page->slot != DISK_SWAP_ERROR) {
		disk_swap_free(anon_page->slot);
	} else if (page->frame != NULL){
		// ASSERT();

		/** 将在pagedir kill 中回收所有页面 */
		pagedir_clear_page(page->spt->thread->pagedir, page->va);
		palloc_free_page(page->frame->kva);
		
		free(page->frame);
	}
}
