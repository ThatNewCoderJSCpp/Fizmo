#ifndef FIZMO_UI_CORE_HPP
#define FIZMO_UI_CORE_HPP

#include "../Windows/window.hpp"
#include "../Windows/renderer.hpp"
#include "../Input/keys.hpp"
#include "../Input/input_types.hpp"
#include "../Graphics/color.hpp"
#include "../Graphics/gradient.hpp"
#include "../Graphics/paint.hpp"
#include "../Text/text_style.hpp"
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

namespace fizmo {
namespace ui {

using graphics::Color;

struct Point {
    float x = 0.0f, y = 0.0f;
};

struct Size {
    float w = 0.0f, h = 0.0f;
};

struct Rect {
    float x = 0.0f, y = 0.0f, w = 0.0f, h = 0.0f;

    constexpr Rect() noexcept = default;
    constexpr Rect(float x_, float y_, float w_, float h_) noexcept : x(x_), y(y_), w(w_), h(h_) {}

    constexpr float right() const noexcept { return x + w; }
    constexpr float bottom() const noexcept { return y + h; }
    constexpr float center_x() const noexcept { return x + w * 0.5f; }
    constexpr float center_y() const noexcept { return y + h * 0.5f; }
    constexpr bool empty() const noexcept { return w <= 0.0f || h <= 0.0f; }
    constexpr bool contains(float px, float py) const noexcept { return px >= x && py >= y && px < x + w && py < y + h; }
    constexpr Rect translated(float dx, float dy) const noexcept { return Rect(x + dx, y + dy, w, h); }
    constexpr Rect inset(float d) const noexcept { return inset(d, d); }
    constexpr Rect inset(float dx, float dy) const noexcept { return Rect(x + dx, y + dy, w - 2.0f * dx > 0.0f ? w - 2.0f * dx : 0.0f, h - 2.0f * dy > 0.0f ? h - 2.0f * dy : 0.0f); }

    Rect intersect(const Rect& o) const noexcept {
        const float l = std::max(x, o.x), t = std::max(y, o.y);
        const float r = std::min(right(), o.right()), b = std::min(bottom(), o.bottom());
        return Rect(l, t, std::max(0.0f, r - l), std::max(0.0f, b - t));
    }

    bool intersects(const Rect& o) const noexcept { return x < o.right() && o.x < right() && y < o.bottom() && o.y < bottom(); }
};

enum class Orientation : std::uint8_t { Horizontal = 0, Vertical };
enum class Align : std::uint8_t { Start = 0, Center, End, Stretch };
enum class Placement : std::uint8_t { Below = 0, Above, Right, Center, At };
enum class ButtonStyle : std::uint8_t { Normal = 0, Primary, Danger, Flat };

inline Color with_alpha(const Color& c, std::uint8_t a) noexcept { return Color(c.red(), c.green(), c.blue(), a); }

inline Color mix(const Color& a, const Color& b, float t) noexcept {
    t = std::max(0.0f, std::min(1.0f, t));
    auto m = [t](std::uint8_t p, std::uint8_t q) { return static_cast<std::uint8_t>(std::lround(p + (static_cast<float>(q) - p) * t)); };
    return Color(m(a.red(), b.red()), m(a.green(), b.green()), m(a.blue(), b.blue()), m(a.alpha(), b.alpha()));
}

struct Theme {
    Color background{ 30, 31, 36 };
    Color panel{ 38, 40, 46 };
    Color surface{ 50, 53, 60 };
    Color surface_hover{ 60, 64, 73 };
    Color surface_active{ 72, 77, 88 };
    Color surface_disabled{ 42, 44, 50 };
    Color field{ 28, 30, 35 };
    Color border{ 72, 76, 86 };
    Color border_hover{ 98, 103, 116 };
    Color focus{ 86, 156, 255 };
    Color accent{ 66, 135, 245 };
    Color accent_hover{ 92, 155, 255 };
    Color accent_active{ 48, 112, 216 };
    Color accent_text{ 255, 255, 255 };
    Color text{ 226, 228, 233 };
    Color text_muted{ 150, 155, 166 };
    Color text_disabled{ 104, 108, 117 };
    Color placeholder{ 118, 122, 132 };
    Color selection{ 66, 135, 245, 110 };
    Color caret{ 236, 238, 242 };
    Color error{ 235, 87, 87 };
    Color warning{ 242, 169, 59 };
    Color success{ 76, 190, 110 };
    Color overlay{ 0, 0, 0, 140 };
    Color popup{ 44, 47, 54 };
    Color tooltip_bg{ 18, 19, 22, 245 };
    Color tooltip_text{ 232, 232, 236 };
    Color shadow{ 0, 0, 0, 70 };
    Color axis_x{ 220, 76, 76 };
    Color axis_y{ 96, 178, 72 };
    Color axis_z{ 72, 124, 230 };
    Color axis_w{ 150, 150, 160 };

    double     font_size      = 14.0;
    text::Font font           = text::Font::None;
    float      radius         = 4.0f;
    float      padding        = 8.0f;
    float      spacing        = 6.0f;
    float      border_width   = 1.0f;
    float      row_height     = 26.0f;
    float      scrollbar_size = 10.0f;
    float      focus_width    = 2.0f;
    float      check_size     = 16.0f;
    double     tooltip_delay  = 0.55;
    double     caret_blink    = 0.53;
    double     repeat_delay   = 0.4;
    double     repeat_rate    = 0.05;

    static Theme dark() { return Theme(); }

    static Theme light() {
        Theme t;
        t.background = Color(240, 241, 244);
        t.panel = Color(250, 250, 252);
        t.surface = Color(232, 234, 238);
        t.surface_hover = Color(222, 225, 231);
        t.surface_active = Color(208, 212, 220);
        t.surface_disabled = Color(238, 239, 242);
        t.field = Color(255, 255, 255);
        t.border = Color(190, 194, 203);
        t.border_hover = Color(150, 156, 168);
        t.focus = Color(40, 110, 230);
        t.accent = Color(40, 110, 230);
        t.accent_hover = Color(60, 128, 240);
        t.accent_active = Color(28, 92, 200);
        t.text = Color(28, 30, 36);
        t.text_muted = Color(96, 101, 112);
        t.text_disabled = Color(160, 164, 172);
        t.placeholder = Color(150, 154, 163);
        t.selection = Color(40, 110, 230, 80);
        t.caret = Color(20, 20, 24);
        t.popup = Color(255, 255, 255);
        t.overlay = Color(0, 0, 0, 90);
        t.tooltip_bg = Color(40, 42, 48, 245);
        t.shadow = Color(0, 0, 0, 40);
        return t;
    }
};

struct MouseEvent {
    float              x = 0.0f, y = 0.0f;
    float              screen_x = 0.0f, screen_y = 0.0f;
    input::MouseButton button = input::MouseButton::Left;
    input::Modifiers   mods = input::Modifiers::None;
    int                clicks = 1;
    float              wheel_x = 0.0f, wheel_y = 0.0f;
    bool               focus_gained = false;

    bool shift() const noexcept { return input::has(mods, input::Modifiers::Shift); }
    bool ctrl() const noexcept { return input::has(mods, input::Modifiers::Control) || input::has(mods, input::Modifiers::Meta); }
    bool alt() const noexcept { return input::has(mods, input::Modifiers::Alt); }
};

struct KeyEvent {
    input::Key       key = input::Key::Unknown;
    input::Modifiers mods = input::Modifiers::None;
    bool             repeat = false;

    bool shift() const noexcept { return input::has(mods, input::Modifiers::Shift); }
    bool ctrl() const noexcept { return input::has(mods, input::Modifiers::Control) || input::has(mods, input::Modifiers::Meta); }
    bool alt() const noexcept { return input::has(mods, input::Modifiers::Alt); }
};

class Ui;
class Painter;

class Widget {
    friend class Ui;

private:
    Ui*                                  m_ui = nullptr;
    Widget*                              m_parent = nullptr;
    std::vector<std::unique_ptr<Widget>> m_children;
    Rect                                 m_rect;

    void set_ui_tree(Ui* ui) noexcept;

protected:
    float                  m_fixed_w = -1.0f, m_fixed_h = -1.0f;
    float                  m_min_w = 0.0f, m_min_h = 0.0f;
    float                  m_max_w = 1e9f, m_max_h = 1e9f;
    float                  m_flex = 0.0f;
    bool                   m_visible = true;
    bool                   m_enabled = true;
    bool                   m_focusable = false;
    std::string            m_tooltip;
    std::string            m_id;
    std::shared_ptr<Theme> m_theme;

public:
    Widget() = default;
    virtual ~Widget();
    Widget(const Widget&) = delete;
    Widget& operator=(const Widget&) = delete;

    template <typename T, typename... Args>
    T& add(Args&&... args) {
        auto p = std::make_unique<T>(std::forward<Args>(args)...);
        T& ref = *p;
        adopt(std::move(p));
        return ref;
    }

    Widget& adopt(std::unique_ptr<Widget> child);
    std::unique_ptr<Widget> release(Widget* child);
    bool remove(Widget* child) { return release(child) != nullptr; }
    void clear_children();

    const std::vector<std::unique_ptr<Widget>>& children() const noexcept { return m_children; }
    std::size_t child_count() const noexcept { return m_children.size(); }
    Widget* child(std::size_t i) const noexcept { return i < m_children.size() ? m_children[i].get() : nullptr; }
    Widget* parent() const noexcept { return m_parent; }
    Ui* ui() const noexcept { return m_ui; }
    bool is_ancestor_of(const Widget* w) const noexcept { for (; w; w = w->m_parent) if (w == this) return true; return false; }

    template <typename T = Widget>
    T* find(const std::string& id) {
        if (m_id == id) if (T* t = dynamic_cast<T*>(this)) return t;
        for (auto& c : m_children) if (T* t = c->template find<T>(id)) return t;
        return nullptr;
    }

    const Rect& rect() const noexcept { return m_rect; }
    Rect bounds() const noexcept { return Rect(0.0f, 0.0f, m_rect.w, m_rect.h); }
    float width() const noexcept { return m_rect.w; }
    float height() const noexcept { return m_rect.h; }
    void set_rect(const Rect& r) noexcept { m_rect = r; }
    Rect screen_rect() const noexcept;

