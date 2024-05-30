#ifndef USERPROG_CHILD_H
#define USERPROG_CHILD_H

#include "lib/kernel/list.h"
#include "threads/thread.h"
#include "threads/synch.h"
typedef int tid_t;

#define ABNORMOUS_EXIT -1               /* Indicates a thread exits abnormously. */

/* Structure of a process's child process. */
struct child
{
  struct thread *thread;               /* Points to the corresponding thread. */
  tid_t tid;                           /* Stores the thread's tid. */
  struct semaphore sema;               /* Child's semaphore used while parent is waiting. */
  bool waited;                         /* Checks whether it has been waited. */
  bool exited;                         /* Checks whether the thread has exited. */
  int exit_status;                     /* Stores the thread's exit status. */
  struct list_elem elem;               /* Enables the child to be put in a list. */
};

#endif /* userprog/child.h */
