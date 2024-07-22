# squashfs-mount

`squashfs-mount <image>:<mountpoint> [<image>:<mountpoint>]...  -- <command> [args...]` is a small setuid binary that
effectively runs `mount -n -o loop,nosuid,nodev,ro -t squashfs [image] [mountpoint]` in
a mount namespace and then executes the given command as the normal user.

## Environment Variables

The kernel unsets some environment variables, e.g. `LD_LIBRARY_PATH` and `LD_PRELOAD`, in setuid binaries for obvious security reasons. `squashfs-mount` will look for environment variables that have the `SQFSMNT_FWD_` prefix, and strip the prefix from the variable for the executed binary. This can be used to foward variables like `LD_LIBRARY_PATH` safely. For example:

```bash
SQFSMNT_FWD_LD_LIBRARY_PATH=$LD_LIBRARY_PATH squashfs-mount $image:$mount -- bash
```

## Motivation

squashfs blobs can be convenient to provide a full software stack: immutable,
single file, no need to extract. The downside is they can only be mounted
"rootless" (well, using setuid fusermount) with `squashfuse`, which has two
issues:

1. Bad performance compared to the Linux kernel (this situation is improving over time).

2. Inconvenient and complicated implementation, as illustrated by [NVIDIA enroot](https://github.com/NVIDIA/enroot):

   ```bash
   # Mount the image as the lower layer.
   squashfuse -f -o "uid=${euid},gid=${egid}" "${image}" "${rootfs}/lower" &
   pid=$!; i=0
   while ! mountpoint -q "${rootfs}/lower"; do
       ! kill -0 "${pid}" 2> /dev/null || ((i++ == timeout)) && exit 1
       sleep .001
   done
   ```

3. There are frequent CVEs related to user namespaces, which see the feature disabled on HPC systems while we wait for a fix (or disabled all-together out of an abundance of caution.)

## Dependencies

- `util-linux` (libmount)

## Install instructions

### Using Meson

```console
mkdir build
cd build
meson setup ..
sudo ninja install
```

### From Makefile

Build and install without privileges and make it a root-owned setuid binary by hand:

```console
make
make install prefix=./install
sudo chown root:root ./install/bin/squashfs-mount
sudo chmod u+s ./install/bin/squashfs-mount
```

Or use the `install-suid` target:

```console
make install prefix=./install
sudo make install-suid prefix=./install
```

### As an RPM

The `rpm` makefile target generates a source RPM, with `_topdir` in `$(pwd)/rpmbuild`.
The source RPM can be compiled
```console
make rpm
topdir="$(pwd)/rpmbuild"
sudo rpmbuild --rebuild "$topdir"/SRPMS/squashfs-mount*.src.rpm --define "_topdir $topdir"
sudo rpm --install "$topdir/RPMS/x86_64/squashfs-mount-*.x86_64.rpm"
```
The source RPM is distributed with tagged releases.
