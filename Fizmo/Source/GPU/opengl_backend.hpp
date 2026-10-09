#ifndef FIZMO_GPU_OPENGL_BACKEND_HPP
#define FIZMO_GPU_OPENGL_BACKEND_HPP

#include "backend.hpp"

#if defined(OS_WINDOWS) || defined(OS_LINUX)

#include "../OpenGL/include.hpp"
#include <array>
#include <cmath>
#include <map>
#include <tuple>

namespace fizmo {
namespace gpu {
namespace detail {
namespace glb {

namespace ogl = ::fizmo::opengl;
namespace gl  = ::fizmo::opengl::gl;
using Enum = ogl::native::Enum;
using Uint = ogl::native::Uint;
using Int  = ogl::native::Int;

struct PixelFormat {
    Enum internal = 0;
    Enum format   = 0;
    Enum type     = 0;
};

inline PixelFormat pixel_format(Format f) noexcept {
    switch (f) {
        case Format::R8Unorm:        return { gl::R8, gl::RED, gl::UNSIGNED_BYTE };
        case Format::RGBA8Unorm:     return { gl::RGBA8, gl::RGBA, gl::UNSIGNED_BYTE };
        case Format::RGBA8Srgb:      return { gl::SRGB8_ALPHA8, gl::RGBA, gl::UNSIGNED_BYTE };
        case Format::BGRA8Unorm:     return { gl::RGBA8, gl::BGRA, gl::UNSIGNED_BYTE };
        case Format::R16Float:       return { gl::R16F, gl::RED, gl::HALF_FLOAT };
        case Format::RGBA16Float:    return { gl::RGBA16F, gl::RGBA, gl::HALF_FLOAT };
        case Format::R32Uint:        return { gl::R32UI, gl::RED_INTEGER, gl::UNSIGNED_INT };
        case Format::R32Float:       return { gl::R32F, gl::RED, gl::FLOAT };
        case Format::RG32Float:      return { gl::RG32F, gl::RG, gl::FLOAT };
        case Format::RGB32Float:     return { gl::RGB32F, gl::RGB_FORMAT, gl::FLOAT };
        case Format::RGBA32Float:    return { gl::RGBA32F, gl::RGBA, gl::FLOAT };
        case Format::RGB32Sint:      return { gl::RGB32I, gl::RGB_INTEGER, gl::INT };
        case Format::D16Unorm:       return { gl::DEPTH_COMPONENT16, gl::DEPTH_COMPONENT, gl::UNSIGNED_SHORT };
        case Format::D24UnormS8Uint: return { gl::DEPTH24_STENCIL8, gl::DEPTH_STENCIL, gl::UNSIGNED_INT_24_8 };
        case Format::D32Float:       return { gl::DEPTH_COMPONENT32F, gl::DEPTH_COMPONENT, gl::FLOAT };
        default:                     return {};
    }
}

inline ogl::Format attribute_source(Format f) noexcept {
    switch (f) {
        case Format::R32Float:    return ogl::Format::R32Float;
        case Format::RG32Float:   return ogl::Format::RG32Float;
        case Format::RGB32Float:  return ogl::Format::RGB32Float;
        case Format::RGBA32Float: return ogl::Format::RGBA32Float;
        case Format::R32Uint:     return ogl::Format::R32Uint;
        case Format::RGB32Sint:   return ogl::Format::RGB32Sint;
        case Format::RGBA8Unorm:  return ogl::Format::RGBA8Unorm;
        default:                  return ogl::Format::Undefined;
    }
}

inline Enum compare_op(CompareOp op) noexcept {
    switch (op) {
        case CompareOp::Never:        return gl::NEVER;
        case CompareOp::Less:         return gl::LESS;
        case CompareOp::Equal:        return gl::EQUAL;
        case CompareOp::LessEqual:    return gl::LEQUAL;
        case CompareOp::Greater:      return gl::GREATER;
        case CompareOp::NotEqual:     return gl::NOTEQUAL;
        case CompareOp::GreaterEqual: return gl::GEQUAL;
        case CompareOp::Always:       return gl::ALWAYS;
    }
    return gl::ALWAYS;
}

inline Enum blend_factor(BlendFactor f) noexcept {
    switch (f) {
        case BlendFactor::Zero:             return gl::ZERO;
        case BlendFactor::One:              return gl::ONE;
        case BlendFactor::SrcColor:         return gl::SRC_COLOR;
        case BlendFactor::OneMinusSrcColor: return gl::ONE_MINUS_SRC_COLOR;
        case BlendFactor::DstColor:         return gl::DST_COLOR;
        case BlendFactor::OneMinusDstColor: return gl::ONE_MINUS_DST_COLOR;
        case BlendFactor::SrcAlpha:         return gl::SRC_ALPHA;
        case BlendFactor::OneMinusSrcAlpha: return gl::ONE_MINUS_SRC_ALPHA;
        case BlendFactor::DstAlpha:         return gl::DST_ALPHA;
        case BlendFactor::OneMinusDstAlpha: return gl::ONE_MINUS_DST_ALPHA;
    }
    return gl::ONE;
}

inline Enum address_mode(AddressMode m) noexcept {
    return m == AddressMode::Repeat ? gl::REPEAT : m == AddressMode::MirroredRepeat ? gl::MIRRORED_REPEAT : gl::CLAMP_TO_EDGE;
}

inline Enum topology(Topology t) noexcept {
    return t == Topology::LineList ? gl::LINES : t == Topology::PointList ? gl::POINTS : gl::TRIANGLES;
}

inline Enum storage_access(StorageAccess a) noexcept {
    return a == StorageAccess::ReadOnly ? gl::READ_ONLY : a == StorageAccess::WriteOnly ? gl::WRITE_ONLY : gl::READ_WRITE;
}

inline std::uint64_t align_up(std::uint64_t v, std::uint64_t a) noexcept { return a ? (v + a - 1) / a * a : v; }

class Core;

struct Buffer final : BufferImpl {
    std::shared_ptr<Core> core;
    Uint                  id = 0;

    ~Buffer() override;
    Result write(const void* data, std::uint64_t bytes, std::uint64_t offset) override;
    Result read(void* out, std::uint64_t bytes, std::uint64_t offset) override;
    void   discard() override;
};

struct Texture final : TextureImpl {
    std::shared_ptr<Core> core;
    Uint                  id     = 0;
    Enum                  target = gl::TEXTURE_2D;
    PixelFormat           pixel;
    std::uint64_t         bytes  = 0;

    ~Texture() override;
};

struct Sampler final : SamplerImpl {
    std::shared_ptr<Core> core;
    Uint                  id = 0;

    ~Sampler() override;
};

struct BindingName {
    std::string   name;
    GlslResource  kind    = GlslResource::Texture;
    std::uint32_t set     = 0;
    std::uint32_t binding = 0;
    std::uint32_t count   = 1;
};

struct Shader final : ShaderImpl {
    std::shared_ptr<Core>    core;
    Uint                     id = 0;
    std::vector<BindingName> bindings;

    ~Shader() override;
};

struct BindGroupLayout final : BindGroupLayoutImpl {};

struct Slot {
    std::uint32_t base     = 0;
    std::uint32_t capacity = 0;
};

struct PipelineLayout final : PipelineLayoutImpl {
    std::map<std::uint64_t, Slot> textures;
    std::map<std::uint64_t, Slot> images;
    std::map<std::uint64_t, Slot> uniforms;
    std::map<std::uint64_t, Slot> storage;

    static std::uint64_t key(std::uint32_t group, std::uint32_t binding) noexcept { return (static_cast<std::uint64_t>(group) << 32) | binding; }

    static const Slot* find(const std::map<std::uint64_t, Slot>& m, std::uint32_t group, std::uint32_t binding) noexcept {
        auto it = m.find(key(group, binding));
        return it == m.end() ? nullptr : &it->second;
    }

    std::uint32_t array_capacity(std::uint32_t group, std::uint32_t binding) const noexcept override {
        if (const Slot* s = find(textures, group, binding)) return s->capacity;
        return PipelineLayoutImpl::array_capacity(group, binding);
    }
};

struct BindGroup final : BindGroupImpl {};

struct Program {
    std::shared_ptr<Core> core;
    Uint                  id = 0;

    ~Program();
};

struct RenderPipeline final : RenderPipelineImpl {
    std::shared_ptr<Program>     program;
    std::vector<VertexBinding>   bindings;
    std::vector<VertexAttribute> attributes;
    std::uint32_t                attribute_mask = 0;
    Enum                         mode           = gl::TRIANGLES;
    CullMode                     cull           = CullMode::None;
    FrontFace                    front          = FrontFace::CounterClockwise;
    DepthState                   depth;
    BlendState                   blend;
    bool                         has_fragment   = true;

    const VertexBinding* binding(std::uint32_t slot) const noexcept {
        for (const VertexBinding& b : bindings) if (b.binding == slot) return &b;
        return nullptr;
    }
};

struct ComputePipeline final : ComputePipelineImpl {
    std::shared_ptr<Program> program;
};

struct QuerySet final : QuerySetImpl {
    std::shared_ptr<Core> core;
    std::vector<Uint>     ids;

    ~QuerySet() override;
};

struct FramebufferKey {
    std::array<std::uint32_t, 14> v{};

    bool operator<(const FramebufferKey& o) const noexcept { return v < o.v; }
};

struct UnitState {
    Enum target  = 0;
    Uint texture = 0;
    Uint sampler = 0;
};

struct RangeState {
    Uint          id     = 0;
    std::uint64_t offset = 0;
    std::uint64_t size   = 0;
};

class Core : public std::enable_shared_from_this<Core> {
public:
    static constexpr std::uint32_t kMaxUnits     = 64;
    static constexpr std::uint32_t kMaxRanges    = 32;
    static constexpr std::uint32_t kMaxAttribs   = 16;
    static constexpr std::uint32_t kPushPoint    = 0;
    static constexpr std::uint64_t kPushCapacity = 256 * 1024;

    ogl::Context                                            ctx;
    Caps                                                    caps;
    std::string                                             error;
    bool                                                    depth_fixup  = false;
    bool                                                    copy_image   = false;
    Uint                                                    vao          = 0;
    Uint                                                    push_buffer  = 0;
    std::uint64_t                                           push_size    = 0;
    std::uint64_t                                           tracked      = 0;
    std::map<FramebufferKey, Uint>                          framebuffers;
    std::map<std::tuple<std::uint64_t, std::uint64_t, std::uint64_t>, std::weak_ptr<Program>> programs;

