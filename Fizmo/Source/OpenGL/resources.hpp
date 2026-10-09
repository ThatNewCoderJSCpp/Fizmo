#ifndef FIZMO_OPENGL_RESOURCES_HPP
#define FIZMO_OPENGL_RESOURCES_HPP

#include "context.hpp"
#include <algorithm>
#include <cstring>
#include <memory>
#include <utility>
#include <vector>

namespace fizmo {
namespace opengl {

namespace detail {

inline void label(const ContextState& s, native::Enum identifier, native::Uint id, const char* name) noexcept {
    if (name && id && s.caps.debug) s.fn.glObjectLabel(identifier, id, -1, name);
}

inline native::Uint scratch_unit(const ContextState& s) noexcept {
    return static_cast<native::Uint>(std::max(s.caps.combined_units, 2) - 1);
}

inline bool usable(const std::shared_ptr<ContextState>& s) noexcept {
    return s && s->alive && s->make_current();
}

inline Result take_error(const ContextState& s) noexcept {
    const native::Enum e = s.fn.glGetError();
    while (s.fn.glGetError() != 0) {}
    if (e == gl::OUT_OF_MEMORY) return Result::OutOfMemory;
    return e == 0 ? Result::Success : Result::InvalidArgument;
}

} // namespace detail

struct BufferDesc {
    std::uint64_t size   = 0;
    BufferTarget  target = BufferTarget::Vertex;
    BufferUsage   usage  = BufferUsage::Static;
    const char*   name   = nullptr;
};

class Buffer {
private:
    std::shared_ptr<ContextState> m_ctx;
    native::Uint                  m_id     = 0;
    std::uint64_t                 m_size   = 0;
    BufferTarget                  m_target = BufferTarget::Vertex;
    BufferUsage                   m_usage  = BufferUsage::Static;

public:
    Buffer() noexcept = default;
    ~Buffer() noexcept { destroy(); }

    Buffer(const Buffer&) = delete;
    Buffer& operator=(const Buffer&) = delete;

    Buffer(Buffer&& o) noexcept
        : m_ctx(std::move(o.m_ctx)), m_id(std::exchange(o.m_id, 0)), m_size(std::exchange(o.m_size, 0)), m_target(o.m_target), m_usage(o.m_usage) {}

    Buffer& operator=(Buffer&& o) noexcept {
        if (this != &o) {
            destroy();
            m_ctx    = std::move(o.m_ctx);
            m_id     = std::exchange(o.m_id, 0);
            m_size   = std::exchange(o.m_size, 0);
            m_target = o.m_target;
            m_usage  = o.m_usage;
        }
        return *this;
    }

    Result create(const Context& ctx, const BufferDesc& d) noexcept {
        destroy();
        if (!ctx.valid() || d.size == 0) return Result::InvalidArgument;
        m_ctx = ctx.state();
        if (!detail::usable(m_ctx)) return Result::NotInitialized;
        const Functions& fn = m_ctx->fn;
        fn.glGenBuffers(1, &m_id);
        if (!m_id) return Result::OutOfMemory;
        m_target = d.target;
        m_usage  = d.usage;
        fn.glBindBuffer(gl::COPY_WRITE_BUFFER, m_id);
        fn.glBufferData(gl::COPY_WRITE_BUFFER, static_cast<native::Sizeiptr>(d.size), nullptr, to_gl(d.usage));
        detail::label(*m_ctx, gl::BUFFER_OBJECT, m_id, d.name);
        const Result r = detail::take_error(*m_ctx);
        if (failed(r)) { destroy(); return r; }
        m_size = d.size;
        return Result::Success;
    }

    void destroy() noexcept {
        if (m_id && detail::usable(m_ctx)) m_ctx->fn.glDeleteBuffers(1, &m_id);
        m_id = 0;
        m_size = 0;
        m_ctx.reset();
    }

    Result write(const void* data, std::uint64_t bytes, std::uint64_t offset = 0) const noexcept {
        if (!m_id || !data || offset + bytes > m_size) return Result::InvalidArgument;
        if (bytes == 0) return Result::Success;
        if (!detail::usable(m_ctx)) return Result::NotInitialized;
        m_ctx->fn.glBindBuffer(gl::COPY_WRITE_BUFFER, m_id);
        m_ctx->fn.glBufferSubData(gl::COPY_WRITE_BUFFER, static_cast<native::Intptr>(offset), static_cast<native::Sizeiptr>(bytes), data);
        return Result::Success;
    }

