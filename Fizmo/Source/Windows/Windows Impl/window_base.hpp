#ifndef FIZMO_WINDOW_BASE_HPP
#define FIZMO_WINDOW_BASE_HPP

#include "../window_events.hpp"
#include "../../Graphics/color.hpp"

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
};

} // namespace detail
} // namespace windows
} // namespace fizmo

#endif // FIZMO_WINDOW_BASE_HPP