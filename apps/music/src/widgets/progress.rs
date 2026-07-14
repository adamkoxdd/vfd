use crate::{framebuffer::Framebuffer, primitives::glow_rect, theme::Theme};

pub fn draw(fb: &mut Framebuffer, x: i32, y: i32, width: u32, progress: f32, theme: Theme) {
    let progress = progress.clamp(0.0, 1.0);
    fb.fill_rect(x, y, width, 4, theme.phosphor.scale(0.16));
    let filled = (width as f32 * progress).round() as u32;
    if filled > 0 { glow_rect(fb, x, y, filled, 4, theme.phosphor, 3); }
}