    Result orphan(std::uint64_t size = 0) noexcept {
        if (!m_id) return Result::InvalidArgument;
        if (!detail::usable(m_ctx)) return Result::NotInitialized;
        const std::uint64_t bytes = size ? size : m_size;
        m_ctx->fn.glBindBuffer(gl::COPY_WRITE_BUFFER, m_id);
        m_ctx->fn.glBufferData(gl::COPY_WRITE_BUFFER, static_cast<native::Sizeiptr>(bytes), nullptr, to_gl(m_usage));
        const Result r = detail::take_error(*m_ctx);
        if (succeeded(r)) m_size = bytes;
        return r;
    }

    Result stream(const void* data, std::uint64_t bytes) noexcept {
        if (!m_id || (!data && bytes)) return Result::InvalidArgument;
        if (!detail::usable(m_ctx)) return Result::NotInitialized;
        const Functions& fn = m_ctx->fn;
        fn.glBindBuffer(gl::COPY_WRITE_BUFFER, m_id);

        if (bytes > m_size) {
            const std::uint64_t grown = std::max<std::uint64_t>(bytes, m_size * 2);
            fn.glBufferData(gl::COPY_WRITE_BUFFER, static_cast<native::Sizeiptr>(grown), nullptr, to_gl(m_usage));
            const Result r = detail::take_error(*m_ctx);
            if (failed(r)) return r;
            m_size = grown;
        } else {
            fn.glBufferData(gl::COPY_WRITE_BUFFER, static_cast<native::Sizeiptr>(m_size), nullptr, to_gl(m_usage));
        }

        if (bytes) fn.glBufferSubData(gl::COPY_WRITE_BUFFER, 0, static_cast<native::Sizeiptr>(bytes), data);
        return Result::Success;
    }

    Result read(void* out, std::uint64_t bytes, std::uint64_t offset = 0) const noexcept {
        if (!m_id || !out || offset + bytes > m_size) return Result::InvalidArgument;
        if (!detail::usable(m_ctx)) return Result::NotInitialized;
        m_ctx->fn.glBindBuffer(gl::COPY_READ_BUFFER, m_id);
        m_ctx->fn.glGetBufferSubData(gl::COPY_READ_BUFFER, static_cast<native::Intptr>(offset), static_cast<native::Sizeiptr>(bytes), out);
        return Result::Success;
    }

    bool          valid()  const noexcept { return m_id != 0; }
    native::Uint  id()     const noexcept { return m_id; }
    std::uint64_t size()   const noexcept { return m_size; }
    BufferTarget  target() const noexcept { return m_target; }
};

struct TextureDesc {
    TextureType type   = TextureType::Texture2D;
    Extent3D    extent;
    Format      format = Format::RGBA8Unorm;
    const char* name   = nullptr;
};

class Texture {
private:
    std::shared_ptr<ContextState> m_ctx;
    native::Uint                  m_id     = 0;
    TextureType                   m_type   = TextureType::Texture2D;
    Extent3D                      m_extent;
    Format                        m_format = Format::Undefined;

public:
    Texture() noexcept = default;
    ~Texture() noexcept { destroy(); }

    Texture(const Texture&) = delete;
    Texture& operator=(const Texture&) = delete;

    Texture(Texture&& o) noexcept
        : m_ctx(std::move(o.m_ctx)), m_id(std::exchange(o.m_id, 0)), m_type(o.m_type), m_extent(o.m_extent), m_format(o.m_format) {}

    Texture& operator=(Texture&& o) noexcept {
        if (this != &o) {
            destroy();
            m_ctx    = std::move(o.m_ctx);
            m_id     = std::exchange(o.m_id, 0);
            m_type   = o.m_type;
            m_extent = o.m_extent;
            m_format = o.m_format;
        }
        return *this;
    }

