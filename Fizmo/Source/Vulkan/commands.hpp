#ifndef FIZMO_VULKAN_COMMANDS_HPP
#define FIZMO_VULKAN_COMMANDS_HPP

#include "pipeline.hpp"

namespace fizmo {
namespace vulkan {

struct ImageBarrier {
    native::Image image      = VK_NULL_HANDLE;
    ImageLayout   old_layout = ImageLayout::Undefined;
    ImageLayout   new_layout = ImageLayout::Undefined;
    PipelineStage src_stage  = PipelineStage::AllCommands;
    Access        src_access = Access::MemoryWrite;
    PipelineStage dst_stage  = PipelineStage::AllCommands;
    Access        dst_access = Access::MemoryRead | Access::MemoryWrite;
    ImageAspect   aspect     = ImageAspect::Color;
    std::uint32_t base_mip   = 0;
    std::uint32_t mip_count  = kRemainingMips;
    std::uint32_t base_layer = 0;
    std::uint32_t layer_count = kRemainingLayers;
};

struct BufferBarrier {
    native::Buffer buffer    = VK_NULL_HANDLE;
    PipelineStage src_stage  = PipelineStage::AllCommands;
    Access        src_access = Access::MemoryWrite;
    PipelineStage dst_stage  = PipelineStage::AllCommands;
    Access        dst_access = Access::MemoryRead;
    std::uint64_t offset     = 0;
    std::uint64_t size       = kWholeSize;
};

namespace detail {

struct StageAccess { PipelineStage stage; Access access; };

inline StageAccess scope_for(ImageLayout layout, bool as_source) noexcept {
    switch (layout) {
        case ImageLayout::Undefined:
            return { as_source ? PipelineStage::AllCommands : PipelineStage::TopOfPipe, Access::None };
        case ImageLayout::Present:
            return { as_source ? PipelineStage::AllCommands : PipelineStage::BottomOfPipe, Access::None };
        case ImageLayout::ColorAttachment:
            return { PipelineStage::ColorAttachmentOutput, Access::ColorAttachmentRead | Access::ColorAttachmentWrite };
        case ImageLayout::DepthAttachment:
            return { PipelineStage::EarlyFragmentTests | PipelineStage::LateFragmentTests,
                     Access::DepthAttachmentRead | Access::DepthAttachmentWrite };
        case ImageLayout::DepthReadOnly:
            return { PipelineStage::EarlyFragmentTests | PipelineStage::LateFragmentTests | PipelineStage::FragmentShader,
                     Access::DepthAttachmentRead | Access::ShaderRead };
        case ImageLayout::ShaderReadOnly:
            return { PipelineStage::VertexShader | PipelineStage::FragmentShader | PipelineStage::ComputeShader, Access::ShaderRead };
        case ImageLayout::TransferSrc:
            return { PipelineStage::Transfer, Access::TransferRead };
        case ImageLayout::TransferDst:
            return { PipelineStage::Transfer, Access::TransferWrite };
        case ImageLayout::General:
        default:
            return { PipelineStage::AllCommands, Access::MemoryRead | Access::MemoryWrite };
    }
}

} // namespace detail

struct ColorAttachment {
    const ImageView* view    = nullptr;
    ImageLayout      layout  = ImageLayout::ColorAttachment;
    LoadOp           load    = LoadOp::Clear;
    StoreOp          store   = StoreOp::Store;
    ClearColor       clear;
    const ImageView* resolve = nullptr;  
};

struct DepthAttachment {
    const ImageView* view   = nullptr;
    ImageLayout      layout = ImageLayout::DepthAttachment;
    LoadOp           load   = LoadOp::Clear;
    StoreOp          store  = StoreOp::DontCare;
    ClearDepth       clear;
    bool             has_stencil = false;
    const ImageView* resolve = nullptr;
};

struct RenderingDesc {
    Rect2D                 area;
    Span<ColorAttachment>  colors;
    const DepthAttachment* depth = nullptr;
};

class CommandBuffer {
private:
    const detail::DeviceState* m_device = nullptr;
    native::CommandBuffer      m_cmd    = VK_NULL_HANDLE;

