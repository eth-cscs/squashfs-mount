/** @file Functions required to run as unprivileged user.
 */

#include "rootless.h"
#define _GNU_SOURCE

#include <squashfuse/ll.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <err.h>
#include <sched.h>
#include <sys/mount.h>
#include <sys/prctl.h>

#define exit_with_error(...)                                                   \
  do {                                                                         \
    fprintf(stderr, __VA_ARGS__);                                              \
    exit(EXIT_FAILURE);                                                        \
  } while (0)

/* Same effect as `unshare --mount --map-root-user` */
void unshare_mount_map_root() {

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
void map_effective_user(uid_t uid, gid_t gid) {

  if (unshare(CLONE_NEWUSER | CLONE_NEWNS) != 0) {
    exit_with_error("unshare(CLONE_NEWUSER|CLONE_NEWNS) failed");
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
