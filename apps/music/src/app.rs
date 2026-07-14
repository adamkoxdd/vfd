use std::{
    env,
    io::Write,
    num::NonZeroU32,
    os::unix::net::UnixStream,
    path::PathBuf,
    time::Instant,
};

use softbuffer::{Context, Surface};
use winit::{
    application::ApplicationHandler,
    dpi::PhysicalSize,
    event::{ElementState, WindowEvent},
    event_loop::{ActiveEventLoop, ControlFlow},
    keyboard::{KeyCode, PhysicalKey},
    window::{Window, WindowAttributes},
};

use crate::{
    audio::{analyzer::{self, SpectrumAnalyzer}, engine::AudioEngine},
    display::vfd::VfdDisplay,
    framebuffer::Framebuffer,
    library::{self, Track},
    player::state::PlayerState,
    playlists,
    theme::Theme,
    widgets::{progress, spectrum, text},
};

pub struct App {
    window: Option<Window>,
    framebuffer: Framebuffer,
    analyzer: SpectrumAnalyzer,
    player: PlayerState,
    audio: Option<AudioEngine>,
    tracks: Vec<Track>,
    selected: usize,
    current: Option<usize>,
    theme_index: usize,
    last_frame: Instant,
    library_mode: bool,
    message: String,
    last_media_state: String,
}

impl App {
    pub fn new() -> Self {
        let root = library::default_music_dir();
        let tracks = library::scan(&root);
        let (analysis_sender, analysis_receiver) = analyzer::channel();
        let (audio, message) = match AudioEngine::new(analysis_sender) {
            Ok(audio) => (Some(audio), format!("{} TRACKS", tracks.len())),
            Err(error) => (None, error),
        };

        Self {
            window: None,
            framebuffer: Framebuffer::new(1, 1),
            analyzer: SpectrumAnalyzer::new(28, analysis_receiver),
            player: PlayerState::new(),
            audio,
            tracks,
            selected: 0,
            current: None,
            theme_index: 0,
            last_frame: Instant::now(),
            library_mode: true,
            message,
            last_media_state: String::new(),
        }
    }

    fn theme(&self) -> Theme {
        [
            Theme::lain(),
            Theme::green(),
            Theme::amber(),
            Theme::blue(),
            Theme::red(),
        ][self.theme_index]
    }

    fn ipc_path() -> PathBuf {
        if let Ok(runtime) = env::var("XDG_RUNTIME_DIR") {
            return PathBuf::from(runtime).join("vfdd.sock");
        }
        PathBuf::from(format!("/run/user/{}/vfdd.sock", env::var("UID").unwrap_or_else(|_| "1000".into())))
    }

    fn clean_media_field(value: &str) -> String {
        value
            .chars()
            .map(|character| match character {
                '\t' | '\n' | '\r' => ' ',
                other => other,
            })
            .collect()
    }

    fn publish_media(&mut self) {
        let status = if self.current.is_none() {
            "stopped"
        } else if self.player.playing {
            "playing"
        } else {
            "paused"
        };
        let artist = Self::clean_media_field(&self.player.artist);
        let title = Self::clean_media_field(&self.player.title);
        let state = format!("{status}\t{artist}\t{title}");
        if state == self.last_media_state {
            return;
        }
        self.last_media_state = state.clone();
        if let Ok(mut stream) = UnixStream::connect(Self::ipc_path()) {
            let _ = stream.write_all(format!("MEDIA SET\t{state}\n").as_bytes());
        }
    }

    fn play_index(&mut self, index: usize) {
        let Some(track) = self.tracks.get(index).cloned() else {
            return;
        };
        let Some(audio) = self.audio.as_mut() else {
            self.message = "NO AUDIO OUTPUT".into();
            return;
        };

        match audio.load(&track.path) {
            Ok(()) => {
                self.current = Some(index);
                self.selected = index;
                self.player.set_track(track.title, track.artist);
                self.library_mode = false;
                self.message = "LOCAL PLAYBACK".into();
                self.publish_media();
            }
            Err(error) => self.message = error,
        }
    }

    fn next(&mut self, delta: isize) {
        if self.tracks.is_empty() {
            return;
        }

        let base = self.current.unwrap_or(self.selected) as isize;
        let len = self.tracks.len() as isize;
        self.play_index((base + delta).rem_euclid(len) as usize);
    }

    fn update_state(&mut self) {
        let mut advance = false;
        if let Some(audio) = &self.audio {
            self.player
                .update(audio.playing(), audio.position(), audio.duration());
            advance = self.current.is_some() && audio.finished();
        }
        if advance {
            self.next(1);
        } else {
            self.publish_media();
        }
    }

