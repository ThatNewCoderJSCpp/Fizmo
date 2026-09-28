#ifndef FIZMO_VULKAN_INSTANCE_HPP
#define FIZMO_VULKAN_INSTANCE_HPP

#include "loader.hpp"
#include "types.hpp"
#include <cstdio>
#include <cstring>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace fizmo {
namespace vulkan {

class Instance;
class Surface;

using DebugCallback = std::function<void(DebugSeverity severity, const char* message)>;

struct InstanceDesc {
    std::string   app_name       = "fizmo application";
    std::uint32_t app_version    = make_version(1, 0, 0);
    std::string   engine_name    = "fizmo";
    std::uint32_t engine_version = make_version(1, 0, 0);

    bool enable_validation = false;
    bool enable_surface = true;

    DebugSeverity min_severity = DebugSeverity::Warning;
    DebugCallback debug_callback;   

    std::vector<const char*> extra_extensions;
    std::vector<const char*> extra_layers;
};

namespace detail {

struct InstanceState {
    Library          library;
    GlobalDispatch   global;
    InstanceDispatch fn;
    native::Instance handle          = VK_NULL_HANDLE;
    native::DebugMessenger messenger = VK_NULL_HANDLE;
    DebugCallback    debug_callback;
    std::uint32_t    api_version = 0;
    bool             validation  = false;
    bool             debug_utils = false;
    bool             surface     = false;

    ~InstanceState() noexcept {
        if (!handle) return;
        if (messenger && fn.vkDestroyDebugUtilsMessengerEXT) fn.vkDestroyDebugUtilsMessengerEXT(handle, messenger, nullptr);
        fn.vkDestroyInstance(handle, nullptr);
    }
};

inline VKAPI_ATTR VkBool32 VKAPI_CALL debug_trampoline(
    VkDebugUtilsMessageSeverityFlagBitsEXT severity,
    VkDebugUtilsMessageTypeFlagsEXT,
    const VkDebugUtilsMessengerCallbackDataEXT* data,
    void* user) {
    auto* state = static_cast<InstanceState*>(user);
    const char* msg = (data && data->pMessage) ? data->pMessage : "";
    const DebugSeverity sev = static_cast<DebugSeverity>(severity);

    if (state && state->debug_callback) {
        state->debug_callback(sev, msg);
    } else {
        const char* tag = sev == DebugSeverity::Error ? "error" : sev == DebugSeverity::Warning ? "warning" : "info";
        std::fprintf(stderr, "[fizmo::vulkan %s] %s\n", tag, msg);
    }

    return VK_FALSE;
}

inline bool contains_name(const std::vector<VkExtensionProperties>& list, const char* name) noexcept {
    for (const auto& e : list) if (std::strcmp(e.extensionName, name) == 0) return true;
    return false;
}

inline bool contains_name(const std::vector<VkLayerProperties>& list, const char* name) noexcept {
    for (const auto& l : list) if (std::strcmp(l.layerName, name) == 0) return true;
    return false;
}

inline VkDebugUtilsMessageSeverityFlagsEXT severity_mask_from(DebugSeverity min) noexcept {
    VkDebugUtilsMessageSeverityFlagsEXT mask = 0;

    const std::uint32_t levels[] = {
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT, VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT,
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT, VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT
    };

    for (std::uint32_t l : levels) if (l >= static_cast<std::uint32_t>(min)) mask |= l;
    return mask;
}

} // namespace detail

struct QueueFamilyInfo {
    std::uint32_t index = 0;
    std::uint32_t queue_count = 0;
    bool graphics = false;
    bool compute  = false;
    bool transfer = false;
};

class PhysicalDevice {
private:
    const detail::InstanceState* m_instance = nullptr;
    native::PhysicalDevice       m_handle   = VK_NULL_HANDLE;

public:
    PhysicalDevice() noexcept = default;
    PhysicalDevice(const detail::InstanceState* inst, native::PhysicalDevice h) noexcept : m_instance(inst), m_handle(h) {}

    bool valid() const noexcept { return m_instance && m_handle; }
    native::PhysicalDevice handle() const noexcept { return m_handle; }
    const detail::InstanceState* instance_state() const noexcept { return m_instance; }

    VkPhysicalDeviceProperties properties() const noexcept {
        VkPhysicalDeviceProperties p{};
        m_instance->fn.vkGetPhysicalDeviceProperties(m_handle, &p);
        return p;
    }

