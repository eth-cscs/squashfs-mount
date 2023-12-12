#pragma once

#define _GNU_SOURCE
#include <limits.h>
#include <unistd.h>

#define exit_with_error(...)                                                   \
  do {                                                                         \
    fprintf(stderr, __VA_ARGS__);                                              \
    exit(EXIT_FAILURE);                                                        \
  } while (0)

typedef struct {
  char squashfs_file[PATH_MAX];
  char mountpoint[PATH_MAX];
} mount_entry_t;

void help(char const *argv0);

void validate_file_and_mountpoint(char const *squashfs_file,
                                  char const *mountpoint);

int compare_mountpoint(const void *p1, const void *p2);

mount_entry_t *parse_mount_entries(char **argv, int argc);

void do_mount_loop(const mount_entry_t *mount_entries, int n,
                   void (*do_mount)(const mount_entry_t *));