    Widget& set_size(float w, float h) noexcept { m_fixed_w = w; m_fixed_h = h; return *this; }
    Widget& set_width(float w) noexcept { m_fixed_w = w; return *this; }
    Widget& set_height(float h) noexcept { m_fixed_h = h; return *this; }
    Widget& set_min_size(float w, float h) noexcept { m_min_w = w; m_min_h = h; return *this; }
    Widget& set_max_size(float w, float h) noexcept { m_max_w = w; m_max_h = h; return *this; }
    Widget& set_flex(float f) noexcept { m_flex = std::max(0.0f, f); return *this; }
    float flex() const noexcept { return m_flex; }

    Widget& set_visible(bool v) noexcept;
    bool visible() const noexcept { return m_visible; }
    Widget& set_enabled(bool e) noexcept;
    bool enabled() const noexcept { for (const Widget* w = this; w; w = w->m_parent) if (!w->m_enabled) return false; return true; }
    Widget& set_focusable(bool f) noexcept { m_focusable = f; return *this; }
    bool focusable() const noexcept { return m_focusable; }
    Widget& set_tooltip(std::string t) { m_tooltip = std::move(t); return *this; }
    const std::string& tooltip() const noexcept { return m_tooltip; }
    Widget& set_id(std::string id) { m_id = std::move(id); return *this; }
    const std::string& id() const noexcept { return m_id; }
    Widget& set_theme(const Theme& t) { m_theme = std::make_shared<Theme>(t); return *this; }
    Widget& set_theme(std::shared_ptr<Theme> t) { m_theme = std::move(t); return *this; }
    Widget& clear_theme() noexcept { m_theme.reset(); return *this; }
    const Theme& theme() const noexcept;

    bool hovered() const noexcept;
    bool hovered_directly() const noexcept;
    bool pressed() const noexcept;
    bool focused() const noexcept;
    bool focus_within() const noexcept;
    bool focus_visible() const noexcept;
    void focus();
    void blur();

    text::TextStyle text_style(const Color& c, double size = 0.0, bool bold = false) const;
    Size text_size(const std::string& s, double size = 0.0, bool bold = false) const;
    float text_width(const std::string& s, double size = 0.0, bool bold = false) const { return text_size(s, size, bold).w; }
    float line_height(double size = 0.0) const { return text_size("Ag", size).h; }

    Size preferred(Ui& ui) {
        Size s = measure(ui);
        if (m_fixed_w >= 0.0f) s.w = m_fixed_w;
        if (m_fixed_h >= 0.0f) s.h = m_fixed_h;
        s.w = std::min(std::max(s.w, m_min_w), m_max_w);
        s.h = std::min(std::max(s.h, m_min_h), m_max_h);
        return s;
    }

    virtual Size measure(Ui&) { return Size{ 0.0f, 0.0f }; }
    virtual void arrange(Ui& ui) { for (auto& c : m_children) if (c->visible()) c->arrange(ui); }
    virtual void draw(Painter& p, Ui& ui) { draw_children(p, ui); }
    virtual void draw_overlay(Painter&, Ui&) {}
    virtual Point content_offset() const noexcept { return Point{}; }
    virtual Rect content_clip() const noexcept { return bounds(); }
    virtual bool clips_children() const noexcept { return false; }
    virtual bool hit_self(float, float) const noexcept { return true; }
    virtual Widget* hit_test(float x, float y);
    virtual windows::SystemCursor cursor(float, float) const noexcept { return windows::SystemCursor::Arrow; }

    virtual bool on_mouse_down(MouseEvent&) { return false; }
    virtual void on_mouse_up(MouseEvent&) {}
    virtual void on_mouse_move(MouseEvent&) {}
    virtual bool on_wheel(MouseEvent&) { return false; }
    virtual bool on_key(const KeyEvent&) { return false; }
    virtual bool on_key_up(const KeyEvent&) { return false; }
    virtual bool on_text(const std::string&) { return false; }
    virtual void on_preedit(const std::string&, int) {}
    virtual void on_focus_changed(bool) {}
    virtual bool on_files_dropped(const std::vector<std::string>&) { return false; }
    virtual bool on_gamepad_button(input::GamepadButton) { return false; }
    virtual void update(Ui&, double) {}
    virtual bool wants_text_input() const noexcept { return false; }
    virtual bool captures_all_keys() const noexcept { return false; }
    virtual void reveal(const Rect&) {}

    void draw_children(Painter& p, Ui& ui);
    void draw_focus_ring(Painter& p, float radius = -1.0f) const;
};

class Painter {
private:
    windows::Renderer*                  m_r = nullptr;
    float                               m_scale = 1.0f;
    float                               m_ox = 0.0f, m_oy = 0.0f;
    std::vector<Point>                  m_offsets;
    std::vector<Rect>                   m_clips;
    float                               m_opacity = 1.0f;
    std::vector<float>                  m_opacities;
    bool                                m_software = false;

    static const graphics::Texture& solid_texture(const Color& c) {
        static std::unordered_map<std::uint32_t, graphics::Texture> cache;
        const std::uint32_t key = (static_cast<std::uint32_t>(c.red()) << 16) | (static_cast<std::uint32_t>(c.green()) << 8) | c.blue();
        auto it = cache.find(key);
        if (it != cache.end()) return it->second;
        if (cache.size() > 512) cache.clear();
        images::BitmapImage img(1, 1);
        img.set_pixel(0, 0, Color(c.red(), c.green(), c.blue()));
        return cache.emplace(key, graphics::Texture(std::move(img), graphics::SampleFilter::Bilinear)).first->second;
    }

    static graphics::Texture ramp_texture(const Color& a, const Color& b, bool vertical) {
        const unsigned int n = 64;
        images::BitmapImage img(vertical ? 1 : n, vertical ? n : 1);
        for (unsigned int i = 0; i < n; ++i) {
            const Color c = mix(a, b, static_cast<float>(i) / (n - 1));
            if (vertical) img.set_pixel(0, i, c); else img.set_pixel(i, 0, c);
        }
        return graphics::Texture(std::move(img), graphics::SampleFilter::Bilinear);
    }

    static const graphics::Texture& tinted_texture(const graphics::Texture& tex, const Color& tint) {
        static const images::BitmapImage* src_key = nullptr;
        static std::uint32_t tint_key = 0;
        static graphics::Texture cached;
        const std::uint32_t tk = (static_cast<std::uint32_t>(tint.red()) << 16) | (static_cast<std::uint32_t>(tint.green()) << 8) | tint.blue();
        if (src_key == tex.image() && tint_key == tk && cached.valid()) return cached;
        const images::BitmapImage& src = *tex.image();
        images::BitmapImage img(src.width(), src.height());
        for (unsigned int y = 0; y < src.height(); ++y) for (unsigned int x = 0; x < src.width(); ++x) {
            const Color c = src.get_pixel(x, y);
            img.set_pixel(x, y, Color(static_cast<std::uint8_t>(c.red() * tint.red() / 255), static_cast<std::uint8_t>(c.green() * tint.green() / 255), static_cast<std::uint8_t>(c.blue() * tint.blue() / 255), c.alpha()));
        }
        cached = graphics::Texture(std::move(img), graphics::SampleFilter::Bilinear);
        src_key = tex.image();
        tint_key = tk;
        return cached;
    }

    void blend_rect(const Rect& r, const Color& c) {
        const Pix p = pix(r);
        if (!p.w || !p.h) return;
        const graphics::Texture& t = solid_texture(c);
        m_r->draw_texture(t, p.x, p.y, p.w, p.h, c.alpha() / 255.0f);
    }

    int px(float v) const noexcept { return static_cast<int>(std::lround(v * m_scale)); }

    Color fade(const Color& c) const noexcept {
        if (m_opacity >= 1.0f) return c;
        return Color(c.red(), c.green(), c.blue(), static_cast<std::uint8_t>(std::lround(c.alpha() * m_opacity)));
    }

    void apply_clip() {
        if (m_clips.empty()) { m_r->reset_clip_rect(); return; }
        const Rect& c = m_clips.back();
        const int x0 = static_cast<int>(std::floor(c.x * m_scale)), y0 = static_cast<int>(std::floor(c.y * m_scale));
        const int x1 = static_cast<int>(std::ceil(c.right() * m_scale)), y1 = static_cast<int>(std::ceil(c.bottom() * m_scale));
        m_r->set_clip_rect(x0, y0, static_cast<unsigned int>(std::max(0, x1 - x0)), static_cast<unsigned int>(std::max(0, y1 - y0)));
    }

    struct Pix { int x, y; unsigned int w, h; };

    Pix pix(const Rect& r) const noexcept {
        const int x0 = px(r.x + m_ox), y0 = px(r.y + m_oy);
        const int x1 = px(r.right() + m_ox), y1 = px(r.bottom() + m_oy);
        return Pix{ x0, y0, static_cast<unsigned int>(std::max(0, x1 - x0)), static_cast<unsigned int>(std::max(0, y1 - y0)) };
    }

public:
    Painter(windows::Renderer& r, float scale) noexcept : m_r(&r), m_scale(scale > 0.0f ? scale : 1.0f), m_software(!r.is_gpu()) {}

    bool software() const noexcept { return m_software; }

    windows::Renderer& renderer() noexcept { return *m_r; }
    float scale() const noexcept { return m_scale; }
    Point offset() const noexcept { return Point{ m_ox, m_oy }; }

    void push_offset(float dx, float dy) { m_offsets.push_back(Point{ m_ox, m_oy }); m_ox += dx; m_oy += dy; }
    void pop_offset() { if (m_offsets.empty()) return; m_ox = m_offsets.back().x; m_oy = m_offsets.back().y; m_offsets.pop_back(); }

    void push_clip(const Rect& local) {
        Rect a = local.translated(m_ox, m_oy);
        if (!m_clips.empty()) a = a.intersect(m_clips.back());
        m_clips.push_back(a);
        apply_clip();
    }

    void pop_clip() { if (m_clips.empty()) return; m_clips.pop_back(); apply_clip(); }

    bool visible(const Rect& local) const noexcept {
        if (m_clips.empty()) return true;
        return local.translated(m_ox, m_oy).intersects(m_clips.back());
    }