    const DeviceDispatch& fn() const noexcept { return m_device->fn; }

public:
    CommandBuffer() noexcept = default;
    CommandBuffer(const detail::DeviceState* d, native::CommandBuffer c) noexcept : m_device(d), m_cmd(c) {}

    bool valid() const noexcept { return m_cmd != VK_NULL_HANDLE; }
    native::CommandBuffer handle() const noexcept { return m_cmd; }
    const detail::DeviceState* device_state() const noexcept { return m_device; }

    Result begin(bool one_time = true) const noexcept {
        auto info = detail::make<VkCommandBufferBeginInfo>(VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO);
        info.flags = one_time ? VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT : 0;
        return to_result(fn().vkBeginCommandBuffer(m_cmd, &info));
    }

    Result end()   const noexcept { return to_result(fn().vkEndCommandBuffer(m_cmd)); }
    Result reset() const noexcept { return to_result(fn().vkResetCommandBuffer(m_cmd, 0)); }

    void begin_rendering(const RenderingDesc& desc) const noexcept {
        constexpr std::size_t kMaxColor = 8;
        VkRenderingAttachmentInfo colors[kMaxColor]{};
        const std::uint32_t n = static_cast<std::uint32_t>(std::min(desc.colors.size(), kMaxColor));
        
        for (std::uint32_t i = 0; i < n; ++i) {
            const auto& c = desc.colors[i];
            colors[i] = detail::make<VkRenderingAttachmentInfo>(VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO);
            colors[i].imageView   = c.view ? c.view->handle() : VK_NULL_HANDLE;
            colors[i].imageLayout = to_vk(c.layout);
            colors[i].loadOp      = to_vk(c.load);
            colors[i].storeOp     = to_vk(c.store);
            colors[i].clearValue  = to_vk(c.clear);

            if (c.resolve) {
                colors[i].resolveMode        = VK_RESOLVE_MODE_AVERAGE_BIT;
                colors[i].resolveImageView   = c.resolve->handle();
                colors[i].resolveImageLayout = to_vk(c.layout);
            }
        }

        auto depth = detail::make<VkRenderingAttachmentInfo>(VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO);
        
        if (desc.depth) {
            depth.imageView   = desc.depth->view ? desc.depth->view->handle() : VK_NULL_HANDLE;
            depth.imageLayout = to_vk(desc.depth->layout);
            depth.loadOp      = to_vk(desc.depth->load);
            depth.storeOp     = to_vk(desc.depth->store);
            depth.clearValue  = to_vk(desc.depth->clear);

            if (desc.depth->resolve) {
                depth.resolveMode        = VK_RESOLVE_MODE_SAMPLE_ZERO_BIT;
                depth.resolveImageView   = desc.depth->resolve->handle();
                depth.resolveImageLayout = to_vk(desc.depth->layout);
            }
        }

        auto info = detail::make<VkRenderingInfo>(VK_STRUCTURE_TYPE_RENDERING_INFO);
        info.renderArea           = to_vk(desc.area);
        info.layerCount           = 1;
        info.colorAttachmentCount = n;
        info.pColorAttachments    = colors;
        info.pDepthAttachment     = desc.depth ? &depth : nullptr;
        info.pStencilAttachment   = (desc.depth && desc.depth->has_stencil) ? &depth : nullptr;
        fn().vkCmdBeginRendering(m_cmd, &info);
    }

    void end_rendering() const noexcept { fn().vkCmdEndRendering(m_cmd); }

    void bind(const Pipeline& pipeline) const noexcept {
        fn().vkCmdBindPipeline(m_cmd, to_vk(pipeline.bind_point()), pipeline.handle());
    }

    void set_viewport(const Viewport& vp) const noexcept {
        const VkViewport v = to_vk(vp);
        fn().vkCmdSetViewport(m_cmd, 0, 1, &v);
    }

    void set_scissor(const Rect2D& rect) const noexcept {
        const VkRect2D r = to_vk(rect);
        fn().vkCmdSetScissor(m_cmd, 0, 1, &r);
    }

