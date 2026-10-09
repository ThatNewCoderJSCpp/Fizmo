#ifndef FIZMO_OPENGL_COMMANDS_HPP
#define FIZMO_OPENGL_COMMANDS_HPP

#include "pipeline.hpp"
#include <array>
#include <cmath>
#include <cstring>

namespace fizmo {
namespace opengl {

class Commands {
private:
    static constexpr std::uint32_t kMaxBindings   = 4;
    static constexpr std::uint32_t kMaxAttributes = 16;
    static constexpr std::uint64_t kPushRingStart = 256 * 1024;

    struct BoundBuffer {
        native::Uint  id     = 0;
        std::uint64_t offset = 0;
    };

    std::shared_ptr<ContextState>            m_ctx;
    native::Uint                             m_vao            = 0;
    Buffer                                   m_push;
    std::uint64_t                            m_push_offset    = 0;
    const Pipeline*                          m_pipeline       = nullptr;
    std::array<BoundBuffer, kMaxBindings>    m_buffers{};
    IndexType                                m_index_type     = IndexType::UInt32;
    bool                                     m_instance_moved = false;
    std::uint32_t                            m_attrib_mask    = 0;
    bool                                     m_known          = false;
    native::Uint                             m_program        = 0;
    BlendState                               m_blend;
    DepthState                               m_depth;
    CullMode                                 m_cull           = CullMode::None;
    FrontFace                                m_front          = FrontFace::CounterClockwise;
    bool                                     m_color_write    = true;
    bool                                     m_scissor_on     = false;

    const Functions& fn() const noexcept { return m_ctx->fn; }

    static std::uint64_t align_up(std::uint64_t v, std::uint64_t a) noexcept { return (v + a - 1) / a * a; }

    void set_cap(native::Enum cap, bool on) const noexcept { if (on) fn().glEnable(cap); else fn().glDisable(cap); }

    void apply_state(const GraphicsPipelineDesc& d) noexcept {
        const bool force = !m_known;

        if (force || !(m_blend == d.blend)) {
            set_cap(gl::BLEND, d.blend.enable);
            if (d.blend.enable) fn().glBlendFuncSeparate(to_gl(d.blend.src_color), to_gl(d.blend.dst_color), to_gl(d.blend.src_alpha), to_gl(d.blend.dst_alpha));
            m_blend = d.blend;
        }

        if (force || m_depth.test != d.depth.test)       set_cap(gl::DEPTH_TEST, d.depth.test);
        if (force || m_depth.write != d.depth.write)     fn().glDepthMask(d.depth.write ? 1 : 0);
        if (force || m_depth.compare != d.depth.compare) fn().glDepthFunc(to_gl(d.depth.compare));
        if (force || m_depth.clamp != d.depth.clamp)     set_cap(gl::DEPTH_CLAMP, d.depth.clamp);

        if (force || m_depth.bias != d.depth.bias || m_depth.bias_constant != d.depth.bias_constant || m_depth.bias_slope != d.depth.bias_slope) {
            set_cap(gl::POLYGON_OFFSET_FILL, d.depth.bias);
            if (d.depth.bias) fn().glPolygonOffset(d.depth.bias_slope, d.depth.bias_constant);
        }

        m_depth = d.depth;

        if (force || m_cull != d.cull_mode) {
            set_cap(gl::CULL_FACE, d.cull_mode != CullMode::None);
            if (d.cull_mode != CullMode::None) fn().glCullFace(d.cull_mode == CullMode::Front ? gl::FRONT : gl::BACK);
            m_cull = d.cull_mode;
        }

        if (force || m_front != d.front_face) {
            fn().glFrontFace(d.front_face == FrontFace::CounterClockwise ? gl::CW : gl::CCW);
            m_front = d.front_face;
        }

        if (force || m_color_write != d.color_write) {
            const native::Boolean w = d.color_write ? 1 : 0;
            fn().glColorMask(w, w, w, w);
            m_color_write = d.color_write;
        }

        m_known = true;
    }