    Uint                                  bound_program  = 0;
    bool                                  state_known    = false;
    BlendState                            blend;
    DepthState                            depth;
    CullMode                              cull           = CullMode::None;
    FrontFace                             front          = FrontFace::CounterClockwise;
    bool                                  scissor_on     = false;
    std::uint32_t                         attrib_mask    = 0;
    std::array<UnitState, kMaxUnits>      units{};
    std::array<RangeState, kMaxRanges>    ubo_ranges{};

    const ogl::Functions& fn() const noexcept { return ctx.fn(); }
    bool current() const noexcept { return ctx.valid() && ctx.make_current(); }

    ~Core() {
        if (!current()) return;
        for (auto& kv : framebuffers) fn().glDeleteFramebuffers(1, &kv.second);
        framebuffers.clear();
        if (push_buffer) fn().glDeleteBuffers(1, &push_buffer);
        if (vao) fn().glDeleteVertexArrays(1, &vao);
        ctx.destroy();
    }

    void forget_bindings() noexcept {
        units = {};
        ubo_ranges = {};
    }

    void invalidate() noexcept {
        state_known   = false;
        bound_program = 0;
        forget_bindings();
        if (!ctx.valid()) return;
        fn().glBindVertexArray(vao);
        fn().glDisable(gl::SCISSOR_TEST);
        scissor_on = false;
        for (std::uint32_t i = 0; i < kMaxAttribs; ++i) if (attrib_mask & (1u << i)) fn().glDisableVertexAttribArray(i);
        attrib_mask = 0;
    }

    void forget_texture(Uint id) noexcept {
        for (auto it = framebuffers.begin(); it != framebuffers.end();) {
            bool uses = false;
            for (std::size_t i = 0; i < it->first.v.size(); i += 3) uses = uses || it->first.v[i] == id;
            if (uses) { fn().glDeleteFramebuffers(1, &it->second); it = framebuffers.erase(it); }
            else ++it;
        }

        forget_bindings();
    }

    void scratch_bind(const Texture& t) noexcept {
        fn().glActiveTexture(gl::TEXTURE0 + static_cast<Uint>(std::max(ctx.caps().combined_units, 2) - 1));
        fn().glBindTexture(t.target, t.id);
    }

    Enum face_target(const Texture& t, std::uint32_t layer) const noexcept {
        return t.desc.dimension == TextureDimension::Cube ? gl::TEXTURE_CUBE_MAP_POSITIVE_X + layer : t.target;
    }

    static Enum depth_point(const Texture& t) noexcept {
        return has_stencil(t.desc.format) ? gl::DEPTH_STENCIL_ATTACHMENT : gl::DEPTH_ATTACHMENT;
    }

    void attach(Enum point, const Texture& t, std::uint32_t layer, std::uint32_t mip) const noexcept {
        if (t.desc.samples > 1) fn().glFramebufferTexture2D(gl::FRAMEBUFFER, point, gl::TEXTURE_2D_MULTISAMPLE, t.id, 0);
        else if (t.desc.dimension == TextureDimension::Cube) fn().glFramebufferTexture2D(gl::FRAMEBUFFER, point, gl::TEXTURE_CUBE_MAP_POSITIVE_X + layer, t.id, static_cast<Int>(mip));
        else if (t.desc.dimension == TextureDimension::D3) fn().glFramebufferTextureLayer(gl::FRAMEBUFFER, point, t.id, static_cast<Int>(mip), static_cast<Int>(layer));
        else fn().glFramebufferTexture2D(gl::FRAMEBUFFER, point, gl::TEXTURE_2D, t.id, static_cast<Int>(mip));
    }

    Uint framebuffer(const Texture* const* colors, const std::uint32_t* layers, const std::uint32_t* mips, std::uint32_t count, const Texture* depth_tex, std::uint32_t depth_layer) {
        FramebufferKey k;

        for (std::uint32_t i = 0; i < count && i < 4; ++i) {
            k.v[i * 3 + 0] = colors[i] ? colors[i]->id : 0;
            k.v[i * 3 + 1] = layers[i];
            k.v[i * 3 + 2] = mips[i];
        }

        k.v[12] = depth_tex ? depth_tex->id : 0;
        k.v[13] = depth_layer;
        auto it = framebuffers.find(k);

        if (it != framebuffers.end()) {
            fn().glBindFramebuffer(gl::FRAMEBUFFER, it->second);
            return it->second;
        }

        Uint fbo = 0;
        fn().glGenFramebuffers(1, &fbo);
        if (!fbo) return 0;
        fn().glBindFramebuffer(gl::FRAMEBUFFER, fbo);
        Enum draw[4] = {};

        for (std::uint32_t i = 0; i < count && i < 4; ++i) {
            if (colors[i]) attach(gl::COLOR_ATTACHMENT0 + i, *colors[i], layers[i], mips[i]);
            draw[i] = colors[i] ? gl::COLOR_ATTACHMENT0 + i : gl::NONE;
        }

        if (count) {
            fn().glDrawBuffers(static_cast<ogl::native::Sizei>(count), draw);
            fn().glReadBuffer(gl::COLOR_ATTACHMENT0);
        } else {
            fn().glDrawBuffer(gl::NONE);
            fn().glReadBuffer(gl::NONE);
        }

        if (depth_tex) attach(depth_point(*depth_tex), *depth_tex, depth_layer, 0);

        if (fn().glCheckFramebufferStatus(gl::FRAMEBUFFER) != gl::FRAMEBUFFER_COMPLETE) {
            fn().glBindFramebuffer(gl::FRAMEBUFFER, 0);
            fn().glDeleteFramebuffers(1, &fbo);
            error = "incomplete framebuffer";
            return 0;
        }

        framebuffers.emplace(k, fbo);
        return fbo;
    }

    Uint single(const Texture& t, std::uint32_t layer, std::uint32_t mip) {
        if (is_depth(t.desc.format)) return framebuffer(nullptr, nullptr, nullptr, 0, &t, layer);
        const Texture* c = &t;
        return framebuffer(&c, &layer, &mip, 1, nullptr, 0);
    }

    void set_cap(Enum cap, bool on) const noexcept { if (on) fn().glEnable(cap); else fn().glDisable(cap); }

    void scissor(bool on) noexcept {
        if (on == scissor_on) return;
        set_cap(gl::SCISSOR_TEST, on);
        scissor_on = on;
    }

    void use_program(Uint id) noexcept {
        if (bound_program == id) return;
        fn().glUseProgram(id);
        bound_program = id;
    }

    void color_mask(bool on) noexcept {
        if (state_known && blend.write == on) return;
        const ogl::native::Boolean w = on ? 1 : 0;
        fn().glColorMask(w, w, w, w);
        blend.write = on;
    }

    void depth_mask(bool on) noexcept {
        if (state_known && depth.write == on) return;
        fn().glDepthMask(on ? 1 : 0);
        depth.write = on;
    }

    void apply(const RenderPipeline& p) noexcept {
        const bool force = !state_known;
        const BlendState& b = p.blend;

        if (force || b.enable != blend.enable || b.src_color != blend.src_color || b.dst_color != blend.dst_color || b.src_alpha != blend.src_alpha || b.dst_alpha != blend.dst_alpha) {
            set_cap(gl::BLEND, b.enable);
            if (b.enable) fn().glBlendFuncSeparate(blend_factor(b.src_color), blend_factor(b.dst_color), blend_factor(b.src_alpha), blend_factor(b.dst_alpha));
        }

        const bool writes = b.write && p.has_fragment;

        if (force || writes != blend.write) {
            const ogl::native::Boolean w = writes ? 1 : 0;
            fn().glColorMask(w, w, w, w);
        }

        blend = b;
        blend.write = writes;
        const DepthState& d = p.depth;
        if (force || d.test != depth.test)       set_cap(gl::DEPTH_TEST, d.test);
        if (force || d.write != depth.write)     fn().glDepthMask(d.write ? 1 : 0);
        if (force || d.compare != depth.compare) fn().glDepthFunc(compare_op(d.compare));
        if (force || d.clamp != depth.clamp)     set_cap(gl::DEPTH_CLAMP, d.clamp);

        if (force || d.bias != depth.bias || d.bias_constant != depth.bias_constant || d.bias_slope != depth.bias_slope) {
            set_cap(gl::POLYGON_OFFSET_FILL, d.bias);
            if (d.bias) fn().glPolygonOffset(d.bias_slope, d.bias_constant);
        }

        depth = d;

        if (force || p.cull != cull) {
            set_cap(gl::CULL_FACE, p.cull != CullMode::None);
            if (p.cull != CullMode::None) fn().glCullFace(p.cull == CullMode::Front ? gl::FRONT : gl::BACK);
            cull = p.cull;
        }

        if (force || p.front != front) {
            fn().glFrontFace(p.front == FrontFace::CounterClockwise ? gl::CW : gl::CCW);
            front = p.front;
        }

        state_known = true;
    }

    void enable_attributes(std::uint32_t wanted) noexcept {
        for (std::uint32_t i = 0; i < kMaxAttribs; ++i) {
            const std::uint32_t bit = 1u << i;
            if ((wanted & bit) && !(attrib_mask & bit)) fn().glEnableVertexAttribArray(i);
            if (!(wanted & bit) && (attrib_mask & bit)) fn().glDisableVertexAttribArray(i);
        }

        attrib_mask = wanted;
    }

    void bind_unit(std::uint32_t unit, Enum target, Uint texture, Uint sampler) noexcept {
        if (unit < kMaxUnits) {
            UnitState& u = units[unit];
            if (u.target == target && u.texture == texture && u.sampler == sampler) return;
            u = { target, texture, sampler };
        }

        fn().glActiveTexture(gl::TEXTURE0 + unit);
        fn().glBindTexture(target, texture);
        fn().glBindSampler(unit, sampler);
    }

    void bind_uniform(std::uint32_t point, Uint id, std::uint64_t offset, std::uint64_t size) noexcept {
        if (point < kMaxRanges) {
            RangeState& r = ubo_ranges[point];
            if (r.id == id && r.offset == offset && r.size == size) return;
            r = { id, offset, size };
        }

        fn().glBindBufferRange(gl::UNIFORM_BUFFER, point, id, static_cast<ogl::native::Intptr>(offset), static_cast<ogl::native::Sizeiptr>(size));
    }

