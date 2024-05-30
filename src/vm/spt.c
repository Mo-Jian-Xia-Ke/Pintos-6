#include <stdio.h>
#include "spt.h"
#include "../threads/thread.h"
#include "../userprog/pagedir.h"
#include "../userprog/syscall.h"
#include "../lib/stddef.h"
#include "../threads/malloc.h"
#include "../lib/debug.h"
#include "../threads/vaddr.h"

bool spt_hash_less (const struct hash_elem *lhs,
					 const struct hash_elem *rhs,
					 void *aux UNUSED);
					 
unsigned spt_hash (const struct hash_elem *e,
					void *aux UNUSED);
					
void spt_destroy_frame (struct hash_elem *e,
							void *aux UNUSED);

/* Init the spt lock. */
void
spt_lock_init (void)
{
	lock_init(&spt_lock);
}

/* Create a page table and init this page table. */
spt *
spt_create (void)
{
	spt *t = malloc(sizeof(spt));

	if (t != NULL) {
		if (spt_init(t) == false) {
			free (t);
			return NULL;
		}
		else {
			return t;
		}
	}
	else {
		return NULL;
	}
}

/* Return whether page init is successful or not 
   and this function will create an initial virtual stack slot. */
bool
spt_init (spt *spt) 
{
	return hash_init (spt, spt_hash, spt_hash_less, NULL);
}

/* Destroy page_table and recycle frame and swap slot. */
void
spt_destroy (spt *spt) 
{
	lock_acquire (&spt_lock);
	hash_destroy (spt, spt_destroy_frame);
	lock_release (&spt_lock);
}

/* Find the element with vaddr = upage in supplemental page table. */
struct spte *
spt_find (spt *spt, void *upage) 
{
  ASSERT (spt != NULL);

	struct spte *spte;
	struct hash_iterator it;
	hash_first (&it, spt);
	while (hash_next (&it)) {
		spte = hash_entry (hash_cur (&it), struct spte, elem);
		if (spte->vaddr == upage) {
			return spte;
		}
	}
	return NULL;
}

/* Remove a hash_elem from spt. */
struct hash_elem *
spt_remove_entry (spt *spt, struct hash_elem *e)
{
  return hash_delete (spt, e);
}

/* Create a new spte and initialize the status to UNLOAD. */
struct spte *
new_spte (void *upage)
{
	struct thread *cur = thread_current ();
	spt *spt = cur->spt;

	lock_acquire (&spt_lock);
	
	struct spte *spte = malloc (sizeof (struct spte));
	if (spte == NULL) {
	  return NULL;
	}

	spte->vaddr = upage;
	spte->status = UNLOAD;
	hash_insert (spt, &spte->elem);
	lock_release (&spt_lock);

	return spte;
}

/* check if upage is below stack underline and available*/
bool 
spt_available_upage (spt *spt, void *upage) 
{
	return upage < PHYS_BASE && spt_find (spt, upage) == NULL;
}

/* used to check whether the two pages are the same */
bool 
spt_hash_less (const struct hash_elem *lhs, const struct hash_elem *rhs,
               void *aux UNUSED) 
{
	return hash_entry (lhs, struct spte, elem)->vaddr <
	       hash_entry (rhs, struct spte, elem)->vaddr;
}

/* hash function of element in hash table */
unsigned 
spt_hash (const struct hash_elem *e, void* aux UNUSED)
{
	struct spte *t = hash_entry (e, struct spte, elem);
	return hash_bytes (&(t->vaddr), sizeof (t->vaddr));
}

/* return FRAME and SWAP slot back */
void 
spt_destroy_frame (struct hash_elem *e, void *aux UNUSED) 
{
	struct spte *t = hash_entry (e, struct spte, elem);
	free(t);
}
