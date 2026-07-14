# VFD Events

`vfdnotify` owns the standard `org.freedesktop.Notifications` D-Bus name. Incoming notifications are rendered with libui and mirrored into `vfdd` history.

## Commands

- `vfdctl events` — list recent events
- `vfdctl events clear` — clear history
- `notify-send "Title" "Body"` — test the service

## Startup

Start `vfdd` before `vfdnotify` so events can be stored. The popup still works if the daemon is temporarily unavailable.

## 0.9 notification behavior

- Up to five notifications stack on the primary monitor.
- `replaces_id` updates an existing notification.
- Low, normal, and critical urgency use different timeout behavior.
- Critical notifications remain until dismissed.
- The first action can be invoked with Enter or Space.
- `NotificationClosed` and `ActionInvoked` D-Bus signals are emitted.
- `EVENTS READ` clears unread state without deleting history.
- The bar displays `EVENTS N` while unread events exist.
