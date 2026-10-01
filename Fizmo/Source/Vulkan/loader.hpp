#ifndef FIZMO_VULKAN_LOADER_HPP
#define FIZMO_VULKAN_LOADER_HPP

#include "compat.hpp"
#include <utility>

#define FIZMO_VK_GLOBAL_FUNCTIONS(X)            \
    X(vkCreateInstance)                         \
    X(vkEnumerateInstanceVersion)               \
    X(vkEnumerateInstanceExtensionProperties)   \
    X(vkEnumerateInstanceLayerProperties)

#define FIZMO_VK_INSTANCE_FUNCTIONS_CORE(X)             \
    X(vkDestroyInstance)                                \
    X(vkEnumeratePhysicalDevices)                       \
    X(vkGetPhysicalDeviceProperties)                    \
    X(vkGetPhysicalDeviceProperties2)                   \
    X(vkGetPhysicalDeviceFeatures)                      \
    X(vkGetPhysicalDeviceFeatures2)                     \
    X(vkGetPhysicalDeviceMemoryProperties)              \
    X(vkGetPhysicalDeviceQueueFamilyProperties)         \
    X(vkGetPhysicalDeviceFormatProperties)              \
    X(vkEnumerateDeviceExtensionProperties)             \
    X(vkCreateDevice)                                   \
    X(vkGetDeviceProcAddr)                              \
    X(vkDestroySurfaceKHR)                              \
    X(vkGetPhysicalDeviceSurfaceSupportKHR)             \
    X(vkGetPhysicalDeviceSurfaceCapabilitiesKHR)        \
    X(vkGetPhysicalDeviceSurfaceFormatsKHR)             \
    X(vkGetPhysicalDeviceSurfacePresentModesKHR)        \
    X(vkCreateDebugUtilsMessengerEXT)                   \
    X(vkDestroyDebugUtilsMessengerEXT)                  \
    X(vkSetDebugUtilsObjectNameEXT)                     \
    X(vkCmdBeginDebugUtilsLabelEXT)                     \
    X(vkCmdEndDebugUtilsLabelEXT)                       \
    X(vkCmdInsertDebugUtilsLabelEXT)

#if defined(OS_WINDOWS)
    #define FIZMO_VK_INSTANCE_FUNCTIONS_PLATFORM(X)         \
        X(vkCreateWin32SurfaceKHR)                          \
        X(vkGetPhysicalDeviceWin32PresentationSupportKHR)
#else
    #define FIZMO_VK_INSTANCE_FUNCTIONS_PLATFORM(X)         \
        X(vkCreateXlibSurfaceKHR)                           \
        X(vkGetPhysicalDeviceXlibPresentationSupportKHR)
#endif

#define FIZMO_VK_INSTANCE_FUNCTIONS(X)          \
    FIZMO_VK_INSTANCE_FUNCTIONS_CORE(X)         \
    FIZMO_VK_INSTANCE_FUNCTIONS_PLATFORM(X)

