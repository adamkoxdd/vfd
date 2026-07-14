use std::{
    fs::File,
    path::Path,
    sync::mpsc::SyncSender,
    time::Duration,
};

use rodio::{
    ChannelCount, Decoder, DeviceSinkBuilder, MixerDeviceSink, Player, SampleRate, Source,
};

use super::analyzer::AnalysisFrame;

const ANALYSIS_CHUNK_FRAMES: usize = 256;

struct AnalysisSource<S> {
    inner: S,
    sender: SyncSender<AnalysisFrame>,
    channels: usize,
    frame_sum: f32,
    frame_channel: usize,
    chunk: Vec<f32>,
}

impl<S> AnalysisSource<S>
where
    S: Source<Item = f32>,
{
    fn new(inner: S, sender: SyncSender<AnalysisFrame>) -> Self {
        let channels = inner.channels().get() as usize;
        Self {
            inner,
            sender,
            channels: channels.max(1),
            frame_sum: 0.0,
            frame_channel: 0,
            chunk: Vec::with_capacity(ANALYSIS_CHUNK_FRAMES),
        }
    }
}

impl<S> Iterator for AnalysisSource<S>
where
    S: Source<Item = f32>,
{
    type Item = f32;

    fn next(&mut self) -> Option<Self::Item> {
        let sample = self.inner.next()?;
        self.frame_sum += sample;
        self.frame_channel += 1;

        if self.frame_channel >= self.channels {
            self.chunk.push(self.frame_sum / self.channels as f32);
            self.frame_sum = 0.0;
            self.frame_channel = 0;

            if self.chunk.len() >= ANALYSIS_CHUNK_FRAMES {
                let samples = std::mem::replace(
                    &mut self.chunk,
                    Vec::with_capacity(ANALYSIS_CHUNK_FRAMES),
                );
                let _ = self.sender.try_send(AnalysisFrame {
                    samples,
                    sample_rate: self.inner.sample_rate().get(),
                });
            }
        }

        Some(sample)
    }

    fn size_hint(&self) -> (usize, Option<usize>) {
        self.inner.size_hint()
    }
}

impl<S> Source for AnalysisSource<S>
where
    S: Source<Item = f32>,
{
    fn current_span_len(&self) -> Option<usize> {
        self.inner.current_span_len()
    }

    fn channels(&self) -> ChannelCount {
        self.inner.channels()
    }

    fn sample_rate(&self) -> SampleRate {
        self.inner.sample_rate()
    }

    fn total_duration(&self) -> Option<Duration> {
        self.inner.total_duration()
    }
}

pub struct AudioEngine {
    _device: MixerDeviceSink,
    player: Player,
    duration: Option<Duration>,
    analysis_sender: SyncSender<AnalysisFrame>,
}

impl AudioEngine {
    pub fn new(analysis_sender: SyncSender<AnalysisFrame>) -> Result<Self, String> {
        let device = DeviceSinkBuilder::open_default_sink()
            .map_err(|error| format!("audio output: {error}"))?;
        let player = Player::connect_new(device.mixer());
        player.set_volume(0.75);
        Ok(Self {
            _device: device,
            player,
            duration: None,
            analysis_sender,
        })
    }

    pub fn load(&mut self, path: &Path) -> Result<(), String> {
        let file = File::open(path).map_err(|error| format!("open {}: {error}", path.display()))?;
        let source = Decoder::try_from(file)
            .map_err(|error| format!("decode {}: {error}", path.display()))?;
        self.duration = source.total_duration();
        self.player.clear();
        self.player
            .append(AnalysisSource::new(source, self.analysis_sender.clone()));
        self.player.play();
        Ok(())
    }

    pub fn toggle(&self) {
        if self.player.is_paused() {
            self.player.play();
        } else {
            self.player.pause();
        }
    }

    pub fn playing(&self) -> bool {
        !self.player.is_paused() && !self.player.empty()
    }

    pub fn finished(&self) -> bool {
        self.player.empty()
    }

    pub fn position(&self) -> Duration {
        self.player.get_pos()
    }

    pub fn duration(&self) -> Option<Duration> {
        self.duration
    }

    pub fn seek_relative(&self, seconds: i64) {
        let current = self.position().as_secs() as i64;
        let target = (current + seconds).max(0) as u64;
        let _ = self.player.try_seek(Duration::from_secs(target));
    }

    pub fn volume(&self) -> f32 {
        self.player.volume()
    }

    pub fn change_volume(&self, delta: f32) {
        self.player
            .set_volume((self.player.volume() + delta).clamp(0.0, 1.5));
    }
}
