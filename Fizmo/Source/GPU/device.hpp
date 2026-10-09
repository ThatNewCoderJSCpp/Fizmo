#ifndef FIZMO_GPU_DEVICE_HPP
#define FIZMO_GPU_DEVICE_HPP

#include "backend.hpp"

namespace fizmo {
namespace gpu {

class Device;

namespace detail {

inline std::shared_ptr<BackendDevice> create_backend(const DeviceInput& input, Result& result, std::string& error);

} // namespace detail

class Buffer {
private:
    std::shared_ptr<detail::BufferImpl> m_impl;

public:
    Buffer() noexcept = default;

    inline Result create(Device& device, const BufferDesc& desc);
    void destroy() noexcept { m_impl.reset(); }

    Result write(const void* data, std::uint64_t bytes, std::uint64_t offset = 0) const {
        if (!m_impl) return Result::NotInitialized;
        if (!data || offset + bytes > m_impl->desc.size) return Result::InvalidArgument;
        return bytes ? m_impl->write(data, bytes, offset) : Result::Success;
    }

    template <typename T>
    Result write(const std::vector<T>& items, std::uint64_t offset = 0) const { return write(items.data(), items.size() * sizeof(T), offset); }

    Result read(void* out, std::uint64_t bytes, std::uint64_t offset = 0) const {
        if (!m_impl) return Result::NotInitialized;
        if (!out || offset + bytes > m_impl->desc.size) return Result::InvalidArgument;
        return m_impl->read(out, bytes, offset);
    }

    void discard() const { if (m_impl) m_impl->discard(); }

    bool              valid()  const noexcept { return static_cast<bool>(m_impl); }
    std::uint64_t     size()   const noexcept { return m_impl ? m_impl->desc.size : 0; }
    const BufferDesc& desc()   const noexcept { static const BufferDesc none{}; return m_impl ? m_impl->desc : none; }
    detail::BufferImpl* impl() const noexcept { return m_impl.get(); }
};

class Texture {
private:
    std::shared_ptr<detail::TextureImpl> m_impl;

public:
    Texture() noexcept = default;

    inline Result create(Device& device, const TextureDesc& desc);
    void destroy() noexcept { m_impl.reset(); }

    bool               valid()      const noexcept { return static_cast<bool>(m_impl); }
    const TextureDesc& desc()       const noexcept { static const TextureDesc none{}; return m_impl ? m_impl->desc : none; }
    std::uint32_t      width()      const noexcept { return m_impl ? m_impl->desc.extent.width : 0; }
    std::uint32_t      height()     const noexcept { return m_impl ? m_impl->desc.extent.height : 0; }
    std::uint32_t      depth()      const noexcept { return m_impl ? m_impl->desc.extent.depth : 0; }
    Extent2D           extent2d()   const noexcept { return { width(), height() }; }
    Format             format()     const noexcept { return m_impl ? m_impl->desc.format : Format::Undefined; }
    std::uint32_t      samples()    const noexcept { return m_impl ? m_impl->desc.samples : 0; }
    std::uint32_t      mip_levels() const noexcept { return m_impl ? m_impl->desc.mip_levels : 0; }
    detail::TextureImpl* impl()     const noexcept { return m_impl.get(); }
};

class Sampler {
private:
    std::shared_ptr<detail::SamplerImpl> m_impl;

public:
    Sampler() noexcept = default;

    inline Result create(Device& device, const SamplerDesc& desc);
    void destroy() noexcept { m_impl.reset(); }

    bool               valid() const noexcept { return static_cast<bool>(m_impl); }
    const SamplerDesc& desc()  const noexcept { static const SamplerDesc none{}; return m_impl ? m_impl->desc : none; }
    detail::SamplerImpl* impl() const noexcept { return m_impl.get(); }
};

class ShaderModule {
private:
    std::shared_ptr<detail::ShaderImpl> m_impl;

public:
    ShaderModule() noexcept = default;

    inline Result create(Device& device, const ShaderDesc& desc);
    void destroy() noexcept { m_impl.reset(); }

    bool        valid() const noexcept { return static_cast<bool>(m_impl); }
    ShaderStage stage() const noexcept { return m_impl ? m_impl->stage : ShaderStage::None; }
    const std::shared_ptr<detail::ShaderImpl>& impl() const noexcept { return m_impl; }
};

class BindGroupLayout {
private:
    std::shared_ptr<detail::BindGroupLayoutImpl> m_impl;

public:
    BindGroupLayout() noexcept = default;

