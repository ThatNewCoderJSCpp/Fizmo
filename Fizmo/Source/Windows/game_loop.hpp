#ifndef FIZMO_GAME_LOOP_HPP
#define FIZMO_GAME_LOOP_HPP

#include "application.hpp"
#include "../Graphics/canvas.hpp"
#include <cmath>

namespace fizmo {
namespace windows {

enum class CanvasAnchor {
    top_left, top_center, top_right,
    center_left, center, center_right,
    bottom_left, bottom_center, bottom_right
};

struct CanvasLayout {
    enum class Mode { fill, fixed, fit };
    Mode mode = Mode::fill;
    unsigned int fixed_w = 0;
    unsigned int fixed_h = 0;
    CanvasAnchor anchor = CanvasAnchor::center;
};

struct CanvasRect {
    int x = 0, y = 0;
    unsigned int w = 0, h = 0;
};

namespace detail {

inline CanvasRect resolve_layout(const CanvasLayout& layout, unsigned int win_w, unsigned int win_h) noexcept {
    CanvasRect r;

    switch (layout.mode) {
        case CanvasLayout::Mode::fill:
            r.w = win_w; r.h = win_h;
            return r;
        case CanvasLayout::Mode::fixed:
            r.w = layout.fixed_w; r.h = layout.fixed_h;
            break;
        case CanvasLayout::Mode::fit: {
            double aspect = static_cast<double>(layout.fixed_w) / layout.fixed_h;
            double win_aspect = static_cast<double>(win_w) / win_h;

            if (win_aspect < aspect) {
                r.h = win_h;
                r.w = static_cast<unsigned int>(std::round(win_h * aspect));
            } else {
                r.w = win_w;
                r.h = static_cast<unsigned int>(std::round(win_w / aspect));
            }

            if (r.w == 0) r.w = 1;
            if (r.h == 0) r.h = 1;
            break;
        }
    }

    if (layout.mode != CanvasLayout::Mode::fill) {
        int iw = static_cast<int>(win_w), ih = static_cast<int>(win_h);
        int cw = static_cast<int>(r.w),   ch = static_cast<int>(r.h);

        switch (layout.anchor) {
            case CanvasAnchor::top_left:      r.x = 0;              r.y = 0;              break;
            case CanvasAnchor::top_center:    r.x = (iw - cw) / 2;  r.y = 0;              break;
            case CanvasAnchor::top_right:     r.x = iw - cw;        r.y = 0;              break;
            case CanvasAnchor::center_left:   r.x = 0;              r.y = (ih - ch) / 2;  break;
            case CanvasAnchor::center:        r.x = (iw - cw) / 2;  r.y = (ih - ch) / 2;  break;
            case CanvasAnchor::center_right:  r.x = iw - cw;        r.y = (ih - ch) / 2;  break;
            case CanvasAnchor::bottom_left:   r.x = 0;              r.y = ih - ch;         break;
            case CanvasAnchor::bottom_center: r.x = (iw - cw) / 2;  r.y = ih - ch;         break;
            case CanvasAnchor::bottom_right:  r.x = iw - cw;        r.y = ih - ch;         break;
        }
    }
    return r;
}

} // namespace detail

class GameLoop {
private:
    Application           m_app;
    graphics::Framebuffer m_fb;
    graphics::Canvas      m_canvas;
    CanvasLayout          m_layout;
    CanvasRect            m_rect;
    graphics::Color       m_letterbox;
    graphics::Color       m_canvas_clear;

    std::function<void(double dt)>          m_on_update;
    std::function<void(graphics::Canvas&)>  m_on_render;
    std::function<void(const WindowEvent&)> m_on_event;
    std::function<void(GameLoop&)>          m_on_startup;
    std::function<void()>                   m_on_shutdown;

    bool m_auto_clear = true;

    void rebuild_canvas() noexcept {
        m_rect = detail::resolve_layout(m_layout, m_app.width(), m_app.height());
        unsigned int fb_w = m_rect.w, fb_h = m_rect.h;
        if (m_layout.mode == CanvasLayout::Mode::fixed) { fb_w = m_layout.fixed_w; fb_h = m_layout.fixed_h; }
        if (fb_w != m_fb.width() || fb_h != m_fb.height()) { m_fb.resize(fb_w, fb_h, m_canvas_clear); }
    }

