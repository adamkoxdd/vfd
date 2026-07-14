use crate::color::Color;

pub struct Framebuffer {
    width: u32,
    height: u32,
    pixels: Vec<u32>,
}

impl Framebuffer {
    pub fn new(width: u32, height: u32) -> Self {
        let width = width.max(1);
        let height = height.max(1);
        Self { width, height, pixels: vec![0; (width * height) as usize] }
    }
    pub fn width(&self) -> u32 { self.width }
    pub fn height(&self) -> u32 { self.height }
    pub fn pixels(&self) -> &[u32] { &self.pixels }
    pub fn resize(&mut self, width: u32, height: u32) {
        let width = width.max(1);
        let height = height.max(1);
        if width == self.width && height == self.height { return; }
        self.width = width;
        self.height = height;
        self.pixels.resize((width * height) as usize, 0);
    }
    pub fn clear(&mut self, color: Color) { self.pixels.fill(color.to_u32()); }
    pub fn set_pixel(&mut self, x: i32, y: i32, color: Color) {
        if x < 0 || y < 0 { return; }
        let (x, y) = (x as u32, y as u32);
        if x >= self.width || y >= self.height { return; }
        self.pixels[(y * self.width + x) as usize] = color.to_u32();
    }
    pub fn blend_pixel(&mut self, x: i32, y: i32, color: Color, opacity: f32) {
        if x < 0 || y < 0 { return; }
        let (ux, uy) = (x as u32, y as u32);
        if ux >= self.width || uy >= self.height { return; }
        let index = (uy * self.width + ux) as usize;
        let bg = Color::from_u32(self.pixels[index]);
        self.pixels[index] = color.blend_over(bg, opacity).to_u32();
    }
    pub fn fill_rect(&mut self, x: i32, y: i32, width: u32, height: u32, color: Color) {
        for yy in 0..height { for xx in 0..width { self.set_pixel(x + xx as i32, y + yy as i32, color); } }
    }
    pub fn stroke_rect(&mut self, x: i32, y: i32, width: u32, height: u32, color: Color) {
        if width == 0 || height == 0 { return; }
        for xx in 0..width {
            self.set_pixel(x + xx as i32, y, color);
            self.set_pixel(x + xx as i32, y + height as i32 - 1, color);
        }
        for yy in 0..height {
            self.set_pixel(x, y + yy as i32, color);
            self.set_pixel(x + width as i32 - 1, y + yy as i32, color);
        }
    }
}
