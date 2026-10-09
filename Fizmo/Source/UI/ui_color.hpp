#ifndef FIZMO_UI_COLOR_HPP
#define FIZMO_UI_COLOR_HPP

#include "ui_controls.hpp"
#include "../Graphics/texture.hpp"
#include "../Images/Bitmap/image.hpp"

namespace fizmo {
namespace ui {

struct Hsv {
    double h = 0.0, s = 0.0, v = 0.0, a = 1.0;
};

inline Color hsv_to_color(const Hsv& c) noexcept {
    const double h = std::fmod(std::fmod(c.h, 360.0) + 360.0, 360.0) / 60.0;
    const double s = std::max(0.0, std::min(1.0, c.s)), v = std::max(0.0, std::min(1.0, c.v));
    const int i = static_cast<int>(std::floor(h)) % 6;
    const double f = h - std::floor(h);
    const double p = v * (1.0 - s), q = v * (1.0 - s * f), t = v * (1.0 - s * (1.0 - f));
    double r = v, g = t, b = p;
    switch (i) {
        case 0: r = v; g = t; b = p; break;
        case 1: r = q; g = v; b = p; break;
        case 2: r = p; g = v; b = t; break;
        case 3: r = p; g = q; b = v; break;
        case 4: r = t; g = p; b = v; break;
        default: r = v; g = p; b = q; break;
    }
    auto u = [](double x) { return static_cast<std::uint8_t>(std::lround(std::max(0.0, std::min(1.0, x)) * 255.0)); };
    return Color(u(r), u(g), u(b), u(c.a));
}

inline Hsv color_to_hsv(const Color& c, double keep_hue = 0.0) noexcept {
    const double r = c.red() / 255.0, g = c.green() / 255.0, b = c.blue() / 255.0;
    const double mx = std::max({ r, g, b }), mn = std::min({ r, g, b }), d = mx - mn;
    Hsv out;
    out.v = mx;
    out.s = mx > 0.0 ? d / mx : 0.0;
    out.a = c.alpha() / 255.0;
    if (d <= 1e-12) out.h = keep_hue;
    else if (mx == r) out.h = 60.0 * std::fmod((g - b) / d + 6.0, 6.0);
    else if (mx == g) out.h = 60.0 * ((b - r) / d + 2.0);
    else out.h = 60.0 * ((r - g) / d + 4.0);
    return out;
}

inline std::string color_to_hex(const Color& c, bool alpha) {
    char buf[16];
    if (alpha) std::snprintf(buf, sizeof(buf), "#%02X%02X%02X%02X", c.red(), c.green(), c.blue(), c.alpha());
    else std::snprintf(buf, sizeof(buf), "#%02X%02X%02X", c.red(), c.green(), c.blue());
    return buf;
}

inline std::optional<Color> color_from_hex(std::string s) {
    while (!s.empty() && s.front() == ' ') s.erase(s.begin());
    while (!s.empty() && s.back() == ' ') s.pop_back();
    if (!s.empty() && s.front() == '#') s.erase(s.begin());
    if (s.size() > 2 && s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) s.erase(0, 2);
    for (char c : s) if (!std::isxdigit(static_cast<unsigned char>(c))) return std::nullopt;
    auto hx = [](char c) { return std::isdigit(static_cast<unsigned char>(c)) ? c - '0' : std::tolower(static_cast<unsigned char>(c)) - 'a' + 10; };
    auto byte = [&](std::size_t i) { return static_cast<std::uint8_t>(hx(s[i]) * 16 + hx(s[i + 1])); };
    auto nib = [&](std::size_t i) { return static_cast<std::uint8_t>(hx(s[i]) * 17); };
    switch (s.size()) {
        case 3: return Color(nib(0), nib(1), nib(2));
        case 4: return Color(nib(0), nib(1), nib(2), nib(3));
        case 6: return Color(byte(0), byte(2), byte(4));
        case 8: return Color(byte(0), byte(2), byte(4), byte(6));
        default: return std::nullopt;
    }
}

inline const graphics::Texture& color_wheel_texture() {
    static const graphics::Texture tex = [] {
        const unsigned int n = 256;
        images::BitmapImage img(n, n);
        const double c = (n - 1) * 0.5, rmax = n * 0.5 - 1.0;
        for (unsigned int y = 0; y < n; ++y) {
            for (unsigned int x = 0; x < n; ++x) {
                const double dx = x - c, dy = c - y;
                const double r = std::sqrt(dx * dx + dy * dy) / rmax;
                const double edge = std::max(0.0, std::min(1.0, (1.0 - r) * rmax + 0.5));
                double h = std::atan2(dy, dx) * 180.0 / 3.14159265358979323846;
                if (h < 0.0) h += 360.0;
                Color col = hsv_to_color(Hsv{ h, std::min(1.0, r), 1.0, 1.0 });
                img.set_pixel(x, y, Color(col.red(), col.green(), col.blue(), static_cast<std::uint8_t>(std::lround(edge * 255.0))));
            }
        }
        return graphics::Texture(std::move(img), graphics::SampleFilter::Bilinear, graphics::WrapMode::Clamp);
    }();
    return tex;
}

class ColorPicker : public Widget {
protected:
    Hsv                               m_hsv{ 0.0, 0.0, 1.0, 1.0 };
    Color                             m_color{ 255, 255, 255 };
    Color                             m_original{ 255, 255, 255 };
    bool                              m_alpha = true;
    bool                              m_show_hsv = false;
    bool                              m_popup_style = false;
    int                               m_drag = 0;
    float                             m_wheel = 168.0f;
    VStack*                           m_rows = nullptr;
    TextField*                        m_hex = nullptr;
    std::array<IntField*, 4>          m_rgba{};
    std::array<IntField*, 3>          m_hsv_fields{};
    HStack*                           m_hsv_row = nullptr;
    bool                              m_syncing = false;
    std::function<void(const Color&)> m_on_change;
    std::function<void()>             m_on_cancel;

