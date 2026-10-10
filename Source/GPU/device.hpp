#ifndef FIZMO_GPU_DEVICE_HPP
#define FIZMO_GPU_DEVICE_HPP

#include "backend.hpp"

namespace fizmo {
namespace gpu {

class Device;

namespace detail {

std::shared_ptr<BackendDevice> create_backend(const DeviceInput& input, Result& result, std::string& error);

} // namespace detail

class Buffer {
private:
    std::shared_ptr<detail::BufferImpl> m_impl;

public:
    Buffer() noexcept = default;

    Result create(Device& device, const BufferDesc& desc);
    void destroy() noexcept { m_impl.reset(); }

    Result write(const void* data, std::uint64_t bytes, std::uint64_t offset = 0) const;

    template <typename T>
    Result write(const std::vector<T>& items, std::uint64_t offset = 0) const { return write(items.data(), items.size() * sizeof(T), offset); }

    Result read(void* out, std::uint64_t bytes, std::uint64_t offset = 0) const;

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

    Result create(Device& device, const TextureDesc& desc);
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

    Result create(Device& device, const SamplerDesc& desc);
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

    Result create(Device& device, const ShaderDesc& desc);
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

    Result create(Device& device, const BindGroupLayoutDesc& desc);
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

    Result create(Device& device, const PipelineLayoutDesc& desc);
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

    static BindGroupEntry sampled(std::uint32_t binding, const Texture& texture, const Sampler& sampler, std::uint32_t element = 0) noexcept;

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

    Result create(Device& device, const BindGroupDesc& desc);
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

    Result create(Device& device, const RenderPipelineDesc& desc);
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

    Result create(Device& device, const ComputePipelineDesc& desc);
    void destroy() noexcept { m_impl.reset(); }

    bool valid() const noexcept { return static_cast<bool>(m_impl); }
    detail::ComputePipelineImpl* impl() const noexcept { return m_impl.get(); }
};

class QuerySet {
private:
    std::shared_ptr<detail::QuerySetImpl> m_impl;

public:
    QuerySet() noexcept = default;

    Result create(Device& device, std::uint32_t count, const char* name = nullptr);
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

    std::uint32_t copy_record(const detail::CopyRecord& c);

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

    void begin_render_pass(const RenderPassDesc& desc);

    void end_render_pass() {
        if (!m_in_pass) return;
        push(detail::Op::EndRenderPass);
        m_in_pass = false;
    }

    void set_pipeline(const RenderPipeline& pipeline);

    void set_pipeline(const ComputePipeline& pipeline);

    void set_bind_group(std::uint32_t index, const BindGroup& group);

    void push_constants(const void* data, std::uint32_t size, std::uint32_t offset = 0);

    template <typename T, typename = std::enable_if_t<!std::is_array<T>::value && !std::is_pointer<T>::value>>
    void push_constants(const T& value, std::uint32_t offset = 0) {
        static_assert(std::is_trivially_copyable<T>::value, "push constant data must be trivially copyable");
        push_constants(&value, static_cast<std::uint32_t>(sizeof(T)), offset);
    }

    void set_vertex_buffer(std::uint32_t slot, const Buffer& buffer, std::uint64_t offset = 0);

    void set_index_buffer(const Buffer& buffer, IndexType type = IndexType::UInt32, std::uint64_t offset = 0);

    void set_viewport(const Viewport& v);

    void set_scissor(const Rect2D& r);

    void draw(std::uint32_t count, std::uint32_t instances = 1, std::uint32_t first = 0, std::uint32_t first_instance = 0);

    void draw_indexed(std::uint32_t count, std::uint32_t instances = 1, std::uint32_t first_index = 0, std::int32_t vertex_offset = 0, std::uint32_t first_instance = 0);

    void draw_indexed_indirect(const Buffer& commands, std::uint64_t offset, std::uint32_t count, std::uint32_t stride);

    void dispatch(std::uint32_t x, std::uint32_t y = 1, std::uint32_t z = 1);

    void copy_buffer(const Buffer& src, std::uint64_t src_offset, const Buffer& dst, std::uint64_t dst_offset, std::uint64_t size);

    void copy_buffer_to_texture(const Buffer& src, std::uint64_t offset, std::uint32_t row_pixels, const Texture& dst, const TextureRegion& region);

    void copy_texture_to_buffer(const Texture& src, const TextureRegion& region, const Buffer& dst, std::uint64_t offset);

    void copy_texture(const Texture& src, const TextureRegion& src_region, const Texture& dst, const TextureRegion& dst_region);

    void copy_texture(const Texture& src, const Texture& dst, const Rect2D& area);

    void blit_texture(const Texture& src, const Rect2D& src_rect, const Texture& dst, const Rect2D& dst_rect, Filter filter = Filter::Linear);

    void generate_mipmaps(const Texture& texture);

    void reset_queries(const QuerySet& set, std::uint32_t first, std::uint32_t count);

    void write_timestamp(const QuerySet& set, std::uint32_t index);

    void push_label(const char* name);

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

    Result create(const DeviceDesc& desc);

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

    std::uint32_t max_samples(Format color, Format depth = Format::Undefined, std::uint32_t wanted = 4) const noexcept;

    Result begin_frame(FrameInfo& out) { return m_backend ? m_backend->begin_frame(out) : Result::NotInitialized; }
    Result submit(const CommandList& list) { return m_backend ? m_backend->submit(list.stream()) : Result::NotInitialized; }
    Result end_frame() { return m_backend ? m_backend->end_frame() : Result::NotInitialized; }

    Result present(const Texture& source, Filter filter = Filter::Linear);

    void   resize(std::uint32_t width, std::uint32_t height) { if (m_backend) m_backend->resize(width, height); }
    Result set_vsync(bool enabled) { return m_backend ? m_backend->set_vsync(enabled) : Result::NotInitialized; }
    Result wait_idle() { return m_backend ? m_backend->wait_idle() : Result::NotInitialized; }

    Result write_buffer(const Buffer& buffer, std::uint64_t offset, const void* data, std::uint64_t bytes) { return buffer.write(data, bytes, offset); }

    Result write_texture(const Texture& texture, const TextureRegion& region, const void* data, std::uint32_t row_pixels = 0);

    Result write_texture(const Texture& texture, const void* data);

    Result read_texture(const Texture& texture, const TextureRegion& region, void* out);

    bool read_timestamps(const QuerySet& set, std::uint32_t first, std::uint32_t count, std::uint64_t* nanoseconds);

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





















} // namespace gpu
} // namespace fizmo

#endif // FIZMO_GPU_DEVICE_HPP
