/** @file Functions required to run as unprivileged user.
 */

#include "rootless.h"
#define _GNU_SOURCE
#include <squashfuse/ll.h>

#include <signal.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sched.h>
#include <sys/mount.h>
#include <sys/prctl.h>
#include <err.h>

#define exit_with_error(...)                                                   \
  do {                                                                         \
    fprintf(stderr, __VA_ARGS__);                                              \
    exit(EXIT_FAILURE);                                                        \
  } while (0)

/* Same effect as `unshare --mount --map-root-user` */
static void unshare_mount_map_root() {

  int uid = getuid(); // get current uid
  int gid = getgid();
  if (unshare(CLONE_NEWUSER | CLONE_NEWNS) != 0)
    err(EXIT_FAILURE, "unshare(CLONE_NEWUSER | CLONE_NEWUSER) failed");

  if (mount(NULL, "/", NULL, MS_SHARED | MS_REC, NULL) != 0)
    err(EXIT_FAILURE, "Failed to remount \"/\" with MS_SLAVE");

  // map current user id to root
  char buf[256];
  int proc_uid_map = openat(AT_FDCWD, "/proc/self/uid_map", O_WRONLY);
  sprintf(buf, "0 %d 1", uid);
  write(proc_uid_map, buf, strlen(buf));
  close(proc_uid_map);

  int proc_setgroups = openat(AT_FDCWD, "/proc/self/setgroups", O_WRONLY);
  write(proc_setgroups, "deny", 4);
  close(proc_setgroups);

  int proc_gid_map = openat(AT_FDCWD, "/proc/self/gid_map", O_WRONLY);
  sprintf(buf, "0 %d 1", gid);
  write(proc_gid_map, buf, strlen(buf));
  close(proc_gid_map);

  // the following is executed by `unshare --mount --map-root-user`
  if (mount("none", "/", NULL, MS_REC | MS_PRIVATE, NULL)) {
    exit_with_error("failed to execute: mount(\" none \", \" / \", NULL, "
                    "MS_REC|MS_PRIVATE, NULL)");
  }
}

/* go back to effective user */
static void map_effective_user(int uid, int gid) {

  if (unshare(CLONE_NEWUSER) != 0) {
    exit_with_error("unshare(CLONE_NEWUSER) failed");
  }

  // map current user id to root
  char buf[256];
  int proc_uid_map = openat(AT_FDCWD, "/proc/self/uid_map", O_WRONLY);
  sprintf(buf, "%d 0 1", uid);
  write(proc_uid_map, buf, strlen(buf));
  close(proc_uid_map);

  int proc_setgroups = openat(AT_FDCWD, "/proc/self/setgroups", O_WRONLY);
  write(proc_setgroups, "allow", 5);
  close(proc_setgroups);

  int proc_gid_map = openat(AT_FDCWD, "/proc/self/gid_map", O_WRONLY);
  sprintf(buf, "%d 0 1", gid);
  write(proc_gid_map, buf, strlen(buf));
  close(proc_gid_map);
}

static void write_pipe(int pipe) {
  char c[32];
  // AppImage seems doing something more advanced:
  // https://github.com/AppImage/AppImageKit/blob/master/src/runtime.c#L138
  memset(c, 'x', sizeof(c));
  int res = write(pipe, c, sizeof(c));
  if (res < 0) {
    exit_with_error("writing to pipe failed");
  }
}

static void init_fs_ops(struct fuse_lowlevel_ops *sqfs_ll_ops) {
  memset(sqfs_ll_ops, 0, sizeof(*sqfs_ll_ops));
  sqfs_ll_ops->getattr = sqfs_ll_op_getattr;
  sqfs_ll_ops->opendir = sqfs_ll_op_opendir;
  sqfs_ll_ops->releasedir = sqfs_ll_op_releasedir;
  sqfs_ll_ops->readdir = sqfs_ll_op_readdir;
  sqfs_ll_ops->lookup = sqfs_ll_op_lookup;
  sqfs_ll_ops->open = sqfs_ll_op_open;
  sqfs_ll_ops->create = sqfs_ll_op_create;
  sqfs_ll_ops->release = sqfs_ll_op_release;
  sqfs_ll_ops->read = sqfs_ll_op_read;
  sqfs_ll_ops->readlink = sqfs_ll_op_readlink;
  sqfs_ll_ops->listxattr = sqfs_ll_op_listxattr;
  sqfs_ll_ops->getxattr = sqfs_ll_op_getxattr;
  sqfs_ll_ops->forget = sqfs_ll_op_forget;
  sqfs_ll_ops->statfs = stfs_ll_op_statfs;
}

/**
 * Mount a squashfs image in the background (forked process), unmount the image
 * when the parent process exits (shell is closed).
 *
 * Adapted from `squashfuse_ll` written by Dave Vasilevsky <dave@vasilevsky.ca>
 * Ref: https://github.com/vasi/squashfuse/blob/master/ll_main.c
 *
 * @param absolute path to squashfs image file
 * @param absolute path to mountpoint
 * @param offset of image in bytes
 */
