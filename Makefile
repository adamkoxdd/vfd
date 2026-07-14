CC ?= cc
CFLAGS ?= -O2 -pipe -std=c17 -Wall -Wextra -Wpedantic
CPPFLAGS += -Iinclude $(shell pkg-config --cflags xft xinerama dbus-1 2>/dev/null)
BUILD=build
CORELIBSRC=libs/libutil/util.c libs/libconfig/config.c libs/libtheme/theme.c libs/libipc/ipc.c
CORELIBOBJ=$(CORELIBSRC:%.c=$(BUILD)/%.o)
UIOBJ=$(BUILD)/libs/libui/ui.o
WIDGETOBJ=$(BUILD)/libs/libwidgets/widgets.o
all: $(BUILD)/vfdd $(BUILD)/vfdctl $(BUILD)/vfdbar $(BUILD)/vfdfm $(BUILD)/vfdshell $(BUILD)/vfdsettings $(BUILD)/vfdfetch $(BUILD)/vfdnotify
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
$(BUILD)/vfdshell: $(CORELIBOBJ) $(UIOBJ) $(WIDGETOBJ) $(BUILD)/apps/launcher/shell.o
	$(CC) $^ -o $@ $(shell pkg-config --libs xft xinerama 2>/dev/null) -lX11
$(BUILD)/vfdsettings: $(CORELIBOBJ) $(UIOBJ) $(WIDGETOBJ) $(BUILD)/apps/settings/main.o
	$(CC) $^ -o $@ $(shell pkg-config --libs xft xinerama 2>/dev/null) -lX11
$(BUILD)/vfdfetch: $(CORELIBOBJ) $(BUILD)/apps/fetch/main.o
	$(CC) $^ -o $@
$(BUILD)/vfdnotify: $(CORELIBOBJ) $(UIOBJ) $(BUILD)/services/notifyd/main.o
	$(CC) $^ -o $@ $(shell pkg-config --libs xft xinerama dbus-1 2>/dev/null) -lX11
install: all
	install -Dm755 $(BUILD)/vfdd $(HOME)/.local/bin/vfdd
	install -Dm755 $(BUILD)/vfdctl $(HOME)/.local/bin/vfdctl
	install -Dm755 $(BUILD)/vfdbar $(HOME)/.local/bin/vfdbar
	install -Dm755 $(BUILD)/vfdfm $(HOME)/.local/bin/vfdfm
	install -Dm755 $(BUILD)/vfdshell $(HOME)/.local/bin/vfdshell
	ln -sf vfdshell $(HOME)/.local/bin/vfdlauncher
	install -Dm755 $(BUILD)/vfdsettings $(HOME)/.local/bin/vfdsettings
	install -Dm755 $(BUILD)/vfdfetch $(HOME)/.local/bin/vfdfetch
	install -Dm755 $(BUILD)/vfdnotify $(HOME)/.local/bin/vfdnotify
	install -Dm755 apps/terminal/vfdterm $(HOME)/.local/bin/vfdterm
	install -Dm644 apps/launcher/vfdshell.desktop $(HOME)/.local/share/applications/vfdshell.desktop
	install -Dm644 apps/settings/vfdsettings.desktop $(HOME)/.local/share/applications/vfdsettings.desktop
	install -Dm644 apps/terminal/vfdterm.desktop $(HOME)/.local/share/applications/vfdterm.desktop
	install -Dm644 apps/fetch/vfdfetch.desktop $(HOME)/.local/share/applications/vfdfetch.desktop
	install -Dm644 apps/terminal/terminal.toml $(HOME)/.config/vfd/terminal.toml
	install -Dm644 config/fontconfig/99-vfd-fonts.conf $(HOME)/.config/fontconfig/conf.d/99-vfd-fonts.conf
	install -Dm644 themes/lain/theme.ini $(HOME)/.config/vfd/themes/lain/theme.ini
clean:
	rm -rf $(BUILD)
.PHONY: all install clean