    void push_opacity(float o) { m_opacities.push_back(m_opacity); m_opacity *= std::max(0.0f, std::min(1.0f, o)); }
    void pop_opacity() { if (m_opacities.empty()) return; m_opacity = m_opacities.back(); m_opacities.pop_back(); }

    void fill_rect(const Rect& r, const Color& c) {
        if (c.alpha() == 0 || r.empty()) return;
        const Color f = fade(c);
        if (m_software && f.alpha() < 255) { blend_rect(r, f); return; }
        const Pix p = pix(r);
        if (p.w && p.h) m_r->draw_rect(p.x, p.y, p.w, p.h, graphics::Paint::fill(f));
    }

    void stroke_rect(const Rect& r, const Color& c, float width = 1.0f) {
        if (c.alpha() == 0 || r.empty()) return;
        const float t = std::min(width, std::min(r.w, r.h) * 0.5f);
        fill_rect(Rect(r.x, r.y, r.w, t), c);
        fill_rect(Rect(r.x, r.bottom() - t, r.w, t), c);
        fill_rect(Rect(r.x, r.y + t, t, r.h - 2.0f * t), c);
        fill_rect(Rect(r.right() - t, r.y + t, t, r.h - 2.0f * t), c);
    }

    void fill_rounded(const Rect& r, float radius, const Color& c) {
        if (c.alpha() == 0 || r.empty()) return;
        if (radius <= 0.5f || (m_software && fade(c).alpha() < 255)) { fill_rect(r, c); return; }
        const Pix p = pix(r);
        const double rad = std::min<double>(radius * m_scale, std::min(p.w, p.h) * 0.5);
        m_r->draw_rounded_rect(p.x, p.y, p.w, p.h, rad, graphics::Paint::fill(fade(c)));
    }

    void stroke_rounded(const Rect& r, float radius, const Color& c, float width = 1.0f) {
        if (c.alpha() == 0 || r.empty()) return;
        if (radius <= 0.5f) { stroke_rect(r, c, width); return; }
        const unsigned int w = static_cast<unsigned int>(std::max(1L, std::lround(width * m_scale)));
        const float half = static_cast<float>(w) * 0.5f / m_scale;
        const Pix p = pix(r.inset(half));
        const double rad = std::min<double>(std::max(0.0f, radius - half) * m_scale, std::min(p.w, p.h) * 0.5);
        m_r->draw_rounded_rect(p.x, p.y, p.w, p.h, rad, graphics::Paint::stroke(fade(c), w));
    }

    void box(const Rect& r, float radius, const Color& fill, const Color& border, float border_width = 1.0f) {
        if (border.alpha() == 0 || border_width <= 0.0f) { fill_rounded(r, radius, fill); return; }
        if (fill.alpha() == 255 && border.alpha() == 255) {
            fill_rounded(r, radius, border);
            fill_rounded(r.inset(border_width), std::max(0.0f, radius - border_width), fill);
            return;
        }
        fill_rounded(r.inset(border_width), std::max(0.0f, radius - border_width), fill);
        stroke_rounded(r, radius, border, border_width);
    }

    void line(float x1, float y1, float x2, float y2, const Color& c, float width = 1.0f) {
        if (c.alpha() == 0) return;
        const float ax = (x1 + m_ox) * m_scale, ay = (y1 + m_oy) * m_scale;
        const float bx = (x2 + m_ox) * m_scale, by = (y2 + m_oy) * m_scale;
        const float dx = bx - ax, dy = by - ay;
        const float len = std::sqrt(dx * dx + dy * dy);
        if (len <= 1e-6f) return;
        const float hw = std::max(0.5f, width * m_scale * 0.5f);
        const float nx = -dy / len * hw, ny = dx / len * hw;
        const windows::RenderPoint pts[4] = {
            windows::RenderPoint(ax + nx, ay + ny), windows::RenderPoint(bx + nx, by + ny),
            windows::RenderPoint(bx - nx, by - ny), windows::RenderPoint(ax - nx, ay - ny)
        };
        m_r->draw_polygon(pts, 4, graphics::Paint::fill(fade(c)));
    }

    void polyline(std::initializer_list<Point> pts, const Color& c, float width = 1.0f) {
        const Point* prev = nullptr;
        for (const Point& p : pts) { if (prev) line(prev->x, prev->y, p.x, p.y, c, width); prev = &p; }
        if (pts.size() > 2) {
            const unsigned int r = static_cast<unsigned int>(std::max(0L, std::lround(width * m_scale * 0.5f)));
            const auto* it = pts.begin() + 1;
            for (; it + 1 != pts.end(); ++it) if (r > 0) m_r->draw_circle(px(it->x + m_ox), px(it->y + m_oy), r, graphics::Paint::fill(fade(c)));
        }
    }

    void fill_circle(float cx, float cy, float radius, const Color& c) {
        if (c.alpha() == 0 || radius <= 0.0f) return;
        m_r->draw_circle(px(cx + m_ox), px(cy + m_oy), static_cast<unsigned int>(std::max(1L, std::lround(radius * m_scale))), graphics::Paint::fill(fade(c)));
    }

    void stroke_circle(float cx, float cy, float radius, const Color& c, float width = 1.0f) {
        if (c.alpha() == 0 || radius <= 0.0f) return;
        const unsigned int w = static_cast<unsigned int>(std::max(1L, std::lround(width * m_scale)));
        m_r->draw_circle(px(cx + m_ox), px(cy + m_oy), static_cast<unsigned int>(std::max(1L, std::lround(radius * m_scale))), graphics::Paint::stroke(fade(c), w));
    }

    void fill_triangle(Point a, Point b, Point c, const Color& col) {
        if (col.alpha() == 0) return;
        const windows::RenderPoint pts[3] = {
            windows::RenderPoint((a.x + m_ox) * m_scale, (a.y + m_oy) * m_scale),
            windows::RenderPoint((b.x + m_ox) * m_scale, (b.y + m_oy) * m_scale),
            windows::RenderPoint((c.x + m_ox) * m_scale, (c.y + m_oy) * m_scale)
        };
        m_r->draw_polygon(pts, 3, graphics::Paint::fill(fade(col)));
    }

    void chevron_down(float cx, float cy, float size, const Color& c, float width = 1.5f) {
        polyline({ Point{ cx - size, cy - size * 0.5f }, Point{ cx, cy + size * 0.5f }, Point{ cx + size, cy - size * 0.5f } }, c, width);
    }

    void gradient_rect(const Rect& r, const Color& a, const Color& b, bool vertical) {
        if (r.empty()) return;
        const Pix p = pix(r);
        if (!p.w || !p.h) return;
        if (m_software) { m_r->draw_texture(ramp_texture(fade(a), fade(b), vertical), p.x, p.y, p.w, p.h, 1.0f); return; }
        graphics::Gradient g = vertical
            ? graphics::Gradient::linear(p.x, p.y, p.x, p.y + static_cast<double>(p.h))
            : graphics::Gradient::linear(p.x, p.y, p.x + static_cast<double>(p.w), p.y);
        g.set_colors(fade(a), fade(b));
        m_r->draw_rect(p.x, p.y, p.w, p.h, graphics::Paint::gradient_fill(g));
    }

    void checkerboard(const Rect& r, float cell, const Color& a, const Color& b) {
        fill_rect(r, a);
        if (cell <= 0.0f) return;
        const int nx = static_cast<int>(std::ceil(r.w / cell)), ny = static_cast<int>(std::ceil(r.h / cell));
        for (int j = 0; j < ny; ++j) for (int i = (j & 1); i < nx; i += 2) {
            const Rect c(r.x + i * cell, r.y + j * cell, std::min(cell, r.w - i * cell), std::min(cell, r.h - j * cell));
            fill_rect(c, b);
        }
    }

    void texture(const graphics::Texture& tex, const Rect& r, const Color& tint = Color(255, 255, 255)) {
        if (r.empty()) return;
        const Pix p = pix(r);
        if (!p.w || !p.h) return;
        const Color t = fade(tint);
        if (m_software) {
            const graphics::Texture& use = graphics::is_white(t) ? tex : tinted_texture(tex, t);
            m_r->draw_texture(use, p.x, p.y, p.w, p.h, std::min(0.999f, t.alpha() / 255.0f));
            return;
        }
        m_r->draw_texture(tex, p.x, p.y, p.w, p.h, tex.full_rect(), Color(t.red(), t.green(), t.blue()), t.alpha() / 255.0f);
    }

    void text(float x, float y, const std::string& s, const text::TextStyle& st) {
        if (s.empty()) return;
        text::TextStyle sc = st;
        sc.set_size((st.has_size() ? st.size() : 14.0) * m_scale);
        if (m_opacity < 1.0f && st.has_fg_color()) sc.set_fg_color(fade(st.fg_color().value()));
        m_r->draw_text(px(x + m_ox), px(y + m_oy), s, sc);
    }
};

class Ui {
    friend class Widget;

public:
    using Clock = std::chrono::steady_clock;

private:
    struct Overlay {
        Widget*                 owner = nullptr;
        Widget*                 content = nullptr;
        Rect                    anchor;
        Placement               placement = Placement::Below;
        bool                    modal = false;
        std::unique_ptr<Widget> owned;
        Widget*                 restore_focus = nullptr;
        std::function<void()>   on_close;
    };

    Theme                                     m_theme;
    float                                     m_scale = 1.0f;
    windows::Window*                          m_window = nullptr;
    windows::Renderer*                        m_renderer = nullptr;
    std::unique_ptr<Widget>                   m_root;
    std::vector<Overlay>                      m_overlays;
    std::vector<std::unique_ptr<Widget>>      m_graveyard;
    std::vector<std::function<void()>>        m_deferred;
    std::vector<std::uint64_t>                m_listeners;
    Widget*                                   m_hover = nullptr;
    Widget*                                   m_pressed = nullptr;
    Widget*                                   m_focus = nullptr;
    input::MouseButton                        m_pressed_button = input::MouseButton::None;
    bool                                      m_focus_visible = false;
    bool                                      m_text_input = false;
    bool                                      m_dying = false;
    bool                                      m_auto_scale = true;
    float                                     m_mouse_x = -1e6f, m_mouse_y = -1e6f;
    double                                    m_time = 0.0;
    double                                    m_blink_start = 0.0;
    bool                                      m_updated = false;
    Clock::time_point                         m_last_draw{};
    bool                                      m_have_last_draw = false;
    Widget*                                   m_tip_widget = nullptr;
    double                                    m_tip_timer = 0.0;
    bool                                      m_tip_shown = false;
    float                                     m_tip_x = 0.0f, m_tip_y = 0.0f;
    std::string                               m_clipboard;
    windows::SystemCursor                     m_cursor = windows::SystemCursor::Arrow;
    Size                                      m_viewport{ 800.0f, 600.0f };
    std::unordered_map<std::string, Size>     m_text_cache;
    bool                                      m_consumed_mouse = false;