    float pad() const noexcept { return 10.0f; }
    float bar_w() const noexcept { return 18.0f; }
    Rect wheel_rect() const noexcept { return Rect(pad(), pad(), m_wheel, m_wheel); }
    Rect value_rect() const noexcept { return Rect(pad() + m_wheel + 12.0f, pad(), bar_w(), m_wheel); }
    Rect alpha_rect() const noexcept { return Rect(value_rect().right() + 8.0f, pad(), bar_w(), m_wheel); }
    float top_width() const noexcept { return (m_alpha ? alpha_rect().right() : value_rect().right()) + pad(); }

    void sync_fields() {
        m_syncing = true;
        m_rgba[0]->set_value(m_color.red());
        m_rgba[1]->set_value(m_color.green());
        m_rgba[2]->set_value(m_color.blue());
        m_rgba[3]->set_value(m_color.alpha());
        m_hsv_fields[0]->set_value(static_cast<long long>(std::lround(m_hsv.h)) % 360);
        m_hsv_fields[1]->set_value(std::lround(m_hsv.s * 100.0));
        m_hsv_fields[2]->set_value(std::lround(m_hsv.v * 100.0));
        if (!m_hex->focused()) m_hex->set_text(color_to_hex(m_color, m_alpha && m_color.alpha() != 255));
        m_syncing = false;
    }

    void from_hsv(bool notify) {
        const Color c = hsv_to_color(m_hsv);
        const bool ch = c != m_color;
        m_color = c;
        sync_fields();
        if (notify && ch && m_on_change) m_on_change(m_color);
    }

    void from_color(const Color& c, bool notify) {
        const Color cc = m_alpha ? c : Color(c.red(), c.green(), c.blue(), 255);
        const bool ch = cc != m_color;
        const Hsv h = color_to_hsv(cc, m_hsv.h);
        m_hsv.h = h.h;
        m_hsv.v = h.v;
        if (h.v > 0.0) m_hsv.s = h.s;
        m_hsv.a = h.a;
        m_color = cc;
        sync_fields();
        if (notify && ch && m_on_change) m_on_change(m_color);
    }