    Result create(const Context& ctx, const TextureDesc& d) noexcept {
        destroy();
        if (!ctx.valid() || d.extent.width == 0 || d.extent.height == 0 || d.extent.depth == 0 || d.format == Format::Undefined) return Result::InvalidArgument;
        m_ctx = ctx.state();
        if (!detail::usable(m_ctx)) return Result::NotInitialized;
        const Functions& fn = m_ctx->fn;
        fn.glGenTextures(1, &m_id);
        if (!m_id) return Result::OutOfMemory;
        m_type   = d.type;
        m_extent = d.extent;
        m_format = d.format;
        const native::Enum target = to_gl(d.type);
        const TextureFormat tf = texture_format(d.format);
        fn.glActiveTexture(gl::TEXTURE0 + detail::scratch_unit(*m_ctx));
        fn.glBindTexture(target, m_id);
        const native::Sizei w = static_cast<native::Sizei>(d.extent.width), h = static_cast<native::Sizei>(d.extent.height);

        if (d.type == TextureType::Cube) {
            for (native::Enum f = 0; f < 6; ++f)
                fn.glTexImage2D(gl::TEXTURE_CUBE_MAP_POSITIVE_X + f, 0, static_cast<native::Int>(tf.internal), w, h, 0, tf.format, tf.type, nullptr);
        } else if (d.type == TextureType::Texture3D) {
            fn.glTexImage3D(target, 0, static_cast<native::Int>(tf.internal), w, h, static_cast<native::Sizei>(d.extent.depth), 0, tf.format, tf.type, nullptr);
        } else {
            fn.glTexImage2D(target, 0, static_cast<native::Int>(tf.internal), w, h, 0, tf.format, tf.type, nullptr);
        }

        fn.glTexParameteri(target, gl::TEXTURE_BASE_LEVEL, 0);
        fn.glTexParameteri(target, gl::TEXTURE_MAX_LEVEL, 0);
        fn.glTexParameteri(target, gl::TEXTURE_MIN_FILTER, static_cast<native::Int>(gl::NEAREST));
        fn.glTexParameteri(target, gl::TEXTURE_MAG_FILTER, static_cast<native::Int>(gl::NEAREST));
        fn.glBindTexture(target, 0);
        detail::label(*m_ctx, gl::TEXTURE_OBJECT, m_id, d.name);
        const Result r = detail::take_error(*m_ctx);
        if (failed(r)) { destroy(); return r; }
        return Result::Success;
    }

    void destroy() noexcept {
        if (m_id && detail::usable(m_ctx)) m_ctx->fn.glDeleteTextures(1, &m_id);
        m_id = 0;
        m_format = Format::Undefined;
        m_ctx.reset();
    }

    Result upload(const void* pixels, Offset2D at, Extent2D size, std::uint32_t row_pixels = 0, std::uint32_t layer = 0) const noexcept {
        if (!m_id || !pixels || size.width == 0 || size.height == 0) return Result::InvalidArgument;
        if (!detail::usable(m_ctx)) return Result::NotInitialized;
        const Functions& fn = m_ctx->fn;
        const TextureFormat tf = texture_format(m_format);
        const native::Enum target = to_gl(m_type);
        fn.glActiveTexture(gl::TEXTURE0 + detail::scratch_unit(*m_ctx));
        fn.glBindTexture(target, m_id);
        fn.glPixelStorei(gl::UNPACK_ALIGNMENT, 1);
        fn.glPixelStorei(gl::UNPACK_ROW_LENGTH, static_cast<native::Int>(row_pixels));
        const native::Enum face = m_type == TextureType::Cube ? gl::TEXTURE_CUBE_MAP_POSITIVE_X + layer : target;
        fn.glTexSubImage2D(face, 0, at.x, at.y, static_cast<native::Sizei>(size.width), static_cast<native::Sizei>(size.height), tf.format, tf.type, pixels);
        fn.glPixelStorei(gl::UNPACK_ROW_LENGTH, 0);
        fn.glPixelStorei(gl::UNPACK_ALIGNMENT, 4);
        fn.glBindTexture(target, 0);
        return Result::Success;
    }

    const Texture& view()   const noexcept { return *this; }
    bool           valid()  const noexcept { return m_id != 0; }
    native::Uint   id()     const noexcept { return m_id; }
    TextureType    type()   const noexcept { return m_type; }
    native::Enum   target() const noexcept { return to_gl(m_type); }
    Extent3D       extent() const noexcept { return m_extent; }
    Format         format() const noexcept { return m_format; }
};

class Renderbuffer {
private:
    std::shared_ptr<ContextState> m_ctx;
    native::Uint                  m_id      = 0;
    Format                        m_format  = Format::Undefined;
    std::uint32_t                 m_samples = 1;
    Extent2D                      m_extent;

public:
    Renderbuffer() noexcept = default;
    ~Renderbuffer() noexcept { destroy(); }

    Renderbuffer(const Renderbuffer&) = delete;
    Renderbuffer& operator=(const Renderbuffer&) = delete;

