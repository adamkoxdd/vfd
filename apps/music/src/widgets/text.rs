use crate::{color::Color, framebuffer::Framebuffer};
const FONT: [[u8; 5]; 38] = include!("text_font.in");
fn index(c: char) -> Option<usize> { match c { 'A'..='Z' => Some((c as u8-b'A') as usize), '0'..='9' => Some(26+(c as u8-b'0') as usize), '-' => Some(36), ':' => Some(37), _ => None } }
pub fn draw(fb: &mut Framebuffer, text: &str, mut x: i32, y: i32, scale: u32, color: Color) { for c in text.to_ascii_uppercase().chars() { if c==' ' { x+=(4*scale) as i32; continue; } if let Some(idx)=index(c) { let glyph=FONT[idx]; for (column,bits) in glyph.iter().enumerate() { for row in 0u32..7 { if (*bits & (1u8<<row))!=0 { fb.fill_rect(x+column as i32*scale as i32,y+(row*scale) as i32,scale,scale,color); } } } } x+=(6*scale) as i32; } }
pub fn truncate(value: &str, max: usize) -> String { if value.chars().count() <= max { return value.to_string(); } value.chars().take(max.saturating_sub(2)).collect::<String>() + "--" }
