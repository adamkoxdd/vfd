# VFD Vision

VFD is a lightweight desktop runtime built around small native applications.

It is not a full desktop environment and does not replace the window manager. It supplies a coherent layer above a window manager through:

- one state daemon;
- one IPC protocol;
- one native UI backend;
- one widget library;
- one theme system;
- one design language.

## Principles

1. `vfdd` owns shared desktop state.
2. Applications are clients and render only what they need.
3. Only `libui` talks directly to X11 or future display backends.
4. Shared behavior belongs in a library, not copied between applications.
5. Components remain replaceable and independently useful.
6. Dependencies stay small, explicit, and justified.
7. The desktop must remain fast, predictable, keyboard-friendly, and understandable.
