#include <stddef.h>
#include <stdbool.h>
#include "lib/string.h"
#include "argument-parsing.h"
#include <debug.h>

#define move_esp_down(n)  esp = (void *) ((char *) esp - (n))
#define for_argc_0        for (int i = ap->argc - 1; i >= 0; i--)

/* Copy the name from command line to the destination. */
void
copy_name_from (char *command, char *destination)
{
  while (*command == ' ') {
    command++;
  }

  for (int i = 0;
       i < MAX_FILE_NAME_LEN && *command != 0 && *command != ' ';
       i++) {
    *destination = *command;
    command++;
    destination++;
  }
  *destination = 0;
}

/* Parse the command and store it to the ap(argument passing) struct. */
bool
parse_arguments_from (char *command, struct ap* ap)
{
  ap->argc = 0;
  char *arg_left = command;
  char *arg_right;

  while (true) {
    // Skip the spaces
    while (*arg_left == ' ') {
      arg_left++;
    }
    // Stop if reach the end of the command
    if (*arg_left == 0) {
      break;
    }
    // Find the end of this argument
    arg_right = arg_left + 1;
    while (*arg_right != ' ' && *arg_right != 0) {
      arg_right++;
    }

    // This argument is too long
    if (arg_right - arg_left >= MAX_ARG_LEN) {
      return false;
    }

    // Too many arguments
    if (ap->argc >= MAX_ARG_NUM) {
      return false;
    }

    // Copy this argument to argv
    ap->arg_length[ap->argc] = arg_right - arg_left;
    memcpy (ap->argv[ap->argc], arg_left, ap->arg_length[ap->argc]);
    ap->argv[ap->argc][ap->arg_length[ap->argc]] = 0;

    ap->argc++;
    arg_left = arg_right;
  }

  return true;
}

/* Check if the size of the argument is acceptable or not. */
bool
argument_size_is_acceptable (struct ap *ap)
{
  long rough_size = 0;

  for (int i = 0; i < ap->argc; i++) {
    rough_size += ap->arg_length[i];
  }

  rough_size += (ap->argc + 3) * (long) sizeof(void *);

  return rough_size <= MAX_ARG_LEN;
}

/* Place the arguments from struct ap. */
void *
place_arguments_at (void *esp, struct ap *ap)
{
  // Push string literals
  for_argc_0 {
    move_esp_down(ap->arg_length[i] + 1);
    memcpy (esp, ap->argv[i], ap->arg_length[i] + 1);
    ASSERT (esp != NULL);
    ap->arg_address[i] = esp;
  }

  // Align
  unsigned long word_align = (unsigned long) esp % 4;
  move_esp_down(word_align);

  // Push sentinel null pointer
  move_esp_down(sizeof (void *));
  *(void **) esp = NULL;

  // Push pointers to string literals
  for_argc_0 {
    move_esp_down(sizeof (void *));
    memcpy (esp, &ap->arg_address[i], sizeof (void *));
  }

  // Push a pointer to the first pointer to the first string literal
  move_esp_down(sizeof (void *));
  *(void **) esp = (void *) ((char *) esp + sizeof (void *));

  // Push the number of arguments
  move_esp_down(sizeof (int));
  *(int *) esp = ap->argc;

  // Push a fake return address
  move_esp_down(sizeof (void *));
  *(void **) esp = NULL;

  return esp;
}

#undef move_esp_down
#undef for_argc_0
#undef null_if_over_4kb
