use crate::{framebuffer::Framebuffer, primitives::glow_rect, theme::Theme};

pub struct VfdDisplay;

impl VfdDisplay {
    pub fn panel(fb: &mut Framebuffer, theme: Theme) {
        fb.clear(theme.background);
        let w = fb.width();
        let h = fb.height();
        if w < 40 || h < 40 { return; }
        fb.fill_rect(10, 10, w - 20, h - 20, theme.panel);
        fb.stroke_rect(10, 10, w - 20, h - 20, theme.border);
        fb.stroke_rect(14, 14, w - 28, h - 28, theme.border.scale(0.45));
        for y in (16..h.saturating_sub(16)).step_by(3) {
            fb.fill_rect(16, y as i32, w.saturating_sub(32), 1, theme.background.scale(0.7));
        }
    }

    pub fn lamp(fb: &mut Framebuffer, x: i32, y: i32, on: bool, theme: Theme) {
        let color = if on { theme.phosphor } else { theme.phosphor.scale(0.18) };
        if on { glow_rect(fb, x, y, 6, 6, color, 4); } else { fb.fill_rect(x, y, 6, 6, color); }
    }
}