    std::shared_ptr<Program> program(const Shader* vs, const Shader* fs, const Shader* cs, const PipelineLayout& layout);

    void label(Enum type, Uint id, const char* name) const noexcept {
        if (name && id && caps.debug_labels) fn().glObjectLabel(type, id, -1, name);
    }

    Result take_error() const noexcept {
        const Enum e = fn().glGetError();
        while (fn().glGetError() != 0) {}
        if (e == gl::OUT_OF_MEMORY) return Result::OutOfMemory;
        return e == 0 ? Result::Success : Result::BackendFailed;
    }
};

inline Program::~Program() {
    if (id && core && core->current()) {
        if (core->bound_program == id) core->bound_program = 0;
        core->fn().glDeleteProgram(id);
    }
}

inline Buffer::~Buffer() {
    if (!id || !core || !core->current()) return;
    core->fn().glDeleteBuffers(1, &id);
    core->tracked -= std::min(core->tracked, desc.size);
    core->forget_bindings();
}

inline Enum buffer_usage(const BufferDesc& d) noexcept {
    if (d.memory == MemoryAccess::Readback) return gl::STREAM_READ;
    if (d.memory == MemoryAccess::Upload)   return gl::STREAM_DRAW;
    return gl::STATIC_DRAW;
}

inline Result Buffer::write(const void* data, std::uint64_t bytes, std::uint64_t offset) {
    if (!core->current()) return Result::NotInitialized;
    core->fn().glBindBuffer(gl::COPY_WRITE_BUFFER, id);
    core->fn().glBufferSubData(gl::COPY_WRITE_BUFFER, static_cast<ogl::native::Intptr>(offset), static_cast<ogl::native::Sizeiptr>(bytes), data);
    return Result::Success;
}

inline Result Buffer::read(void* out, std::uint64_t bytes, std::uint64_t offset) {
    if (!core->current()) return Result::NotInitialized;
    core->fn().glBindBuffer(gl::COPY_READ_BUFFER, id);
    core->fn().glGetBufferSubData(gl::COPY_READ_BUFFER, static_cast<ogl::native::Intptr>(offset), static_cast<ogl::native::Sizeiptr>(bytes), out);
    return Result::Success;
}

inline void Buffer::discard() {
    if (!core->current()) return;
    core->fn().glBindBuffer(gl::COPY_WRITE_BUFFER, id);
    core->fn().glBufferData(gl::COPY_WRITE_BUFFER, static_cast<ogl::native::Sizeiptr>(desc.size), nullptr, buffer_usage(desc));
}

inline Texture::~Texture() {
    if (!id || !core || !core->current()) return;
    core->forget_texture(id);
    core->fn().glDeleteTextures(1, &id);
    core->tracked -= std::min(core->tracked, bytes);
}

inline Sampler::~Sampler() {
    if (!id || !core || !core->current()) return;
    core->fn().glDeleteSamplers(1, &id);
    core->forget_bindings();
}

inline Shader::~Shader() {
    if (id && core && core->current()) core->fn().glDeleteShader(id);
}

inline QuerySet::~QuerySet() {
    if (!ids.empty() && core && core->current()) core->fn().glDeleteQueries(static_cast<ogl::native::Sizei>(ids.size()), ids.data());
}

inline std::string info_log(const ogl::Functions& fn, Uint id, bool program) {
    Int len = 0;
    if (program) fn.glGetProgramiv(id, gl::INFO_LOG_LENGTH, &len);
    else         fn.glGetShaderiv(id, gl::INFO_LOG_LENGTH, &len);
    std::string log(static_cast<std::size_t>(std::max(len, 1)), '\0');
    if (program) fn.glGetProgramInfoLog(id, static_cast<ogl::native::Sizei>(log.size()), nullptr, &log[0]);
    else         fn.glGetShaderInfoLog(id, static_cast<ogl::native::Sizei>(log.size()), nullptr, &log[0]);
    return std::string(log.c_str());
}

inline std::shared_ptr<Program> Core::program(const Shader* vs, const Shader* fs, const Shader* cs, const PipelineLayout& layout) {
    const auto key = std::make_tuple(cs ? cs->id : vs->id, fs ? fs->id : 0, layout.id);
    auto found = programs.find(key);

    if (found != programs.end()) {
        if (auto p = found->second.lock()) return p;
        programs.erase(found);
    }

    const ogl::Functions& f = fn();
    auto p = std::make_shared<Program>();
    p->core = shared_from_this();
    p->id = f.glCreateProgram();
    if (!p->id) return nullptr;
    const Shader* stages[2] = { cs ? cs : vs, cs ? nullptr : fs };
    for (const Shader* s : stages) if (s) f.glAttachShader(p->id, s->id);
    f.glLinkProgram(p->id);
    for (const Shader* s : stages) if (s) f.glDetachShader(p->id, s->id);
    Int linked = 0;
    f.glGetProgramiv(p->id, gl::LINK_STATUS, &linked);

    if (!linked) {
        error = "link failed (" + std::string(stages[0]->name) + (stages[1] ? " + " + stages[1]->name : std::string()) + "): " + info_log(f, p->id, true);
        return nullptr;
    }

    f.glUseProgram(p->id);
    bound_program = p->id;

    for (const Shader* s : stages) {
        if (!s) continue;

        for (const BindingName& b : s->bindings) {
            switch (b.kind) {
                case GlslResource::Texture: {
                    const Slot* slot = PipelineLayout::find(layout.textures, b.set, b.binding);
                    if (!slot || slot->capacity == 0) break;

                    for (std::uint32_t i = 0; i < b.count; ++i) {
                        const std::string name = b.count > 1 ? b.name + "[" + std::to_string(i) + "]" : b.name;
                        const Int loc = f.glGetUniformLocation(p->id, name.c_str());
                        if (loc >= 0) f.glUniform1i(loc, static_cast<Int>(slot->base + std::min(i, slot->capacity - 1)));
                    }

                    break;
                }

                case GlslResource::StorageImage: {
                    const Slot* slot = PipelineLayout::find(layout.images, b.set, b.binding);
                    const Int loc = f.glGetUniformLocation(p->id, b.name.c_str());
                    if (slot && loc >= 0) f.glUniform1i(loc, static_cast<Int>(slot->base));
                    break;
                }

                case GlslResource::UniformBlock: {
                    const Slot* slot = PipelineLayout::find(layout.uniforms, b.set, b.binding);
                    const Uint index = f.glGetUniformBlockIndex(p->id, b.name.c_str());
                    if (slot && index != gl::INVALID_INDEX) f.glUniformBlockBinding(p->id, index, slot->base);
                    break;
                }

                case GlslResource::PushBlock: {
                    const Uint index = f.glGetUniformBlockIndex(p->id, b.name.c_str());
                    if (index != gl::INVALID_INDEX) f.glUniformBlockBinding(p->id, index, kPushPoint);
                    break;
                }

                case GlslResource::StorageBlock: {
                    const Slot* slot = PipelineLayout::find(layout.storage, b.set, b.binding);
                    if (!slot || !f.glGetProgramResourceIndex || !f.glShaderStorageBlockBinding) break;
                    const Uint index = f.glGetProgramResourceIndex(p->id, gl::SHADER_STORAGE_BLOCK, b.name.c_str());
                    if (index != gl::INVALID_INDEX) f.glShaderStorageBlockBinding(p->id, index, slot->base);
                    break;
                }
            }
        }
    }

    f.glGetError();
    programs[key] = p;
    return p;
}

class Device final : public BackendDevice {
private:
    static constexpr std::uint32_t kMaxGroups   = 4;
    static constexpr std::uint32_t kMaxSlots    = 8;
    static constexpr std::uint32_t kPushShadow  = 256;

    struct VertexSlot {
        Uint          id     = 0;
        std::uint64_t offset = 0;
    };

    std::shared_ptr<Core>      m_core;
    std::uint32_t              m_frames   = 2;
    std::uint64_t              m_serial   = 0;
    bool                       m_in_frame = false;
    Extent2D                   m_window;
    std::vector<std::uint8_t>  m_push_blob;
    std::vector<std::uint32_t> m_push_at;

    const ogl::Functions& fn() const noexcept { return m_core->fn(); }

    static Texture* gl_texture(TextureImpl* t) noexcept { return static_cast<Texture*>(t); }
    static Buffer*  gl_buffer(BufferImpl* b)   noexcept { return static_cast<Buffer*>(b); }

    Result fail(Result r, const std::string& message) {
        m_core->error = message;
        return r;
    }

public:
    Result create(const DeviceInput& in) {
        m_core = std::make_shared<Core>();
        ogl::ContextDesc cd;
        cd.debug = in.debug;
        cd.vsync = in.vsync;
        const ogl::Result r = m_core->ctx.create(in.window, cd);
        if (ogl::failed(r)) return fail(Result::BackendFailed, std::string("opengl context: ") + std::string(ogl::to_string(r)));
        const ogl::Caps& c = m_core->ctx.caps();
        const ogl::Functions& f = fn();
        Caps& caps = m_core->caps;
        caps.backend             = Backend::OpenGL;
        caps.device_name         = c.renderer;
        caps.api_version         = c.version_string;
        caps.vendor_id           = vendor_id_from_name(c.vendor);
        if (caps.vendor_id == 0) caps.vendor_id = vendor_id_from_name(c.renderer);
        if (c.renderer.find("llvmpipe") != std::string::npos || c.renderer.find("softpipe") != std::string::npos) caps.adapter_type = AdapterType::Cpu;
        Int color_samples = c.max_samples, depth_samples = c.max_samples;
        f.glGetIntegerv(gl::MAX_COLOR_TEXTURE_SAMPLES, &color_samples);
        f.glGetIntegerv(gl::MAX_DEPTH_TEXTURE_SAMPLES, &depth_samples);
        f.glGetError();
        caps.max_samples         = static_cast<std::uint32_t>(std::max(1, std::min({ c.max_samples, color_samples, depth_samples })));
        caps.max_texture_size    = static_cast<std::uint32_t>(c.max_texture_size);
        caps.max_textures        = static_cast<std::uint32_t>(std::min<int>(c.texture_units, static_cast<int>(Core::kMaxUnits)));
        caps.uniform_alignment   = c.uniform_alignment;
        caps.max_uniform_range   = c.max_uniform_block;
        caps.max_push_constants  = 128;
        caps.compute             = c.compute;
        caps.storage_buffers     = c.compute && c.at_least(4, 3) && f.glShaderStorageBlockBinding && f.glGetProgramResourceIndex;
        caps.multi_draw_indirect = c.multi_draw;
        caps.base_instance       = c.base_instance;
        caps.depth_clamp         = true;
        caps.timestamps          = c.timer_query;
        caps.debug_labels        = c.debug;
        m_core->depth_fixup      = !c.clip_control;
        m_core->copy_image       = f.glCopyImageSubData && (c.at_least(4, 3) || m_core->ctx.state()->has_extension("GL_ARB_copy_image"));
        if (c.clip_control) f.glClipControl(gl::LOWER_LEFT, gl::ZERO_TO_ONE);
        if (c.seamless_cube) f.glEnable(gl::TEXTURE_CUBE_MAP_SEAMLESS);
        f.glGenVertexArrays(1, &m_core->vao);
        f.glBindVertexArray(m_core->vao);
        f.glGenBuffers(1, &m_core->push_buffer);
        f.glBindBuffer(gl::UNIFORM_BUFFER, m_core->push_buffer);
        f.glBufferData(gl::UNIFORM_BUFFER, static_cast<ogl::native::Sizeiptr>(Core::kPushCapacity), nullptr, gl::STREAM_DRAW);
        m_core->push_size = Core::kPushCapacity;
        f.glPixelStorei(gl::UNPACK_ALIGNMENT, 1);
        f.glPixelStorei(gl::PACK_ALIGNMENT, 1);
        if (failed(m_core->take_error()) || !m_core->vao || !m_core->push_buffer) return fail(Result::BackendFailed, "opengl setup failed");
        m_core->invalidate();
        m_frames = std::max(in.frames_in_flight, 1u);
        m_window = in.size;
        return Result::Success;
    }

