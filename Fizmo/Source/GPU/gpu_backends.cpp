#include "fizmo_library.hpp"
#include "vulkan_backend.hpp"
#include "opengl_backend.hpp"

namespace fizmo {
namespace gpu {
namespace detail {

std::shared_ptr<BackendDevice> create_backend(const DeviceInput& in, Result& result, std::string& error) {
    result = Result::NoBackend;
    error.clear();

#if defined(OS_WINDOWS) || defined(OS_LINUX)
    if (in.backend == Backend::Auto || in.backend == Backend::Vulkan) {
        auto d = std::make_shared<vkb::Device>();
        result = d->create(in);
        if (succeeded(result)) return d;
        error += "vulkan: " + d->last_error();
    }

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
