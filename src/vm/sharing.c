#include <stdio.h>
#include "sharing.h"

static struct list sharing_list;

/* Init our sharing list. */
void
sharing_list_init (void) {
  list_init(&sharing_list);
}

/* Return the sharing entry based on file pointer and offset. 
   Return NULL if it has not been shared. */
struct sharing_entry *
get_sharing_entry (struct file *file, off_t ofs) {
  struct executable_identity temp_exec_id;
  temp_exec_id.file = file;
  temp_exec_id.ofs = ofs;
  struct sharing_entry *se;
  struct list_elem *e;

  for (e = list_begin (&sharing_list); e != list_end (&sharing_list);
  e = list_next (e)) {
    se = list_entry (e, struct sharing_entry, elem);
    if (is_same_executable (&se->exec_identity, &temp_exec_id)) {
      return se;
    }
  }
    return NULL;
}

/* Insert a new sharing entry into sharing list and init its owners list. */
void
insert_sharing_entry (struct file *file, off_t ofs, struct sharing_entry *se) {
  se->exec_identity.file = file;
  se->exec_identity.ofs = ofs;
  se->shared_num = 1;
  list_init (&se->owners);
  list_push_back (&sharing_list, &se->elem);
}

/* Allocate a new frame if the page is read-only and has not been shared.
   Add current thread to one sharing entry's owners list and 
   increment shared_num if the frame has been shared.
   If the page is writable, simply return a new frame. */
void *
allocate_page_share (struct file *file, off_t ofs, void *user_address,
bool writable, bool zeroed)
{
  struct sharing_entry *se = get_sharing_entry (file, ofs);
  void *kernel_page;
  struct thread *thread = thread_current ();

  if (!writable) {
    // If it is a read-only file.
    if (se == NULL) {
      //If it has not been shared before.
      kernel_page = allocate_user_page (user_address, writable, zeroed);
      se = malloc (sizeof (struct sharing_entry));
      se->kernel_page = kernel_page;
      insert_sharing_entry (file, ofs, se);
      list_push_back (&se->owners, &thread->share_elem);
    } else {
        //If it has been shared before.
        kernel_page = se->kernel_page;
        list_push_back (&se->owners, &thread->share_elem);
        se->shared_num++;
    }
    struct spte *e = spt_find (thread_current()->spt, user_address);

    ASSERT (e != NULL);
    if (e != NULL) {
      e->is_shared = true;
      e->se = se;
      list_push_back (&se->owners, &thread->share_elem);
    }
  } else {
    // If it is a writable file.
    kernel_page = allocate_user_page (user_address, writable, zeroed);
  }

  return kernel_page;
}


/* Implement copy_on write. Assert that the page needs to be writable 
   and !pagedir_is_writable(t->pagedir, upage). If this frame is only shared by
   current thread, then we simply remove it from sharing entry and 
   free this sharing entry. Otherwise, we allocate a new frame to it and 
   copy the content to the new frame, then remove current thread 
   from owners list and decrement shared_num.  */
void
copy_on_write (void *user_address) 
{
  struct thread *thread = thread_current ();
  struct spte *e = spt_find (thread_current()->spt, user_address);
  struct sharing_entry *se = e->se;


  ASSERT (e != NULL);

  e->is_shared = false;
  e->se = NULL; 

  if (se->shared_num > 1)
  {
    uint8_t *new_frame = allocate_user_page (user_address, true, false);
    memcpy(new_frame, e->value, PGSIZE);
    e->value = new_frame;
    se->shared_num--;
    list_remove (&thread->share_elem);
  } else {
    list_remove (&se->elem);
    free (se);
  } 
}

/* Check if a and b are the same executable frame based on file pointer 
   and offset. */
bool
is_same_executable(struct executable_identity *a,
struct executable_identity *b) {
  return (a->file == b->file) && (a->ofs == b->ofs);
}
