#include "fizmo_library.hpp"
#include "vulkan_backend.hpp"
#include "opengl_backend.hpp"

namespace fizmo {
namespace gpu {
namespace detail {

std::shared_ptr<BackendDevice> create_backend(const DeviceInput& in, Result& result, std::string& error) {
    result = Result::NoBackend;
    error.clear();

#if defined(OS_WINDOWS) || defined(OS_LINUX) || defined(OS_MACOS) || defined(OS_ANDROID)
    if (in.backend == Backend::Auto || in.backend == Backend::Vulkan) {
        auto d = std::make_shared<vkb::Device>();
        result = d->create(in);
        if (succeeded(result)) return d;
        error += "vulkan: " + d->last_error();
    }
#endif

#if defined(OS_MACOS)
    if (in.backend == Backend::OpenGL) {
        result = Result::NoBackend;
        error += "opengl: OpenGL isn't supported on macOS; use Vulkan through MoltenVK";
    }
#elif defined(OS_ANDROID)
    if (in.backend == Backend::OpenGL) {
        result = Result::NoBackend;
        error += "opengl: OpenGL ES isn't supported on Android yet; use Vulkan or the software renderer";
    }
#endif

#if defined(OS_WINDOWS) || defined(OS_LINUX)
    if (in.backend == Backend::Auto || in.backend == Backend::OpenGL) {
        auto d = std::make_shared<glb::Device>();
        result = d->create(in);
        if (succeeded(result)) return d;
        if (!error.empty()) error += "; ";
        error += "opengl: " + d->last_error();
    }
#endif

    return nullptr;
}

} // namespace detail
} // namespace gpu
} // namespace fizmo
