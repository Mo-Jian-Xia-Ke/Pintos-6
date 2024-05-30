#include "../devices/swap.h"
#include "../threads/palloc.h"
#include "../threads/loader.h"
#include "../threads/vaddr.h"
#include "../threads/synch.h"
#include "../userprog/pagedir.h"
#include "../lib/debug.h"
#include "../lib/kernel/bitmap.h"
#include "../threads/pte.h"
#include "frame-table.h"
#include "filesys/filesys.h"
#include "filesys/file.h"

struct FrameTable {
  void *user_pool_base;
  uint32_t user_pool_page_count;
  struct Frame *frames;         /* A list of frames. */
} frame_table;

struct lock eviction_lock;
struct lock frame_table_lock;

int get_user_frame_number (void *kernel_page);

/* Init the frame table based on user_pool_base and user_pool_page_count. */
void frame_table_init (void *user_pool_base, uint32_t user_pool_page_count)
{
  frame_table.user_pool_base = user_pool_base;
  frame_table.user_pool_page_count = user_pool_page_count;

  frame_table.frames = palloc_get_multiple (PAL_ZERO, 
                       user_pool_page_count * sizeof(Frame) / PGSIZE + 1);

  if (frame_table.frames == NULL)
    PANIC ("frame_table_init: Cannot allocate memory for %i frame tables", init_ram_pages);

  lock_init (&eviction_lock);
  lock_init (&frame_table_lock);
}

/* Get user_frame_number. */
int get_user_frame_number (void *kernel_page)
{
  return ((char *) kernel_page - (char *) frame_table.user_pool_base) >> PGBITS;
}

/* Evict a frame based on clock algorithm. */
static uint32_t choose_frame_to_evict (void)
{
  // Using clock algorithm
  static uint32_t hand = 0;
  while (true) {
    if (frame_table.frames[hand].r) {
      // Give a chance to frame with R = 1
      frame_table.frames[hand].r = false;
      // Hand returns to the beginning when it reaches the end
      hand += 1;
      if (hand == frame_table.user_pool_page_count)
        hand = 0;
    } else {
      // Evict this frame if we meet it twice.
      // Next time, we must see the new allocated frame at the first glance.
      // Because it's new (with R = 1), so we will not evict it.
      uint32_t evict_frame_no = hand;
      // Hand returns to the beginning when it reaches the end
      hand += 1;
      if (hand == frame_table.user_pool_page_count)
        hand = 0;
      return evict_frame_no;
    }
  }
}

/* Allocate a kernel page for a user page. */
void *allocate_user_page (void *user_address, bool writable, bool zeroed)
{
  void *kernel_page = palloc_get_page (PAL_USER | (zeroed ? PAL_ZERO : 0));
  if (kernel_page == NULL) {
    lock_acquire (&eviction_lock);

    // User pool is full, so choose a frame to evict
    lock_acquire (&frame_table_lock);
    uint32_t evict_frame_no = choose_frame_to_evict ();
    struct thread *owner = frame_table.frames[evict_frame_no].owner;
    void *user_page = frame_table.frames[evict_frame_no].user_page;
    lock_release (&frame_table_lock);

    void *frame = pagedir_get_page (owner->pagedir, user_page);

    struct spte *spte = spt_find (owner->spt, user_page);
    if (spte->status == MMAP) {
      /* Evicting a page mapped by mmap writes it back to the file it was
      mapped from */
      if (pagedir_is_dirty (owner->pagedir, user_page)) {
        lock_acquire (&filesys_lock);
        file_seek (spte->file, spte->file_ofs);
        file_write (spte->file, spte->value, spte->bytes_read);
        lock_release (&filesys_lock);
      }
      pagedir_clear_page (owner->pagedir, user_page);
      palloc_free_page (frame);

      /* Set value to NULL to detect that the frame has been evicted */
      spte->value = NULL;
    } else {
      // Try to write this frame to swap
      pagedir_clear_page (owner->pagedir, user_page);
      size_t swap_slot = swap_out (frame);
      if (swap_slot == BITMAP_ERROR) {
        PANIC ("allocate_user_page: User pool is full. \
                Swap is full. Cannot allocate more pages.");
        return NULL;
      }
      spte->status = SWAP;
      spte->value = (void *) swap_slot;

      // Evict this frame from RAM
      palloc_free_page (frame);

    }

    // Now there must be a free frame
    kernel_page = palloc_get_page (PAL_USER | (zeroed ? PAL_ZERO : 0));
    ASSERT (kernel_page != NULL);
    lock_release (&eviction_lock);
  }

  struct thread *thread = thread_current ();

  /* Verify that there's not already a page at that virtual
     address, then map our page there. */
  if (pagedir_get_page (thread->pagedir, user_address) != NULL) {
    palloc_free_page (kernel_page);
    return NULL;
  }

  if (!pagedir_set_page (thread->pagedir, 
       user_address, kernel_page, writable)) {
    palloc_free_page (kernel_page);
    return NULL;
  }

  int frame_number = get_user_frame_number (kernel_page);

  lock_acquire (&frame_table_lock);
  frame_table.frames[frame_number].owner = thread;
  frame_table.frames[frame_number].user_page = user_address;
  frame_table.frames[frame_number].r = true;
  lock_release (&frame_table_lock);

  return kernel_page;
}

/* Frees all user pages. */
void free_all_user_pages (struct thread *thread)
{
  uint32_t *page_directory = thread->pagedir;

  for (uint32_t i = 0; i < frame_table.user_pool_page_count; i++) {
    if (frame_table.frames[i].owner == thread) {
      frame_table.frames[i].owner = NULL;
      frame_table.frames[i].user_page = NULL;
    }
  }

  pagedir_destroy (page_directory);
}
