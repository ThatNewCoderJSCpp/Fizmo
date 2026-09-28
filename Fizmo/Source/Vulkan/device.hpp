#ifndef FIZMO_VULKAN_DEVICE_HPP
#define FIZMO_VULKAN_DEVICE_HPP

#include "instance.hpp"
#include <mutex>

namespace fizmo {
namespace vulkan {

class Device;
class CommandBuffer;
class Fence;
class Semaphore;

struct DeviceDesc {
    const Surface* surface = nullptr;

    DeviceType preferred_type = DeviceType::Discrete;

    PhysicalDevice force_device;

    std::vector<const char*> extra_extensions;
};

struct DeviceFeatures {
    bool dynamic_rendering     = false;   
    bool synchronization2      = false;   
    bool timeline_semaphores   = false;   
    bool buffer_device_address = false;
    bool descriptor_indexing   = false;
    bool sampler_anisotropy    = false;
    bool fill_mode_non_solid   = false;   
    bool wide_lines            = false;
    bool swapchain             = false;
};

namespace detail {

struct QueueSlot {
    native::Queue handle = VK_NULL_HANDLE;
    std::uint32_t family = kQueueFamilyIgnored;
};

struct DeviceState {
    const InstanceState*             instance = nullptr;
    native::PhysicalDevice           physical = VK_NULL_HANDLE;
    native::Device                   handle   = VK_NULL_HANDLE;
    DeviceDispatch                   fn;
    VkPhysicalDeviceProperties       properties{};
    VkPhysicalDeviceMemoryProperties memory{};
    DeviceFeatures                   features;

    QueueSlot graphics;   
    QueueSlot present;    
    QueueSlot compute;    
    QueueSlot transfer;   

    std::mutex             immediate_mutex;
    native::CommandPool    immediate_pool  = VK_NULL_HANDLE;
    native::CommandBuffer  immediate_cmd   = VK_NULL_HANDLE;
    native::Fence          immediate_fence = VK_NULL_HANDLE;

    ~DeviceState() noexcept {
        if (!handle) return;
        fn.vkDeviceWaitIdle(handle);
        if (immediate_fence) fn.vkDestroyFence(handle, immediate_fence, nullptr);
        if (immediate_pool)  fn.vkDestroyCommandPool(handle, immediate_pool, nullptr);
        fn.vkDestroyDevice(handle, nullptr);
    }

    std::uint32_t find_memory_type(std::uint32_t type_bits, VkMemoryPropertyFlags want, VkMemoryPropertyFlags avoid = 0) const noexcept {
        for (std::uint32_t i = 0; i < memory.memoryTypeCount; ++i) {
            const VkMemoryPropertyFlags f = memory.memoryTypes[i].propertyFlags;
            if ((type_bits & (1u << i)) && (f & want) == want && (f & avoid) == 0) return i;
        }

        return UINT32_MAX;
    }

    void set_name(VkObjectType type, std::uint64_t object, const char* name) const noexcept {
        if (!instance->fn.vkSetDebugUtilsObjectNameEXT || !name) return;
        auto info = detail::make<VkDebugUtilsObjectNameInfoEXT>(VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT);
        info.objectType   = type;
        info.objectHandle = object;
        info.pObjectName  = name;
        instance->fn.vkSetDebugUtilsObjectNameEXT(handle, &info);
    }
};

template <typename H, typename PFN, PFN DeviceDispatch::* Destroy>
class UniqueHandle {
private:
    const DeviceState* m_device = nullptr;
    H                  m_handle = VK_NULL_HANDLE;

public:
    UniqueHandle() noexcept = default;
    UniqueHandle(const DeviceState* d, H h) noexcept : m_device(d), m_handle(h) {}
    ~UniqueHandle() noexcept { reset(); }

    UniqueHandle(const UniqueHandle&) = delete;
    UniqueHandle& operator=(const UniqueHandle&) = delete;

    UniqueHandle(UniqueHandle&& o) noexcept
        : m_device(std::exchange(o.m_device, nullptr)), m_handle(std::exchange(o.m_handle, VK_NULL_HANDLE)) {}

    UniqueHandle& operator=(UniqueHandle&& o) noexcept {
        if (this != &o) {
            reset();
            m_device = std::exchange(o.m_device, nullptr);
            m_handle = std::exchange(o.m_handle, VK_NULL_HANDLE);
        }

        return *this;
    }

    void reset() noexcept {
        if (m_handle && m_device) (m_device->fn.*Destroy)(m_device->handle, m_handle, nullptr);
        m_handle = VK_NULL_HANDLE;
        m_device = nullptr;
    }

