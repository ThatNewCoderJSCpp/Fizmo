#ifndef FIZMO_GPU_VULKAN_BACKEND_HPP
#define FIZMO_GPU_VULKAN_BACKEND_HPP

#include "backend.hpp"

#if defined(OS_WINDOWS) || defined(OS_LINUX)

#include "../Vulkan/include.hpp"
#include "vulkan_present.hpp"
#include <cstdio>
#include <cstring>
#include <map>

namespace fizmo {
namespace gpu {
namespace detail {
namespace vkb {

namespace vk = ::fizmo::vulkan;

inline vk::Format format(Format f) noexcept {
    switch (f) {
        case Format::R8Unorm:        return vk::Format::R8Unorm;
        case Format::RGBA8Unorm:     return vk::Format::RGBA8Unorm;
        case Format::RGBA8Srgb:      return vk::Format::RGBA8Srgb;
        case Format::BGRA8Unorm:     return vk::Format::BGRA8Unorm;
        case Format::R16Float:       return vk::Format::R16Float;
        case Format::RGBA16Float:    return vk::Format::RGBA16Float;
        case Format::R32Uint:        return vk::Format::R32Uint;
        case Format::R32Float:       return vk::Format::R32Float;
        case Format::RG32Float:      return vk::Format::RG32Float;
        case Format::RGB32Float:     return vk::Format::RGB32Float;
        case Format::RGBA32Float:    return vk::Format::RGBA32Float;
        case Format::RGB32Sint:      return vk::Format::RGB32Sint;
        case Format::D16Unorm:       return vk::Format::D16Unorm;
        case Format::D24UnormS8Uint: return vk::Format::D24UnormS8Uint;
        case Format::D32Float:       return vk::Format::D32Float;
        default:                     return vk::Format::Undefined;
    }
}

inline vk::ShaderStage stages(ShaderStage s) noexcept {
    vk::ShaderStage out = vk::ShaderStage::None;
    if (any(s, ShaderStage::Vertex))   out |= vk::ShaderStage::Vertex;
    if (any(s, ShaderStage::Fragment)) out |= vk::ShaderStage::Fragment;
    if (any(s, ShaderStage::Compute))  out |= vk::ShaderStage::Compute;
    return out;
}

inline vk::CompareOp compare_op(CompareOp op) noexcept {
    switch (op) {
        case CompareOp::Never:        return vk::CompareOp::Never;
        case CompareOp::Less:         return vk::CompareOp::Less;
        case CompareOp::Equal:        return vk::CompareOp::Equal;
        case CompareOp::LessEqual:    return vk::CompareOp::LessEqual;
        case CompareOp::Greater:      return vk::CompareOp::Greater;
        case CompareOp::NotEqual:     return vk::CompareOp::NotEqual;
        case CompareOp::GreaterEqual: return vk::CompareOp::GreaterEqual;
        case CompareOp::Always:       return vk::CompareOp::Always;
    }
    return vk::CompareOp::Always;
}

inline vk::BlendFactor blend_factor(BlendFactor f) noexcept {
    switch (f) {
        case BlendFactor::Zero:             return vk::BlendFactor::Zero;
        case BlendFactor::One:              return vk::BlendFactor::One;
        case BlendFactor::SrcColor:         return vk::BlendFactor::SrcColor;
        case BlendFactor::OneMinusSrcColor: return vk::BlendFactor::OneMinusSrcColor;
        case BlendFactor::DstColor:         return vk::BlendFactor::DstColor;
        case BlendFactor::OneMinusDstColor: return vk::BlendFactor::OneMinusDstColor;
        case BlendFactor::SrcAlpha:         return vk::BlendFactor::SrcAlpha;
        case BlendFactor::OneMinusSrcAlpha: return vk::BlendFactor::OneMinusSrcAlpha;
        case BlendFactor::DstAlpha:         return vk::BlendFactor::DstAlpha;
        case BlendFactor::OneMinusDstAlpha: return vk::BlendFactor::OneMinusDstAlpha;
    }
    return vk::BlendFactor::One;
}

inline vk::AddressMode address_mode(AddressMode m) noexcept {
    return m == AddressMode::Repeat ? vk::AddressMode::Repeat : m == AddressMode::MirroredRepeat ? vk::AddressMode::MirroredRepeat : vk::AddressMode::ClampToEdge;
}

inline std::string text(vk::Result r) { return std::string(vk::to_string(r)); }

inline vk::Filter filter(Filter f) noexcept { return f == Filter::Linear ? vk::Filter::Linear : vk::Filter::Nearest; }

inline Result result(vk::Result r) noexcept {
    if (vk::succeeded(r)) return Result::Success;
    switch (r) {
        case vk::Result::OutOfHostMemory:
        case vk::Result::OutOfDeviceMemory: return Result::OutOfMemory;
        case vk::Result::DeviceLost:        return Result::DeviceLost;
        case vk::Result::InvalidArgument:   return Result::InvalidArgument;
        case vk::Result::FormatNotSupported:
        case vk::Result::FeatureNotPresent:
        case vk::Result::ExtensionNotPresent: return Result::Unsupported;
        default:                            return Result::BackendFailed;
    }
}

struct Retired {
    virtual ~Retired() = default;
};

template <typename T>
struct Kept final : Retired {
    T value;
    explicit Kept(T&& v) : value(std::move(v)) {}
};

struct FreedSet {
    const vk::detail::DeviceState* device = nullptr;
    VkDescriptorPool               pool   = VK_NULL_HANDLE;
    VkDescriptorSet                set    = VK_NULL_HANDLE;

    FreedSet() noexcept = default;
    FreedSet(const vk::detail::DeviceState* d, VkDescriptorPool p, VkDescriptorSet s) noexcept : device(d), pool(p), set(s) {}
    FreedSet(FreedSet&& o) noexcept : device(o.device), pool(o.pool), set(std::exchange(o.set, VK_NULL_HANDLE)) {}
    FreedSet& operator=(FreedSet&&) = delete;
    ~FreedSet() { if (set && device) device->fn.vkFreeDescriptorSets(device->handle, pool, 1, &set); }
};

class Core {
public:
    struct Slot {
        vk::CommandPool                  pool;
        vk::CommandBuffer                cmd;
        vk::Fence                        fence;
        vk::Semaphore                    acquired;
        std::uint64_t                    serial = 0;
        std::vector<vk::DescriptorPool>  pools;
        std::size_t                      cursor = 0;
    };

    vk::Instance                                               instance;
    vk::Surface                                                surface;
    vk::Device                                                 device;
    vk::Swapchain                                              swapchain;
    std::vector<Slot>                                          slots;
    std::vector<vk::DescriptorPool>                            persistent;
    std::vector<std::pair<std::uint64_t, std::unique_ptr<Retired>>> retired;
    std::uint64_t                                              serial      = 1;
    std::uint64_t                                              submitted   = 0;
    std::uint64_t                                              completed   = 0;
    bool                                                       in_frame    = false;
    Slot*                                                      frame       = nullptr;
    std::uint32_t                                              image_index = 0;
    vk::ImageLayout                                            image_layout = vk::ImageLayout::Undefined;
    bool                                                       presented   = false;
    bool                                                       needs_resize = false;
    Extent2D                                                   pending;
    bool                                                       vsync       = true;
    bool                                                       blit_present = false;
    Caps                                                       caps;
    std::string                                                error;
    vk::ShaderModule                                           present_vs;
    vk::ShaderModule                                           present_fs;
    vk::DescriptorSetLayout                                    present_set;
    vk::PipelineLayout                                         present_layout;
    vk::Pipeline                                               present_pipeline;
    vk::Format                                                 present_format = vk::Format::Undefined;
    vk::Sampler                                                present_samplers[2];

    ~Core() {
        if (device.valid()) device.wait_idle();
        completed = submitted;
        retired.clear();
        present_pipeline.destroy();
        present_layout.destroy();
        present_set.destroy();
        present_fs.destroy();
        present_vs.destroy();
        for (auto& s : present_samplers) s.destroy();
        persistent.clear();
        slots.clear();
        swapchain.destroy();
        device.destroy();
        surface.destroy();
        instance.destroy();
    }

    const vk::DeviceDispatch& fn() const noexcept { return device.fn(); }

    std::uint64_t tag() const noexcept { return in_frame ? serial : submitted; }

    template <typename T>
    void retire(T&& value) {
        const std::uint64_t t = tag();
        if (t <= completed) return;
        retired.emplace_back(t, std::unique_ptr<Retired>(new Kept<std::decay_t<T>>(std::move(value))));
    }

    void collect() {
        retired.erase(std::remove_if(retired.begin(), retired.end(), [this](const auto& r) { return r.first <= completed; }), retired.end());
    }