    inline Result create(Device& device, const BindGroupLayoutDesc& desc);
    void destroy() noexcept { m_impl.reset(); }

    bool valid() const noexcept { return static_cast<bool>(m_impl); }
    const std::vector<BindGroupLayoutEntry>& entries() const noexcept { static const std::vector<BindGroupLayoutEntry> none; return m_impl ? m_impl->entries : none; }
    const std::shared_ptr<detail::BindGroupLayoutImpl>& impl() const noexcept { return m_impl; }
};

struct PipelineLayoutDesc {
    std::vector<const BindGroupLayout*> groups;
    std::uint32_t                       push_constant_size   = 0;
    ShaderStage                         push_constant_stages = ShaderStage::Graphics;
    const char*                         name                 = nullptr;
};

class PipelineLayout {
private:
    std::shared_ptr<detail::PipelineLayoutImpl> m_impl;

public:
    PipelineLayout() noexcept = default;

    inline Result create(Device& device, const PipelineLayoutDesc& desc);
    void destroy() noexcept { m_impl.reset(); }

    bool          valid()              const noexcept { return static_cast<bool>(m_impl); }
    std::uint32_t push_constant_size() const noexcept { return m_impl ? m_impl->push_size : 0; }

    std::uint32_t array_capacity(std::uint32_t group, std::uint32_t binding) const noexcept {
        return m_impl ? m_impl->array_capacity(group, binding) : 0;
    }

    const std::shared_ptr<detail::PipelineLayoutImpl>& impl() const noexcept { return m_impl; }
};

struct BindGroupEntry {
    std::uint32_t  binding = 0;
    std::uint32_t  element = 0;
    const Buffer*  buffer  = nullptr;
    std::uint64_t  offset  = 0;
    std::uint64_t  size    = 0;
    const Texture* texture = nullptr;
    const Sampler* sampler = nullptr;

    static BindGroupEntry uniform(std::uint32_t binding, const Buffer& buffer, std::uint64_t offset = 0, std::uint64_t size = 0) noexcept {
        BindGroupEntry e;
        e.binding = binding; e.buffer = &buffer; e.offset = offset; e.size = size;
        return e;
    }

    static BindGroupEntry storage(std::uint32_t binding, const Buffer& buffer, std::uint64_t offset = 0, std::uint64_t size = 0) noexcept {
        return uniform(binding, buffer, offset, size);
    }

    static BindGroupEntry sampled(std::uint32_t binding, const Texture& texture, const Sampler& sampler, std::uint32_t element = 0) noexcept {
        BindGroupEntry e;
        e.binding = binding; e.element = element; e.texture = &texture; e.sampler = &sampler;
        return e;
    }

    static BindGroupEntry storage_texture(std::uint32_t binding, const Texture& texture) noexcept {
        BindGroupEntry e;
        e.binding = binding; e.texture = &texture;
        return e;
    }
};

struct BindGroupDesc {
    const BindGroupLayout* layout   = nullptr;
    Span<BindGroupEntry>   entries;
    BindGroupLifetime      lifetime = BindGroupLifetime::Persistent;
    const char*            name     = nullptr;
};

class BindGroup {
private:
    std::shared_ptr<detail::BindGroupImpl> m_impl;

public:
    BindGroup() noexcept = default;

    inline Result create(Device& device, const BindGroupDesc& desc);
    void destroy() noexcept { m_impl.reset(); }

    bool valid() const noexcept { return static_cast<bool>(m_impl); }
    detail::BindGroupImpl* impl() const noexcept { return m_impl.get(); }
};

struct RenderPipelineDesc {
    const ShaderModule*          vertex       = nullptr;
    const ShaderModule*          fragment     = nullptr;
    const PipelineLayout*        layout       = nullptr;
    std::vector<VertexBinding>   vertex_bindings;
    std::vector<VertexAttribute> vertex_attributes;
    Topology                     topology     = Topology::TriangleList;
    CullMode                     cull_mode    = CullMode::None;
    FrontFace                    front_face   = FrontFace::CounterClockwise;
    DepthState                   depth;
    BlendState                   blend;
    std::vector<Format>          color_formats;
    Format                       depth_format = Format::Undefined;
    std::uint32_t                samples      = 1;
    const char*                  name         = nullptr;
};

class RenderPipeline {
private:
    std::shared_ptr<detail::RenderPipelineImpl> m_impl;

public:
    RenderPipeline() noexcept = default;

    inline Result create(Device& device, const RenderPipelineDesc& desc);
    void destroy() noexcept { m_impl.reset(); }

