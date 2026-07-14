use crate::{color::Color, framebuffer::Framebuffer};

pub fn glow_rect(fb: &mut Framebuffer, x: i32, y: i32, width: u32, height: u32, color: Color, radius: i32) {
    for r in (1..=radius.max(1)).rev() {
        let opacity = 0.025 + 0.08 * (1.0 - r as f32 / (radius.max(1) + 1) as f32);
        let left = x - r;
        let top = y - r;
        let w = width as i32 + r * 2;
        let h = height as i32 + r * 2;
        for xx in 0..w { fb.blend_pixel(left + xx, top, color, opacity); fb.blend_pixel(left + xx, top + h - 1, color, opacity); }
        for yy in 0..h { fb.blend_pixel(left, top + yy, color, opacity); fb.blend_pixel(left + w - 1, top + yy, color, opacity); }
    }
    fb.fill_rect(x, y, width, height, color);
}