    ~Device() override {
        if (m_core && m_core->current()) fn().glFinish();
    }

    Backend            backend()          const noexcept override { return Backend::OpenGL; }
    const Caps&        caps()             const noexcept override { return m_core->caps; }
    std::uint32_t      frames_in_flight() const noexcept override { return m_frames; }
    const std::string& last_error()       const noexcept override { return m_core->error; }
    bool               in_frame()         const noexcept override { return m_in_frame; }

    MemoryStats memory_stats() const noexcept override {
        MemoryStats m;
        m.valid = m_core && m_core->ctx.valid();
        if (!m.valid) return m;
        m.used = m_core->tracked;
        if (!m_core->current()) return m;
        const ogl::Caps& c = m_core->ctx.caps();

        if (c.memory_nvx) {
            Int total = 0, available = 0;
            fn().glGetIntegerv(gl::GPU_MEMORY_INFO_TOTAL_AVAILABLE_NVX, &total);
            fn().glGetIntegerv(gl::GPU_MEMORY_INFO_CURRENT_AVAILABLE_NVX, &available);
            m.measured = true;
            m.total    = static_cast<std::uint64_t>(total) * 1024;
            m.budget   = m.total;
            m.used     = static_cast<std::uint64_t>(std::max(total - available, 0)) * 1024;
        } else if (c.memory_ati) {
            Int info[4] = {};
            fn().glGetIntegerv(gl::TEXTURE_FREE_MEMORY_ATI, info);
            if (info[0] > 0) m.budget = m.used + static_cast<std::uint64_t>(info[0]) * 1024;
        }

        fn().glGetError();
        return m;
    }

    bool supports(Format format, TextureUsage usage, std::uint32_t samples) const noexcept override {
        if (pixel_format(format).internal == 0) return false;
        if (samples > m_core->caps.max_samples) return false;
        if (any(usage, TextureUsage::Storage)) {
            if (!m_core->caps.compute) return false;
            if (format != Format::R32Float && format != Format::RGBA32Float && format != Format::RGBA16Float && format != Format::R32Uint && format != Format::RGBA8Unorm) return false;
        }
        if (any(usage, TextureUsage::DepthStencil) && !is_depth(format)) return false;
        if (any(usage, TextureUsage::RenderTarget) && (is_depth(format) || format == Format::RGB32Float || format == Format::RGB32Sint)) return false;
        return true;
    }

    bool supports_linear_filter(Format format) const noexcept override {
        return format != Format::R32Uint && format != Format::RGB32Sint && pixel_format(format).internal != 0;
    }

    Result create_buffer(const BufferDesc& desc, std::shared_ptr<BufferImpl>& out) override {
        if (!m_core->current()) return Result::NotInitialized;
        auto b = std::make_shared<Buffer>();
        b->core = m_core;
        b->desc = desc;
        fn().glGenBuffers(1, &b->id);
        if (!b->id) return Result::OutOfMemory;
        fn().glBindBuffer(gl::COPY_WRITE_BUFFER, b->id);
        fn().glBufferData(gl::COPY_WRITE_BUFFER, static_cast<ogl::native::Sizeiptr>(desc.size), nullptr, buffer_usage(desc));
        m_core->label(gl::BUFFER_OBJECT, b->id, desc.name);
        const Result r = m_core->take_error();
        if (failed(r)) return r;
        m_core->tracked += desc.size;
        out = std::move(b);
        return Result::Success;
    }

    Result create_texture(const TextureDesc& desc, std::shared_ptr<TextureImpl>& out) override {
        if (!m_core->current()) return Result::NotInitialized;
        const PixelFormat pf = pixel_format(desc.format);
        if (!pf.internal || !supports(desc.format, desc.usage, desc.samples)) return Result::Unsupported;
        auto t = std::make_shared<Texture>();
        t->core   = m_core;
        t->desc   = desc;
        t->pixel  = pf;
        t->target = desc.samples > 1 ? gl::TEXTURE_2D_MULTISAMPLE
                  : desc.dimension == TextureDimension::Cube ? gl::TEXTURE_CUBE_MAP
                  : desc.dimension == TextureDimension::D3 ? gl::TEXTURE_3D : gl::TEXTURE_2D;
        fn().glGenTextures(1, &t->id);
        if (!t->id) return Result::OutOfMemory;
        m_core->scratch_bind(*t);
        const ogl::native::Sizei w = static_cast<ogl::native::Sizei>(desc.extent.width), h = static_cast<ogl::native::Sizei>(desc.extent.height);

        if (desc.samples > 1) {
            fn().glTexImage2DMultisample(gl::TEXTURE_2D_MULTISAMPLE, static_cast<ogl::native::Sizei>(desc.samples), pf.internal, w, h, 1);
        } else {
            for (std::uint32_t m = 0; m < desc.mip_levels; ++m) {
                const Extent3D e = mip_extent(desc, m);
                const ogl::native::Sizei mw = static_cast<ogl::native::Sizei>(e.width), mh = static_cast<ogl::native::Sizei>(e.height);

                if (desc.dimension == TextureDimension::Cube) {
                    for (Enum face = 0; face < 6; ++face) fn().glTexImage2D(gl::TEXTURE_CUBE_MAP_POSITIVE_X + face, static_cast<Int>(m), static_cast<Int>(pf.internal), mw, mh, 0, pf.format, pf.type, nullptr);
                } else if (desc.dimension == TextureDimension::D3) {
                    fn().glTexImage3D(gl::TEXTURE_3D, static_cast<Int>(m), static_cast<Int>(pf.internal), mw, mh, static_cast<ogl::native::Sizei>(e.depth), 0, pf.format, pf.type, nullptr);
                } else {
                    fn().glTexImage2D(gl::TEXTURE_2D, static_cast<Int>(m), static_cast<Int>(pf.internal), mw, mh, 0, pf.format, pf.type, nullptr);
                }
            }

            fn().glTexParameteri(t->target, gl::TEXTURE_BASE_LEVEL, 0);
            fn().glTexParameteri(t->target, gl::TEXTURE_MAX_LEVEL, static_cast<Int>(desc.mip_levels - 1));
            fn().glTexParameteri(t->target, gl::TEXTURE_MIN_FILTER, static_cast<Int>(gl::NEAREST));
            fn().glTexParameteri(t->target, gl::TEXTURE_MAG_FILTER, static_cast<Int>(gl::NEAREST));
        }

        m_core->label(gl::TEXTURE_OBJECT, t->id, desc.name);
        const Result r = m_core->take_error();
        if (failed(r)) return fail(r, "texture allocation failed");
        std::uint64_t bytes = 0;
        for (std::uint32_t m = 0; m < desc.mip_levels; ++m) { const Extent3D e = mip_extent(desc, m); bytes += static_cast<std::uint64_t>(e.width) * e.height * e.depth * texel_size(desc.format); }
        t->bytes = bytes * t->layers() * desc.samples;
        m_core->tracked += t->bytes;
        out = std::move(t);
        return Result::Success;
    }

    Result create_sampler(const SamplerDesc& desc, std::shared_ptr<SamplerImpl>& out) override {
        if (!m_core->current()) return Result::NotInitialized;
        auto s = std::make_shared<Sampler>();
        s->core = m_core;
        s->desc = desc;
        fn().glGenSamplers(1, &s->id);
        if (!s->id) return Result::OutOfMemory;
        const bool mips = desc.max_lod > 0.0f;
        const bool lin = desc.min_filter == Filter::Linear, mip_lin = desc.mip_filter == Filter::Linear;
        const Enum min = !mips ? (lin ? gl::LINEAR : gl::NEAREST)
                       : lin ? (mip_lin ? gl::LINEAR_MIPMAP_LINEAR : gl::LINEAR_MIPMAP_NEAREST)
                             : (mip_lin ? gl::NEAREST_MIPMAP_LINEAR : gl::NEAREST_MIPMAP_NEAREST);
        fn().glSamplerParameteri(s->id, gl::TEXTURE_MIN_FILTER, static_cast<Int>(min));
        fn().glSamplerParameteri(s->id, gl::TEXTURE_MAG_FILTER, static_cast<Int>(desc.mag_filter == Filter::Linear ? gl::LINEAR : gl::NEAREST));
        fn().glSamplerParameteri(s->id, gl::TEXTURE_WRAP_S, static_cast<Int>(address_mode(desc.address_u)));
        fn().glSamplerParameteri(s->id, gl::TEXTURE_WRAP_T, static_cast<Int>(address_mode(desc.address_v)));
        fn().glSamplerParameteri(s->id, gl::TEXTURE_WRAP_R, static_cast<Int>(address_mode(desc.address_w)));
        fn().glSamplerParameterf(s->id, gl::TEXTURE_MAX_LOD, desc.max_lod);

        if (desc.compare) {
            fn().glSamplerParameteri(s->id, gl::TEXTURE_COMPARE_MODE, static_cast<Int>(gl::COMPARE_REF_TO_TEXTURE));
            fn().glSamplerParameteri(s->id, gl::TEXTURE_COMPARE_FUNC, static_cast<Int>(compare_op(desc.compare_op)));
        }

        const ogl::ContextState& cs = *m_core->ctx.state();
        if (desc.max_anisotropy > 1.0f && (m_core->ctx.caps().at_least(4, 6) || cs.has_extension("GL_EXT_texture_filter_anisotropic") || cs.has_extension("GL_ARB_texture_filter_anisotropic")))
            fn().glSamplerParameterf(s->id, gl::TEXTURE_MAX_ANISOTROPY, desc.max_anisotropy);

        m_core->label(gl::SAMPLER_OBJECT, s->id, desc.name);
        const Result r = m_core->take_error();
        if (failed(r)) return r;
        out = std::move(s);
        return Result::Success;
    }