    void pick_wheel(float x, float y) {
        const Rect w = wheel_rect();
        const float dx = x - w.center_x(), dy = w.center_y() - y;
        const float r = w.w * 0.5f;
        double h = std::atan2(dy, dx) * 180.0 / 3.14159265358979323846;
        if (h < 0.0) h += 360.0;
        m_hsv.h = h;
        m_hsv.s = std::min(1.0, std::sqrt(static_cast<double>(dx) * dx + static_cast<double>(dy) * dy) / r);
        from_hsv(true);
    }

    void pick_value(float y) { const Rect v = value_rect(); m_hsv.v = std::max(0.0, std::min(1.0, 1.0 - (y - v.y) / v.h)); from_hsv(true); }
    void pick_alpha(float y) { const Rect a = alpha_rect(); m_hsv.a = std::max(0.0, std::min(1.0, 1.0 - (y - a.y) / a.h)); from_hsv(true); }

public:
    explicit ColorPicker(const Color& c = Color(255, 255, 255), bool alpha = true) : m_alpha(alpha) {
        m_rows = &add<VStack>(6.0f);
        HStack& top = m_rows->add<HStack>(6.0f);
        top.set_cross_align(Align::Stretch);
        m_hex = &top.add<TextField>("");
        m_hex->set_flex(1.0f);
        m_hex->set_filter(CharFilter::hex() || CharFilter::only("#"));
        m_hex->set_max_length(9);
        m_hex->set_case(TextCase::Upper);
        m_hex->set_select_all_on_focus(true);
        m_hex->set_check([](const std::string& s) { return color_from_hex(s).has_value(); });
        m_hex->on_commit([this](const std::string& s) { if (m_syncing) return; if (auto c = color_from_hex(s)) from_color(*c, true); else sync_fields(); });
        HStack& rgba = m_rows->add<HStack>(4.0f);
        static const char* names[4] = { "R", "G", "B", "A" };
        static const Color tags[4] = { Color(200, 70, 70), Color(70, 160, 70), Color(70, 110, 210), Color(130, 130, 140) };
        for (int i = 0; i < 4; ++i) {
            IntField& f = rgba.add<IntField>(0, 0, 255, 1);
            f.set_flex(1.0f);
            f.set_min_size(52.0f, 0.0f);
            f.set_tag(names[i], tags[i]);
            f.on_value_change([this](long long) {
                if (m_syncing) return;
                from_color(Color(static_cast<std::uint8_t>(m_rgba[0]->value()), static_cast<std::uint8_t>(m_rgba[1]->value()), static_cast<std::uint8_t>(m_rgba[2]->value()), static_cast<std::uint8_t>(m_rgba[3]->value())), true);
            });
            m_rgba[static_cast<std::size_t>(i)] = &f;
        }
        m_hsv_row = &m_rows->add<HStack>(4.0f);
        static const char* hn[3] = { "H", "S", "V" };
        static const long long hmax[3] = { 359, 100, 100 };
        for (int i = 0; i < 3; ++i) {
            IntField& f = m_hsv_row->add<IntField>(0, 0, hmax[i], 1);
            f.set_flex(1.0f);
            f.set_min_size(52.0f, 0.0f);
            f.set_tag(hn[i], Color(110, 110, 125));
            if (i == 0) f.set_wrap(true);
            f.on_value_change([this](long long) {
                if (m_syncing) return;
                m_hsv.h = static_cast<double>(m_hsv_fields[0]->value());
                m_hsv.s = m_hsv_fields[1]->value() / 100.0;
                m_hsv.v = m_hsv_fields[2]->value() / 100.0;
                from_hsv(true);
            });
            m_hsv_fields[static_cast<std::size_t>(i)] = &f;
        }
        set_alpha_enabled(alpha);
        set_show_hsv(false);
        set_color(c);
        m_original = m_color;
    }

