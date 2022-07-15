#ifndef UNPRIVILEGED_H
#define UNPRIVILEGED_H

void unshare_mount_map_root();
void map_effective_user(int uid, int gid);
void do_sqfs_mount(const char image[], int offsetconst, char mountpoint[]);

#endif /* UNPRIVILEGED_H */