    static vk::DescriptorPoolDesc pool_desc(bool frame) {
        vk::DescriptorPoolDesc d;
        d.max_sets = frame ? 1024 : 256;
        const std::uint32_t n = d.max_sets;
        d.sizes = {
            { vk::DescriptorType::CombinedImageSampler, n * 8 },
            { vk::DescriptorType::UniformBuffer,        n * 2 },
            { vk::DescriptorType::StorageImage,         n },
            { vk::DescriptorType::StorageBuffer,        n },
        };
        d.free_individual_sets = !frame;
        return d;
    }

    vk::Result allocate(const vk::DescriptorSetLayout& layout, bool frame_lifetime, vk::DescriptorSet& out, VkDescriptorPool& pool) {
        std::vector<vk::DescriptorPool>& pools = frame_lifetime && frame ? frame->pools : persistent;
        std::size_t start = frame_lifetime && frame ? frame->cursor : 0;

        for (std::size_t i = start; i < pools.size(); ++i) {
            if (vk::succeeded(pools[i].allocate(layout, out))) {
                pool = pools[i].handle();
                if (frame_lifetime && frame) frame->cursor = i;
                return vk::Result::Success;
            }
        }

        vk::DescriptorPool fresh;
        const vk::Result r = fresh.create(device, pool_desc(frame_lifetime && frame));
        if (vk::failed(r)) return r;
        pools.push_back(std::move(fresh));
        if (frame_lifetime && frame) frame->cursor = pools.size() - 1;
        pool = pools.back().handle();
        return pools.back().allocate(layout, out);
    }
};

struct Buffer final : BufferImpl {
    std::shared_ptr<Core> core;
    vk::Buffer            buffer;

    ~Buffer() override { if (core) core->retire(std::move(buffer)); }
    Result write(const void* data, std::uint64_t bytes, std::uint64_t offset) override;
    Result read(void* out, std::uint64_t bytes, std::uint64_t offset) override;
};

struct TextureOwned {
    vk::Image                              image;
    std::map<std::uint32_t, vk::ImageView> views;
    vk::ImageView                          sample;
};

struct Texture final : TextureImpl {
    std::shared_ptr<Core> core;
    TextureOwned          own;
    bool                  custom_sample = false;

    ~Texture() override { if (core) core->retire(std::move(own)); }

    const vk::ImageView& sampled() const noexcept { return custom_sample ? own.sample : own.image.view(); }

    const vk::ImageView* attachment(std::uint32_t layer, std::uint32_t mip) {
        if (desc.dimension == TextureDimension::D2 && desc.mip_levels == 1 && layer == 0 && mip == 0) return &own.image.view();
        const std::uint32_t key = (layer << 8) | mip;
        auto it = own.views.find(key);
        if (it != own.views.end()) return &it->second;
        vk::ImageViewDesc d;
        d.base_layer  = layer;
        d.layer_count = 1;
        d.base_mip    = mip;
        d.mip_count   = 1;
        vk::ImageView v;
        if (vk::failed(v.create(core->device, own.image, d))) return nullptr;
        return &own.views.emplace(key, std::move(v)).first->second;
    }

    vk::ImageAspect aspect() const noexcept { return vk::aspect_of(own.image.format()); }
};

struct Sampler final : SamplerImpl {
    std::shared_ptr<Core> core;
    vk::Sampler           sampler;

    ~Sampler() override { if (core) core->retire(std::move(sampler)); }
};

struct Shader final : ShaderImpl {
    vk::ShaderModule module;
};

struct BindGroupLayout final : BindGroupLayoutImpl {
    vk::DescriptorSetLayout layout;
};

struct PipelineLayout final : PipelineLayoutImpl {
    std::shared_ptr<Core> core;
    vk::PipelineLayout    layout;
    vk::ShaderStage       push = vk::ShaderStage::None;

    ~PipelineLayout() override { if (core) core->retire(std::move(layout)); }
};

struct BindGroup final : BindGroupImpl {
    std::shared_ptr<Core> core;
    vk::DescriptorSet     set;
    VkDescriptorPool      pool = VK_NULL_HANDLE;

    ~BindGroup() override {
        if (core && lifetime == BindGroupLifetime::Persistent && set.valid()) core->retire(FreedSet(core->device.state(), pool, set.handle()));
    }
};

struct RenderPipeline final : RenderPipelineImpl {
    std::shared_ptr<Core> core;
    vk::Pipeline          pipeline;

    ~RenderPipeline() override { if (core) core->retire(std::move(pipeline)); }
};

struct ComputePipeline final : ComputePipelineImpl {
    std::shared_ptr<Core> core;
    vk::Pipeline          pipeline;

    ~ComputePipeline() override { if (core) core->retire(std::move(pipeline)); }
};

struct QuerySet final : QuerySetImpl {
    std::shared_ptr<Core> core;
    vk::TimestampPool     pool;

    ~QuerySet() override { if (core) core->retire(std::move(pool)); }
};

class Barriers {
private:
    std::vector<VkImageMemoryBarrier2> m_images;

public:
    void add(Texture& t, vk::ImageLayout to) {
        const vk::ImageLayout from = t.own.image.layout();
        if (from == to) return;
        for (const auto& b : m_images) if (b.image == t.own.image.handle()) return;
        auto ib = vk::detail::make<VkImageMemoryBarrier2>(VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2);
        ib.srcStageMask        = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
        ib.srcAccessMask       = VK_ACCESS_2_MEMORY_WRITE_BIT;
        ib.dstStageMask        = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
        ib.dstAccessMask       = VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT;
        ib.oldLayout           = vk::to_vk(from);
        ib.newLayout           = vk::to_vk(to);
        ib.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        ib.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        ib.image               = t.own.image.handle();
        ib.subresourceRange    = { vk::to_vk(t.aspect()), 0, VK_REMAINING_MIP_LEVELS, 0, VK_REMAINING_ARRAY_LAYERS };
        m_images.push_back(ib);
        t.own.image.set_layout(to);
    }

    void add_raw(VkImage image, vk::ImageLayout from, vk::ImageLayout to) {
        auto ib = vk::detail::make<VkImageMemoryBarrier2>(VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2);
        ib.srcStageMask        = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
        ib.srcAccessMask       = VK_ACCESS_2_MEMORY_WRITE_BIT;
        ib.dstStageMask        = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
        ib.dstAccessMask       = VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT;
        ib.oldLayout           = vk::to_vk(from);
        ib.newLayout           = vk::to_vk(to);
        ib.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        ib.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        ib.image               = image;
        ib.subresourceRange    = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
        m_images.push_back(ib);
    }

    void flush(const vk::CommandBuffer& cmd, bool memory) {
        if (m_images.empty() && !memory) return;
        auto mb = vk::detail::make<VkMemoryBarrier2>(VK_STRUCTURE_TYPE_MEMORY_BARRIER_2);
        mb.srcStageMask  = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
        mb.srcAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT;
        mb.dstStageMask  = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
        mb.dstAccessMask = VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT;
        auto dep = vk::detail::make<VkDependencyInfo>(VK_STRUCTURE_TYPE_DEPENDENCY_INFO);
        dep.memoryBarrierCount      = memory ? 1u : 0u;
        dep.pMemoryBarriers         = &mb;
        dep.imageMemoryBarrierCount = static_cast<std::uint32_t>(m_images.size());
        dep.pImageMemoryBarriers    = m_images.data();
        cmd.device_state()->fn.vkCmdPipelineBarrier2(cmd.handle(), &dep);
        m_images.clear();
    }
};

inline Texture* vk_texture(TextureImpl* t) noexcept { return static_cast<Texture*>(t); }
inline Buffer*  vk_buffer(BufferImpl* b)   noexcept { return static_cast<Buffer*>(b); }

inline VkBufferImageCopy image_copy_region(const Texture& t, const TextureRegion& r, std::uint64_t offset, std::uint32_t row_pixels) noexcept {
    VkBufferImageCopy c{};
    c.bufferOffset      = offset;
    c.bufferRowLength   = row_pixels;
    c.bufferImageHeight = 0;
    const bool depth = is_depth(t.desc.format);
    c.imageSubresource  = { static_cast<VkImageAspectFlags>(depth ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT), r.mip, t.desc.dimension == TextureDimension::D3 ? 0u : r.layer, 1 };
    c.imageOffset       = { r.origin.x, r.origin.y, t.desc.dimension == TextureDimension::D3 ? r.origin.z : 0 };
    c.imageExtent       = { r.extent.width, r.extent.height, t.desc.dimension == TextureDimension::D3 ? std::max(r.extent.depth, 1u) : 1u };
    return c;
}

inline Result staged_buffer_write(Core& core, vk::Buffer& dst, const void* data, std::uint64_t bytes, std::uint64_t offset);

inline Result Buffer::write(const void* data, std::uint64_t bytes, std::uint64_t offset) {
    if (buffer.is_mapped()) return result(buffer.write(data, bytes, offset));
    return staged_buffer_write(*core, buffer, data, bytes, offset);
}

inline Result Buffer::read(void* out, std::uint64_t bytes, std::uint64_t offset) {
    if (buffer.is_mapped()) return result(buffer.read(out, bytes, offset));
    if (core->in_frame) return Result::InvalidArgument;
    core->device.wait_idle();
    return result(vk::download(core->device, buffer, out, bytes, offset));
}

inline Result staged_buffer_write(Core& core, vk::Buffer& dst, const void* data, std::uint64_t bytes, std::uint64_t offset) {
    vk::Buffer staging;
    vk::Result r = staging.create(core.device, { bytes, vk::BufferUsage::TransferSrc, vk::MemoryUsage::CpuToGpu, "fizmo gpu staging" });
    if (vk::failed(r)) return result(r);
    r = staging.write(data, bytes);
    if (vk::failed(r)) return result(r);

    if (core.in_frame && core.frame) {
        const vk::CommandBuffer& cmd = core.frame->cmd;
        Barriers b;
        b.flush(cmd, true);
        cmd.copy_buffer(staging, dst, bytes, 0, offset);
        core.retire(std::move(staging));
        return Result::Success;
    }

    return result(vk::submit_immediate(core.device, [&](vk::CommandBuffer& cmd) {
        Barriers b;
        b.flush(cmd, true);
        cmd.copy_buffer(staging, dst, bytes, 0, offset);
    }));
}

class Device final : public BackendDevice {
private:
    static constexpr std::uint32_t kMaxGroups = 4;

