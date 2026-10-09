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

 ViewRect resolve_layout(const ViewLayout& layout, unsigned int win_w, unsigned int win_h) noexcept;

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

    void render_frame(Renderer& r);

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

    void set_logical_size(unsigned int w, unsigned int h, bool integer_scale = false) noexcept;

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

    vector2d window_to_view(double wx, double wy) const noexcept;

    vector2d view_to_window(double vx, double vy) const noexcept;

    bool is_inside_view(int wx, int wy) const noexcept;

    int run() noexcept;

    std::uint64_t add_event_listener(WindowEventType type, std::function<void(const WindowEvent&)> cb) noexcept { return m_app.add_event_listener(type, std::move(cb)); }
    std::uint64_t add_event_listener(const std::string& id_str, WindowEventType type, std::function<void(const WindowEvent&)> cb) noexcept { return m_app.add_event_listener(id_str, type, std::move(cb)); }
    bool remove_event_listener(std::uint64_t id) noexcept { return m_app.remove_event_listener(id); }
    bool remove_event_listener(const std::string& id_str) noexcept { return m_app.remove_event_listener(id_str); }
    void remove_event_listeners(WindowEventType type) noexcept { m_app.remove_event_listeners(type); }
};

} // namespace windows
} // namespace fizmo

#endif // FIZMO_GAME_LOOP_HPP