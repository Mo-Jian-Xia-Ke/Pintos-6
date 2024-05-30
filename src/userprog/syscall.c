#include <stdio.h>
#include <syscall-nr.h>
#include "threads/malloc.h"
#include "../userprog/syscall.h"
#include "../lib/user/syscall.h"
#include "../threads/interrupt.h"
#include "../threads/thread.h"
#include "../threads/vaddr.h"
#include "../threads/synch.h"
#include "../filesys/file.h"
#include "../filesys/filesys.h"
#include "../devices/shutdown.h"
#include "../devices/input.h"
#include "pagedir.h"
#include "argument-parsing.h"
#include "process.h"
#include "vm/frame-table.h"

int *arg[MAX_ARGUMENT_NUMBER];

static void syscall_handler (struct intr_frame *);
static void *accessUserMemory (uint32_t *, const void *);
struct semaphore exec_sema;
bool exec_load_success;

/* Registers internal interrupt 0x30 to invoke syscall_handler. */
void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
  sema_init (&exec_sema, 0);
}

static void
get_argument (struct intr_frame *f, int number)
{
  for (int i = 0; i < number; i++) {
    arg[i] = (int *) accessUserMemory (
      thread_current ()->pagedir, ((int *) (f->esp)) + i + 1);
  }
}

/* Returns the pointer to the file associated with the fd. */
static struct file *
get_file_with_fd (int fd)
{
  if (0 <= fd && fd < MAX_OPEN_FILE) {
    return thread_current ()->fd_table[fd];
  } else {
    return NULL;
  }
}

/* Terminates Pintos by calling shutdown_power_off (). */
void
halt (void)
{
  shutdown_power_off ();
}

/* Terminates the current user program, 
   sending its exit status to the kernel. */
void
exit (int status)
{
  struct thread *cur = thread_current ();
  printf ("%s: exit(%d)\n", cur->name, status);
  if (cur->child != NULL) {
    cur->child->exit_status = status;
  }

  for (int fd = 0; fd < MAX_OPEN_FILE; fd++) {
    close (fd);
  }

  thread_exit ();
}

/* Runs the executable whose name is given in cmd line. */
pid_t
exec (const char *file)
{
  /* If a null file is passed in, return a -1. */
	if (file == NULL) {
		return -1;
	}

  char filename[16];
  copy_name_from ((char *) file, filename);

  int fd = open (filename);
  if (fd == -1) {
    return -1;
  }
  close (fd);

  /* Get and return the PID of the process that is created. */
	pid_t child_tid = process_execute (file);
  sema_down (&exec_sema);
  if (!exec_load_success) {
    return -1;
  }

	return child_tid;
}

/* Creates a file of given name and size and return true
   if the operation succeed. */
bool
create (const char *file, unsigned initial_size)
{
  lock_acquire (&filesys_lock);
  bool file_status = filesys_create (file, initial_size);
  lock_release (&filesys_lock);
  return file_status;
}

/* Remove the file from the file system, and return true 
   if the operation succeeds. */
bool
remove (const char *file)
{
  lock_acquire (&filesys_lock);
  bool removed = filesys_remove (file);
  lock_release (&filesys_lock);
  return removed;
}

/* Writes size bytes from buffer to the open file fd.
   Returns the number of bytes actually written. 
   Fd 1 writes to the console. */
int
write (int fd, const void *buffer, unsigned size) {
  if (fd == 1) {
    putbuf (buffer, size);
    return size;
  } else {
    struct file *file = get_file_with_fd (fd);
    if (file != NULL) {
      int bytes_written = file_write (file, buffer, size);
      return bytes_written;
    } else {
      return -1;
    }
  }
}

/* Reads size bytes from the file open as fd into buffer. Returns the number of
   bytes actually read (0 at end of file), or -1 if the file could not be read.
   Fd 0 reads from the keyboard. */
int
read (int fd, void *buffer, unsigned length)
{
  if (fd == 0) {
    unsigned int total = 0;
    char *pos = (char *) buffer;
    while (total < length) {
      char c = input_getc();
      if (c == '\0' || c == EOF) {
        break;
      }
      *pos++ = c;
      total++;
    }
    *pos = '\0';
    return total;
  } else {
    struct file *file = get_file_with_fd (fd);
    if (file != NULL) {
      int bytes_read = file_read (file, (void *) buffer, length);
      return bytes_read;
    } else {
      return -1;
    }
  }
}

/* Waits for a child process pid and retrieves the child’s exit status. */
int
wait (pid_t pid)
{
  return process_wait (pid);
}

/* Opens the file called file. Returns a nonnegative file descriptor,
   or -1 if the file could not be opened. */
