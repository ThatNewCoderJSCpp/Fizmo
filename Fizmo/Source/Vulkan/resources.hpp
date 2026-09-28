#ifndef FIZMO_VULKAN_RESOURCES_HPP
#define FIZMO_VULKAN_RESOURCES_HPP

#include "device.hpp"
#include <algorithm>
#include <cstring>

namespace fizmo {
namespace vulkan {

namespace detail {

inline Result allocate_memory(
    const DeviceState* d, const VkMemoryRequirements& req, MemoryUsage usage,
    bool device_address, native::DeviceMemory& out, VkMemoryPropertyFlags& out_flags
) noexcept {
    std::uint32_t type = UINT32_MAX;
    switch (usage) {
        case MemoryUsage::GpuOnly:
            type = d->find_memory_type(req.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
            if (type == UINT32_MAX) type = d->find_memory_type(req.memoryTypeBits, 0);
            break;
        case MemoryUsage::CpuToGpu:
            type = d->find_memory_type(req.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
            if (type == UINT32_MAX) type = d->find_memory_type(req.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
            break;
        case MemoryUsage::GpuToCpu:
            type = d->find_memory_type(req.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_CACHED_BIT);
            if (type == UINT32_MAX) type = d->find_memory_type(req.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
            break;
    }

    if (type == UINT32_MAX) return Result::NoSuitableMemory;
    auto flags = detail::make<VkMemoryAllocateFlagsInfo>(VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO);
    flags.flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT;
    auto info = detail::make<VkMemoryAllocateInfo>(VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO);
    info.pNext           = device_address ? &flags : nullptr;
    info.allocationSize  = req.size;
    info.memoryTypeIndex = type;
    out_flags = d->memory.memoryTypes[type].propertyFlags;
    return to_result(d->fn.vkAllocateMemory(d->handle, &info, nullptr, &out));
}

} // namespace detail

struct BufferDesc {
    std::uint64_t size   = 0;
    BufferUsage   usage  = BufferUsage::None;
    MemoryUsage   memory = MemoryUsage::GpuOnly;
    const char*   name   = nullptr;
};

class Buffer {
private:
    FIZMO_VK_UNIQUE(VkDeviceMemory, vkFreeMemory) m_memory;   
    FIZMO_VK_UNIQUE(VkBuffer, vkDestroyBuffer)    m_buffer;
    std::uint64_t         m_size     = 0;
    BufferUsage           m_usage    = BufferUsage::None;
    MemoryUsage           m_memory_usage = MemoryUsage::GpuOnly;
    void*                 m_mapped   = nullptr;
    bool                  m_coherent = false;
    native::DeviceAddress m_address  = 0;

public:
    Buffer() noexcept = default;
    Buffer(Buffer&& o) noexcept { *this = std::move(o); }

    Buffer& operator=(Buffer&& o) noexcept {
        if (this != &o) {
            destroy();
            m_memory = std::move(o.m_memory);
            m_buffer = std::move(o.m_buffer);
            m_size = std::exchange(o.m_size, 0);
            m_usage = o.m_usage;
            m_memory_usage = o.m_memory_usage;
            m_mapped = std::exchange(o.m_mapped, nullptr);
            m_coherent = o.m_coherent;
            m_address = std::exchange(o.m_address, 0);
        }
        return *this;
    }

    ~Buffer() noexcept { destroy(); }

    Result create(const Device& device, const BufferDesc& desc) noexcept {
        destroy();
        if (!device.valid()) return Result::NotInitialized;
        if (desc.size == 0) return Result::InvalidArgument;
        const auto* d = device.state();
        BufferUsage usage = desc.usage;
        if (desc.memory == MemoryUsage::GpuOnly)  usage |= BufferUsage::TransferDst | BufferUsage::TransferSrc;
        if (desc.memory == MemoryUsage::GpuToCpu) usage |= BufferUsage::TransferDst;
        const bool wants_address = has_flag(usage, BufferUsage::DeviceAddress);
        if (wants_address && !d->features.buffer_device_address) return Result::FeatureNotPresent;
        auto info = detail::make<VkBufferCreateInfo>(VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO);
        info.size        = desc.size;
        info.usage       = to_vk(usage);
        info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        native::Buffer buf = VK_NULL_HANDLE;
        VkResult r = d->fn.vkCreateBuffer(d->handle, &info, nullptr, &buf);
        if (r != VK_SUCCESS) return to_result(r);
        m_buffer = { d, buf };
        VkMemoryRequirements req{};
        d->fn.vkGetBufferMemoryRequirements(d->handle, buf, &req);
        native::DeviceMemory mem = VK_NULL_HANDLE;
        VkMemoryPropertyFlags flags = 0;
        Result res = detail::allocate_memory(d, req, desc.memory, wants_address, mem, flags);
        if (failed(res)) { destroy(); return res; }
        m_memory = { d, mem };
        r = d->fn.vkBindBufferMemory(d->handle, buf, mem, 0);
        if (r != VK_SUCCESS) { destroy(); return to_result(r); }

        if (flags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) {
            r = d->fn.vkMapMemory(d->handle, mem, 0, VK_WHOLE_SIZE, 0, &m_mapped);
            if (r != VK_SUCCESS) { destroy(); return to_result(r); }
            m_coherent = (flags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) != 0;
        }

        if (wants_address) {
            auto ai = detail::make<VkBufferDeviceAddressInfo>(VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO);
            ai.buffer = buf;
            m_address = d->fn.vkGetBufferDeviceAddress(d->handle, &ai);
        }

        m_size = desc.size;
        m_usage = usage;
        m_memory_usage = desc.memory;
        if (desc.name) d->set_name(VK_OBJECT_TYPE_BUFFER, reinterpret_cast<std::uint64_t>(buf), desc.name);
        return Result::Success;
    }

    void destroy() noexcept {
        m_buffer.reset();
        m_memory.reset();   
        m_mapped = nullptr;
        m_size = 0;
        m_address = 0;
    }

    Result write(const void* data, std::uint64_t size, std::uint64_t offset = 0) noexcept {
        if (!m_mapped) return Result::InvalidArgument;
        if (offset + size > m_size) return Result::InvalidArgument;
        std::memcpy(static_cast<char*>(m_mapped) + offset, data, static_cast<std::size_t>(size));
        return flush(offset, size);
    }

    template <typename T>
    Result write(const std::vector<T>& items, std::uint64_t offset = 0) noexcept {
        return write(items.data(), items.size() * sizeof(T), offset);
    }

    Result read(void* out, std::uint64_t size, std::uint64_t offset = 0) const noexcept {
        if (!m_mapped || offset + size > m_size) return Result::InvalidArgument;
        Result r = invalidate(offset, size);
        if (failed(r)) return r;
        std::memcpy(out, static_cast<const char*>(m_mapped) + offset, static_cast<std::size_t>(size));
        return Result::Success;
    }

    Result flush(std::uint64_t = 0, std::uint64_t = kWholeSize) const noexcept {
        if (m_coherent || !m_mapped) return Result::Success;
        auto range = detail::make<VkMappedMemoryRange>(VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE);
        range.memory = m_memory.get();
        range.size   = VK_WHOLE_SIZE;
        auto* d = m_memory.device();
        return to_result(d->fn.vkFlushMappedMemoryRanges(d->handle, 1, &range));
    }

    Result invalidate(std::uint64_t = 0, std::uint64_t = kWholeSize) const noexcept {
        if (m_coherent || !m_mapped) return Result::Success;
        auto range = detail::make<VkMappedMemoryRange>(VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE);
        range.memory = m_memory.get();
        range.size   = VK_WHOLE_SIZE;
        auto* d = m_memory.device();
        return to_result(d->fn.vkInvalidateMappedMemoryRanges(d->handle, 1, &range));
    }

    bool                  valid()             const noexcept { return static_cast<bool>(m_buffer); }
    native::Buffer        handle()            const noexcept { return m_buffer.get(); }
    std::uint64_t         size()              const noexcept { return m_size; }
    BufferUsage           usage()             const noexcept { return m_usage; }
    MemoryUsage           memory_usage()      const noexcept { return m_memory_usage; }
    void*                 mapped()            const noexcept { return m_mapped; }
    bool                  is_mapped()         const noexcept { return m_mapped != nullptr; }
    native::DeviceAddress address()           const noexcept { return m_address; }
    const detail::DeviceState* device_state() const noexcept { return m_buffer.device(); }

    template <typename T> T* mapped_as() const noexcept { return static_cast<T*>(m_mapped); }
};

class Image;

struct ImageViewDesc {
    Format        format      = Format::Undefined;   
    ImageAspect   aspect      = ImageAspect::None;   
    std::uint32_t base_mip    = 0;
    std::uint32_t mip_count   = kRemainingMips;
    std::uint32_t base_layer  = 0;
    std::uint32_t layer_count = kRemainingLayers;
    bool          as_cube     = false;
};

class ImageView {
private:
    FIZMO_VK_UNIQUE(VkImageView, vkDestroyImageView) m_view;

public:
    ImageView() noexcept = default;
    inline Result create(const Device& device, const Image& image, const ImageViewDesc& desc = {}) noexcept;

    Result create(
        const detail::DeviceState* d, native::Image image, VkImageViewType type, Format format,
        ImageAspect aspect, std::uint32_t base_mip, std::uint32_t mips,
        std::uint32_t base_layer, std::uint32_t layers
    ) noexcept {
        m_view.reset();
        auto info = detail::make<VkImageViewCreateInfo>(VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO);
        info.image    = image;
        info.viewType = type;
        info.format   = to_vk(format);
        info.subresourceRange.aspectMask     = to_vk(aspect);
        info.subresourceRange.baseMipLevel   = base_mip;
        info.subresourceRange.levelCount     = mips;
        info.subresourceRange.baseArrayLayer = base_layer;
        info.subresourceRange.layerCount     = layers;
        native::ImageView h = VK_NULL_HANDLE;
        VkResult r = d->fn.vkCreateImageView(d->handle, &info, nullptr, &h);
        if (r != VK_SUCCESS) return to_result(r);
        m_view = { d, h };
        return Result::Success;
    }

    void destroy() noexcept { m_view.reset(); }
    bool valid() const noexcept { return static_cast<bool>(m_view); }
    native::ImageView handle() const noexcept { return m_view.get(); }
};

struct ImageDesc {
    Extent3D      extent;
    Format        format      = Format::RGBA8Unorm;
    ImageUsage    usage       = ImageUsage::Sampled | ImageUsage::TransferDst;
    ImageType     type        = ImageType::Image2D;
    std::uint32_t mip_levels  = 1;      
    std::uint32_t array_layers = 1;     
    SampleCount   samples     = SampleCount::X1;
    const char*   name        = nullptr;
};

inline std::uint32_t full_mip_count(Extent3D e) noexcept {
    std::uint32_t largest = std::max(e.width, std::max(e.height, e.depth));
    std::uint32_t levels = 1;
    while (largest > 1) { largest >>= 1; ++levels; }
    return levels;
}

class Image {
private:
    FIZMO_VK_UNIQUE(VkDeviceMemory, vkFreeMemory) m_memory;
    FIZMO_VK_UNIQUE(VkImage, vkDestroyImage)      m_image;
    ImageView     m_view;                 
    Extent3D      m_extent;
    Format        m_format  = Format::Undefined;
    ImageUsage    m_usage   = ImageUsage::None;
    ImageType     m_type    = ImageType::Image2D;
    std::uint32_t m_mips    = 1;
    std::uint32_t m_layers  = 1;
    SampleCount   m_samples = SampleCount::X1;
    ImageLayout   m_layout  = ImageLayout::Undefined;   

public:
    Image() noexcept = default;
    Image(Image&&) noexcept = default;
    Image& operator=(Image&&) noexcept = default;

    Result create(const Device& device, const ImageDesc& desc) noexcept {
        destroy();
        if (!device.valid()) return Result::NotInitialized;
        if (desc.extent.width == 0 || desc.extent.height == 0 || desc.extent.depth == 0) return Result::InvalidArgument;
        const auto* d = device.state();
        m_type    = desc.type;
        m_layers  = desc.type == ImageType::Cube ? 6u : std::max(1u, desc.array_layers);
        m_mips    = desc.mip_levels == 0 ? full_mip_count(desc.extent) : desc.mip_levels;
        m_extent  = desc.extent;
        m_format  = desc.format;
        m_usage   = desc.usage;
        m_samples = desc.samples;
        m_layout  = ImageLayout::Undefined;
        auto info = detail::make<VkImageCreateInfo>(VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO);
        info.imageType     = desc.type == ImageType::Image3D ? VK_IMAGE_TYPE_3D : VK_IMAGE_TYPE_2D;
        info.flags         = desc.type == ImageType::Cube ? VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT : 0;
        info.format        = to_vk(desc.format);
        info.extent        = to_vk(desc.extent);
        info.mipLevels     = m_mips;
        info.arrayLayers   = m_layers;
        info.samples       = to_vk(desc.samples);
        info.tiling        = VK_IMAGE_TILING_OPTIMAL;
        info.usage         = to_vk(desc.usage);
        info.sharingMode   = VK_SHARING_MODE_EXCLUSIVE;
        info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        native::Image img = VK_NULL_HANDLE;
        VkResult r = d->fn.vkCreateImage(d->handle, &info, nullptr, &img);
        if (r != VK_SUCCESS) return to_result(r);
        m_image = { d, img };
        VkMemoryRequirements req{};
        d->fn.vkGetImageMemoryRequirements(d->handle, img, &req);
        native::DeviceMemory mem = VK_NULL_HANDLE;
        VkMemoryPropertyFlags flags = 0;
        Result res = detail::allocate_memory(d, req, MemoryUsage::GpuOnly, false, mem, flags);
        if (failed(res)) { destroy(); return res; }
        m_memory = { d, mem };
        r = d->fn.vkBindImageMemory(d->handle, img, mem, 0);
        if (r != VK_SUCCESS) { destroy(); return to_result(r); }
        res = m_view.create(device, *this);
        if (failed(res)) { destroy(); return res; }
        if (desc.name) d->set_name(VK_OBJECT_TYPE_IMAGE, reinterpret_cast<std::uint64_t>(img), desc.name);
        return Result::Success;
    }

    void destroy() noexcept {
        m_view.destroy();
        m_image.reset();
        m_memory.reset();
        m_layout = ImageLayout::Undefined;
    }

    bool             valid()   const noexcept { return static_cast<bool>(m_image); }
    native::Image    handle()  const noexcept { return m_image.get(); }
    const ImageView& view()    const noexcept { return m_view; }
    Extent3D         extent()  const noexcept { return m_extent; }
    Extent2D         extent2d() const noexcept { return { m_extent.width, m_extent.height }; }
    Format           format()  const noexcept { return m_format; }
    ImageUsage       usage()   const noexcept { return m_usage; }
    ImageType        type()    const noexcept { return m_type; }
    ImageAspect      aspect()  const noexcept { return aspect_of(m_format); }
    std::uint32_t    mip_levels()   const noexcept { return m_mips; }
    std::uint32_t    array_layers() const noexcept { return m_layers; }
    SampleCount      samples() const noexcept { return m_samples; }
    ImageLayout      layout()  const noexcept { return m_layout; }
    void             set_layout(ImageLayout l) noexcept { m_layout = l; }
    const detail::DeviceState* device_state() const noexcept { return m_image.device(); }
};

inline Result ImageView::create(const Device& device, const Image& image, const ImageViewDesc& desc) noexcept {
    if (!device.valid() || !image.valid()) return Result::NotInitialized;
    VkImageViewType type = VK_IMAGE_VIEW_TYPE_2D;
    const std::uint32_t layers = desc.layer_count == kRemainingLayers ? image.array_layers() - desc.base_layer : desc.layer_count;
    if (image.type() == ImageType::Image3D)                          type = VK_IMAGE_VIEW_TYPE_3D;
    else if (image.type() == ImageType::Cube && (desc.as_cube || layers == 6)) type = VK_IMAGE_VIEW_TYPE_CUBE;
    else if (layers > 1)                                             type = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
    const Format fmt = desc.format == Format::Undefined ? image.format() : desc.format;
    const ImageAspect aspect = desc.aspect == ImageAspect::None ? aspect_of(fmt) : desc.aspect;
    return create(device.state(), image.handle(), type, fmt, aspect, desc.base_mip, desc.mip_count, desc.base_layer, desc.layer_count);
}

struct SamplerDesc {
    Filter      mag_filter = Filter::Linear;
    Filter      min_filter = Filter::Linear;
    MipmapMode  mipmap     = MipmapMode::Linear;
    AddressMode address_u  = AddressMode::Repeat;
    AddressMode address_v  = AddressMode::Repeat;
    AddressMode address_w  = AddressMode::Repeat;
    float       max_anisotropy = 0.0f;        
    bool        compare_enable = false;       
    CompareOp   compare_op = CompareOp::LessEqual;
    float       min_lod = 0.0f;
    float       max_lod = VK_LOD_CLAMP_NONE;
    const char* name = nullptr;

    static SamplerDesc linear_repeat() noexcept { return {}; }

    static SamplerDesc linear_clamp() noexcept {
        SamplerDesc s;
        s.address_u = s.address_v = s.address_w = AddressMode::ClampToEdge;
        return s;
    }

    static SamplerDesc nearest_clamp() noexcept {      
        SamplerDesc s = linear_clamp();
        s.mag_filter = s.min_filter = Filter::Nearest;
        s.mipmap = MipmapMode::Nearest;
        return s;
    }
};

class Sampler {
private:
    FIZMO_VK_UNIQUE(VkSampler, vkDestroySampler) m_sampler;

public:
    Sampler() noexcept = default;

    Result create(const Device& device, const SamplerDesc& desc = {}) noexcept {
        m_sampler.reset();
        if (!device.valid()) return Result::NotInitialized;
        const auto* d = device.state();
        auto info = detail::make<VkSamplerCreateInfo>(VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO);
        info.magFilter    = to_vk(desc.mag_filter);
        info.minFilter    = to_vk(desc.min_filter);
        info.mipmapMode   = to_vk(desc.mipmap);
        info.addressModeU = to_vk(desc.address_u);
        info.addressModeV = to_vk(desc.address_v);
        info.addressModeW = to_vk(desc.address_w);
        info.minLod       = desc.min_lod;
        info.maxLod       = desc.max_lod;
        info.borderColor  = VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK;
        info.compareEnable = desc.compare_enable ? VK_TRUE : VK_FALSE;
        info.compareOp     = to_vk(desc.compare_op);

        if (desc.max_anisotropy > 1.0f && d->features.sampler_anisotropy) {
            info.anisotropyEnable = VK_TRUE;
            info.maxAnisotropy    = std::min(desc.max_anisotropy, d->properties.limits.maxSamplerAnisotropy);
        }

        native::Sampler h = VK_NULL_HANDLE;
        VkResult r = d->fn.vkCreateSampler(d->handle, &info, nullptr, &h);
        if (r != VK_SUCCESS) return to_result(r);
        m_sampler = { d, h };
        if (desc.name) d->set_name(VK_OBJECT_TYPE_SAMPLER, reinterpret_cast<std::uint64_t>(h), desc.name);
        return Result::Success;
    }

    void destroy() noexcept { m_sampler.reset(); }
    bool valid() const noexcept { return static_cast<bool>(m_sampler); }
    native::Sampler handle() const noexcept { return m_sampler.get(); }
};

} // namespace vulkan
} // namespace fizmo

#endif // FIZMO_VULKAN_RESOURCES_HPP