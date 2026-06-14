#ifndef FIZMO_WINDOW_CLASS_HPP
#define FIZMO_WINDOW_CLASS_HPP

#include "../Basic/basic_includes.hpp"
#include "../Util Hpp/util_functions.hpp"
#include "../Graphics/color.hpp"
#include "window_events.hpp"
#include "../Input/keybord.hpp"

#ifdef OS_WINDOWS
#include "Windows Impl/window_impl.hpp"
#endif

namespace fizmo {
namespace windows {

class Window : public detail::IWindowEventHandler {
private:
    unsigned int m_width;
    unsigned int m_height;
    std::string m_title;
    fizmo::graphics::Color m_color;
    WindowEventHandler m_event_handler;
    std::unique_ptr<detail::ImplBase> m_impl;

public:
    Window(unsigned int w, unsigned int h, const std::string& t, const graphics::Color& background_color = graphics::Color()) noexcept : m_width(w), m_height(h), m_title(t), m_color(background_color) {
    #ifdef OS_WINDOWS
        m_impl = std::make_unique<detail::WindowImpl>(this, background_color);
    #endif
    }

    ~Window() = default;

    void dispatch_event(const WindowEvent& event) noexcept override { m_event_handler.dispatch_event(event); }

    void update_size(unsigned int width, unsigned int height) noexcept override {
        m_width = width;
        m_height = height;
    }

    unsigned int width() const noexcept { return m_width; }
    unsigned int height() const noexcept { return m_height; }
    const std::string& title() const noexcept { return m_title; }
    const fizmo::graphics::Color& background_color() const noexcept { return m_color; }
    WindowEventHandler& event_handler() noexcept { return m_event_handler; }

    void set_title(const std::string& title) noexcept { m_title = title; }

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
    void poll_events() noexcept { m_impl->poll_events(); }
    bool is_open() const noexcept { return m_impl->is_open(); }
    void invalidate() noexcept { m_impl->invalidate(); }
    void* native_handle() const noexcept { return m_impl->native_handle(); }
    void set_paint_callback(std::function<void(void*)> cb) noexcept { m_impl->set_paint_callback(std::move(cb)); }
};

} // namespace windows
} // namespace fizmo

#endif // FIZMO_WINDOW_CLASS_HPP