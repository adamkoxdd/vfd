CC ?= cc
AR ?= ar
PKG_CONFIG ?= pkg-config
PREFIX ?= /usr/local
LIBDIR ?= $(PREFIX)/lib
INCLUDEDIR ?= $(PREFIX)/include
PKGCONFIGDIR ?= $(LIBDIR)/pkgconfig
CPPFLAGS += -Iinclude $(shell $(PKG_CONFIG) --cflags x11 cairo pangocairo)
CFLAGS ?= -O2 -pipe -Wall -Wextra -Wpedantic -std=c11
LDLIBS += $(shell $(PKG_CONFIG) --libs x11 cairo pangocairo) -lm
BUILD := build
LIB := $(BUILD)/libvfdui.a
OBJ := $(BUILD)/vfdui.o
DEMO := $(BUILD)/vfdui-demo
all: $(LIB) $(DEMO)
$(BUILD): ; mkdir -p $(BUILD)
$(OBJ): src/vfdui.c include/vfdui/vfdui.h | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) -fPIC -c $< -o $@
$(LIB): $(OBJ)
	$(AR) rcs $@ $^
$(DEMO): examples/demo.c $(LIB)
	$(CC) $(CPPFLAGS) $(CFLAGS) $< $(LIB) $(LDLIBS) -o $@
install: $(LIB)
	install -Dm644 $(LIB) "$(DESTDIR)$(LIBDIR)/libvfdui.a"
	install -Dm644 include/vfdui/vfdui.h "$(DESTDIR)$(INCLUDEDIR)/vfdui/vfdui.h"
	install -Dm644 vfdui.pc "$(DESTDIR)$(PKGCONFIGDIR)/vfdui.pc"
demo: $(DEMO)
	./$(DEMO)
clean:
	rm -rf $(BUILD)
.PHONY: all install demo clean