#define FIZMO_VK_DEVICE_FUNCTIONS(X)            \
    X(vkDestroyDevice)                          \
    X(vkGetDeviceQueue)                         \
    X(vkDeviceWaitIdle)                         \
    X(vkQueueSubmit2)                           \
    X(vkQueueWaitIdle)                          \
    X(vkQueuePresentKHR)                        \
    X(vkCreateSwapchainKHR)                     \
    X(vkDestroySwapchainKHR)                    \
    X(vkGetSwapchainImagesKHR)                  \
    X(vkAcquireNextImageKHR)                    \
    X(vkAllocateMemory)                         \
    X(vkFreeMemory)                             \
    X(vkMapMemory)                              \
    X(vkUnmapMemory)                            \
    X(vkFlushMappedMemoryRanges)                \
    X(vkInvalidateMappedMemoryRanges)           \
    X(vkCreateBuffer)                           \
    X(vkDestroyBuffer)                          \
    X(vkGetBufferMemoryRequirements)            \
    X(vkBindBufferMemory)                       \
    X(vkGetBufferDeviceAddress)                 \
    X(vkCreateImage)                            \
    X(vkDestroyImage)                           \
    X(vkGetImageMemoryRequirements)             \
    X(vkBindImageMemory)                        \
    X(vkCreateImageView)                        \
    X(vkDestroyImageView)                       \
    X(vkCreateSampler)                          \
    X(vkDestroySampler)                         \
    X(vkCreateShaderModule)                     \
    X(vkDestroyShaderModule)                    \
    X(vkCreatePipelineLayout)                   \
    X(vkDestroyPipelineLayout)                  \
    X(vkCreateGraphicsPipelines)                \
    X(vkCreateComputePipelines)                 \
    X(vkDestroyPipeline)                        \
    X(vkCreateDescriptorSetLayout)              \
    X(vkDestroyDescriptorSetLayout)             \
    X(vkCreateDescriptorPool)                   \
    X(vkDestroyDescriptorPool)                  \
    X(vkResetDescriptorPool)                    \
    X(vkAllocateDescriptorSets)                 \
    X(vkFreeDescriptorSets)                     \
    X(vkUpdateDescriptorSets)                   \
    X(vkCreateCommandPool)                      \
    X(vkDestroyCommandPool)                     \
    X(vkResetCommandPool)                       \
    X(vkAllocateCommandBuffers)                 \
    X(vkFreeCommandBuffers)                     \
    X(vkBeginCommandBuffer)                     \
    X(vkEndCommandBuffer)                       \
    X(vkResetCommandBuffer)                     \
    X(vkCreateFence)                            \
    X(vkDestroyFence)                           \
    X(vkWaitForFences)                          \
    X(vkResetFences)                            \
    X(vkGetFenceStatus)                         \
    X(vkCreateSemaphore)                        \
    X(vkDestroySemaphore)                       \
    X(vkWaitSemaphores)                         \
    X(vkSignalSemaphore)                        \
    X(vkGetSemaphoreCounterValue)               \
    X(vkCmdBeginRendering)                      \
    X(vkCmdEndRendering)                        \
    X(vkCmdPipelineBarrier2)                    \
    X(vkCmdBindPipeline)                        \
    X(vkCmdBindVertexBuffers)                   \
    X(vkCmdBindIndexBuffer)                     \
    X(vkCmdBindDescriptorSets)                  \
    X(vkCmdPushConstants)                       \
    X(vkCmdSetViewport)                         \
    X(vkCmdSetScissor)                          \
    X(vkCmdDraw)                                \
    X(vkCmdDrawIndexed)                         \
    X(vkCmdDrawIndirect)                        \
    X(vkCmdDrawIndexedIndirect)                 \
    X(vkCmdDispatch)                            \
    X(vkCmdDispatchIndirect)                    \
    X(vkCmdCopyBuffer)                          \
    X(vkCmdCopyBufferToImage)                   \
    X(vkCmdCopyImageToBuffer)                   \
    X(vkCmdCopyImage)                           \
    X(vkCmdBlitImage)                           \
    X(vkCmdClearColorImage)                     \
    X(vkCmdFillBuffer)                          \
    X(vkCmdUpdateBuffer)                        \
    X(vkCreateQueryPool)                        \
    X(vkDestroyQueryPool)                       \
    X(vkGetQueryPoolResults)                    \
    X(vkCmdResetQueryPool)                      \
    X(vkCmdWriteTimestamp2)

namespace fizmo {
namespace vulkan {

class Library {
private:
    native::LibraryHandle      m_handle   = nullptr;
    PFN_vkGetInstanceProcAddr  m_get_proc = nullptr;

public:
    Library() noexcept = default;
    ~Library() noexcept { unload(); }

    Library(const Library&) = delete;
    Library& operator=(const Library&) = delete;

    Library(Library&& o) noexcept
        : m_handle(std::exchange(o.m_handle, nullptr)), m_get_proc(std::exchange(o.m_get_proc, nullptr)) {}

