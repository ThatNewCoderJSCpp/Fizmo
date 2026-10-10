#ifndef FIZMO_OPENGL_PIPELINE_HPP
#define FIZMO_OPENGL_PIPELINE_HPP

#include "resources.hpp"
#include <initializer_list>
#include <string>
#include <utility>
#include <vector>

namespace fizmo {
namespace opengl {

struct ShaderSource {
    const char* const* pieces  = nullptr;
    std::size_t        count   = 0;
    int                version = 330;
    const char*        name    = nullptr;

    template <std::size_t N>
    static ShaderSource of(const char* const (&p)[N], int version, const char* name) noexcept { return { p, N, version, name }; }

    bool valid() const noexcept { return pieces && count > 0; }
};

struct UniformBlockBinding {
    const char*   name    = nullptr;
    std::uint32_t binding = 0;
};

struct SamplerBinding {
    const char* name = nullptr;
    int         unit = 0;
};

struct ProgramDesc {
    ShaderSource                     vertex;
    ShaderSource                     fragment;
    ShaderSource                     compute;
    std::vector<std::string>         defines;
    std::vector<UniformBlockBinding> blocks;
    std::vector<SamplerBinding>      samplers;
    const char*                      name = nullptr;
};

class Program {
private:
    std::shared_ptr<ContextState> m_ctx;
    native::Uint                  m_id = 0;
    std::string                   m_log;

    native::Uint compile(native::Enum stage, const ShaderSource& src, const std::vector<std::string>& defines) {
        const Functions& fn = m_ctx->fn;
        const native::Uint shader = fn.glCreateShader(stage);
        if (!shader) return 0;
        std::string header = "#version " + std::to_string(src.version) + (src.version >= 150 ? " core\n" : "\n");
        for (const std::string& d : defines) header += "#define " + d + "\n";
        std::vector<const native::Char*> strings;
        strings.reserve(src.count + 1);
        strings.push_back(header.c_str());
        for (std::size_t i = 0; i < src.count; ++i) strings.push_back(src.pieces[i]);
        fn.glShaderSource(shader, static_cast<native::Sizei>(strings.size()), strings.data(), nullptr);
        fn.glCompileShader(shader);
        native::Int ok = 0;
        fn.glGetShaderiv(shader, gl::COMPILE_STATUS, &ok);

        if (!ok) {
            native::Int len = 0;
            fn.glGetShaderiv(shader, gl::INFO_LOG_LENGTH, &len);
            std::string log(static_cast<std::size_t>(std::max(len, 1)), '\0');
            fn.glGetShaderInfoLog(shader, static_cast<native::Sizei>(log.size()), nullptr, &log[0]);
            m_log += std::string(src.name ? src.name : "shader") + ": " + log.c_str() + "\n";
            fn.glDeleteShader(shader);
            return 0;
        }

        return shader;
    }

public:
    Program() noexcept = default;
    ~Program() noexcept { destroy(); }

    Program(const Program&) = delete;
    Program& operator=(const Program&) = delete;
    Program(Program&& o) noexcept : m_ctx(std::move(o.m_ctx)), m_id(std::exchange(o.m_id, 0)), m_log(std::move(o.m_log)) {}

    Program& operator=(Program&& o) noexcept {
        if (this != &o) {
            destroy();
            m_ctx = std::move(o.m_ctx);
            m_id  = std::exchange(o.m_id, 0);
            m_log = std::move(o.m_log);
        }
        return *this;
    }

    Result create(const Context& ctx, const ProgramDesc& d) {
        destroy();
        if (!ctx.valid() || (!d.compute.valid() && !d.vertex.valid())) return Result::InvalidArgument;
        m_ctx = ctx.state();
        if (!detail::usable(m_ctx)) return Result::NotInitialized;
        const Functions& fn = m_ctx->fn;
        native::Uint shaders[2] = { 0, 0 };
        std::size_t count = 0;
        bool ok = true;

        if (d.compute.valid()) {
            shaders[count] = compile(gl::COMPUTE_SHADER, d.compute, d.defines);
            ok = shaders[count++] != 0;
        } else {
            shaders[count] = compile(gl::VERTEX_SHADER, d.vertex, d.defines);
            ok = shaders[count++] != 0;

            if (ok && d.fragment.valid()) {
                shaders[count] = compile(gl::FRAGMENT_SHADER, d.fragment, d.defines);
                ok = shaders[count++] != 0;
            }
        }

        if (!ok) {
            for (std::size_t i = 0; i < count; ++i) if (shaders[i]) fn.glDeleteShader(shaders[i]);
            m_ctx.reset();
            return Result::CompileFailed;
        }

        m_id = fn.glCreateProgram();
        for (std::size_t i = 0; i < count; ++i) fn.glAttachShader(m_id, shaders[i]);
        fn.glLinkProgram(m_id);
        for (std::size_t i = 0; i < count; ++i) { fn.glDetachShader(m_id, shaders[i]); fn.glDeleteShader(shaders[i]); }
        native::Int linked = 0;
        fn.glGetProgramiv(m_id, gl::LINK_STATUS, &linked);

        if (!linked) {
            native::Int len = 0;
            fn.glGetProgramiv(m_id, gl::INFO_LOG_LENGTH, &len);
            std::string log(static_cast<std::size_t>(std::max(len, 1)), '\0');
            fn.glGetProgramInfoLog(m_id, static_cast<native::Sizei>(log.size()), nullptr, &log[0]);
            m_log += std::string(d.name ? d.name : "program") + ": " + log.c_str() + "\n";
            fn.glDeleteProgram(m_id);
            m_id = 0;
            return Result::LinkFailed;
        }

        for (const UniformBlockBinding& b : d.blocks) {
            const native::Uint index = fn.glGetUniformBlockIndex(m_id, b.name);
            if (index != gl::INVALID_INDEX) fn.glUniformBlockBinding(m_id, index, b.binding);
        }

        fn.glUseProgram(m_id);

        for (const SamplerBinding& s : d.samplers) {
            const native::Int loc = fn.glGetUniformLocation(m_id, s.name);
            if (loc >= 0) fn.glUniform1i(loc, s.unit);
        }

        fn.glUseProgram(0);
        detail::label(*m_ctx, gl::PROGRAM_OBJECT, m_id, d.name);
        fn.glGetError();
        return Result::Success;
    }

