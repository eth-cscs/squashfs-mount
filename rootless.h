#ifndef NON_SUID_H
#define NON_SUID_H

int mount_squashfuse(const char* sqfs_file, const char* mountpoint, char** argv);

#endif /* NON_SUID_H */
