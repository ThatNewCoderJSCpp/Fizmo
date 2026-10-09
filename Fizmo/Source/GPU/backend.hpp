#ifndef FIZMO_GPU_BACKEND_HPP
#define FIZMO_GPU_BACKEND_HPP

#include "types.hpp"
#include <algorithm>
#include <atomic>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

namespace fizmo {
namespace gpu {
namespace detail {

inline std::uint64_t next_object_id() noexcept {
    static std::atomic<std::uint64_t> counter{ 0 };
    return ++counter;
}

struct Object {
    const std::uint64_t id = next_object_id();
    std::string         name;

    virtual ~Object() = default;
};

struct BufferImpl : Object {
    BufferDesc desc;

    virtual Result write(const void* data, std::uint64_t bytes, std::uint64_t offset) = 0;
    virtual Result read(void* out, std::uint64_t bytes, std::uint64_t offset) = 0;
    virtual void   discard() {}
};

struct TextureImpl : Object {
    TextureDesc desc;

    std::uint32_t layers() const noexcept { return desc.dimension == TextureDimension::Cube ? 6u : 1u; }
};

struct SamplerImpl : Object {
    SamplerDesc desc;
};

struct ShaderImpl : Object {
    ShaderStage stage = ShaderStage::Vertex;
};

struct BindGroupLayoutImpl : Object {
    std::vector<BindGroupLayoutEntry> entries;

    const BindGroupLayoutEntry* find(std::uint32_t binding) const noexcept {
        for (const BindGroupLayoutEntry& e : entries) if (e.binding == binding) return &e;
        return nullptr;
    }
};

struct PipelineLayoutImpl : Object {
    std::vector<std::shared_ptr<BindGroupLayoutImpl>> groups;
    std::uint32_t                                     push_size   = 0;
    ShaderStage                                       push_stages = ShaderStage::None;

    virtual std::uint32_t array_capacity(std::uint32_t group, std::uint32_t binding) const noexcept {
        if (group >= groups.size() || !groups[group]) return 0;
        const BindGroupLayoutEntry* e = groups[group]->find(binding);
        return e ? e->count : 0;
    }
};

struct ResourceBinding {
    std::uint32_t binding = 0;
    std::uint32_t element = 0;
    BufferImpl*   buffer  = nullptr;
    std::uint64_t offset  = 0;
    std::uint64_t size    = 0;
    TextureImpl*  texture = nullptr;
    SamplerImpl*  sampler = nullptr;
};

struct BindGroupImpl : Object {
    std::shared_ptr<BindGroupLayoutImpl> layout;
    std::vector<ResourceBinding>         entries;
    BindGroupLifetime                    lifetime = BindGroupLifetime::Persistent;
};

struct RenderPipelineImpl : Object {
    std::shared_ptr<PipelineLayoutImpl> layout;
};

struct ComputePipelineImpl : Object {
    std::shared_ptr<PipelineLayoutImpl> layout;
};

struct QuerySetImpl : Object {
    std::uint32_t count = 0;
};

enum class Op : std::uint8_t {
    BeginRenderPass = 0,
    EndRenderPass,
    SetRenderPipeline,
    SetComputePipeline,
    SetBindGroup,
    PushConstants,
    SetVertexBuffer,
    SetIndexBuffer,
    SetViewport,
    SetScissor,
    Draw,
    DrawIndexed,
    DrawIndexedIndirect,
    Dispatch,
    CopyBuffer,
    CopyBufferToTexture,
    CopyTextureToBuffer,
    CopyTexture,
    BlitTexture,
    GenerateMipmaps,
    ResetQueries,
    WriteTimestamp,
    PushLabel,
    PopLabel,
};

struct Command {
    Op op = Op::PopLabel;

    union {
        struct { std::uint32_t index; } pass;
        struct { RenderPipelineImpl* pipeline; } render;
        struct { ComputePipelineImpl* pipeline; } compute;
        struct { std::uint32_t index; BindGroupImpl* group; } bind;
        struct { std::uint32_t at; std::uint32_t size; std::uint32_t offset; } bytes;
        struct { std::uint32_t slot; BufferImpl* buffer; std::uint64_t offset; } vertex;
        struct { BufferImpl* buffer; std::uint64_t offset; IndexType type; } index;
        struct { float x, y, w, h, min_depth, max_depth; } viewport;
        struct { std::int32_t x, y; std::uint32_t w, h; } scissor;
        struct { std::uint32_t count, instances, first, first_instance; } draw;
        struct { std::uint32_t count, instances, first_index; std::int32_t vertex_offset; std::uint32_t first_instance; } indexed;
        struct { BufferImpl* buffer; std::uint64_t offset; std::uint32_t count, stride; } indirect;
        struct { std::uint32_t x, y, z, groups_first, groups_count; } dispatch;
        struct { std::uint32_t index; } copy;
        struct { TextureImpl* texture; } mips;
        struct { QuerySetImpl* set; std::uint32_t first, count; } query;
        std::uint64_t raw[3];
    };

