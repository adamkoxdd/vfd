# VFD Music 0.2

A local-first VFD-style music player for Linux, evolved from the original visualizer prototype.

## Current milestone

- Scans `~/Music` recursively (`VFD_MUSIC_DIR` overrides it)
- Plays MP3, FLAC, OGG/Opus, WAV, AAC and M4A through rodio/Symphonia
- Library browser with Vim-style navigation
- Play/pause, next/previous, seek and volume
- Adds tracks to `~/.config/vfd/music/playlists/favorites.m3u`
- Preserves the software framebuffer, bitmap text, phosphor glow and animated spectrum

The spectrum remains synthetic in 0.2. A real FFT fed by decoded PCM is the next audio milestone.

## Arch dependencies

```bash
sudo pacman -S --needed rust cargo alsa-lib pkgconf
```

## Build

```bash
cargo build --release
install -Dm755 target/release/vfdmusic ~/.local/bin/vfdmusic
```

## Music layout

A simple layout works well:

```text
~/Music/
  Artist/
    Album/
      01 - Track.flac
```

Until tag parsing is added, the file name becomes the title and the parent directory becomes the artist label.

## Controls

- `J/K`, arrows: select track
- `Enter`: play selected
- `Space`: play/pause
- `N/P`: next/previous
- Left/Right: seek 10 seconds
- `+/-`: volume
- `L`: toggle library/player view
- `A`: add selected track to Favorites
- `R`: rescan library
- `T`, `1`–`5`: theme
- Escape: quit
