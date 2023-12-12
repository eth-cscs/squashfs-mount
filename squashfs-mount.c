#include <linux/limits.h>
#define _GNU_SOURCE
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mount.h>
#include <sys/prctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <libmount/libmount.h>

#include "utils.h"

#define ENV_MOUNT_LIST "UENV_MOUNT_LIST"

static void unshare_mntns_and_become_root() {
  if (unshare(CLONE_NEWNS) != 0)
    err(EXIT_FAILURE, "Failed to unshare the mount namespace");

  if (mount(NULL, "/", NULL, MS_SLAVE | MS_REC, NULL) != 0)
    err(EXIT_FAILURE, "Failed to remount \"/\" with MS_SLAVE");

  // Set real user to root before creating the mount context, otherwise it
  // fails.
  if (setreuid(0, 0) != 0)
    err(EXIT_FAILURE, "Failed to setreuid\n");

  // Configure the mount
  // Makes LIBMOUNT_DEBUG=... work.
  mnt_init_debug(0);
}

/// set real, effective, saved user id to original user and allow no new
/// priviledges
static void return_to_user_and_no_new_privs(int uid) {
  if (setresuid(uid, uid, uid) != 0)
    errx(EXIT_FAILURE, "setresuid failed");

  if (prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0) != 0)
    err(EXIT_FAILURE, "PR_SET_NO_NEW_PRIVS failed");
}

static void do_mount(const mount_entry_t *entry) {
  struct libmnt_context *cxt;

  validate_file_and_mountpoint(entry->squashfs_file, entry->mountpoint);

  cxt = mnt_new_context();

  if (mnt_context_disable_mtab(cxt, 1) != 0)
    errx(EXIT_FAILURE, "Failed to disable mtab");

  if (mnt_context_set_fstype(cxt, "squashfs") != 0)
    errx(EXIT_FAILURE, "Failed to set fstype to squashfs");

  if (mnt_context_append_options(cxt, "loop,nosuid,nodev,ro") != 0)
    errx(EXIT_FAILURE, "Failed to set mount options");

  if (mnt_context_set_source(cxt, entry->squashfs_file) != 0)
    errx(EXIT_FAILURE, "Failed to set source");

  if (mnt_context_set_target(cxt, entry->mountpoint) != 0)
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
}

int main(int argc, char **argv) {
  char **fwd_argv;
  mount_entry_t *mount_entries;
  uid_t uid = getuid();

  char *program = argv[0];

  argv++;
  argc--;

  int positional_args = 0;
  // Early exit for -h, --help, -v, --version.
  for (int i = 0; i < argc; ++i, positional_args++) {
    char const *arg = argv[i];
    // Skip positional args.
    if (arg[0] != '-' || arg[1] == '\0') {
      continue;
    }

    // finish parsing after -- flag
    if (strcmp(arg, "--") == 0) {
      break;
    }
    if (strcmp(arg, "-h") == 0 || strcmp(arg, "--help") == 0)
      help(program);
    if (strcmp(arg, "-v") == 0 || strcmp(arg, "--version") == 0) {
      puts(VERSION);
      exit(EXIT_SUCCESS);
    }
    // Error on unrecognized flags.
    errx(EXIT_FAILURE, "Unknown flag %s", argv[i]);
  }

  if (argc <= positional_args + 1) {
    exit_with_error("no command given");
  }

  fwd_argv = argv + (positional_args + 1);
  // if no mountpoints given, run command directly
  if (positional_args == 0) {
    fprintf(stderr, "Warning no <image>:<mountpoint> argument was given.\n");
    return execvp(fwd_argv[0], fwd_argv);
  }

  mount_entries = parse_mount_entries(argv, positional_args);

  unshare_mntns_and_become_root();
  do_mount_loop(mount_entries, positional_args, &do_mount);
  // return to user, set PR_SET_NO_NEW_PRIVS
  return_to_user_and_no_new_privs(uid);

  // export environment variable with mounted images (for slurm plugin)
  char *uenv_mount_list = malloc(sizeof(char) * 2 * positional_args * PATH_MAX);
  sprintf(uenv_mount_list, "file://%s:%s", mount_entries[0].squashfs_file,
          mount_entries[0].mountpoint);
  for (int i = 1; i < positional_args; ++i) {
    char buf[2 * PATH_MAX + 8];
    sprintf(buf, ",file://%s:%s", mount_entries[i].squashfs_file,
            mount_entries[i].mountpoint);
    strcat(uenv_mount_list, buf);
  }
  if (setenv(ENV_MOUNT_LIST, uenv_mount_list, 1)) {
    err(EXIT_FAILURE, "failed to set environment variables");
  }

  // cleanup
  free(uenv_mount_list);
  free(mount_entries);

  return execvp(fwd_argv[0], fwd_argv);
}
