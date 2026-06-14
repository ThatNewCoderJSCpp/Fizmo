#ifndef FIZMO_PAINT_HPP
#define FIZMO_PAINT_HPP

#include "gradient.hpp"

namespace fizmo {

enum class RectOrigin {
    TopLeft = 0,
    TopCenter,
    TopRight,

    CenterLeft,
    Center,
    CenterRight,

    BottomLeft,
    BottomCenter,
    BottomRight
};

constexpr void resolve_rect_origin(
    int& out_x, int& out_y,
    int x, int y,
    unsigned int w, unsigned int h,
    RectOrigin origin
) noexcept {
    int iw = static_cast<int>(w);
    int ih = static_cast<int>(h);

    switch (origin) {
        case RectOrigin::TopLeft:
            out_x = x;
            out_y = y;
            break;

        case RectOrigin::TopCenter:
            out_x = x - iw / 2;
            out_y = y;
            break;

        case RectOrigin::TopRight:
            out_x = x - iw;
            out_y = y;
            break;

        case RectOrigin::CenterLeft:
            out_x = x;
            out_y = y - ih / 2;
            break;

        case RectOrigin::Center:
            out_x = x - iw / 2;
            out_y = y - ih / 2;
            break;

        case RectOrigin::CenterRight:
            out_x = x - iw;
            out_y = y - ih / 2;
            break;

        case RectOrigin::BottomLeft:
            out_x = x;
            out_y = y - ih;
            break;

        case RectOrigin::BottomCenter:
            out_x = x - iw / 2;
            out_y = y - ih;
            break;

        case RectOrigin::BottomRight:
            out_x = x - iw;
            out_y = y - ih;
            break;
    }
}

constexpr void resolve_rect_origin(
    int& x, int& y, 
    unsigned int w, unsigned int h,
    RectOrigin origin
) noexcept {
    resolve_rect_origin(x, y, x, y, w, h, origin);
}

namespace graphics {

enum class PaintStyle {
    Fill = 0,
    Stroke,
    FillAndStroke
};

enum class LineCap {
    Flat = 0,
    Round,
    Square
};

enum class LineJoin {
    Miter = 0,
    Round,
    Bevel
};

class Paint {
private:
    Color m_fill_color;
    Color m_stroke_color;
    unsigned int m_stroke_width = 0;
    PaintStyle m_style = PaintStyle::Fill;
    RectOrigin m_origin = RectOrigin::TopLeft;
    LineCap m_line_cap = LineCap::Flat;
    LineJoin m_line_join = LineJoin::Miter;
    float m_opacity = 1.0f;
    double m_start_angle = 0.0;
    double m_end_angle = 0.0;
    const Gradient* m_fill_gradient   = nullptr;   
    const Gradient* m_stroke_gradient = nullptr;

public:
    constexpr Paint() noexcept = default;

    static constexpr Paint fill(const Color& color) noexcept {
        Paint p;
        p.m_fill_color = color;
        p.m_style = PaintStyle::Fill;
        return p;
    }

    static constexpr Paint stroke(const Color& color, unsigned int width = 1) noexcept {
        Paint p;
        p.m_stroke_color = color;
        p.m_stroke_width = width;
        p.m_style = PaintStyle::Stroke;
        return p;
    }

    static constexpr Paint stroke(unsigned int width = 1) noexcept { return stroke(Color(), width); }

    static constexpr Paint fill_and_stroke(
        const Color& fill_color,
        const Color& stroke_color,
        unsigned int stroke_width = 1
    ) noexcept {
        Paint p;
        p.m_fill_color = fill_color;
        p.m_stroke_color = stroke_color;
        p.m_stroke_width = stroke_width;
        p.m_style = PaintStyle::FillAndStroke;
        return p;
    }

    static constexpr Paint fill_and_stroke(
        const Color& fill_color,
        unsigned int stroke_width = 1
    ) noexcept {
        return fill_and_stroke(fill_color, Color(), stroke_width);
    }

    static Paint gradient_fill(const Gradient& g) noexcept {
        Paint p;
        p.m_style = PaintStyle::Fill;
        p.m_fill_gradient = &g;
        return p;
    }

