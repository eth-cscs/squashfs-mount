#!/usr/bin/bash

# Creates a squashfs image, mounts it with squashfs-mount and tests
# the mounted image.
# Requires that squashfs-mount has been installed as suid.

tmpdir=$(mktemp -d)
(
    cd "$tmpdir" || exit
    mkdir test-dir
    echo "hello world" >> test-dir/test.txt
    mksquashfs test-dir fs.sqsh -noappend -comp zstd
    mkdir mountpoint
    result=$(squashfs-mount-rootless fs.sqsh mountpoint cat mountpoint/test.txt)

    if [[ "$result" != "hello world" ]]; then
        echo "------ unexpected result: \"$result\""
        exit 1;
    else
        echo "------ Success"
    fi

)