    fn draw_library(&mut self, theme: Theme, width: u32, height: u32) {
        text::draw(
            &mut self.framebuffer,
            "LIBRARY",
            34,
            30,
            2,
            theme.phosphor,
        );
        text::draw(
            &mut self.framebuffer,
            &self.message,
            36,
            54,
            1,
            theme.phosphor.scale(0.55),
        );

        let rows = ((height.saturating_sub(115)) / 18).max(1) as usize;
        let start = self
            .selected
            .saturating_sub(rows / 2)
            .min(self.tracks.len().saturating_sub(rows));

        for (row, track) in self.tracks.iter().skip(start).take(rows).enumerate() {
            let index = start + row;
            let y = 82 + row as i32 * 18;

            if index == self.selected {
                self.framebuffer.fill_rect(
                    28,
                    y - 4,
                    width.saturating_sub(56),
                    15,
                    theme.border.scale(0.45),
                );
            }

            let marker = if Some(index) == self.current {
                "PLAY"
            } else if index == self.selected {
                "SEL"
            } else {
                ""
            };

            text::draw(
                &mut self.framebuffer,
                marker,
                34,
                y,
                1,
                theme.phosphor,
            );
            text::draw(
                &mut self.framebuffer,
                &text::truncate(&track.title, 42),
                76,
                y,
                1,
                theme
                    .phosphor
                    .scale(if index == self.selected { 1.0 } else { 0.68 }),
            );
        }

        text::draw(
            &mut self.framebuffer,
            "ENTER PLAY  SPACE PAUSE  A FAVORITE  L PLAYER",
            34,
            height as i32 - 32,
            1,
            theme.phosphor.scale(0.55),
        );
    }

    fn draw_player(&mut self, theme: Theme, width: u32, height: u32) {
        text::draw(
            &mut self.framebuffer,
            &text::truncate(&self.player.title, 48),
            34,
            34,
            2,
            theme.phosphor,
        );
        text::draw(
            &mut self.framebuffer,
            &text::truncate(&self.player.artist, 54),
            36,
            55,
            1,
            theme.phosphor.scale(0.55),
        );
        text::draw(
            &mut self.framebuffer,
            theme.name,
            width as i32 - 86,
            28,
            1,
            theme.phosphor.scale(0.65),
        );
        text::draw(
            &mut self.framebuffer,
            if self.player.playing { "PLAY" } else { "PAUSE" },
            35,
            height as i32 - 40,
            1,
            theme.phosphor,
        );

        VfdDisplay::lamp(
            &mut self.framebuffer,
            20,
            height as i32 - 40,
            self.player.playing,
            theme,
        );

        spectrum::draw(
            &mut self.framebuffer,
            self.analyzer.bands(),
            34,
            height as i32 - 64,
            width - 68,
            height.saturating_sub(145),
            theme,
        );
        progress::draw(
            &mut self.framebuffer,
            34,
            height as i32 - 23,
            width - 68,
            self.player.progress,
            theme,
        );

        let elapsed = self.player.elapsed.as_secs();
        let duration = self.player.duration.map(|value| value.as_secs()).unwrap_or(0);
        text::draw(
            &mut self.framebuffer,
            &format!(
                "{:02}:{:02}  {:02}:{:02}",
                elapsed / 60,
                elapsed % 60,
                duration / 60,
                duration % 60
            ),
            width as i32 - 168,
            height as i32 - 40,
            1,
            theme.phosphor.scale(0.65),
        );
    }

    fn draw(&mut self) {
        let theme = self.theme();
        VfdDisplay::panel(&mut self.framebuffer, theme);

        let width = self.framebuffer.width();
        let height = self.framebuffer.height();
        if width < 360 || height < 180 {
            return;
        }

        if self.library_mode {
            self.draw_library(theme, width, height);
        } else {
            self.draw_player(theme, width, height);
        }
    }

    fn redraw(&mut self) {
        let now = Instant::now();
        let delta = (now - self.last_frame).as_secs_f32().min(0.1);
        self.last_frame = now;

        self.analyzer.update(delta);
        self.update_state();

        let size = match self.window.as_ref() {
            Some(window) => window.inner_size(),
            None => return,
        };
        let (width, height) = (size.width.max(1), size.height.max(1));

        self.framebuffer.resize(width, height);
        self.draw();

        let Some(window) = self.window.as_ref() else {
            return;
        };
        let context = Context::new(window).expect("softbuffer context");
        let mut surface = Surface::new(&context, window).expect("softbuffer surface");
        surface
            .resize(
                NonZeroU32::new(width).unwrap(),
                NonZeroU32::new(height).unwrap(),
            )
            .expect("surface resize");

        let mut buffer = surface.buffer_mut().expect("surface buffer");
        buffer.copy_from_slice(self.framebuffer.pixels());
        buffer.present().expect("present");
    }

