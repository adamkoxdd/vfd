# Contributing

## Local workflow

Create a branch for each focused change:

```sh
git switch -c widgets/button-focus
make clean && make
```

Keep the working bar and daemon stable. New widget or Files work should not change bar behavior unless the commit explicitly targets the bar.

## Commit style

Use concise conventional prefixes:

- `feat(files): add rename prompt`
- `feat(widgets): add button primitive`
- `fix(ipc): reconnect after daemon restart`
- `refactor(ui): centralize font metrics`
- `docs: describe widget ownership`

## Architecture rules

- Applications must not include Xlib/Xft headers.
- Shared desktop state belongs in `vfdd`.
- Reusable interaction and drawing behavior belongs in `libwidgets`.
- Display-backend details belong in `libui`.
- Avoid adding dependencies when the needed behavior is small and maintainable locally.
