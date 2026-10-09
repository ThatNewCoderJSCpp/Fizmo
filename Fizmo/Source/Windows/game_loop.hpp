#ifndef FIZMO_GAME_LOOP_HPP
#define FIZMO_GAME_LOOP_HPP

#include "application.hpp"
#include <algorithm>
#include <cmath>
#include <utility>

namespace fizmo {
namespace windows {

enum class ViewAnchor {
    top_left = 0, top_center, top_right,
    center_left, center, center_right,
    bottom_left, bottom_center, bottom_right
};

struct ViewLayout {
    enum class Mode { fill, fixed, fit, stretch };
    Mode         mode    = Mode::fill;
    unsigned int fixed_w = 0;
    unsigned int fixed_h = 0;
    ViewAnchor   anchor  = ViewAnchor::center;
    bool         integer_scale = false; 
};

struct ViewRect {
    int x = 0, y = 0;
    unsigned int w = 0, h = 0;
};

namespace detail {

inline ViewRect resolve_layout(const ViewLayout& layout, unsigned int win_w, unsigned int win_h) noexcept {
    ViewRect r;
    const bool has_fixed = layout.fixed_w > 0 && layout.fixed_h > 0;

    if (layout.mode == ViewLayout::Mode::fill || !has_fixed || win_w == 0 || win_h == 0) {
        r.w = win_w; r.h = win_h;
        return r;
    }

    switch (layout.mode) {
        case ViewLayout::Mode::fixed:
            r.w = layout.fixed_w; r.h = layout.fixed_h;
            break;
        case ViewLayout::Mode::stretch:
            r.w = win_w; r.h = win_h;
            return r;
        case ViewLayout::Mode::fit:
        default: {
            double s = std::min(static_cast<double>(win_w) / layout.fixed_w, static_cast<double>(win_h) / layout.fixed_h);
            if (layout.integer_scale && s >= 1.0) s = std::floor(s);
            r.w = std::max(1u, static_cast<unsigned int>(std::lround(layout.fixed_w * s)));
            r.h = std::max(1u, static_cast<unsigned int>(std::lround(layout.fixed_h * s)));
            break;
        }
    }

    const int iw = static_cast<int>(win_w), ih = static_cast<int>(win_h);
    const int cw = static_cast<int>(r.w),   ch = static_cast<int>(r.h);
    const int col = static_cast<int>(layout.anchor) % 3, row = static_cast<int>(layout.anchor) / 3;
    r.x = col == 0 ? 0 : (col == 1 ? (iw - cw) / 2 : iw - cw);
    r.y = row == 0 ? 0 : (row == 1 ? (ih - ch) / 2 : ih - ch);
    return r;
}

} // namespace detail

class GameLoop {
private:
    Application     m_app;
    ViewLayout      m_layout;
    ViewRect        m_rect;
    graphics::Color m_letterbox;
    graphics::Color m_clear;

    std::function<void(double dt)>          m_on_update;
    std::function<void(Renderer&)>          m_on_render;
    std::function<void(const WindowEvent&)> m_on_event;
    std::function<void(GameLoop&)>          m_on_startup;
    std::function<void()>                   m_on_shutdown;

    bool m_auto_clear = true;

    void rebuild_view() noexcept { m_rect = detail::resolve_layout(m_layout, m_app.width(), m_app.height()); }

    bool uses_logical_size() const noexcept { return m_layout.mode != ViewLayout::Mode::fill && m_layout.fixed_w > 0 && m_layout.fixed_h > 0; }

    void render_frame(Renderer& r) {
        if (!uses_logical_size()) {
            if (m_auto_clear) r.clear(m_clear);
            if (m_on_render) m_on_render(r);
            return;
        }

        r.clear(m_letterbox);
        r.push_transform();
        r.reset_transform();
        r.set_clip_rect(m_rect.x, m_rect.y, m_rect.w, m_rect.h);
        r.translate(m_rect.x, m_rect.y);
        r.scale(static_cast<double>(m_rect.w) / view_width(), static_cast<double>(m_rect.h) / view_height());
        if (m_auto_clear) r.draw_rect(0, 0, view_width(), view_height(), graphics::Paint::fill(m_clear));
        if (m_on_render) m_on_render(r);
        r.pop_transform();
        r.reset_clip_rect();
    }

public:
    GameLoop(
        unsigned int width, unsigned int height,
        const std::string& title,
        const graphics::Color& background = graphics::Color()
    ) noexcept
        : m_app(width, height, title, background)
        , m_letterbox(background)
        , m_clear(background)
    {}

    ~GameLoop() = default;
    GameLoop(const GameLoop&) = delete;
    GameLoop& operator=(const GameLoop&) = delete;

    void set_view_layout(const ViewLayout& layout) noexcept {
        m_layout = layout;
        if (m_app.window().is_open()) rebuild_view();
    }

