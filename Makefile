.PHONY: install install-suid clean

prefix = /usr/local
exec_prefix = $(prefix)
bindir = $(exec_prefix)/bin

CFLAGS ?= -Os -Wall -Wpedantic
LDFLAGS ?= -Wl,--gc-sections,-s

SQUASHFS_MOUNT_VERSION := $(shell cat VERSION)
SQUASHFS_MOUNT_CFLAGS = -std=c99 -DVERSION=\"$(SQUASHFS_MOUNT_VERSION)\"
SQUASHFS_MOUNT_LDFLAGS = -lmount

RPMBUILD ?= rpmbuild

all: squashfs-mount

%.o: %.c
	$(CC) $(CFLAGS) $(SQUASHFS_MOUNT_CFLAGS) -c -o $@ $<

squashfs-mount.o: VERSION

squashfs-mount: squashfs-mount.o
	$(CC) $< $(LDFLAGS) $(SQUASHFS_MOUNT_LDFLAGS) -o $@

install: squashfs-mount
	mkdir -p $(DESTDIR)$(bindir)
	cp -p squashfs-mount $(DESTDIR)$(bindir)

install-suid: install
	chown root:root $(DESTDIR)$(bindir)/squashfs-mount
	chmod u+s $(DESTDIR)$(bindir)/squashfs-mount

rpm: squashfs-mount.c VERSION LICENSE Makefile
	./generate-rpm.sh -b $@
	$(RPMBUILD) -bs --define "_topdir $@" $@/SPECS/squashfs-mount.spec

clean:
	rm -rf squashfs-mount squashfs-mount.o rpm
