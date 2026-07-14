use crate::color::Color;

#[derive(Clone, Copy, Debug)]
pub struct Theme {
    pub name: &'static str,
    pub background: Color,
    pub panel: Color,
    pub border: Color,
    pub phosphor: Color,
}

impl Theme {
    pub const fn lain() -> Self {
        Self {
            name: "LAIN",
            background: Color::new(10, 10, 15),
            panel: Color::new(15, 15, 26),
            border: Color::new(80, 64, 112),
            phosphor: Color::new(155, 127, 212),
        }
    }

    pub const fn green() -> Self {
        Self {
            name: "GREEN",
            background: Color::new(3, 10, 10),
            panel: Color::new(7, 20, 19),
            border: Color::new(18, 70, 62),
            phosphor: Color::new(65, 230, 180),
        }
    }

    pub const fn amber() -> Self {
        Self {
            name: "AMBER",
            background: Color::new(14, 8, 2),
            panel: Color::new(27, 16, 4),
            border: Color::new(105, 64, 16),
            phosphor: Color::new(255, 176, 48),
        }
    }

    pub const fn blue() -> Self {
        Self {
            name: "BLUE",
            background: Color::new(2, 7, 15),
            panel: Color::new(5, 15, 28),
            border: Color::new(18, 62, 102),
            phosphor: Color::new(75, 190, 255),
        }
    }

    pub const fn red() -> Self {
        Self {
            name: "RED",
            background: Color::new(15, 3, 3),
            panel: Color::new(30, 6, 6),
            border: Color::new(105, 24, 24),
            phosphor: Color::new(255, 75, 64),
        }
    }
}
