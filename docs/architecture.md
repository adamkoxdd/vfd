# Architecture

i3 owns windows and key bindings. `vfdd` owns desktop state. Applications render state and issue commands through shared libraries.

The first milestone avoids plugins and subscriptions until the request/response protocol and shared libraries stabilize.
