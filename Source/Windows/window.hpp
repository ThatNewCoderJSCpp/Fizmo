#ifndef FIZMO_WINDOW_CLASS_HPP
#define FIZMO_WINDOW_CLASS_HPP

#include "../Basic/basic_includes.hpp"
#include "../Util Hpp/util_functions.hpp"
#include "../Graphics/color.hpp"
#include "window_events.hpp"
#include "../Input/keybord.hpp"
#include "Windows Impl/window_factory.hpp"
#include "window_types.hpp"
#include "../Input/gestures.hpp"

namespace fizmo {
namespace windows {

class Window : public detail::IWindowEventHandler {
private:
    unsigned int m_width;
    unsigned int m_height;
    std::string m_title;
    fizmo::graphics::Color m_color;
    WindowEventHandler m_event_handler;
    input::GestureRecognizer m_gestures;
    bool m_gestures_enabled = true;
    std::unique_ptr<detail::ImplBase> m_impl;

    void emit_gesture(const input::GestureData& g) noexcept;

    void feed_gestures(const WindowEvent& e) noexcept;

public:
    Window(unsigned int w, unsigned int h, const std::string& t, const graphics::Color& background_color = graphics::Color()) noexcept;

    ~Window() = default;

    void dispatch_event(const WindowEvent& event) noexcept override {
        m_event_handler.dispatch_event(event);
        feed_gestures(event);
    }

    void update_size(unsigned int width, unsigned int height) noexcept override {
        m_width = width;
        m_height = height;
    }

    unsigned int width() const noexcept { return m_width; }
    unsigned int height() const noexcept { return m_height; }
    const std::string& title() const noexcept { return m_title; }
    const fizmo::graphics::Color& background_color() const noexcept { return m_color; }
    WindowEventHandler& event_handler() noexcept { return m_event_handler; }

    void set_title(const std::string& title) noexcept {
        m_title = title;
        m_impl->set_title(title);
    }

    void set_background_color(const fizmo::graphics::Color& color) noexcept {
        m_color = color;
        m_impl->set_background_color(color);
    }

    std::uint64_t add_event_listener(WindowEventType type, std::function<void(const WindowEvent&)> callback) noexcept { return m_event_handler.add_event_listener(type, std::move(callback)); }
    std::uint64_t add_event_listener(const std::string& id_str, WindowEventType type, std::function<void(const WindowEvent&)> callback) noexcept { return m_event_handler.add_event_listener(id_str, type, std::move(callback)); }
    bool remove_event_listener(std::uint64_t id) noexcept { return m_event_handler.remove_event_listener(id); }
    bool remove_event_listener(const std::string& id_str) noexcept { return m_event_handler.remove_event_listener(id_str); }
    void remove_event_listeners(WindowEventType type) noexcept { m_event_handler.remove_event_listeners(type); }

    bool create() noexcept { return m_impl->create(m_width, m_height, m_title); }
    void poll_events() noexcept;
    bool is_open() const noexcept { return m_impl->is_open(); }
    void invalidate() noexcept { m_impl->invalidate(); }
    void* native_handle() const noexcept { return m_impl->native_handle(); }
    void set_paint_callback(std::function<void(void*)> cb) noexcept { m_impl->set_paint_callback(std::move(cb)); }
    void set_background_erase(bool enabled) noexcept { m_impl->set_background_erase(enabled); }

    bool set_cursor_locked(bool locked) noexcept { return m_impl->set_cursor_locked(locked); }
    bool cursor_locked() const noexcept { return m_impl->cursor_locked(); }
    void set_cursor_visible(bool visible) noexcept { m_impl->set_cursor_visible(visible); }
    bool cursor_visible() const noexcept { return m_impl->cursor_visible(); }

    bool set_relative_mouse(bool enabled) noexcept { return set_cursor_locked(enabled); }
    bool relative_mouse() const noexcept { return cursor_locked(); }

    bool position(int& x, int& y) const noexcept { return m_impl->position(x, y); }
    bool set_position(int x, int y) noexcept { return m_impl->set_position(x, y); }
    bool set_size(unsigned int w, unsigned int h) noexcept { return m_impl->set_size(w, h); }
    void set_resizable(bool resizable) noexcept { m_impl->set_resizable(resizable); }
    void set_min_size(unsigned int w, unsigned int h) noexcept { m_impl->set_min_size(w, h); }
    void minimize() noexcept { m_impl->minimize(); }
    void maximize() noexcept { m_impl->maximize(); }
    void restore() noexcept { m_impl->restore(); }
    void focus() noexcept { m_impl->focus(); }

    bool set_mode(WindowMode mode, int monitor = -1) noexcept { return m_impl->set_mode(mode, monitor); }
    WindowMode mode() const noexcept { return m_impl->mode(); }
    bool set_fullscreen(bool enabled, int monitor = -1) noexcept { return set_mode(enabled ? WindowMode::Fullscreen : WindowMode::Windowed, monitor); }
    bool fullscreen() const noexcept { return mode() == WindowMode::Fullscreen; }
    bool set_borderless(bool enabled, int monitor = -1) noexcept { return set_mode(enabled ? WindowMode::Borderless : WindowMode::Windowed, monitor); }
    float dpi_scale() const noexcept { return m_impl->dpi_scale(); }
    int monitor_index() const noexcept { return m_impl->monitor_index(); }

    bool set_cursor(SystemCursor cursor) noexcept { return m_impl->set_system_cursor(cursor); }
    bool set_cursor(const images::BitmapImage& image, int hot_x = 0, int hot_y = 0) noexcept { return m_impl->set_cursor_image(image, hot_x, hot_y); }

    std::string clipboard_text() noexcept { return m_impl->clipboard_text(); }
    bool set_clipboard_text(const std::string& text) noexcept { return m_impl->set_clipboard_text(text); }
    void set_drop_enabled(bool enabled) noexcept { m_impl->set_drop_enabled(enabled); }

    void start_text_input() noexcept { m_impl->start_text_input(); }
    void stop_text_input() noexcept { m_impl->stop_text_input(); }
    bool text_input_active() const noexcept { return m_impl->text_input_active(); }
    void set_text_input_rect(int x, int y, unsigned int w, unsigned int h) noexcept { m_impl->set_text_input_rect(x, y, w, h); }

    input::GestureRecognizer& gestures() noexcept { return m_gestures; }
    void set_gestures_enabled(bool enabled) noexcept { m_gestures_enabled = enabled; if (!enabled) m_gestures.reset(); }
    bool gestures_enabled() const noexcept { return m_gestures_enabled; }
};

std::vector<MonitorInfo> monitors();

MonitorInfo primary_monitor();

class CursorLock {
private:
    Window* m_window;

public:
    explicit CursorLock(Window& window) noexcept : m_window(&window) {}
    ~CursorLock() { unlock(); }
    CursorLock(const CursorLock&) = delete;
    CursorLock& operator=(const CursorLock&) = delete;

    bool lock() noexcept   { return m_window->set_cursor_locked(true); }
    void unlock() noexcept { if (m_window->cursor_locked()) m_window->set_cursor_locked(false); }
    bool toggle() noexcept { if (locked()) { unlock(); return false; } return lock(); }
    bool locked() const noexcept { return m_window->cursor_locked(); }
};

} // namespace windows
} // namespace fizmo

#endif // FIZMO_WINDOW_CLASS_HPP