# libwidgets

`libwidgets` contains reusable interaction and presentation primitives built on `libui`. Applications may compose these primitives but should not reimplement them.

## Current primitives

- `VfdLabel`: text and alignment helpers
- `VfdListView`: selection, keyboard movement, scrolling, hit testing, and drawing
- `VfdTextInput`: compact editable UTF-8 byte buffer for command/filter input
- `VfdButton`: pointer state, activation testing, and drawing
- `VfdScrollView`: generic row-based scroll state
- `VfdStatusBar`: left/center/right status layout
- `VfdContainer`: padded content and row geometry

The library is intentionally immediate-mode at this stage. Applications own widget state and call draw functions during each frame.
