#include "fizmo_library.hpp"
#include "device.hpp"

namespace fizmo {
namespace gpu {

auto Buffer::write(const void* data, std::uint64_t bytes, std::uint64_t offset) const -> Result {
    if (!m_impl) return Result::NotInitialized;
    if (!data || offset + bytes > m_impl->desc.size) return Result::InvalidArgument;
    return bytes ? m_impl->write(data, bytes, offset) : Result::Success;
}

auto Buffer::read(void* out, std::uint64_t bytes, std::uint64_t offset) const -> Result {
    if (!m_impl) return Result::NotInitialized;
    if (!out || offset + bytes > m_impl->desc.size) return Result::InvalidArgument;
    return m_impl->read(out, bytes, offset);
}

auto BindGroupEntry::sampled(std::uint32_t binding, const Texture& texture, const Sampler& sampler, std::uint32_t element) noexcept -> BindGroupEntry {
    BindGroupEntry e;
    e.binding = binding; e.element = element; e.texture = &texture; e.sampler = &sampler;
    return e;
}

std::uint32_t CommandList::copy_record(const detail::CopyRecord& c) {
    m_stream.copies.push_back(c);
    return static_cast<std::uint32_t>(m_stream.copies.size() - 1);
}

void CommandList::begin_render_pass(const RenderPassDesc& desc) {
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

void CommandList::set_pipeline(const RenderPipeline& pipeline) {
    if (!pipeline.valid()) return;
    push(detail::Op::SetRenderPipeline).render.pipeline = pipeline.impl();
}

void CommandList::set_pipeline(const ComputePipeline& pipeline) {
    if (!pipeline.valid()) return;
    push(detail::Op::SetComputePipeline).compute.pipeline = pipeline.impl();
}

void CommandList::set_bind_group(std::uint32_t index, const BindGroup& group) {
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

void CommandList::push_constants(const void* data, std::uint32_t size, std::uint32_t offset) {
    if (!data || size == 0) return;
    const std::uint32_t at = m_stream.store(data, size);
    detail::Command& c = push(detail::Op::PushConstants);
    c.bytes.at = at;
    c.bytes.size = size;
    c.bytes.offset = offset;
}

void CommandList::set_vertex_buffer(std::uint32_t slot, const Buffer& buffer, std::uint64_t offset) {
    if (!buffer.valid()) return;
    detail::Command& c = push(detail::Op::SetVertexBuffer);
    c.vertex.slot = slot;
    c.vertex.buffer = buffer.impl();
    c.vertex.offset = offset;
}

void CommandList::set_index_buffer(const Buffer& buffer, IndexType type, std::uint64_t offset) {
    if (!buffer.valid()) return;
    detail::Command& c = push(detail::Op::SetIndexBuffer);
    c.index.buffer = buffer.impl();
    c.index.offset = offset;
    c.index.type = type;
}

void CommandList::set_viewport(const Viewport& v) {
    detail::Command& c = push(detail::Op::SetViewport);
    c.viewport.x = v.x; c.viewport.y = v.y; c.viewport.w = v.width; c.viewport.h = v.height;
    c.viewport.min_depth = v.min_depth; c.viewport.max_depth = v.max_depth;
}

void CommandList::set_scissor(const Rect2D& r) {
    detail::Command& c = push(detail::Op::SetScissor);
    c.scissor.x = r.offset.x; c.scissor.y = r.offset.y; c.scissor.w = r.extent.width; c.scissor.h = r.extent.height;
}

void CommandList::draw(std::uint32_t count, std::uint32_t instances, std::uint32_t first, std::uint32_t first_instance) {
    if (count == 0 || instances == 0) return;
    detail::Command& c = push(detail::Op::Draw);
    c.draw.count = count; c.draw.instances = instances; c.draw.first = first; c.draw.first_instance = first_instance;
}

void CommandList::draw_indexed(std::uint32_t count, std::uint32_t instances, std::uint32_t first_index, std::int32_t vertex_offset, std::uint32_t first_instance) {
    if (count == 0 || instances == 0) return;
    detail::Command& c = push(detail::Op::DrawIndexed);
    c.indexed.count = count; c.indexed.instances = instances; c.indexed.first_index = first_index;
    c.indexed.vertex_offset = vertex_offset; c.indexed.first_instance = first_instance;
}

void CommandList::draw_indexed_indirect(const Buffer& commands, std::uint64_t offset, std::uint32_t count, std::uint32_t stride) {
    if (!commands.valid() || count == 0) return;
    detail::Command& c = push(detail::Op::DrawIndexedIndirect);
    c.indirect.buffer = commands.impl(); c.indirect.offset = offset; c.indirect.count = count; c.indirect.stride = stride;
}

void CommandList::dispatch(std::uint32_t x, std::uint32_t y, std::uint32_t z) {
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

void CommandList::copy_buffer(const Buffer& src, std::uint64_t src_offset, const Buffer& dst, std::uint64_t dst_offset, std::uint64_t size) {
    if (m_in_pass || !src.valid() || !dst.valid() || size == 0) return;
    detail::CopyRecord r;
    r.src_buffer = src.impl(); r.src_offset = src_offset;
    r.dst_buffer = dst.impl(); r.dst_offset = dst_offset;
    r.size = size;
    push(detail::Op::CopyBuffer).copy.index = copy_record(r);
}

void CommandList::copy_buffer_to_texture(const Buffer& src, std::uint64_t offset, std::uint32_t row_pixels, const Texture& dst, const TextureRegion& region) {
    if (m_in_pass || !src.valid() || !dst.valid()) return;
    detail::CopyRecord r;
    r.src_buffer = src.impl(); r.src_offset = offset; r.row_pixels = row_pixels;
    r.dst_texture = dst.impl(); r.dst_region = region;
    push(detail::Op::CopyBufferToTexture).copy.index = copy_record(r);
}

void CommandList::copy_texture_to_buffer(const Texture& src, const TextureRegion& region, const Buffer& dst, std::uint64_t offset) {
    if (m_in_pass || !src.valid() || !dst.valid()) return;
    detail::CopyRecord r;
    r.src_texture = src.impl(); r.src_region = region;
    r.dst_buffer = dst.impl(); r.dst_offset = offset;
    push(detail::Op::CopyTextureToBuffer).copy.index = copy_record(r);
}

void CommandList::copy_texture(const Texture& src, const TextureRegion& src_region, const Texture& dst, const TextureRegion& dst_region) {
    if (m_in_pass || !src.valid() || !dst.valid()) return;
    detail::CopyRecord r;
    r.src_texture = src.impl(); r.src_region = src_region;
    r.dst_texture = dst.impl(); r.dst_region = dst_region;
    r.dst_region.extent = src_region.extent;
    push(detail::Op::CopyTexture).copy.index = copy_record(r);
}

void CommandList::copy_texture(const Texture& src, const Texture& dst, const Rect2D& area) {
    TextureRegion region;
    region.origin = { area.offset.x, area.offset.y, 0 };
    region.extent = { area.extent.width, area.extent.height, 1 };
    copy_texture(src, region, dst, region);
}

void CommandList::blit_texture(const Texture& src, const Rect2D& src_rect, const Texture& dst, const Rect2D& dst_rect, Filter filter) {
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

void CommandList::generate_mipmaps(const Texture& texture) {
    if (m_in_pass || !texture.valid() || texture.mip_levels() < 2) return;
    push(detail::Op::GenerateMipmaps).mips.texture = texture.impl();
}

void CommandList::reset_queries(const QuerySet& set, std::uint32_t first, std::uint32_t count) {
    if (m_in_pass || !set.valid() || count == 0 || first + count > set.count()) return;
    detail::Command& c = push(detail::Op::ResetQueries);
    c.query.set = set.impl(); c.query.first = first; c.query.count = count;
}

void CommandList::write_timestamp(const QuerySet& set, std::uint32_t index) {
    if (!set.valid() || index >= set.count()) return;
    detail::Command& c = push(detail::Op::WriteTimestamp);
    c.query.set = set.impl(); c.query.first = index; c.query.count = 1;
}

void CommandList::push_label(const char* name) {
    if (!name) return;
    detail::Command& c = push(detail::Op::PushLabel);
    c.bytes.at = m_stream.store(name, std::strlen(name) + 1);
}

auto Device::create(const DeviceDesc& desc) -> Result {
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

std::uint32_t Device::max_samples(Format color, Format depth, std::uint32_t wanted) const noexcept {
    for (std::uint32_t s = wanted; s > 1; s >>= 1) {
        if (s > caps().max_samples) continue;
        if (!supports(color, TextureUsage::RenderTarget, s)) continue;
        if (depth != Format::Undefined && !supports(depth, TextureUsage::DepthStencil, s)) continue;
        return s;
    }

    return 1;
}

auto Device::present(const Texture& source, Filter filter) -> Result {
    if (!m_backend) return Result::NotInitialized;
    if (!source.valid()) return Result::InvalidArgument;
    return m_backend->present(*source.impl(), filter);
}

auto Device::write_texture(const Texture& texture, const TextureRegion& region, const void* data, std::uint32_t row_pixels) -> Result {
    if (!m_backend) return Result::NotInitialized;
    if (!texture.valid() || !data || region.extent.width == 0 || region.extent.height == 0 || region.extent.depth == 0) return Result::InvalidArgument;
    return m_backend->write_texture(*texture.impl(), region, data, row_pixels);
}

auto Device::write_texture(const Texture& texture, const void* data) -> Result {
    TextureRegion all;
    all.extent = texture.desc().extent;
    return write_texture(texture, all, data);
}

auto Device::read_texture(const Texture& texture, const TextureRegion& region, void* out) -> Result {
    if (!m_backend) return Result::NotInitialized;
    if (!texture.valid() || !out || region.extent.width == 0 || region.extent.height == 0) return Result::InvalidArgument;
    return m_backend->read_texture(*texture.impl(), region, out);
}

bool Device::read_timestamps(const QuerySet& set, std::uint32_t first, std::uint32_t count, std::uint64_t* nanoseconds) {
    if (!m_backend || !set.valid() || !nanoseconds || count == 0 || first + count > set.count()) return false;
    return m_backend->read_timestamps(*set.impl(), first, count, nanoseconds);
}

Result Buffer::create(Device& device, const BufferDesc& desc) {
    m_impl.reset();
    if (!device.valid()) return Result::NotInitialized;
    if (desc.size == 0) return Result::InvalidArgument;
    return Device::named(device.m_backend->create_buffer(desc, m_impl), m_impl, desc.name);
}

Result Texture::create(Device& device, const TextureDesc& desc) {
    m_impl.reset();
    if (!device.valid()) return Result::NotInitialized;
    TextureDesc d = desc;
    if (d.mip_levels == 0) d.mip_levels = detail::full_mip_count(d.extent);
    d.samples = std::max(d.samples, 1u);
    const Result v = detail::validate(d);
    if (failed(v)) return v;
    return Device::named(device.m_backend->create_texture(d, m_impl), m_impl, desc.name);
}

Result Sampler::create(Device& device, const SamplerDesc& desc) {
    m_impl.reset();
    if (!device.valid()) return Result::NotInitialized;
    return Device::named(device.m_backend->create_sampler(desc, m_impl), m_impl, desc.name);
}

Result ShaderModule::create(Device& device, const ShaderDesc& desc) {
    m_impl.reset();
    if (!device.valid()) return Result::NotInitialized;
    return Device::named(device.m_backend->create_shader(desc, m_impl), m_impl, desc.name);
}

Result BindGroupLayout::create(Device& device, const BindGroupLayoutDesc& desc) {
    m_impl.reset();
    if (!device.valid()) return Result::NotInitialized;
    return Device::named(device.m_backend->create_bind_group_layout(desc, m_impl), m_impl, desc.name);
}

Result PipelineLayout::create(Device& device, const PipelineLayoutDesc& desc) {
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

Result BindGroup::create(Device& device, const BindGroupDesc& desc) {
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

Result RenderPipeline::create(Device& device, const RenderPipelineDesc& desc) {
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

Result ComputePipeline::create(Device& device, const ComputePipelineDesc& desc) {
    m_impl.reset();
    if (!device.valid()) return Result::NotInitialized;
    if (!desc.shader || !desc.shader->valid() || desc.shader->stage() != ShaderStage::Compute || !desc.layout || !desc.layout->valid()) return Result::InvalidArgument;
    if (!device.caps().compute) return Result::Unsupported;
    return Device::named(device.m_backend->create_compute_pipeline(desc.shader->impl(), desc.layout->impl(), desc.name, m_impl), m_impl, desc.name);
}

Result QuerySet::create(Device& device, std::uint32_t count, const char* name) {
    m_impl.reset();
    if (!device.valid()) return Result::NotInitialized;
    if (count == 0) return Result::InvalidArgument;
    if (!device.caps().timestamps) return Result::Unsupported;
    return Device::named(device.m_backend->create_query_set(count, name, m_impl), m_impl, name);
}

} // namespace gpu
} // namespace fizmo
