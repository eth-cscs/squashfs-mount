#include "utils.h"
#include <err.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#define exit_with_error(...)                                                   \
  do {                                                                         \
    fprintf(stderr, __VA_ARGS__);                                              \
    exit(EXIT_FAILURE);                                                        \
  } while (0)

void help(char const *argv0) {
  exit_with_error("Usage: %s <image>:<mountpoint> [<image>:<mountpoint>]...  "
                  "-- <command> [args...]\n",
                  argv0);
}

/// check if squashsfs_file is an existent file, mounpoint is an existent
/// directory
void validate_file_and_mountpoint(char const *squashfs_file,
                                  char const *mountpoint) {
  struct stat mnt_stat;
  // Check that the mount point exists.
  int mnt_status = stat(mountpoint, &mnt_stat);
  if (mnt_status)
    err(EXIT_FAILURE, "Invalid mount point \"%s\"", mountpoint);
  if (!S_ISDIR(mnt_stat.st_mode))
    errx(EXIT_FAILURE, "Invalid mount point \"%s\" is not a directory",
         mountpoint);

  // Check that the input squashfs file exists.
  int sqsh_status = stat(squashfs_file, &mnt_stat);
  if (sqsh_status)
    err(EXIT_FAILURE, "Invalid squashfs image file \"%s\"", squashfs_file);
  if (!S_ISREG(mnt_stat.st_mode))
    errx(EXIT_FAILURE, "Requested squashfs image \"%s\" is not a file",
         squashfs_file);
}

/// lexical sorting for mountpoint
int compare_mountpoint(const void *p1, const void *p2) {
  return strcmp(((const mount_entry_t *)p1)->mountpoint,
                ((const mount_entry_t *)p2)->mountpoint);
}

/// split by `:` and convert to abspath, sort by mountpoint
mount_entry_t *parse_mount_entries(char **argv, int argc) {
  // TODO `:` in argv get overwritten by `\0` in this function, is this OK?
  mount_entry_t *mount_entries = malloc(sizeof(mount_entry_t) * argc);

  for (int i = 0; i < argc; ++i) {
    char *mnt, *file;
    if (!(file = strtok(argv[i], ":")) || !(mnt = strtok(NULL, ":"))) {
      errx(EXIT_FAILURE, "invalid format %s", argv[i]);
    } else {
      if (strtok(NULL, ":")) {
        // expect file:mountpoint, strtok must return NULL when called once more
        errx(EXIT_FAILURE, "invalid format %s", argv[i]);
      }
    }

    strcpy(mount_entries[i].squashfs_file, file);
    strcpy(mount_entries[i].mountpoint, mnt);

    // convert to absolute paths (if needed)
    // absolute paths are skipped, since we allow to do nested mounts (for given
    // absolute), they won't be resolvable via realpath, since in general
    // non-existent outside the image
    if ((file[0] != '/') &&
        realpath(file, mount_entries[i].squashfs_file) == NULL) {
      errx(EXIT_FAILURE, "Failed to obtain realpath of %s, error: %s", file,
           strerror(errno));
    }
    if ((mnt[0] != '/') && realpath(mnt, mount_entries[i].mountpoint) == NULL) {
      errx(EXIT_FAILURE, "Failed to obtain realpath of %s, error: %s", mnt,
           strerror(errno));
    }
  }

  // sort by mountpoint
  qsort(mount_entries, argc, sizeof(mount_entry_t), compare_mountpoint);

  return mount_entries;
}

void do_mount_loop(const mount_entry_t *mount_entries, int n,
                   void (*do_mount)(const mount_entry_t *)) {

  // exit if there is a duplicate  in (sorted) array of mountpoints
  for (int i = 0; i < n - 1; ++i) {
    if (strcmp(mount_entries[i].mountpoint, mount_entries[i + 1].mountpoint) ==
        0) {
      errx(EXIT_FAILURE, "duplicate mountpoint: %s",
           mount_entries[i].mountpoint);
    }
  }

  // check for duplicate image -> warning
  for (int i = 0; i < n; ++i) {
    for (int j = i + 1; j < n; ++j) {
      if (strcmp(mount_entries[i].squashfs_file,
                 mount_entries[j].squashfs_file) == 0) {
        fprintf(stderr, "WARNING: duplicate image: %s\n",
                mount_entries[i].squashfs_file);
      }
    }
  }

  for (int i = 0; i < n; ++i) {
    do_mount(mount_entries + i);
  }
}