    Command() noexcept : raw{} {}
    explicit Command(Op o) noexcept : op(o), raw{} {}
};

static_assert(sizeof(Command) <= 32, "commands should stay compact");

struct ColorTargetRecord {
    TextureImpl*  texture = nullptr;
    std::uint32_t layer   = 0;
    std::uint32_t mip     = 0;
    TextureImpl*  resolve = nullptr;
    LoadOp        load    = LoadOp::Clear;
    StoreOp       store   = StoreOp::Store;
    ClearColor    clear;
};

struct PassRecord {
    static constexpr std::uint32_t kMaxColors = 4;

    Rect2D            area;
    ColorTargetRecord colors[kMaxColors];
    std::uint32_t     color_count   = 0;
    TextureImpl*      depth         = nullptr;
    std::uint32_t     depth_layer   = 0;
    TextureImpl*      depth_resolve = nullptr;
    LoadOp            depth_load    = LoadOp::Clear;
    StoreOp           depth_store   = StoreOp::DontCare;
    float             clear_depth   = 1.0f;
    std::uint32_t     clear_stencil = 0;
    std::uint32_t     groups_first  = 0;
    std::uint32_t     groups_count  = 0;
    std::uint32_t     label         = UINT32_MAX;

    Extent2D attachment_extent() const noexcept {
        const TextureImpl* t = color_count ? colors[0].texture : depth;
        if (!t) return {};
        const std::uint32_t mip = color_count ? colors[0].mip : 0;
        return { std::max(1u, t->desc.extent.width >> mip), std::max(1u, t->desc.extent.height >> mip) };
    }
};

struct CopyRecord {
    BufferImpl*   src_buffer  = nullptr;
    BufferImpl*   dst_buffer  = nullptr;
    TextureImpl*  src_texture = nullptr;
    TextureImpl*  dst_texture = nullptr;
    std::uint64_t src_offset  = 0;
    std::uint64_t dst_offset  = 0;
    std::uint64_t size        = 0;
    std::uint32_t row_pixels  = 0;
    TextureRegion src_region;
    TextureRegion dst_region;
    Filter        filter      = Filter::Linear;
};

struct CommandStream {
    std::vector<Command>        commands;
    std::vector<PassRecord>     passes;
    std::vector<CopyRecord>     copies;
    std::vector<BindGroupImpl*> groups;
    std::vector<std::uint8_t>   bytes;

    void clear() noexcept {
        commands.clear();
        passes.clear();
        copies.clear();
        groups.clear();
        bytes.clear();
    }

    std::uint32_t store(const void* data, std::size_t size) {
        const std::uint32_t at = static_cast<std::uint32_t>(bytes.size());
        bytes.resize(bytes.size() + size);
        if (size) std::memcpy(bytes.data() + at, data, size);
        return at;
    }

    const char* text(std::uint32_t at) const noexcept { return reinterpret_cast<const char*>(bytes.data() + at); }
};

struct BindingInput {
    std::uint32_t binding = 0;
    std::uint32_t element = 0;
    BufferImpl*   buffer  = nullptr;
    std::uint64_t offset  = 0;
    std::uint64_t size    = 0;
    TextureImpl*  texture = nullptr;
    SamplerImpl*  sampler = nullptr;
};

struct RenderPipelineInput {
    std::shared_ptr<ShaderImpl>         vertex;
    std::shared_ptr<ShaderImpl>         fragment;
    std::shared_ptr<PipelineLayoutImpl> layout;
    std::vector<VertexBinding>          vertex_bindings;
    std::vector<VertexAttribute>        vertex_attributes;
    Topology                            topology     = Topology::TriangleList;
    CullMode                            cull_mode    = CullMode::None;
    FrontFace                           front_face   = FrontFace::CounterClockwise;
    DepthState                          depth;
    BlendState                          blend;
    std::vector<Format>                 color_formats;
    Format                              depth_format = Format::Undefined;
    std::uint32_t                       samples      = 1;
    const char*                         name         = nullptr;
};

struct DeviceInput {
    Backend       backend          = Backend::Auto;
    void*         window           = nullptr;
    Extent2D      size;
    bool          vsync            = true;
    bool          debug            = false;
    std::uint32_t frames_in_flight = 2;
    const char*   app_name         = "fizmo";
};

class BackendDevice {
public:
    virtual ~BackendDevice() = default;

