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

#ifdef ROOTLESS
#include "rootless.h"
#endif

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
  struct libmnt_context *cxt;
  uid_t uid = getuid();
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

  // Set real user to root before creating the mount context, otherwise it
  // fails.
  if (setreuid(0, 0) != 0) {
#ifdef ROOTLESS
    fprintf(stderr, "Non-root user, mount image via squashfuse.\n");
    return mount_squashfuse(squashfs_file, mountpoint, argv);
#else
    err(EXIT_FAILURE, "Insufficient permissions.");
#endif
  }

  /* we are root */
  if (unshare(CLONE_NEWNS) != 0)
    err(EXIT_FAILURE, "Failed to unshare the mount namespace");

  if (mount(NULL, "/", NULL, MS_SLAVE | MS_REC, NULL) != 0)
    err(EXIT_FAILURE, "Failed to remount \"/\" with MS_SLAVE");

  // Configure the mount
  // Makes LIBMOUNT_DEBUG=... work.
  mnt_init_debug(0);

  cxt = mnt_new_context();

  if (mnt_context_disable_mtab(cxt, 1) != 0)
    errx(EXIT_FAILURE, "Failed to disable mtab");

  if (mnt_context_set_fstype(cxt, "squashfs") != 0)
    errx(EXIT_FAILURE, "Failed to set fstype to squashfs");

  if (mnt_context_append_options(cxt, "loop,nosuid,nodev,ro") != 0)
    errx(EXIT_FAILURE, "Failed to set mount options");

  if (mnt_context_set_source(cxt, squashfs_file) != 0)
    errx(EXIT_FAILURE, "Failed to set source");

  if (mnt_context_set_target(cxt, mountpoint) != 0)
    errx(EXIT_FAILURE, "Failed to set target");

  // Attempt to mount
  int mount_exit_code = mnt_context_mount(cxt);
  if (mount_exit_code != 0) {
    char err_buf[BUFSIZ] = {0};
    mnt_context_get_excode(cxt, mount_exit_code, err_buf, sizeof(err_buf));
    const char *tgt = mnt_context_get_target(cxt);
    if (*err_buf != '\0' && tgt != NULL)
      exit_with_error("%s: %s\n", tgt, err_buf);
    errx(EXIT_FAILURE, "Failed to mount");
  }

  if (setresuid(uid, uid, uid) != 0)
    errx(EXIT_FAILURE, "setresuid failed");

  if (prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0) != 0)
    err(EXIT_FAILURE, "PR_SET_NO_NEW_PRIVS failed");

  return execvp(argv[0], argv);
}