    H                  get()    const noexcept { return m_handle; }
    const DeviceState* device() const noexcept { return m_device; }
    explicit operator bool()    const noexcept { return m_handle != VK_NULL_HANDLE; }
};

#define FIZMO_VK_UNIQUE(Handle, DestroyFn) \
    ::fizmo::vulkan::detail::UniqueHandle<Handle, PFN_##DestroyFn, &::fizmo::vulkan::DeviceDispatch::DestroyFn>

} // namespace detail

class Fence {
private:
    FIZMO_VK_UNIQUE(VkFence, vkDestroyFence) m_fence;

public:
    Fence() noexcept = default;
    inline Result create(const Device& device, bool signaled = false) noexcept;
    void destroy() noexcept { m_fence.reset(); }

    Result wait(std::uint64_t timeout_ns = kNoTimeout) const noexcept {
        auto* d = m_fence.device();
        if (!d) return Result::NotInitialized;
        native::Fence f = m_fence.get();
        return to_result(d->fn.vkWaitForFences(d->handle, 1, &f, VK_TRUE, timeout_ns));
    }

    Result reset() noexcept {
        auto* d = m_fence.device();
        if (!d) return Result::NotInitialized;
        native::Fence f = m_fence.get();
        return to_result(d->fn.vkResetFences(d->handle, 1, &f));
    }

    bool is_signaled() const noexcept {
        auto* d = m_fence.device();
        return d && d->fn.vkGetFenceStatus(d->handle, m_fence.get()) == VK_SUCCESS;
    }

    bool valid() const noexcept { return static_cast<bool>(m_fence); }
    native::Fence handle() const noexcept { return m_fence.get(); }
};

class Semaphore {
private:
    FIZMO_VK_UNIQUE(VkSemaphore, vkDestroySemaphore) m_semaphore;
    bool m_timeline = false;

public:
    Semaphore() noexcept = default;

    inline Result create(const Device& device) noexcept;
    inline Result create_timeline(const Device& device, std::uint64_t initial_value = 0) noexcept;
    void destroy() noexcept { m_semaphore.reset(); }

    bool is_timeline() const noexcept { return m_timeline; }
    bool valid() const noexcept { return static_cast<bool>(m_semaphore); }
    native::Semaphore handle() const noexcept { return m_semaphore.get(); }

    std::uint64_t value() const noexcept {
        auto* d = m_semaphore.device();
        std::uint64_t v = 0;
        if (d && m_timeline) d->fn.vkGetSemaphoreCounterValue(d->handle, m_semaphore.get(), &v);
        return v;
    }

    Result signal(std::uint64_t value) noexcept {
        auto* d = m_semaphore.device();
        if (!d || !m_timeline) return Result::InvalidArgument;
        auto info = detail::make<VkSemaphoreSignalInfo>(VK_STRUCTURE_TYPE_SEMAPHORE_SIGNAL_INFO);
        info.semaphore = m_semaphore.get();
        info.value     = value;
        return to_result(d->fn.vkSignalSemaphore(d->handle, &info));
    }

    Result wait(std::uint64_t value, std::uint64_t timeout_ns = kNoTimeout) const noexcept {
        auto* d = m_semaphore.device();
        if (!d || !m_timeline) return Result::InvalidArgument;
        native::Semaphore s = m_semaphore.get();
        auto info = detail::make<VkSemaphoreWaitInfo>(VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO);
        info.semaphoreCount = 1;
        info.pSemaphores    = &s;
        info.pValues        = &value;
        return to_result(d->fn.vkWaitSemaphores(d->handle, &info, timeout_ns));
    }
};

struct SemaphoreWait {
    const Semaphore* semaphore = nullptr;
    PipelineStage    stage     = PipelineStage::AllCommands;
    std::uint64_t    value     = 0;   
};

using SemaphoreSignal = SemaphoreWait;

class Queue {
private:
    const detail::DeviceState* m_device = nullptr;
    native::Queue              m_handle = VK_NULL_HANDLE;
    std::uint32_t              m_family = kQueueFamilyIgnored;

public:
    Queue() noexcept = default;
    Queue(const detail::DeviceState* d, const detail::QueueSlot& slot) noexcept
        : m_device(d), m_handle(slot.handle), m_family(slot.family) {}

    bool valid() const noexcept { return m_handle != VK_NULL_HANDLE; }
    native::Queue handle() const noexcept { return m_handle; }
    std::uint32_t family() const noexcept { return m_family; }
    const detail::DeviceState* device_state() const noexcept { return m_device; }

