.PHONY: install install-suid clean

prefix = /usr/local
exec_prefix = $(prefix)
bindir = $(exec_prefix)/bin
datarootdir = $(prefix)/share
mandir ?= $(datarootdir)/man

CPPFLAGS ?= -Wdate-time -D_FORTIFY_SOURCE=2
CFLAGS ?= -Os -Wall -Wpedantic -Wextra -Wformat-overflow -Werror-implicit-function-declaration
LDFLAGS ?= -Wl,--gc-sections,-s

SQUASHFS_MOUNT_VERSION := $(shell cat VERSION)
SQUASHFS_MOUNT_CFLAGS = -std=c99 -DVERSION=\"$(SQUASHFS_MOUNT_VERSION)\"
SQUASHFS_MOUNT_LDFLAGS = -lmount

RPMBUILD ?= rpmbuild

all: squashfs-mount

%.o: %.c
	$(CC) $(CPPFLAGS) $(CFLAGS) $(SQUASHFS_MOUNT_CFLAGS) -c -o $@ $<

squashfs-mount.o: VERSION

squashfs-mount: squashfs-mount.o
	$(CC) $< $(LDFLAGS) $(SQUASHFS_MOUNT_LDFLAGS) -o $@

install: squashfs-mount
	mkdir -p $(DESTDIR)$(bindir)
	cp -p squashfs-mount $(DESTDIR)$(bindir)
	mkdir -p $(DESTDIR)$(mandir)/man1
	cp -p doc/squashfs-mount.1 $(DESTDIR)$(mandir)/man1

install-suid: install
	chown root:root $(DESTDIR)$(bindir)/squashfs-mount
	chmod u+s $(DESTDIR)$(bindir)/squashfs-mount

rpm: squashfs-mount.c VERSION LICENSE Makefile doc/squashfs-mount.1
	./generate-rpm.sh -b $@
	$(RPMBUILD) -bs --define "_topdir $@" $@/SPECS/squashfs-mount.spec

clean:
	rm -rf squashfs-mount squashfs-mount.o rpm