    void point_attributes(std::uint32_t binding, std::uint64_t extra) noexcept {
        if (!m_pipeline || binding >= kMaxBindings) return;
        const VertexBinding* vb = m_pipeline->binding(binding);
        const BoundBuffer& bb = m_buffers[binding];
        if (!vb || !bb.id) return;
        fn().glBindBuffer(gl::ARRAY_BUFFER, bb.id);

        for (const VertexAttribute& a : m_pipeline->desc().vertex_attributes) {
            if (a.binding != binding || a.location >= kMaxAttributes) continue;
            const AttributeFormat f = attribute_format(a.format);
            const void* at = reinterpret_cast<const void*>(static_cast<std::uintptr_t>(bb.offset + extra + a.offset));
            if (f.integer) fn().glVertexAttribIPointer(a.location, f.components, f.type, static_cast<native::Sizei>(vb->stride), at);
            else fn().glVertexAttribPointer(a.location, f.components, f.type, f.normalized ? 1 : 0, static_cast<native::Sizei>(vb->stride), at);
            fn().glVertexAttribDivisor(a.location, vb->rate == VertexRate::PerInstance ? 1u : 0u);
        }
    }

    void move_instances(std::uint32_t first_instance) noexcept {
        if (!m_pipeline) return;

        for (const VertexBinding& b : m_pipeline->desc().vertex_bindings) {
            if (b.rate != VertexRate::PerInstance) continue;
            point_attributes(b.binding, static_cast<std::uint64_t>(first_instance) * b.stride);
        }

        m_instance_moved = first_instance != 0;
    }

    native::Enum index_gl() const noexcept { return m_index_type == IndexType::UInt16 ? gl::UNSIGNED_SHORT : gl::UNSIGNED_INT; }
    std::uint64_t index_size() const noexcept { return m_index_type == IndexType::UInt16 ? 2u : 4u; }

public:
    Commands() noexcept = default;
    ~Commands() noexcept { destroy(); }

    Commands(const Commands&) = delete;
    Commands& operator=(const Commands&) = delete;

    Result create(const Context& ctx) noexcept {
        destroy();
        if (!ctx.valid()) return Result::InvalidArgument;
        m_ctx = ctx.state();
        if (!detail::usable(m_ctx)) return Result::NotInitialized;
        fn().glGenVertexArrays(1, &m_vao);
        if (!m_vao) return Result::OutOfMemory;
        fn().glBindVertexArray(m_vao);
        detail::label(*m_ctx, gl::VERTEX_ARRAY_OBJECT, m_vao, "fizmo vertex array");
        const Result r = m_push.create(ctx, { kPushRingStart, BufferTarget::Uniform, BufferUsage::Stream, "fizmo push constants" });
        if (failed(r)) { destroy(); return r; }
        invalidate();
        return Result::Success;
    }

    void destroy() noexcept {
        m_push.destroy();
        if (m_vao && detail::usable(m_ctx)) fn().glDeleteVertexArrays(1, &m_vao);
        m_vao = 0;
        m_pipeline = nullptr;
        m_ctx.reset();
    }

    bool valid() const noexcept { return m_vao != 0; }

    void invalidate() noexcept {
        m_known = false;
        m_program = 0;
        m_pipeline = nullptr;
        m_buffers = {};
        m_instance_moved = false;
        m_scissor_on = false;
        if (!m_ctx) return;
        fn().glBindVertexArray(m_vao);
        fn().glDisable(gl::SCISSOR_TEST);
        for (std::uint32_t i = 0; i < kMaxAttributes; ++i) if (m_attrib_mask & (1u << i)) fn().glDisableVertexAttribArray(i);
        m_attrib_mask = 0;
    }

    void begin_frame() noexcept {
        if (!m_ctx) return;
        m_ctx->make_current();
        invalidate();
        m_push_offset = 0;
        m_push.orphan();
    }

    void bind_framebuffer(native::Uint fbo) noexcept { fn().glBindFramebuffer(gl::FRAMEBUFFER, fbo); }

    void bind(const Pipeline& p) noexcept {
        if (!p.valid()) return;
        const GraphicsPipelineDesc& d = p.desc();

        if (m_program != d.program->id()) {
            fn().glUseProgram(d.program->id());
            m_program = d.program->id();
        }

        apply_state(d);
        std::uint32_t wanted = 0;
        for (const VertexAttribute& a : d.vertex_attributes) if (a.location < kMaxAttributes) wanted |= 1u << a.location;

        for (std::uint32_t i = 0; i < kMaxAttributes; ++i) {
            const std::uint32_t bit = 1u << i;
            if ((wanted & bit) && !(m_attrib_mask & bit)) fn().glEnableVertexAttribArray(i);
            if (!(wanted & bit) && (m_attrib_mask & bit)) fn().glDisableVertexAttribArray(i);
        }

        m_attrib_mask = wanted;
        m_pipeline = &p;
        m_buffers = {};
        m_instance_moved = false;
    }