int
open (const char *file)
{
  lock_acquire (&filesys_lock);
  struct file *file_ptr = filesys_open (file);
  lock_release (&filesys_lock);

  if (file_ptr != NULL) {
    struct file **fd_table = thread_current ()->fd_table;
    for (int fd = 2; fd < MAX_OPEN_FILE; fd++) {
      if (fd_table[fd] == NULL) {
        fd_table[fd] = file_ptr;
        return fd;
      }
    }
  }

  return -1;
}

/* Closes file descriptor fd. */
void
close (int fd)
{
  if (0 <= fd && fd < MAX_OPEN_FILE) {
    struct file *file_ptr = get_file_with_fd (fd);

    struct hash_iterator it;
	  hash_first (&it, &thread_current ()->mmapped_file_table);
	  while (hash_next (&it)) {
		struct mmapped_file *mmapped_file = hash_entry (hash_cur (&it),
                                                    struct mmapped_file, elem);
		if (mmapped_file->file == file_ptr) {
			return;
		}
	}
    file_close (file_ptr);

    thread_current ()->fd_table[fd] = NULL;
  }
}

/* Returns the size, in bytes, of the file open as fd.
   Returns -1 if fd is invalid. */
int
filesize (int fd)
{
  if (0 <= fd && fd < MAX_OPEN_FILE) {
    struct file *file_ptr = get_file_with_fd (fd);

    if (file_ptr != NULL) {
      int size = file_length (file_ptr);

      return size;
    }
  }

  return -1;
}

/* Changes the next byte to be read or written in open file fd to position,
   expressed in bytes from the beginning of the file. */
void
seek (int fd, unsigned position)
{
  if (0 <= fd && fd < MAX_OPEN_FILE) {
    struct file *file_ptr = get_file_with_fd (fd);

    if (file_ptr != NULL) {
      file_seek (file_ptr, position);
    }
  }
}

/* Returns the position of the next byte to be read or written in open file fd,
   expressed in bytes from the beginning of the file. Returns (unsigned int)
   (-1) if fd is invalid. */
unsigned
tell (int fd)
{
  if (0 <= fd && fd < MAX_OPEN_FILE) {
    struct file *file_ptr = get_file_with_fd (fd);

    if (file_ptr != NULL) {
      unsigned pos = file_tell (file_ptr);

      return pos;
    }
  }

  return -1;
}

mapid_t
mmap (int fd, void *addr)
{
  /* Address must be page-aligned */
  if (addr == NULL || pg_ofs (addr) != 0) {
    return MAP_FAILED;
  }

  /* Cannot map console input or output */
  if (fd == 0 || fd == 1) {
    return MAP_FAILED;
  }

  /* File must not be empty */
  struct file *file = get_file_with_fd (fd);
  if (file == NULL || file_length (file) == 0) {
    return MAP_FAILED;
  }

  int length = file_length (file);
  struct thread *cur = thread_current ();

  /* Checks that the range of pages mapped 
     does not overlap any existing segment */
  int check_bytes = 0;
  void *check_addr = addr;
  while (check_bytes < length) {
    struct spte *spte = spt_find (cur->spt, check_addr);
    if (spte != NULL) {
      return MAP_FAILED;
    }
    check_bytes += PGSIZE;
    check_addr += PGSIZE;
  }

  int bytes_mapped = 0;
  void *start_addr = addr;
  while (bytes_mapped < length) {

    /* Create the page mapping in the supplemental page table */
    struct spte *spte = new_spte (addr);
    lock_acquire (&spt_lock);
    spte->file = file;
    spte->status = MMAP;
    spte->file_ofs = bytes_mapped;
    spte->read_bytes = PGSIZE;
    lock_release (&spt_lock);

    addr += PGSIZE;
    bytes_mapped += PGSIZE;
  }

  /* Add the file mapping to process's table */
  struct mmapped_file *mmapped_file = (struct mmapped_file *) 
                                      malloc (sizeof (struct mmapped_file));
  mmapped_file->mapping_id = cur->next_mapid++;
  mmapped_file->file = file;
  mmapped_file->addr = start_addr;
  hash_insert (&cur->mmapped_file_table, &mmapped_file->elem);

  return mmapped_file->mapping_id;
}