    std::shared_ptr<Core> m_core;

    Result fail(Result r, const std::string& message) {
        m_core->error = message;
        return r;
    }

    vk::SwapchainDesc swapchain_desc(Extent2D size) const noexcept {
        vk::SwapchainDesc d;
        d.size  = { std::max(size.width, 1u), std::max(size.height, 1u) };
        d.vsync = m_core->vsync;
        d.srgb  = false;
        d.usage = vk::ImageUsage::ColorAttachment | vk::ImageUsage::TransferDst;
        return d;
    }

    void decide_present_path() {
        const vk::PhysicalDevice gpu = m_core->device.physical_device();
        const VkSurfaceCapabilitiesKHR sc = m_core->surface.capabilities(gpu);
        const bool transfer = (sc.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_DST_BIT) != 0;
        const bool dst = m_core->swapchain.valid() && gpu.supports_format(m_core->swapchain.format(), VK_FORMAT_FEATURE_BLIT_DST_BIT);
        const bool src = gpu.supports_format(vk::Format::RGBA8Unorm, VK_FORMAT_FEATURE_BLIT_SRC_BIT | VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT);
        m_core->blit_present = transfer && dst && src;
    }

public:
    Result create(const DeviceInput& in) {
        m_core = std::make_shared<Core>();
        Core& c = *m_core;
        vk::InstanceDesc id;
        id.app_name = in.app_name ? in.app_name : "fizmo";
        id.enable_validation = in.debug;
        vk::Result r = c.instance.create(id);
        if (vk::failed(r)) return fail(result(r), std::string("vulkan instance: ") + text(r));
        r = c.surface.create(c.instance, in.window);
        if (vk::failed(r)) return fail(result(r), std::string("vulkan surface: ") + text(r));
        vk::DeviceDesc dd;
        dd.surface = &c.surface;
        r = c.device.create(c.instance, dd);
        if (vk::failed(r)) return fail(result(r), std::string("vulkan device: ") + text(r));
        c.vsync = in.vsync;
        c.pending = in.size;
        r = c.swapchain.create(c.device, c.surface, swapchain_desc(in.size));
        if (r == vk::Result::NotReady) c.needs_resize = true;
        else if (vk::failed(r)) return fail(result(r), std::string("vulkan swapchain: ") + text(r));
        c.slots.resize(std::max(in.frames_in_flight, 1u));

        for (Core::Slot& s : c.slots) {
            if (vk::failed(s.pool.create(c.device, vk::QueueType::Graphics, true))) return fail(Result::BackendFailed, "vulkan command pool");
            if (vk::failed(s.pool.allocate(s.cmd))) return fail(Result::BackendFailed, "vulkan command buffer");
            if (vk::failed(s.fence.create(c.device, true))) return fail(Result::BackendFailed, "vulkan fence");
            if (vk::failed(s.acquired.create(c.device))) return fail(Result::BackendFailed, "vulkan semaphore");
        }

        const VkPhysicalDeviceLimits& l = c.device.limits();
        const vk::DeviceFeatures& f = c.device.features();
        Caps& caps = c.caps;
        caps.backend             = Backend::Vulkan;
        caps.device_name         = c.device.name();
        describe_adapter(caps);
        const std::uint32_t api  = c.device.state()->properties.apiVersion;
        caps.api_version         = "Vulkan " + std::to_string(VK_API_VERSION_MAJOR(api)) + "." + std::to_string(VK_API_VERSION_MINOR(api)) + "." + std::to_string(VK_API_VERSION_PATCH(api));
        const VkSampleCountFlags counts = l.framebufferColorSampleCounts & l.framebufferDepthSampleCounts;
        caps.max_samples = 1;
        for (std::uint32_t s = 64; s > 1; s >>= 1) if (counts & s) { caps.max_samples = s; break; }
        caps.max_texture_size    = l.maxImageDimension2D;
        caps.max_textures        = std::min<std::uint32_t>(l.maxPerStageDescriptorSamplers, 1024);
        caps.uniform_alignment   = std::max<std::uint64_t>(l.minUniformBufferOffsetAlignment, 1);
        caps.max_uniform_range   = l.maxUniformBufferRange;
        caps.max_push_constants  = std::min<std::uint32_t>(l.maxPushConstantsSize, 256);
        caps.compute             = true;
        caps.storage_buffers     = true;
        caps.multi_draw_indirect = f.multi_draw_indirect && f.draw_indirect_first_instance;
        caps.base_instance       = true;
        caps.depth_clamp         = f.depth_clamp;
        caps.timestamps          = vk::TimestampPool::supported(c.device);
        caps.debug_labels        = c.instance.has_debug_utils();
        decide_present_path();
        return Result::Success;
    }

    void describe_adapter(Caps& caps) const {
        const vk::detail::DeviceState* st = m_core->device.state();
        const VkPhysicalDeviceProperties& p = st->properties;
        caps.vendor_id = p.vendorID;
        caps.device_id = p.deviceID;

        switch (p.deviceType) {
            case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU: caps.adapter_type = AdapterType::Integrated; break;
            case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:   caps.adapter_type = AdapterType::Discrete;   break;
            case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU:    caps.adapter_type = AdapterType::Virtual;    break;
            case VK_PHYSICAL_DEVICE_TYPE_CPU:            caps.adapter_type = AdapterType::Cpu;        break;
            default:                                     caps.adapter_type = AdapterType::Unknown;    break;
        }

        const auto get2 = st->instance ? st->instance->fn.vkGetPhysicalDeviceProperties2 : nullptr;
        if (!get2) return;
        const bool pci = m_core->device.physical_device().supports_extension(VK_EXT_PCI_BUS_INFO_EXTENSION_NAME);

        VkPhysicalDevicePCIBusInfoPropertiesEXT bus{};
        bus.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PCI_BUS_INFO_PROPERTIES_EXT;
        VkPhysicalDeviceIDProperties id{};
        id.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ID_PROPERTIES;
        id.pNext = pci ? &bus : nullptr;
        VkPhysicalDeviceProperties2 props{};
        props.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
        props.pNext = &id;
        get2(st->physical, &props);

        if (id.deviceLUIDValid) {
            caps.luid_valid = true;
            std::memcpy(&caps.luid, id.deviceLUID, sizeof(caps.luid));
        }

        if (pci) {
            char text[32];
            std::snprintf(text, sizeof(text), "%04x:%02x:%02x.%x", bus.pciDomain, bus.pciBus, bus.pciDevice, bus.pciFunction);
            caps.pci_bus = text;
        }
    }

    ~Device() override {
        if (m_core && m_core->device.valid()) {
            m_core->device.wait_idle();
            m_core->completed = m_core->submitted;
            m_core->collect();
        }
    }

    Backend            backend()          const noexcept override { return Backend::Vulkan; }
    const Caps&        caps()             const noexcept override { return m_core->caps; }
    std::uint32_t      frames_in_flight() const noexcept override { return static_cast<std::uint32_t>(m_core->slots.size()); }
    const std::string& last_error()       const noexcept override { return m_core->error; }
    bool               in_frame()         const noexcept override { return m_core->in_frame; }