    static Paint gradient_fill_and_stroke(
        const Gradient& fill_grad,
        const Color& stroke_color,
        unsigned int stroke_width = 1
    ) noexcept {
        Paint p;
        p.m_fill_gradient = &fill_grad;
        p.m_stroke_color  = stroke_color;
        p.m_stroke_width  = stroke_width;
        p.m_style = PaintStyle::FillAndStroke;
        return p;
    }

public:
    constexpr Paint& set_fill_color(const Color& c) noexcept { m_fill_color = c; return *this; }
    constexpr Paint& set_stroke_color(const Color& c) noexcept { m_stroke_color = c; return *this; }
    constexpr Paint& set_stroke_width(unsigned int w) noexcept { m_stroke_width = w; return *this; }
    constexpr Paint& set_style(PaintStyle s) noexcept { m_style = s; return *this; }
    constexpr Paint& set_origin(RectOrigin o) noexcept { m_origin = o; return *this; }
    constexpr Paint& set_line_cap(LineCap c) noexcept { m_line_cap = c; return *this; }
    constexpr Paint& set_line_join(LineJoin j) noexcept { m_line_join = j; return *this; }
    constexpr Paint& set_opacity(float o) noexcept { m_opacity = o; return *this; }

    constexpr Paint& set_start_angle(double a, bool input_in_degrees = true) noexcept { 
        a = input_in_degrees ? a : a * constants::reciprocal_pi_180();
        m_start_angle = a; 
        return *this; 
    }
   
    constexpr Paint& set_end_angle(double a, bool input_in_degrees = true) noexcept { 
        a = input_in_degrees ? a : a * constants::reciprocal_pi_180();
        m_end_angle = a; 
        return *this; 
    }
    
    constexpr Paint& set_angles(double start, double end, bool input_in_degrees = true) noexcept {
        set_start_angle(start, input_in_degrees);
        set_end_angle(end, input_in_degrees);
        return *this;
    }

    Paint& set_fill_gradient(const Gradient* g) noexcept {
        m_fill_gradient = g;
        return *this;
    }

    Paint& set_stroke_gradient(const Gradient* g) noexcept {
        m_stroke_gradient = g;
        return *this;
    }

    Paint& clear_fill_gradient() noexcept {
        m_fill_gradient = nullptr;
        return *this;
    }

    Paint& clear_stroke_gradient() noexcept {
        m_stroke_gradient = nullptr;
        return *this;
    }

public:
    constexpr const Color& fill_color() const noexcept { return m_fill_color; }
    constexpr const Color& stroke_color() const noexcept { return m_stroke_color; }
    constexpr unsigned int stroke_width() const noexcept { return m_stroke_width; }
    constexpr PaintStyle style() const noexcept { return m_style; }
    constexpr RectOrigin origin() const noexcept { return m_origin; }
    constexpr LineCap line_cap() const noexcept { return m_line_cap; }
    constexpr LineJoin line_join() const noexcept { return m_line_join; }
    constexpr float opacity() const noexcept { return m_opacity; }
    constexpr double start_angle() const noexcept { return m_start_angle; }
    constexpr double end_angle() const noexcept { return m_end_angle; }
    const Gradient* fill_gradient()   const noexcept { return m_fill_gradient; }
    const Gradient* stroke_gradient() const noexcept { return m_stroke_gradient; }

public:
    constexpr bool has_fill() const noexcept { return m_style == PaintStyle::Fill || m_style == PaintStyle::FillAndStroke; }
    constexpr bool has_stroke() const noexcept { return (m_style == PaintStyle::Stroke || m_style == PaintStyle::FillAndStroke) && m_stroke_width > 0; }
    constexpr bool is_full_sweep() const noexcept { return std::abs(m_end_angle - m_start_angle) >= 360.0; }
    bool has_fill_gradient()   const noexcept { return m_fill_gradient != nullptr; }
    bool has_stroke_gradient() const noexcept { return m_stroke_gradient != nullptr; }
};

} // namespace graphics
} // namespace fizmo

#endif // FIZMO_PAINT_HPP