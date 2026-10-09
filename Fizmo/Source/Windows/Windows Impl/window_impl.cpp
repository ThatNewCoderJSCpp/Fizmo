#include "fizmo_library.hpp"
#include "window_factory.hpp"

#if defined(OS_WINDOWS)
    #include "window_windows_impl.hpp"
#elif defined(OS_LINUX)
    #include "window_linux_impl.hpp"
#endif

namespace fizmo {
namespace windows {
namespace detail {

std::unique_ptr<ImplBase> make_window_impl(IWindowEventHandler* handler, const graphics::Color& background) {
#if defined(OS_WINDOWS) || defined(OS_LINUX)
    return std::make_unique<WindowImpl>(handler, background);
#else
    (void)handler;
    (void)background;
    return nullptr;
#endif
}

} // namespace detail
} // namespace windows
} // namespace fizmo