    VkPhysicalDeviceMemoryProperties memory_properties() const noexcept {
        VkPhysicalDeviceMemoryProperties p{};
        m_instance->fn.vkGetPhysicalDeviceMemoryProperties(m_handle, &p);
        return p;
    }

    std::string   name()        const { return properties().deviceName; }
    DeviceType    type()        const noexcept { return static_cast<DeviceType>(properties().deviceType); }
    std::uint32_t api_version() const noexcept { return properties().apiVersion; }

    std::uint64_t device_local_memory() const noexcept {
        const auto mem = memory_properties();
        std::uint64_t total = 0;
        for (std::uint32_t i = 0; i < mem.memoryHeapCount; ++i) if (mem.memoryHeaps[i].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT) total += mem.memoryHeaps[i].size;
        return total;
    }

    std::vector<QueueFamilyInfo> queue_families() const {
        std::uint32_t n = 0;
        m_instance->fn.vkGetPhysicalDeviceQueueFamilyProperties(m_handle, &n, nullptr);
        std::vector<VkQueueFamilyProperties> raw(n);
        m_instance->fn.vkGetPhysicalDeviceQueueFamilyProperties(m_handle, &n, raw.data());
        std::vector<QueueFamilyInfo> out(n);

        for (std::uint32_t i = 0; i < n; ++i) {
            out[i].index       = i;
            out[i].queue_count = raw[i].queueCount;
            out[i].graphics    = (raw[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0;
            out[i].compute     = (raw[i].queueFlags & VK_QUEUE_COMPUTE_BIT)  != 0;
            out[i].transfer    = (raw[i].queueFlags & (VK_QUEUE_TRANSFER_BIT | VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT)) != 0;
        }

        return out;
    }

    std::vector<VkExtensionProperties> extensions() const {
        std::uint32_t n = 0;
        m_instance->fn.vkEnumerateDeviceExtensionProperties(m_handle, nullptr, &n, nullptr);
        std::vector<VkExtensionProperties> list(n);
        m_instance->fn.vkEnumerateDeviceExtensionProperties(m_handle, nullptr, &n, list.data());
        return list;
    }

    bool supports_extension(const char* name) const { return detail::contains_name(extensions(), name); }

    bool supports_format(Format format, VkFormatFeatureFlags features, bool optimal_tiling = true) const noexcept {
        VkFormatProperties p{};
        m_instance->fn.vkGetPhysicalDeviceFormatProperties(m_handle, to_vk(format), &p);
        const VkFormatFeatureFlags have = optimal_tiling ? p.optimalTilingFeatures : p.linearTilingFeatures;
        return (have & features) == features;
    }

    Format best_depth_format(bool need_stencil = false) const noexcept {
        const Format candidates[] = { Format::D32Float, Format::D32FloatS8Uint, Format::D24UnormS8Uint, Format::D16Unorm };
        
        for (Format f : candidates) {
            if (need_stencil && !has_stencil(f)) continue;
            if (supports_format(f, VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT)) return f;
        }

        return Format::Undefined;
    }

    inline bool supports_present(const Surface& surface, std::uint32_t queue_family) const noexcept;
};

class Instance {
private:
    std::unique_ptr<detail::InstanceState> m_state;

public:
    Instance() noexcept = default;
    Instance(Instance&&) noexcept = default;
    Instance& operator=(Instance&&) noexcept = default;
    Instance(const Instance&) = delete;
    Instance& operator=(const Instance&) = delete;
    ~Instance() noexcept = default;

    Result create(const InstanceDesc& desc = {}) {
        destroy();
        auto state = std::make_unique<detail::InstanceState>();
        if (!state->library.load()) return Result::LibraryNotFound;
        if (!state->global.load(state->library.get_instance_proc_addr())) return Result::MissingEntryPoint;
        std::uint32_t loader_version = VK_API_VERSION_1_0;
        if (state->global.vkEnumerateInstanceVersion) state->global.vkEnumerateInstanceVersion(&loader_version);
        if (loader_version < kApiVersion13) return Result::IncompatibleDriver;
        std::uint32_t n = 0;
        state->global.vkEnumerateInstanceExtensionProperties(nullptr, &n, nullptr);
        std::vector<VkExtensionProperties> available_ext(n);
        state->global.vkEnumerateInstanceExtensionProperties(nullptr, &n, available_ext.data());
        n = 0;
        state->global.vkEnumerateInstanceLayerProperties(&n, nullptr);
        std::vector<VkLayerProperties> available_layers(n);
        state->global.vkEnumerateInstanceLayerProperties(&n, available_layers.data());
        std::vector<const char*> extensions;
        std::vector<const char*> layers;

        if (desc.enable_surface) {
            extensions.push_back(VK_KHR_SURFACE_EXTENSION_NAME);
        #if defined(OS_WINDOWS)
            extensions.push_back(VK_KHR_WIN32_SURFACE_EXTENSION_NAME);
        #else
            extensions.push_back(VK_KHR_XLIB_SURFACE_EXTENSION_NAME);
        #endif
            for (const char* e : extensions) if (!detail::contains_name(available_ext, e)) return Result::ExtensionNotPresent;
            state->surface = true;
        }

        const bool want_debug = desc.enable_validation && detail::contains_name(available_ext, VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
        if (want_debug) { extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME); state->debug_utils = true; }

        if (desc.enable_validation && detail::contains_name(available_layers, "VK_LAYER_KHRONOS_validation")) {
            layers.push_back("VK_LAYER_KHRONOS_validation");
            state->validation = true;
        }

        for (const char* e : desc.extra_extensions) extensions.push_back(e);
        for (const char* l : desc.extra_layers)     layers.push_back(l);
        auto app                       = detail::make<VkApplicationInfo>(VK_STRUCTURE_TYPE_APPLICATION_INFO);
        app.pApplicationName           = desc.app_name.c_str();
        app.applicationVersion         = desc.app_version;
        app.pEngineName                = desc.engine_name.c_str();
        app.engineVersion              = desc.engine_version;
        app.apiVersion                 = kApiVersion13;
        state->debug_callback          = desc.debug_callback;
        auto messenger_info            = detail::make<VkDebugUtilsMessengerCreateInfoEXT>(VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT);
        messenger_info.messageSeverity = detail::severity_mask_from(desc.min_severity);
        messenger_info.messageType     = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
        messenger_info.pfnUserCallback = detail::debug_trampoline;
        messenger_info.pUserData       = state.get();
        auto info                      = detail::make<VkInstanceCreateInfo>(VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO);
        info.pNext                     = state->debug_utils ? &messenger_info : nullptr;  // also catches create/destroy errors
        info.pApplicationInfo          = &app;
        info.enabledExtensionCount     = static_cast<std::uint32_t>(extensions.size());
        info.ppEnabledExtensionNames   = extensions.data();
        info.enabledLayerCount         = static_cast<std::uint32_t>(layers.size());
        info.ppEnabledLayerNames       = layers.data();
        VkResult r                     = state->global.vkCreateInstance(&info, nullptr, &state->handle);
        if (r != VK_SUCCESS) { state->handle = VK_NULL_HANDLE; return to_result(r); }
        if (!state->fn.load(state->library.get_instance_proc_addr(), state->handle)) return Result::MissingEntryPoint;

        if (state->debug_utils && state->fn.vkCreateDebugUtilsMessengerEXT) {
            state->fn.vkCreateDebugUtilsMessengerEXT(state->handle, &messenger_info, nullptr, &state->messenger);
        }

        state->api_version = kApiVersion13;
        m_state = std::move(state);
        return Result::Success;
    }

    void destroy() noexcept { m_state.reset(); }

    bool valid() const noexcept { return m_state && m_state->handle; }
    explicit operator bool() const noexcept { return valid(); }

    native::Instance handle() const noexcept { return m_state ? m_state->handle : VK_NULL_HANDLE; }
    const InstanceDispatch& fn() const noexcept { return m_state->fn; }
    const detail::InstanceState* state() const noexcept { return m_state.get(); }

    bool has_validation()  const noexcept { return m_state && m_state->validation; }
    bool has_debug_utils() const noexcept { return m_state && m_state->debug_utils; }

    std::vector<PhysicalDevice> physical_devices() const {
        std::vector<PhysicalDevice> out;
        if (!valid()) return out;
        std::uint32_t n = 0;
        m_state->fn.vkEnumeratePhysicalDevices(m_state->handle, &n, nullptr);
        std::vector<native::PhysicalDevice> raw(n);
        m_state->fn.vkEnumeratePhysicalDevices(m_state->handle, &n, raw.data());
        out.reserve(n);
        for (auto h : raw) out.emplace_back(m_state.get(), h);
        return out;
    }
};

class Surface {
private:
    const detail::InstanceState* m_instance = nullptr;
    native::Surface              m_handle   = VK_NULL_HANDLE;

public:
    Surface() noexcept = default;
    ~Surface() noexcept { destroy(); }

    Surface(const Surface&) = delete;
    Surface& operator=(const Surface&) = delete;

    Surface(Surface&& o) noexcept
        : m_instance(std::exchange(o.m_instance, nullptr)), m_handle(std::exchange(o.m_handle, VK_NULL_HANDLE)) {}

    Surface& operator=(Surface&& o) noexcept {
        if (this != &o) {
            destroy();
            m_instance = std::exchange(o.m_instance, nullptr);
            m_handle   = std::exchange(o.m_handle, VK_NULL_HANDLE);
        }

        return *this;
    }

    Result create(const Instance& instance, void* native_window) noexcept {
        destroy();
        if (!instance.valid() || !native_window) return Result::InvalidArgument;
        const auto* st = instance.state();
        if (!st->surface) return Result::ExtensionNotPresent;

    #if defined(OS_WINDOWS)
        if (!st->fn.vkCreateWin32SurfaceKHR) return Result::MissingEntryPoint;
        auto info = detail::make<VkWin32SurfaceCreateInfoKHR>(VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR);
        info.hinstance = GetModuleHandleW(nullptr);
        info.hwnd      = static_cast<HWND>(native_window);
        VkResult r = st->fn.vkCreateWin32SurfaceKHR(st->handle, &info, nullptr, &m_handle);
    #else
        if (!st->fn.vkCreateXlibSurfaceKHR) return Result::MissingEntryPoint;
        const auto* h = static_cast<const x11::Handle*>(native_window);
        if (!h->display || !h->window) return Result::InvalidArgument;
        auto info = detail::make<VkXlibSurfaceCreateInfoKHR>(VK_STRUCTURE_TYPE_XLIB_SURFACE_CREATE_INFO_KHR);
        info.dpy    = h->display;
        info.window = h->window;
        VkResult r = st->fn.vkCreateXlibSurfaceKHR(st->handle, &info, nullptr, &m_handle);
    #endif

        if (r != VK_SUCCESS) { m_handle = VK_NULL_HANDLE; return to_result(r); }
        m_instance = st;
        return Result::Success;
    }

    void destroy() noexcept {
        if (m_handle && m_instance) m_instance->fn.vkDestroySurfaceKHR(m_instance->handle, m_handle, nullptr);
        m_handle   = VK_NULL_HANDLE;
        m_instance = nullptr;
    }

    bool valid() const noexcept { return m_handle != VK_NULL_HANDLE; }
    native::Surface handle() const noexcept { return m_handle; }

    VkSurfaceCapabilitiesKHR capabilities(const PhysicalDevice& gpu) const noexcept {
        VkSurfaceCapabilitiesKHR caps{};
        m_instance->fn.vkGetPhysicalDeviceSurfaceCapabilitiesKHR(gpu.handle(), m_handle, &caps);
        return caps;
    }

    std::vector<VkSurfaceFormatKHR> formats(const PhysicalDevice& gpu) const {
        std::uint32_t n = 0;
        m_instance->fn.vkGetPhysicalDeviceSurfaceFormatsKHR(gpu.handle(), m_handle, &n, nullptr);
        std::vector<VkSurfaceFormatKHR> list(n);
        m_instance->fn.vkGetPhysicalDeviceSurfaceFormatsKHR(gpu.handle(), m_handle, &n, list.data());
        return list;
    }

    std::vector<PresentMode> present_modes(const PhysicalDevice& gpu) const {
        std::uint32_t n = 0;
        m_instance->fn.vkGetPhysicalDeviceSurfacePresentModesKHR(gpu.handle(), m_handle, &n, nullptr);
        std::vector<VkPresentModeKHR> raw(n);
        m_instance->fn.vkGetPhysicalDeviceSurfacePresentModesKHR(gpu.handle(), m_handle, &n, raw.data());
        std::vector<PresentMode> out;
        out.reserve(n);
        for (auto m : raw) out.push_back(static_cast<PresentMode>(m));
        return out;
    }
};

inline bool PhysicalDevice::supports_present(const Surface& surface, std::uint32_t queue_family) const noexcept {
    if (!surface.valid() || !m_instance->fn.vkGetPhysicalDeviceSurfaceSupportKHR) return false;
    native::Bool32 ok = VK_FALSE;
    m_instance->fn.vkGetPhysicalDeviceSurfaceSupportKHR(m_handle, queue_family, surface.handle(), &ok);
    return ok == VK_TRUE;
}

} // namespace vulkan
} // namespace fizmo

#endif // FIZMO_VULKAN_INSTANCE_HPP