    static Point mouse_of(const windows::WindowEvent& e) noexcept {
        if (e.fx != 0.0f || e.fy != 0.0f) return Point{ e.fx, e.fy };
        return Point{ static_cast<float>(e.x), static_cast<float>(e.y) };
    }

    MouseEvent make_mouse(const Widget* w, float sx, float sy, const windows::WindowEvent& e) const noexcept {
        MouseEvent m;
        const Rect r = w ? w->screen_rect() : Rect();
        m.screen_x = sx; m.screen_y = sy;
        m.x = sx - r.x; m.y = sy - r.y;
        m.mods = e.mods;
        m.button = static_cast<input::MouseButton>(e.button <= 5 ? e.button : 0);
        return m;
    }

    void forget(Widget* w) noexcept {
        if (m_dying || !w) return;
        if (m_hover == w) m_hover = nullptr;
        if (m_pressed == w) m_pressed = nullptr;
        if (m_tip_widget == w) { m_tip_widget = nullptr; m_tip_shown = false; }
        if (m_focus == w) { m_focus = nullptr; update_text_input(); }
        for (std::size_t i = 0; i < m_overlays.size();) {
            Overlay& o = m_overlays[i];
            if (o.restore_focus == w) o.restore_focus = nullptr;
            if (o.content == w || o.owner == w) {
                if (o.owned) m_graveyard.push_back(std::move(o.owned));
                m_overlays.erase(m_overlays.begin() + static_cast<std::ptrdiff_t>(i));
            } else ++i;
        }
    }

    void update_text_input() noexcept {
        const bool want = m_focus && m_focus->wants_text_input() && m_focus->enabled();
        if (!m_window) { m_text_input = want; return; }
        if (want) {
            const Rect r = m_focus->screen_rect();
            m_window->set_text_input_rect(static_cast<int>(r.x * m_scale), static_cast<int>(r.y * m_scale), static_cast<unsigned int>(r.w * m_scale), static_cast<unsigned int>(r.h * m_scale));
            if (!m_text_input || !m_window->text_input_active()) m_window->start_text_input();
            m_text_input = true;
        } else if (m_text_input) {
            m_window->stop_text_input();
            m_text_input = false;
        }
    }

    Widget* scope_root() const noexcept {
        for (auto it = m_overlays.rbegin(); it != m_overlays.rend(); ++it) {
            if (it->modal) return it->content;
            if (m_focus && it->content->is_ancestor_of(m_focus)) return it->content;
        }
        return m_root.get();
    }

    static void collect_focusable(Widget* w, std::vector<Widget*>& out) {
        if (!w || !w->visible() || !w->enabled()) return;
        if (w->focusable()) out.push_back(w);
        for (auto& c : w->m_children) collect_focusable(c.get(), out);
    }

    void run_deferred() {
        for (std::size_t guard = 0; guard < 8 && !m_deferred.empty(); ++guard) {
            std::vector<std::function<void()>> d;
            d.swap(m_deferred);
            for (auto& f : d) if (f) f();
        }
        m_graveyard.clear();
    }

    void set_hover(Widget* w) {
        m_hover = w;
        windows::SystemCursor c = windows::SystemCursor::Arrow;
        Widget* cw = m_pressed ? m_pressed : w;
        if (cw && cw->enabled()) {
            const Rect r = cw->screen_rect();
            c = cw->cursor(m_mouse_x - r.x, m_mouse_y - r.y);
        }
        if (c != m_cursor) { m_cursor = c; if (m_window) m_window->set_cursor(c); }
    }

    void update_tree(Widget* w, double dt) {
        if (!w || !w->visible()) return;
        w->update(*this, dt);
        for (std::size_t i = 0; i < w->m_children.size(); ++i) update_tree(w->m_children[i].get(), dt);
    }

    Rect place(const Overlay& o, Size s) const noexcept {
        s.w = std::min(s.w, m_viewport.w);
        s.h = std::min(s.h, m_viewport.h);
        float x = 0.0f, y = 0.0f;
        switch (o.placement) {
            case Placement::Below:
                x = o.anchor.x; y = o.anchor.bottom() + 2.0f;
                if (y + s.h > m_viewport.h && o.anchor.y - s.h - 2.0f >= 0.0f) y = o.anchor.y - s.h - 2.0f;
                break;
            case Placement::Above:
                x = o.anchor.x; y = o.anchor.y - s.h - 2.0f;
                if (y < 0.0f && o.anchor.bottom() + s.h + 2.0f <= m_viewport.h) y = o.anchor.bottom() + 2.0f;
                break;
            case Placement::Right:
                x = o.anchor.right() + 2.0f; y = o.anchor.y;
                if (x + s.w > m_viewport.w && o.anchor.x - s.w - 2.0f >= 0.0f) x = o.anchor.x - s.w - 2.0f;
                break;
            case Placement::Center:
                x = (m_viewport.w - s.w) * 0.5f; y = (m_viewport.h - s.h) * 0.5f;
                break;
            case Placement::At:
                x = o.anchor.x; y = o.anchor.y;
                break;
        }
        x = std::max(0.0f, std::min(x, m_viewport.w - s.w));
        y = std::max(0.0f, std::min(y, m_viewport.h - s.h));
        return Rect(std::round(x), std::round(y), s.w, s.h);
    }

    bool dispatch_key(const KeyEvent& k) {
        Widget* w = m_focus;
        if (!w) {
            for (auto it = m_overlays.rbegin(); it != m_overlays.rend(); ++it) { w = it->content; break; }
        }
        for (; w; w = w->m_parent) if (w->enabled() && w->on_key(k)) return true;
        return false;
    }

public:
    Ui() { m_root = make_root(); m_root->set_ui_tree(this); }
    explicit Ui(windows::Window& window) : Ui() { attach(window); }
    Ui(const Ui&) = delete;
    Ui& operator=(const Ui&) = delete;

    ~Ui() {
        detach();
        m_dying = true;
        m_overlays.clear();
        m_graveyard.clear();
        m_root.reset();
    }

    static std::unique_ptr<Widget> make_root();

    Widget& root() noexcept { return *m_root; }

    template <typename T, typename... Args>
    T& set_root(Args&&... args) {
        auto p = std::make_unique<T>(std::forward<Args>(args)...);
        T& ref = *p;
        if (m_root) { m_graveyard.push_back(std::move(m_root)); m_graveyard.back()->set_ui_tree(nullptr); }
        m_root = std::move(p);
        m_root->set_ui_tree(this);
        return ref;
    }

    template <typename T, typename... Args>
    T& add(Args&&... args) { return m_root->template add<T>(std::forward<Args>(args)...); }

    Theme& theme() noexcept { m_text_cache.clear(); return m_theme; }
    const Theme& theme() const noexcept { return m_theme; }
    void set_theme(const Theme& t) { m_theme = t; m_text_cache.clear(); }
    float scale() const noexcept { return m_scale; }
    void set_scale(float s) noexcept { m_scale = s > 0.05f ? s : 1.0f; m_auto_scale = false; m_text_cache.clear(); }
    void set_auto_scale(bool on) noexcept { m_auto_scale = on; }
    Size viewport() const noexcept { return m_viewport; }
    windows::Window* window() const noexcept { return m_window; }
    windows::Renderer* renderer() const noexcept { return m_renderer; }
    double time() const noexcept { return m_time; }
    Point mouse() const noexcept { return Point{ m_mouse_x, m_mouse_y }; }

    void attach(windows::Window& window) {
        detach();
        m_window = &window;
        if (m_auto_scale && window.dpi_scale() > 0.0f) m_scale = window.dpi_scale();
        using T = windows::WindowEventType;
        const T types[] = { T::MouseMove, T::MouseClick, T::MouseRelease, T::MouseScroll, T::MouseDoubleClick, T::KeyPress, T::KeyRelease,
                            T::TextInput, T::TextEditing, T::FilesDropped, T::WindowBlur, T::GamepadButtonDown, T::DpiChanged };
        for (T t : types) m_listeners.push_back(window.add_event_listener(t, [this](const windows::WindowEvent& e) { handle_event(e); }));
    }

    void detach() {
        if (m_window) {
            for (std::uint64_t id : m_listeners) m_window->remove_event_listener(id);
            if (m_text_input) m_window->stop_text_input();
        }
        m_listeners.clear();
        m_text_input = false;
        m_window = nullptr;
    }

    text::TextStyle text_style(const Theme& th, const Color& c, double size = 0.0, bool bold = false) const {
        text::TextStyle st(size > 0.0 ? size : th.font_size, c);
        if (bold) st.set_bold(true);
        if (th.font != text::Font::None) st.set_font(th.font);
        return st;
    }

    Size text_size(const std::string& s, const text::TextStyle& st) {
        const double size = st.has_size() ? st.size() : m_theme.font_size;
        if (s.empty()) {
            Size h = text_size(std::string("Ag"), st);
            return Size{ 0.0f, h.h };
        }
        std::string key;
        key.reserve(s.size() + 24);
        key += std::to_string(static_cast<int>(size * 64.0));
        key += st.is_bold() ? 'b' : 'r';
        key += st.is_italic() ? 'i' : 'n';
        key += std::to_string(static_cast<int>(st.font()));
        key += '\x1f';
        key += s;
        auto it = m_text_cache.find(key);
        if (it != m_text_cache.end()) return it->second;
        Size out;
        if (m_renderer && m_renderer->is_bound()) {
            text::TextStyle sc = st;
            sc.set_size(size * m_scale);
            const text::TextMetrics m = m_renderer->measure_text(s, sc);
            out = Size{ m.width / m_scale, m.height / m_scale };
        } else {
            std::size_t cps = 0;
            for (unsigned char c : s) if ((c & 0xC0) != 0x80) ++cps;
            out = Size{ static_cast<float>(cps * size * 0.55), static_cast<float>(size * 1.25) };
        }
        if (out.h <= 0.0f) out.h = static_cast<float>(size * 1.25);
        if (m_text_cache.size() > 8192) m_text_cache.clear();
        if (m_renderer) m_text_cache.emplace(std::move(key), out);
        return out;
    }