    void set_viewport_and_scissor(Extent2D size) const noexcept {
        set_viewport({ 0.0f, 0.0f, static_cast<float>(size.width), static_cast<float>(size.height), 0.0f, 1.0f });
        set_scissor({ { 0, 0 }, size });
    }

    void bind_vertex_buffer(std::uint32_t binding, const Buffer& buffer, std::uint64_t offset = 0) const noexcept {
        native::Buffer b = buffer.handle();
        fn().vkCmdBindVertexBuffers(m_cmd, binding, 1, &b, &offset);
    }

    void bind_index_buffer(const Buffer& buffer, IndexType type = IndexType::UInt32, std::uint64_t offset = 0) const noexcept {
        fn().vkCmdBindIndexBuffer(m_cmd, buffer.handle(), offset, to_vk(type));
    }

    void bind_descriptor_set(
        const Pipeline& pipeline, std::uint32_t set_index, const DescriptorSet& set,
        Span<std::uint32_t> dynamic_offsets = {}
    ) const noexcept {
        native::DescriptorSet s = set.handle();

        fn().vkCmdBindDescriptorSets(
            m_cmd, to_vk(pipeline.bind_point()), 
            pipeline.layout(), set_index, 1, &s,
            dynamic_offsets.count(), dynamic_offsets.data()
        );
    }

    void push_constants(
        const Pipeline& pipeline, ShaderStage stages, const void* data,
        std::uint32_t size, std::uint32_t offset = 0
    ) const noexcept {
        fn().vkCmdPushConstants(m_cmd, pipeline.layout(), to_vk(stages), offset, size, data);
    }

    template <typename T>
    void push_constants(
        const Pipeline& pipeline, const T& value, ShaderStage stages = ShaderStage::All,
        std::uint32_t offset = 0
    ) const noexcept {
        static_assert(std::is_trivially_copyable<T>::value, "push constant type must be trivially copyable");
        push_constants(pipeline, stages, &value, static_cast<std::uint32_t>(sizeof(T)), offset);
    }

    void draw(
        std::uint32_t vertex_count, std::uint32_t instance_count = 1,
        std::uint32_t first_vertex = 0, std::uint32_t first_instance = 0
    ) const noexcept {
        fn().vkCmdDraw(m_cmd, vertex_count, instance_count, first_vertex, first_instance);
    }

    void draw_indexed(
        std::uint32_t index_count, std::uint32_t instance_count = 1, std::uint32_t first_index = 0,
        std::int32_t vertex_offset = 0, std::uint32_t first_instance = 0
    ) const noexcept {
        fn().vkCmdDrawIndexed(m_cmd, index_count, instance_count, first_index, vertex_offset, first_instance);
    }

    void draw_indirect(const Buffer& args, std::uint64_t offset, std::uint32_t draw_count, std::uint32_t stride) const noexcept {
        fn().vkCmdDrawIndirect(m_cmd, args.handle(), offset, draw_count, stride);
    }

    void draw_indexed_indirect(const Buffer& args, std::uint64_t offset, std::uint32_t draw_count, std::uint32_t stride) const noexcept {
        fn().vkCmdDrawIndexedIndirect(m_cmd, args.handle(), offset, draw_count, stride);
    }

    void dispatch(std::uint32_t x, std::uint32_t y = 1, std::uint32_t z = 1) const noexcept {
        fn().vkCmdDispatch(m_cmd, x, y, z);
    }

    void dispatch_indirect(const Buffer& args, std::uint64_t offset = 0) const noexcept {
        fn().vkCmdDispatchIndirect(m_cmd, args.handle(), offset);
    }

    void copy_buffer(
        const Buffer& src, const Buffer& dst, std::uint64_t size,
        std::uint64_t src_offset = 0, std::uint64_t dst_offset = 0
    ) const noexcept {
        const VkBufferCopy region{ src_offset, dst_offset, size };
        fn().vkCmdCopyBuffer(m_cmd, src.handle(), dst.handle(), 1, &region);
    }

