CC ?= cc
CFLAGS ?= -O2 -pipe -Wall -Wextra -Wpedantic
CPPFLAGS += $(shell pkg-config --cflags x11 xrandr cairo pangocairo)
LDLIBS += $(shell pkg-config --libs x11 xrandr cairo pangocairo)

PREFIX ?= $(HOME)/.local

all: vfdlock

vfdlock: src/vfdlock.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ $< $(LDLIBS)

install: vfdlock
	install -Dm755 vfdlock "$(DESTDIR)$(PREFIX)/bin/vfdlock"

clean:
	rm -f vfdlock

.PHONY: all install clean
