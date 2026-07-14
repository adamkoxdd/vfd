# VFD 1.1.1.0

VFD desktop runtime foundation with a shared X11/Xft UI backend and a graphical daemon-driven bar.

## Components

- `vfdd`: desktop state daemon
- `vfdctl`: daemon control client
- `vfdbar`: graphical multi-monitor bar
- `libipc`: Unix socket requests
- `libtheme`: shared color theme
- `libconfig`: INI reader
- `libutil`: common path helpers
- `libui`: the only component that directly includes Xlib/Xft

## Dependencies (Arch)

```sh
sudo pacman -S --needed base-devel libx11 libxft libxinerama fontconfig pkgconf
```

## Build and install

```sh
make clean
make
make install
```

The graphical bar is installed as `~/.local/bin/vfdbar`. Back up an older binary first if desired.

## i3

```i3
exec_always --no-startup-id pkill -x vfdd
exec_always --no-startup-id ~/.local/bin/vfdd
exec_always --no-startup-id pkill -x vfdbar
exec_always --no-startup-id sh -c 'sleep 0.3; exec ~/.local/bin/vfdbar'
```

## Layout

`FILES TERM WEB MUSIC NVIM DISCORD` on the left, daemon clock centered, and daemon CPU/MEM/BAT values on the right.

The bar uses one X11 dock window per Xinerama monitor and clickable launcher labels.

## 1.1.1 fix

- Graphical bar now requests one `SNAPSHOT` per update and parses live daemon values.
- IPC socket discovery falls back to `/run/user/$UID/vfdd.sock` when `XDG_RUNTIME_DIR` is unavailable (common in some i3 startup environments).
- Disconnected state is visibly dimmed instead of silently looking live.

## Foundation 0.2 additions

The `Files` application now lives under `apps/files` and uses `libui`, `libtheme`, and the new `libwidgets` library. The working bar remains unchanged.

Run Files with:

```sh
vfdfm
vfdfm ~/Projects
```


## Foundation 0.3 additions

This milestone adds shared button, scroll-state, status-bar, aligned-label, list hit-testing, and container primitives. It also adds VISION.md, CONTRIBUTING.md, STYLE.md, and an updated roadmap. The daemon, graphical bar, and Files behavior remain unchanged.

## Launcher

`vfdshell` is the first VFD command palette with nested desktop actions. It scans XDG desktop entries, merges them with VFD desktop commands, and provides fuzzy keyboard search.

```sh
vfdshell
```

Recommended i3 binding:

```i3
bindsym $mod+space exec --no-startup-id ~/.local/bin/vfdshell
for_window [class="(?i)^vfdshell$"] floating enable, border pixel 1
```


## Control Center

Run `vfdsettings` to open Appearance, Components, Keybindings and About.

`vfdlauncher` is retained as a compatibility symlink to `vfdshell`.

## Terminal identity

VFD includes `vfdterm`, a stable VFD entry point backed by Alacritty, and `vfdfetch`, a native daemon-aware system banner.

```sh
vfdterm
vfdterm nvim
vfdfetch
```

Install `alacritty`, `fastfetch` (optional), and `terminus-font`. The installed Fontconfig rule changes the preferred monospace family only; it does not force websites or proportional GUI text to use Terminus.

## VFD Events 0.8

This milestone adds `vfdnotify`, a native notification service implementing the freedesktop D-Bus interface. Notifications are stored in `vfdd` and exposed in VFD Shell under **Events**.

## VFD Events 0.9

The notification service now supports stacked notifications, replacement IDs,
urgency-aware timeouts, standard close/action D-Bus signals, unread state, and
a bar indicator. Opening the Events view in VFD Shell marks events as read while
preserving history.

## Animated GIF wallpaper

VFD now includes `vfdwall`, a small native GIF wallpaper renderer using X11, Xinerama, and giflib. Configure it in `~/.config/vfd/wall.ini`, launch it with `vfdwall`, and reload it with `pkill -HUP vfdwall`.