    Renderbuffer(Renderbuffer&& o) noexcept
        : m_ctx(std::move(o.m_ctx)), m_id(std::exchange(o.m_id, 0)), m_format(o.m_format), m_samples(o.m_samples), m_extent(o.m_extent) {}

    Renderbuffer& operator=(Renderbuffer&& o) noexcept {
        if (this != &o) {
            destroy();
            m_ctx     = std::move(o.m_ctx);
            m_id      = std::exchange(o.m_id, 0);
            m_format  = o.m_format;
            m_samples = o.m_samples;
            m_extent  = o.m_extent;
        }
        return *this;
    }

    Result create(const Context& ctx, Format format, Extent2D size, std::uint32_t samples, const char* name = nullptr) noexcept {
        destroy();
        if (!ctx.valid() || size.width == 0 || size.height == 0) return Result::InvalidArgument;
        m_ctx = ctx.state();
        if (!detail::usable(m_ctx)) return Result::NotInitialized;
        const Functions& fn = m_ctx->fn;
        fn.glGenRenderbuffers(1, &m_id);
        if (!m_id) return Result::OutOfMemory;
        fn.glBindRenderbuffer(gl::RENDERBUFFER, m_id);
        fn.glRenderbufferStorageMultisample(gl::RENDERBUFFER, static_cast<native::Sizei>(samples > 1 ? samples : 0), texture_format(format).internal,
                                            static_cast<native::Sizei>(size.width), static_cast<native::Sizei>(size.height));
        fn.glBindRenderbuffer(gl::RENDERBUFFER, 0);
        detail::label(*m_ctx, gl::RENDERBUFFER_OBJECT, m_id, name);
        const Result r = detail::take_error(*m_ctx);
        if (failed(r)) { destroy(); return r; }
        m_format  = format;
        m_samples = samples;
        m_extent  = size;
        return Result::Success;
    }

    void destroy() noexcept {
        if (m_id && detail::usable(m_ctx)) m_ctx->fn.glDeleteRenderbuffers(1, &m_id);
        m_id = 0;
        m_ctx.reset();
    }

    bool          valid()   const noexcept { return m_id != 0; }
    native::Uint  id()      const noexcept { return m_id; }
    Format        format()  const noexcept { return m_format; }
    std::uint32_t samples() const noexcept { return m_samples; }
    Extent2D      extent()  const noexcept { return m_extent; }
};

class Framebuffer {
private:
    std::shared_ptr<ContextState> m_ctx;
    native::Uint                  m_id = 0;

    bool bind_for_edit() const noexcept {
        if (!m_id || !detail::usable(m_ctx)) return false;
        m_ctx->fn.glBindFramebuffer(gl::FRAMEBUFFER, m_id);
        return true;
    }

    static native::Enum depth_point(Format f) noexcept { return has_stencil(f) ? gl::DEPTH_STENCIL_ATTACHMENT : gl::DEPTH_ATTACHMENT; }

public:
    Framebuffer() noexcept = default;
    ~Framebuffer() noexcept { destroy(); }

    Framebuffer(const Framebuffer&) = delete;
    Framebuffer& operator=(const Framebuffer&) = delete;

    Framebuffer(Framebuffer&& o) noexcept : m_ctx(std::move(o.m_ctx)), m_id(std::exchange(o.m_id, 0)) {}

    Framebuffer& operator=(Framebuffer&& o) noexcept {
        if (this != &o) {
            destroy();
            m_ctx = std::move(o.m_ctx);
            m_id  = std::exchange(o.m_id, 0);
        }
        return *this;
    }

    Result create(const Context& ctx, const char* name = nullptr) noexcept {
        destroy();
        if (!ctx.valid()) return Result::InvalidArgument;
        m_ctx = ctx.state();
        if (!detail::usable(m_ctx)) return Result::NotInitialized;
        m_ctx->fn.glGenFramebuffers(1, &m_id);
        if (!m_id) return Result::OutOfMemory;
        m_ctx->fn.glBindFramebuffer(gl::FRAMEBUFFER, m_id);
        detail::label(*m_ctx, gl::FRAMEBUFFER_OBJECT, m_id, name);
        return Result::Success;
    }

    void destroy() noexcept {
        if (m_id && detail::usable(m_ctx)) m_ctx->fn.glDeleteFramebuffers(1, &m_id);
        m_id = 0;
        m_ctx.reset();
    }