void do_sqfs_mount(const char image[], int offset, const char mountpoint[]) {

  // use a pipe to synchronize parent and child process
  int pipe_wait[2];
  if (pipe(pipe_wait) != 0) {
    exit_with_error("pipe error");
  }

  int pid = fork();
  if (pid == 0) {

    // kill the fuse process when the parent exits
    prctl(PR_SET_PDEATHSIG, SIGHUP);

    // do not listen for SIGINT (ctrl+c)
    signal(SIGINT, SIG_IGN);

    // setup dummy fuse_args to make sqfs_ll_open happy
    struct fuse_args args;
    char *dummy_argv[] = {"dummy", NULL};
    args.argc = 1;
    args.argv = dummy_argv;
    args.allocated = 0;
    int err;
    sqfs_ll *ll;

    // define squashfs file system operations
    struct fuse_lowlevel_ops sqfs_ll_ops;
    init_fs_ops(&sqfs_ll_ops);

    /* fuse_daemonize() will unconditionally clobber fds 0-2.
     * If we get one of these file descriptors in sqfs_ll_open,
     * we're going to have a bad time. Just make sure that all
     * these fds are open before opening the image file, that way
     * we must get a different fd.
     */
    while (true) {
      int fd = open("/dev/null", O_RDONLY);
      if (fd == -1) {
        /* Can't open /dev/null, how bizarre! However,
         * fuse_deamonize won't clobber fds if it can't
         * open /dev/null either, so we ought to be OK.
         */
        break;
      }
      if (fd > 2) {
        /* fds 0-2 are now guaranteed to be open. */
        close(fd);
        break;
      }
    }

    int idle_timeout_secs = 0;
    /* the child process starts fuse and informs the calling process via pipe
     * when done */

    /* OPEN FS */
    err = !(ll = sqfs_ll_open(image, offset));
    if (err) {
      exit_with_error("sqfs_ll_open failed\n");
    }
    if (!err) {
      /* startup fuse */
      sqfs_ll_chan ch;
      /* err = -1; */
      sqfs_err sqfs_ret = sqfs_ll_mount(&ch, mountpoint, &args, &sqfs_ll_ops, sizeof(sqfs_ll_ops), ll);
      if (sqfs_ret == SQFS_OK) {

        if (sqfs_ll_daemonize(true /*foreground*/) != -1) {
          // inform parent process that sqfs has been mounted
          close(pipe_wait[0]);
          write_pipe(pipe_wait[1]);

          // setup signlal handlers and enter fuse_session_loop
          if (fuse_set_signal_handlers(ch.session) != -1) {
            if (idle_timeout_secs) {
              setup_idle_timeout(ch.session, idle_timeout_secs);
            }
            /* FIXME: multithreading */
            err = fuse_session_loop(ch.session);
            teardown_idle_timeout();
            fuse_remove_signal_handlers(ch.session);
          } else {
            exit_with_error("set signal handlers failed.");
          }
        } else {
          exit_with_error("daemonize failed");
        }
        sqfs_ll_destroy(ll);
        sqfs_ll_unmount(&ch, mountpoint);
      } else {
        switch (sqfs_ret) {
          case SQFS_ERR: {
            printf("SQFS_ERR\n");
            break;
          }
          case SQFS_BADFORMAT: {
            printf("SQFS_BADFORMAT (unsupported file format)\n");
            break;
          }
          case SQFS_BADVERSION: {
            printf("SQFS_BADVERSION\n");
            break;
          }
          case SQFS_BADCOMP: {
            printf("SQFS_BADCOMP\n");
            break;
          }
          case SQFS_UNSUP: {
            printf("SQFS_UNSUP, unsupported feature\n");
            break;
          }
          case SQFS_OK: {
            break;
          }
        }
        exit_with_error("sqfs_ll_mount failed.\n");
      }

    } else {
      exit_with_error("sqfs_ll_open_failed");
    }
    fuse_opt_free_args(&args);
    free(ll);
    exit(0);
  } else {
    /* parent block on pipe until fusemount has finished. */
    char buf[256];
    close(pipe_wait[1]);
    int res = read(pipe_wait[0], buf, 256);
    if (res == 0) {
      // The child process has exited before reaching fuse_session_loop.
      exit_with_error("mounting sqfs file failed\n");
    }
    if (res < 0) {
      exit_with_error("mounting sqfs file failed\n");
    }
  }
}

int mount_squashfuse(const char *sqfs_file, const char *mountpoint, char** argv)
{
  int uid = getuid();
  int gid = getgid();

  /* clone mount and user namespace, map as root */
  unshare_mount_map_root();

  /* mount image at mountpoint */
  do_sqfs_mount(sqfs_file, 0 /* offset */, mountpoint);

  // return to normal user by mapping root to the effective user
  // NOTE: additional groups are lost
  map_effective_user(uid, gid);

  return execvp(argv[0], argv);
}
