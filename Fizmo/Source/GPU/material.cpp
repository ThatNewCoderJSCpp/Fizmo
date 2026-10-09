#include "fizmo_library.hpp"
#include "material.hpp"

namespace fizmo {
namespace gpu {

auto Material::binding_type(GlslResource kind) noexcept -> BindingType {
    switch (kind) {
        case GlslResource::UniformBlock: return BindingType::UniformBuffer;
        case GlslResource::StorageBlock: return BindingType::StorageBuffer;
        case GlslResource::StorageImage: return BindingType::StorageTexture;
        default:                         return BindingType::SampledTexture;
    }
}

bool Material::vertex_format(const ShaderInput& in, Format& out, std::uint32_t& size) noexcept {
    if (in.type == ScalarType::Float) {
        switch (in.components) {
            case 1: out = Format::R32Float; size = 4; return true;
            case 2: out = Format::RG32Float; size = 8; return true;
            case 3: out = Format::RGB32Float; size = 12; return true;
            case 4: out = Format::RGBA32Float; size = 16; return true;
            default: return false;
        }
    }
    if (in.type == ScalarType::UInt && in.components == 1) { out = Format::R32Uint; size = 4; return true; }
    if (in.type == ScalarType::Int && in.components == 3) { out = Format::RGB32Sint; size = 12; return true; }
    return false;
}

auto Material::collect(const ShaderBundle& b, std::vector<Slot>& slots, ShaderBlock& push, ShaderStage& push_stages, std::vector<UniformBuffer>& uniforms) -> Result {
    for (const GlslBinding& g : b.bindings()) {
        if (g.kind == GlslResource::PushBlock) continue;
        if (g.set >= kMaxGroups) return fail(Result::Unsupported, "binding set exceeds 4 groups in " + b.name());
        const BindingType type = binding_type(g.kind);
        auto it = std::find_if(slots.begin(), slots.end(), [&](const Slot& s) { return s.set == g.set && s.binding == g.binding; });

        if (it != slots.end()) {
            if (it->type != type) return fail(Result::InvalidArgument, "conflicting binding types at set " + std::to_string(g.set) + " binding " + std::to_string(g.binding));
            it->stages |= b.stage();
            continue;
        }

        Slot s;
        s.set = g.set;
        s.binding = g.binding;
        s.type = type;
        s.stages = b.stage();
        s.count = std::max(g.count, 1u);
        s.name = g.name ? g.name : "";
        slots.push_back(s);
    }

    for (const ShaderBlock& blk : b.blocks()) {
        if (blk.kind == BlockKind::Push) {
            if (blk.size > push.size) {
                std::vector<ShaderMember> merged = push.members;
                push = blk;
                for (const ShaderMember& m : merged) if (!push.find(m.name)) push.members.push_back(m);
            } else {
                for (const ShaderMember& m : blk.members) if (!push.find(m.name)) push.members.push_back(m);
            }
            push_stages |= b.stage();
            continue;
        }

        if (blk.kind != BlockKind::Uniform) continue;
        auto it = std::find_if(uniforms.begin(), uniforms.end(), [&](const UniformBuffer& u) { return u.set == blk.set && u.binding == blk.binding; });
        if (it != uniforms.end()) {
            if (blk.size > it->block.size) it->block.size = blk.size;
            for (const ShaderMember& m : blk.members) if (!it->block.find(m.name)) it->block.members.push_back(m);
            continue;
        }
        UniformBuffer u;
        u.set = blk.set;
        u.binding = blk.binding;
        u.block = blk;
        uniforms.push_back(std::move(u));
    }

    return Result::Success;
}

auto Material::ensure_defaults() -> Result {
    if (!m_default_sampler) {
        auto s = std::make_shared<Sampler>();
        SamplerDesc sd;
        sd.name = "fizmo material sampler";
        if (failed(s->create(*m_device, sd))) return fail(Result::BackendFailed, "cannot create default sampler");
        m_default_sampler = s;
    }

    if (!m_white) {
        auto t = std::make_shared<Texture>();
        TextureDesc td;
        td.extent = { 1, 1, 1 };
        td.name = "fizmo material white";
        if (failed(t->create(*m_device, td))) return fail(Result::BackendFailed, "cannot create default texture");
        const std::uint8_t white[4] = { 255, 255, 255, 255 };
        m_device->write_texture(*t, white);
        m_white = t;
    }

    return Result::Success;
}

auto Material::build(Resources& res, std::vector<Slot>& slots, ShaderBlock& push, ShaderStage& push_stages, std::uint32_t& group_count) -> Result {
    const MaterialDesc& d = m_desc;
    if (is_compute()) {
        if (d.compute.stage() != ShaderStage::Compute) return fail(Result::InvalidArgument, "compute bundle is not a compute shader");
        Result r = collect(d.compute, slots, push, push_stages, res.uniforms);
        if (failed(r)) return r;
        if (failed(res.cs.create(*m_device, d.compute.desc()))) return fail(Result::ShaderFailed, "compute shader failed: " + m_device->last_error());
    } else {
        if (!d.vertex.valid() || !d.fragment.valid()) return fail(Result::InvalidArgument, "material needs a vertex and a fragment shader");
        if (d.vertex.stage() != ShaderStage::Vertex || d.fragment.stage() != ShaderStage::Fragment) return fail(Result::InvalidArgument, "shader stages do not match vertex/fragment");
        Result r = collect(d.vertex, slots, push, push_stages, res.uniforms);
        if (failed(r)) return r;
        r = collect(d.fragment, slots, push, push_stages, res.uniforms);
        if (failed(r)) return r;
        if (failed(res.vs.create(*m_device, d.vertex.desc()))) return fail(Result::ShaderFailed, "vertex shader failed: " + m_device->last_error());
        if (failed(res.fs.create(*m_device, d.fragment.desc()))) return fail(Result::ShaderFailed, "fragment shader failed: " + m_device->last_error());
    }

    group_count = 0;
    for (const Slot& s : slots) group_count = std::max(group_count, s.set + 1);

    for (std::uint32_t g = 0; g < group_count; ++g) {
        std::vector<BindGroupLayoutEntry> entries;
        for (const Slot& s : slots) {
            if (s.set != g) continue;
            BindGroupLayoutEntry e;
            e.binding = s.binding;
            e.type = s.type;
            e.stages = s.stages;
            e.count = s.count;
            if (s.type == BindingType::StorageTexture) e.storage_format = Format::RGBA8Unorm;
            entries.push_back(e);
        }
        BindGroupLayoutDesc ld;
        ld.entries = { entries.data(), entries.size() };
        if (failed(res.layouts[g].create(*m_device, ld))) return fail(Result::BackendFailed, "bind group layout failed: " + m_device->last_error());
    }

    PipelineLayoutDesc pl;
    for (std::uint32_t g = 0; g < group_count; ++g) pl.groups.push_back(&res.layouts[g]);
    pl.push_constant_size = (push.size + 3u) & ~3u;
    pl.push_constant_stages = push_stages == ShaderStage::None ? ShaderStage::Graphics : push_stages;
    if (failed(res.layout.create(*m_device, pl))) return fail(Result::BackendFailed, "pipeline layout failed: " + m_device->last_error());

    if (is_compute()) {
        ComputePipelineDesc cd;
        cd.shader = &res.cs;
        cd.layout = &res.layout;
        cd.name = d.name.empty() ? nullptr : d.name.c_str();
        if (failed(res.compute.create(*m_device, cd))) return fail(Result::BackendFailed, "compute pipeline failed: " + m_device->last_error());
    } else {
        RenderPipelineDesc pd;
        pd.vertex = &res.vs;
        pd.fragment = &res.fs;
        pd.layout = &res.layout;
        pd.vertex_bindings = d.vertex_bindings;
        pd.vertex_attributes = d.vertex_attributes;

        if (pd.vertex_attributes.empty() && !d.vertex.inputs().empty()) {
            std::vector<ShaderInput> inputs = d.vertex.inputs();
            std::sort(inputs.begin(), inputs.end(), [](const ShaderInput& a, const ShaderInput& b) { return a.location < b.location; });
            std::uint32_t offset = 0;
            for (const ShaderInput& in : inputs) {
                Format f = Format::Undefined;
                std::uint32_t size = 0;
                if (!vertex_format(in, f, size)) return fail(Result::Unsupported, "cannot derive a vertex format for input " + in.name);
                pd.vertex_attributes.push_back({ in.location, 0, f, offset });
                offset += size;
            }
            if (pd.vertex_bindings.empty()) pd.vertex_bindings.push_back({ 0, offset, VertexRate::PerVertex });
        }

        pd.topology = d.topology;
        pd.cull_mode = d.cull_mode;
        pd.front_face = d.front_face;
        pd.depth = d.depth;
        pd.blend = d.blend;
        pd.color_formats = d.color_formats;
        pd.depth_format = d.depth_format;
        pd.samples = d.samples;
        pd.name = d.name.empty() ? nullptr : d.name.c_str();
        if (failed(res.pipeline.create(*m_device, pd))) return fail(Result::BackendFailed, "render pipeline failed: " + m_device->last_error());
    }

    m_ring = std::max(1u, m_device->frames_in_flight());
    for (UniformBuffer& u : res.uniforms) {
        u.data.assign(std::max(u.block.size, 16u), 0);
        u.ring.clear();
        for (std::uint32_t i = 0; i < m_ring; ++i) {
            auto buf = std::make_unique<Buffer>();
            BufferDesc bd;
            bd.size = (u.data.size() + 255u) & ~std::uint64_t(255);
            bd.usage = BufferUsage::Uniform;
            bd.memory = MemoryAccess::Upload;
            bd.name = "fizmo material uniforms";
            if (failed(buf->create(*m_device, bd))) return fail(Result::OutOfMemory, "uniform buffer failed: " + m_device->last_error());
            u.ring.push_back(std::move(buf));
        }
        u.dirty.assign(m_ring, true);
    }

    res.groups.assign(m_ring, {});
    return Result::Success;
}

void Material::carry_values(const Resources& from) {
    for (UniformBuffer& u : m_res.uniforms) {
        for (const UniformBuffer& old : from.uniforms) {
            if (old.block.name != u.block.name) continue;
            for (const ShaderMember& m : u.block.members) {
                const ShaderMember* om = old.block.find(m.name);
                if (!om || om->type != m.type || om->rows != m.rows || om->columns != m.columns) continue;
                const std::uint32_t n = std::min(m.array_count, om->array_count);
                for (std::uint32_t i = 0; i < n; ++i) {
                    const std::size_t dst = m.offset + i * m.array_stride, src = om->offset + i * om->array_stride;
                    const std::size_t bytes = m.element_size();
                    if (dst + bytes <= u.data.size() && src + bytes <= old.data.size()) std::memcpy(&u.data[dst], &old.data[src], bytes);
                }
            }
        }
    }
}

std::uint8_t* Material::locate(const std::string& full, const ShaderMember*& member, std::uint32_t& index, std::vector<bool>** dirty) {
    std::string name = full;
    index = 0;
    std::string block_name;
    const std::size_t dot = name.find('.');
    if (dot != std::string::npos) block_name = name.substr(0, dot);

    auto search = [&](ShaderBlock& blk, std::vector<std::uint8_t>& data, const std::string& n) -> std::uint8_t* {
        const ShaderMember* m = blk.find(n);
        std::uint32_t idx = 0;
        if (!m && !n.empty() && n.back() == ']') {
            const std::size_t open = n.rfind('[');
            if (open != std::string::npos) {
                m = blk.find(n.substr(0, open));
                idx = static_cast<std::uint32_t>(std::strtoul(n.c_str() + open + 1, nullptr, 10));
            }
        }
        if (!m || idx >= m->array_count) return nullptr;
        member = m;
        index = idx;
        const std::size_t at = m->offset + static_cast<std::size_t>(idx) * m->array_stride;
        if (at + m->element_size() > data.size()) return nullptr;
        return &data[at];
    };

    for (UniformBuffer& u : m_res.uniforms) {
        std::uint8_t* p = search(u.block, u.data, name);
        if (!p && !block_name.empty() && block_name == u.block.name) p = search(u.block, u.data, name.substr(dot + 1));
        if (p) { if (dirty) *dirty = &u.dirty; return p; }
    }

    std::uint8_t* p = search(m_push_block, m_push, name);
    if (!p && !block_name.empty() && block_name == m_push_block.name) p = search(m_push_block, m_push, name.substr(dot + 1));
    if (p && dirty) *dirty = nullptr;
    return p;
}

bool Material::write_member(const std::string& name, const void* src, std::size_t bytes, bool floats) {
    const ShaderMember* m = nullptr;
    std::uint32_t index = 0;
    std::vector<bool>* dirty = nullptr;
    std::uint8_t* dst = locate(name, m, index, &dirty);
    if (!dst || !m) return false;

    if (floats && m->is_matrix() && m->type == ScalarType::Float) {
        const float* f = static_cast<const float*>(src);
        const std::size_t count = bytes / sizeof(float);
        const std::uint32_t stride = m->matrix_stride ? m->matrix_stride : m->rows * 4u;
        for (std::uint32_t c = 0; c < m->columns && (c + 1) * m->rows <= count; ++c) std::memcpy(dst + c * stride, f + c * m->rows, m->rows * sizeof(float));
    } else {
        std::memcpy(dst, src, std::min<std::size_t>(bytes, m->element_size()));
    }

    if (dirty) dirty->assign(dirty->size(), true);
    return true;
}

auto Material::find_slot(const std::string& name) const noexcept -> const Slot* {
    for (const Slot& s : m_slots) if (s.name == name) return &s;
    for (const UniformBuffer& u : m_res.uniforms) if (u.block.name == name) {
        for (const Slot& s : m_slots) if (s.set == u.set && s.binding == u.binding) return &s;
    }
    return nullptr;
}

bool Material::build_groups(std::uint32_t ring) {
    auto& groups = m_res.groups[ring];

    for (std::uint32_t g = 0; g < m_group_count; ++g) {
        std::vector<BindGroupEntry> entries;
        for (const Slot& s : m_slots) {
            if (s.set != g) continue;
            switch (s.type) {
                case BindingType::UniformBuffer: {
                    const UniformBuffer* u = nullptr;
                    for (const UniformBuffer& x : m_res.uniforms) if (x.set == s.set && x.binding == s.binding) u = &x;
                    if (!u) return false;
                    entries.push_back(BindGroupEntry::uniform(s.binding, *u->ring[ring], 0, u->data.size()));
                    break;
                }
                case BindingType::SampledTexture: {
                    const TextureBinding* t = texture_slot(s.set, s.binding);
                    const Texture* tex = t && t->texture ? t->texture : m_white.get();
                    const Sampler* smp = t && t->sampler ? t->sampler : m_default_sampler.get();
                    for (std::uint32_t e = 0; e < s.count; ++e) entries.push_back(BindGroupEntry::sampled(s.binding, *tex, *smp, e));
                    break;
                }
                case BindingType::StorageBuffer: {
                    const StorageBinding* b = storage_slot(s.set, s.binding);
                    if (b && b->buffer) entries.push_back(BindGroupEntry::storage(s.binding, *b->buffer, b->offset, b->size));
                    else {
                        if (!m_dummy_storage) {
                            auto buf = std::make_shared<Buffer>();
                            BufferDesc bd;
                            bd.size = 256;
                            bd.usage = BufferUsage::Storage;
                            bd.memory = MemoryAccess::Upload;
                            if (failed(buf->create(*m_device, bd))) return false;
                            m_dummy_storage = buf;
                        }
                        entries.push_back(BindGroupEntry::storage(s.binding, *m_dummy_storage, 0, 256));
                    }
                    break;
                }
                case BindingType::StorageTexture: {
                    const StorageBinding* b = storage_slot(s.set, s.binding);
                    if (!b || !b->image) return false;
                    entries.push_back(BindGroupEntry::storage_texture(s.binding, *b->image));
                    break;
                }
            }
        }

        BindGroupDesc bd;
        bd.layout = &m_res.layouts[g];
        bd.entries = { entries.data(), entries.size() };
        BindGroup group;
        if (failed(group.create(*m_device, bd))) return false;
        groups[g] = group;
    }

    return true;
}

void Material::advance(std::uint32_t frame_index) {
    if (frame_index == m_last_frame) return;
    m_last_frame = frame_index;
    for (Retired& r : m_retired) --r.frames;
    m_retired.erase(std::remove_if(m_retired.begin(), m_retired.end(), [](const Retired& r) { return r.frames <= 0; }), m_retired.end());
}

bool Material::prepare(std::uint32_t frame_index) {
    if (!valid()) return false;
    advance(frame_index);
    const std::uint32_t ring = frame_index % m_ring;

    for (UniformBuffer& u : m_res.uniforms) {
        if (!u.dirty[ring]) continue;
        if (failed(u.ring[ring]->write(u.data.data(), u.data.size()))) return false;
        u.dirty[ring] = false;
    }

    if (m_groups_dirty[ring] || (m_group_count > 0 && !m_res.groups[ring][0].valid())) {
        if (!build_groups(ring)) return false;
        m_groups_dirty[ring] = false;
    }

    return true;
}

auto Material::install(MaterialDesc desc, bool keep_values) -> Result {
    MaterialDesc previous = std::move(m_desc);
    m_desc = std::move(desc);
    Resources res;
    std::vector<Slot> slots;
    ShaderBlock push;
    ShaderStage push_stages = ShaderStage::None;
    std::uint32_t groups = 0;
    Result r = ensure_defaults();
    if (succeeded(r)) r = build(res, slots, push, push_stages, groups);
    if (failed(r)) { m_desc = std::move(previous); return r; }

    if (m_res.pipeline.valid() || m_res.compute.valid()) m_retired.push_back({ static_cast<int>(m_ring) + 1, std::move(m_res) });
    const std::vector<std::uint8_t> old_push = m_push;
    const ShaderBlock old_push_block = m_push_block;
    m_res = std::move(res);
    m_slots = std::move(slots);
    m_push_block = std::move(push);
    m_push_stages = push_stages;
    m_group_count = groups;
    m_push.assign((m_push_block.size + 3u) & ~3u, 0);

    if (keep_values && !m_retired.empty()) {
        carry_values(m_retired.back().res);
        for (const ShaderMember& m : m_push_block.members) {
            const ShaderMember* om = old_push_block.find(m.name);
            if (!om || om->type != m.type || om->rows != m.rows || om->columns != m.columns) continue;
            const std::size_t bytes = m.element_size();
            if (m.offset + bytes <= m_push.size() && om->offset + bytes <= old_push.size()) std::memcpy(&m_push[m.offset], &old_push[om->offset], bytes);
        }
    }

    for (UniformBuffer& u : m_res.uniforms) u.dirty.assign(m_ring, true);
    m_groups_dirty.assign(m_ring, true);
    m_error.clear();
    return Result::Success;
}

auto Material::create(Device& device, MaterialDesc desc) -> Result {
    if (!device.valid()) return fail(Result::NotInitialized, "device is not initialised");
    m_device = &device;
    m_textures.clear();
    m_storage.clear();
    m_retired.clear();
    m_res = Resources{};
    return install(std::move(desc), false);
}

auto Material::reload(const ShaderBundle* vertex, const ShaderBundle* fragment, const ShaderBundle* compute) -> Result {
    if (!m_device) return Result::NotInitialized;
    MaterialDesc d = m_desc;
    if (vertex) d.vertex = *vertex;
    if (fragment) d.fragment = *fragment;
    if (compute) d.compute = *compute;
    return install(std::move(d), true);
}

void Material::destroy() noexcept {
    m_res = Resources{};
    m_retired.clear();
    m_slots.clear();
    m_textures.clear();
    m_storage.clear();
    m_default_sampler.reset();
    m_white.reset();
    m_dummy_storage.reset();
    m_device = nullptr;
}

bool Material::has(const std::string& name) {
    const ShaderMember* m = nullptr;
    std::uint32_t i = 0;
    return locate(name, m, i, nullptr) != nullptr || find_slot(name) != nullptr;
}

std::vector<std::string> Material::uniform_names() const {
    std::vector<std::string> out;
    for (const UniformBuffer& u : m_res.uniforms) for (const ShaderMember& m : u.block.members) out.push_back(u.block.name + "." + m.name);
    for (const ShaderMember& m : m_push_block.members) out.push_back(m_push_block.name + "." + m.name);
    return out;
}

bool Material::set_push(const void* data, std::size_t bytes, std::size_t offset) {
    if (offset + bytes > m_push.size()) return false;
    std::memcpy(m_push.data() + offset, data, bytes);
    return true;
}

bool Material::set_texture(const std::string& name, const Texture& texture, const Sampler* sampler) {
    const Slot* s = find_slot(name);
    if (!s || s->type != BindingType::SampledTexture) return false;
    TextureBinding* t = texture_slot(s->set, s->binding);
    if (!t) { m_textures.push_back({ key(s->set, s->binding), {} }); t = &m_textures.back().second; }
    if (t->texture == &texture && t->sampler == sampler) return true;
    t->texture = &texture;
    t->sampler = sampler;
    m_groups_dirty.assign(m_ring, true);
    return true;
}

bool Material::set_storage(const std::string& name, const Buffer& buffer, std::uint64_t offset, std::uint64_t size) {
    const Slot* s = find_slot(name);
    if (!s || s->type != BindingType::StorageBuffer) return false;
    StorageBinding* b = storage_slot(s->set, s->binding);
    if (!b) { m_storage.push_back({ key(s->set, s->binding), {} }); b = &m_storage.back().second; }
    b->buffer = &buffer;
    b->offset = offset;
    b->size = size ? size : buffer.size() - offset;
    m_groups_dirty.assign(m_ring, true);
    return true;
}

bool Material::set_image(const std::string& name, const Texture& texture) {
    const Slot* s = find_slot(name);
    if (!s || s->type != BindingType::StorageTexture) return false;
    StorageBinding* b = storage_slot(s->set, s->binding);
    if (!b) { m_storage.push_back({ key(s->set, s->binding), {} }); b = &m_storage.back().second; }
    b->image = &texture;
    m_groups_dirty.assign(m_ring, true);
    return true;
}

bool Material::bind(CommandList& cmd, std::uint32_t frame_index) {
    if (!prepare(frame_index)) return false;
    const std::uint32_t ring = frame_index % m_ring;
    if (m_res.compute.valid()) cmd.set_pipeline(m_res.compute);
    else cmd.set_pipeline(m_res.pipeline);
    for (std::uint32_t g = 0; g < m_group_count; ++g) cmd.set_bind_group(g, m_res.groups[ring][g]);
    if (!m_push.empty()) cmd.push_constants(m_push.data(), static_cast<std::uint32_t>(m_push.size()));
    return true;
}

bool Material::bind_groups(CommandList& cmd, std::uint32_t frame_index) {
    if (!prepare(frame_index)) return false;
    const std::uint32_t ring = frame_index % m_ring;
    for (std::uint32_t g = 0; g < m_group_count; ++g) cmd.set_bind_group(g, m_res.groups[ring][g]);
    return true;
}

bool Material::draw(CommandList& cmd, std::uint32_t frame_index, const Buffer& vertices, std::uint32_t count, std::uint32_t instances) {
    if (!bind(cmd, frame_index)) return false;
    cmd.set_vertex_buffer(0, vertices);
    cmd.draw(count, instances);
    return true;
}

bool Material::dispatch(CommandList& cmd, std::uint32_t frame_index, std::uint32_t x, std::uint32_t y, std::uint32_t z) {
    if (!m_res.compute.valid() || !bind(cmd, frame_index)) return false;
    cmd.dispatch(x, y, z);
    return true;
}

} // namespace gpu
} // namespace fizmo