    Library& operator=(Library&& o) noexcept {
        if (this != &o) {
            unload();
            m_handle   = std::exchange(o.m_handle, nullptr);
            m_get_proc = std::exchange(o.m_get_proc, nullptr);
        }
        return *this;
    }

    bool load() noexcept {
        if (m_get_proc) return true;

    #if defined(OS_WINDOWS)
        m_handle = LoadLibraryA("vulkan-1.dll");
        if (!m_handle) return false;
        m_get_proc = reinterpret_cast<PFN_vkGetInstanceProcAddr>(reinterpret_cast<void*>(GetProcAddress(m_handle, "vkGetInstanceProcAddr")));
    #else
        m_handle = dlopen("libvulkan.so.1", RTLD_NOW | RTLD_LOCAL);
        if (!m_handle) m_handle = dlopen("libvulkan.so", RTLD_NOW | RTLD_LOCAL);
        if (!m_handle) return false;
        m_get_proc = reinterpret_cast<PFN_vkGetInstanceProcAddr>(dlsym(m_handle, "vkGetInstanceProcAddr"));
    #endif

        if (!m_get_proc) { unload(); return false; }
        return true;
    }

    void unload() noexcept {
        m_get_proc = nullptr;
        if (!m_handle) return;
    #if defined(OS_WINDOWS)
        FreeLibrary(m_handle);
    #else
        dlclose(m_handle);
    #endif
        m_handle = nullptr;
    }

    bool is_loaded() const noexcept { return m_get_proc != nullptr; }
    PFN_vkGetInstanceProcAddr get_instance_proc_addr() const noexcept { return m_get_proc; }
};

#define FIZMO_VK_DECLARE_PFN(name) PFN_##name name = nullptr;

struct GlobalDispatch {
    PFN_vkGetInstanceProcAddr vkGetInstanceProcAddr = nullptr;
    FIZMO_VK_GLOBAL_FUNCTIONS(FIZMO_VK_DECLARE_PFN)

    bool load(PFN_vkGetInstanceProcAddr gipa) noexcept {
        vkGetInstanceProcAddr = gipa;
        if (!gipa) return false;
        #define FIZMO_VK_LOAD(name) name = reinterpret_cast<PFN_##name>(gipa(nullptr, #name));
        FIZMO_VK_GLOBAL_FUNCTIONS(FIZMO_VK_LOAD)
        #undef FIZMO_VK_LOAD
        return vkCreateInstance != nullptr;
    }
};

struct InstanceDispatch {
    FIZMO_VK_INSTANCE_FUNCTIONS(FIZMO_VK_DECLARE_PFN)

    bool load(PFN_vkGetInstanceProcAddr gipa, native::Instance instance) noexcept {
        if (!gipa || !instance) return false;
        #define FIZMO_VK_LOAD(name) name = reinterpret_cast<PFN_##name>(gipa(instance, #name));
        FIZMO_VK_INSTANCE_FUNCTIONS(FIZMO_VK_LOAD)
        #undef FIZMO_VK_LOAD
        return vkDestroyInstance && vkEnumeratePhysicalDevices && vkCreateDevice && vkGetDeviceProcAddr;
    }
};

struct DeviceDispatch {
    FIZMO_VK_DEVICE_FUNCTIONS(FIZMO_VK_DECLARE_PFN)

    bool load(PFN_vkGetDeviceProcAddr gdpa, native::Device device) noexcept {
        if (!gdpa || !device) return false;
        #define FIZMO_VK_LOAD(name) name = reinterpret_cast<PFN_##name>(gdpa(device, #name));
        FIZMO_VK_DEVICE_FUNCTIONS(FIZMO_VK_LOAD)
        #undef FIZMO_VK_LOAD
        return vkDestroyDevice && vkGetDeviceQueue && vkQueueSubmit2 && vkCmdBeginRendering && vkCmdPipelineBarrier2;
    }
};

#undef FIZMO_VK_DECLARE_PFN

} // namespace vulkan
} // namespace fizmo

#endif // FIZMO_VULKAN_LOADER_HPP