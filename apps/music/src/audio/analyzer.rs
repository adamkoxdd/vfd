use std::{
    collections::VecDeque,
    sync::mpsc::{sync_channel, Receiver, SyncSender},
};

use rustfft::{num_complex::Complex32, FftPlanner};

const FFT_SIZE: usize = 2048;
const HISTORY_SIZE: usize = FFT_SIZE * 2;

#[derive(Debug)]
pub struct AnalysisFrame {
    pub samples: Vec<f32>,
    pub sample_rate: u32,
}

pub fn channel() -> (SyncSender<AnalysisFrame>, Receiver<AnalysisFrame>) {
    sync_channel(8)
}

pub struct SpectrumAnalyzer {
    receiver: Receiver<AnalysisFrame>,
    history: VecDeque<f32>,
    bands: Vec<f32>,
    sample_rate: u32,
    fft: std::sync::Arc<dyn rustfft::Fft<f32>>,
    fft_input: Vec<Complex32>,
}

impl SpectrumAnalyzer {
    pub fn new(count: usize, receiver: Receiver<AnalysisFrame>) -> Self {
        let mut planner = FftPlanner::<f32>::new();
        let fft = planner.plan_fft_forward(FFT_SIZE);
        Self {
            receiver,
            history: VecDeque::with_capacity(HISTORY_SIZE),
            bands: vec![0.0; count],
            sample_rate: 48_000,
            fft,
            fft_input: vec![Complex32::new(0.0, 0.0); FFT_SIZE],
        }
    }

    pub fn update(&mut self, dt: f32) {
        let mut received_audio = false;
        while let Ok(frame) = self.receiver.try_recv() {
            self.sample_rate = frame.sample_rate.max(1);
            for sample in frame.samples {
                if self.history.len() == HISTORY_SIZE {
                    self.history.pop_front();
                }
                self.history.push_back(sample);
            }
            received_audio = true;
        }

        if self.history.len() < FFT_SIZE {
            self.decay(dt);
            return;
        }

        let start = self.history.len() - FFT_SIZE;
        for (index, sample) in self.history.iter().skip(start).enumerate() {
            let phase = index as f32 / (FFT_SIZE - 1) as f32;
            let window = 0.5 - 0.5 * (std::f32::consts::TAU * phase).cos();
            self.fft_input[index] = Complex32::new(sample * window, 0.0);
        }
        self.fft.process(&mut self.fft_input);

        let nyquist = self.sample_rate as f32 * 0.5;
        let low_hz = 35.0_f32;
        let high_hz = nyquist.min(18_000.0).max(low_hz * 2.0);
        let band_count = self.bands.len();

        for band_index in 0..band_count {
            let low_t = band_index as f32 / band_count as f32;
            let high_t = (band_index + 1) as f32 / band_count as f32;
            let band_low = low_hz * (high_hz / low_hz).powf(low_t);
            let band_high = low_hz * (high_hz / low_hz).powf(high_t);

            let first_bin = ((band_low * FFT_SIZE as f32 / self.sample_rate as f32) as usize)
                .clamp(1, FFT_SIZE / 2 - 1);
            let last_bin = ((band_high * FFT_SIZE as f32 / self.sample_rate as f32).ceil() as usize)
                .clamp(first_bin + 1, FFT_SIZE / 2);

            let mut energy = 0.0_f32;
            for bin in first_bin..last_bin {
                energy += self.fft_input[bin].norm_sqr();
            }
            energy /= (last_bin - first_bin) as f32;

            // Convert spectral energy into a perceptually useful 0..1 display value.
            let level = ((energy.sqrt() / FFT_SIZE as f32) * 45.0)
                .sqrt()
                .clamp(0.0, 1.0);
            let previous = self.bands[band_index];
            let attack = if received_audio { 0.58 } else { 0.30 };
            let release = (dt * 7.0).clamp(0.04, 0.35);
            self.bands[band_index] = if level > previous {
                previous + (level - previous) * attack
            } else {
                previous + (level - previous) * release
            };
        }
    }

    fn decay(&mut self, dt: f32) {
        let amount = (dt * 3.5).clamp(0.01, 0.25);
        for band in &mut self.bands {
            *band += (0.0 - *band) * amount;
        }
    }

    pub fn bands(&self) -> &[f32] {
        &self.bands
    }
}
