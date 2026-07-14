use std::{fs, io::{self, Write}, path::{Path, PathBuf}};
use crate::library::Track;

pub fn playlist_dir() -> PathBuf {
    let base = std::env::var_os("XDG_CONFIG_HOME").map(PathBuf::from).or_else(|| std::env::var_os("HOME").map(|h| PathBuf::from(h).join(".config"))).unwrap_or_else(|| PathBuf::from("."));
    base.join("vfd/music/playlists")
}

pub fn append_favorite(track: &Track) -> io::Result<PathBuf> {
    let dir = playlist_dir();
    fs::create_dir_all(&dir)?;
    let path = dir.join("favorites.m3u");
    let existing = fs::read_to_string(&path).unwrap_or_default();
    let value = track.path.to_string_lossy();
    if !existing.lines().any(|line| line == value) {
        let mut file = fs::OpenOptions::new().create(true).append(true).open(&path)?;
        writeln!(file, "{}", value)?;
    }
    Ok(path)
}

pub fn load(path: &Path) -> Vec<PathBuf> {
    fs::read_to_string(path).unwrap_or_default().lines().map(str::trim).filter(|line| !line.is_empty() && !line.starts_with('#')).map(PathBuf::from).filter(|path| path.exists()).collect()
}