    const Color& color() const noexcept { return m_color; }
    const Hsv& hsv() const noexcept { return m_hsv; }
    const Color& original() const noexcept { return m_original; }
    ColorPicker& set_color(const Color& c, bool notify = false) { from_color(c, notify); return *this; }
    ColorPicker& set_hsv(const Hsv& h, bool notify = false) { m_hsv = h; from_hsv(notify); return *this; }
    ColorPicker& set_original(const Color& c) noexcept { m_original = c; return *this; }
    ColorPicker& set_alpha_enabled(bool a) {
        m_alpha = a;
        m_rgba[3]->set_visible(a);
        if (!a) { m_hsv.a = 1.0; from_hsv(false); }
        return *this;
    }
    bool alpha_enabled() const noexcept { return m_alpha; }
    ColorPicker& set_show_hsv(bool s) { m_show_hsv = s; m_hsv_row->set_visible(s); return *this; }
    ColorPicker& set_wheel_size(float s) noexcept { m_wheel = std::max(60.0f, s); return *this; }
    ColorPicker& set_popup_style(bool p) noexcept { m_popup_style = p; return *this; }
    ColorPicker& on_change(std::function<void(const Color&)> fn) { m_on_change = std::move(fn); return *this; }
    ColorPicker& on_cancel(std::function<void()> fn) { m_on_cancel = std::move(fn); return *this; }
    TextField& hex_field() noexcept { return *m_hex; }
    IntField& channel_field(int i) noexcept { return *m_rgba.at(static_cast<std::size_t>(i)); }
    IntField& hsv_field(int i) noexcept { return *m_hsv_fields.at(static_cast<std::size_t>(i)); }
    Rect wheel_area() const noexcept { return wheel_rect(); }
    Rect value_area() const noexcept { return value_rect(); }
    Rect alpha_area() const noexcept { return alpha_rect(); }

    Point marker() const noexcept {
        const Rect w = wheel_rect();
        const double a = m_hsv.h * 3.14159265358979323846 / 180.0;
        const float r = static_cast<float>(m_hsv.s) * w.w * 0.5f;
        return Point{ w.center_x() + static_cast<float>(std::cos(a)) * r, w.center_y() - static_cast<float>(std::sin(a)) * r };
    }

    Size measure(Ui& ui) override {
        const Size rows = m_rows->preferred(ui);
        const float w = std::max(top_width(), rows.w + pad() * 2.0f);
        return Size{ w, pad() + m_wheel + 10.0f + 30.0f + 8.0f + rows.h + pad() };
    }

    void arrange(Ui& ui) override {
        const float y = pad() + m_wheel + 10.0f + 30.0f + 8.0f;
        m_rows->set_rect(Rect(pad(), y, std::max(0.0f, width() - pad() * 2.0f), m_rows->preferred(ui).h));
        m_rows->arrange(ui);
    }

    void draw(Painter& p, Ui& ui) override {
        const Theme& t = theme();
        if (m_popup_style) p.box(bounds(), t.radius, t.popup, t.border, t.border_width);
        const Rect w = wheel_rect();
        const std::uint8_t v = static_cast<std::uint8_t>(std::lround(m_hsv.v * 255.0));
        p.texture(color_wheel_texture(), w, enabled() ? Color(v, v, v) : Color(v / 2, v / 2, v / 2));
        const Point mk = marker();
        p.stroke_circle(mk.x, mk.y, 6.0f, Color(0, 0, 0, 160), 3.0f);
        p.stroke_circle(mk.x, mk.y, 6.0f, Color(255, 255, 255), 1.5f);
        const Rect vr = value_rect();
        const Color full = hsv_to_color(Hsv{ m_hsv.h, m_hsv.s, 1.0, 1.0 });
        p.gradient_rect(vr, full, Color(0, 0, 0), true);
        p.stroke_rect(vr, t.border, 1.0f);
        const float vy = vr.y + static_cast<float>((1.0 - m_hsv.v) * vr.h);
        p.fill_rect(Rect(vr.x - 3.0f, std::round(vy) - 2.0f, vr.w + 6.0f, 4.0f), Color(255, 255, 255));
        p.stroke_rect(Rect(vr.x - 3.0f, std::round(vy) - 2.0f, vr.w + 6.0f, 4.0f), Color(0, 0, 0, 170), 1.0f);
        if (m_alpha) {
            const Rect ar = alpha_rect();
            p.push_clip(ar);
            p.checkerboard(ar, 6.0f, Color(200, 200, 200), Color(150, 150, 150));
            const Color opaque(m_color.red(), m_color.green(), m_color.blue(), 255), clear(m_color.red(), m_color.green(), m_color.blue(), 0);
            p.gradient_rect(ar, opaque, clear, true);
            p.pop_clip();
            p.stroke_rect(ar, t.border, 1.0f);
            const float ay = ar.y + static_cast<float>((1.0 - m_hsv.a) * ar.h);
            p.fill_rect(Rect(ar.x - 3.0f, std::round(ay) - 2.0f, ar.w + 6.0f, 4.0f), Color(255, 255, 255));
            p.stroke_rect(Rect(ar.x - 3.0f, std::round(ay) - 2.0f, ar.w + 6.0f, 4.0f), Color(0, 0, 0, 170), 1.0f);
        }
        const float sy = pad() + m_wheel + 10.0f;
        const Rect sw(pad(), sy, std::max(0.0f, width() - pad() * 2.0f), 30.0f);
        p.push_clip(sw);
        p.checkerboard(sw, 7.0f, Color(200, 200, 200), Color(150, 150, 150));
        p.fill_rect(Rect(sw.x, sw.y, std::round(sw.w * 0.5f), sw.h), m_original);
        p.fill_rect(Rect(sw.x + std::round(sw.w * 0.5f), sw.y, sw.w - std::round(sw.w * 0.5f), sw.h), m_color);
        p.pop_clip();
        p.stroke_rounded(sw, 2.0f, t.border, 1.0f);
        draw_children(p, ui);
    }

