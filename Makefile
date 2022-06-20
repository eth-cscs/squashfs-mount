.PHONY: install clean

prefix = /usr/local
exec_prefix = $(prefix)
bindir = $(exec_prefix)/bin

CFLAGS ?= -Os -Wall -Wpedantic
LDFLAGS ?= -Wl,--gc-sections,-s
SQUASHFS_MOUNT_CFLAGS = -std=c99
SQUASHFS_MOUNT_LDFLAGS = -lmount

all: squashfs-mount

%.o: %.c
	$(CC) $(CFLAGS) $(SQUASHFS_MOUNT_CFLAGS) -c -o $@ $<

squashfs-mount: squashfs-mount.o
	$(CC) $< $(LDFLAGS) $(SQUASHFS_MOUNT_LDFLAGS) -o $@


install: squashfs-mount
	mkdir -p $(DESTDIR)$(bindir)
	cp -p squashfs-mount $(DESTDIR)$(bindir)

clean:
	rm -f squashfs-mount squashfs-mount.o
