#ifndef FIZMO_APPLICATION_HPP
#define FIZMO_APPLICATION_HPP

#include "window.hpp"
#include "renderer.hpp"
#include "input_manager.hpp"
#include "../Input/gamepad.hpp"
#include <functional>
#include <vector>
#include <chrono>
#include <thread>

namespace fizmo {
namespace windows {

class Application {
private:
    using Clock     = std::chrono::steady_clock;
    using TimePoint = Clock::time_point;
    using Duration  = std::chrono::duration<double>;

    static constexpr double kSmoothing = 0.05;
    static constexpr double kMaxDelta  = 0.25;

private:
    Window          m_window;
    Renderer        m_renderer;
    graphics::Color m_clear_color;
    InputManager    m_input;
    input::GamepadManager m_gamepads;

    TimePoint   m_time_start;
    TimePoint   m_time_last;
    double      m_delta       = 0.0;
    double      m_fps         = 0.0;
    double      m_fps_avg     = 0.0;
    std::size_t m_frame_count = 0;

    std::function<void(double dt)>          m_on_update;
    std::function<void(Renderer&)>          m_on_render;
    std::function<void(const WindowEvent&)> m_on_event;
    std::function<void(Application&)>       m_on_startup;
    std::function<void()>                   m_on_shutdown;

    bool m_running    = false;
    bool m_auto_clear = true;

    double m_max_fps = 0.0;
    TimePoint m_next_frame;

public:
    Application(
        unsigned int width,
        unsigned int height,
        const std::string& title,
        const graphics::Color& background = graphics::Color()
    ) noexcept
        : m_window(width, height, title, background)
        , m_renderer(m_window)
        , m_clear_color(background)
    {}

    ~Application() = default;
    Application(const Application&) = delete;
    Application& operator=(const Application&) = delete;

    void on_update(std::function<void(double dt)> fn)         noexcept { m_on_update   = std::move(fn); }
    void on_render(std::function<void(Renderer&)> fn)         noexcept { m_on_render   = std::move(fn); }
    void on_event(std::function<void(const WindowEvent&)> fn) noexcept { m_on_event    = std::move(fn); }
    void on_startup(std::function<void(Application&)> fn)     noexcept { m_on_startup  = std::move(fn); }
    void on_shutdown(std::function<void()> fn)                noexcept { m_on_shutdown = std::move(fn); }

    Window&       window()   noexcept { return m_window; }
    Renderer&     renderer() noexcept { return m_renderer; }
    InputManager& input()    noexcept { return m_input; }
    input::GamepadManager& gamepads() noexcept { return m_gamepads; }

    const Window&       window()   const noexcept { return m_window; }
    const Renderer&     renderer() const noexcept { return m_renderer; }
    const InputManager& input()    const noexcept { return m_input; }

    unsigned int width()  const noexcept { return m_window.width(); }
    unsigned int height() const noexcept { return m_window.height(); }

    void set_auto_clear(bool v) noexcept { m_auto_clear = v; }
    void set_clear_color(const graphics::Color& c) noexcept { m_clear_color = c; }

    void   set_max_fps(double fps) noexcept { m_max_fps = fps > 0.0 ? fps : 0.0; m_next_frame = Clock::now(); }
    double max_fps() const noexcept { return m_max_fps; }

    double      delta_seconds()   const noexcept { return m_delta;       }
    double      fps()             const noexcept { return m_fps;         }
    double      fps_average()     const noexcept { return m_fps_avg;     }
    std::size_t frame_count()     const noexcept { return m_frame_count; }
    double      elapsed_seconds() const noexcept { return Duration(Clock::now() - m_time_start).count(); }

    bool create() noexcept;

    void quit() noexcept { m_running = false; }

    int run() noexcept;

    std::uint64_t add_event_listener(WindowEventType type, std::function<void(const WindowEvent&)> cb) noexcept { return m_window.add_event_listener(type, std::move(cb)); }
    std::uint64_t add_event_listener(const std::string& id_str, WindowEventType type, std::function<void(const WindowEvent&)> cb) noexcept { return m_window.add_event_listener(id_str, type, std::move(cb)); }
    bool remove_event_listener(std::uint64_t id) noexcept { return m_window.remove_event_listener(id); }
    bool remove_event_listener(const std::string& id_str) noexcept { return m_window.remove_event_listener(id_str); }
    void remove_event_listeners(WindowEventType type) noexcept { m_window.remove_event_listeners(type); }

private:
    void install_internal_events() noexcept;

    void limit_frame_rate() noexcept;
};

} // namespace windows
} // namespace fizmo

#endif // FIZMO_APPLICATION_HPP