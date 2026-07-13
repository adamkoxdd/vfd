CC ?= cc
CFLAGS ?= -O2 -pipe -std=c17 -Wall -Wextra -Wpedantic
CPPFLAGS += -Iinclude $(shell pkg-config --cflags xft xinerama 2>/dev/null)
BUILD=build
CORELIBSRC=libs/libutil/util.c libs/libconfig/config.c libs/libtheme/theme.c libs/libipc/ipc.c
CORELIBOBJ=$(CORELIBSRC:%.c=$(BUILD)/%.o)
UIOBJ=$(BUILD)/libs/libui/ui.o
WIDGETOBJ=$(BUILD)/libs/libwidgets/widgets.o
all: $(BUILD)/vfdd $(BUILD)/vfdctl $(BUILD)/vfdbar $(BUILD)/vfdfm
$(BUILD)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@
$(BUILD)/vfdd: $(CORELIBOBJ) $(BUILD)/apps/vfdd/main.o
	$(CC) $^ -o $@
$(BUILD)/vfdctl: $(CORELIBOBJ) $(BUILD)/apps/vfdd/vfdctl.o
	$(CC) $^ -o $@
$(BUILD)/vfdbar: $(CORELIBOBJ) $(UIOBJ) $(BUILD)/apps/vfdbar/main.o
	$(CC) $^ -o $@ $(shell pkg-config --libs xft xinerama 2>/dev/null) -lX11
$(BUILD)/vfdfm: $(CORELIBOBJ) $(UIOBJ) $(WIDGETOBJ) $(BUILD)/apps/files/main.o
	$(CC) $^ -o $@ $(shell pkg-config --libs xft xinerama 2>/dev/null) -lX11
install: all
	install -Dm755 $(BUILD)/vfdd $(HOME)/.local/bin/vfdd
	install -Dm755 $(BUILD)/vfdctl $(HOME)/.local/bin/vfdctl
	install -Dm755 $(BUILD)/vfdbar $(HOME)/.local/bin/vfdbar
	install -Dm755 $(BUILD)/vfdfm $(HOME)/.local/bin/vfdfm
	install -Dm644 themes/lain/theme.ini $(HOME)/.config/vfd/themes/lain/theme.ini
clean:
	rm -rf $(BUILD)
.PHONY: all install clean
