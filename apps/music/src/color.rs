#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub struct Color {
    red: u8,
    green: u8,
    blue: u8,
}

impl Color {
    pub const fn new(red: u8, green: u8, blue: u8) -> Self {
        Self { red, green, blue }
    }

    pub const fn to_u32(self) -> u32 {
        ((self.red as u32) << 16) | ((self.green as u32) << 8) | self.blue as u32
    }

    pub fn scale(self, brightness: f32) -> Self {
        let brightness = brightness.clamp(0.0, 1.0);
        Self::new(
            (self.red as f32 * brightness).round() as u8,
            (self.green as f32 * brightness).round() as u8,
            (self.blue as f32 * brightness).round() as u8,
        )
    }

    pub fn blend_over(self, background: Self, opacity: f32) -> Self {
        let opacity = opacity.clamp(0.0, 1.0);
        let inverse = 1.0 - opacity;
        Self::new(
            (self.red as f32 * opacity + background.red as f32 * inverse).round() as u8,
            (self.green as f32 * opacity + background.green as f32 * inverse).round() as u8,
            (self.blue as f32 * opacity + background.blue as f32 * inverse).round() as u8,
        )
    }

    pub const fn from_u32(value: u32) -> Self {
        Self::new(
            ((value >> 16) & 0xff) as u8,
            ((value >> 8) & 0xff) as u8,
            (value & 0xff) as u8,
        )
    }
}
