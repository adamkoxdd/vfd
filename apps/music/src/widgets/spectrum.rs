use crate::{framebuffer::Framebuffer, primitives::glow_rect, theme::Theme};

pub fn draw(fb: &mut Framebuffer, values: &[f32], x: i32, baseline: i32, width: u32, height: u32, theme: Theme) {
    if values.is_empty() { return; }
    let gap = 3u32;
    let bar_width = (width / values.len() as u32).saturating_sub(gap).max(2);
    let stride = width as f32 / values.len() as f32;
    for (i, value) in values.iter().enumerate() {
        let bar_height = (value.clamp(0.0, 1.0) * height as f32).round() as u32;
        if bar_height == 0 { continue; }
        let bx = x + (i as f32 * stride).round() as i32;
        let by = baseline - bar_height as i32;
        glow_rect(fb, bx, by, bar_width, bar_height, theme.phosphor, 2);
    }
}
