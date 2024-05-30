#ifndef VM_SHARING_H
#define VM_SHARING_H

#include "filesys/file.h"
#include "filesys/off_t.h"
#include "lib/kernel/hash.h"
#include "lib/kernel/list.h"
#include "lib/string.h"
#include "threads/palloc.h"
#include "threads/thread.h"
#include "threads/malloc.h"
#include "threads/vaddr.h"
#include "vm/frame-table.h"

struct executable_identity {
  struct file *file;                        /* File pointer. */
  off_t ofs;                                /* The offset. */
};

struct sharing_entry {
  struct executable_identity exec_identity; /* File pointer and offset. */
  struct list owners;                       /* List of owner threads. */
  void *kernel_page;                        /* Pointer to shared frame. */
  int shared_num;                           /* The number of times shared. */
  struct list_elem elem;                    /* List elem. */
};

void sharing_list_init (void);
struct sharing_entry *get_sharing_entry (struct file *file, off_t ofs);
void insert_sharing_entry (struct file *file, off_t ofs,
struct sharing_entry *se);
void *allocate_page_share (struct file *file, off_t ofs, void *user_address,
bool writable, bool zeroed);
void copy_on_write (void *user_address);
bool is_same_executable(struct executable_identity *a,
struct executable_identity *b);

#endif