    Widget* focused() const noexcept { return m_focus; }
    Widget* hovered() const noexcept { return m_hover; }
    Widget* pressed() const noexcept { return m_pressed; }
    bool focus_visible() const noexcept { return m_focus_visible; }

    void set_focus(Widget* w, bool from_keyboard = false) {
        if (w && (!w->visible() || !w->enabled())) w = nullptr;
        m_focus_visible = from_keyboard;
        if (w == m_focus) return;
        Widget* old = m_focus;
        m_focus = w;
        m_blink_start = m_time;
        if (old) old->on_focus_changed(false);
        if (m_focus == w && w) w->on_focus_changed(true);
        if (w && from_keyboard) {
            const Rect r = w->screen_rect();
            for (Widget* p = w->m_parent; p; p = p->m_parent) p->reveal(r);
        }
        update_text_input();
    }

    void clear_focus() { set_focus(nullptr); }

    bool focus_next(bool backward = false) {
        std::vector<Widget*> list;
        collect_focusable(scope_root(), list);
        if (list.empty()) return false;
        std::size_t idx = list.size();
        for (std::size_t i = 0; i < list.size(); ++i) if (list[i] == m_focus) idx = i;
        std::size_t next;
        if (idx == list.size()) next = backward ? list.size() - 1 : 0;
        else next = backward ? (idx + list.size() - 1) % list.size() : (idx + 1) % list.size();
        set_focus(list[next], true);
        return true;
    }

    void reset_caret_blink() noexcept { m_blink_start = m_time; }
    bool caret_on() const noexcept {
        const double b = m_theme.caret_blink;
        if (b <= 0.0) return true;
        return std::fmod(m_time - m_blink_start, 2.0 * b) < b;
    }

    std::string clipboard() {
        if (m_window) { std::string s = m_window->clipboard_text(); if (!s.empty()) return s; }
        return m_clipboard;
    }

    void set_clipboard(const std::string& s) {
        m_clipboard = s;
        if (m_window) m_window->set_clipboard_text(s);
    }

    void defer(std::function<void()> fn) { m_deferred.push_back(std::move(fn)); }

    void open_popup(Widget& owner, Widget& content, const Rect& anchor, Placement placement = Placement::Below, std::function<void()> on_close = {}) {
        for (const Overlay& o : m_overlays) if (o.content == &content) return;
        Overlay o;
        o.owner = &owner;
        o.content = &content;
        o.anchor = anchor;
        o.placement = placement;
        o.on_close = std::move(on_close);
        o.restore_focus = m_focus;
        content.set_ui_tree(this);
        m_overlays.push_back(std::move(o));
    }

    bool close_popup(Widget& content) {
        for (std::size_t i = 0; i < m_overlays.size(); ++i) {
            if (m_overlays[i].content != &content) continue;
            Overlay o = std::move(m_overlays[i]);
            m_overlays.erase(m_overlays.begin() + static_cast<std::ptrdiff_t>(i));
            if (m_focus && content.is_ancestor_of(m_focus)) set_focus(o.restore_focus);
            if (m_hover && content.is_ancestor_of(m_hover)) m_hover = nullptr;
            if (m_pressed && content.is_ancestor_of(m_pressed)) m_pressed = nullptr;
            if (o.owned) m_graveyard.push_back(std::move(o.owned));
            if (o.on_close) o.on_close();
            return true;
        }
        return false;
    }

    bool popup_open(const Widget& content) const noexcept {
        for (const Overlay& o : m_overlays) if (o.content == &content) return true;
        return false;
    }

    void set_popup_anchor(const Widget& content, const Rect& anchor) noexcept {
        for (Overlay& o : m_overlays) if (o.content == &content) o.anchor = anchor;
    }

    std::size_t overlay_count() const noexcept { return m_overlays.size(); }
    bool modal_open() const noexcept { for (const Overlay& o : m_overlays) if (o.modal) return true; return false; }

    template <typename T>
    T& show_modal(std::unique_ptr<T> modal) {
        T& ref = *modal;
        Overlay o;
        o.content = modal.get();
        o.placement = Placement::Center;
        o.modal = true;
        o.restore_focus = m_focus;
        o.owned = std::move(modal);
        o.content->set_ui_tree(this);
        m_overlays.push_back(std::move(o));
        if (m_pressed) m_pressed = nullptr;
        std::vector<Widget*> list;
        collect_focusable(&ref, list);
        Widget* first = nullptr;
        for (Widget* w : list) if (w->wants_text_input()) { first = w; break; }
        if (!first && !list.empty()) first = list.back();
        set_focus(first, false);
        return ref;
    }

    void close_modal(Widget& content) {
        for (std::size_t i = 0; i < m_overlays.size(); ++i) {
            if (m_overlays[i].content != &content || !m_overlays[i].modal) continue;
            Widget* restore = m_overlays[i].restore_focus;
            if (m_overlays[i].owned) m_graveyard.push_back(std::move(m_overlays[i].owned));
            Widget* c = m_overlays[i].content;
            m_overlays.erase(m_overlays.begin() + static_cast<std::ptrdiff_t>(i));
            if (m_focus && c->is_ancestor_of(m_focus)) { m_focus = nullptr; set_focus(restore); }
            if (m_hover && c->is_ancestor_of(m_hover)) m_hover = nullptr;
            if (m_pressed && c->is_ancestor_of(m_pressed)) m_pressed = nullptr;
            if (m_tip_widget && c->is_ancestor_of(m_tip_widget)) { m_tip_widget = nullptr; m_tip_shown = false; }
            c->set_ui_tree(nullptr);
            return;
        }
    }

    void alert(const std::string& title, const std::string& message, std::function<void()> on_close = {});
    void confirm(const std::string& title, const std::string& message, std::function<void(bool)> on_result, const std::string& yes = "OK", const std::string& no = "Cancel");
    void prompt(const std::string& title, const std::string& message, const std::string& initial, std::function<void(std::optional<std::string>)> on_result);

    bool wants_mouse() const noexcept {
        if (m_pressed || modal_open()) return true;
        return m_hover != nullptr;
    }

    bool wants_keyboard() const noexcept { return m_focus != nullptr || modal_open(); }
    bool tooltip_visible() const noexcept { return m_tip_shown; }

    Widget* pick(float x, float y) {
        for (auto it = m_overlays.rbegin(); it != m_overlays.rend(); ++it) {
            const Rect r = it->content->m_rect;
            if (r.contains(x, y)) { Widget* h = it->content->hit_test(x - r.x, y - r.y); if (h) return h; }
            if (it->modal) return nullptr;
        }
        return m_root ? m_root->hit_test(x - m_root->m_rect.x, y - m_root->m_rect.y) : nullptr;
    }

