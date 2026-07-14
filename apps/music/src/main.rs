mod app;
mod audio;
mod color;
mod display;
mod framebuffer;
mod library;
mod player;
mod playlists;
mod primitives;
mod theme;
mod widgets;
use app::App;
use winit::event_loop::EventLoop;
fn main(){let event_loop=EventLoop::new().expect("failed to create event loop");let mut app=App::new();event_loop.run_app(&mut app).expect("application failed")}
