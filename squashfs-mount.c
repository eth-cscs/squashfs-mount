#define _GNU_SOURCE

#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdint.h>
#include <sched.h>

#include <sys/types.h>
#include <sys/mount.h>
#include <sys/prctl.h>

#include <linux/loop.h>

#include <libmount/libmount.h>

#define exit_with_error(...) do { fprintf(stderr, __VA_ARGS__); exit(EXIT_FAILURE); } while (0)

int main (int argc, char **argv) {
    struct libmnt_context *cxt;
    uid_t uid = getuid();

    if (argc < 4)
        exit_with_error("Usage: %s [squashfs file] [mountpoint] args...\n", argv[0]);

    if (unshare(CLONE_NEWNS) != 0)
        exit_with_error("Failed to unshare the mount namespace\n");

    if(mount(NULL, "/", NULL, MS_SLAVE|MS_REC, NULL) != 0)
        exit_with_error("Failed to remount \"/\" with MS_SLAVE\n");

    // Set real user to root before creating the mount context, otherwise it fails.
    if(setreuid(0, 0) != 0)
        exit_with_error("Failed to setreuid\n");

    cxt = mnt_new_context();

    if (mnt_context_disable_mtab(cxt, 1) != 0)
        exit_with_error("Failed to disable mtab\n");

    if (mnt_context_set_fstype(cxt, "squashfs") != 0)
        exit_with_error("Failed to set fstype to squashfs\n");

    if (mnt_context_append_options(cxt, "loop,nosuid,nodev,ro") != 0)
        exit_with_error("Failed to set target\n");

    if (mnt_context_set_source(cxt, argv[1]) != 0)
        exit_with_error("Failed to set source\n");

    if (mnt_context_set_target(cxt, argv[2]) != 0)
        exit_with_error("Failed to set target\n");

    if (mnt_context_mount(cxt) != 0)
        exit_with_error("Failed to mount\n");

    if (setresuid(uid, uid, uid) != 0)
        exit_with_error("setresuid failed\n");

    // If SQUASHFS_MOUNT_NO_NEW_PRIVS is defined, we effectively do not allow
    // nested `squashfs-mount` calls
#if defined(SQUASHFS_MOUNT_NO_NEW_PRIVS)
    if (prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0) != 0)
        exit_with_error("prctl failed\n");
#endif

    return execvp(argv[3], argv + 3);
}
