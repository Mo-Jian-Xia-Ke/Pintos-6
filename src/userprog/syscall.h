#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

#include <stdbool.h>

typedef int pid_t;
#define PID_ERROR ((pid_t) -1)
#define MAX_ARGUMENT_NUMBER 3
#define EOF (-1)

void syscall_init (void);
void exit (int status);
int open (const char *);
struct semaphore exec_sema;
bool exec_load_success;

#endif /* userprog/syscall.h */
