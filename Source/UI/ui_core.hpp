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

    Rect intersect(const Rect& o) const noexcept;

    bool intersects(const Rect& o) const noexcept { return x < o.right() && o.x < right() && y < o.bottom() && o.y < bottom(); }
};

enum class Orientation : std::uint8_t { Horizontal = 0, Vertical };
enum class Align : std::uint8_t { Start = 0, Center, End, Stretch };
enum class Placement : std::uint8_t { Below = 0, Above, Right, Center, At };
enum class ButtonStyle : std::uint8_t { Normal = 0, Primary, Danger, Flat };

inline Color with_alpha(const Color& c, std::uint8_t a) noexcept { return Color(c.red(), c.green(), c.blue(), a); }

Color mix(const Color& a, const Color& b, float t) noexcept;

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

    static Theme light();
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
    bool ctrl() const noexcept;
    bool alt() const noexcept { return input::has(mods, input::Modifiers::Alt); }
};

struct KeyEvent {
    input::Key       key = input::Key::Unknown;
    input::Modifiers mods = input::Modifiers::None;
    bool             repeat = false;

    bool shift() const noexcept { return input::has(mods, input::Modifiers::Shift); }
    bool ctrl() const noexcept;
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

    Size preferred(Ui& ui);

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

    static const graphics::Texture& solid_texture(const Color& c);

    static graphics::Texture ramp_texture(const Color& a, const Color& b, bool vertical);

    static const graphics::Texture& tinted_texture(const graphics::Texture& tex, const Color& tint);

    void blend_rect(const Rect& r, const Color& c);

    int px(float v) const noexcept { return static_cast<int>(std::lround(v * m_scale)); }

    Color fade(const Color& c) const noexcept;

    void apply_clip();

    struct Pix { int x, y; unsigned int w, h; };

    Pix pix(const Rect& r) const noexcept;

public:
    Painter(windows::Renderer& r, float scale) noexcept : m_r(&r), m_scale(scale > 0.0f ? scale : 1.0f), m_software(!r.is_gpu()) {}

    bool software() const noexcept { return m_software; }

    windows::Renderer& renderer() noexcept { return *m_r; }
    float scale() const noexcept { return m_scale; }
    Point offset() const noexcept { return Point{ m_ox, m_oy }; }

    void push_offset(float dx, float dy) { m_offsets.push_back(Point{ m_ox, m_oy }); m_ox += dx; m_oy += dy; }
    void pop_offset();

    void push_clip(const Rect& local);

    void pop_clip() { if (m_clips.empty()) return; m_clips.pop_back(); apply_clip(); }

    bool visible(const Rect& local) const noexcept;

    void push_opacity(float o) { m_opacities.push_back(m_opacity); m_opacity *= std::max(0.0f, std::min(1.0f, o)); }
    void pop_opacity() { if (m_opacities.empty()) return; m_opacity = m_opacities.back(); m_opacities.pop_back(); }

    void fill_rect(const Rect& r, const Color& c);

    void stroke_rect(const Rect& r, const Color& c, float width = 1.0f);

    void fill_rounded(const Rect& r, float radius, const Color& c);

    void stroke_rounded(const Rect& r, float radius, const Color& c, float width = 1.0f);

    void box(const Rect& r, float radius, const Color& fill, const Color& border, float border_width = 1.0f);

    void line(float x1, float y1, float x2, float y2, const Color& c, float width = 1.0f);

    void polyline(std::initializer_list<Point> pts, const Color& c, float width = 1.0f);

    void fill_circle(float cx, float cy, float radius, const Color& c);

    void stroke_circle(float cx, float cy, float radius, const Color& c, float width = 1.0f);

    void fill_triangle(Point a, Point b, Point c, const Color& col);

    void chevron_down(float cx, float cy, float size, const Color& c, float width = 1.5f);

    void gradient_rect(const Rect& r, const Color& a, const Color& b, bool vertical);

    void checkerboard(const Rect& r, float cell, const Color& a, const Color& b);

    void texture(const graphics::Texture& tex, const Rect& r, const Color& tint = Color(255, 255, 255));

    void text(float x, float y, const std::string& s, const text::TextStyle& st);
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

    static Point mouse_of(const windows::WindowEvent& e) noexcept;

    MouseEvent make_mouse(const Widget* w, float sx, float sy, const windows::WindowEvent& e) const noexcept;

    void forget(Widget* w) noexcept;

    void update_text_input() noexcept;

    Widget* scope_root() const noexcept;

    static void collect_focusable(Widget* w, std::vector<Widget*>& out);

    void run_deferred();

    void set_hover(Widget* w);

    void update_tree(Widget* w, double dt);

    Rect place(const Overlay& o, Size s) const noexcept;

