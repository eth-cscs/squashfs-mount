#define _GNU_SOURCE
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <sys/mount.h>
#include <sys/prctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/mount.h>
#include <sys/wait.h>
#include <unistd.h>

#include <libmount/libmount.h>

#define ENV_MOUNT_LIST "UENV_MOUNT_LIST"

#define exit_with_error(...)                                                   \
  do {                                                                         \
    fprintf(stderr, __VA_ARGS__);                                              \
    exit(EXIT_FAILURE);                                                        \
  } while (0)

static void help(char const *argv0) {
  exit_with_error("Usage: %s <options> <image>:<mountpoint> [<image>:<mountpoint>]...  "
                  "-- <command> [args...]\nOptions:\n--squashfuse   Use Squashfuse to mount images rootless.\n",
                  argv0);
}

typedef struct {
  char squashfs_file[PATH_MAX];
  char mountpoint[PATH_MAX];
} mount_entry_t;

static void unshare_mntns_and_usrns(uid_t euid, uid_t egid) {
  int fd; //initialize file descriptor 

  if (unshare(CLONE_NEWNS|CLONE_NEWUSER) != 0) //unshare mount and user namespace
    err(EXIT_FAILURE, "Failed to unshare the user and mount namespaces");
  
  // Set the userid inside the user namespace to the root user (0)
  if (-1 == (fd = open("/proc/self/uid_map", O_WRONLY)))
    err(EXIT_FAILURE, "Failed to open uid_map for user namespace");
  if (1 > dprintf(fd, "%d %d 1\n", 0, euid)) //change 0, 0 for non-root inside namespace
    err(EXIT_FAILURE, "Failed to set UID inside user namespace");
  close(fd);

  // Set the setgroups option to 'deny' inside the user namespace
  if (-1 == (fd = open("/proc/self/setgroups", O_WRONLY)))
    err(EXIT_FAILURE, "Failed to open setgroups for user namespace");
  if (1 > dprintf(fd, "deny\n")) 
    err(EXIT_FAILURE, "Failed to set 'setgroups deny' inside user namespace");
  close(fd);

  // Set the groupid inside the user namespace to the root group (0)
  if (-1 == (fd = open("/proc/self/gid_map", O_WRONLY)))
    err(EXIT_FAILURE, "Failed to open gid_map for user namespace");
  if (1 > dprintf(fd, "%d %d 1\n", 0, egid)) //change 0, 0 for non-root inside namespace
    err(EXIT_FAILURE, "Failed to set GID inside user namespace");
  close(fd);
}

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