    Result create_shader(const ShaderDesc& desc, std::shared_ptr<ShaderImpl>& out) override {
        if (!m_core->current()) return Result::NotInitialized;
        if (!desc.glsl || desc.glsl_pieces == 0) return fail(Result::Unsupported, std::string(desc.name ? desc.name : "shader") + ": no GLSL source for the OpenGL backend");
        if (desc.stage == ShaderStage::Compute && !m_core->caps.compute) return fail(Result::Unsupported, "compute shaders need OpenGL 4.3");
        const int need_major = desc.glsl_version >= 330 ? desc.glsl_version / 100 : 3, need_minor = desc.glsl_version >= 330 ? (desc.glsl_version % 100) / 10 : 3;
        if (!m_core->ctx.caps().at_least(need_major, need_minor)) return fail(Result::Unsupported, std::string(desc.name ? desc.name : "shader") + ": GLSL " + std::to_string(desc.glsl_version) + " needs a newer OpenGL context");
        const Enum stage = desc.stage == ShaderStage::Vertex ? gl::VERTEX_SHADER : desc.stage == ShaderStage::Fragment ? gl::FRAGMENT_SHADER : gl::COMPUTE_SHADER;
        auto s = std::make_shared<Shader>();
        s->core  = m_core;
        s->stage = desc.stage;
        s->name  = desc.name ? desc.name : "shader";
        s->id    = fn().glCreateShader(stage);
        if (!s->id) return Result::OutOfMemory;
        std::string header = "#version " + std::to_string(desc.glsl_version) + (desc.glsl_version >= 150 ? " core\n" : "\n");
        if (desc.stage == ShaderStage::Vertex && m_core->depth_fixup) header += "#define FIZMO_GL_DEPTH_FIXUP\n";
        std::vector<const ogl::native::Char*> strings;
        strings.reserve(desc.glsl_pieces + 1);
        strings.push_back(header.c_str());
        for (std::size_t i = 0; i < desc.glsl_pieces; ++i) strings.push_back(desc.glsl[i]);
        fn().glShaderSource(s->id, static_cast<ogl::native::Sizei>(strings.size()), strings.data(), nullptr);
        fn().glCompileShader(s->id);
        Int ok = 0;
        fn().glGetShaderiv(s->id, gl::COMPILE_STATUS, &ok);
        if (!ok) return fail(Result::ShaderFailed, s->name + ": " + info_log(fn(), s->id, false));
        for (const GlslBinding& b : desc.glsl_bindings) if (b.name) s->bindings.push_back({ b.name, b.kind, b.set, b.binding, std::max(b.count, 1u) });
        m_core->label(gl::SHADER_OBJECT, s->id, desc.name);
        fn().glGetError();
        out = std::move(s);
        return Result::Success;
    }

    Result create_bind_group_layout(const BindGroupLayoutDesc& desc, std::shared_ptr<BindGroupLayoutImpl>& out) override {
        auto l = std::make_shared<BindGroupLayout>();

        for (const BindGroupLayoutEntry& e : desc.entries) {
            if (e.count == 0 || l->find(e.binding)) return Result::InvalidArgument;
            if (e.type == BindingType::StorageTexture && !m_core->caps.compute) return Result::Unsupported;
            if (e.type == BindingType::StorageBuffer && !m_core->caps.storage_buffers) return Result::Unsupported;
            l->entries.push_back(e);
        }

        std::sort(l->entries.begin(), l->entries.end(), [](const BindGroupLayoutEntry& a, const BindGroupLayoutEntry& b) { return a.binding < b.binding; });
        out = std::move(l);
        return Result::Success;
    }

    Result create_pipeline_layout(std::vector<std::shared_ptr<BindGroupLayoutImpl>> groups, std::uint32_t push_size, ShaderStage push_stages, const char*, std::shared_ptr<PipelineLayoutImpl>& out) override {
        auto l = std::make_shared<PipelineLayout>();
        l->groups      = std::move(groups);
        l->push_size   = push_size;
        l->push_stages = push_stages;
        struct Item { std::uint32_t group; const BindGroupLayoutEntry* entry; std::uint32_t capacity; };
        std::vector<Item> textures;
        std::uint32_t images = 0, uniforms = Core::kPushPoint + 1, storage = 0;

        for (std::uint32_t g = 0; g < l->groups.size(); ++g) {
            for (const BindGroupLayoutEntry& e : l->groups[g]->entries) {
                switch (e.type) {
                    case BindingType::SampledTexture: textures.push_back({ g, &e, 1 }); break;
                    case BindingType::StorageTexture: l->images[PipelineLayout::key(g, e.binding)] = { images, 1 }; images += 1; break;
                    case BindingType::UniformBuffer:  l->uniforms[PipelineLayout::key(g, e.binding)] = { uniforms, e.count }; uniforms += e.count; break;
                    case BindingType::StorageBuffer:  l->storage[PipelineLayout::key(g, e.binding)] = { storage, e.count }; storage += e.count; break;
                }
            }
        }

        const std::uint32_t units = m_core->caps.max_textures;
        if (textures.size() > units) return fail(Result::Unsupported, "pipeline layout uses more textures than the OpenGL device has units");
        if (uniforms >= Core::kMaxRanges) return fail(Result::Unsupported, "pipeline layout uses too many uniform buffers");
        std::uint32_t spare = units - static_cast<std::uint32_t>(textures.size());

        for (Item& t : textures) {
            const std::uint32_t extra = std::min(t.entry->count - 1, spare);
            t.capacity += extra;
            spare -= extra;
        }

        std::uint32_t next = 0;

        for (const Item& t : textures) {
            l->textures[PipelineLayout::key(t.group, t.entry->binding)] = { next, t.capacity };
            next += t.capacity;
        }

        out = std::move(l);
        return Result::Success;
    }

    Result create_bind_group(const std::shared_ptr<BindGroupLayoutImpl>& layout, Span<BindingInput> entries, BindGroupLifetime lifetime, const char*, std::shared_ptr<BindGroupImpl>& out) override {
        auto g = std::make_shared<BindGroup>();
        g->layout   = layout;
        g->lifetime = lifetime;
        g->entries.reserve(entries.size());
        for (const BindingInput& e : entries) g->entries.push_back({ e.binding, e.element, e.buffer, e.offset, e.size, e.texture, e.sampler });
        out = std::move(g);
        return Result::Success;
    }

    Result create_render_pipeline(const RenderPipelineInput& d, std::shared_ptr<RenderPipelineImpl>& out) override {
        if (!m_core->current()) return Result::NotInitialized;
        const auto* layout = static_cast<const PipelineLayout*>(d.layout.get());
        auto p = std::make_shared<RenderPipeline>();
        p->layout       = d.layout;
        p->program      = m_core->program(static_cast<const Shader*>(d.vertex.get()), static_cast<const Shader*>(d.fragment.get()), nullptr, *layout);
        if (!p->program) return Result::ShaderFailed;
        p->bindings     = d.vertex_bindings;
        p->attributes   = d.vertex_attributes;
        p->mode         = topology(d.topology);
        p->cull         = d.cull_mode;
        p->front        = d.front_face;
        p->depth        = d.depth;
        p->blend        = d.blend;
        p->has_fragment = d.fragment != nullptr && !d.color_formats.empty();

        for (const VertexAttribute& a : d.vertex_attributes) {
            if (a.location >= Core::kMaxAttribs || attribute_source(a.format) == ogl::Format::Undefined || !p->binding(a.binding)) return Result::InvalidArgument;
            p->attribute_mask |= 1u << a.location;
        }

        out = std::move(p);
        return Result::Success;
    }

    Result create_compute_pipeline(const std::shared_ptr<ShaderImpl>& shader, const std::shared_ptr<PipelineLayoutImpl>& layout, const char*, std::shared_ptr<ComputePipelineImpl>& out) override {
        if (!m_core->current()) return Result::NotInitialized;
        auto p = std::make_shared<ComputePipeline>();
        p->layout  = layout;
        p->program = m_core->program(nullptr, nullptr, static_cast<const Shader*>(shader.get()), *static_cast<const PipelineLayout*>(layout.get()));
        if (!p->program) return Result::ShaderFailed;
        out = std::move(p);
        return Result::Success;
    }

    Result create_query_set(std::uint32_t count, const char*, std::shared_ptr<QuerySetImpl>& out) override {
        if (!m_core->current()) return Result::NotInitialized;
        auto q = std::make_shared<QuerySet>();
        q->core  = m_core;
        q->count = count;
        q->ids.assign(count, 0);
        fn().glGenQueries(static_cast<ogl::native::Sizei>(count), q->ids.data());
        const Result r = m_core->take_error();
        if (failed(r)) return r;
        out = std::move(q);
        return Result::Success;
    }

    Result begin_frame(FrameInfo& out) override {
        if (m_in_frame) return Result::InvalidArgument;
        if (!m_core->current()) return Result::DeviceLost;
        if (m_window.width == 0 || m_window.height == 0) return Result::NotReady;
        m_core->invalidate();
        m_in_frame   = true;
        out.index    = static_cast<std::uint32_t>(m_serial % m_frames);
        out.serial   = m_serial;
        out.extent   = m_window;
        return Result::Success;
    }