    bool dispatch_key(const KeyEvent& k);

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

    void attach(windows::Window& window);

    void detach();

    text::TextStyle text_style(const Theme& th, const Color& c, double size = 0.0, bool bold = false) const;

    Size text_size(const std::string& s, const text::TextStyle& st);

    Widget* focused() const noexcept { return m_focus; }
    Widget* hovered() const noexcept { return m_hover; }
    Widget* pressed() const noexcept { return m_pressed; }
    bool focus_visible() const noexcept { return m_focus_visible; }

    void set_focus(Widget* w, bool from_keyboard = false);

    void clear_focus() { set_focus(nullptr); }

    bool focus_next(bool backward = false);

    void reset_caret_blink() noexcept { m_blink_start = m_time; }
    bool caret_on() const noexcept;

    std::string clipboard();

    void set_clipboard(const std::string& s) {
        m_clipboard = s;
        if (m_window) m_window->set_clipboard_text(s);
    }

    void defer(std::function<void()> fn) { m_deferred.push_back(std::move(fn)); }

    void open_popup(Widget& owner, Widget& content, const Rect& anchor, Placement placement = Placement::Below, std::function<void()> on_close = {});

    bool close_popup(Widget& content);

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

    void close_modal(Widget& content);

    void alert(const std::string& title, const std::string& message, std::function<void()> on_close = {});
    void confirm(const std::string& title, const std::string& message, std::function<void(bool)> on_result, const std::string& yes = "OK", const std::string& no = "Cancel");
    void prompt(const std::string& title, const std::string& message, const std::string& initial, std::function<void(std::optional<std::string>)> on_result);

    bool wants_mouse() const noexcept {
        if (m_pressed || modal_open()) return true;
        return m_hover != nullptr;
    }

    bool wants_keyboard() const noexcept { return m_focus != nullptr || modal_open(); }
    bool tooltip_visible() const noexcept { return m_tip_shown; }

    Widget* pick(float x, float y);

    bool handle_event(const windows::WindowEvent& e);

    void update(double dt);

    void layout(windows::Renderer& r);

    void draw(windows::Renderer& r);

private:
    void draw_overlays(Widget* w, Painter& p);

    void draw_tooltip(Painter& p, const std::string& text);
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












inline bool Widget::hovered_directly() const noexcept { return m_ui && m_ui->m_hover == this; }
inline bool Widget::pressed() const noexcept { return m_ui && m_ui->m_pressed == this; }
inline bool Widget::focused() const noexcept { return m_ui && m_ui->m_focus == this; }
inline bool Widget::focus_within() const noexcept { return m_ui && m_ui->m_focus && is_ancestor_of(m_ui->m_focus); }
inline bool Widget::focus_visible() const noexcept { return focused() && m_ui->m_focus_visible; }
inline void Widget::focus() { if (m_ui) m_ui->set_focus(this); }
inline void Widget::blur() { if (m_ui && focus_within()) m_ui->set_focus(nullptr); }











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

    Size measure(Ui& ui) override;

    void arrange(Ui& ui) override;

    void draw(Painter& p, Ui& ui) override;
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

    Size measure(Ui& ui) override;

    void arrange(Ui& ui) override;
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
    void draw(Painter& p, Ui&) override;
};

class Label : public Widget {
protected:
    std::string m_text;
    Color       m_color{ 0, 0, 0, 0 };
    double      m_size = 0.0;
    bool        m_bold = false;
    bool        m_muted = false;
    Align       m_align = Align::Start;

    std::vector<std::string> lines() const;

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

    Size measure(Ui&) override;

    void draw(Painter& p, Ui&) override;
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

    Size measure(Ui&) override;

    void draw(Painter& p, Ui&) override;

    bool on_mouse_down(MouseEvent& e) override { if (e.button != input::MouseButton::Left) return false; m_armed = true; return true; }
    void on_mouse_up(MouseEvent& e) override { if (m_armed && bounds().contains(e.x, e.y)) click(); m_armed = false; }

    bool on_key(const KeyEvent& k) override;
};

class Form : public Widget {
protected:
    struct Row { std::string label; Widget* widget = nullptr; };
    std::vector<Row> m_rows;
    float            m_spacing = -1.0f;
    float            m_label_width = -1.0f;
    float            m_gap = 10.0f;

    float label_column() const;

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

    Size measure(Ui& ui) override;

    void arrange(Ui& ui) override;

    void draw(Painter& p, Ui& ui) override;
};

inline std::unique_ptr<Widget> Ui::make_root() {
    auto r = std::make_unique<VStack>();
    r->set_padding(-1.0f);
    return r;
}

} // namespace ui
} // namespace fizmo

#endif // FIZMO_UI_CORE_HPP