    virtual Backend            backend() const noexcept = 0;
    virtual const Caps&        caps() const noexcept = 0;
    virtual MemoryStats        memory_stats() const noexcept = 0;
    virtual bool               supports(Format format, TextureUsage usage, std::uint32_t samples) const noexcept = 0;
    virtual bool               supports_linear_filter(Format format) const noexcept = 0;
    virtual std::uint32_t      frames_in_flight() const noexcept = 0;
    virtual const std::string& last_error() const noexcept = 0;

    virtual Result create_buffer(const BufferDesc& desc, std::shared_ptr<BufferImpl>& out) = 0;
    virtual Result create_texture(const TextureDesc& desc, std::shared_ptr<TextureImpl>& out) = 0;
    virtual Result create_sampler(const SamplerDesc& desc, std::shared_ptr<SamplerImpl>& out) = 0;
    virtual Result create_shader(const ShaderDesc& desc, std::shared_ptr<ShaderImpl>& out) = 0;
    virtual Result create_bind_group_layout(const BindGroupLayoutDesc& desc, std::shared_ptr<BindGroupLayoutImpl>& out) = 0;
    virtual Result create_pipeline_layout(std::vector<std::shared_ptr<BindGroupLayoutImpl>> groups, std::uint32_t push_size, ShaderStage push_stages, const char* name, std::shared_ptr<PipelineLayoutImpl>& out) = 0;
    virtual Result create_bind_group(const std::shared_ptr<BindGroupLayoutImpl>& layout, Span<BindingInput> entries, BindGroupLifetime lifetime, const char* name, std::shared_ptr<BindGroupImpl>& out) = 0;
    virtual Result create_render_pipeline(const RenderPipelineInput& desc, std::shared_ptr<RenderPipelineImpl>& out) = 0;
    virtual Result create_compute_pipeline(const std::shared_ptr<ShaderImpl>& shader, const std::shared_ptr<PipelineLayoutImpl>& layout, const char* name, std::shared_ptr<ComputePipelineImpl>& out) = 0;
    virtual Result create_query_set(std::uint32_t count, const char* name, std::shared_ptr<QuerySetImpl>& out) = 0;

    virtual Result begin_frame(FrameInfo& out) = 0;
    virtual Result submit(const CommandStream& stream) = 0;
    virtual Result present(TextureImpl& source, Filter filter) = 0;
    virtual Result end_frame() = 0;
    virtual bool   in_frame() const noexcept = 0;
    virtual void   resize(std::uint32_t width, std::uint32_t height) = 0;
    virtual Result set_vsync(bool enabled) = 0;
    virtual Result wait_idle() = 0;

    virtual Result write_texture(TextureImpl& texture, const TextureRegion& region, const void* data, std::uint32_t row_pixels) = 0;
    virtual Result read_texture(TextureImpl& texture, const TextureRegion& region, void* out) = 0;
    virtual bool   read_timestamps(QuerySetImpl& set, std::uint32_t first, std::uint32_t count, std::uint64_t* nanoseconds) = 0;
};

inline Extent3D mip_extent(const TextureDesc& d, std::uint32_t mip) noexcept {
    return { std::max(1u, d.extent.width >> mip), std::max(1u, d.extent.height >> mip), d.dimension == TextureDimension::D3 ? std::max(1u, d.extent.depth >> mip) : 1u };
}

inline std::uint32_t full_mip_count(Extent3D e) noexcept {
    std::uint32_t largest = std::max(e.width, std::max(e.height, e.depth));
    std::uint32_t levels = 1;
    while (largest > 1) { largest >>= 1; ++levels; }
    return levels;
}

inline Result validate(const TextureDesc& d) noexcept {
    if (d.extent.width == 0 || d.extent.height == 0 || d.extent.depth == 0 || d.format == Format::Undefined) return Result::InvalidArgument;
    if (d.dimension == TextureDimension::Cube && d.extent.width != d.extent.height) return Result::InvalidArgument;
    if (d.dimension != TextureDimension::D3 && d.extent.depth != 1) return Result::InvalidArgument;
    if (d.samples > 1 && (d.dimension != TextureDimension::D2 || d.mip_levels != 1)) return Result::InvalidArgument;
    if (d.samples > 1 && (any(d.usage, TextureUsage::Sampled) || any(d.usage, TextureUsage::Storage))) return Result::Unsupported;
    return Result::Success;
}

} // namespace detail
} // namespace gpu
} // namespace fizmo

#endif // FIZMO_GPU_BACKEND_HPP
