#include "swap.h"
#include "devices/block.h"
#include <bitmap.h>
#include "threads/vaddr.h"
#include "threads/synch.h"
#include "filesys/filesys.h"
/* Swap slot size */
#define SLOT_SIZE (PGSIZE)



static struct  {
  struct block *swap_table;
  struct bitmap *map;
  struct lock swap_lock;
  size_t slot_cnt;
} swap_map ;



/* Initialize the swap slot */
void
disk_swap_init (void) {
  
  /* Get the block */
  swap_map.swap_table = block_get_role (BLOCK_SWAP);

  if (swap_map.swap_table == NULL) {
    PANIC ("No swap device found");
  }
  swap_map.slot_cnt = (block_size (swap_map.swap_table) * BLOCK_SECTOR_SIZE) / SLOT_SIZE;

  /* Get the bitmap */
  swap_map.map = bitmap_create (swap_map.slot_cnt);
  if (swap_map.map == NULL) {
    PANIC ("Cannot create swap bitmap");
  }
  /* Initialize the lock */
  lock_init (&swap_map.swap_lock);
}

/* Swap in the page by reading SLOT from the swap device to KVA */
void
disk_swap_in (size_t slot, void *kva) {
  // lock_acquire (&swap_map.swap_lock);
  bool filesys_locked = is_held_filesys_lock();
  if (!filesys_locked) {
    filesys_getlock();
  }
  /* Read the slot from the swap device to KVA */
  for (size_t i = 0; i < SLOT_SIZE / BLOCK_SECTOR_SIZE; i++) {
    block_read (swap_map.swap_table, slot * (SLOT_SIZE / BLOCK_SECTOR_SIZE) + i, kva + i * BLOCK_SECTOR_SIZE);
  }
  
  /* Mark the slot as free */
  bitmap_reset (swap_map.map, slot);
  // lock_release (&swap_map.swap_lock);
  if (!filesys_locked)
      filesys_releaselock();
  
}

/* Swap out the page by writing KVA to the swap device and return the slot */
size_t
disk_swap_out (void *kva) {
  // lock_acquire (&swap_map.swap_lock);
  bool filesys_locked = is_held_filesys_lock();
  if (!filesys_locked) {
    filesys_getlock();
  }
  /* Find a free slot */
  size_t slot = bitmap_scan_and_flip (swap_map.map, 0, 1, false);
  if (slot == BITMAP_ERROR) {
    printf ("No free swap slot");
    // lock_release (&swap_map.swap_lock);
    if (!filesys_locked)
      filesys_releaselock();
    return DISK_SWAP_ERROR;
  }
  
  /* Write KVA to the slot of the swap device */
  for (size_t i = 0; i < SLOT_SIZE / BLOCK_SECTOR_SIZE; i++) {
    block_write (swap_map.swap_table, slot * (SLOT_SIZE / BLOCK_SECTOR_SIZE) + i, kva + i * BLOCK_SECTOR_SIZE);
  }
   if (!filesys_locked)
      filesys_releaselock();
  // lock_release (&swap_map.swap_lock);
  return slot;
}

void disk_swap_free(size_t slot) {
  // lock_acquire (&swap_map.swap_lock);
  bool filesys_locked = is_held_filesys_lock();
  if (!filesys_locked) {
    filesys_getlock();
  }
  bitmap_reset (swap_map.map, slot);
  // lock_release (&swap_map.swap_lock);
  if (!filesys_locked)
      filesys_releaselock();
}