    Result wait_idle() const noexcept {
        return m_device ? to_result(m_device->fn.vkQueueWaitIdle(m_handle)) : Result::NotInitialized;
    }

    inline Result submit(
        Span<CommandBuffer> command_buffers,
        Span<SemaphoreWait> waits = {},
        Span<SemaphoreSignal> signals = {},
        const Fence* fence = nullptr
    ) const noexcept;
};

class Device {
private:
    std::unique_ptr<detail::DeviceState> m_state;

    struct Candidate {
        PhysicalDevice gpu;
        int            score = -1;
        std::uint32_t  graphics = kQueueFamilyIgnored, present = kQueueFamilyIgnored;
        std::uint32_t  compute  = kQueueFamilyIgnored, transfer = kQueueFamilyIgnored;
    };

    static Candidate evaluate(const PhysicalDevice& gpu, const DeviceDesc& desc) {
        Candidate c;
        c.gpu = gpu;
        const auto props = gpu.properties();
        if (props.apiVersion < kApiVersion13) return c;
        const auto exts = gpu.extensions();
        if (desc.surface && !detail::contains_name(exts, VK_KHR_SWAPCHAIN_EXTENSION_NAME)) return c;
        for (const char* e : desc.extra_extensions) if (!detail::contains_name(exts, e)) return c;
        auto f13 = detail::make<VkPhysicalDeviceVulkan13Features>(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES);
        auto f12 = detail::make<VkPhysicalDeviceVulkan12Features>(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES);
        f12.pNext = &f13;
        auto f2 = detail::make<VkPhysicalDeviceFeatures2>(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2);
        f2.pNext = &f12;
        gpu.instance_state()->fn.vkGetPhysicalDeviceFeatures2(gpu.handle(), &f2);
        if (!f13.dynamicRendering || !f13.synchronization2 || !f12.timelineSemaphore) return c;
        const auto families = gpu.queue_families();

        for (const auto& q : families) {
            if (q.graphics && c.graphics == kQueueFamilyIgnored) c.graphics = q.index;
            if (q.compute && !q.graphics && c.compute == kQueueFamilyIgnored) c.compute = q.index;
            if (q.transfer && !q.graphics && !q.compute && c.transfer == kQueueFamilyIgnored) c.transfer = q.index;
        }

        if (c.graphics == kQueueFamilyIgnored) return c;

        if (desc.surface) {
            if (gpu.supports_present(*desc.surface, c.graphics)) {
                c.present = c.graphics;
            } else {
                for (const auto& q : families) if (gpu.supports_present(*desc.surface, q.index)) { c.present = q.index; break; }
            }
            if (c.present == kQueueFamilyIgnored) return c;
        } else {
            c.present = c.graphics;
        }

        if (c.compute  == kQueueFamilyIgnored) c.compute  = c.graphics;
        if (c.transfer == kQueueFamilyIgnored) c.transfer = c.graphics;
        const DeviceType type = static_cast<DeviceType>(props.deviceType);

        switch (type) {
            case DeviceType::Discrete:   c.score = 1000; break;
            case DeviceType::Integrated: c.score = 500;  break;
            case DeviceType::Virtual:    c.score = 200;  break;
            case DeviceType::Cpu:        c.score = 50;   break;
            default:                     c.score = 10;   break;
        }

        if (type == desc.preferred_type) c.score += 5000;
        return c;
    }

public:
    Device() noexcept = default;
    Device(Device&&) noexcept = default;
    Device& operator=(Device&&) noexcept = default;
    Device(const Device&) = delete;
    Device& operator=(const Device&) = delete;
    ~Device() noexcept = default;

