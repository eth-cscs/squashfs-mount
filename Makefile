.PHONY: install clean

prefix = /usr/local
exec_prefix = $(prefix)
bindir = $(exec_prefix)/bin

CFLAGS ?= -Os -Wall -Wpedantic
LDFLAGS ?= -Wl,--gc-sections,-s
ENV_RUN_CFLAGS = -std=c99
ENV_RUN_LDFLAGS = -lmount

all: squashfs-mount

%.o: %.c
	$(CC) $(INC) $(CFLAGS) $(ENV_RUN_CFLAGS) -c -o $@ $<

squashfs-mount: squashfs-mount.o
	$(CC) $< $(LIB) $(LDFLAGS) $(ENV_RUN_LDFLAGS) -o $@


install: squashfs-mount
	mkdir -p $(DESTDIR)$(bindir)
	cp -p squashfs-mount $(DESTDIR)$(bindir)

clean:
	rm -f squashfs-mount squashfs-mount.o
