# VFD IPC protocol

Unix stream socket: `$XDG_RUNTIME_DIR/vfdd.sock`.

Requests are newline-terminated ASCII:
- `PING`
- `STATUS`
- `SNAPSHOT`
- `GET cpu`
- `GET memory`
- `GET battery`
- `GET clock`
- `GET theme`
- `SHUTDOWN`

Version 1 deliberately stays human-readable and debuggable with `socat`.