    Result end_frame() override {
        if (!m_in_frame) return Result::InvalidArgument;
        m_in_frame = false;
        m_core->ctx.swap_buffers();
        ++m_serial;
        return Result::Success;
    }

    Result present(TextureImpl& source, Filter filter) override {
        if (!m_in_frame) return Result::InvalidArgument;
        if (!m_core->current()) return Result::DeviceLost;
        Texture& src = *gl_texture(&source);
        if (src.desc.samples > 1 || is_depth(src.desc.format)) return Result::InvalidArgument;
        const Uint fbo = m_core->single(src, 0, 0);
        if (!fbo) return Result::BackendFailed;
        m_core->scissor(false);
        fn().glBindFramebuffer(gl::READ_FRAMEBUFFER, fbo);
        fn().glBindFramebuffer(gl::DRAW_FRAMEBUFFER, 0);
        const Int sw = static_cast<Int>(src.desc.extent.width), sh = static_cast<Int>(src.desc.extent.height);
        const Int dw = static_cast<Int>(m_window.width), dh = static_cast<Int>(m_window.height);
        const bool same = sw == dw && sh == dh;
        fn().glBlitFramebuffer(0, 0, sw, sh, 0, dh, dw, 0, gl::COLOR_BUFFER_BIT, filter == Filter::Linear && !same ? gl::LINEAR : gl::NEAREST);
        fn().glBindFramebuffer(gl::FRAMEBUFFER, 0);
        return Result::Success;
    }

    void resize(std::uint32_t width, std::uint32_t height) override { m_window = { width, height }; }

    Result set_vsync(bool enabled) override {
        return m_core->ctx.set_swap_interval(enabled ? 1 : 0) ? Result::Success : Result::Unsupported;
    }

    Result wait_idle() override {
        if (!m_core->current()) return Result::DeviceLost;
        fn().glFinish();
        return Result::Success;
    }

    Result write_texture(TextureImpl& texture, const TextureRegion& region, const void* data, std::uint32_t row_pixels) override {
        if (!m_core->current()) return Result::DeviceLost;
        Texture& t = *gl_texture(&texture);
        if (t.desc.samples > 1) return Result::InvalidArgument;
        upload(t, region, data, row_pixels);
        return m_core->take_error();
    }

    Result read_texture(TextureImpl& texture, const TextureRegion& region, void* out) override {
        if (!m_core->current()) return Result::DeviceLost;
        Texture& t = *gl_texture(&texture);
        if (t.desc.samples > 1) return Result::InvalidArgument;
        const Uint fbo = m_core->single(t, region.layer + static_cast<std::uint32_t>(region.origin.z), region.mip);
        if (!fbo) return Result::BackendFailed;
        fn().glBindFramebuffer(gl::READ_FRAMEBUFFER, fbo);
        fn().glReadPixels(region.origin.x, region.origin.y, static_cast<ogl::native::Sizei>(region.extent.width), static_cast<ogl::native::Sizei>(region.extent.height), t.pixel.format, t.pixel.type, out);
        fn().glBindFramebuffer(gl::FRAMEBUFFER, 0);
        return m_core->take_error();
    }

    bool read_timestamps(QuerySetImpl& set, std::uint32_t first, std::uint32_t count, std::uint64_t* out) override {
        if (!m_core->current()) return false;
        auto& q = static_cast<QuerySet&>(set);
        Int ready = 0;
        fn().glGetQueryObjectiv(q.ids[first + count - 1], gl::QUERY_RESULT_AVAILABLE, &ready);
        if (!ready) return false;
        for (std::uint32_t i = 0; i < count; ++i) fn().glGetQueryObjectui64v(q.ids[first + i], gl::QUERY_RESULT, &out[i]);
        return true;
    }

    Result submit(const CommandStream& s) override {
        if (!m_core->current()) return Result::DeviceLost;
        if (s.commands.empty()) return Result::Success;
        plan_push_constants(s);
        Replay r(*this, s);
        r.run();
        return m_core->take_error() == Result::OutOfMemory ? Result::OutOfMemory : Result::Success;
    }

private:
    void upload(Texture& t, const TextureRegion& region, const void* data, std::uint32_t row_pixels) {
        m_core->scratch_bind(t);
        fn().glPixelStorei(gl::UNPACK_ROW_LENGTH, static_cast<Int>(row_pixels));
        const ogl::native::Sizei w = static_cast<ogl::native::Sizei>(region.extent.width), h = static_cast<ogl::native::Sizei>(region.extent.height);

        if (t.desc.dimension == TextureDimension::D3) {
            fn().glTexSubImage3D(gl::TEXTURE_3D, static_cast<Int>(region.mip), region.origin.x, region.origin.y, region.origin.z, w, h, static_cast<ogl::native::Sizei>(region.extent.depth), t.pixel.format, t.pixel.type, data);
        } else {
            fn().glTexSubImage2D(m_core->face_target(t, region.layer), static_cast<Int>(region.mip), region.origin.x, region.origin.y, w, h, t.pixel.format, t.pixel.type, data);
        }

        fn().glPixelStorei(gl::UNPACK_ROW_LENGTH, 0);
    }

    static std::uint32_t push_block_size(const PipelineLayoutImpl* layout) noexcept {
        return layout ? static_cast<std::uint32_t>(align_up(layout->push_size, 16)) : 0;
    }

    void plan_push_constants(const CommandStream& s) {
        m_push_blob.clear();
        m_push_at.assign(s.commands.size(), UINT32_MAX);
        std::uint8_t shadow[kPushShadow] = {};
        const PipelineLayoutImpl* layout = nullptr;
        bool dirty = true;
        const std::uint64_t align = std::max<std::uint64_t>(m_core->caps.uniform_alignment, 16);

        for (std::size_t i = 0; i < s.commands.size(); ++i) {
            const Command& c = s.commands[i];

            switch (c.op) {
                case Op::BeginRenderPass:
                case Op::EndRenderPass:      layout = nullptr; dirty = true; break;
                case Op::SetRenderPipeline:  if (c.render.pipeline->layout.get() != layout) { layout = c.render.pipeline->layout.get(); dirty = true; } break;
                case Op::SetComputePipeline: if (c.compute.pipeline->layout.get() != layout) { layout = c.compute.pipeline->layout.get(); dirty = true; } break;

                case Op::PushConstants: {
                    const std::uint32_t off = std::min<std::uint32_t>(c.bytes.offset, kPushShadow);
                    std::memcpy(shadow + off, s.bytes.data() + c.bytes.at, std::min<std::uint32_t>(c.bytes.size, kPushShadow - off));
                    dirty = true;
                    break;
                }

                case Op::Draw:
                case Op::DrawIndexed:
                case Op::DrawIndexedIndirect:
                case Op::Dispatch: {
                    const std::uint32_t size = push_block_size(layout);
                    if (!dirty || size == 0) break;
                    const std::size_t at = static_cast<std::size_t>(align_up(m_push_blob.size(), align));
                    m_push_blob.resize(at + size);
                    std::memcpy(m_push_blob.data() + at, shadow, size);
                    m_push_at[i] = static_cast<std::uint32_t>(at);
                    dirty = false;
                    break;
                }

                default: break;
            }
        }

        if (m_push_blob.empty()) return;
        fn().glBindBuffer(gl::UNIFORM_BUFFER, m_core->push_buffer);

        if (m_push_blob.size() > m_core->push_size) {
            m_core->push_size = std::max<std::uint64_t>(m_push_blob.size(), m_core->push_size * 2);
        }

        fn().glBufferData(gl::UNIFORM_BUFFER, static_cast<ogl::native::Sizeiptr>(m_core->push_size), nullptr, gl::STREAM_DRAW);
        fn().glBufferSubData(gl::UNIFORM_BUFFER, 0, static_cast<ogl::native::Sizeiptr>(m_push_blob.size()), m_push_blob.data());
        m_core->ubo_ranges[Core::kPushPoint] = {};
    }

    class Replay {
    private:
        Device&                  d;
        Core&                    core;
        const CommandStream&     s;
        const ogl::Functions&    fn;
        const RenderPipeline*    pipeline        = nullptr;
        const ComputePipeline*   compute         = nullptr;
        const PipelineLayout*    layout          = nullptr;
        const PipelineLayout*    applied_layout  = nullptr;
        BindGroupImpl*           groups[kMaxGroups]  = {};
        BindGroupImpl*           applied[kMaxGroups] = {};
        VertexSlot               slots[kMaxSlots] = {};
        bool                     attributes_dirty = true;
        std::uint32_t            instance_shift   = 0;
        Uint                     index_id         = 0;
        std::uint64_t            index_offset     = 0;
        IndexType                index_type       = IndexType::UInt32;
        bool                     push_dirty       = true;
        std::uint32_t            push_block       = UINT32_MAX;
        const PassRecord*        pass             = nullptr;
        Uint                     pass_fbo         = 0;

    public:
        Replay(Device& device, const CommandStream& stream) : d(device), core(*device.m_core), s(stream), fn(device.fn()) {}

        void run() {
            fn.glBindVertexArray(core.vao);

            for (std::size_t i = 0; i < s.commands.size(); ++i) {
                const Command& c = s.commands[i];
                if (d.m_push_at[i] != UINT32_MAX) push_block = d.m_push_at[i];

                switch (c.op) {
                    case Op::BeginRenderPass:     begin_pass(s.passes[c.pass.index]); break;
                    case Op::EndRenderPass:       end_pass(); break;
                    case Op::SetRenderPipeline:   set_pipeline(static_cast<const RenderPipeline*>(c.render.pipeline)); break;
                    case Op::SetComputePipeline:  set_compute(static_cast<const ComputePipeline*>(c.compute.pipeline)); break;
                    case Op::SetBindGroup:        groups[c.bind.index] = c.bind.group; break;
                    case Op::PushConstants:       push_dirty = true; break;
                    case Op::SetVertexBuffer:     set_vertex(c); break;
                    case Op::SetIndexBuffer:      set_index(c); break;
                    case Op::SetViewport:         viewport(c); break;
                    case Op::SetScissor:          scissor(c.scissor.x, c.scissor.y, c.scissor.w, c.scissor.h); break;
                    case Op::Draw:                draw(c); break;
                    case Op::DrawIndexed:         draw_indexed(c); break;
                    case Op::DrawIndexedIndirect: draw_indirect(c); break;
                    case Op::Dispatch:            dispatch(c); break;
                    case Op::CopyBuffer:          copy_buffer(s.copies[c.copy.index]); break;
                    case Op::CopyBufferToTexture: copy_buffer_to_texture(s.copies[c.copy.index]); break;
                    case Op::CopyTextureToBuffer: copy_texture_to_buffer(s.copies[c.copy.index]); break;
                    case Op::CopyTexture:         copy_texture(s.copies[c.copy.index]); break;
                    case Op::BlitTexture:         blit(s.copies[c.copy.index]); break;
                    case Op::GenerateMipmaps:     mipmaps(*gl_texture(c.mips.texture)); break;
                    case Op::ResetQueries:        break;
                    case Op::WriteTimestamp:      fn.glQueryCounter(static_cast<QuerySet*>(c.query.set)->ids[c.query.first], gl::TIMESTAMP); break;
                    case Op::PushLabel:           if (core.caps.debug_labels) fn.glPushDebugGroup(gl::DEBUG_SOURCE_APPLICATION, 0, -1, s.text(c.bytes.at)); break;
                    case Op::PopLabel:            if (core.caps.debug_labels) fn.glPopDebugGroup(); break;
                }
            }

            if (pass) end_pass();
            fn.glBindFramebuffer(gl::FRAMEBUFFER, 0);
        }