    bool valid() const noexcept { return static_cast<bool>(m_impl); }
    detail::RenderPipelineImpl* impl() const noexcept { return m_impl.get(); }
};

struct ComputePipelineDesc {
    const ShaderModule*   shader = nullptr;
    const PipelineLayout* layout = nullptr;
    const char*           name   = nullptr;
};

class ComputePipeline {
private:
    std::shared_ptr<detail::ComputePipelineImpl> m_impl;

public:
    ComputePipeline() noexcept = default;

    inline Result create(Device& device, const ComputePipelineDesc& desc);
    void destroy() noexcept { m_impl.reset(); }

    bool valid() const noexcept { return static_cast<bool>(m_impl); }
    detail::ComputePipelineImpl* impl() const noexcept { return m_impl.get(); }
};

class QuerySet {
private:
    std::shared_ptr<detail::QuerySetImpl> m_impl;

public:
    QuerySet() noexcept = default;

    inline Result create(Device& device, std::uint32_t count, const char* name = nullptr);
    void destroy() noexcept { m_impl.reset(); }

    bool          valid() const noexcept { return static_cast<bool>(m_impl); }
    std::uint32_t count() const noexcept { return m_impl ? m_impl->count : 0; }
    detail::QuerySetImpl* impl() const noexcept { return m_impl.get(); }
};

struct ColorTarget {
    const Texture* texture = nullptr;
    std::uint32_t  layer   = 0;
    std::uint32_t  mip     = 0;
    const Texture* resolve = nullptr;
    LoadOp         load    = LoadOp::Clear;
    StoreOp        store   = StoreOp::Store;
    ClearColor     clear;
};

struct DepthTarget {
    const Texture* texture       = nullptr;
    std::uint32_t  layer         = 0;
    const Texture* resolve       = nullptr;
    LoadOp         load          = LoadOp::Clear;
    StoreOp        store         = StoreOp::DontCare;
    float          clear_depth   = 1.0f;
    std::uint32_t  clear_stencil = 0;
};

struct RenderPassDesc {
    Rect2D             area;
    Span<ColorTarget>  colors;
    const DepthTarget* depth = nullptr;
    const char*        label = nullptr;
};

class CommandList {
private:
    static constexpr std::uint32_t kMaxGroups = 4;

    detail::CommandStream  m_stream;
    bool                   m_in_pass = false;
    detail::BindGroupImpl* m_compute_groups[kMaxGroups] = {};

    detail::Command& push(detail::Op op) {
        m_stream.commands.emplace_back(op);
        return m_stream.commands.back();
    }

    std::uint32_t copy_record(const detail::CopyRecord& c) {
        m_stream.copies.push_back(c);
        return static_cast<std::uint32_t>(m_stream.copies.size() - 1);
    }

public:
    CommandList() = default;

    void clear() noexcept {
        m_stream.clear();
        m_in_pass = false;
        for (auto& g : m_compute_groups) g = nullptr;
    }

    bool                         empty()  const noexcept { return m_stream.commands.empty(); }
    bool                         in_pass() const noexcept { return m_in_pass; }
    const detail::CommandStream& stream() const noexcept { return m_stream; }

    void reserve(std::size_t commands) { m_stream.commands.reserve(commands); }

    void begin_render_pass(const RenderPassDesc& desc) {
        if (m_in_pass) end_render_pass();
        detail::PassRecord p;
        p.area = desc.area;
        p.color_count = static_cast<std::uint32_t>(std::min<std::size_t>(desc.colors.size(), detail::PassRecord::kMaxColors));

        for (std::uint32_t i = 0; i < p.color_count; ++i) {
            const ColorTarget& c = desc.colors[i];
            detail::ColorTargetRecord& r = p.colors[i];
            r.texture = c.texture ? c.texture->impl() : nullptr;
            r.layer   = c.layer;
            r.mip     = c.mip;
            r.resolve = c.resolve ? c.resolve->impl() : nullptr;
            r.load    = c.load;
            r.store   = c.store;
            r.clear   = c.clear;
        }

        if (desc.depth && desc.depth->texture) {
            p.depth         = desc.depth->texture->impl();
            p.depth_layer   = desc.depth->layer;
            p.depth_resolve = desc.depth->resolve ? desc.depth->resolve->impl() : nullptr;
            p.depth_load    = desc.depth->load;
            p.depth_store   = desc.depth->store;
            p.clear_depth   = desc.depth->clear_depth;
            p.clear_stencil = desc.depth->clear_stencil;
        }

        if (p.area.extent.width == 0 || p.area.extent.height == 0) p.area = { { 0, 0 }, p.attachment_extent() };
        if (desc.label) p.label = m_stream.store(desc.label, std::strlen(desc.label) + 1);
        p.groups_first = static_cast<std::uint32_t>(m_stream.groups.size());
        m_stream.passes.push_back(p);
        push(detail::Op::BeginRenderPass).pass.index = static_cast<std::uint32_t>(m_stream.passes.size() - 1);
        m_in_pass = true;
    }

