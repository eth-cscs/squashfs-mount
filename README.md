# squashfs-mount

`squashfs-mount [src.squashfs] [dst] command...` is a small setuid binary that
effectively runs `mount -n -o loop,nosuid,nodev,ro -t squashfs [src.squashfs] [dst]` in
a mount namespace and then executes the given command as the normal user.

Dependencies:

- `util-linux`

Instructions:

```console
make
sudo chown root:root squashfs-mount
sudo chmod +s squashfs-mount
make install