    private:
        void reset_bindings() noexcept {
            pipeline = nullptr;
            compute = nullptr;
            layout = nullptr;
            applied_layout = nullptr;
            for (std::uint32_t i = 0; i < kMaxGroups; ++i) groups[i] = applied[i] = nullptr;
            for (VertexSlot& v : slots) v = {};
            index_id = 0;
            attributes_dirty = true;
            push_dirty = true;
        }

        void scissor(std::int32_t x, std::int32_t y, std::uint32_t w, std::uint32_t h) noexcept {
            core.scissor(true);
            fn.glScissor(x, y, static_cast<ogl::native::Sizei>(w), static_cast<ogl::native::Sizei>(h));
        }

        void viewport(const Command& c) noexcept {
            fn.glViewport(static_cast<Int>(std::floor(c.viewport.x)), static_cast<Int>(std::floor(c.viewport.y)),
                          static_cast<ogl::native::Sizei>(std::lround(c.viewport.w)), static_cast<ogl::native::Sizei>(std::lround(c.viewport.h)));
        }

        void begin_pass(const PassRecord& p) {
            if (pass) end_pass();
            const Texture* colors[PassRecord::kMaxColors] = {};
            std::uint32_t layers[PassRecord::kMaxColors] = {}, mips[PassRecord::kMaxColors] = {};

            for (std::uint32_t i = 0; i < p.color_count; ++i) {
                colors[i] = gl_texture(p.colors[i].texture);
                layers[i] = p.colors[i].layer;
                mips[i]   = p.colors[i].mip;
            }

            pass_fbo = core.framebuffer(colors, layers, mips, p.color_count, p.depth ? gl_texture(p.depth) : nullptr, p.depth_layer);
            pass = &p;
            reset_bindings();
            if (!pass_fbo) return;
            fn.glBindFramebuffer(gl::FRAMEBUFFER, pass_fbo);
            fn.glViewport(p.area.offset.x, p.area.offset.y, static_cast<ogl::native::Sizei>(p.area.extent.width), static_cast<ogl::native::Sizei>(p.area.extent.height));
            scissor(p.area.offset.x, p.area.offset.y, p.area.extent.width, p.area.extent.height);
            bool masked = false;

            for (std::uint32_t i = 0; i < p.color_count; ++i) {
                if (p.colors[i].load != LoadOp::Clear || !colors[i]) continue;
                if (!masked) { core.color_mask(true); masked = true; }
                const float v[4] = { p.colors[i].clear.r, p.colors[i].clear.g, p.colors[i].clear.b, p.colors[i].clear.a };
                fn.glClearBufferfv(gl::COLOR, static_cast<Int>(i), v);
            }

            if (p.depth && p.depth_load == LoadOp::Clear) {
                core.depth_mask(true);
                if (has_stencil(p.depth->desc.format)) fn.glClearBufferfi(gl::DEPTH_STENCIL, 0, p.clear_depth, static_cast<Int>(p.clear_stencil));
                else fn.glClearBufferfv(gl::DEPTH, 0, &p.clear_depth);
            }

            if (p.label != UINT32_MAX && core.caps.debug_labels) fn.glPushDebugGroup(gl::DEBUG_SOURCE_APPLICATION, 0, -1, s.text(p.label));
        }

        void end_pass() {
            const PassRecord& p = *pass;
            pass = nullptr;

            if (pass_fbo) {
                const Rect2D a = p.area;
                const Int x0 = a.offset.x, y0 = a.offset.y, x1 = x0 + static_cast<Int>(a.extent.width), y1 = y0 + static_cast<Int>(a.extent.height);
                bool scissor_off = false;

                for (std::uint32_t i = 0; i < p.color_count; ++i) {
                    if (!p.colors[i].resolve || !p.colors[i].texture) continue;
                    const Uint dst = core.single(*gl_texture(p.colors[i].resolve), 0, 0);
                    if (!dst) continue;
                    if (!scissor_off) { core.scissor(false); scissor_off = true; }
                    fn.glBindFramebuffer(gl::READ_FRAMEBUFFER, pass_fbo);
                    fn.glReadBuffer(gl::COLOR_ATTACHMENT0 + i);
                    fn.glBindFramebuffer(gl::DRAW_FRAMEBUFFER, dst);
                    fn.glBlitFramebuffer(x0, y0, x1, y1, x0, y0, x1, y1, gl::COLOR_BUFFER_BIT, gl::NEAREST);
                    fn.glBindFramebuffer(gl::READ_FRAMEBUFFER, pass_fbo);
                    fn.glReadBuffer(gl::COLOR_ATTACHMENT0);
                }

                if (p.depth && p.depth_resolve) {
                    const Uint dst = core.single(*gl_texture(p.depth_resolve), 0, 0);

                    if (dst) {
                        if (!scissor_off) { core.scissor(false); scissor_off = true; }
                        core.depth_mask(true);
                        fn.glBindFramebuffer(gl::READ_FRAMEBUFFER, pass_fbo);
                        fn.glBindFramebuffer(gl::DRAW_FRAMEBUFFER, dst);
                        fn.glBlitFramebuffer(x0, y0, x1, y1, x0, y0, x1, y1, gl::DEPTH_BUFFER_BIT, gl::NEAREST);
                    }
                }

                if (p.label != UINT32_MAX && core.caps.debug_labels) fn.glPopDebugGroup();
            }

            fn.glBindFramebuffer(gl::FRAMEBUFFER, 0);
            pass_fbo = 0;
            reset_bindings();
        }

        void set_pipeline(const RenderPipeline* p) {
            if (!pass || !pass_fbo) return;
            const auto* l = static_cast<const PipelineLayout*>(p->layout.get());
            if (l != layout) push_dirty = true;
            pipeline = p;
            compute  = nullptr;
            layout   = l;
            core.use_program(p->program->id);
            core.apply(*p);
            core.enable_attributes(p->attribute_mask);
            attributes_dirty = true;
        }

        void set_compute(const ComputePipeline* p) {
            if (pass) return;
            const auto* l = static_cast<const PipelineLayout*>(p->layout.get());
            if (l != layout) push_dirty = true;
            compute  = p;
            pipeline = nullptr;
            layout   = l;
            core.use_program(p->program->id);
        }

        void set_vertex(const Command& c) noexcept {
            if (c.vertex.slot >= kMaxSlots) return;
            slots[c.vertex.slot] = { gl_buffer(c.vertex.buffer)->id, c.vertex.offset };
            attributes_dirty = true;
        }

        void set_index(const Command& c) noexcept {
            index_id = gl_buffer(c.index.buffer)->id;
            index_offset = c.index.offset;
            index_type = c.index.type;
            fn.glBindBuffer(gl::ELEMENT_ARRAY_BUFFER, index_id);
        }

        void point_attributes(std::uint32_t shift) noexcept {
            Uint bound = 0;

            for (const VertexAttribute& a : pipeline->attributes) {
                const VertexBinding* b = pipeline->binding(a.binding);
                if (!b || a.binding >= kMaxSlots || !slots[a.binding].id) continue;
                const VertexSlot& v = slots[a.binding];
                if (v.id != bound) { fn.glBindBuffer(gl::ARRAY_BUFFER, v.id); bound = v.id; }
                const ogl::AttributeFormat f = ogl::attribute_format(attribute_source(a.format));
                const std::uint64_t extra = b->rate == VertexRate::PerInstance ? static_cast<std::uint64_t>(shift) * b->stride : 0;
                const void* at = reinterpret_cast<const void*>(static_cast<std::uintptr_t>(v.offset + a.offset + extra));
                if (f.integer) fn.glVertexAttribIPointer(a.location, f.components, f.type, static_cast<ogl::native::Sizei>(b->stride), at);
                else fn.glVertexAttribPointer(a.location, f.components, f.type, f.normalized ? 1 : 0, static_cast<ogl::native::Sizei>(b->stride), at);
                fn.glVertexAttribDivisor(a.location, b->rate == VertexRate::PerInstance ? 1u : 0u);
            }

            instance_shift = shift;
            attributes_dirty = false;
        }

