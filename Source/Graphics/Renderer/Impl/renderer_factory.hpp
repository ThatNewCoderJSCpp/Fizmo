#ifndef FIZMO_RENDERER_FACTORY_HPP
#define FIZMO_RENDERER_FACTORY_HPP

#include "renderer_base.hpp"
#include <memory>

namespace fizmo {
namespace graphics {
namespace detail {

std::unique_ptr<RendererImplBase> make_gpu_renderer_impl(gpu::Backend backend);
std::unique_ptr<RendererImplBase> make_software_renderer_impl();

} // namespace detail
} // namespace graphics
} // namespace fizmo

#endif // FIZMO_RENDERER_FACTORY_HPP
