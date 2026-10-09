#ifndef FIZMO_WINDOW_BASE_HPP
#define FIZMO_WINDOW_BASE_HPP

#include "../window_events.hpp"
#include "../../Graphics/color.hpp"
#include "../../Images/Bitmap/image.hpp"
#include "../window_types.hpp"
#include <string>
#include <vector>

namespace fizmo {
namespace windows {
namespace detail {

class IWindowEventHandler {
public:
    virtual ~IWindowEventHandler() = default;
    virtual void dispatch_event(const WindowEvent& event) noexcept = 0;
    virtual void update_size(unsigned int width, unsigned int height) noexcept = 0;
};

class ImplBase {
protected:
    IWindowEventHandler* m_event_handler;

public:
    ImplBase(IWindowEventHandler* handler) noexcept : m_event_handler(handler) {}
    virtual ~ImplBase() noexcept = default;

    virtual bool create(unsigned int width, unsigned int height, const std::string& title) noexcept = 0;
    virtual void poll_events() noexcept = 0;
    virtual bool is_open() const noexcept = 0;
    virtual void set_background_color(const fizmo::graphics::Color& color) noexcept = 0;
    virtual void invalidate() noexcept = 0;
    virtual void* native_handle() const noexcept = 0;
    virtual void set_paint_callback(std::function<void(void*)> cb) noexcept = 0;
    virtual void set_background_erase(bool /*enabled*/) noexcept {}
    virtual bool set_cursor_locked(bool /*locked*/) noexcept { return false; }
    virtual bool cursor_locked() const noexcept { return false; }
    virtual void set_cursor_visible(bool /*visible*/) noexcept {}
    virtual bool cursor_visible() const noexcept { return true; }

    virtual void set_title(const std::string&) noexcept {}
    virtual bool set_position(int, int) noexcept { return false; }
    virtual bool position(int& x, int& y) const noexcept { x = y = 0; return false; }
    virtual bool set_size(unsigned int, unsigned int) noexcept { return false; }
    virtual void set_resizable(bool) noexcept {}
    virtual void set_min_size(unsigned int, unsigned int) noexcept {}
    virtual void minimize() noexcept {}
    virtual void maximize() noexcept {}
    virtual void restore() noexcept {}
    virtual void focus() noexcept {}
    virtual bool set_mode(WindowMode, int) noexcept { return false; }
    virtual WindowMode mode() const noexcept { return WindowMode::Windowed; }
    virtual float dpi_scale() const noexcept { return 1.0f; }
    virtual int monitor_index() const noexcept { return 0; }

    virtual bool set_system_cursor(SystemCursor) noexcept { return false; }
    virtual bool set_cursor_image(const images::BitmapImage&, int, int) noexcept { return false; }

    virtual std::string clipboard_text() noexcept { return {}; }
    virtual bool set_clipboard_text(const std::string&) noexcept { return false; }
    virtual void set_drop_enabled(bool) noexcept {}

    virtual void start_text_input() noexcept {}
    virtual void stop_text_input() noexcept {}
    virtual bool text_input_active() const noexcept { return false; }
    virtual void set_text_input_rect(int, int, unsigned int, unsigned int) noexcept {}
};

} // namespace detail
} // namespace windows
} // namespace fizmo

#endif // FIZMO_WINDOW_BASE_HPP