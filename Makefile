.PHONY: install install-suid clean

prefix = /usr/local
exec_prefix = $(prefix)
bindir = $(exec_prefix)/bin

CFLAGS ?= -Os -Wall -Wpedantic
SQFS_CFLAGS = -D_FILE_OFFSET_BITS=64 # only needed for fuse2
LDFLAGS ?= -Wl,--gc-sections,-s

squashfuse_install_dir = /usr
fuse_include_dir = /usr/include/fuse
fuse_lib = -lfuse # -lfuse3

SQUASHFS_MOUNT_VERSION := $(shell cat VERSION)
SQUASHFS_MOUNT_CFLAGS = -std=c99 -DVERSION=\"$(SQUASHFS_MOUNT_VERSION)\" -isystem $(fuse_include_dir) -isystem $(squashfuse_install_dir)/include
SQUASHFS_MOUNT_LDFLAGS =-lmount  -L$(squashfuse_install_dir)/lib -lsquashfuse_ll $(fuse_lib)

RPMBUILD ?= rpmbuild

all: squashfs-mount

%.o: %.c
	$(CC) $(CFLAGS) $(SQFS_CFLAGS) $(SQUASHFS_MOUNT_CFLAGS) -c -o $@ $<
squashfs-mount.o: VERSION non-suid.h

squashfs-mount: squashfs-mount.o non-suid.o
		$(CC) $^ $(LDFLAGS) $(SQUASHFS_MOUNT_LDFLAGS) -o $@

install: squashfs-mount
	mkdir -p $(DESTDIR)$(bindir)
	cp -p squashfs-mount $(DESTDIR)$(bindir)

install-suid: install
	chown root:root $(DESTDIR)$(bindir)/squashfs-mount
	chmod u+s $(DESTDIR)$(bindir)/squashfs-mount

rpm: squashfs-mount.c non-suid.c non-suid.h VERSION LICENSE Makefile
	./generate-rpm.sh -b $@
	$(RPMBUILD) -bs --define "_topdir $@" $@/SPECS/squashfs-mount.spec

clean:
	rm -rf squashfs-mount squashfs-mount.o non-suid.o rpm