    void end_render_pass() {
        if (!m_in_pass) return;
        push(detail::Op::EndRenderPass);
        m_in_pass = false;
    }

    void set_pipeline(const RenderPipeline& pipeline) {
        if (!pipeline.valid()) return;
        push(detail::Op::SetRenderPipeline).render.pipeline = pipeline.impl();
    }

    void set_pipeline(const ComputePipeline& pipeline) {
        if (!pipeline.valid()) return;
        push(detail::Op::SetComputePipeline).compute.pipeline = pipeline.impl();
    }

    void set_bind_group(std::uint32_t index, const BindGroup& group) {
        if (!group.valid() || index >= kMaxGroups) return;
        detail::Command& c = push(detail::Op::SetBindGroup);
        c.bind.index = index;
        c.bind.group = group.impl();

        if (m_in_pass) {
            detail::PassRecord& p = m_stream.passes.back();
            if (p.groups_count == 0 || m_stream.groups.back() != group.impl()) {
                m_stream.groups.push_back(group.impl());
                ++p.groups_count;
            }
        } else {
            m_compute_groups[index] = group.impl();
        }
    }

    void push_constants(const void* data, std::uint32_t size, std::uint32_t offset = 0) {
        if (!data || size == 0) return;
        const std::uint32_t at = m_stream.store(data, size);
        detail::Command& c = push(detail::Op::PushConstants);
        c.bytes.at = at;
        c.bytes.size = size;
        c.bytes.offset = offset;
    }

    template <typename T, typename = std::enable_if_t<!std::is_array<T>::value && !std::is_pointer<T>::value>>
    void push_constants(const T& value, std::uint32_t offset = 0) {
        static_assert(std::is_trivially_copyable<T>::value, "push constant data must be trivially copyable");
        push_constants(&value, static_cast<std::uint32_t>(sizeof(T)), offset);
    }

    void set_vertex_buffer(std::uint32_t slot, const Buffer& buffer, std::uint64_t offset = 0) {
        if (!buffer.valid()) return;
        detail::Command& c = push(detail::Op::SetVertexBuffer);
        c.vertex.slot = slot;
        c.vertex.buffer = buffer.impl();
        c.vertex.offset = offset;
    }

    void set_index_buffer(const Buffer& buffer, IndexType type = IndexType::UInt32, std::uint64_t offset = 0) {
        if (!buffer.valid()) return;
        detail::Command& c = push(detail::Op::SetIndexBuffer);
        c.index.buffer = buffer.impl();
        c.index.offset = offset;
        c.index.type = type;
    }

    void set_viewport(const Viewport& v) {
        detail::Command& c = push(detail::Op::SetViewport);
        c.viewport.x = v.x; c.viewport.y = v.y; c.viewport.w = v.width; c.viewport.h = v.height;
        c.viewport.min_depth = v.min_depth; c.viewport.max_depth = v.max_depth;
    }

    void set_scissor(const Rect2D& r) {
        detail::Command& c = push(detail::Op::SetScissor);
        c.scissor.x = r.offset.x; c.scissor.y = r.offset.y; c.scissor.w = r.extent.width; c.scissor.h = r.extent.height;
    }

    void draw(std::uint32_t count, std::uint32_t instances = 1, std::uint32_t first = 0, std::uint32_t first_instance = 0) {
        if (count == 0 || instances == 0) return;
        detail::Command& c = push(detail::Op::Draw);
        c.draw.count = count; c.draw.instances = instances; c.draw.first = first; c.draw.first_instance = first_instance;
    }

    void draw_indexed(std::uint32_t count, std::uint32_t instances = 1, std::uint32_t first_index = 0, std::int32_t vertex_offset = 0, std::uint32_t first_instance = 0) {
        if (count == 0 || instances == 0) return;
        detail::Command& c = push(detail::Op::DrawIndexed);
        c.indexed.count = count; c.indexed.instances = instances; c.indexed.first_index = first_index;
        c.indexed.vertex_offset = vertex_offset; c.indexed.first_instance = first_instance;
    }