void
munmap (mapid_t map_id)
{
  /* Find the file mapping using map_id in process's table */
  struct thread *cur = thread_current ();
  struct mmapped_file toFind;
  toFind.mapping_id = map_id;
  struct hash_elem *e = hash_find (&cur->mmapped_file_table, &toFind.elem);

  if (e == NULL) {
    return;
  }

  struct mmapped_file *mmapped_file = 
    hash_entry (e, struct mmapped_file, elem);

  struct file *file = mmapped_file->file;
  void *addr = mmapped_file->addr;
  int length = file_length (file);
  int current_byte = 0;

  /* Iterate over the pages of the mapped file */
  while (current_byte < length) {
    struct spte *spte = spt_find (cur->spt, addr);
    ASSERT (spte != NULL);
    ASSERT (spte->status == MMAP);
    ASSERT (spte->file == file);

    if (spte->value != NULL) {
      /* Still in frame, haven't been evicted */
      /* Write back to the file if the mapped page is dirty */
      if (pagedir_is_dirty (cur->pagedir, addr)) {
        lock_acquire (&filesys_lock);
        file_seek (file, spte->file_ofs);
        file_write (file, spte->value, spte->bytes_read);
        lock_release (&filesys_lock);
      }
      
      /* Free resources */
      palloc_free_page (pagedir_get_page (cur->pagedir, addr));
      pagedir_clear_page (cur->pagedir, addr);
    }
    spt_remove_entry (cur->spt, &spte->elem);
    free (spte);

    current_byte += PGSIZE;
    addr += PGSIZE;
  }

  /* Remove the mapping from the process's table */
  hash_delete (&cur->mmapped_file_table, &mmapped_file->elem);
  free (mmapped_file);
}

/* Accesses the system call number from the user stack and perform actions
   based on the system call number. */
static void
syscall_handler (struct intr_frame *f)
{
  int *syscall_num = (int *) accessUserMemory (
    thread_current ()->pagedir, f->esp);
  thread_current ()->esp = f->esp;
  switch (*syscall_num) {
    case SYS_HALT:
      halt ();
      break;
    case SYS_EXIT:
      get_argument (f, 1);
      exit (*arg[0]);
      break;
    case SYS_EXEC:
      get_argument (f, 1);
			f->eax = exec ((const char *) accessUserMemory (
                                            thread_current ()->pagedir,
                                            (const void *) *arg[0]
                                          ));
			break;
    case SYS_WAIT:
      get_argument (f, 1);
      f->eax = wait (*arg[0]);
      break;
    case SYS_CREATE:
      get_argument (f, 2);
      f->eax = create((const char *) accessUserMemory (
                                              thread_current ()->pagedir,
                                              (const void *) *arg[0]
                                            ),
                      (unsigned) *arg[1]);
      break;
    case SYS_REMOVE:
      get_argument (f, 1);
      f->eax = remove((const char *) accessUserMemory (
                                              thread_current ()->pagedir,
                                              (const void *) *arg[0]
                                            ));
			break;
    case SYS_OPEN:
      get_argument (f, 1);
      f->eax = open ((const char *) accessUserMemory (
                                              thread_current ()->pagedir,
                                              (const void *) *arg[0]
                                            ));
      break;
    case SYS_FILESIZE:
      get_argument (f, 1);
      f->eax = filesize (*arg[0]);
      break;
    case SYS_READ:
      get_argument (f, 3);
      f->eax = read (
                     *arg[0],
                     accessUserMemory (
                                thread_current ()->pagedir,
                                (const void *) *arg[1]
                           ),
                     *arg[2]
                  );
      break;
    case SYS_WRITE:
      get_argument (f, 3);
      f->eax = write (
                  *arg[0],
                  accessUserMemory (
                            thread_current ()->pagedir,
                            (const void *) *arg[1]
                          ),
                  *arg[2]
                );
      break;
    case SYS_SEEK:
      get_argument (f, 2);
      seek (*arg[0], (unsigned) *arg[1]);
      break;
    case SYS_TELL:
      get_argument (f, 1);
      f->eax = tell (*arg[0]);
      break;
    case SYS_CLOSE:
      get_argument (f, 1);
      close (*arg[0]);
      break;

    /* The rest are not implemented for task 2 */
    case SYS_MMAP:
      get_argument (f, 2);
      f->eax = mmap (
                  *arg[0],
                  (void *) *arg[1]
                    );
      break;
    case SYS_MUNMAP:
      get_argument (f, 1);
      munmap (*arg[0]);
      break;
    case SYS_CHDIR:
      break;
    case SYS_MKDIR:
      break;
    case SYS_READDIR:
      break;
    case SYS_ISDIR:
      break;
    case SYS_INUMBER:
      break;
    default:
      exit (-1);
  }
}

/* Checks to ensure that the user virtual address is valid and returns the
   kernel virtual address corresponding to that user virtual address. If the
   user virtual address is invalid, the process will be terminated and its
   resources will be freed. */
static void *
accessUserMemory (uint32_t *pd, const void *uaddr)
{
  void *kernel_vaddr;
  if (uaddr == NULL || !is_user_vaddr (uaddr)
      || (kernel_vaddr = pagedir_get_page (pd, uaddr)) == NULL) {
    
    exit (-1);
    return NULL;
  } else {
    return kernel_vaddr;
  }
}