    fn favorite(&mut self) {
        if let Some(track) = self.tracks.get(self.selected) {
            match playlists::append_favorite(track) {
                Ok(path) => {
                    self.message = format!(
                        "SAVED {}",
                        path.file_name()
                            .and_then(|name| name.to_str())
                            .unwrap_or("PLAYLIST")
                    );
                }
                Err(error) => self.message = format!("PLAYLIST ERROR {error}"),
            }
        }
    }
}

impl ApplicationHandler for App {
    fn resumed(&mut self, event_loop: &ActiveEventLoop) {
        event_loop.set_control_flow(ControlFlow::Poll);
        let window = event_loop
            .create_window(
                WindowAttributes::default()
                    .with_title("VFD Music")
                    .with_inner_size(PhysicalSize::new(940, 420))
                    .with_min_inner_size(PhysicalSize::new(520, 260)),
            )
            .expect("window");
        self.window = Some(window);
    }

    fn window_event(
        &mut self,
        event_loop: &ActiveEventLoop,
        _window_id: winit::window::WindowId,
        event: WindowEvent,
    ) {
        match event {
            WindowEvent::CloseRequested => event_loop.exit(),
            WindowEvent::Resized(_) => {
                if let Some(window) = &self.window {
                    window.request_redraw();
                }
            }
            WindowEvent::RedrawRequested => self.redraw(),
            WindowEvent::KeyboardInput { event, .. }
                if event.state == ElementState::Pressed =>
            {
                match event.physical_key {
                    PhysicalKey::Code(KeyCode::Escape) => event_loop.exit(),
                    PhysicalKey::Code(KeyCode::Space) => {
                        if let Some(audio) = &self.audio {
                            audio.toggle();
                        }
                    }
                    PhysicalKey::Code(KeyCode::ArrowDown)
                    | PhysicalKey::Code(KeyCode::KeyJ) => {
                        if !self.tracks.is_empty() {
                            self.selected = (self.selected + 1).min(self.tracks.len() - 1);
                        }
                    }
                    PhysicalKey::Code(KeyCode::ArrowUp)
                    | PhysicalKey::Code(KeyCode::KeyK) => {
                        self.selected = self.selected.saturating_sub(1);
                    }
                    PhysicalKey::Code(KeyCode::Enter) => self.play_index(self.selected),
                    PhysicalKey::Code(KeyCode::KeyN) => self.next(1),
                    PhysicalKey::Code(KeyCode::KeyP) => self.next(-1),
                    PhysicalKey::Code(KeyCode::ArrowRight) => {
                        if let Some(audio) = &self.audio {
                            audio.seek_relative(10);
                        }
                    }
                    PhysicalKey::Code(KeyCode::ArrowLeft) => {
                        if let Some(audio) = &self.audio {
                            audio.seek_relative(-10);
                        }
                    }
                    PhysicalKey::Code(KeyCode::Equal)
                    | PhysicalKey::Code(KeyCode::NumpadAdd) => {
                        if let Some(audio) = &self.audio {
                            audio.change_volume(0.05);
                        }
                    }
                    PhysicalKey::Code(KeyCode::Minus)
                    | PhysicalKey::Code(KeyCode::NumpadSubtract) => {
                        if let Some(audio) = &self.audio {
                            audio.change_volume(-0.05);
                        }
                    }
                    PhysicalKey::Code(KeyCode::KeyL) => {
                        self.library_mode = !self.library_mode;
                    }
                    PhysicalKey::Code(KeyCode::KeyA) => self.favorite(),
                    PhysicalKey::Code(KeyCode::KeyR) => {
                        self.tracks = library::scan(&library::default_music_dir());
                        self.selected = self
                            .selected
                            .min(self.tracks.len().saturating_sub(1));
                        self.message = format!("{} TRACKS", self.tracks.len());
                    }
                    PhysicalKey::Code(KeyCode::Digit1) => self.theme_index = 0,
                    PhysicalKey::Code(KeyCode::Digit2) => self.theme_index = 1,
                    PhysicalKey::Code(KeyCode::Digit3) => self.theme_index = 2,
                    PhysicalKey::Code(KeyCode::Digit4) => self.theme_index = 3,
                    PhysicalKey::Code(KeyCode::Digit5) => self.theme_index = 4,
                    PhysicalKey::Code(KeyCode::KeyT) => {
                        self.theme_index = (self.theme_index + 1) % 5;
                    }
                    _ => {}
                }

                if let Some(window) = &self.window {
                    window.request_redraw();
                }
            }
            _ => {}
        }
    }

    fn about_to_wait(&mut self, _: &ActiveEventLoop) {
        if let Some(window) = &self.window {
            window.request_redraw();
        }
    }
}