    bool on_mouse_down(MouseEvent& e) override {
        if (e.button != input::MouseButton::Left) return true;
        const Rect w = wheel_rect();
        const float dx = e.x - w.center_x(), dy = e.y - w.center_y();
        if (dx * dx + dy * dy <= (w.w * 0.5f + 2.0f) * (w.w * 0.5f + 2.0f)) { m_drag = 1; pick_wheel(e.x, e.y); return true; }
        if (value_rect().inset(-4.0f, -4.0f).contains(e.x, e.y)) { m_drag = 2; pick_value(e.y); return true; }
        if (m_alpha && alpha_rect().inset(-4.0f, -4.0f).contains(e.x, e.y)) { m_drag = 3; pick_alpha(e.y); return true; }
        const float sy = pad() + m_wheel + 10.0f;
        const Rect sw(pad(), sy, width() - pad() * 2.0f, 30.0f);
        if (sw.contains(e.x, e.y) && e.x < sw.center_x()) { from_color(m_original, true); return true; }
        return true;
    }

    void on_mouse_move(MouseEvent& e) override {
        if (!pressed()) return;
        if (m_drag == 1) pick_wheel(e.x, e.y);
        else if (m_drag == 2) pick_value(e.y);
        else if (m_drag == 3) pick_alpha(e.y);
    }

    void on_mouse_up(MouseEvent&) override { m_drag = 0; }

    bool on_key(const KeyEvent& k) override {
        if (k.key == input::Key::Escape && m_on_cancel) { m_on_cancel(); return true; }
        return false;
    }

    windows::SystemCursor cursor(float x, float y) const noexcept override {
        const Rect w = wheel_rect();
        const float dx = x - w.center_x(), dy = y - w.center_y();
        if (dx * dx + dy * dy <= w.w * w.w * 0.25f || value_rect().contains(x, y) || (m_alpha && alpha_rect().contains(x, y))) return windows::SystemCursor::Crosshair;
        return windows::SystemCursor::Arrow;
    }
};

class ColorField : public Widget {
protected:
    Color                              m_color;
    bool                               m_alpha = true;
    bool                               m_show_hex = true;
    std::unique_ptr<ColorPicker>       m_picker;
    std::function<void(const Color&)>  m_on_change;
    bool                               m_armed = false;

    Rect swatch_rect() const noexcept {
        const float h = height() - 8.0f;
        return m_show_hex ? Rect(4.0f, 4.0f, std::max(h, 28.0f), h) : Rect(4.0f, 4.0f, std::max(0.0f, width() - 8.0f), h);
    }

public:
    explicit ColorField(const Color& c = Color(255, 255, 255), bool alpha = true) : m_color(c), m_alpha(alpha) {
        m_focusable = true;
        m_picker = std::make_unique<ColorPicker>(c, alpha);
        m_picker->set_popup_style(true);
        m_picker->on_change([this](const Color& col) { set_color(col, true); });
        m_picker->on_cancel([this]() { revert(); close(); });
    }