    void color(const Texture& t, int layer = -1) const noexcept {
        if (!bind_for_edit()) return;
        const Functions& fn = m_ctx->fn;
        if (!t.valid()) { fn.glFramebufferRenderbuffer(gl::FRAMEBUFFER, gl::COLOR_ATTACHMENT0, gl::RENDERBUFFER, 0); return; }
        if (t.type() == TextureType::Cube && layer >= 0) fn.glFramebufferTexture2D(gl::FRAMEBUFFER, gl::COLOR_ATTACHMENT0, gl::TEXTURE_CUBE_MAP_POSITIVE_X + static_cast<native::Enum>(layer), t.id(), 0);
        else if (t.type() == TextureType::Texture3D && layer >= 0) fn.glFramebufferTextureLayer(gl::FRAMEBUFFER, gl::COLOR_ATTACHMENT0, t.id(), 0, layer);
        else fn.glFramebufferTexture2D(gl::FRAMEBUFFER, gl::COLOR_ATTACHMENT0, gl::TEXTURE_2D, t.id(), 0);
    }

    void color(const Renderbuffer& rb) const noexcept {
        if (!bind_for_edit()) return;
        m_ctx->fn.glFramebufferRenderbuffer(gl::FRAMEBUFFER, gl::COLOR_ATTACHMENT0, gl::RENDERBUFFER, rb.id());
    }

    void no_color() const noexcept {
        if (!bind_for_edit()) return;
        const Functions& fn = m_ctx->fn;
        fn.glFramebufferTexture2D(gl::FRAMEBUFFER, gl::COLOR_ATTACHMENT0, gl::TEXTURE_2D, 0, 0);
        fn.glDrawBuffer(gl::NONE);
        fn.glReadBuffer(gl::NONE);
    }

    void depth(const Texture& t, int face = -1) const noexcept {
        if (!bind_for_edit()) return;
        const Functions& fn = m_ctx->fn;
        const native::Enum point = depth_point(t.format());
        if (t.type() == TextureType::Cube && face >= 0) fn.glFramebufferTexture2D(gl::FRAMEBUFFER, point, gl::TEXTURE_CUBE_MAP_POSITIVE_X + static_cast<native::Enum>(face), t.id(), 0);
        else fn.glFramebufferTexture2D(gl::FRAMEBUFFER, point, gl::TEXTURE_2D, t.id(), 0);
    }

    void depth(const Renderbuffer& rb) const noexcept {
        if (!bind_for_edit()) return;
        m_ctx->fn.glFramebufferRenderbuffer(gl::FRAMEBUFFER, depth_point(rb.format()), gl::RENDERBUFFER, rb.id());
    }

    Result check() const noexcept {
        if (!bind_for_edit()) return Result::NotInitialized;
        return m_ctx->fn.glCheckFramebufferStatus(gl::FRAMEBUFFER) == gl::FRAMEBUFFER_COMPLETE ? Result::Success : Result::IncompleteFramebuffer;
    }

    bool         valid() const noexcept { return m_id != 0; }
    native::Uint id()    const noexcept { return m_id; }
};

struct SamplerDesc {
    Filter      mag_filter     = Filter::Nearest;
    Filter      min_filter     = Filter::Nearest;
    AddressMode address_u      = AddressMode::ClampToEdge;
    AddressMode address_v      = AddressMode::ClampToEdge;
    AddressMode address_w      = AddressMode::ClampToEdge;
    bool        compare_enable = false;
    CompareOp   compare_op     = CompareOp::LessEqual;
    float       max_lod        = 0.0f;
};

class Sampler {
private:
    std::shared_ptr<ContextState> m_ctx;
    native::Uint                  m_id = 0;

public:
    Sampler() noexcept = default;
    ~Sampler() noexcept { destroy(); }

    Sampler(const Sampler&) = delete;
    Sampler& operator=(const Sampler&) = delete;
    Sampler(Sampler&& o) noexcept : m_ctx(std::move(o.m_ctx)), m_id(std::exchange(o.m_id, 0)) {}

    Sampler& operator=(Sampler&& o) noexcept {
        if (this != &o) {
            destroy();
            m_ctx = std::move(o.m_ctx);
            m_id  = std::exchange(o.m_id, 0);
        }
        return *this;
    }