    void copy_buffer_to_image(
        const Buffer& src, const Image& dst, std::uint32_t mip = 0,
        std::uint32_t base_layer = 0, std::uint32_t layer_count = 1,
        std::uint64_t src_offset = 0
    ) const noexcept {
        VkBufferImageCopy region{};
        region.bufferOffset = src_offset;
        region.imageSubresource = { to_vk(dst.aspect()), mip, base_layer, layer_count };
        const Extent3D e = dst.extent();
        region.imageExtent = { std::max(1u, e.width >> mip), std::max(1u, e.height >> mip), std::max(1u, e.depth >> mip) };
        fn().vkCmdCopyBufferToImage(m_cmd, src.handle(), dst.handle(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
    }

    void copy_buffer_to_image_region(
        const Buffer& src, const Image& dst, std::uint64_t src_offset,
        Offset2D offset, Extent2D extent,
        std::uint32_t mip = 0, std::uint32_t layer = 0
    ) const noexcept {
        VkBufferImageCopy region{};
        region.bufferOffset      = src_offset;
        region.imageSubresource  = { to_vk(dst.aspect()), mip, layer, 1 };
        region.imageOffset       = { offset.x, offset.y, 0 };
        region.imageExtent       = { extent.width, extent.height, 1 };
        fn().vkCmdCopyBufferToImage(m_cmd, src.handle(), dst.handle(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
    }

    void copy_image_to_buffer(
        const Image& src, const Buffer& dst, std::uint32_t mip = 0,
        std::uint32_t layer = 0, std::uint64_t dst_offset = 0
    ) const noexcept {
        VkBufferImageCopy region{};
        region.bufferOffset = dst_offset;
        region.imageSubresource = { to_vk(src.aspect()), mip, layer, 1 };
        const Extent3D e = src.extent();
        region.imageExtent = { std::max(1u, e.width >> mip), std::max(1u, e.height >> mip), std::max(1u, e.depth >> mip) };
        fn().vkCmdCopyImageToBuffer(m_cmd, src.handle(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, dst.handle(), 1, &region);
    }

    void blit(
        native::Image src, Rect2D src_rect, native::Image dst, Rect2D dst_rect,
        Filter filter = Filter::Linear, std::uint32_t src_mip = 0, std::uint32_t dst_mip = 0
    ) const noexcept {
        VkImageBlit region{};
        region.srcSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, src_mip, 0, 1 };
        region.srcOffsets[0]  = { src_rect.offset.x, src_rect.offset.y, 0 };
        region.srcOffsets[1]  = { src_rect.offset.x + static_cast<std::int32_t>(src_rect.extent.width),
                                  src_rect.offset.y + static_cast<std::int32_t>(src_rect.extent.height), 1 };
        region.dstSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, dst_mip, 0, 1 };
        region.dstOffsets[0]  = { dst_rect.offset.x, dst_rect.offset.y, 0 };
        region.dstOffsets[1]  = { dst_rect.offset.x + static_cast<std::int32_t>(dst_rect.extent.width),
                                  dst_rect.offset.y + static_cast<std::int32_t>(dst_rect.extent.height), 1 };
        fn().vkCmdBlitImage(m_cmd, src, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, dst, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region, to_vk(filter));
    }

    void copy_image(const Image& src, const Image& dst) const noexcept {
        VkImageCopy region{};
        const VkImageAspectFlags aspect = to_vk(src.aspect());
        region.srcSubresource = { aspect, 0, 0, 1 };
        region.dstSubresource = { aspect, 0, 0, 1 };
        const Extent3D e = src.extent();
        region.extent = { e.width, e.height, e.depth };
        fn().vkCmdCopyImage(m_cmd, src.handle(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, dst.handle(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
    }

    void clear_color(native::Image image, const ClearColor& color, ImageLayout layout = ImageLayout::TransferDst) const noexcept {
        const VkClearColorValue value = to_vk(color).color;
        const VkImageSubresourceRange range{ VK_IMAGE_ASPECT_COLOR_BIT, 0, VK_REMAINING_MIP_LEVELS, 0, VK_REMAINING_ARRAY_LAYERS };
        fn().vkCmdClearColorImage(m_cmd, image, to_vk(layout), &value, 1, &range);
    }

    void fill_buffer(const Buffer& buffer, std::uint32_t value, std::uint64_t offset = 0, std::uint64_t size = kWholeSize) const noexcept {
        fn().vkCmdFillBuffer(m_cmd, buffer.handle(), offset, size, value);
    }

    void barrier(const ImageBarrier& b) const noexcept {
        auto ib = detail::make<VkImageMemoryBarrier2>(VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2);
        ib.srcStageMask  = to_vk(b.src_stage);
        ib.srcAccessMask = to_vk(b.src_access);
        ib.dstStageMask  = to_vk(b.dst_stage);
        ib.dstAccessMask = to_vk(b.dst_access);
        ib.oldLayout     = to_vk(b.old_layout);
        ib.newLayout     = to_vk(b.new_layout);
        ib.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        ib.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        ib.image = b.image;
        ib.subresourceRange = { to_vk(b.aspect), b.base_mip, b.mip_count, b.base_layer, b.layer_count };
        auto dep = detail::make<VkDependencyInfo>(VK_STRUCTURE_TYPE_DEPENDENCY_INFO);
        dep.imageMemoryBarrierCount = 1;
        dep.pImageMemoryBarriers    = &ib;
        fn().vkCmdPipelineBarrier2(m_cmd, &dep);
    }

    void barrier(const BufferBarrier& b) const noexcept {
        auto bb = detail::make<VkBufferMemoryBarrier2>(VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2);
        bb.srcStageMask  = to_vk(b.src_stage);
        bb.srcAccessMask = to_vk(b.src_access);
        bb.dstStageMask  = to_vk(b.dst_stage);
        bb.dstAccessMask = to_vk(b.dst_access);
        bb.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        bb.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        bb.buffer = b.buffer;
        bb.offset = b.offset;
        bb.size   = b.size;
        auto dep = detail::make<VkDependencyInfo>(VK_STRUCTURE_TYPE_DEPENDENCY_INFO);
        dep.bufferMemoryBarrierCount = 1;
        dep.pBufferMemoryBarriers    = &bb;
        fn().vkCmdPipelineBarrier2(m_cmd, &dep);
    }

    void memory_barrier(PipelineStage src_stage, Access src_access, PipelineStage dst_stage, Access dst_access) const noexcept {
        auto mb = detail::make<VkMemoryBarrier2>(VK_STRUCTURE_TYPE_MEMORY_BARRIER_2);
        mb.srcStageMask  = to_vk(src_stage);
        mb.srcAccessMask = to_vk(src_access);
        mb.dstStageMask  = to_vk(dst_stage);
        mb.dstAccessMask = to_vk(dst_access);
        auto dep = detail::make<VkDependencyInfo>(VK_STRUCTURE_TYPE_DEPENDENCY_INFO);
        dep.memoryBarrierCount = 1;
        dep.pMemoryBarriers    = &mb;
        fn().vkCmdPipelineBarrier2(m_cmd, &dep);
    }

    void transition(native::Image image, ImageLayout from, ImageLayout to, ImageAspect aspect = ImageAspect::Color) const noexcept {
        const auto src = detail::scope_for(from, true);
        const auto dst = detail::scope_for(to, false);
        ImageBarrier b;
        b.image = image; b.old_layout = from; b.new_layout = to; b.aspect = aspect;
        b.src_stage = src.stage; 
        b.src_access = src.access & (Access::ColorAttachmentWrite | Access::DepthAttachmentWrite | Access::TransferWrite | Access::ShaderWrite | Access::MemoryWrite);
        b.dst_stage = dst.stage; b.dst_access = dst.access;
        barrier(b);
    }

    void transition(Image& image, ImageLayout to) const noexcept {
        if (image.layout() == to) return;
        transition(image.handle(), image.layout(), to, image.aspect());
        image.set_layout(to);
    }

    void generate_mipmaps(Image& image, ImageLayout final_layout = ImageLayout::ShaderReadOnly) const noexcept {
        std::int32_t w = static_cast<std::int32_t>(image.extent().width);
        std::int32_t h = static_cast<std::int32_t>(image.extent().height);
        const std::uint32_t mips = image.mip_levels();

        for (std::uint32_t i = 1; i < mips; ++i) {
            ImageBarrier to_src;
            to_src.image = image.handle(); to_src.base_mip = i - 1; to_src.mip_count = 1;
            to_src.old_layout = ImageLayout::TransferDst; to_src.new_layout = ImageLayout::TransferSrc;
            to_src.src_stage = PipelineStage::Transfer;   to_src.src_access = Access::TransferWrite;
            to_src.dst_stage = PipelineStage::Transfer;   to_src.dst_access = Access::TransferRead;
            barrier(to_src);
            const std::int32_t nw = std::max(1, w / 2), nh = std::max(1, h / 2);

            blit(
                image.handle(), { { 0, 0 }, { std::uint32_t(w), std::uint32_t(h) } },
                image.handle(), { { 0, 0 }, { std::uint32_t(nw), std::uint32_t(nh) } }, 
                Filter::Linear, i - 1, i
            );

            ImageBarrier done;
            done.image = image.handle(); done.base_mip = i - 1; done.mip_count = 1;
            done.old_layout = ImageLayout::TransferSrc; done.new_layout = final_layout;
            const auto dst = detail::scope_for(final_layout, false);
            done.src_stage = PipelineStage::Transfer; done.src_access = Access::None;
            done.dst_stage = dst.stage;               done.dst_access = dst.access;
            barrier(done);
            w = nw; h = nh;
        }

        const auto dst = detail::scope_for(final_layout, false);
        ImageBarrier last;
        last.image = image.handle(); last.base_mip = mips - 1; last.mip_count = 1;
        last.old_layout = ImageLayout::TransferDst; last.new_layout = final_layout;
        last.src_stage = PipelineStage::Transfer; last.src_access = Access::TransferWrite;
        last.dst_stage = dst.stage;               last.dst_access = dst.access;
        barrier(last);
        image.set_layout(final_layout);
    }

    void begin_label(const char* name, ClearColor color = { 1, 1, 1, 1 }) const noexcept {
        if (!m_device->instance->fn.vkCmdBeginDebugUtilsLabelEXT) return;
        auto label = detail::make<VkDebugUtilsLabelEXT>(VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT);
        label.pLabelName = name;
        label.color[0] = color.r; label.color[1] = color.g; label.color[2] = color.b; label.color[3] = color.a;
        m_device->instance->fn.vkCmdBeginDebugUtilsLabelEXT(m_cmd, &label);
    }

    void end_label() const noexcept {
        if (m_device->instance->fn.vkCmdEndDebugUtilsLabelEXT) m_device->instance->fn.vkCmdEndDebugUtilsLabelEXT(m_cmd);
    }
};

class CommandPool {
private:
    FIZMO_VK_UNIQUE(VkCommandPool, vkDestroyCommandPool) m_pool;

public:
    CommandPool() noexcept = default;

    Result create(const Device& device, QueueType queue = QueueType::Graphics, bool resettable = true) noexcept {
        return create(device, device.queue(queue).family(), resettable);
    }

    Result create(const Device& device, std::uint32_t queue_family, bool resettable = true) noexcept {
        m_pool.reset();
        if (!device.valid()) return Result::NotInitialized;
        const auto* d = device.state();
        auto info = detail::make<VkCommandPoolCreateInfo>(VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO);
        info.flags            = resettable ? VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT : 0;
        info.queueFamilyIndex = queue_family;
        native::CommandPool h = VK_NULL_HANDLE;
        VkResult r = d->fn.vkCreateCommandPool(d->handle, &info, nullptr, &h);
        if (r != VK_SUCCESS) return to_result(r);
        m_pool = { d, h };
        return Result::Success;
    }

    Result allocate(CommandBuffer& out) noexcept {
        auto* d = m_pool.device();
        if (!d) return Result::NotInitialized;
        auto info = detail::make<VkCommandBufferAllocateInfo>(VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO);
        info.commandPool        = m_pool.get();
        info.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        info.commandBufferCount = 1;
        native::CommandBuffer c = VK_NULL_HANDLE;
        VkResult r = d->fn.vkAllocateCommandBuffers(d->handle, &info, &c);
        if (r != VK_SUCCESS) return to_result(r);
        out = CommandBuffer(d, c);
        return Result::Success;
    }

    Result allocate(std::uint32_t count, std::vector<CommandBuffer>& out) {
        auto* d = m_pool.device();
        if (!d) return Result::NotInitialized;
        std::vector<native::CommandBuffer> raw(count);
        auto info = detail::make<VkCommandBufferAllocateInfo>(VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO);
        info.commandPool        = m_pool.get();
        info.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        info.commandBufferCount = count;
        VkResult r = d->fn.vkAllocateCommandBuffers(d->handle, &info, raw.data());
        if (r != VK_SUCCESS) return to_result(r);
        out.clear();
        for (auto c : raw) out.emplace_back(d, c);
        return Result::Success;
    }

    Result reset() noexcept {
        auto* d = m_pool.device();
        if (!d) return Result::NotInitialized;
        return to_result(d->fn.vkResetCommandPool(d->handle, m_pool.get(), 0));
    }

    void destroy() noexcept { m_pool.reset(); }
    bool valid() const noexcept { return static_cast<bool>(m_pool); }
    native::CommandPool handle() const noexcept { return m_pool.get(); }
};

inline Result Queue::submit(
    Span<CommandBuffer> command_buffers, Span<SemaphoreWait> waits,
    Span<SemaphoreSignal> signals, const Fence* fence
) const noexcept {
    if (!m_device) return Result::NotInitialized;
    constexpr std::size_t kMax = 16;
    if (command_buffers.size() > kMax || waits.size() > kMax || signals.size() > kMax) return Result::InvalidArgument;
    VkCommandBufferSubmitInfo cmd_infos[kMax]{};

    for (std::size_t i = 0; i < command_buffers.size(); ++i) {
        cmd_infos[i] = detail::make<VkCommandBufferSubmitInfo>(VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO);
        cmd_infos[i].commandBuffer = command_buffers[i].handle();
    }

    auto fill = [](VkSemaphoreSubmitInfo* out, Span<SemaphoreWait> list) {
        for (std::size_t i = 0; i < list.size(); ++i) {
            out[i] = detail::make<VkSemaphoreSubmitInfo>(VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO);
            out[i].semaphore = list[i].semaphore ? list[i].semaphore->handle() : VK_NULL_HANDLE;
            out[i].stageMask = to_vk(list[i].stage);
            out[i].value     = list[i].value;
        }
    };

    VkSemaphoreSubmitInfo wait_infos[kMax]{};
    VkSemaphoreSubmitInfo signal_infos[kMax]{};
    fill(wait_infos, waits);
    fill(signal_infos, signals);
    auto submit = detail::make<VkSubmitInfo2>(VK_STRUCTURE_TYPE_SUBMIT_INFO_2);
    submit.commandBufferInfoCount   = command_buffers.count();
    submit.pCommandBufferInfos      = cmd_infos;
    submit.waitSemaphoreInfoCount   = waits.count();
    submit.pWaitSemaphoreInfos      = wait_infos;
    submit.signalSemaphoreInfoCount = signals.count();
    submit.pSignalSemaphoreInfos    = signal_infos;
    return to_result(m_device->fn.vkQueueSubmit2(m_handle, 1, &submit, fence ? fence->handle() : VK_NULL_HANDLE));
}

template <typename F>
Result submit_immediate(Device& device, F&& record) {
    auto* d = device.state();
    if (!d) return Result::NotInitialized;
    std::lock_guard<std::mutex> lock(d->immediate_mutex);
    CommandBuffer cmd(d, d->immediate_cmd);
    Result r = cmd.reset();
    if (failed(r)) return r;
    r = cmd.begin(true);
    if (failed(r)) return r;
    record(cmd);
    r = cmd.end();
    if (failed(r)) return r;
    auto ci = detail::make<VkCommandBufferSubmitInfo>(VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO);
    ci.commandBuffer = d->immediate_cmd;
    auto submit = detail::make<VkSubmitInfo2>(VK_STRUCTURE_TYPE_SUBMIT_INFO_2);
    submit.commandBufferInfoCount = 1;
    submit.pCommandBufferInfos    = &ci;
    r = to_result(d->fn.vkQueueSubmit2(d->graphics.handle, 1, &submit, d->immediate_fence));
    if (failed(r)) return r;
    r = to_result(d->fn.vkWaitForFences(d->handle, 1, &d->immediate_fence, VK_TRUE, kNoTimeout));
    d->fn.vkResetFences(d->handle, 1, &d->immediate_fence);
    return r;
}

inline Result upload(Device& device, Buffer& dst, const void* data, std::uint64_t size, std::uint64_t dst_offset = 0) {
    if (!dst.valid() || !data) return Result::InvalidArgument;
    if (dst.is_mapped()) return dst.write(data, size, dst_offset);
    Buffer staging;
    Result r = staging.create(device, { size, BufferUsage::TransferSrc, MemoryUsage::CpuToGpu, "fizmo staging" });
    if (failed(r)) return r;
    r = staging.write(data, size);
    if (failed(r)) return r;

    return submit_immediate(device, [&](CommandBuffer& cmd) {
        cmd.copy_buffer(staging, dst, size, 0, dst_offset);
    });
}

template <typename T>
Result upload(Device& device, Buffer& dst, const std::vector<T>& items, std::uint64_t dst_offset = 0) {
    return upload(device, dst, items.data(), items.size() * sizeof(T), dst_offset);
}

inline Result upload(
    Device& device, Image& dst, const void* pixels, std::uint64_t bytes,
    ImageLayout final_layout = ImageLayout::ShaderReadOnly, bool build_mips = true
) {
    if (!dst.valid() || !pixels || bytes == 0) return Result::InvalidArgument;
    const bool mips = build_mips && dst.mip_levels() > 1;

    if (mips) {
        if (!has_flag(dst.usage(), ImageUsage::TransferSrc)) return Result::InvalidArgument;
        const PhysicalDevice gpu = device.physical_device();
        if (!gpu.supports_format(dst.format(), VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT | VK_FORMAT_FEATURE_BLIT_SRC_BIT | VK_FORMAT_FEATURE_BLIT_DST_BIT)) return Result::FormatNotSupported;
    }

    Buffer staging;
    Result r = staging.create(device, { bytes, BufferUsage::TransferSrc, MemoryUsage::CpuToGpu, "fizmo image staging" });
    if (failed(r)) return r;
    r = staging.write(pixels, bytes);
    if (failed(r)) return r;

    return submit_immediate(device, [&](CommandBuffer& cmd) {
        cmd.transition(dst, ImageLayout::TransferDst);
        cmd.copy_buffer_to_image(staging, dst, 0, 0, dst.array_layers());

        if (mips) cmd.generate_mipmaps(dst, final_layout);
        else      cmd.transition(dst, final_layout);
    });
}

inline Result download(Device& device, const Buffer& src, void* out, std::uint64_t size, std::uint64_t src_offset = 0) {
    if (!src.valid() || !out) return Result::InvalidArgument;
    if (src.is_mapped()) return src.read(out, size, src_offset);
    Buffer readback;
    Result r = readback.create(device, { size, BufferUsage::TransferDst, MemoryUsage::GpuToCpu, "fizmo readback" });
    if (failed(r)) return r;
    r = submit_immediate(device, [&](CommandBuffer& cmd) { cmd.copy_buffer(src, readback, size, src_offset, 0); });
    if (failed(r)) return r;
    return readback.read(out, size);
}

} // namespace vulkan
} // namespace fizmo

#endif // FIZMO_VULKAN_COMMANDS_HPP