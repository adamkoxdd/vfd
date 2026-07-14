# VFD Wallpaper

`vfdwall` is a focused X11 animated GIF wallpaper renderer. It creates one override-redirect desktop window per Xinerama monitor, marks each window as `_NET_WM_WINDOW_TYPE_DESKTOP`, lowers it beneath normal applications, and plays the GIF using its embedded frame delays.

## Configuration

`~/.config/vfd/wall.ini`

```ini
[wallpaper]
file = ~/Pictures/wallpapers/lain.gif
scaling = fill
fps-cap = 30
paused = false
```

Scaling modes: `fill`, `fit`, `stretch`, `center`.

## Control

- `SIGHUP`: reload configuration and GIF
- `SIGTERM`: stop

```sh
pkill -HUP vfdwall
pkill -x vfdwall
```

The initial release deliberately supports GIF only. It does not attempt Wallpaper Engine scenes, video playback, playlists, audio, or shader effects.
