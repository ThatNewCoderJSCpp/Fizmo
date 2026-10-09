#include "fizmo_library.hpp"
#include "renderer_factory.hpp"
#include "renderer_gpu_impl.hpp"

#if defined(OS_WINDOWS)
    #include "renderer_windows_impl.hpp"
#elif defined(OS_LINUX)
    #include "renderer_linux_impl.hpp"
#endif

namespace fizmo {
namespace windows {
namespace detail {

std::unique_ptr<RendererImplBase> make_gpu_renderer_impl(gpu::Backend backend) { return std::make_unique<RendererImplGPU>(backend); }

std::unique_ptr<RendererImplBase> make_software_renderer_impl() { return std::make_unique<RendererImpl>(); }

} // namespace detail
} // namespace windows
} // namespace fizmo
