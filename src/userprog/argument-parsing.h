#ifndef PINTOS_6_ARGUMENT_PARSING_H
#define PINTOS_6_ARGUMENT_PARSING_H

#include <stdbool.h>

#define MAX_ARG_NUM 128
#define MAX_ARG_LEN 1024
#define MAX_FILE_NAME_LEN 14

/* The data structure for our argument passing. */
struct ap {
  int argc;
  char argv[MAX_ARG_NUM][MAX_ARG_LEN];
  long arg_length[MAX_ARG_NUM];
  char *arg_address[MAX_ARG_NUM];
};

void copy_name_from(char *command, char *destination);

bool parse_arguments_from(char *command, struct ap *ap);

void *place_arguments_at(void *esp, struct ap *ap);

bool argument_size_is_acceptable (struct ap *ap);

#endif //PINTOS_6_ARGUMENT_PARSING_H