    void destroy() noexcept {
        if (m_id && detail::usable(m_ctx)) m_ctx->fn.glDeleteProgram(m_id);
        m_id = 0;
        m_ctx.reset();
    }

    bool               valid() const noexcept { return m_id != 0; }
    native::Uint       id()    const noexcept { return m_id; }
    const std::string& log()   const noexcept { return m_log; }
};

struct VertexBinding {
    std::uint32_t binding = 0;
    std::uint32_t stride  = 0;
    VertexRate    rate    = VertexRate::PerVertex;
};

struct VertexAttribute {
    std::uint32_t location = 0;
    std::uint32_t binding  = 0;
    Format        format   = Format::Undefined;
    std::uint32_t offset   = 0;
};

struct BlendState {
    bool        enable    = false;
    BlendFactor src_color = BlendFactor::One;
    BlendFactor dst_color = BlendFactor::Zero;
    BlendFactor src_alpha = BlendFactor::One;
    BlendFactor dst_alpha = BlendFactor::Zero;

    static BlendState opaque() noexcept { return {}; }

    static BlendState premultiplied() noexcept {
        BlendState b;
        b.enable = true;
        b.src_color = BlendFactor::One; b.dst_color = BlendFactor::OneMinusSrcAlpha;
        b.src_alpha = BlendFactor::One; b.dst_alpha = BlendFactor::OneMinusSrcAlpha;
        return b;
    }

    bool operator==(const BlendState& o) const noexcept {
        return enable == o.enable && (!enable || (src_color == o.src_color && dst_color == o.dst_color && src_alpha == o.src_alpha && dst_alpha == o.dst_alpha));
    }
};

struct DepthState {
    bool      test          = false;
    bool      write         = false;
    CompareOp compare       = CompareOp::Less;
    bool      clamp         = false;
    bool      bias          = false;
    float     bias_constant = 0.0f;
    float     bias_slope    = 0.0f;
};

struct GraphicsPipelineDesc {
    const Program*               program   = nullptr;
    std::vector<VertexBinding>   vertex_bindings;
    std::vector<VertexAttribute> vertex_attributes;
    Topology                     topology   = Topology::Triangles;
    CullMode                     cull_mode  = CullMode::None;
    FrontFace                    front_face = FrontFace::CounterClockwise;
    DepthState                   depth;
    BlendState                   blend;
    bool                         color_write = true;
    const char*                  name        = nullptr;
};

class Pipeline {
private:
    GraphicsPipelineDesc m_desc;
    bool                 m_valid = false;

public:
    Pipeline() noexcept = default;

    Result create(const Context& ctx, const GraphicsPipelineDesc& d) {
        m_valid = false;
        if (!ctx.valid() || !d.program || !d.program->valid()) return Result::InvalidArgument;

        for (const VertexAttribute& a : d.vertex_attributes) {
            bool found = false;
            for (const VertexBinding& b : d.vertex_bindings) found = found || b.binding == a.binding;
            if (!found || attribute_format(a.format).components == 0) return Result::InvalidArgument;
        }

        m_desc  = d;
        m_valid = true;
        return Result::Success;
    }

    void destroy() noexcept { m_valid = false; m_desc = {}; }

    bool                        valid()   const noexcept { return m_valid; }
    const GraphicsPipelineDesc& desc()    const noexcept { return m_desc; }
    const Program&              program() const noexcept { return *m_desc.program; }

    const VertexBinding* binding(std::uint32_t index) const noexcept {
        for (const VertexBinding& b : m_desc.vertex_bindings) if (b.binding == index) return &b;
        return nullptr;
    }
};

} // namespace opengl
} // namespace fizmo

#endif // FIZMO_OPENGL_PIPELINE_HPP