    bool handle_event(const windows::WindowEvent& e) {
        using T = windows::WindowEventType;
        bool used = false;
        switch (e.type) {
            case T::MouseMove: {
                const Point p = mouse_of(e);
                m_mouse_x = p.x / m_scale; m_mouse_y = p.y / m_scale;
                if (m_pressed) {
                    MouseEvent m = make_mouse(m_pressed, m_mouse_x, m_mouse_y, e);
                    m.button = m_pressed_button;
                    m_pressed->on_mouse_move(m);
                    set_hover(pick(m_mouse_x, m_mouse_y));
                    used = true;
                } else {
                    Widget* h = pick(m_mouse_x, m_mouse_y);
                    set_hover(h);
                    if (h) { MouseEvent m = make_mouse(h, m_mouse_x, m_mouse_y, e); m.button = input::MouseButton::None; h->on_mouse_move(m); }
                    used = h != nullptr || modal_open();
                }
                break;
            }
            case T::MouseClick: {
                const Point p = mouse_of(e);
                m_mouse_x = p.x / m_scale; m_mouse_y = p.y / m_scale;
                m_tip_shown = false; m_tip_widget = nullptr; m_tip_timer = 0.0;
                Widget* target = pick(m_mouse_x, m_mouse_y);
                bool closed = false;
                for (std::size_t i = m_overlays.size(); i-- > 0;) {
                    Overlay& o = m_overlays[i];
                    if (o.modal) break;
                    if (target && o.content->is_ancestor_of(target)) break;
                    Widget* owner = o.owner;
                    close_popup(*o.content);
                    closed = true;
                    if (owner && target && owner->is_ancestor_of(target)) { run_deferred(); return true; }
                }
                if (closed) target = pick(m_mouse_x, m_mouse_y);
                m_focus_visible = false;
                Widget* f = target;
                while (f && !(f->focusable() && f->enabled())) f = f->m_parent;
                const bool gained = f && f != m_focus;
                if (f != m_focus) set_focus(f);
                for (Widget* w = target; w; w = w->m_parent) {
                    if (!w->enabled()) continue;
                    MouseEvent m = make_mouse(w, m_mouse_x, m_mouse_y, e);
                    m.focus_gained = gained;
                    if (w->on_mouse_down(m)) { m_pressed = w; m_pressed_button = m.button; break; }
                }
                m_blink_start = m_time;
                used = target != nullptr || closed || modal_open();
                set_hover(target);
                break;
            }
            case T::MouseDoubleClick: {
                Widget* w = m_pressed ? m_pressed : pick(m_mouse_x, m_mouse_y);
                if (w && w->enabled()) {
                    MouseEvent m = make_mouse(w, m_mouse_x, m_mouse_y, e);
                    m.clicks = 2;
                    if (w->on_mouse_down(m) && !m_pressed) { m_pressed = w; m_pressed_button = m.button; }
                    used = true;
                }
                break;
            }
            case T::MouseRelease: {
                const Point p = mouse_of(e);
                m_mouse_x = p.x / m_scale; m_mouse_y = p.y / m_scale;
                if (m_pressed) {
                    Widget* w = m_pressed;
                    MouseEvent m = make_mouse(w, m_mouse_x, m_mouse_y, e);
                    m_pressed = nullptr;
                    w->on_mouse_up(m);
                    used = true;
                }
                set_hover(pick(m_mouse_x, m_mouse_y));
                used = used || m_hover != nullptr || modal_open();
                break;
            }
            case T::MouseScroll: {
                Widget* t = pick(m_mouse_x, m_mouse_y);
                MouseEvent m;
                m.wheel_y = e.scroll_y != 0.0f ? e.scroll_y : static_cast<float>(e.scroll_delta);
                m.wheel_x = e.scroll_x;
                m.mods = e.mods;
                if (m.shift() && m.wheel_x == 0.0f) { m.wheel_x = m.wheel_y; m.wheel_y = 0.0f; }
                for (Widget* w = t; w; w = w->m_parent) {
                    if (!w->enabled()) continue;
                    const Rect r = w->screen_rect();
                    m.x = m_mouse_x - r.x; m.y = m_mouse_y - r.y;
                    if (w->on_wheel(m)) { used = true; break; }
                }
                m_tip_shown = false;
                used = used || t != nullptr || modal_open();
                break;
            }
            case T::KeyPress: {
                KeyEvent k;
                k.key = e.logical_key != input::Key::Unknown ? e.logical_key : e.physical_key;
                k.mods = e.mods;
                k.repeat = e.repeat;
                m_blink_start = m_time;
                m_tip_shown = false;
                if (m_focus && m_focus->captures_all_keys()) { used = m_focus->on_key(k); break; }
                used = dispatch_key(k);
                if (!used && k.key == input::Key::Tab && !k.ctrl() && !k.alt()) used = focus_next(k.shift());
                if (!used && k.key == input::Key::Escape) {
                    for (std::size_t i = m_overlays.size(); i-- > 0;) {
                        if (m_overlays[i].modal) break;
                        close_popup(*m_overlays[i].content);
                        used = true;
                        break;
                    }
                }
                used = used || m_focus != nullptr || modal_open();
                break;
            }
            case T::KeyRelease: {
                KeyEvent k;
                k.key = e.logical_key != input::Key::Unknown ? e.logical_key : e.physical_key;
                k.mods = e.mods;
                if (m_focus) used = m_focus->on_key_up(k) || m_focus->captures_all_keys();
                break;
            }
            case T::TextInput:
                if (m_focus && m_focus->enabled()) { used = m_focus->on_text(e.text); m_blink_start = m_time; }
                break;
            case T::TextEditing:
                if (m_focus && m_focus->enabled()) { m_focus->on_preedit(e.text, e.text_cursor); used = true; }
                break;
            case T::FilesDropped: {
                const Point p = mouse_of(e);
                for (Widget* w = pick(p.x / m_scale, p.y / m_scale); w; w = w->m_parent) if (w->enabled() && w->on_files_dropped(e.paths)) { used = true; break; }
                break;
            }
            case T::GamepadButtonDown:
                if (m_focus && m_focus->enabled()) used = m_focus->on_gamepad_button(e.gamepad_button);
                break;
            case T::WindowBlur:
                if (m_pressed) { Widget* w = m_pressed; MouseEvent m = make_mouse(w, m_mouse_x, m_mouse_y, e); m_pressed = nullptr; w->on_mouse_up(m); }
                m_tip_shown = false; m_tip_widget = nullptr;
                break;
            case T::DpiChanged:
                if (m_auto_scale && e.dpi_scale > 0.0f) { m_scale = e.dpi_scale; m_text_cache.clear(); }
                break;
            default:
                break;
        }
        run_deferred();
        m_consumed_mouse = used;
        return used;
    }

    void update(double dt) {
        if (dt < 0.0) dt = 0.0;
        m_time += dt;
        m_updated = true;
        update_tree(m_root.get(), dt);
        for (std::size_t i = 0; i < m_overlays.size(); ++i) update_tree(m_overlays[i].content, dt);
        Widget* tip = nullptr;
        if (!m_pressed) for (Widget* w = m_hover; w; w = w->m_parent) if (!w->tooltip().empty()) { tip = w; break; }
        if (tip != m_tip_widget) { m_tip_widget = tip; m_tip_timer = 0.0; m_tip_shown = false; }
        else if (tip) {
            m_tip_timer += dt;
            if (!m_tip_shown && m_tip_timer >= m_theme.tooltip_delay) { m_tip_shown = true; m_tip_x = m_mouse_x + 12.0f; m_tip_y = m_mouse_y + 18.0f; }
        }
        run_deferred();
    }

    void layout(windows::Renderer& r) {
        m_renderer = &r;
        if (m_auto_scale && m_window && m_window->dpi_scale() > 0.0f && std::abs(m_window->dpi_scale() - m_scale) > 1e-3f) { m_scale = m_window->dpi_scale(); m_text_cache.clear(); }
        m_viewport = Size{ r.viewport_width() / m_scale, r.viewport_height() / m_scale };
        if (m_root) {
            m_root->m_rect = Rect(0.0f, 0.0f, m_viewport.w, m_viewport.h);
            m_root->arrange(*this);
        }
        for (Overlay& o : m_overlays) {
            if (o.owner) o.anchor = o.owner->screen_rect();
            const Size s = o.content->preferred(*this);
            o.content->m_rect = place(o, s);
            o.content->arrange(*this);
        }
    }

    void draw(windows::Renderer& r) {
        const Clock::time_point now = Clock::now();
        if (!m_updated) update(m_have_last_draw ? std::chrono::duration<double>(now - m_last_draw).count() : 0.0);
        m_last_draw = now;
        m_have_last_draw = true;
        m_updated = false;
        run_deferred();
        r.push_transform();
        r.reset_transform();
        layout(r);
        Painter p(r, m_scale);
        if (m_root && m_root->visible()) {
            p.push_offset(m_root->m_rect.x, m_root->m_rect.y);
            m_root->draw(p, *this);
            p.pop_offset();
        }
        for (std::size_t i = 0; i < m_overlays.size(); ++i) {
            Overlay& o = m_overlays[i];
            if (o.modal) p.fill_rect(Rect(0.0f, 0.0f, m_viewport.w, m_viewport.h), m_theme.overlay);
            const Rect rc = o.content->m_rect;
            p.fill_rounded(rc.translated(0.0f, 3.0f).inset(-2.0f, -1.0f), m_theme.radius + 2.0f, m_theme.shadow);
            p.push_offset(rc.x, rc.y);
            o.content->draw(p, *this);
            p.pop_offset();
        }
        if (m_root) { p.push_offset(m_root->m_rect.x, m_root->m_rect.y); draw_overlays(m_root.get(), p); p.pop_offset(); }
        if (m_tip_shown && m_tip_widget) draw_tooltip(p, m_tip_widget->tooltip());
        r.reset_clip_rect();
        r.pop_transform();
    }

private:
    void draw_overlays(Widget* w, Painter& p) {
        if (!w->visible()) return;
        w->draw_overlay(p, *this);
        const Point o = w->content_offset();
        for (auto& c : w->m_children) {
            p.push_offset(c->m_rect.x + o.x, c->m_rect.y + o.y);
            draw_overlays(c.get(), p);
            p.pop_offset();
        }
    }