/// check if squashsfs_file is an existent file, mounpoint is an existent
/// directory
static void validate_file_and_mountpoint(char const *squashfs_file,
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

static void do_mount(const mount_entry_t *entry, bool squashfuse) {
  struct libmnt_context *cxt;

  validate_file_and_mountpoint(entry->squashfs_file, entry->mountpoint);
  if (squashfuse) { //use squashfuse instead of mount
    char squashfuse_command[1024];
    sprintf(squashfuse_command, "squashfuse %s %s", entry->squashfs_file, entry->mountpoint);
    if (system(squashfuse_command) == -1)
      err(EXIT_FAILURE, "Failed to mount squashfs image with squashfuse");
  }
  else {// normal squashfs-mount: use mount
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
}

static void do_mount_loop(const mount_entry_t *mount_entries, int n, bool squashfuse) {

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
    do_mount(mount_entries + i, squashfuse); //pass squashfuse bool to do_mount
  }

}

static void do_unmount(const mount_entry_t *entry) { //single unmount for squashfuse mount
  if (umount2(entry->mountpoint, MNT_DETACH) == -1)
    err(EXIT_FAILURE, "Failed to unmount squashfs image (check ps -ef to kill)");
}

static void do_unmount_loop(const mount_entry_t *mount_entries, int n){ //loop for unmount each squashfuse mount
  for (int i = 0; i < n; ++i) {
    do_unmount(mount_entries + i);
  } 
}

/// lexical sorting for mountpoint
static int compare_mountpoint(const void *p1, const void *p2) {
  return strcmp(((const mount_entry_t *)p1)->mountpoint,
                ((const mount_entry_t *)p2)->mountpoint);
}

/// split by `:` and convert to abspath, sort by mountpoint
static mount_entry_t *parse_mount_entries(char **argv, int argc) {
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

char **fwd_env() {

  int num_old_vars = 0;
  int num_fwd_vars = 0;
  const char *prefix = "SQFSMNT_FWD_";
  size_t prefix_len = strlen(prefix);
  while (environ[num_old_vars] != NULL) {
    if (strncmp(environ[num_old_vars], prefix, prefix_len) == 0) {
      ++num_fwd_vars;
    }
    ++num_old_vars;
  }

  const int num_total_vars = num_old_vars + num_fwd_vars;

  // allocate memory for the new environment variables
  char **new_environ = (char **)malloc(sizeof(char *) * (num_total_vars + 1));
  if (new_environ == NULL) {
    return NULL;
  }

  // Copy the old environment to new_environ.
  // Append the forwarded environment variables to the additional num_total_vars
  // slots that were allocated.
  int i = 0;
  int j = num_old_vars;

  for (i = 0; i < num_old_vars; ++i) {
    new_environ[i] = strdup(environ[i]);
    if (new_environ[i] == NULL) {
      return NULL;
    }
    // check whether the env. var name starts with the prefix
    if (strncmp(environ[i], prefix, prefix_len) == 0) {
      // assert(j < num_total_vars);
      new_environ[j] = strdup(new_environ[i] + prefix_len);
      if (new_environ[j] == NULL) {
        return NULL;
      }
      ++j;
    }
  }

  // assert(j==num_total_vars);
  new_environ[j] = NULL;

  // For each new variable that was set, check whether it was already set in the
  // calling environment. If it is, overwrite the original value with the new
  // one. This step is not necessary in bash, but zsh requires it for the new
  // value to be set correctly.
  for (j = num_old_vars; j < num_total_vars; ++j) {
    // find the first = sign
    char *pos = strchr(new_environ[j], '=');
    if (pos) {
      size_t len = pos - new_environ[j] + 1;
      // search for the first occurence of this in the existing variable list
      for (i = 0; i < num_old_vars; ++i) {
        if (strncmp(new_environ[i], new_environ[j], len) == 0) {
          // copy in place
          free(new_environ[i]);
          new_environ[i] = strdup(new_environ[j]);
          break;
        }
      }
    }
  }

  return new_environ;
}

void free_env(char **envp) {
  for (int i = 0; envp[i] != NULL; i++) {
    free(envp[i]);
  }
  free(envp);
}

int main(int argc, char **argv) {
  char **fwd_argv;
  mount_entry_t *mount_entries;
  uid_t uid = getuid();
  bool squashfuse = false;

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
    if (strcmp(arg, "-s") == 0 || strcmp(arg, "--squashfuse") == 0) {
      squashfuse = true;
        argv++;
        argc--;
      continue;
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
    return_to_user_and_no_new_privs(uid);
    fprintf(stderr, "Warning no <image>:<mountpoint> argument was given.\n");
    char **new_env = fwd_env();
    if (new_env == NULL) {
      err(EXIT_FAILURE, "failed to modify the environment variables");
    }
    return execvpe(fwd_argv[0], fwd_argv, new_env);
  }

  mount_entries = parse_mount_entries(argv, positional_args);
  
  //get euid and egid for squashfuse
  uid_t euid = geteuid();
  uid_t egid = getegid();

  if (squashfuse){ //if squashfuse, unshare mount and user namespace (rootless)
    unshare_mntns_and_usrns(euid, egid);
  }
  else{
    unshare_mntns_and_become_root(); //else, unshare mount namespace and setuid()
  }
  do_mount_loop(mount_entries, positional_args, squashfuse); //pass squashfuse bool to do_mount_loop

  // return to user, set PR_SET_NO_NEW_PRIVS (if no squashfuse)
  if (!squashfuse){
    return_to_user_and_no_new_privs(uid);
  }
  // export environment variable with mounted images (for slurm plugin)
  char *uenv_mount_list = malloc(sizeof(char) * 2 * positional_args * PATH_MAX);
  sprintf(uenv_mount_list, "%s:%s", mount_entries[0].squashfs_file,
          mount_entries[0].mountpoint);
  for (int i = 1; i < positional_args; ++i) {
    char buf[2 * PATH_MAX + 8];
    sprintf(buf, ",%s:%s", mount_entries[i].squashfs_file,
            mount_entries[i].mountpoint);
    strcat(uenv_mount_list, buf);
  }
  if (setenv(ENV_MOUNT_LIST, uenv_mount_list, 1)) {
    err(EXIT_FAILURE, "failed to set environment variables");
  }

  char **new_env = fwd_env();
  if (new_env == NULL) {
    err(EXIT_FAILURE, "failed to modify the environment variables");
  }

  if (squashfuse){
    pid_t pid = fork(); //child process runs execvpe, parent process waits for return and then unmounts squashfuse.
    int cmd_length = 5+argc-positional_args; //(unshare + args =5) + (command + command args = (argc-positional_args-1)) + (NULL = 1) --> 5+argc-positional_args
    char *unshare_command[cmd_length];

    if (pid == -1){
      perror("Fork Failed\n");
      return 1;
    }
    else if (pid == 0) { //child process
      //unshare mount and user namespace
      unshare_command[0] = "unshare";
      unshare_command[1] = "--mount";
      unshare_command[2] = "--user";

      //map to outside euid
      char map_user[64];
      sprintf(map_user, "--map-user=%d", euid);
      unshare_command[3] = map_user;

      //map to outside egid
      char map_group[64];
      sprintf(map_group, "--map-group=%d", egid);
      unshare_command[4] = map_group;

      //load fwd_argv into unshare_command
      for (int i = 0; i < (argc-positional_args-1); i++){
        unshare_command[i+5] = fwd_argv[i];
        //printf(fwd_argv[i]);
      }
      unshare_command[cmd_length-1] = NULL; //add null terminator 

      int result = execvpe(unshare_command[0], unshare_command, new_env); //exec nested unshare with user command
      free_env(new_env);
      err(EXIT_FAILURE, "unable to perform exve");
      return result;
    }
    else{ //parent process
      int status;
      waitpid(pid, &status, 0); //wait for return
      do_unmount_loop(mount_entries, positional_args); //unmount squashfuse mounts
    }
    // cleanup
    free(uenv_mount_list);
    free(mount_entries);
  }
  else{ //normal squashfs-mount
      // cleanup
      free(uenv_mount_list);
      free(mount_entries);
      int result = execvpe(fwd_argv[0], fwd_argv, new_env);
      free_env(new_env);
      err(EXIT_FAILURE, "unable to perform exve");
      return result;
  }

  return 1;
  // the remaining code is only called if execvpe fails

}
