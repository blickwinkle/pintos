/* file.c: Implementation of memory backed file object (mmaped object). */

#include "vm/vm.h"
#include "threads/vaddr.h"
#include "filesys/filesys.h"
#include "userprog/pagedir.h"

static bool file_backed_swap_in (struct page *page, void *kva);
static bool file_backed_swap_out (struct page *page);
static void file_backed_destroy (struct page *page);

/* DO NOT MODIFY this struct */
static const struct page_operations file_ops = {
	.swap_in = file_backed_swap_in,
	.swap_out = file_backed_swap_out,
	.destroy = file_backed_destroy,
	.type = VM_FILE,
};

/* The initializer of file vm */
void
vm_file_init (void) {
}

/* Initialize the file backed page */
bool
file_backed_initializer (struct page *page, enum vm_type type, void *kva) {
	/* Set up the handler */
	page->operations = &file_ops;

	struct uninit_page *uninit = &page->uninit;
	struct mmap_file *mmap = uninit->aux;
	struct file_page *file_page = &page->file;
	file_page->mmap = mmap;

	return file_backed_swap_in(page, kva);
}

/* Swap in the page by read contents from the file. */
static bool
file_backed_swap_in (struct page *page, void *kva) {
	struct file_page *file_page UNUSED = &page->file;
	struct mmap_file *mmap = file_page->mmap;
	/* Set up the file info */
	int start_off = mmap->offset + ((uint8_t *)page->va - (uint8_t *)mmap->start);
	int read_len = PGSIZE;
	if (start_off + PGSIZE > mmap->offset + mmap->len) {
		read_len = mmap->offset + mmap->len - start_off;
	}
	if (read_len <= 0) {
		PANIC("read_len is less than 0");
	}
	off_t ofs = start_off;
	bool filesys_locked = is_held_filesys_lock();
	if (!filesys_locked) {
		filesys_getlock();
	}
	file_seek(mmap->file, ofs);
	if (file_read(mmap->file, kva, read_len) != (int)read_len) {
		if (!filesys_locked) {
			filesys_releaselock();
		}
		return false;
	}
	if (!filesys_locked) {
		filesys_releaselock();
	}
	memset(kva + read_len, 0, PGSIZE - read_len);
	
	pagedir_set_dirty(page->spt->thread->pagedir, page->va, false);

	return true;
}

/* Swap out the page by writeback contents to the file. */
static bool
file_backed_swap_out (struct page *page) {
	struct file_page *file_page UNUSED = &page->file;
	/* If Page is undirty, need not write back to file*/
	if (pagedir_is_dirty(page->spt->thread->pagedir, page->va) == false) {
		return true;
	}
	struct mmap_file *mmap = file_page->mmap;
	/* Set up the file info */
	int start_off = mmap->offset + ((uint8_t *)page->va - (uint8_t *)mmap->start);
	int write_len = PGSIZE;
	if (start_off + PGSIZE > mmap->offset + mmap->len) {
		write_len = mmap->offset + mmap->len - start_off;
	}
	if (write_len <= 0) {
		PANIC("write_len is less than 0");
	}
	off_t ofs = start_off;
	bool filesys_locked = is_held_filesys_lock();
	if (!filesys_locked) {
		filesys_getlock();
	}
	
	file_seek(mmap->file, ofs);
	if (file_write(mmap->file, page->frame->kva, write_len) != (int)write_len) {
		if (!filesys_locked) {
			filesys_releaselock();
		}
		return false;
	}

	if (!filesys_locked) {
		filesys_releaselock();
	}
	return true;
}

/* Destory the file backed page. PAGE will be freed by the caller. */
static void
file_backed_destroy (struct page *page) {
	struct file_page *file_page UNUSED = &page->file;
	if (page->frame == NULL) {
		// return true;
		return ;
	}

	file_backed_swap_out(page);

	pagedir_clear_page(page->spt->thread->pagedir, page->va);
	palloc_free_page(page->frame->kva);
	free(page->frame);
	return ;
}

