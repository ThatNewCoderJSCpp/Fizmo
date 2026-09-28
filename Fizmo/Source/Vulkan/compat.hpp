#ifndef FIZMO_VULKAN_COMPAT_HPP
#define FIZMO_VULKAN_COMPAT_HPP

#include "../Basic/fizmo_defines.hpp"

#ifndef VK_NO_PROTOTYPES
    #define VK_NO_PROTOTYPES
#endif

#if defined(OS_WINDOWS)
    #ifndef VK_USE_PLATFORM_WIN32_KHR
        #define VK_USE_PLATFORM_WIN32_KHR
    #endif
#elif defined(OS_LINUX)
    #ifndef VK_USE_PLATFORM_XLIB_KHR
        #define VK_USE_PLATFORM_XLIB_KHR
    #endif
    #include <dlfcn.h>
#endif

#include <vulkan/vulkan.h>
#include <cstdint>
#include <cstddef>

#if defined(OS_LINUX)
    #include "../x11_compat.hpp"
#endif

namespace fizmo {
namespace vulkan {
namespace native {

using Instance            = ::VkInstance;
using PhysicalDevice      = ::VkPhysicalDevice;
using Device              = ::VkDevice;
using Queue               = ::VkQueue;
using Surface             = ::VkSurfaceKHR;
using Swapchain           = ::VkSwapchainKHR;
using CommandPool         = ::VkCommandPool;
using CommandBuffer       = ::VkCommandBuffer;
using Fence               = ::VkFence;
using Semaphore           = ::VkSemaphore;
using DeviceMemory        = ::VkDeviceMemory;
using Buffer              = ::VkBuffer;
using Image               = ::VkImage;
using ImageView           = ::VkImageView;
using Sampler             = ::VkSampler;
using ShaderModule        = ::VkShaderModule;
using DescriptorSetLayout = ::VkDescriptorSetLayout;
using DescriptorPool      = ::VkDescriptorPool;
using DescriptorSet       = ::VkDescriptorSet;
using PipelineLayout      = ::VkPipelineLayout;
using Pipeline            = ::VkPipeline;
using DebugMessenger      = ::VkDebugUtilsMessengerEXT;
using DeviceSize          = ::VkDeviceSize;
using DeviceAddress       = ::VkDeviceAddress;
using Bool32              = ::VkBool32;

#if defined(OS_WINDOWS)
using LibraryHandle = ::HMODULE;
#else
using LibraryHandle = void*;
#endif

} // namespace native

inline constexpr std::uint32_t kQueueFamilyIgnored = VK_QUEUE_FAMILY_IGNORED;
inline constexpr std::uint64_t kWholeSize          = VK_WHOLE_SIZE;
inline constexpr std::uint32_t kRemainingMips      = VK_REMAINING_MIP_LEVELS;
inline constexpr std::uint32_t kRemainingLayers    = VK_REMAINING_ARRAY_LAYERS;
inline constexpr std::uint64_t kNoTimeout          = UINT64_MAX;

inline constexpr std::uint32_t make_version(std::uint32_t major, std::uint32_t minor, std::uint32_t patch) noexcept {
    return VK_MAKE_API_VERSION(0, major, minor, patch);
}

inline constexpr std::uint32_t version_major(std::uint32_t v) noexcept { return VK_API_VERSION_MAJOR(v); }
inline constexpr std::uint32_t version_minor(std::uint32_t v) noexcept { return VK_API_VERSION_MINOR(v); }
inline constexpr std::uint32_t version_patch(std::uint32_t v) noexcept { return VK_API_VERSION_PATCH(v); }

inline constexpr std::uint32_t kApiVersion13 = VK_API_VERSION_1_3;

namespace detail {

template <typename T>
inline T make(VkStructureType type) noexcept {
    T value{};
    value.sType = type;
    return value;
}

} // namespace detail

} // namespace vulkan
} // namespace fizmo

#endif // FIZMO_VULKAN_COMPAT_HPP