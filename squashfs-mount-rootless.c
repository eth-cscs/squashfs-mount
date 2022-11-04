/* #define FUSE_USE_VERSION 32 */

#define _GNU_SOURCE

/* #include "sqfs-util.h" */
#include <err.h>
#include <fcntl.h>
#include <sched.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/mount.h>
#include <sys/prctl.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <linux/loop.h>

#include <libmount/libmount.h>

#include "rootless.h"

#define exit_with_error(...)                                                   \
  do {                                                                         \
    fprintf(stderr, __VA_ARGS__);                                              \
    exit(EXIT_FAILURE);                                                        \
  } while (0)

static void help(char const *argv0) {
  exit_with_error(
      "Usage: %s <squashfs file> <mountpoint> <command> [args...]\n", argv0);
}

int main(int argc, char **argv) {
  char *program = argv[0];
  struct stat mnt_stat;

  argv++;
  argc--;

  // Early exit for -h, --help, -v, --version.
  for (int i = 0, num_positional = 0; i < argc && num_positional <= 2; ++i) {
    char const *arg = argv[i];
    // Skip positional args.
    if (arg[0] != '-' || arg[1] == '\0') {
      ++num_positional;
      continue;
    }
    // Early exit on -h, --help, -v, --version
    ++arg;
    if (strcmp(arg, "h") == 0 || strcmp(arg, "-help") == 0)
      help(program);
    if (strcmp(arg, "v") == 0 || strcmp(arg, "-version") == 0) {
      puts(VERSION);
      exit(EXIT_SUCCESS);
    }
    // Error on unrecognized flags.
    errx(EXIT_FAILURE, "Unknown flag %s", argv[i]);
  }

  // We need [squashfs_file] [mountpoint] [command]
  if (argc < 3)
    help(program);

  char *squashfs_file = *argv++;
  argc--;

  char *mountpoint = *argv++;
  argc--;

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



  return mount_squashfuse(squashfs_file, mountpoint, argv);
}