    MemoryStats memory_stats() const noexcept override {
        MemoryStats m;
        if (!m_core->device.valid()) return m;
        const vk::MemoryReport r = m_core->device.memory_report();
        m.valid        = true;
        m.measured     = r.measured;
        m.used         = r.device_used;
        m.budget       = r.device_budget;
        m.total        = r.device_total;
        m.shared_used  = r.shared_used;
        m.shared_total = r.shared_total;
        return m;
    }

    bool supports(Format f, TextureUsage usage, std::uint32_t samples) const noexcept override {
        const vk::Format vf = format(f);
        if (vf == vk::Format::Undefined) return false;
        VkFormatFeatureFlags need = 0;
        if (any(usage, TextureUsage::Sampled))      need |= VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT;
        if (any(usage, TextureUsage::RenderTarget)) need |= VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT;
        if (any(usage, TextureUsage::DepthStencil)) need |= VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT;
        if (any(usage, TextureUsage::Storage))      need |= VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT;
        if (any(usage, TextureUsage::CopySrc))      need |= VK_FORMAT_FEATURE_TRANSFER_SRC_BIT;
        if (any(usage, TextureUsage::CopyDst))      need |= VK_FORMAT_FEATURE_TRANSFER_DST_BIT;
        if (!m_core->device.physical_device().supports_format(vf, need)) return false;
        if (samples <= 1) return true;
        const VkPhysicalDeviceLimits& l = m_core->device.limits();
        const VkSampleCountFlags counts = is_depth(f) ? l.framebufferDepthSampleCounts : l.framebufferColorSampleCounts;
        return (counts & samples) != 0;
    }

    bool supports_linear_filter(Format f) const noexcept override {
        const vk::Format vf = format(f);
        return vf != vk::Format::Undefined && m_core->device.physical_device().supports_format(vf, VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT);
    }

    Result create_buffer(const BufferDesc& desc, std::shared_ptr<BufferImpl>& out) override {
        auto b = std::make_shared<Buffer>();
        b->core = m_core;
        b->desc = desc;
        vk::BufferUsage u = vk::BufferUsage::TransferSrc | vk::BufferUsage::TransferDst;
        if (any(desc.usage, BufferUsage::Vertex))   u |= vk::BufferUsage::Vertex;
        if (any(desc.usage, BufferUsage::Index))    u |= vk::BufferUsage::Index;
        if (any(desc.usage, BufferUsage::Uniform))  u |= vk::BufferUsage::Uniform;
        if (any(desc.usage, BufferUsage::Storage))  u |= vk::BufferUsage::Storage;
        if (any(desc.usage, BufferUsage::Indirect)) u |= vk::BufferUsage::Indirect;
        const vk::MemoryUsage mem = desc.memory == MemoryAccess::Upload ? vk::MemoryUsage::CpuToGpu : desc.memory == MemoryAccess::Readback ? vk::MemoryUsage::GpuToCpu : vk::MemoryUsage::GpuOnly;
        const vk::Result r = b->buffer.create(m_core->device, { desc.size, u, mem, desc.name });
        if (vk::failed(r)) return result(r);
        out = std::move(b);
        return Result::Success;
    }

    Result create_texture(const TextureDesc& desc, std::shared_ptr<TextureImpl>& out) override {
        const vk::Format vf = format(desc.format);
        if (vf == vk::Format::Undefined) return Result::Unsupported;
        auto t = std::make_shared<Texture>();
        t->core = m_core;
        t->desc = desc;
        vk::ImageDesc d;
        d.extent     = { desc.extent.width, desc.extent.height, desc.dimension == TextureDimension::D3 ? desc.extent.depth : 1u };
        d.format     = vf;
        d.type       = desc.dimension == TextureDimension::Cube ? vk::ImageType::Cube : desc.dimension == TextureDimension::D3 ? vk::ImageType::Image3D : vk::ImageType::Image2D;
        d.mip_levels = desc.mip_levels;
        d.samples    = static_cast<vk::SampleCount>(desc.samples);
        d.name       = desc.name;
        vk::ImageUsage u = desc.samples > 1 ? vk::ImageUsage::None : vk::ImageUsage::TransferSrc | vk::ImageUsage::TransferDst;
        if (any(desc.usage, TextureUsage::Sampled))      u |= vk::ImageUsage::Sampled;
        if (any(desc.usage, TextureUsage::RenderTarget)) u |= vk::ImageUsage::ColorAttachment;
        if (any(desc.usage, TextureUsage::DepthStencil)) u |= vk::ImageUsage::DepthAttachment;
        if (any(desc.usage, TextureUsage::Storage))      u |= vk::ImageUsage::Storage;
        if (any(desc.usage, TextureUsage::CopySrc))      u |= vk::ImageUsage::TransferSrc;
        if (any(desc.usage, TextureUsage::CopyDst))      u |= vk::ImageUsage::TransferDst;
        d.usage = u;
        const vk::Result r = t->own.image.create(m_core->device, d);
        if (vk::failed(r)) return fail(result(r), std::string("texture creation failed: ") + text(r));

        if (desc.dimension == TextureDimension::Cube || (has_stencil(desc.format) && any(desc.usage, TextureUsage::Sampled))) {
            vk::ImageViewDesc vd;
            vd.as_cube = desc.dimension == TextureDimension::Cube;
            if (is_depth(desc.format)) vd.aspect = vk::ImageAspect::Depth;
            if (vk::failed(t->own.sample.create(m_core->device, t->own.image, vd))) return Result::BackendFailed;
            t->custom_sample = true;
        }

        out = std::move(t);
        return Result::Success;
    }

    Result create_sampler(const SamplerDesc& desc, std::shared_ptr<SamplerImpl>& out) override {
        auto s = std::make_shared<Sampler>();
        s->core = m_core;
        s->desc = desc;
        vk::SamplerDesc d;
        d.mag_filter     = filter(desc.mag_filter);
        d.min_filter     = filter(desc.min_filter);
        d.mipmap         = desc.mip_filter == Filter::Linear ? vk::MipmapMode::Linear : vk::MipmapMode::Nearest;
        d.address_u      = address_mode(desc.address_u);
        d.address_v      = address_mode(desc.address_v);
        d.address_w      = address_mode(desc.address_w);
        d.compare_enable = desc.compare;
        d.compare_op     = compare_op(desc.compare_op);
        d.max_lod        = desc.max_lod;
        d.max_anisotropy = desc.max_anisotropy;
        d.name           = desc.name;
        const vk::Result r = s->sampler.create(m_core->device, d);
        if (vk::failed(r)) return result(r);
        out = std::move(s);
        return Result::Success;
    }

    Result create_shader(const ShaderDesc& desc, std::shared_ptr<ShaderImpl>& out) override {
        if (desc.spirv.empty()) return fail(Result::Unsupported, std::string(desc.name ? desc.name : "shader") + ": no SPIR-V for the Vulkan backend");
        auto s = std::make_shared<Shader>();
        s->stage = desc.stage;
        const vk::Result r = s->module.create(m_core->device, vk::Span<std::uint32_t>(desc.spirv.data(), desc.spirv.size()), desc.name);
        if (vk::failed(r)) return fail(Result::ShaderFailed, std::string(desc.name ? desc.name : "shader") + ": " + text(r));
        out = std::move(s);
        return Result::Success;
    }

    Result create_bind_group_layout(const BindGroupLayoutDesc& desc, std::shared_ptr<BindGroupLayoutImpl>& out) override {
        auto l = std::make_shared<BindGroupLayout>();
        std::vector<vk::DescriptorBinding> bindings;

        for (const BindGroupLayoutEntry& e : desc.entries) {
            if (e.count == 0 || l->find(e.binding)) return Result::InvalidArgument;
            l->entries.push_back(e);
            vk::DescriptorType type = vk::DescriptorType::CombinedImageSampler;
            if (e.type == BindingType::UniformBuffer)  type = vk::DescriptorType::UniformBuffer;
            if (e.type == BindingType::StorageBuffer)  type = vk::DescriptorType::StorageBuffer;
            if (e.type == BindingType::StorageTexture) type = vk::DescriptorType::StorageImage;
            bindings.push_back({ e.binding, type, stages(e.stages), e.count });
        }

        std::sort(l->entries.begin(), l->entries.end(), [](const BindGroupLayoutEntry& a, const BindGroupLayoutEntry& b) { return a.binding < b.binding; });
        const vk::Result r = l->layout.create(m_core->device, bindings);
        if (vk::failed(r)) return result(r);
        out = std::move(l);
        return Result::Success;
    }

