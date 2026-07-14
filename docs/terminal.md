# VFD Terminal

`vfdterm` is the desktop-owned terminal entry point. Alacritty is currently the rendering backend; applications must launch `vfdterm` rather than Alacritty directly.

The profile is installed to `~/.config/vfd/terminal.toml`. This gives VFD one place to control font, palette, opacity, cursor style, and future shell integration while keeping the backend replaceable.

`vfdfetch` is a native client. It requests one `SNAPSHOT` from `vfdd`, combines it with kernel/session data, and renders the VFD system banner using the active theme accent.

## i3 integration

```i3
bindsym $mod+Return exec --no-startup-id ~/.local/bin/vfdterm
for_window [class="(?i)^vfdterm$"] border pixel 1
```

The bar and Shell use `vfdterm` after this release. Run `fc-cache -f` after installation so Fontconfig sees the monospace preference.