    void blit_canvas_to_window() noexcept {
        Renderer& r = m_app.renderer();
        if (m_layout.mode != CanvasLayout::Mode::fill) { r.clear(m_letterbox); }

        if (m_layout.mode == CanvasLayout::Mode::fixed && m_fb.width() == m_rect.w && m_fb.height() == m_rect.h && m_rect.x == 0 && m_rect.y == 0) {
            r.blit_framebuffer(m_fb);
        } else {
            auto img = m_fb.to_bitmap_image();
            r.draw_image(img, m_rect.x, m_rect.y, m_rect.w, m_rect.h);
        }
    }

public:
    GameLoop(
        unsigned int width, unsigned int height,
        const std::string& title,
        const graphics::Color& background = graphics::Color()
    ) noexcept
        : m_app(width, height, title, background)
        , m_fb(width, height, background)
        , m_canvas(m_fb)
        , m_letterbox(background)
        , m_canvas_clear(background)
    {}

    ~GameLoop() = default;
    GameLoop(const GameLoop&) = delete;
    GameLoop& operator=(const GameLoop&) = delete;

    void set_canvas_layout(const CanvasLayout& layout) noexcept {
        m_layout = layout;
        if (m_app.window().is_open()) rebuild_canvas();
    }

    const CanvasLayout& canvas_layout() const noexcept { return m_layout; }
    const CanvasRect&   canvas_rect()   const noexcept { return m_rect; }

    void set_canvas_clear_color(const graphics::Color& c) noexcept { m_canvas_clear = c; }
    void set_letterbox_color(const graphics::Color& c)    noexcept { m_letterbox = c; }
    void set_auto_clear(bool v) noexcept { m_auto_clear = v; }

    void on_update(std::function<void(double dt)> fn)         noexcept { m_on_update   = std::move(fn); }
    void on_render(std::function<void(graphics::Canvas&)> fn) noexcept { m_on_render   = std::move(fn); }
    void on_event(std::function<void(const WindowEvent&)> fn) noexcept { m_on_event    = std::move(fn); }
    void on_startup(std::function<void(GameLoop&)> fn)        noexcept { m_on_startup  = std::move(fn); }
    void on_shutdown(std::function<void()> fn)                 noexcept { m_on_shutdown = std::move(fn); }

    Application&                 app()               noexcept { return m_app; }
    Window&                      window()            noexcept { return m_app.window(); }
    Renderer&                    renderer()          noexcept { return m_app.renderer(); }
    graphics::Canvas&            canvas()            noexcept { return m_canvas; }
    graphics::Framebuffer&       framebuffer()       noexcept { return m_fb; }
    InputManager&                input()             noexcept { return m_app.input(); }
    const Application&           app()         const noexcept { return m_app; }
    const Window&                window()      const noexcept { return m_app.window(); }
    const Renderer&              renderer()    const noexcept { return m_app.renderer(); }
    const graphics::Canvas&      canvas()      const noexcept { return m_canvas; }
    const graphics::Framebuffer& framebuffer() const noexcept { return m_fb; }
    const InputManager&          input()       const noexcept { return m_app.input(); }

    unsigned int canvas_width()  const noexcept { return m_fb.width(); }
    unsigned int canvas_height() const noexcept { return m_fb.height(); }

    double      delta_seconds() const noexcept { return m_app.delta_seconds(); }
    double      fps()           const noexcept { return m_app.fps(); }
    double      fps_average()   const noexcept { return m_app.fps_average(); }
    std::size_t frame_count()   const noexcept { return m_app.frame_count(); }

    void quit() noexcept { m_app.quit(); }

    std::pair<int, int> window_to_canvas(int wx, int wy) const noexcept {
        double sx = static_cast<double>(m_fb.width())  / m_rect.w;
        double sy = static_cast<double>(m_fb.height()) / m_rect.h;
        int cx = static_cast<int>(std::round((wx - m_rect.x) * sx));
        int cy = static_cast<int>(std::round((wy - m_rect.y) * sy));
        return { cx, cy };
    }

    bool is_inside_canvas(int wx, int wy) const noexcept {
        return wx >= m_rect.x && wx < m_rect.x + static_cast<int>(m_rect.w) && wy >= m_rect.y && wy < m_rect.y + static_cast<int>(m_rect.h);
    }

    int run() noexcept {
        m_app.set_auto_clear(false);

        m_app.on_startup([this](Application&) {
            rebuild_canvas();
            if (m_on_startup) m_on_startup(*this);
        });

        m_app.on_event([this](const WindowEvent& e) {
            if (e.type == WindowEventType::WindowResize) { rebuild_canvas(); }
            if (m_on_event) m_on_event(e);
        });

        m_app.on_update([this](double dt) { if (m_on_update) m_on_update(dt); });

        m_app.on_render([this](Renderer&) {
            if (m_auto_clear) m_canvas.clear(m_canvas_clear);
            if (m_on_render) m_on_render(m_canvas);
            blit_canvas_to_window();
        });

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