    Result create_pipeline_layout(std::vector<std::shared_ptr<BindGroupLayoutImpl>> groups, std::uint32_t push_size, ShaderStage push_stages, const char*, std::shared_ptr<PipelineLayoutImpl>& out) override {
        auto l = std::make_shared<PipelineLayout>();
        l->core        = m_core;
        l->groups      = std::move(groups);
        l->push_size   = push_size;
        l->push_stages = push_stages;
        l->push        = stages(push_stages);
        vk::PipelineLayoutDesc d;
        for (const auto& g : l->groups) d.set_layouts.push_back(&static_cast<BindGroupLayout*>(g.get())->layout);
        if (push_size) d.push_constants = { { l->push, 0, push_size } };
        const vk::Result r = l->layout.create(m_core->device, d);
        if (vk::failed(r)) return result(r);
        out = std::move(l);
        return Result::Success;
    }

    Result create_bind_group(const std::shared_ptr<BindGroupLayoutImpl>& layout, Span<BindingInput> entries, BindGroupLifetime lifetime, const char*, std::shared_ptr<BindGroupImpl>& out) override {
        auto g = std::make_shared<BindGroup>();
        g->core     = m_core;
        g->layout   = layout;
        g->lifetime = lifetime == BindGroupLifetime::Frame && m_core->in_frame ? BindGroupLifetime::Frame : BindGroupLifetime::Persistent;
        const auto& set_layout = static_cast<BindGroupLayout*>(layout.get())->layout;
        const vk::Result r = m_core->allocate(set_layout, g->lifetime == BindGroupLifetime::Frame, g->set, g->pool);
        if (vk::failed(r)) return fail(result(r), "descriptor allocation failed");
        vk::DescriptorWriter w;
        g->entries.reserve(entries.size());

        for (const BindingInput& e : entries) {
            g->entries.push_back({ e.binding, e.element, e.buffer, e.offset, e.size, e.texture, e.sampler });
            const BindGroupLayoutEntry* le = layout->find(e.binding);

            switch (le->type) {
                case BindingType::SampledTexture:
                    w.image(e.binding, vk_texture(e.texture)->sampled(), static_cast<Sampler*>(e.sampler)->sampler, vk::ImageLayout::ShaderReadOnly, vk::DescriptorType::CombinedImageSampler, e.element);
                    break;
                case BindingType::StorageTexture:
                    w.storage_image(e.binding, vk_texture(e.texture)->own.image.view(), vk::ImageLayout::General, e.element);
                    break;
                case BindingType::UniformBuffer:
                    w.buffer(e.binding, vk_buffer(e.buffer)->buffer, vk::DescriptorType::UniformBuffer, e.offset, e.size, e.element);
                    break;
                case BindingType::StorageBuffer:
                    w.buffer(e.binding, vk_buffer(e.buffer)->buffer, vk::DescriptorType::StorageBuffer, e.offset, e.size, e.element);
                    break;
            }
        }

        w.update(g->set);
        out = std::move(g);
        return Result::Success;
    }

    Result create_render_pipeline(const RenderPipelineInput& in, std::shared_ptr<RenderPipelineImpl>& out) override {
        auto p = std::make_shared<RenderPipeline>();
        p->core   = m_core;
        p->layout = in.layout;
        vk::GraphicsPipelineDesc d;
        d.vertex_shader   = &static_cast<Shader*>(in.vertex.get())->module;
        d.fragment_shader = in.fragment ? &static_cast<Shader*>(in.fragment.get())->module : nullptr;
        d.layout          = &static_cast<PipelineLayout*>(in.layout.get())->layout;
        for (const VertexBinding& b : in.vertex_bindings) d.vertex_bindings.push_back({ b.binding, b.stride, b.rate == VertexRate::PerInstance ? vk::VertexRate::PerInstance : vk::VertexRate::PerVertex });
        for (const VertexAttribute& a : in.vertex_attributes) d.vertex_attributes.push_back({ a.location, a.binding, format(a.format), a.offset });
        d.topology   = in.topology == Topology::LineList ? vk::Topology::LineList : in.topology == Topology::PointList ? vk::Topology::PointList : vk::Topology::TriangleList;
        d.cull_mode  = in.cull_mode == CullMode::Front ? vk::CullMode::Front : in.cull_mode == CullMode::Back ? vk::CullMode::Back : vk::CullMode::None;
        d.front_face = in.front_face == FrontFace::Clockwise ? vk::FrontFace::Clockwise : vk::FrontFace::CounterClockwise;
        d.samples    = static_cast<vk::SampleCount>(in.samples);
        d.depth.test          = in.depth.test;
        d.depth.write         = in.depth.write;
        d.depth.compare       = compare_op(in.depth.compare);
        d.depth.clamp         = in.depth.clamp;
        d.depth.bias          = in.depth.bias;
        d.depth.bias_constant = in.depth.bias_constant;
        d.depth.bias_slope    = in.depth.bias_slope;
        for (Format f : in.color_formats) d.color_formats.push_back(format(f));
        d.depth_format = format(in.depth_format);
        vk::BlendState b;
        b.enable     = in.blend.enable;
        b.src_color  = blend_factor(in.blend.src_color);
        b.dst_color  = blend_factor(in.blend.dst_color);
        b.src_alpha  = blend_factor(in.blend.src_alpha);
        b.dst_alpha  = blend_factor(in.blend.dst_alpha);
        b.write_mask = in.blend.write ? vk::ColorWrite::All : vk::ColorWrite::None;
        d.blend = { b };
        d.name  = in.name;
        const vk::Result r = p->pipeline.create(m_core->device, d);
        if (vk::failed(r)) return fail(result(r), std::string("render pipeline ") + (in.name ? in.name : "") + ": " + text(r));
        out = std::move(p);
        return Result::Success;
    }

    Result create_compute_pipeline(const std::shared_ptr<ShaderImpl>& shader, const std::shared_ptr<PipelineLayoutImpl>& layout, const char* name, std::shared_ptr<ComputePipelineImpl>& out) override {
        auto p = std::make_shared<ComputePipeline>();
        p->core   = m_core;
        p->layout = layout;
        vk::ComputePipelineDesc d;
        d.shader = &static_cast<Shader*>(shader.get())->module;
        d.layout = &static_cast<PipelineLayout*>(layout.get())->layout;
        d.name   = name;
        const vk::Result r = p->pipeline.create(m_core->device, d);
        if (vk::failed(r)) return fail(result(r), std::string("compute pipeline: ") + text(r));
        out = std::move(p);
        return Result::Success;
    }

    Result create_query_set(std::uint32_t count, const char* name, std::shared_ptr<QuerySetImpl>& out) override {
        auto q = std::make_shared<QuerySet>();
        q->core  = m_core;
        q->count = count;
        const vk::Result r = q->pool.create(m_core->device, count, name);
        if (vk::failed(r)) return result(r);
        out = std::move(q);
        return Result::Success;
    }

    Result begin_frame(FrameInfo& out) override {
        Core& c = *m_core;
        if (c.in_frame) return Result::InvalidArgument;

        if (c.needs_resize || !c.swapchain.valid()) {
            const vk::Result r = c.swapchain.valid() ? c.swapchain.recreate(c.device, vk::Extent2D{ std::max(c.pending.width, 1u), std::max(c.pending.height, 1u) }) : c.swapchain.create(c.device, c.surface, swapchain_desc(c.pending));
            if (r == vk::Result::NotReady) return Result::NotReady;
            if (vk::failed(r)) return fail(result(r), std::string("swapchain: ") + text(r));
            c.needs_resize = false;
            decide_present_path();
        }

        const std::uint32_t index = static_cast<std::uint32_t>(c.serial % c.slots.size());
        Core::Slot& s = c.slots[index];
        vk::Result r = s.fence.wait();
        if (vk::failed(r)) return result(r);
        c.completed = std::max(c.completed, s.serial);
        c.collect();
        for (auto& p : s.pools) p.reset();
        s.cursor = 0;
        r = c.swapchain.acquire(s.acquired, c.image_index);

        if (r == vk::Result::OutOfDate) {
            c.needs_resize = true;
            c.pending = { c.swapchain.desc().size.width, c.swapchain.desc().size.height };
            return Result::NotReady;
        }

        if (vk::failed(r)) return result(r);
        s.fence.reset();
        s.cmd.reset();
        s.cmd.begin(true);
        s.serial       = c.serial;
        c.frame        = &s;
        c.in_frame     = true;
        c.presented    = false;
        c.image_layout = vk::ImageLayout::Undefined;
        out.index  = index;
        out.serial = c.serial;
        out.extent = { c.swapchain.extent().width, c.swapchain.extent().height };
        return Result::Success;
    }

    Result submit(const CommandStream& s) override {
        if (s.commands.empty()) return Result::Success;
        Core& c = *m_core;

        if (c.in_frame) {
            Replay(c, c.frame->cmd, s).run();
            return Result::Success;
        }

        return result(vk::submit_immediate(c.device, [&](vk::CommandBuffer& cmd) { Replay(c, cmd, s).run(); }));
    }