    void use(const Program& program) noexcept {
        if (m_program == program.id()) return;
        fn().glUseProgram(program.id());
        m_program = program.id();
        m_pipeline = nullptr;
    }

    void bind_vertex_buffer(std::uint32_t binding, const Buffer& buffer, std::uint64_t offset = 0) noexcept {
        if (binding >= kMaxBindings || !buffer.valid()) return;
        m_buffers[binding] = { buffer.id(), offset };
        point_attributes(binding, 0);
    }

    void bind_index_buffer(const Buffer& buffer, IndexType type) noexcept {
        if (!buffer.valid()) return;
        fn().glBindBuffer(gl::ELEMENT_ARRAY_BUFFER, buffer.id());
        m_index_type = type;
    }

    void bind_texture(std::uint32_t unit, const Texture& tex, const Sampler& sampler) noexcept {
        fn().glActiveTexture(gl::TEXTURE0 + unit);
        fn().glBindTexture(tex.target(), tex.id());
        fn().glBindSampler(unit, sampler.id());
    }

    void bind_image(std::uint32_t unit, const Texture& tex, native::Enum access, native::Enum format) noexcept {
        if (!m_ctx->caps.compute) return;
        fn().glBindImageTexture(unit, tex.id(), 0, tex.type() == TextureType::Texture2D ? 0 : 1, 0, access, format);
    }

    void bind_uniform(std::uint32_t index, const Buffer& buffer, std::uint64_t offset, std::uint64_t size) noexcept {
        fn().glBindBufferRange(gl::UNIFORM_BUFFER, index, buffer.id(), static_cast<native::Intptr>(offset), static_cast<native::Sizeiptr>(size));
    }

    void push_constants(const void* data, std::uint32_t bytes, std::uint32_t binding = 1) noexcept {
        if (!data || bytes == 0 || !m_push.valid()) return;
        const std::uint64_t size = align_up(bytes, 16);
        const std::uint64_t step = align_up(size, m_ctx->caps.uniform_alignment);

        if (m_push_offset + step > m_push.size()) {
            if (failed(m_push.orphan(m_push.size() * 2))) return;
            m_push_offset = 0;
        }

        std::uint8_t block[256] = {};
        std::memcpy(block, data, std::min<std::uint32_t>(bytes, sizeof(block)));
        m_push.write(block, size, m_push_offset);
        bind_uniform(binding, m_push, m_push_offset, size);
        m_push_offset += step;
    }

    void set_viewport(const Viewport& v) noexcept {
        fn().glViewport(static_cast<native::Int>(std::floor(v.x)), static_cast<native::Int>(std::floor(v.y)),
                        static_cast<native::Sizei>(std::lround(v.width)), static_cast<native::Sizei>(std::lround(v.height)));
    }

    void set_scissor(const Rect2D& r) noexcept {
        if (!m_scissor_on) { fn().glEnable(gl::SCISSOR_TEST); m_scissor_on = true; }
        fn().glScissor(r.offset.x, r.offset.y, static_cast<native::Sizei>(r.extent.width), static_cast<native::Sizei>(r.extent.height));
    }

    void set_viewport_and_scissor(Extent2D e) noexcept {
        set_viewport({ 0.0f, 0.0f, static_cast<float>(e.width), static_cast<float>(e.height), 0.0f, 1.0f });
        set_scissor({ { 0, 0 }, e });
    }

    void no_scissor() noexcept {
        if (m_scissor_on) { fn().glDisable(gl::SCISSOR_TEST); m_scissor_on = false; }
    }

    void clear(const Rect2D& area, const ClearColor* color, const float* depth) noexcept {
        native::Bitfield mask = 0;
        set_scissor(area);

        if (color) {
            fn().glClearColor(color->r, color->g, color->b, color->a);
            if (!m_color_write) { fn().glColorMask(1, 1, 1, 1); m_color_write = true; }
            mask |= gl::COLOR_BUFFER_BIT;
        }

        if (depth) {
            fn().glClearDepth(*depth);
            if (!m_depth.write) { fn().glDepthMask(1); m_depth.write = true; }
            mask |= gl::DEPTH_BUFFER_BIT;
        }

        if (mask) fn().glClear(mask);
    }