    ~ColorField() override { if (ui() && m_picker) ui()->close_popup(*m_picker); }

    const Color& color() const noexcept { return m_color; }
    ColorField& set_color(const Color& c, bool notify = false) {
        const Color cc = m_alpha ? c : Color(c.red(), c.green(), c.blue(), 255);
        const bool ch = cc != m_color;
        m_color = cc;
        if (!is_open()) m_picker->set_color(cc);
        if (notify && ch && m_on_change) m_on_change(m_color);
        return *this;
    }
    ColorField& set_alpha_enabled(bool a) { m_alpha = a; m_picker->set_alpha_enabled(a); return *this; }
    ColorField& set_show_hex(bool s) noexcept { m_show_hex = s; return *this; }
    ColorField& on_change(std::function<void(const Color&)> fn) { m_on_change = std::move(fn); return *this; }
    ColorPicker& picker() noexcept { return *m_picker; }
    bool is_open() const noexcept { return ui() && m_picker && ui()->popup_open(*m_picker); }
    windows::SystemCursor cursor(float, float) const noexcept override { return windows::SystemCursor::Hand; }

    void open() {
        if (!ui() || is_open() || !enabled()) return;
        m_picker->set_color(m_color);
        m_picker->set_original(m_color);
        ui()->open_popup(*this, *m_picker, screen_rect(), Placement::Below);
    }

    void close() { if (ui() && m_picker) ui()->close_popup(*m_picker); }

    void revert() {
        set_color(m_picker->original(), true);
        m_picker->set_color(m_picker->original());
    }

    Size measure(Ui&) override {
        const Theme& t = theme();
        return Size{ m_show_hex ? std::max(120.0f, text_width("#FFFFFFFF") + 52.0f + t.padding) : 48.0f, t.row_height };
    }

    void draw(Painter& p, Ui&) override {
        const Theme& t = theme();
        const bool en = enabled(), hot = hovered() && en;
        p.box(bounds(), t.radius, !en ? t.surface_disabled : is_open() ? t.surface_active : hot ? t.surface_hover : t.surface, is_open() || focused() ? t.focus : hot ? t.border_hover : t.border, t.border_width);
        const Rect s = swatch_rect();
        p.push_clip(s);
        p.checkerboard(s, 5.0f, Color(205, 205, 205), Color(150, 150, 150));
        p.fill_rect(s, en ? m_color : mix(m_color, t.surface_disabled, 0.6f));
        p.pop_clip();
        p.stroke_rect(s, Color(0, 0, 0, 90), 1.0f);
        if (m_show_hex) {
            const std::string hex = color_to_hex(m_color, m_alpha && m_color.alpha() != 255);
            const float lh = line_height();
            p.text(s.right() + 8.0f, std::round((height() - lh) * 0.5f), hex, text_style(en ? t.text : t.text_disabled));
        }
        draw_focus_ring(p);
    }

    bool on_mouse_down(MouseEvent& e) override { if (e.button != input::MouseButton::Left) return false; m_armed = true; if (is_open()) close(); else open(); return true; }
    void on_mouse_up(MouseEvent&) override { m_armed = false; }

    bool on_key(const KeyEvent& k) override {
        using K = input::Key;
        if (is_open() && k.key == K::Escape) { revert(); close(); return true; }
        if (!is_open() && (k.key == K::Enter || k.key == K::Space || k.key == K::NumpadEnter)) { open(); return true; }
        if (k.ctrl() && k.key == K::C && ui()) { ui()->set_clipboard(color_to_hex(m_color, m_alpha && m_color.alpha() != 255)); return true; }
        if (k.ctrl() && k.key == K::V && ui()) { if (auto c = color_from_hex(ui()->clipboard())) set_color(*c, true); return true; }
        return false;
    }
};

} // namespace ui
} // namespace fizmo

#endif // FIZMO_UI_COLOR_HPP