    Result present(TextureImpl& source, Filter f) override {
        Core& c = *m_core;
        if (!c.in_frame) return Result::InvalidArgument;
        Texture& src = *vk_texture(&source);
        if (src.desc.samples > 1 || is_depth(src.desc.format)) return Result::InvalidArgument;
        const vk::CommandBuffer& cmd = c.frame->cmd;
        const VkImage target = c.swapchain.image(c.image_index);
        const vk::Extent2D extent = c.swapchain.extent();
        const bool same = extent.width == src.desc.extent.width && extent.height == src.desc.extent.height;
        cmd.begin_label("fizmo present");

        if (c.blit_present) {
            Barriers b;
            b.add(src, vk::ImageLayout::TransferSrc);
            b.add_raw(target, c.image_layout, vk::ImageLayout::TransferDst);
            b.flush(cmd, true);
            c.image_layout = vk::ImageLayout::TransferDst;
            cmd.blit(src.own.image.handle(), { { 0, 0 }, { src.desc.extent.width, src.desc.extent.height } }, target, { { 0, 0 }, extent }, same ? vk::Filter::Nearest : filter(f));
        } else {
            const Result r = present_with_pipeline(src, same ? Filter::Nearest : f);
            if (failed(r)) { cmd.end_label(); return r; }
        }

        cmd.end_label();
        c.presented = true;
        return Result::Success;
    }

    Result end_frame() override {
        Core& c = *m_core;
        if (!c.in_frame) return Result::InvalidArgument;
        Core::Slot& s = *c.frame;
        const VkImage target = c.swapchain.image(c.image_index);

        if (!c.presented && c.blit_present) {
            Barriers b;
            b.add_raw(target, c.image_layout, vk::ImageLayout::TransferDst);
            b.flush(s.cmd, true);
            c.image_layout = vk::ImageLayout::TransferDst;
            s.cmd.clear_color(target, vk::ClearColor{ 0, 0, 0, 1 });
        }

        Barriers b;
        b.add_raw(target, c.image_layout, vk::ImageLayout::Present);
        b.flush(s.cmd, false);
        c.in_frame = false;
        c.frame = nullptr;
        vk::Result r = s.cmd.end();
        if (vk::failed(r)) return result(r);
        const vk::SemaphoreWait   wait  { &s.acquired, vk::PipelineStage::AllCommands, 0 };
        const vk::SemaphoreSignal signal{ &c.swapchain.present_semaphore(c.image_index), vk::PipelineStage::AllCommands, 0 };
        r = c.device.graphics_queue().submit(s.cmd, wait, signal, &s.fence);
        if (vk::failed(r)) return result(r);
        c.submitted = c.serial;
        ++c.serial;
        r = c.swapchain.present(c.device.present_queue(), c.image_index);

        if (r == vk::Result::OutOfDate || r == vk::Result::Suboptimal) {
            if (!c.needs_resize) { c.pending = { c.swapchain.desc().size.width, c.swapchain.desc().size.height }; c.needs_resize = true; }
            return Result::Success;
        }

        return result(r);
    }

    void resize(std::uint32_t width, std::uint32_t height) override {
        m_core->pending = { width, height };
        m_core->needs_resize = true;
    }

    Result set_vsync(bool enabled) override {
        Core& c = *m_core;
        if (enabled == c.vsync) return Result::Success;
        c.vsync = enabled;
        if (c.in_frame) return Result::InvalidArgument;
        c.device.wait_idle();
        c.completed = c.submitted;
        c.collect();
        const vk::Extent2D e = c.swapchain.valid() ? c.swapchain.extent() : vk::Extent2D{ c.pending.width, c.pending.height };
        c.swapchain.destroy();
        const vk::Result r = c.swapchain.create(c.device, c.surface, swapchain_desc({ e.width, e.height }));
        if (r == vk::Result::NotReady) { c.needs_resize = true; return Result::Success; }
        decide_present_path();
        return result(r);
    }

    Result wait_idle() override {
        Core& c = *m_core;
        if (!c.device.valid()) return Result::NotInitialized;
        const vk::Result r = c.device.wait_idle();
        c.completed = c.submitted;
        if (!c.in_frame) c.collect();
        return result(r);
    }

