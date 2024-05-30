#ifndef VM_PAGE_H
#define VM_PAGE_H

#include "../lib/stdint.h"
#include "../lib/kernel/hash.h"
#include "../threads/palloc.h"
#include "../threads/thread.h"
#include "../threads/synch.h"
#include "../vm/sharing.h"
typedef struct hash spt;

struct lock spt_lock;

enum page_status {
	FRAME,
	SWAP,
	FILE,
	UNLOAD,
  MMAP
};

/* Record necessary information of supplemental page table,
   key is the virtual address of page, value is the physical address 
   of frame if status = FRAME, or index of swap slot if status = SWAP,
   or mapid of mapped file if status = FILE. 
   File stores a file pointer if the status is FILE, read_bytes stores
   the number of bytes we will read.
   Writable indicates whether pages from the filesystem are allowed 
   to be written back to the filesystem or not.
	*/
struct spte {
  void *vaddr;
  void *value;
  int id;
  struct file *file;
  size_t read_bytes;        /* Total bytes should be read into this page. */
  size_t bytes_read;        /* Actual bytes that are loaded. */
  size_t file_ofs;
  enum page_status status;
  bool writable;
  bool is_shared;           /* If this page is shared. */
  struct sharing_entry *se; /* Corresponding sharing entry. */
  struct hash_elem elem;
};

void spt_lock_init (void);
spt *spt_create (void);
bool spt_init (spt *spt);
void spt_destroy (spt *spt);
struct spte *spt_find (spt *spt, void *upage);
struct hash_elem *spt_remove_entry (spt *spt, struct hash_elem *e);
struct spte *new_spte (void *upage);
bool spt_available_upage (spt *spt, void *upage);

#endif