    void draw_tooltip(Painter& p, const std::string& text) {
        std::vector<std::string> lines;
        std::size_t start = 0;
        for (std::size_t i = 0; i <= text.size(); ++i) if (i == text.size() || text[i] == '\n') { lines.push_back(text.substr(start, i - start)); start = i + 1; }
        const text::TextStyle st = text_style(m_theme, m_theme.tooltip_text, m_theme.font_size * 0.92);
        float w = 0.0f, lh = 0.0f;
        for (const std::string& l : lines) { const Size s = text_size(l, st); w = std::max(w, s.w); lh = std::max(lh, s.h); }
        if (lh <= 0.0f) lh = static_cast<float>(m_theme.font_size * 1.2);
        const float pad = 6.0f;
        Rect r(m_tip_x, m_tip_y, w + pad * 2.0f, lh * lines.size() + pad * 2.0f - 2.0f);
        if (r.right() > m_viewport.w) r.x = std::max(0.0f, m_viewport.w - r.w - 2.0f);
        if (r.bottom() > m_viewport.h) r.y = std::max(0.0f, m_tip_y - 24.0f - r.h);
        p.fill_rounded(r.translated(0.0f, 2.0f), m_theme.radius, m_theme.shadow);
        p.fill_rounded(r, m_theme.radius, m_theme.tooltip_bg);
        for (std::size_t i = 0; i < lines.size(); ++i) p.text(r.x + pad, r.y + pad - 1.0f + lh * i, lines[i], st);
    }
};

inline Widget::~Widget() {
    if (m_ui) m_ui->forget(this);
    for (auto& c : m_children) c->m_parent = nullptr;
}

inline void Widget::set_ui_tree(Ui* ui) noexcept {
    if (m_ui && m_ui != ui) m_ui->forget(this);
    m_ui = ui;
    for (auto& c : m_children) c->set_ui_tree(ui);
}

inline Widget& Widget::adopt(std::unique_ptr<Widget> child) {
    if (child->m_parent) child = child->m_parent->release(child.get());
    child->m_parent = this;
    child->set_ui_tree(m_ui);
    m_children.push_back(std::move(child));
    return *m_children.back();
}

inline std::unique_ptr<Widget> Widget::release(Widget* child) {
    for (auto it = m_children.begin(); it != m_children.end(); ++it) {
        if (it->get() != child) continue;
        std::unique_ptr<Widget> out = std::move(*it);
        m_children.erase(it);
        out->set_ui_tree(nullptr);
        out->m_parent = nullptr;
        return out;
    }
    return nullptr;
}

inline void Widget::clear_children() {
    std::vector<std::unique_ptr<Widget>> old;
    old.swap(m_children);
    for (auto& c : old) { c->set_ui_tree(nullptr); c->m_parent = nullptr; }
    if (m_ui) { Ui* u = m_ui; u->defer([held = std::make_shared<std::vector<std::unique_ptr<Widget>>>(std::move(old))]() mutable { held->clear(); }); }
}

inline Rect Widget::screen_rect() const noexcept {
    Rect r = m_rect;
    for (const Widget* p = m_parent; p; p = p->m_parent) {
        const Point o = p->content_offset();
        r.x += p->m_rect.x + o.x;
        r.y += p->m_rect.y + o.y;
    }
    return r;
}

inline Widget& Widget::set_visible(bool v) noexcept {
    if (m_visible == v) return *this;
    m_visible = v;
    if (!v && m_ui) {
        if (m_ui->m_focus && is_ancestor_of(m_ui->m_focus)) m_ui->set_focus(nullptr);
        if (m_ui->m_hover && is_ancestor_of(m_ui->m_hover)) m_ui->m_hover = nullptr;
        if (m_ui->m_pressed && is_ancestor_of(m_ui->m_pressed)) m_ui->m_pressed = nullptr;
    }
    return *this;
}

inline Widget& Widget::set_enabled(bool e) noexcept {
    m_enabled = e;
    if (!e && m_ui) {
        if (m_ui->m_focus && is_ancestor_of(m_ui->m_focus)) m_ui->set_focus(nullptr);
        if (m_ui->m_pressed && is_ancestor_of(m_ui->m_pressed)) m_ui->m_pressed = nullptr;
    }
    return *this;
}

inline const Theme& Widget::theme() const noexcept {
    for (const Widget* w = this; w; w = w->m_parent) if (w->m_theme) return *w->m_theme;
    if (m_ui) return m_ui->m_theme;
    static const Theme fallback;
    return fallback;
}

inline bool Widget::hovered() const noexcept { return m_ui && m_ui->m_hover && is_ancestor_of(m_ui->m_hover) && (!m_ui->m_pressed || m_ui->m_pressed == this || is_ancestor_of(m_ui->m_pressed)); }
inline bool Widget::hovered_directly() const noexcept { return m_ui && m_ui->m_hover == this; }
inline bool Widget::pressed() const noexcept { return m_ui && m_ui->m_pressed == this; }
inline bool Widget::focused() const noexcept { return m_ui && m_ui->m_focus == this; }
inline bool Widget::focus_within() const noexcept { return m_ui && m_ui->m_focus && is_ancestor_of(m_ui->m_focus); }
inline bool Widget::focus_visible() const noexcept { return focused() && m_ui->m_focus_visible; }
inline void Widget::focus() { if (m_ui) m_ui->set_focus(this); }
inline void Widget::blur() { if (m_ui && focus_within()) m_ui->set_focus(nullptr); }

inline text::TextStyle Widget::text_style(const Color& c, double size, bool bold) const {
    const Theme& t = theme();
    text::TextStyle st(size > 0.0 ? size : t.font_size, c);
    if (bold) st.set_bold(true);
    if (t.font != text::Font::None) st.set_font(t.font);
    return st;
}

inline Size Widget::text_size(const std::string& s, double size, bool bold) const {
    const text::TextStyle st = text_style(Color(), size, bold);
    if (m_ui) return m_ui->text_size(s, st);
    std::size_t cps = 0;
    for (unsigned char c : s) if ((c & 0xC0) != 0x80) ++cps;
    const double sz = st.size();
    return Size{ static_cast<float>(cps * sz * 0.55), static_cast<float>(sz * 1.25) };
}

inline Widget* Widget::hit_test(float x, float y) {
    if (!m_visible || !bounds().contains(x, y)) return nullptr;
    const Rect clip = content_clip();
    if (clip.contains(x, y)) {
        const Point o = content_offset();
        for (auto it = m_children.rbegin(); it != m_children.rend(); ++it) {
            Widget* c = it->get();
            if (!c->m_visible) continue;
            if (Widget* h = c->hit_test(x - o.x - c->m_rect.x, y - o.y - c->m_rect.y)) return h;
        }
    }
    return hit_self(x, y) ? this : nullptr;
}

inline void Widget::draw_children(Painter& p, Ui& ui) {
    const bool clip = clips_children();
    if (clip) p.push_clip(content_clip());
    const Point o = content_offset();
    for (auto& c : m_children) {
        if (!c->m_visible) continue;
        const Rect r = c->m_rect.translated(o.x, o.y);
        if (!p.visible(r.inset(-4.0f))) continue;
        p.push_offset(r.x, r.y);
        c->draw(p, ui);
        p.pop_offset();
    }
    if (clip) p.pop_clip();
}

inline void Widget::draw_focus_ring(Painter& p, float radius) const {
    if (!focus_visible()) return;
    const Theme& t = theme();
    p.stroke_rounded(bounds().inset(-2.0f), (radius < 0.0f ? t.radius : radius) + 2.0f, t.focus, t.focus_width);
}

class Container : public Widget {
protected:
    Color m_background{ 0, 0, 0, 0 };
    Color m_border{ 0, 0, 0, 0 };
    float m_radius = -1.0f;
    float m_padding = 0.0f;
    bool  m_clip = false;
    bool  m_panel_style = false;

    Color background() const noexcept { return m_panel_style && m_background.alpha() == 0 ? theme().panel : m_background; }
    Color border_color() const noexcept { return m_panel_style && m_border.alpha() == 0 ? theme().border : m_border; }

public:
    Container& set_background(const Color& c) noexcept { m_background = c; return *this; }
    Container& set_border(const Color& c) noexcept { m_border = c; return *this; }
    Container& set_radius(float r) noexcept { m_radius = r; return *this; }
    Container& set_padding(float p) noexcept { m_padding = p; return *this; }
    Container& set_clip(bool c) noexcept { m_clip = c; return *this; }
    float padding() const noexcept { return m_padding < 0.0f ? theme().padding : m_padding; }
    float radius() const noexcept { return m_radius < 0.0f ? theme().radius : m_radius; }

    bool hit_self(float, float) const noexcept override { return background().alpha() != 0; }
    bool clips_children() const noexcept override { return m_clip; }

    Size measure(Ui& ui) override {
        Size s;
        for (auto& c : children()) {
            if (!c->visible()) continue;
            const Size p = c->preferred(ui);
            s.w = std::max(s.w, c->rect().x + p.w);
            s.h = std::max(s.h, c->rect().y + p.h);
        }
        return s;
    }

    void arrange(Ui& ui) override {
        for (auto& c : children()) {
            if (!c->visible()) continue;
            Rect r = c->rect();
            const Size p = c->preferred(ui);
            if (r.w <= 0.0f) r.w = p.w;
            if (r.h <= 0.0f) r.h = p.h;
            c->set_rect(r);
            c->arrange(ui);
        }
    }

    void draw(Painter& p, Ui& ui) override {
        const Color bg = background(), bd = border_color();
        if (bg.alpha() || bd.alpha()) p.box(bounds(), radius(), bg, bd, theme().border_width);
        draw_children(p, ui);
    }
};

class Stack : public Container {
protected:
    Orientation m_orientation = Orientation::Vertical;
    float       m_spacing = -1.0f;
    Align       m_cross = Align::Stretch;
    Align       m_main = Align::Start;

public:
    explicit Stack(Orientation o = Orientation::Vertical) noexcept : m_orientation(o) {}

    Stack& set_spacing(float s) noexcept { m_spacing = s; return *this; }
    Stack& set_cross_align(Align a) noexcept { m_cross = a; return *this; }
    Stack& set_main_align(Align a) noexcept { m_main = a; return *this; }
    Stack& set_orientation(Orientation o) noexcept { m_orientation = o; return *this; }
    float spacing() const noexcept { return m_spacing < 0.0f ? theme().spacing : m_spacing; }
    Orientation orientation() const noexcept { return m_orientation; }

    Size measure(Ui& ui) override {
        const bool v = m_orientation == Orientation::Vertical;
        float main = 0.0f, cross = 0.0f;
        int n = 0;
        for (auto& c : children()) {
            if (!c->visible()) continue;
            const Size s = c->preferred(ui);
            main += v ? s.h : s.w;
            cross = std::max(cross, v ? s.w : s.h);
            ++n;
        }
        if (n > 1) main += spacing() * (n - 1);
        const float pad = padding() * 2.0f;
        return v ? Size{ cross + pad, main + pad } : Size{ main + pad, cross + pad };
    }

    void arrange(Ui& ui) override {
        const bool v = m_orientation == Orientation::Vertical;
        const float pad = padding();
        const Rect inner = bounds().inset(pad);
        std::vector<Widget*> items;
        std::vector<Size> sizes;
        float used = 0.0f, flex = 0.0f;
        for (auto& c : children()) {
            if (!c->visible()) continue;
            items.push_back(c.get());
            sizes.push_back(c->preferred(ui));
            used += v ? sizes.back().h : sizes.back().w;
            flex += c->flex();
        }
        if (items.empty()) return;
        const float gap = spacing();
        used += gap * (items.size() - 1);
        const float avail = v ? inner.h : inner.w;
        float extra = avail - used;
        float pos = v ? inner.y : inner.x;
        if (flex <= 0.0f && extra > 0.0f) {
            if (m_main == Align::Center) pos += extra * 0.5f;
            else if (m_main == Align::End) pos += extra;
        }
        for (std::size_t i = 0; i < items.size(); ++i) {
            Widget* w = items[i];
            const Size s = sizes[i];
            float len = v ? s.h : s.w;
            if (flex > 0.0f && w->flex() > 0.0f) len = std::max(0.0f, len + extra * (w->flex() / flex));
            const float cross_avail = v ? inner.w : inner.h;
            float clen = v ? s.w : s.h;
            float cpos = v ? inner.x : inner.y;
            if (m_cross == Align::Stretch) clen = cross_avail;
            else {
                clen = std::min(clen, cross_avail);
                if (m_cross == Align::Center) cpos += (cross_avail - clen) * 0.5f;
                else if (m_cross == Align::End) cpos += cross_avail - clen;
            }
            const Rect r = v ? Rect(cpos, pos, clen, len) : Rect(pos, cpos, len, clen);
            w->set_rect(Rect(std::round(r.x), std::round(r.y), std::round(r.right()) - std::round(r.x), std::round(r.bottom()) - std::round(r.y)));
            w->arrange(ui);
            pos += len + gap;
        }
    }
};

class VStack : public Stack {
public:
    VStack() noexcept : Stack(Orientation::Vertical) {}
    explicit VStack(float spacing) noexcept : Stack(Orientation::Vertical) { m_spacing = spacing; }
};

class HStack : public Stack {
public:
    HStack() noexcept : Stack(Orientation::Horizontal) {}
    explicit HStack(float spacing) noexcept : Stack(Orientation::Horizontal) { m_spacing = spacing; }
};

class Panel : public Stack {
public:
    explicit Panel(Orientation o = Orientation::Vertical) : Stack(o) { m_panel_style = true; m_padding = -1.0f; }
};

class Spacer : public Widget {
public:
    explicit Spacer(float flex = 1.0f) { m_flex = flex; }
    Spacer(float w, float h) { m_fixed_w = w; m_fixed_h = h; }
    bool hit_self(float, float) const noexcept override { return false; }
};

class Separator : public Widget {
    Orientation m_orientation;

public:
    explicit Separator(Orientation o = Orientation::Horizontal) noexcept : m_orientation(o) {}
    bool hit_self(float, float) const noexcept override { return false; }
    Size measure(Ui&) override { return m_orientation == Orientation::Horizontal ? Size{ 1.0f, 9.0f } : Size{ 9.0f, 1.0f }; }
    void draw(Painter& p, Ui&) override {
        const Rect b = bounds();
        if (m_orientation == Orientation::Horizontal) p.fill_rect(Rect(b.x, std::floor(b.center_y()), b.w, 1.0f), theme().border);
        else p.fill_rect(Rect(std::floor(b.center_x()), b.y, 1.0f, b.h), theme().border);
    }
};

class Label : public Widget {
protected:
    std::string m_text;
    Color       m_color{ 0, 0, 0, 0 };
    double      m_size = 0.0;
    bool        m_bold = false;
    bool        m_muted = false;
    Align       m_align = Align::Start;