    Result write_texture(TextureImpl& texture, const TextureRegion& region, const void* data, std::uint32_t row_pixels) override {
        Core& c = *m_core;
        Texture& t = *vk_texture(&texture);
        if (t.desc.samples > 1) return Result::InvalidArgument;
        const std::uint64_t texel = texel_size(t.desc.format);
        const std::uint64_t row   = row_pixels ? row_pixels : region.extent.width;
        const std::uint64_t slice = row * region.extent.height * texel;
        const std::uint64_t bytes = slice * (std::max(region.extent.depth, 1u) - 1) + (row * (region.extent.height - 1) + region.extent.width) * texel;
        vk::Buffer staging;
        vk::Result r = staging.create(c.device, { bytes, vk::BufferUsage::TransferSrc, vk::MemoryUsage::CpuToGpu, "fizmo gpu texture staging" });
        if (vk::failed(r)) return result(r);
        r = staging.write(data, bytes);
        if (vk::failed(r)) return result(r);
        const VkBufferImageCopy region_info = image_copy_region(t, region, 0, row_pixels);

        auto record = [&](const vk::CommandBuffer& cmd) {
            Barriers b;
            b.add(t, vk::ImageLayout::TransferDst);
            b.flush(cmd, true);
            c.fn().vkCmdCopyBufferToImage(cmd.handle(), staging.handle(), t.own.image.handle(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region_info);
        };

        if (c.in_frame) {
            record(c.frame->cmd);
            c.retire(std::move(staging));
            return Result::Success;
        }

        return result(vk::submit_immediate(c.device, [&](vk::CommandBuffer& cmd) { record(cmd); }));
    }

    Result read_texture(TextureImpl& texture, const TextureRegion& region, void* out) override {
        Core& c = *m_core;
        if (c.in_frame) return Result::InvalidArgument;
        Texture& t = *vk_texture(&texture);
        if (t.desc.samples > 1) return Result::InvalidArgument;
        const std::uint64_t bytes = static_cast<std::uint64_t>(region.extent.width) * region.extent.height * std::max(region.extent.depth, 1u) * texel_size(t.desc.format);
        vk::Buffer readback;
        vk::Result r = readback.create(c.device, { bytes, vk::BufferUsage::TransferDst, vk::MemoryUsage::GpuToCpu, "fizmo gpu readback" });
        if (vk::failed(r)) return result(r);
        c.device.wait_idle();
        const VkBufferImageCopy region_info = image_copy_region(t, region, 0, 0);

        r = vk::submit_immediate(c.device, [&](vk::CommandBuffer& cmd) {
            Barriers b;
            b.add(t, vk::ImageLayout::TransferSrc);
            b.flush(cmd, true);
            c.fn().vkCmdCopyImageToBuffer(cmd.handle(), t.own.image.handle(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, readback.handle(), 1, &region_info);
        });

        if (vk::failed(r)) return result(r);
        return result(readback.read(out, bytes));
    }

    bool read_timestamps(QuerySetImpl& set, std::uint32_t first, std::uint32_t count, std::uint64_t* out) override {
        auto& q = static_cast<QuerySet&>(set);
        if (!q.pool.read(first, count, out)) return false;
        const double ns_per_tick = static_cast<double>(m_core->device.limits().timestampPeriod);
        for (std::uint32_t i = 0; i < count; ++i) out[i] = static_cast<std::uint64_t>(static_cast<double>(out[i]) * ns_per_tick);
        return true;
    }

private:
    Result present_with_pipeline(Texture& src, Filter f) {
        Core& c = *m_core;
        const vk::CommandBuffer& cmd = c.frame->cmd;

        if (!c.present_pipeline.valid() || c.present_format != c.swapchain.format()) {
            c.present_pipeline.destroy();
            if (!c.present_vs.valid() && vk::failed(c.present_vs.create(c.device, vk::Span<std::uint32_t>(kPresentVertex, sizeof(kPresentVertex) / 4), "fizmo gpu present.vert"))) return Result::BackendFailed;
            if (!c.present_fs.valid() && vk::failed(c.present_fs.create(c.device, vk::Span<std::uint32_t>(kPresentFragment, sizeof(kPresentFragment) / 4), "fizmo gpu present.frag"))) return Result::BackendFailed;
            if (!c.present_set.valid() && vk::failed(c.present_set.create(c.device, { vk::DescriptorBinding{ 0, vk::DescriptorType::CombinedImageSampler, vk::ShaderStage::Fragment } }))) return Result::BackendFailed;

            if (!c.present_layout.valid()) {
                vk::PipelineLayoutDesc ld;
                ld.set_layouts = { &c.present_set };
                if (vk::failed(c.present_layout.create(c.device, ld))) return Result::BackendFailed;
            }

            for (int i = 0; i < 2; ++i) {
                if (c.present_samplers[i].valid()) continue;
                vk::SamplerDesc sd = vk::SamplerDesc::linear_clamp();
                if (i == 0) sd = vk::SamplerDesc::nearest_clamp();
                sd.max_lod = 0.0f;
                if (vk::failed(c.present_samplers[i].create(c.device, sd))) return Result::BackendFailed;
            }

            vk::GraphicsPipelineDesc pd;
            pd.vertex_shader   = &c.present_vs;
            pd.fragment_shader = &c.present_fs;
            pd.layout          = &c.present_layout;
            pd.color_formats   = { c.swapchain.format() };
            pd.name            = "fizmo gpu present";
            if (vk::failed(c.present_pipeline.create(c.device, pd))) return Result::BackendFailed;
            c.present_format = c.swapchain.format();
        }

        vk::DescriptorSet set;
        VkDescriptorPool pool = VK_NULL_HANDLE;
        if (vk::failed(c.allocate(c.present_set, true, set, pool))) return Result::OutOfMemory;
        vk::DescriptorWriter().image(0, src.sampled(), c.present_samplers[f == Filter::Linear ? 1 : 0]).update(set);
        Barriers b;
        b.add(src, vk::ImageLayout::ShaderReadOnly);
        b.add_raw(c.swapchain.image(c.image_index), c.image_layout, vk::ImageLayout::ColorAttachment);
        b.flush(cmd, true);
        c.image_layout = vk::ImageLayout::ColorAttachment;
        vk::ColorAttachment out;
        out.view = &c.swapchain.view(c.image_index);
        out.load = vk::LoadOp::DontCare;
        cmd.begin_rendering({ { { 0, 0 }, c.swapchain.extent() }, out });
        cmd.bind(c.present_pipeline);
        cmd.set_viewport_and_scissor(c.swapchain.extent());
        cmd.bind_descriptor_set(c.present_pipeline, 0, set);
        cmd.draw(3);
        cmd.end_rendering();
        return Result::Success;
    }

    class Replay {
    private:
        Core&                     c;
        const vk::CommandBuffer&  cmd;
        const CommandStream&      s;
        const vk::DeviceDispatch& fn;
        const RenderPipeline*     pipeline   = nullptr;
        const ComputePipeline*    compute    = nullptr;
        const PipelineLayout*     layout     = nullptr;
        VkPipelineBindPoint       point      = VK_PIPELINE_BIND_POINT_GRAPHICS;
        BindGroupImpl*            groups[kMaxGroups] = {};
        std::uint32_t             dirty      = 0;
        const PipelineLayout*     flushed    = nullptr;
        bool                      in_pass    = false;
        bool                      labelled   = false;

    public:
        Replay(Core& core, const vk::CommandBuffer& command, const CommandStream& stream) : c(core), cmd(command), s(stream), fn(core.fn()) {}

        void run() {
            for (const Command& x : s.commands) {
                switch (x.op) {
                    case Op::BeginRenderPass:     begin_pass(s.passes[x.pass.index]); break;
                    case Op::EndRenderPass:       end_pass(); break;
                    case Op::SetRenderPipeline:   set_pipeline(static_cast<const RenderPipeline*>(x.render.pipeline)); break;
                    case Op::SetComputePipeline:  set_compute(static_cast<const ComputePipeline*>(x.compute.pipeline)); break;
                    case Op::SetBindGroup:        if (groups[x.bind.index] != x.bind.group) { groups[x.bind.index] = x.bind.group; dirty |= 1u << x.bind.index; } break;
                    case Op::PushConstants:       push(x); break;
                    case Op::SetVertexBuffer:     { const VkBuffer b = vk_buffer(x.vertex.buffer)->buffer.handle(); const VkDeviceSize o = x.vertex.offset; fn.vkCmdBindVertexBuffers(cmd.handle(), x.vertex.slot, 1, &b, &o); break; }
                    case Op::SetIndexBuffer:      fn.vkCmdBindIndexBuffer(cmd.handle(), vk_buffer(x.index.buffer)->buffer.handle(), x.index.offset, x.index.type == IndexType::UInt16 ? VK_INDEX_TYPE_UINT16 : VK_INDEX_TYPE_UINT32); break;
                    case Op::SetViewport:         cmd.set_viewport({ x.viewport.x, x.viewport.y, x.viewport.w, x.viewport.h, x.viewport.min_depth, x.viewport.max_depth }); break;
                    case Op::SetScissor:          cmd.set_scissor({ { x.scissor.x, x.scissor.y }, { x.scissor.w, x.scissor.h } }); break;
                    case Op::Draw:                if (ready()) fn.vkCmdDraw(cmd.handle(), x.draw.count, x.draw.instances, x.draw.first, x.draw.first_instance); break;
                    case Op::DrawIndexed:         if (ready()) fn.vkCmdDrawIndexed(cmd.handle(), x.indexed.count, x.indexed.instances, x.indexed.first_index, x.indexed.vertex_offset, x.indexed.first_instance); break;
                    case Op::DrawIndexedIndirect: if (ready() && c.caps.multi_draw_indirect) fn.vkCmdDrawIndexedIndirect(cmd.handle(), vk_buffer(x.indirect.buffer)->buffer.handle(), x.indirect.offset, x.indirect.count, x.indirect.stride); break;
                    case Op::Dispatch:            dispatch(x); break;
                    case Op::CopyBuffer:          copy_buffer(s.copies[x.copy.index]); break;
                    case Op::CopyBufferToTexture: copy_buffer_to_texture(s.copies[x.copy.index]); break;
                    case Op::CopyTextureToBuffer: copy_texture_to_buffer(s.copies[x.copy.index]); break;
                    case Op::CopyTexture:         copy_texture(s.copies[x.copy.index]); break;
                    case Op::BlitTexture:         blit(s.copies[x.copy.index]); break;
                    case Op::GenerateMipmaps:     mipmaps(*vk_texture(x.mips.texture)); break;
                    case Op::ResetQueries:        cmd.reset_timestamps(static_cast<QuerySet*>(x.query.set)->pool, x.query.first, x.query.count); break;
                    case Op::WriteTimestamp:      cmd.write_timestamp(static_cast<QuerySet*>(x.query.set)->pool, x.query.first); break;
                    case Op::PushLabel:           cmd.begin_label(s.text(x.bytes.at)); break;
                    case Op::PopLabel:            cmd.end_label(); break;
                }
            }

            if (in_pass) end_pass();
        }

    private:
        void reset() noexcept {
            pipeline = nullptr;
            compute  = nullptr;
            layout   = nullptr;
            flushed  = nullptr;
            dirty    = 0;
            for (auto& g : groups) g = nullptr;
        }

        static void prepare_groups(Barriers& b, BindGroupImpl* const* list, std::uint32_t count) {
            for (std::uint32_t i = 0; i < count; ++i) {
                const BindGroupImpl* g = list[i];

                for (const ResourceBinding& e : g->entries) {
                    if (!e.texture) continue;
                    const BindGroupLayoutEntry* le = g->layout->find(e.binding);
                    if (!le) continue;
                    b.add(*vk_texture(e.texture), le->type == BindingType::StorageTexture ? vk::ImageLayout::General : vk::ImageLayout::ShaderReadOnly);
                }
            }
        }

        void begin_pass(const PassRecord& p) {
            if (in_pass) end_pass();
            Barriers b;
            prepare_groups(b, s.groups.data() + p.groups_first, p.groups_count);
            vk::ColorAttachment colors[PassRecord::kMaxColors];

            for (std::uint32_t i = 0; i < p.color_count; ++i) {
                const ColorTargetRecord& r = p.colors[i];
                Texture& t = *vk_texture(r.texture);
                b.add(t, vk::ImageLayout::ColorAttachment);
                colors[i].view   = t.attachment(r.layer, r.mip);
                colors[i].layout = vk::ImageLayout::ColorAttachment;
                colors[i].load   = r.load == LoadOp::Clear ? vk::LoadOp::Clear : r.load == LoadOp::Load ? vk::LoadOp::Load : vk::LoadOp::DontCare;
                colors[i].store  = r.store == StoreOp::Store ? vk::StoreOp::Store : vk::StoreOp::DontCare;
                colors[i].clear  = { r.clear.r, r.clear.g, r.clear.b, r.clear.a };

                if (r.resolve) {
                    Texture& rt = *vk_texture(r.resolve);
                    b.add(rt, vk::ImageLayout::ColorAttachment);
                    colors[i].resolve = rt.attachment(0, 0);
                }
            }

            vk::DepthAttachment depth;

            if (p.depth) {
                Texture& t = *vk_texture(p.depth);
                b.add(t, vk::ImageLayout::DepthAttachment);
                depth.view        = t.attachment(p.depth_layer, 0);
                depth.layout      = vk::ImageLayout::DepthAttachment;
                depth.load        = p.depth_load == LoadOp::Clear ? vk::LoadOp::Clear : p.depth_load == LoadOp::Load ? vk::LoadOp::Load : vk::LoadOp::DontCare;
                depth.store       = p.depth_store == StoreOp::Store ? vk::StoreOp::Store : vk::StoreOp::DontCare;
                depth.clear       = { p.clear_depth, p.clear_stencil };
                depth.has_stencil = has_stencil(t.desc.format);

                if (p.depth_resolve) {
                    Texture& rt = *vk_texture(p.depth_resolve);
                    b.add(rt, vk::ImageLayout::DepthAttachment);
                    depth.resolve = rt.attachment(0, 0);
                }
            }

            b.flush(cmd, true);
            reset();
            labelled = p.label != UINT32_MAX;
            if (labelled) cmd.begin_label(s.text(p.label));
            vk::RenderingDesc rd;
            rd.area   = { { p.area.offset.x, p.area.offset.y }, { p.area.extent.width, p.area.extent.height } };
            rd.colors = vk::Span<vk::ColorAttachment>(colors, p.color_count);
            rd.depth  = p.depth ? &depth : nullptr;
            cmd.begin_rendering(rd);
            cmd.set_viewport({ static_cast<float>(p.area.offset.x), static_cast<float>(p.area.offset.y), static_cast<float>(p.area.extent.width), static_cast<float>(p.area.extent.height), 0.0f, 1.0f });
            cmd.set_scissor(rd.area);
            in_pass = true;
        }

        void end_pass() {
            cmd.end_rendering();
            if (labelled) cmd.end_label();
            labelled = false;
            in_pass = false;
            reset();
        }

        void bind_layout(const PipelineLayout* l, VkPipelineBindPoint p) noexcept {
            if (l != layout || p != point) dirty = ~0u;
            layout = l;
            point = p;
        }

        void set_pipeline(const RenderPipeline* p) {
            if (!in_pass) return;
            pipeline = p;
            compute = nullptr;
            fn.vkCmdBindPipeline(cmd.handle(), VK_PIPELINE_BIND_POINT_GRAPHICS, p->pipeline.handle());
            bind_layout(static_cast<const PipelineLayout*>(p->layout.get()), VK_PIPELINE_BIND_POINT_GRAPHICS);
        }

        void set_compute(const ComputePipeline* p) {
            if (in_pass) return;
            compute = p;
            pipeline = nullptr;
            fn.vkCmdBindPipeline(cmd.handle(), VK_PIPELINE_BIND_POINT_COMPUTE, p->pipeline.handle());
            bind_layout(static_cast<const PipelineLayout*>(p->layout.get()), VK_PIPELINE_BIND_POINT_COMPUTE);
        }

        void push(const Command& x) {
            if (!layout || !layout->push_size) return;
            if (x.bytes.offset >= layout->push_size) return;
            const std::uint32_t size = std::min(x.bytes.size, layout->push_size - x.bytes.offset);
            fn.vkCmdPushConstants(cmd.handle(), layout->layout.handle(), vk::to_vk(layout->push), x.bytes.offset, size, s.bytes.data() + x.bytes.at);
        }

        void flush_groups() {
            if (!layout || !dirty) return;
            const std::uint32_t n = static_cast<std::uint32_t>(std::min<std::size_t>(layout->groups.size(), kMaxGroups));

            for (std::uint32_t i = 0; i < n; ++i) {
                if (!(dirty & (1u << i)) || !groups[i]) continue;
                const VkDescriptorSet set = static_cast<const BindGroup*>(groups[i])->set.handle();
                fn.vkCmdBindDescriptorSets(cmd.handle(), point, layout->layout.handle(), i, 1, &set, 0, nullptr);
            }

            dirty = 0;
            flushed = layout;
        }

        bool ready() {
            if (!pipeline) return false;
            flush_groups();
            return true;
        }

        void dispatch(const Command& x) {
            if (!compute || in_pass) return;
            Barriers b;
            prepare_groups(b, s.groups.data() + x.dispatch.groups_first, x.dispatch.groups_count);
            b.flush(cmd, true);
            flush_groups();
            fn.vkCmdDispatch(cmd.handle(), x.dispatch.x, x.dispatch.y, x.dispatch.z);
        }

        void copy_buffer(const CopyRecord& r) {
            Barriers b;
            b.flush(cmd, true);
            cmd.copy_buffer(vk_buffer(r.src_buffer)->buffer, vk_buffer(r.dst_buffer)->buffer, r.size, r.src_offset, r.dst_offset);
        }

        void copy_buffer_to_texture(const CopyRecord& r) {
            Texture& t = *vk_texture(r.dst_texture);
            Barriers b;
            b.add(t, vk::ImageLayout::TransferDst);
            b.flush(cmd, true);
            const VkBufferImageCopy region = image_copy_region(t, r.dst_region, r.src_offset, r.row_pixels);
            fn.vkCmdCopyBufferToImage(cmd.handle(), vk_buffer(r.src_buffer)->buffer.handle(), t.own.image.handle(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
        }

        void copy_texture_to_buffer(const CopyRecord& r) {
            Texture& t = *vk_texture(r.src_texture);
            Barriers b;
            b.add(t, vk::ImageLayout::TransferSrc);
            b.flush(cmd, true);
            const VkBufferImageCopy region = image_copy_region(t, r.src_region, r.dst_offset, 0);
            fn.vkCmdCopyImageToBuffer(cmd.handle(), t.own.image.handle(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, vk_buffer(r.dst_buffer)->buffer.handle(), 1, &region);
        }

        static VkImageSubresourceLayers layers_of(const Texture& t, const TextureRegion& r) noexcept {
            return { vk::to_vk(t.aspect()), r.mip, t.desc.dimension == TextureDimension::D3 ? 0u : r.layer, 1 };
        }

        void copy_texture(const CopyRecord& r) {
            Texture& src = *vk_texture(r.src_texture);
            Texture& dst = *vk_texture(r.dst_texture);
            Barriers b;
            b.add(src, vk::ImageLayout::TransferSrc);
            b.add(dst, vk::ImageLayout::TransferDst);
            b.flush(cmd, true);
            VkImageCopy region{};
            region.srcSubresource = layers_of(src, r.src_region);
            region.dstSubresource = layers_of(dst, r.dst_region);
            region.srcOffset      = { r.src_region.origin.x, r.src_region.origin.y, r.src_region.origin.z };
            region.dstOffset      = { r.dst_region.origin.x, r.dst_region.origin.y, r.dst_region.origin.z };
            region.extent         = { r.src_region.extent.width, r.src_region.extent.height, std::max(r.src_region.extent.depth, 1u) };
            fn.vkCmdCopyImage(cmd.handle(), src.own.image.handle(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, dst.own.image.handle(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
        }

        void blit(const CopyRecord& r) {
            Texture& src = *vk_texture(r.src_texture);
            Texture& dst = *vk_texture(r.dst_texture);
            Barriers b;
            b.add(src, vk::ImageLayout::TransferSrc);
            b.add(dst, vk::ImageLayout::TransferDst);
            b.flush(cmd, true);
            VkImageBlit region{};
            region.srcSubresource = layers_of(src, r.src_region);
            region.dstSubresource = layers_of(dst, r.dst_region);
            const auto& so = r.src_region.origin; const auto& se = r.src_region.extent;
            const auto& d0 = r.dst_region.origin; const auto& de = r.dst_region.extent;
            region.srcOffsets[0] = { so.x, so.y, so.z };
            region.srcOffsets[1] = { so.x + static_cast<std::int32_t>(se.width), so.y + static_cast<std::int32_t>(se.height), so.z + static_cast<std::int32_t>(std::max(se.depth, 1u)) };
            region.dstOffsets[0] = { d0.x, d0.y, d0.z };
            region.dstOffsets[1] = { d0.x + static_cast<std::int32_t>(de.width), d0.y + static_cast<std::int32_t>(de.height), d0.z + static_cast<std::int32_t>(std::max(de.depth, 1u)) };
            const VkFilter f = is_depth(src.desc.format) || r.filter == Filter::Nearest ? VK_FILTER_NEAREST : VK_FILTER_LINEAR;
            fn.vkCmdBlitImage(cmd.handle(), src.own.image.handle(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, dst.own.image.handle(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region, f);
        }

        void mipmaps(Texture& t) {
            Barriers b;
            b.add(t, vk::ImageLayout::TransferDst);
            b.flush(cmd, true);
            cmd.generate_mipmaps(t.own.image, vk::ImageLayout::ShaderReadOnly);
        }
    };
};

} // namespace vkb
} // namespace detail
} // namespace gpu
} // namespace fizmo

#endif // OS_WINDOWS || OS_LINUX
#endif // FIZMO_GPU_VULKAN_BACKEND_HPP