    void set_logical_size(unsigned int w, unsigned int h, bool integer_scale = false) noexcept {
        ViewLayout l = m_layout;
        l.mode = ViewLayout::Mode::fit;
        l.fixed_w = w; l.fixed_h = h;
        l.integer_scale = integer_scale;
        set_view_layout(l);
    }

    const ViewLayout& view_layout() const noexcept { return m_layout; }
    const ViewRect&   view_rect()   const noexcept { return m_rect; }

    void set_clear_color(const graphics::Color& c)     noexcept { m_clear = c; }
    void set_letterbox_color(const graphics::Color& c) noexcept { m_letterbox = c; }
    void set_auto_clear(bool v) noexcept { m_auto_clear = v; }

    void on_update(std::function<void(double dt)> fn)         noexcept { m_on_update   = std::move(fn); }
    void on_render(std::function<void(Renderer&)> fn)         noexcept { m_on_render   = std::move(fn); }
    void on_event(std::function<void(const WindowEvent&)> fn) noexcept { m_on_event    = std::move(fn); }
    void on_startup(std::function<void(GameLoop&)> fn)        noexcept { m_on_startup  = std::move(fn); }
    void on_shutdown(std::function<void()> fn)                noexcept { m_on_shutdown = std::move(fn); }

    Application&        app()            noexcept { return m_app; }
    Window&             window()         noexcept { return m_app.window(); }
    Renderer&           renderer()       noexcept { return m_app.renderer(); }
    InputManager&       input()          noexcept { return m_app.input(); }
    input::GamepadManager& gamepads()    noexcept { return m_app.gamepads(); }
    const Application&  app()      const noexcept { return m_app; }
    const Window&       window()   const noexcept { return m_app.window(); }
    const Renderer&     renderer() const noexcept { return m_app.renderer(); }
    const InputManager& input()    const noexcept { return m_app.input(); }

    unsigned int view_width()  const noexcept { return uses_logical_size() ? m_layout.fixed_w : m_app.width(); }
    unsigned int view_height() const noexcept { return uses_logical_size() ? m_layout.fixed_h : m_app.height(); }

    double      delta_seconds() const noexcept { return m_app.delta_seconds(); }
    double      fps()           const noexcept { return m_app.fps(); }
    double      fps_average()   const noexcept { return m_app.fps_average(); }
    std::size_t frame_count()   const noexcept { return m_app.frame_count(); }

    void quit() noexcept { m_app.quit(); }

    vector2d window_to_view(double wx, double wy) const noexcept {
        if (m_rect.w == 0 || m_rect.h == 0) return { wx, wy };
        return {
            (wx - m_rect.x) * static_cast<double>(view_width())  / m_rect.w,
            (wy - m_rect.y) * static_cast<double>(view_height()) / m_rect.h
        };
    }

    vector2d view_to_window(double vx, double vy) const noexcept {
        if (view_width() == 0 || view_height() == 0) return { vx, vy };
        return {
            m_rect.x + vx * static_cast<double>(m_rect.w) / view_width(),
            m_rect.y + vy * static_cast<double>(m_rect.h) / view_height()
        };
    }

    bool is_inside_view(int wx, int wy) const noexcept {
        return wx >= m_rect.x && wx < m_rect.x + static_cast<int>(m_rect.w) && wy >= m_rect.y && wy < m_rect.y + static_cast<int>(m_rect.h);
    }

    int run() noexcept {
        m_app.set_auto_clear(false);

        m_app.on_startup([this](Application&) {
            rebuild_view();
            if (m_on_startup) m_on_startup(*this);
        });

        m_app.on_event([this](const WindowEvent& e) {
            if (e.type == WindowEventType::WindowResize) rebuild_view();
            if (m_on_event) m_on_event(e);
        });

        m_app.on_update([this](double dt) { if (m_on_update) m_on_update(dt); });
        m_app.on_render([this](Renderer& r) { try { render_frame(r); } catch (...) {} });
        m_app.on_shutdown([this]() { if (m_on_shutdown) m_on_shutdown(); });
        return m_app.run();
    }

    std::uint64_t add_event_listener(WindowEventType type, std::function<void(const WindowEvent&)> cb) noexcept { return m_app.add_event_listener(type, std::move(cb)); }
    std::uint64_t add_event_listener(const std::string& id_str, WindowEventType type, std::function<void(const WindowEvent&)> cb) noexcept { return m_app.add_event_listener(id_str, type, std::move(cb)); }
    bool remove_event_listener(std::uint64_t id) noexcept { return m_app.remove_event_listener(id); }
    bool remove_event_listener(const std::string& id_str) noexcept { return m_app.remove_event_listener(id_str); }
    void remove_event_listeners(WindowEventType type) noexcept { m_app.remove_event_listeners(type); }
};

} // namespace windows
} // namespace fizmo

#endif // FIZMO_GAME_LOOP_HPP