        void apply_groups() {
            if (!layout) return;
            const bool relayout = applied_layout != layout;

            for (std::uint32_t g = 0; g < layout->groups.size() && g < kMaxGroups; ++g) {
                BindGroupImpl* group = groups[g];
                if (!group || (!relayout && applied[g] == group)) continue;
                applied[g] = group;

                for (const ResourceBinding& e : group->entries) {
                    const BindGroupLayoutEntry* le = group->layout->find(e.binding);
                    if (!le) continue;

                    switch (le->type) {
                        case BindingType::SampledTexture: {
                            const Slot* slot = PipelineLayout::find(layout->textures, g, e.binding);
                            if (!slot || e.element >= slot->capacity || !e.texture) break;
                            const Texture& t = *gl_texture(e.texture);
                            core.bind_unit(slot->base + e.element, t.target, t.id, e.sampler ? static_cast<Sampler*>(e.sampler)->id : 0);
                            break;
                        }

                        case BindingType::StorageTexture: {
                            const Slot* slot = PipelineLayout::find(layout->images, g, e.binding);
                            if (!slot || !e.texture) break;
                            const Texture& t = *gl_texture(e.texture);
                            const bool layered = t.desc.dimension != TextureDimension::D2;
                            fn.glBindImageTexture(slot->base, t.id, 0, layered ? 1 : 0, 0, storage_access(le->access), t.pixel.internal);
                            break;
                        }

                        case BindingType::UniformBuffer: {
                            const Slot* slot = PipelineLayout::find(layout->uniforms, g, e.binding);
                            if (!slot || !e.buffer) break;
                            core.bind_uniform(slot->base + e.element, gl_buffer(e.buffer)->id, e.offset, e.size);
                            break;
                        }

                        case BindingType::StorageBuffer: {
                            const Slot* slot = PipelineLayout::find(layout->storage, g, e.binding);
                            if (!slot || !e.buffer) break;
                            fn.glBindBufferRange(gl::SHADER_STORAGE_BUFFER, slot->base + e.element, gl_buffer(e.buffer)->id, static_cast<ogl::native::Intptr>(e.offset), static_cast<ogl::native::Sizeiptr>(e.size));
                            break;
                        }
                    }
                }
            }

            applied_layout = layout;
        }

        void apply_push() {
            if (!push_dirty) return;
            const std::uint32_t size = push_block_size(layout);
            if (size == 0 || push_block == UINT32_MAX) return;
            push_dirty = false;
            core.bind_uniform(Core::kPushPoint, core.push_buffer, push_block, size);
        }

        bool ready_to_draw(std::uint32_t first_instance) {
            if (!pipeline || !pass_fbo) return false;
            apply_groups();
            apply_push();
            const std::uint32_t shift = core.caps.base_instance ? 0u : first_instance;
            if (attributes_dirty || shift != instance_shift) point_attributes(shift);
            return true;
        }

        void draw(const Command& c) {
            if (!ready_to_draw(c.draw.first_instance)) return;
            const Int first = static_cast<Int>(c.draw.first);
            const ogl::native::Sizei count = static_cast<ogl::native::Sizei>(c.draw.count), inst = static_cast<ogl::native::Sizei>(c.draw.instances);
            if (c.draw.first_instance && core.caps.base_instance) fn.glDrawArraysInstancedBaseInstance(pipeline->mode, first, count, inst, c.draw.first_instance);
            else if (c.draw.instances == 1 && c.draw.first_instance == 0) fn.glDrawArrays(pipeline->mode, first, count);
            else fn.glDrawArraysInstanced(pipeline->mode, first, count, inst);
        }

        void draw_indexed(const Command& c) {
            if (!index_id || !ready_to_draw(c.indexed.first_instance)) return;
            const std::uint64_t size = index_type == IndexType::UInt16 ? 2u : 4u;
            const Enum type = index_type == IndexType::UInt16 ? gl::UNSIGNED_SHORT : gl::UNSIGNED_INT;
            const void* at = reinterpret_cast<const void*>(static_cast<std::uintptr_t>(index_offset + c.indexed.first_index * size));
            const ogl::native::Sizei count = static_cast<ogl::native::Sizei>(c.indexed.count), inst = static_cast<ogl::native::Sizei>(c.indexed.instances);
            if (c.indexed.first_instance && core.caps.base_instance) fn.glDrawElementsInstancedBaseVertexBaseInstance(pipeline->mode, count, type, at, inst, c.indexed.vertex_offset, c.indexed.first_instance);
            else if (c.indexed.instances == 1 && c.indexed.first_instance == 0) fn.glDrawElementsBaseVertex(pipeline->mode, count, type, at, c.indexed.vertex_offset);
            else fn.glDrawElementsInstancedBaseVertex(pipeline->mode, count, type, at, inst, c.indexed.vertex_offset);
        }

        void draw_indirect(const Command& c) {
            if (!core.caps.multi_draw_indirect || !index_id || !ready_to_draw(0)) return;
            fn.glBindBuffer(gl::DRAW_INDIRECT_BUFFER, gl_buffer(c.indirect.buffer)->id);
            fn.glMultiDrawElementsIndirect(pipeline->mode, index_type == IndexType::UInt16 ? gl::UNSIGNED_SHORT : gl::UNSIGNED_INT,
                                           reinterpret_cast<const void*>(static_cast<std::uintptr_t>(c.indirect.offset)),
                                           static_cast<ogl::native::Sizei>(c.indirect.count), static_cast<ogl::native::Sizei>(c.indirect.stride));
        }

        void dispatch(const Command& c) {
            if (!compute || pass || !core.caps.compute) return;
            apply_groups();
            apply_push();
            fn.glDispatchCompute(c.dispatch.x, c.dispatch.y, c.dispatch.z);
            fn.glMemoryBarrier(gl::ALL_BARRIER_BITS);
        }

        void copy_buffer(const CopyRecord& r) {
            fn.glBindBuffer(gl::COPY_READ_BUFFER, gl_buffer(r.src_buffer)->id);
            fn.glBindBuffer(gl::COPY_WRITE_BUFFER, gl_buffer(r.dst_buffer)->id);
            fn.glCopyBufferSubData(gl::COPY_READ_BUFFER, gl::COPY_WRITE_BUFFER, static_cast<ogl::native::Intptr>(r.src_offset), static_cast<ogl::native::Intptr>(r.dst_offset), static_cast<ogl::native::Sizeiptr>(r.size));
        }

        void copy_buffer_to_texture(const CopyRecord& r) {
            Texture& t = *gl_texture(r.dst_texture);
            if (t.desc.samples > 1) return;
            fn.glBindBuffer(gl::PIXEL_UNPACK_BUFFER, gl_buffer(r.src_buffer)->id);
            d.upload(t, r.dst_region, reinterpret_cast<const void*>(static_cast<std::uintptr_t>(r.src_offset)), r.row_pixels);
            fn.glBindBuffer(gl::PIXEL_UNPACK_BUFFER, 0);
            core.forget_bindings();
        }

        void copy_texture_to_buffer(const CopyRecord& r) {
            Texture& t = *gl_texture(r.src_texture);
            const Uint fbo = core.single(t, r.src_region.layer + static_cast<std::uint32_t>(r.src_region.origin.z), r.src_region.mip);
            if (!fbo) return;
            fn.glBindFramebuffer(gl::READ_FRAMEBUFFER, fbo);
            fn.glBindBuffer(gl::PIXEL_PACK_BUFFER, gl_buffer(r.dst_buffer)->id);
            fn.glReadPixels(r.src_region.origin.x, r.src_region.origin.y, static_cast<ogl::native::Sizei>(r.src_region.extent.width), static_cast<ogl::native::Sizei>(r.src_region.extent.height),
                            t.pixel.format, t.pixel.type, reinterpret_cast<void*>(static_cast<std::uintptr_t>(r.dst_offset)));
            fn.glBindBuffer(gl::PIXEL_PACK_BUFFER, 0);
            fn.glBindFramebuffer(gl::FRAMEBUFFER, 0);
        }

        void framebuffer_blit(Texture& src, const TextureRegion& sr, Texture& dst, const TextureRegion& dr, Enum filter) {
            const Uint read = core.single(src, sr.layer + static_cast<std::uint32_t>(sr.origin.z), sr.mip);
            const Uint draw = core.single(dst, dr.layer + static_cast<std::uint32_t>(dr.origin.z), dr.mip);
            if (!read || !draw) return;
            core.scissor(false);
            const bool depth = is_depth(src.desc.format);
            if (depth) core.depth_mask(true);
            fn.glBindFramebuffer(gl::READ_FRAMEBUFFER, read);
            fn.glBindFramebuffer(gl::DRAW_FRAMEBUFFER, draw);
            const Int sx0 = sr.origin.x, sy0 = sr.origin.y, sx1 = sx0 + static_cast<Int>(sr.extent.width), sy1 = sy0 + static_cast<Int>(sr.extent.height);
            const Int dx0 = dr.origin.x, dy0 = dr.origin.y, dx1 = dx0 + static_cast<Int>(dr.extent.width), dy1 = dy0 + static_cast<Int>(dr.extent.height);
            fn.glBlitFramebuffer(sx0, sy0, sx1, sy1, dx0, dy0, dx1, dy1, depth ? (has_stencil(src.desc.format) ? gl::DEPTH_BUFFER_BIT | gl::STENCIL_BUFFER_BIT : gl::DEPTH_BUFFER_BIT) : gl::COLOR_BUFFER_BIT, depth ? gl::NEAREST : filter);
            fn.glBindFramebuffer(gl::FRAMEBUFFER, 0);
        }

        void copy_texture(const CopyRecord& r) {
            Texture& src = *gl_texture(r.src_texture);
            Texture& dst = *gl_texture(r.dst_texture);
            const TextureRegion& sr = r.src_region;
            const TextureRegion& dr = r.dst_region;

            if (core.copy_image && src.desc.samples == dst.desc.samples) {
                const bool layered_src = src.desc.dimension != TextureDimension::D2, layered_dst = dst.desc.dimension != TextureDimension::D2;
                fn.glCopyImageSubData(src.id, src.target, static_cast<Int>(sr.mip), sr.origin.x, sr.origin.y, layered_src ? sr.origin.z + static_cast<Int>(sr.layer) : 0,
                                      dst.id, dst.target, static_cast<Int>(dr.mip), dr.origin.x, dr.origin.y, layered_dst ? dr.origin.z + static_cast<Int>(dr.layer) : 0,
                                      static_cast<ogl::native::Sizei>(sr.extent.width), static_cast<ogl::native::Sizei>(sr.extent.height), static_cast<ogl::native::Sizei>(std::max(sr.extent.depth, 1u)));
                return;
            }

            framebuffer_blit(src, sr, dst, dr, gl::NEAREST);
        }

        void blit(const CopyRecord& r) {
            framebuffer_blit(*gl_texture(r.src_texture), r.src_region, *gl_texture(r.dst_texture), r.dst_region, r.filter == Filter::Linear ? gl::LINEAR : gl::NEAREST);
        }

        void mipmaps(Texture& t) {
            core.scratch_bind(t);
            fn.glGenerateMipmap(t.target);
        }
    };
};

} // namespace glb
} // namespace detail
} // namespace gpu
} // namespace fizmo

#endif // OS_WINDOWS || OS_LINUX
#endif // FIZMO_GPU_OPENGL_BACKEND_HPP