static int
allocate_mmapid (void) {
	static int next_mmapid = 0;
	return next_mmapid++;
}

/* Do the mmap */
int
do_mmap (void *addr, size_t length, int writable,
		struct file *file, off_t offset) {
	if (addr != pg_round_down(addr) || length == 0 || addr == 0) {
		return -1;
	}
	struct supplemental_page_table *spt = &thread_current()->spt;
	/* Check if the addr is already mapped */
	void *end = addr + length;

	if (is_user_vaddr(addr) == false || is_user_vaddr(addr + length - 1) == false) {
		return -1;
	}

	for (void *i = addr; i < end; i += PGSIZE) {
		if (spt_find_page(spt, i) != NULL) {
			return -1;
		}
	}
	/* Create a new mmapid */
	int mmapid = allocate_mmapid();

	/* Create a new file */
	bool filesys_locked = is_held_filesys_lock();
	if (!filesys_locked) {
		filesys_getlock();
	}
	struct file *new_file = file_reopen(file);
	if (!filesys_locked) {
		filesys_releaselock();
	}
	
	if (new_file == NULL) {
		return -1;
	}
	/* Create a new mmap file */
	struct mmap_file *mmap_file = malloc(sizeof(struct mmap_file));
	if (mmap_file == NULL) {
		bool filesys_locked = is_held_filesys_lock();
		if (!filesys_locked) {
			filesys_getlock();
		}
		file_close(new_file);
		if (!filesys_locked) {
			filesys_releaselock();
		}

		return -1;
	}
	mmap_file->file = new_file;
	mmap_file->mapid = mmapid;
	mmap_file->start = addr;
	mmap_file->writable = writable;
	mmap_file->offset = offset;
	mmap_file->len = length;
	list_init(&mmap_file->pages);
	
	/* Add the mmap file to the list */
	list_push_back(&spt->mmap_table, &mmap_file->elem);

	/* Map the file to the pages */
	for (void *i = addr; i < end; i += PGSIZE) {
		struct page *page = malloc(sizeof(struct page));
		
		if (page == NULL) {
			do_munmap(mmapid);
			return -1;
		}
		uninit_new(page, i, NULL, VM_FILE, mmap_file, writable, spt, file_backed_initializer);
		if ( !spt_insert_page (spt, page)) {
			free(page);
			do_munmap(mmapid);
			return -1;
		}
		list_push_back(&mmap_file->pages, &page->mmap_elem);
	}
	return mmapid;
	
}

/* Do the munmap */
void
do_munmap (int mmapid) {
	struct supplemental_page_table *spt = &thread_current()->spt;
	struct mmap_file *mmap_file = NULL;
	struct list_elem *e;
	for (e = list_begin(&spt->mmap_table); e != list_end(&spt->mmap_table); e = list_next(e)) {
		struct mmap_file *mmap = list_entry(e, struct mmap_file, elem);
		if (mmap->mapid == mmapid) {
			mmap_file = mmap;
			break;
		}
	}
	if (mmap_file == NULL) {
		return;
	}
	/* Remove the mmap file from the list */
	list_remove(&mmap_file->elem);
	/* Unmap the file from the pages */
	struct list_elem *e2;

	lock_acquire(&spt->lock);
	for (e2 = list_begin(&mmap_file->pages); e2 != list_end(&mmap_file->pages); ) {
		struct page *page = list_entry(e2, struct page, mmap_elem);
		e2 = list_next(e2);
		spt_remove_page(spt, page);
	}
	lock_release(&spt->lock);

	/* Close the file */
	bool filesys_locked = is_held_filesys_lock();
	if (!filesys_locked) {
		filesys_getlock();
	}
	file_close(mmap_file->file);
	if (!filesys_locked) {
		filesys_releaselock();
	}
	free(mmap_file);
}
