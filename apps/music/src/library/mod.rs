use std::{fs, path::{Path, PathBuf}};

#[derive(Clone, Debug)]
pub struct Track {
    pub path: PathBuf,
    pub title: String,
    pub artist: String,
}

impl Track {
    fn from_path(path: PathBuf) -> Self {
        let title = path.file_stem().and_then(|s| s.to_str()).unwrap_or("UNKNOWN").replace(['_', '-'], " ");
        let artist = path.parent().and_then(Path::file_name).and_then(|s| s.to_str()).unwrap_or("LOCAL FILE").replace(['_', '-'], " ");
        Self { path, title, artist }
    }
}

pub fn default_music_dir() -> PathBuf {
    std::env::var_os("VFD_MUSIC_DIR").map(PathBuf::from).or_else(|| {
        std::env::var_os("HOME").map(|home| PathBuf::from(home).join("Music"))
    }).unwrap_or_else(|| PathBuf::from("."))
}

pub fn scan(root: &Path) -> Vec<Track> {
    let mut tracks = Vec::new();
    scan_dir(root, &mut tracks);
    tracks.sort_by(|a, b| a.artist.to_lowercase().cmp(&b.artist.to_lowercase()).then(a.title.to_lowercase().cmp(&b.title.to_lowercase())));
    tracks
}

fn scan_dir(dir: &Path, tracks: &mut Vec<Track>) {
    let Ok(entries) = fs::read_dir(dir) else { return; };
    for entry in entries.flatten() {
        let path = entry.path();
        if path.is_dir() { scan_dir(&path, tracks); continue; }
        let supported = path.extension().and_then(|s| s.to_str()).map(|s| matches!(s.to_ascii_lowercase().as_str(), "mp3" | "flac" | "ogg" | "oga" | "wav" | "m4a" | "aac" | "opus")).unwrap_or(false);
        if supported { tracks.push(Track::from_path(path)); }
    }
}