    void draw_indexed_indirect(const Buffer& commands, std::uint64_t offset, std::uint32_t count, std::uint32_t stride) {
        if (!commands.valid() || count == 0) return;
        detail::Command& c = push(detail::Op::DrawIndexedIndirect);
        c.indirect.buffer = commands.impl(); c.indirect.offset = offset; c.indirect.count = count; c.indirect.stride = stride;
    }

    void dispatch(std::uint32_t x, std::uint32_t y = 1, std::uint32_t z = 1) {
        if (m_in_pass || x == 0 || y == 0 || z == 0) return;
        detail::Command& c = push(detail::Op::Dispatch);
        c.dispatch.x = x; c.dispatch.y = y; c.dispatch.z = z;
        c.dispatch.groups_first = static_cast<std::uint32_t>(m_stream.groups.size());

        for (detail::BindGroupImpl* g : m_compute_groups) {
            if (!g) continue;
            m_stream.groups.push_back(g);
            ++c.dispatch.groups_count;
        }
    }

    void copy_buffer(const Buffer& src, std::uint64_t src_offset, const Buffer& dst, std::uint64_t dst_offset, std::uint64_t size) {
        if (m_in_pass || !src.valid() || !dst.valid() || size == 0) return;
        detail::CopyRecord r;
        r.src_buffer = src.impl(); r.src_offset = src_offset;
        r.dst_buffer = dst.impl(); r.dst_offset = dst_offset;
        r.size = size;
        push(detail::Op::CopyBuffer).copy.index = copy_record(r);
    }

    void copy_buffer_to_texture(const Buffer& src, std::uint64_t offset, std::uint32_t row_pixels, const Texture& dst, const TextureRegion& region) {
        if (m_in_pass || !src.valid() || !dst.valid()) return;
        detail::CopyRecord r;
        r.src_buffer = src.impl(); r.src_offset = offset; r.row_pixels = row_pixels;
        r.dst_texture = dst.impl(); r.dst_region = region;
        push(detail::Op::CopyBufferToTexture).copy.index = copy_record(r);
    }

    void copy_texture_to_buffer(const Texture& src, const TextureRegion& region, const Buffer& dst, std::uint64_t offset) {
        if (m_in_pass || !src.valid() || !dst.valid()) return;
        detail::CopyRecord r;
        r.src_texture = src.impl(); r.src_region = region;
        r.dst_buffer = dst.impl(); r.dst_offset = offset;
        push(detail::Op::CopyTextureToBuffer).copy.index = copy_record(r);
    }

    void copy_texture(const Texture& src, const TextureRegion& src_region, const Texture& dst, const TextureRegion& dst_region) {
        if (m_in_pass || !src.valid() || !dst.valid()) return;
        detail::CopyRecord r;
        r.src_texture = src.impl(); r.src_region = src_region;
        r.dst_texture = dst.impl(); r.dst_region = dst_region;
        r.dst_region.extent = src_region.extent;
        push(detail::Op::CopyTexture).copy.index = copy_record(r);
    }

    void copy_texture(const Texture& src, const Texture& dst, const Rect2D& area) {
        TextureRegion region;
        region.origin = { area.offset.x, area.offset.y, 0 };
        region.extent = { area.extent.width, area.extent.height, 1 };
        copy_texture(src, region, dst, region);
    }

    void blit_texture(const Texture& src, const Rect2D& src_rect, const Texture& dst, const Rect2D& dst_rect, Filter filter = Filter::Linear) {
        if (m_in_pass || !src.valid() || !dst.valid()) return;
        detail::CopyRecord r;
        r.src_texture = src.impl();
        r.src_region.origin = { src_rect.offset.x, src_rect.offset.y, 0 };
        r.src_region.extent = { src_rect.extent.width, src_rect.extent.height, 1 };
        r.dst_texture = dst.impl();
        r.dst_region.origin = { dst_rect.offset.x, dst_rect.offset.y, 0 };
        r.dst_region.extent = { dst_rect.extent.width, dst_rect.extent.height, 1 };
        r.filter = filter;
        push(detail::Op::BlitTexture).copy.index = copy_record(r);
    }

    void generate_mipmaps(const Texture& texture) {
        if (m_in_pass || !texture.valid() || texture.mip_levels() < 2) return;
        push(detail::Op::GenerateMipmaps).mips.texture = texture.impl();
    }

    void reset_queries(const QuerySet& set, std::uint32_t first, std::uint32_t count) {
        if (m_in_pass || !set.valid() || count == 0 || first + count > set.count()) return;
        detail::Command& c = push(detail::Op::ResetQueries);
        c.query.set = set.impl(); c.query.first = first; c.query.count = count;
    }

