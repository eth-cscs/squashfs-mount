#!/usr/bin/bash

# Creates a squashfs image, mounts it with squashfs-mount and tests
# the mounted image.
# Requires that squashfs-mount has been installed as suid.

rm -rf test-dir
mkdir test-dir
echo "hello world" >> test-dir/test.txt
mksquashfs test-dir fs.sqsh -noappend 
sudo mkdir /mntpnt
result=$(squashfs-mount fs.sqsh /mntpnt cat /mntpnt/test.txt)
if [[ "$result" != "hello world" ]]; then
    echo "------ unexpected result: \"$result\""
    exit 1;
else
    echo "------ Success"
fi