    Result create(const Instance& instance, const DeviceDesc& desc = {}) {
        destroy();
        if (!instance.valid()) return Result::NotInitialized;
        Candidate best;

        if (desc.force_device.valid()) {
            best = evaluate(desc.force_device, desc);
        } else {
            for (const auto& gpu : instance.physical_devices()) {
                Candidate c = evaluate(gpu, desc);
                if (c.score > best.score) best = c;
            }
        }

        if (best.score < 0) return Result::NoSuitableDevice;
        auto state = std::make_unique<detail::DeviceState>();
        state->instance = instance.state();
        state->physical = best.gpu.handle();
        state->properties = best.gpu.properties();
        state->memory = best.gpu.memory_properties();
        std::vector<std::uint32_t> unique_families;

        for (std::uint32_t f : { best.graphics, best.present, best.compute, best.transfer }) {
            bool seen = false;
            for (std::uint32_t u : unique_families) seen |= (u == f);
            if (!seen) unique_families.push_back(f);
        }

        const float priority = 1.0f;
        std::vector<VkDeviceQueueCreateInfo> queue_infos;

        for (std::uint32_t f : unique_families) {
            auto qi = detail::make<VkDeviceQueueCreateInfo>(VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO);
            qi.queueFamilyIndex = f;
            qi.queueCount       = 1;
            qi.pQueuePriorities = &priority;
            queue_infos.push_back(qi);
        }

        auto s13 = detail::make<VkPhysicalDeviceVulkan13Features>(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES);
        auto s12 = detail::make<VkPhysicalDeviceVulkan12Features>(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES);
        s12.pNext = &s13;
        auto s2 = detail::make<VkPhysicalDeviceFeatures2>(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2);
        s2.pNext = &s12;
        instance.fn().vkGetPhysicalDeviceFeatures2(best.gpu.handle(), &s2);
        auto e13 = detail::make<VkPhysicalDeviceVulkan13Features>(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES);
        e13.dynamicRendering = VK_TRUE;
        e13.synchronization2 = VK_TRUE;
        e13.maintenance4     = s13.maintenance4;
        auto e12 = detail::make<VkPhysicalDeviceVulkan12Features>(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES);
        e12.pNext                = &e13;
        e12.timelineSemaphore    = VK_TRUE;
        e12.bufferDeviceAddress  = s12.bufferDeviceAddress;
        e12.scalarBlockLayout    = s12.scalarBlockLayout;
        e12.descriptorIndexing   = s12.descriptorIndexing;
        e12.runtimeDescriptorArray                    = s12.runtimeDescriptorArray;
        e12.descriptorBindingPartiallyBound           = s12.descriptorBindingPartiallyBound;
        e12.descriptorBindingVariableDescriptorCount  = s12.descriptorBindingVariableDescriptorCount;
        e12.shaderSampledImageArrayNonUniformIndexing = s12.shaderSampledImageArrayNonUniformIndexing;
        e12.descriptorBindingSampledImageUpdateAfterBind = s12.descriptorBindingSampledImageUpdateAfterBind;
        auto e2 = detail::make<VkPhysicalDeviceFeatures2>(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2);
        e2.pNext = &e12;
        e2.features.samplerAnisotropy = s2.features.samplerAnisotropy;
        e2.features.fillModeNonSolid  = s2.features.fillModeNonSolid;
        e2.features.wideLines         = s2.features.wideLines;
        auto& feat = state->features;
        feat.dynamic_rendering     = true;
        feat.synchronization2      = true;
        feat.timeline_semaphores   = true;
        feat.buffer_device_address = s12.bufferDeviceAddress == VK_TRUE;
        feat.descriptor_indexing   = s12.descriptorIndexing == VK_TRUE;
        feat.sampler_anisotropy    = s2.features.samplerAnisotropy == VK_TRUE;
        feat.fill_mode_non_solid   = s2.features.fillModeNonSolid == VK_TRUE;
        feat.wide_lines            = s2.features.wideLines == VK_TRUE;
        feat.swapchain             = desc.surface != nullptr;
        std::vector<const char*> extensions;
        if (desc.surface) extensions.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
        for (const char* e : desc.extra_extensions) extensions.push_back(e);
        auto info = detail::make<VkDeviceCreateInfo>(VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO);
        info.pNext                   = &e2;
        info.queueCreateInfoCount    = static_cast<std::uint32_t>(queue_infos.size());
        info.pQueueCreateInfos       = queue_infos.data();
        info.enabledExtensionCount   = static_cast<std::uint32_t>(extensions.size());
        info.ppEnabledExtensionNames = extensions.data();
        VkResult r = instance.fn().vkCreateDevice(best.gpu.handle(), &info, nullptr, &state->handle);
        if (r != VK_SUCCESS) { state->handle = VK_NULL_HANDLE; return to_result(r); }
        if (!state->fn.load(instance.fn().vkGetDeviceProcAddr, state->handle)) return Result::MissingEntryPoint;

        auto grab = [&](detail::QueueSlot& slot, std::uint32_t family) {
            slot.family = family;
            state->fn.vkGetDeviceQueue(state->handle, family, 0, &slot.handle);
        };

        grab(state->graphics, best.graphics);
        grab(state->present,  best.present);
        grab(state->compute,  best.compute);
        grab(state->transfer, best.transfer);
        auto pool_info = detail::make<VkCommandPoolCreateInfo>(VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO);
        pool_info.flags            = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        pool_info.queueFamilyIndex = best.graphics;
        r = state->fn.vkCreateCommandPool(state->handle, &pool_info, nullptr, &state->immediate_pool);
        if (r != VK_SUCCESS) return to_result(r);
        auto alloc = detail::make<VkCommandBufferAllocateInfo>(VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO);
        alloc.commandPool        = state->immediate_pool;
        alloc.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        alloc.commandBufferCount = 1;
        r = state->fn.vkAllocateCommandBuffers(state->handle, &alloc, &state->immediate_cmd);
        if (r != VK_SUCCESS) return to_result(r);
        auto fence_info = detail::make<VkFenceCreateInfo>(VK_STRUCTURE_TYPE_FENCE_CREATE_INFO);
        r = state->fn.vkCreateFence(state->handle, &fence_info, nullptr, &state->immediate_fence);
        if (r != VK_SUCCESS) return to_result(r);
        m_state = std::move(state);
        return Result::Success;
    }

