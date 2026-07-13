# libui MVP

`libui` owns X11 and Xft. Applications use the public API in `include/vfd/ui.h` for windows, colors, text, rectangles, monitor enumeration, and events.

Current rule: application sources must not include Xlib or Xft directly.