    void write_timestamp(const QuerySet& set, std::uint32_t index) {
        if (!set.valid() || index >= set.count()) return;
        detail::Command& c = push(detail::Op::WriteTimestamp);
        c.query.set = set.impl(); c.query.first = index; c.query.count = 1;
    }

    void push_label(const char* name) {
        if (!name) return;
        detail::Command& c = push(detail::Op::PushLabel);
        c.bytes.at = m_stream.store(name, std::strlen(name) + 1);
    }

    void pop_label() { push(detail::Op::PopLabel); }
};

struct DeviceDesc {
    Backend       backend          = Backend::Auto;
    void*         window           = nullptr;
    Extent2D      size;
    bool          vsync            = true;
    bool          debug            = false;
    std::uint32_t frames_in_flight = 2;
    const char*   app_name         = "fizmo";
};

class Device {
private:
    std::shared_ptr<detail::BackendDevice> m_backend;
    Caps                                   m_none;
    std::string                            m_error;

    template <typename Impl>
    static Result named(Result r, const std::shared_ptr<Impl>& impl, const char* name) {
        if (succeeded(r) && impl && name) impl->name = name;
        return r;
    }

public:
    Device() noexcept = default;
    ~Device() noexcept { destroy(); }

    Device(const Device&) = delete;
    Device& operator=(const Device&) = delete;
    Device(Device&&) noexcept = default;
    Device& operator=(Device&&) noexcept = default;

    Result create(const DeviceDesc& desc) {
        destroy();
        if (!desc.window) return Result::InvalidArgument;
        detail::DeviceInput in;
        in.backend          = desc.backend;
        in.window           = desc.window;
        in.size             = { std::max(desc.size.width, 1u), std::max(desc.size.height, 1u) };
        in.vsync            = desc.vsync;
        in.debug            = desc.debug;
        in.frames_in_flight = std::max(desc.frames_in_flight, 1u);
        in.app_name         = desc.app_name;
        Result r = Result::NoBackend;

        try {
            m_error.clear();
            m_backend = detail::create_backend(in, r, m_error);
        } catch (...) {
            m_backend.reset();
            r = Result::OutOfMemory;
        }

        if (!m_backend && succeeded(r)) r = Result::NoBackend;
        return m_backend ? Result::Success : r;
    }

    void destroy() noexcept {
        if (!m_backend) return;
        try { m_backend->wait_idle(); } catch (...) {}
        m_backend.reset();
    }

    bool          valid()            const noexcept { return static_cast<bool>(m_backend); }
    Backend       backend()          const noexcept { return m_backend ? m_backend->backend() : Backend::Auto; }
    const char*   backend_name()     const noexcept { return gpu::backend_name(backend()); }
    const Caps&   caps()             const noexcept { return m_backend ? m_backend->caps() : m_none; }
    MemoryStats   memory_stats()     const noexcept { return m_backend ? m_backend->memory_stats() : MemoryStats{}; }
    std::uint32_t frames_in_flight() const noexcept { return m_backend ? m_backend->frames_in_flight() : 1; }
    bool          in_frame()         const noexcept { return m_backend && m_backend->in_frame(); }

    const std::string& last_error() const noexcept { return m_backend ? m_backend->last_error() : m_error; }

    bool supports(Format format, TextureUsage usage, std::uint32_t samples = 1) const noexcept {
        return m_backend && m_backend->supports(format, usage, samples);
    }

    bool supports_linear_filter(Format format) const noexcept { return m_backend && m_backend->supports_linear_filter(format); }

    std::uint32_t max_samples(Format color, Format depth = Format::Undefined, std::uint32_t wanted = 4) const noexcept {
        for (std::uint32_t s = wanted; s > 1; s >>= 1) {
            if (s > caps().max_samples) continue;
            if (!supports(color, TextureUsage::RenderTarget, s)) continue;
            if (depth != Format::Undefined && !supports(depth, TextureUsage::DepthStencil, s)) continue;
            return s;
        }

        return 1;
    }

    Result begin_frame(FrameInfo& out) { return m_backend ? m_backend->begin_frame(out) : Result::NotInitialized; }
    Result submit(const CommandList& list) { return m_backend ? m_backend->submit(list.stream()) : Result::NotInitialized; }
    Result end_frame() { return m_backend ? m_backend->end_frame() : Result::NotInitialized; }

    Result present(const Texture& source, Filter filter = Filter::Linear) {
        if (!m_backend) return Result::NotInitialized;
        if (!source.valid()) return Result::InvalidArgument;
        return m_backend->present(*source.impl(), filter);
    }

