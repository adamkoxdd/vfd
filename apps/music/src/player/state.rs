use std::time::Duration;

pub struct PlayerState {
    pub playing: bool,
    pub progress: f32,
    pub title: String,
    pub artist: String,
    pub elapsed: Duration,
    pub duration: Option<Duration>,
    pub status: String,
}

impl PlayerState {
    pub fn new() -> Self {
        Self { playing: false, progress: 0.0, title: "NO TRACK SELECTED".into(), artist: "SCAN ~/MUSIC".into(), elapsed: Duration::ZERO, duration: None, status: "LIBRARY".into() }
    }
    pub fn set_track(&mut self, title: String, artist: String) { self.title = title; self.artist = artist; self.status = "PLAYING".into(); }
    pub fn update(&mut self, playing: bool, elapsed: Duration, duration: Option<Duration>) {
        self.playing = playing; self.elapsed = elapsed; self.duration = duration;
        self.progress = duration.filter(|d| !d.is_zero()).map(|d| (elapsed.as_secs_f32() / d.as_secs_f32()).clamp(0.0, 1.0)).unwrap_or(0.0);
    }
}
