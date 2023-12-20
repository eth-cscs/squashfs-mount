#pragma once

#include "utils.h"

/* Same effect as `unshare --mount --map-root-user` */
void unshare_mount_map_root();

/* go back to effective user */
void map_effective_user(uid_t uid, gid_t gid);

/* squashfs_ll mount */
void do_sqfs_mount(const mount_entry_t *);