    void   resize(std::uint32_t width, std::uint32_t height) { if (m_backend) m_backend->resize(width, height); }
    Result set_vsync(bool enabled) { return m_backend ? m_backend->set_vsync(enabled) : Result::NotInitialized; }
    Result wait_idle() { return m_backend ? m_backend->wait_idle() : Result::NotInitialized; }

    Result write_buffer(const Buffer& buffer, std::uint64_t offset, const void* data, std::uint64_t bytes) { return buffer.write(data, bytes, offset); }

    Result write_texture(const Texture& texture, const TextureRegion& region, const void* data, std::uint32_t row_pixels = 0) {
        if (!m_backend) return Result::NotInitialized;
        if (!texture.valid() || !data || region.extent.width == 0 || region.extent.height == 0 || region.extent.depth == 0) return Result::InvalidArgument;
        return m_backend->write_texture(*texture.impl(), region, data, row_pixels);
    }

    Result write_texture(const Texture& texture, const void* data) {
        TextureRegion all;
        all.extent = texture.desc().extent;
        return write_texture(texture, all, data);
    }

    Result read_texture(const Texture& texture, const TextureRegion& region, void* out) {
        if (!m_backend) return Result::NotInitialized;
        if (!texture.valid() || !out || region.extent.width == 0 || region.extent.height == 0) return Result::InvalidArgument;
        return m_backend->read_texture(*texture.impl(), region, out);
    }

    bool read_timestamps(const QuerySet& set, std::uint32_t first, std::uint32_t count, std::uint64_t* nanoseconds) {
        if (!m_backend || !set.valid() || !nanoseconds || count == 0 || first + count > set.count()) return false;
        return m_backend->read_timestamps(*set.impl(), first, count, nanoseconds);
    }

    detail::BackendDevice* backend_device() const noexcept { return m_backend.get(); }

