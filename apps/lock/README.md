# vfdlock

VFD lock-screen compositor written in C.

It renders a native PNG using Cairo/Pango, then hands control to `i3lock`.
`vfdlock` never reads or validates passwords; PAM authentication remains inside
the audited locker.

## Build

```sh
make
make install
```

Dependencies: `i3lock cairo pango libx11 pkgconf`.