    void destroy() noexcept { m_state.reset(); }
    bool valid() const noexcept { return m_state && m_state->handle; }
    explicit operator bool() const noexcept { return valid(); }

    native::Device             handle() const noexcept { return m_state ? m_state->handle : VK_NULL_HANDLE; }
    const DeviceDispatch&      fn()     const noexcept { return m_state->fn; }
    const detail::DeviceState* state()  const noexcept { return m_state.get(); }
    detail::DeviceState*       state()        noexcept { return m_state.get(); }

    PhysicalDevice physical_device() const noexcept { return PhysicalDevice(m_state->instance, m_state->physical); }
    const DeviceFeatures& features() const noexcept { return m_state->features; }
    const VkPhysicalDeviceLimits& limits() const noexcept { return m_state->properties.limits; }
    std::string name() const { return m_state->properties.deviceName; }

    Queue graphics_queue() const noexcept { return Queue(m_state.get(), m_state->graphics); }
    Queue present_queue()  const noexcept { return Queue(m_state.get(), m_state->present); }
    Queue compute_queue()  const noexcept { return Queue(m_state.get(), m_state->compute); }
    Queue transfer_queue() const noexcept { return Queue(m_state.get(), m_state->transfer); }

    Queue queue(QueueType type) const noexcept {
        switch (type) {
            case QueueType::Compute:  return compute_queue();
            case QueueType::Transfer: return transfer_queue();
            default:                  return graphics_queue();
        }
    }

    Result wait_idle() const noexcept {
        return m_state ? to_result(m_state->fn.vkDeviceWaitIdle(m_state->handle)) : Result::NotInitialized;
    }

    template <typename H>
    void set_debug_name(H handle, VkObjectType type, const char* name) const noexcept {
        if (m_state) m_state->set_name(type, reinterpret_cast<std::uint64_t>(handle), name);
    }
};

inline Result Fence::create(const Device& device, bool signaled) noexcept {
    destroy();
    if (!device.valid()) return Result::NotInitialized;
    auto info = detail::make<VkFenceCreateInfo>(VK_STRUCTURE_TYPE_FENCE_CREATE_INFO);
    info.flags = signaled ? VK_FENCE_CREATE_SIGNALED_BIT : 0;
    native::Fence h = VK_NULL_HANDLE;
    VkResult r = device.fn().vkCreateFence(device.handle(), &info, nullptr, &h);
    if (r != VK_SUCCESS) return to_result(r);
    m_fence = { device.state(), h };
    return Result::Success;
}

inline Result Semaphore::create(const Device& device) noexcept {
    destroy();
    if (!device.valid()) return Result::NotInitialized;
    auto info = detail::make<VkSemaphoreCreateInfo>(VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO);
    native::Semaphore h = VK_NULL_HANDLE;
    VkResult r = device.fn().vkCreateSemaphore(device.handle(), &info, nullptr, &h);
    if (r != VK_SUCCESS) return to_result(r);
    m_semaphore = { device.state(), h };
    m_timeline = false;
    return Result::Success;
}

inline Result Semaphore::create_timeline(const Device& device, std::uint64_t initial_value) noexcept {
    destroy();
    if (!device.valid()) return Result::NotInitialized;
    auto type = detail::make<VkSemaphoreTypeCreateInfo>(VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO);
    type.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE;
    type.initialValue  = initial_value;
    auto info = detail::make<VkSemaphoreCreateInfo>(VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO);
    info.pNext = &type;
    native::Semaphore h = VK_NULL_HANDLE;
    VkResult r = device.fn().vkCreateSemaphore(device.handle(), &info, nullptr, &h);
    if (r != VK_SUCCESS) return to_result(r);
    m_semaphore = { device.state(), h };
    m_timeline = true;
    return Result::Success;
}

} // namespace vulkan
} // namespace fizmo

#endif // FIZMO_VULKAN_DEVICE_HPP