    friend class Buffer;
    friend class Texture;
    friend class Sampler;
    friend class ShaderModule;
    friend class BindGroupLayout;
    friend class PipelineLayout;
    friend class BindGroup;
    friend class RenderPipeline;
    friend class ComputePipeline;
    friend class QuerySet;
};

inline Result Buffer::create(Device& device, const BufferDesc& desc) {
    m_impl.reset();
    if (!device.valid()) return Result::NotInitialized;
    if (desc.size == 0) return Result::InvalidArgument;
    return Device::named(device.m_backend->create_buffer(desc, m_impl), m_impl, desc.name);
}

inline Result Texture::create(Device& device, const TextureDesc& desc) {
    m_impl.reset();
    if (!device.valid()) return Result::NotInitialized;
    TextureDesc d = desc;
    if (d.mip_levels == 0) d.mip_levels = detail::full_mip_count(d.extent);
    d.samples = std::max(d.samples, 1u);
    const Result v = detail::validate(d);
    if (failed(v)) return v;
    return Device::named(device.m_backend->create_texture(d, m_impl), m_impl, desc.name);
}

inline Result Sampler::create(Device& device, const SamplerDesc& desc) {
    m_impl.reset();
    if (!device.valid()) return Result::NotInitialized;
    return Device::named(device.m_backend->create_sampler(desc, m_impl), m_impl, desc.name);
}

inline Result ShaderModule::create(Device& device, const ShaderDesc& desc) {
    m_impl.reset();
    if (!device.valid()) return Result::NotInitialized;
    return Device::named(device.m_backend->create_shader(desc, m_impl), m_impl, desc.name);
}

inline Result BindGroupLayout::create(Device& device, const BindGroupLayoutDesc& desc) {
    m_impl.reset();
    if (!device.valid()) return Result::NotInitialized;
    return Device::named(device.m_backend->create_bind_group_layout(desc, m_impl), m_impl, desc.name);
}

inline Result PipelineLayout::create(Device& device, const PipelineLayoutDesc& desc) {
    m_impl.reset();
    if (!device.valid()) return Result::NotInitialized;
    std::vector<std::shared_ptr<detail::BindGroupLayoutImpl>> groups;

    for (const BindGroupLayout* g : desc.groups) {
        if (!g || !g->valid()) return Result::InvalidArgument;
        groups.push_back(g->impl());
    }

    if (desc.push_constant_size > device.caps().max_push_constants || desc.push_constant_size % 4 != 0) return Result::InvalidArgument;
    return Device::named(device.m_backend->create_pipeline_layout(std::move(groups), desc.push_constant_size, desc.push_constant_stages, desc.name, m_impl), m_impl, desc.name);
}

inline Result BindGroup::create(Device& device, const BindGroupDesc& desc) {
    m_impl.reset();
    if (!device.valid()) return Result::NotInitialized;
    if (!desc.layout || !desc.layout->valid()) return Result::InvalidArgument;
    constexpr std::size_t kInline = 24;
    detail::BindingInput inline_entries[kInline];
    std::vector<detail::BindingInput> heap;
    detail::BindingInput* entries = inline_entries;

    if (desc.entries.size() > kInline) {
        heap.resize(desc.entries.size());
        entries = heap.data();
    }

    for (std::size_t i = 0; i < desc.entries.size(); ++i) {
        const BindGroupEntry& e = desc.entries[i];
        const BindGroupLayoutEntry* le = desc.layout->impl()->find(e.binding);
        if (!le || e.element >= le->count) return Result::InvalidArgument;
        detail::BindingInput& b = entries[i];
        b.binding = e.binding;
        b.element = e.element;
        b.buffer  = e.buffer && e.buffer->valid() ? e.buffer->impl() : nullptr;
        b.offset  = e.offset;
        b.size    = e.size;
        b.texture = e.texture && e.texture->valid() ? e.texture->impl() : nullptr;
        b.sampler = e.sampler && e.sampler->valid() ? e.sampler->impl() : nullptr;
        const bool buffer = le->type == BindingType::UniformBuffer || le->type == BindingType::StorageBuffer;
        if (buffer ? !b.buffer : !b.texture) return Result::InvalidArgument;
        if (le->type == BindingType::SampledTexture && !b.sampler) return Result::InvalidArgument;
        if (buffer && b.size == 0) b.size = b.buffer->desc.size - std::min(b.offset, b.buffer->desc.size);
    }

    return Device::named(device.m_backend->create_bind_group(desc.layout->impl(), Span<detail::BindingInput>(entries, desc.entries.size()), desc.lifetime, desc.name, m_impl), m_impl, desc.name);
}

inline Result RenderPipeline::create(Device& device, const RenderPipelineDesc& desc) {
    m_impl.reset();
    if (!device.valid()) return Result::NotInitialized;
    if (!desc.vertex || !desc.vertex->valid() || !desc.layout || !desc.layout->valid()) return Result::InvalidArgument;
    if (desc.vertex->stage() != ShaderStage::Vertex) return Result::InvalidArgument;
    if (desc.fragment && (!desc.fragment->valid() || desc.fragment->stage() != ShaderStage::Fragment)) return Result::InvalidArgument;
    if (desc.color_formats.size() > detail::PassRecord::kMaxColors) return Result::InvalidArgument;
    detail::RenderPipelineInput in;
    in.vertex            = desc.vertex->impl();
    in.fragment          = desc.fragment ? desc.fragment->impl() : nullptr;
    in.layout            = desc.layout->impl();
    in.vertex_bindings   = desc.vertex_bindings;
    in.vertex_attributes = desc.vertex_attributes;
    in.topology          = desc.topology;
    in.cull_mode         = desc.cull_mode;
    in.front_face        = desc.front_face;
    in.depth             = desc.depth;
    in.blend             = desc.blend;
    in.color_formats     = desc.color_formats;
    in.depth_format      = desc.depth_format;
    in.samples           = std::max(desc.samples, 1u);
    in.name              = desc.name;
    return Device::named(device.m_backend->create_render_pipeline(in, m_impl), m_impl, desc.name);
}

inline Result ComputePipeline::create(Device& device, const ComputePipelineDesc& desc) {
    m_impl.reset();
    if (!device.valid()) return Result::NotInitialized;
    if (!desc.shader || !desc.shader->valid() || desc.shader->stage() != ShaderStage::Compute || !desc.layout || !desc.layout->valid()) return Result::InvalidArgument;
    if (!device.caps().compute) return Result::Unsupported;
    return Device::named(device.m_backend->create_compute_pipeline(desc.shader->impl(), desc.layout->impl(), desc.name, m_impl), m_impl, desc.name);
}

inline Result QuerySet::create(Device& device, std::uint32_t count, const char* name) {
    m_impl.reset();
    if (!device.valid()) return Result::NotInitialized;
    if (count == 0) return Result::InvalidArgument;
    if (!device.caps().timestamps) return Result::Unsupported;
    return Device::named(device.m_backend->create_query_set(count, name, m_impl), m_impl, name);
}

} // namespace gpu
} // namespace fizmo

#endif // FIZMO_GPU_DEVICE_HPP
