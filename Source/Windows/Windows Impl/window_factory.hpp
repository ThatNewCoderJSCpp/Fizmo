#ifndef FIZMO_WINDOW_FACTORY_HPP
#define FIZMO_WINDOW_FACTORY_HPP

#include "window_base.hpp"
#include "../window_types.hpp"
#include <memory>
#include <vector>

namespace fizmo {
namespace windows {
namespace detail {

std::unique_ptr<ImplBase> make_window_impl(IWindowEventHandler* handler, const graphics::Color& background);
std::vector<MonitorInfo> platform_monitors();

} // namespace detail
} // namespace windows
} // namespace fizmo

#endif // FIZMO_WINDOW_FACTORY_HPP