    void draw(std::uint32_t count, std::uint32_t instances = 1, std::uint32_t first = 0, std::uint32_t first_instance = 0) noexcept {
        if (count == 0 || instances == 0 || !m_pipeline) return;
        const native::Enum mode = to_gl(m_pipeline->desc().topology);

        if (first_instance != 0 && m_ctx->caps.base_instance) {
            if (m_instance_moved) move_instances(0);
            fn().glDrawArraysInstancedBaseInstance(mode, static_cast<native::Int>(first), static_cast<native::Sizei>(count), static_cast<native::Sizei>(instances), first_instance);
            return;
        }

        if (first_instance != 0 || m_instance_moved) move_instances(first_instance);
        if (instances == 1) fn().glDrawArrays(mode, static_cast<native::Int>(first), static_cast<native::Sizei>(count));
        else fn().glDrawArraysInstanced(mode, static_cast<native::Int>(first), static_cast<native::Sizei>(count), static_cast<native::Sizei>(instances));
    }

    void draw_unbound(std::uint32_t count) noexcept {
        if (count) fn().glDrawArrays(gl::TRIANGLES, 0, static_cast<native::Sizei>(count));
    }

    void draw_indexed(std::uint32_t count, std::uint32_t instances = 1, std::uint32_t first_index = 0, std::int32_t vertex_offset = 0, std::uint32_t first_instance = 0) noexcept {
        if (count == 0 || instances == 0 || !m_pipeline) return;
        const native::Enum mode = to_gl(m_pipeline->desc().topology);
        const void* at = reinterpret_cast<const void*>(static_cast<std::uintptr_t>(first_index * index_size()));

        if (first_instance != 0 && m_ctx->caps.base_instance) {
            if (m_instance_moved) move_instances(0);
            fn().glDrawElementsInstancedBaseVertexBaseInstance(mode, static_cast<native::Sizei>(count), index_gl(), at, static_cast<native::Sizei>(instances), vertex_offset, first_instance);
            return;
        }

        if (first_instance != 0 || m_instance_moved) move_instances(first_instance);
        if (instances == 1) fn().glDrawElementsBaseVertex(mode, static_cast<native::Sizei>(count), index_gl(), at, vertex_offset);
        else fn().glDrawElementsInstancedBaseVertex(mode, static_cast<native::Sizei>(count), index_gl(), at, static_cast<native::Sizei>(instances), vertex_offset);
    }

    void draw_indexed_indirect(const Buffer& commands, std::uint64_t offset, std::uint32_t count, std::uint32_t stride) noexcept {
        if (!m_pipeline || count == 0 || !m_ctx->caps.multi_draw) return;
        if (m_instance_moved) move_instances(0);
        fn().glBindBuffer(gl::DRAW_INDIRECT_BUFFER, commands.id());
        fn().glMultiDrawElementsIndirect(to_gl(m_pipeline->desc().topology), index_gl(), reinterpret_cast<const void*>(static_cast<std::uintptr_t>(offset)),
                                         static_cast<native::Sizei>(count), static_cast<native::Sizei>(stride));
    }

    void dispatch(std::uint32_t x, std::uint32_t y = 1, std::uint32_t z = 1) noexcept {
        if (m_ctx->caps.compute && x && y && z) fn().glDispatchCompute(x, y, z);
    }

    void memory_barrier(native::Bitfield bits) noexcept {
        if (m_ctx->caps.compute) fn().glMemoryBarrier(bits);
    }

    void blit(native::Uint read_fbo, const Rect2D& src, native::Uint draw_fbo, const Rect2D& dst, native::Bitfield mask, Filter filter) noexcept {
        no_scissor();
        fn().glBindFramebuffer(gl::READ_FRAMEBUFFER, read_fbo);
        fn().glBindFramebuffer(gl::DRAW_FRAMEBUFFER, draw_fbo);
        const native::Int sx0 = src.offset.x, sy0 = src.offset.y;
        const native::Int sx1 = sx0 + static_cast<native::Int>(src.extent.width), sy1 = sy0 + static_cast<native::Int>(src.extent.height);
        const native::Int dx0 = dst.offset.x, dy0 = dst.offset.y;
        const native::Int dx1 = dx0 + static_cast<native::Int>(dst.extent.width), dy1 = dy0 + static_cast<native::Int>(dst.extent.height);
        fn().glBlitFramebuffer(sx0, sy0, sx1, sy1, dx0, dy0, dx1, dy1, mask, to_gl(filter));
    }

    void begin_label(const char* name) noexcept {
        if (m_ctx->caps.debug && name) fn().glPushDebugGroup(gl::DEBUG_SOURCE_APPLICATION, 0, -1, name);
    }

    void end_label() noexcept {
        if (m_ctx->caps.debug) fn().glPopDebugGroup();
    }
};

} // namespace opengl
} // namespace fizmo

#endif // FIZMO_OPENGL_COMMANDS_HPP
