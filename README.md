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