    Result create(const Context& ctx, const SamplerDesc& d) noexcept {
        destroy();
        if (!ctx.valid()) return Result::InvalidArgument;
        m_ctx = ctx.state();
        if (!detail::usable(m_ctx)) return Result::NotInitialized;
        const Functions& fn = m_ctx->fn;
        fn.glGenSamplers(1, &m_id);
        if (!m_id) return Result::OutOfMemory;
        fn.glSamplerParameteri(m_id, gl::TEXTURE_MAG_FILTER, static_cast<native::Int>(to_gl(d.mag_filter)));
        fn.glSamplerParameteri(m_id, gl::TEXTURE_MIN_FILTER, static_cast<native::Int>(to_gl(d.min_filter)));
        fn.glSamplerParameteri(m_id, gl::TEXTURE_WRAP_S, static_cast<native::Int>(to_gl(d.address_u)));
        fn.glSamplerParameteri(m_id, gl::TEXTURE_WRAP_T, static_cast<native::Int>(to_gl(d.address_v)));
        fn.glSamplerParameteri(m_id, gl::TEXTURE_WRAP_R, static_cast<native::Int>(to_gl(d.address_w)));
        fn.glSamplerParameterf(m_id, gl::TEXTURE_MAX_LOD, d.max_lod);

        if (d.compare_enable) {
            fn.glSamplerParameteri(m_id, gl::TEXTURE_COMPARE_MODE, static_cast<native::Int>(gl::COMPARE_REF_TO_TEXTURE));
            fn.glSamplerParameteri(m_id, gl::TEXTURE_COMPARE_FUNC, static_cast<native::Int>(to_gl(d.compare_op)));
        }

        return detail::take_error(*m_ctx);
    }

    void destroy() noexcept {
        if (m_id && detail::usable(m_ctx)) m_ctx->fn.glDeleteSamplers(1, &m_id);
        m_id = 0;
        m_ctx.reset();
    }

    bool         valid() const noexcept { return m_id != 0; }
    native::Uint id()    const noexcept { return m_id; }
};

class TimestampPool {
private:
    std::shared_ptr<ContextState> m_ctx;
    std::vector<native::Uint>     m_ids;

public:
    TimestampPool() noexcept = default;
    ~TimestampPool() noexcept { destroy(); }

    TimestampPool(const TimestampPool&) = delete;
    TimestampPool& operator=(const TimestampPool&) = delete;
    TimestampPool(TimestampPool&&) noexcept = default;

    TimestampPool& operator=(TimestampPool&& o) noexcept {
        if (this != &o) {
            destroy();
            m_ctx = std::move(o.m_ctx);
            m_ids = std::move(o.m_ids);
        }
        return *this;
    }

    Result create(const Context& ctx, std::uint32_t count) {
        destroy();
        if (!ctx.valid() || count == 0 || !ctx.caps().timer_query) return Result::Unsupported;
        m_ctx = ctx.state();
        if (!detail::usable(m_ctx)) return Result::NotInitialized;
        m_ids.assign(count, 0);
        m_ctx->fn.glGenQueries(static_cast<native::Sizei>(count), m_ids.data());
        return detail::take_error(*m_ctx);
    }

    void destroy() noexcept {
        if (!m_ids.empty() && detail::usable(m_ctx)) m_ctx->fn.glDeleteQueries(static_cast<native::Sizei>(m_ids.size()), m_ids.data());
        m_ids.clear();
        m_ctx.reset();
    }

    void write(std::uint32_t index) const noexcept {
        if (index < m_ids.size() && m_ctx) m_ctx->fn.glQueryCounter(m_ids[index], gl::TIMESTAMP);
    }

    bool read(std::uint32_t first, std::uint32_t count, std::uint64_t* out) const noexcept {
        if (!m_ctx || first + count > m_ids.size() || count == 0) return false;
        const Functions& fn = m_ctx->fn;
        native::Int ready = 0;
        fn.glGetQueryObjectiv(m_ids[first + count - 1], gl::QUERY_RESULT_AVAILABLE, &ready);
        if (!ready) return false;
        for (std::uint32_t i = 0; i < count; ++i) fn.glGetQueryObjectui64v(m_ids[first + i], gl::QUERY_RESULT, &out[i]);
        return true;
    }

    static double ticks_to_ms(std::uint64_t ticks) noexcept { return static_cast<double>(ticks) * 1e-6; }
    bool valid() const noexcept { return !m_ids.empty(); }
};

} // namespace opengl
} // namespace fizmo

#endif // FIZMO_OPENGL_RESOURCES_HPP