    std::vector<std::string> lines() const {
        std::vector<std::string> out;
        std::size_t start = 0;
        for (std::size_t i = 0; i <= m_text.size(); ++i) if (i == m_text.size() || m_text[i] == '\n') { out.push_back(m_text.substr(start, i - start)); start = i + 1; }
        return out;
    }

public:
    explicit Label(std::string text = "") : m_text(std::move(text)) {}

    Label& set_text(std::string t) { m_text = std::move(t); return *this; }
    const std::string& text() const noexcept { return m_text; }
    Label& set_color(const Color& c) noexcept { m_color = c; return *this; }
    Label& set_font_size(double s) noexcept { m_size = s; return *this; }
    Label& set_bold(bool b = true) noexcept { m_bold = b; return *this; }
    Label& set_muted(bool m = true) noexcept { m_muted = m; return *this; }
    Label& set_align(Align a) noexcept { m_align = a; return *this; }
    bool hit_self(float, float) const noexcept override { return !m_tooltip.empty(); }

    Size measure(Ui&) override {
        Size s;
        for (const std::string& l : lines()) { const Size t = text_size(l, m_size, m_bold); s.w = std::max(s.w, t.w); s.h += t.h; }
        s.h = std::max(s.h, line_height(m_size));
        return s;
    }

    void draw(Painter& p, Ui&) override {
        const Theme& t = theme();
        const Color c = !enabled() ? t.text_disabled : m_color.alpha() ? m_color : m_muted ? t.text_muted : t.text;
        const text::TextStyle st = text_style(c, m_size, m_bold);
        const auto ls = lines();
        const float lh = line_height(m_size);
        float y = std::round((height() - lh * ls.size()) * 0.5f);
        for (const std::string& l : ls) {
            const float w = text_size(l, m_size, m_bold).w;
            float x = 0.0f;
            if (m_align == Align::Center) x = std::round((width() - w) * 0.5f);
            else if (m_align == Align::End) x = width() - w;
            p.text(x, y, l, st);
            y += lh;
        }
    }
};

class Button : public Widget {
protected:
    std::string           m_text;
    ButtonStyle           m_style = ButtonStyle::Normal;
    std::function<void()> m_on_click;
    bool                  m_armed = false;

public:
    explicit Button(std::string text = "", std::function<void()> on_click = {}) : m_text(std::move(text)), m_on_click(std::move(on_click)) { m_focusable = true; }

    Button& set_text(std::string t) { m_text = std::move(t); return *this; }
    const std::string& text() const noexcept { return m_text; }
    Button& set_style(ButtonStyle s) noexcept { m_style = s; return *this; }
    Button& on_click(std::function<void()> fn) { m_on_click = std::move(fn); return *this; }
    void click() { if (enabled() && m_on_click) m_on_click(); }
    windows::SystemCursor cursor(float, float) const noexcept override { return windows::SystemCursor::Hand; }

    Size measure(Ui&) override {
        const Theme& t = theme();
        return Size{ std::max(t.row_height * 2.4f, text_width(m_text) + t.padding * 2.0f + 8.0f), t.row_height };
    }

    void draw(Painter& p, Ui&) override {
        const Theme& t = theme();
        const bool en = enabled(), hot = hovered() && en, down = pressed() && hovered();
        Color bg = t.surface, fg = en ? t.text : t.text_disabled, bd = t.border;
        switch (m_style) {
            case ButtonStyle::Primary: bg = down ? t.accent_active : hot ? t.accent_hover : t.accent; fg = t.accent_text; bd = Color(0, 0, 0, 0); break;
            case ButtonStyle::Danger:  bg = down ? mix(t.error, Color(0, 0, 0), 0.2f) : hot ? mix(t.error, Color(255, 255, 255), 0.12f) : t.error; fg = Color(255, 255, 255); bd = Color(0, 0, 0, 0); break;
            case ButtonStyle::Flat:    bg = down ? t.surface_active : hot ? t.surface_hover : Color(0, 0, 0, 0); bd = Color(0, 0, 0, 0); break;
            default:                   bg = down ? t.surface_active : hot ? t.surface_hover : t.surface; bd = hot ? t.border_hover : t.border; break;
        }
        if (!en) { bg = m_style == ButtonStyle::Flat ? Color(0, 0, 0, 0) : t.surface_disabled; fg = t.text_disabled; }
        p.box(bounds(), t.radius, bg, bd, t.border_width);
        const Size ts = text_size(m_text);
        p.push_clip(bounds().inset(2.0f));
        p.text(std::round((width() - ts.w) * 0.5f), std::round((height() - ts.h) * 0.5f) + (down ? 1.0f : 0.0f), m_text, text_style(fg));
        p.pop_clip();
        draw_focus_ring(p);
    }

    bool on_mouse_down(MouseEvent& e) override { if (e.button != input::MouseButton::Left) return false; m_armed = true; return true; }
    void on_mouse_up(MouseEvent& e) override { if (m_armed && bounds().contains(e.x, e.y)) click(); m_armed = false; }

    bool on_key(const KeyEvent& k) override {
        if (k.key == input::Key::Space || k.key == input::Key::Enter || k.key == input::Key::NumpadEnter) { if (!k.repeat) click(); return true; }
        return false;
    }
};

class Form : public Widget {
protected:
    struct Row { std::string label; Widget* widget = nullptr; };
    std::vector<Row> m_rows;
    float            m_spacing = -1.0f;
    float            m_label_width = -1.0f;
    float            m_gap = 10.0f;

    float label_column() const {
        if (m_label_width >= 0.0f) return m_label_width;
        float w = 0.0f;
        for (const Row& r : m_rows) if (!r.label.empty() && r.widget->visible()) w = std::max(w, text_width(r.label));
        return w > 0.0f ? w + m_gap : 0.0f;
    }

public:
    Form& set_spacing(float s) noexcept { m_spacing = s; return *this; }
    Form& set_label_width(float w) noexcept { m_label_width = w; return *this; }
    float spacing() const noexcept { return m_spacing < 0.0f ? theme().spacing : m_spacing; }
    bool hit_self(float, float) const noexcept override { return false; }

    template <typename T, typename... Args>
    T& add_row(const std::string& label, Args&&... args) {
        T& w = add<T>(std::forward<Args>(args)...);
        m_rows.push_back(Row{ label, &w });
        return w;
    }

    Widget& add_row(const std::string& label, std::unique_ptr<Widget> w) {
        Widget& ref = adopt(std::move(w));
        m_rows.push_back(Row{ label, &ref });
        return ref;
    }

    Size measure(Ui& ui) override {
        const float lc = label_column();
        float w = 0.0f, h = 0.0f;
        int n = 0;
        for (const Row& r : m_rows) {
            if (!r.widget->visible()) continue;
            const Size s = r.widget->preferred(ui);
            w = std::max(w, (r.label.empty() ? 0.0f : lc) + s.w);
            h += std::max(s.h, r.label.empty() ? 0.0f : line_height());
            ++n;
        }
        if (n > 1) h += spacing() * (n - 1);
        return Size{ w, h };
    }

    void arrange(Ui& ui) override {
        const float lc = label_column();
        float y = 0.0f;
        for (Row& r : m_rows) {
            if (!r.widget->visible()) continue;
            const Size s = r.widget->preferred(ui);
            const float h = std::max(s.h, r.label.empty() ? 0.0f : line_height());
            const float x = r.label.empty() ? 0.0f : lc;
            r.widget->set_rect(Rect(x, std::round(y + (h - s.h) * 0.5f), std::max(0.0f, width() - x), s.h));
            r.widget->arrange(ui);
            y += h + spacing();
        }
    }

    void draw(Painter& p, Ui& ui) override {
        const Theme& t = theme();
        for (const Row& r : m_rows) {
            if (!r.widget->visible() || r.label.empty()) continue;
            const Rect wr = r.widget->rect();
            const float lh = line_height();
            p.text(0.0f, std::round(wr.y + (wr.h - lh) * 0.5f), r.label, text_style(r.widget->enabled() ? t.text_muted : t.text_disabled));
        }
        draw_children(p, ui);
    }
};

inline std::unique_ptr<Widget> Ui::make_root() {
    auto r = std::make_unique<VStack>();
    r->set_padding(-1.0f);
    return r;
}

} // namespace ui
} // namespace fizmo

#endif // FIZMO_UI_CORE_HPP
