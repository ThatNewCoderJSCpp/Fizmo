#ifndef FIZMO_WINDOW_NULL_IMPL_HPP
#define FIZMO_WINDOW_NULL_IMPL_HPP

#include "window_base.hpp"
#include "../../Basic/fizmo_defines.hpp"

#if defined(OS_ANDROID)

#include <functional>
#include <string>

namespace fizmo {
namespace windows {
namespace detail {

class WindowImpl : public ImplBase {
public:
    WindowImpl(IWindowEventHandler* handler, const fizmo::colors::Color&) noexcept : ImplBase(handler) {}

    bool create(unsigned int, unsigned int, const std::string&) noexcept override { return false; }
    void poll_events() noexcept override {}
    bool is_open() const noexcept override { return false; }
    void set_background_color(const fizmo::colors::Color&) noexcept override {}
    void invalidate() noexcept override {}
    void* native_handle() const noexcept override { return nullptr; }
    void set_paint_callback(std::function<void(void*)>) noexcept override {}
};

} // namespace detail
} // namespace windows
} // namespace fizmo

#endif // OS_ANDROID
#endif // FIZMO_WINDOW_NULL_IMPL_HPP
