#define ALL_FIZMO
#include <fizmo/includes.hpp>
#include "ui_color.hpp"

namespace fizmo {
namespace ui {

Color hsv_to_color(const Hsv& c) noexcept {
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

Hsv color_to_hsv(const Color& c, double keep_hue) noexcept {
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

std::string color_to_hex(const Color& c, bool alpha) {
    char buf[16];
    if (alpha) std::snprintf(buf, sizeof(buf), "#%02X%02X%02X%02X", c.red(), c.green(), c.blue(), c.alpha());
    else std::snprintf(buf, sizeof(buf), "#%02X%02X%02X", c.red(), c.green(), c.blue());
    return buf;
}

std::optional<Color> color_from_hex(std::string s) {
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

const graphics::Texture& color_wheel_texture() {
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

auto ColorPicker::sync_fields() -> void {
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

auto ColorPicker::from_hsv(bool notify) -> void {
        const Color c = hsv_to_color(m_hsv);
        const bool ch = c != m_color;
        m_color = c;
        sync_fields();
        if (notify && ch && m_on_change) m_on_change(m_color);
    }

auto ColorPicker::from_color(const Color& c, bool notify) -> void {
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

auto ColorPicker::pick_wheel(float x, float y) -> void {
        const Rect w = wheel_rect();
        const float dx = x - w.center_x(), dy = w.center_y() - y;
        const float r = w.w * 0.5f;
        double h = std::atan2(dy, dx) * 180.0 / 3.14159265358979323846;
        if (h < 0.0) h += 360.0;
        m_hsv.h = h;
        m_hsv.s = std::min(1.0, std::sqrt(static_cast<double>(dx) * dx + static_cast<double>(dy) * dy) / r);
        from_hsv(true);
    }

auto ColorPicker::pick_value(float y) -> void { const Rect v = value_rect(); m_hsv.v = std::max(0.0, std::min(1.0, 1.0 - (y - v.y) / v.h)); from_hsv(true); }

auto ColorPicker::pick_alpha(float y) -> void { const Rect a = alpha_rect(); m_hsv.a = std::max(0.0, std::min(1.0, 1.0 - (y - a.y) / a.h)); from_hsv(true); }

ColorPicker::ColorPicker(const Color& c, bool alpha) : m_alpha(alpha) {
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

auto ColorPicker::marker() const noexcept -> Point {
        const Rect w = wheel_rect();
        const double a = m_hsv.h * 3.14159265358979323846 / 180.0;
        const float r = static_cast<float>(m_hsv.s) * w.w * 0.5f;
        return Point{ w.center_x() + static_cast<float>(std::cos(a)) * r, w.center_y() - static_cast<float>(std::sin(a)) * r };
    }

auto ColorPicker::measure(Ui& ui) -> Size {
        const Size rows = m_rows->preferred(ui);
        const float w = std::max(top_width(), rows.w + pad() * 2.0f);
        return Size{ w, pad() + m_wheel + 10.0f + 30.0f + 8.0f + rows.h + pad() };
    }

auto ColorPicker::arrange(Ui& ui) -> void {
        const float y = pad() + m_wheel + 10.0f + 30.0f + 8.0f;
        m_rows->set_rect(Rect(pad(), y, std::max(0.0f, width() - pad() * 2.0f), m_rows->preferred(ui).h));
        m_rows->arrange(ui);
    }

auto ColorPicker::draw(Painter& p, Ui& ui) -> void {
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

auto ColorPicker::on_mouse_down(MouseEvent& e) -> bool {
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

auto ColorPicker::on_mouse_move(MouseEvent& e) -> void {
        if (!pressed()) return;
        if (m_drag == 1) pick_wheel(e.x, e.y);
        else if (m_drag == 2) pick_value(e.y);
        else if (m_drag == 3) pick_alpha(e.y);
    }

auto ColorPicker::cursor(float x, float y) const noexcept -> windows::SystemCursor {
        const Rect w = wheel_rect();
        const float dx = x - w.center_x(), dy = y - w.center_y();
        if (dx * dx + dy * dy <= w.w * w.w * 0.25f || value_rect().contains(x, y) || (m_alpha && alpha_rect().contains(x, y))) return windows::SystemCursor::Crosshair;
        return windows::SystemCursor::Arrow;
    }

auto ColorField::swatch_rect() const noexcept -> Rect {
        const float h = height() - 8.0f;
        return m_show_hex ? Rect(4.0f, 4.0f, std::max(h, 28.0f), h) : Rect(4.0f, 4.0f, std::max(0.0f, width() - 8.0f), h);
    }

ColorField::ColorField(const Color& c, bool alpha) : m_color(c), m_alpha(alpha) {
        m_focusable = true;
        m_picker = std::make_unique<ColorPicker>(c, alpha);
        m_picker->set_popup_style(true);
        m_picker->on_change([this](const Color& col) { set_color(col, true); });
        m_picker->on_cancel([this]() { revert(); close(); });
    }

auto ColorField::set_color(const Color& c, bool notify) -> ColorField& {
        const Color cc = m_alpha ? c : Color(c.red(), c.green(), c.blue(), 255);
        const bool ch = cc != m_color;
        m_color = cc;
        if (!is_open()) m_picker->set_color(cc);
        if (notify && ch && m_on_change) m_on_change(m_color);
        return *this;
    }

auto ColorField::open() -> void {
        if (!ui() || is_open() || !enabled()) return;
        m_picker->set_color(m_color);
        m_picker->set_original(m_color);
        ui()->open_popup(*this, *m_picker, screen_rect(), Placement::Below);
    }

auto ColorField::measure(Ui&) -> Size {
        const Theme& t = theme();
        return Size{ m_show_hex ? std::max(120.0f, text_width("#FFFFFFFF") + 52.0f + t.padding) : 48.0f, t.row_height };
    }

auto ColorField::draw(Painter& p, Ui&) -> void {
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

auto ColorField::on_mouse_down(MouseEvent& e) -> bool { if (e.button != input::MouseButton::Left) return false; m_armed = true; if (is_open()) close(); else open(); return true; }

auto ColorField::on_key(const KeyEvent& k) -> bool {
        using K = input::Key;
        if (is_open() && k.key == K::Escape) { revert(); close(); return true; }
        if (!is_open() && (k.key == K::Enter || k.key == K::Space || k.key == K::NumpadEnter)) { open(); return true; }
        if (k.ctrl() && k.key == K::C && ui()) { ui()->set_clipboard(color_to_hex(m_color, m_alpha && m_color.alpha() != 255)); return true; }
        if (k.ctrl() && k.key == K::V && ui()) { if (auto c = color_from_hex(ui()->clipboard())) set_color(*c, true); return true; }
        return false;
    }

} // namespace ui
} // namespace fizmo
