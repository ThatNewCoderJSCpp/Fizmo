#ifndef FIZMO_OPENGL_LOADER_HPP
#define FIZMO_OPENGL_LOADER_HPP

#include "compat.hpp"
#include <utility>

#define FIZMO_GL_CORE_FUNCTIONS(X)                                                                                                                         \
    X(native::Enum,          glGetError,                         (void))                                                                                   \
    X(const native::Ubyte*,  glGetString,                        (native::Enum name))                                                                      \
    X(const native::Ubyte*,  glGetStringi,                       (native::Enum name, native::Uint index))                                                  \
    X(void,                  glGetIntegerv,                      (native::Enum pname, native::Int* data))                                                  \
    X(void,                  glGetInteger64v,                    (native::Enum pname, native::Int64* data))                                                \
    X(void,                  glEnable,                           (native::Enum cap))                                                                       \
    X(void,                  glDisable,                          (native::Enum cap))                                                                       \
    X(void,                  glViewport,                         (native::Int x, native::Int y, native::Sizei w, native::Sizei h))                         \
    X(void,                  glScissor,                          (native::Int x, native::Int y, native::Sizei w, native::Sizei h))                         \
    X(void,                  glClearColor,                       (native::Float r, native::Float g, native::Float b, native::Float a))                     \
    X(void,                  glClearDepth,                       (native::Double depth))                                                                   \
    X(void,                  glClear,                            (native::Bitfield mask))                                                                  \
    X(void,                  glColorMask,                        (native::Boolean r, native::Boolean g, native::Boolean b, native::Boolean a))             \
    X(void,                  glDepthMask,                        (native::Boolean flag))                                                                   \
    X(void,                  glDepthFunc,                        (native::Enum func))                                                                      \
    X(void,                  glBlendFuncSeparate,                (native::Enum sc, native::Enum dc, native::Enum sa, native::Enum da))                     \
    X(void,                  glBlendEquation,                    (native::Enum mode))                                                                      \
    X(void,                  glCullFace,                         (native::Enum mode))                                                                      \
    X(void,                  glFrontFace,                        (native::Enum mode))                                                                      \
    X(void,                  glPolygonOffset,                    (native::Float factor, native::Float units))                                              \
    X(void,                  glPixelStorei,                      (native::Enum pname, native::Int param))                                                  \
    X(void,                  glReadPixels,                       (native::Int x, native::Int y, native::Sizei w, native::Sizei h, native::Enum format, native::Enum type, void* data)) \
    X(void,                  glFlush,                            (void))                                                                                   \
    X(void,                  glFinish,                           (void))                                                                                   \
    X(void,                  glGenTextures,                      (native::Sizei n, native::Uint* out))                                                     \
    X(void,                  glDeleteTextures,                   (native::Sizei n, const native::Uint* ids))                                               \
    X(void,                  glBindTexture,                      (native::Enum target, native::Uint id))                                                   \
    X(void,                  glActiveTexture,                    (native::Enum unit))                                                                      \
    X(void,                  glTexImage2D,                       (native::Enum target, native::Int level, native::Int internal, native::Sizei w, native::Sizei h, native::Int border, native::Enum format, native::Enum type, const void* data)) \
    X(void,                  glTexImage3D,                       (native::Enum target, native::Int level, native::Int internal, native::Sizei w, native::Sizei h, native::Sizei d, native::Int border, native::Enum format, native::Enum type, const void* data)) \
    X(void,                  glTexSubImage3D,                    (native::Enum target, native::Int level, native::Int x, native::Int y, native::Int z, native::Sizei w, native::Sizei h, native::Sizei d, native::Enum format, native::Enum type, const void* data)) \
    X(void,                  glGenerateMipmap,                   (native::Enum target))                                                                    \
    X(void,                  glClearBufferfv,                    (native::Enum buffer, native::Int drawbuffer, const native::Float* value))                \
    X(void,                  glClearBufferfi,                    (native::Enum buffer, native::Int drawbuffer, native::Float depth, native::Int stencil)) \
    X(void,                  glTexSubImage2D,                    (native::Enum target, native::Int level, native::Int x, native::Int y, native::Sizei w, native::Sizei h, native::Enum format, native::Enum type, const void* data)) \
    X(void,                  glTexParameteri,                    (native::Enum target, native::Enum pname, native::Int param))                             \
    X(void,                  glTexImage2DMultisample,            (native::Enum target, native::Sizei samples, native::Enum internal, native::Sizei w, native::Sizei h, native::Boolean fixed)) \
    X(void,                  glGenSamplers,                      (native::Sizei n, native::Uint* out))                                                     \
    X(void,                  glDeleteSamplers,                   (native::Sizei n, const native::Uint* ids))                                               \
    X(void,                  glBindSampler,                      (native::Uint unit, native::Uint sampler))                                                \
    X(void,                  glSamplerParameteri,                (native::Uint sampler, native::Enum pname, native::Int param))                            \
    X(void,                  glSamplerParameterf,                (native::Uint sampler, native::Enum pname, native::Float param))                          \
    X(void,                  glGenBuffers,                       (native::Sizei n, native::Uint* out))                                                     \
    X(void,                  glDeleteBuffers,                    (native::Sizei n, const native::Uint* ids))                                               \
    X(void,                  glBindBuffer,                       (native::Enum target, native::Uint id))                                                   \
    X(void,                  glBufferData,                       (native::Enum target, native::Sizeiptr size, const void* data, native::Enum usage))      \
    X(void,                  glBufferSubData,                    (native::Enum target, native::Intptr offset, native::Sizeiptr size, const void* data))   \
    X(void,                  glBindBufferRange,                  (native::Enum target, native::Uint index, native::Uint id, native::Intptr offset, native::Sizeiptr size)) \
    X(void,                  glBindBufferBase,                   (native::Enum target, native::Uint index, native::Uint id))                               \
    X(void,                  glCopyBufferSubData,                (native::Enum read, native::Enum write, native::Intptr ro, native::Intptr wo, native::Sizeiptr size)) \
    X(void,                  glGetBufferSubData,                 (native::Enum target, native::Intptr offset, native::Sizeiptr size, void* data))         \
    X(void,                  glGenVertexArrays,                  (native::Sizei n, native::Uint* out))                                                     \
    X(void,                  glDeleteVertexArrays,               (native::Sizei n, const native::Uint* ids))                                               \
    X(void,                  glBindVertexArray,                  (native::Uint id))                                                                        \
    X(void,                  glEnableVertexAttribArray,          (native::Uint index))                                                                     \
    X(void,                  glDisableVertexAttribArray,         (native::Uint index))                                                                     \
    X(void,                  glVertexAttribPointer,              (native::Uint index, native::Int size, native::Enum type, native::Boolean norm, native::Sizei stride, const void* offset)) \
    X(void,                  glVertexAttribIPointer,             (native::Uint index, native::Int size, native::Enum type, native::Sizei stride, const void* offset)) \
    X(void,                  glVertexAttribDivisor,              (native::Uint index, native::Uint divisor))                                               \
    X(void,                  glDrawArrays,                       (native::Enum mode, native::Int first, native::Sizei count))                              \
    X(void,                  glDrawArraysInstanced,              (native::Enum mode, native::Int first, native::Sizei count, native::Sizei instances))     \
    X(void,                  glDrawElementsBaseVertex,           (native::Enum mode, native::Sizei count, native::Enum type, const void* offset, native::Int base)) \
    X(void,                  glDrawElementsInstancedBaseVertex,  (native::Enum mode, native::Sizei count, native::Enum type, const void* offset, native::Sizei instances, native::Int base)) \
    X(native::Uint,          glCreateShader,                     (native::Enum type))                                                                      \
    X(void,                  glShaderSource,                     (native::Uint shader, native::Sizei count, const native::Char* const* strings, const native::Int* lengths)) \
    X(void,                  glCompileShader,                    (native::Uint shader))                                                                    \
    X(void,                  glGetShaderiv,                      (native::Uint shader, native::Enum pname, native::Int* out))                              \
    X(void,                  glGetShaderInfoLog,                 (native::Uint shader, native::Sizei size, native::Sizei* length, native::Char* log))      \
    X(void,                  glDeleteShader,                     (native::Uint shader))                                                                    \
    X(native::Uint,          glCreateProgram,                    (void))                                                                                   \
    X(void,                  glAttachShader,                     (native::Uint program, native::Uint shader))                                              \
    X(void,                  glDetachShader,                     (native::Uint program, native::Uint shader))                                              \
    X(void,                  glLinkProgram,                      (native::Uint program))                                                                   \
    X(void,                  glGetProgramiv,                     (native::Uint program, native::Enum pname, native::Int* out))                             \
    X(void,                  glGetProgramInfoLog,                (native::Uint program, native::Sizei size, native::Sizei* length, native::Char* log))     \
    X(void,                  glDeleteProgram,                    (native::Uint program))                                                                   \
    X(void,                  glUseProgram,                       (native::Uint program))                                                                   \
    X(native::Int,           glGetUniformLocation,               (native::Uint program, const native::Char* name))                                         \
    X(void,                  glUniform1i,                        (native::Int location, native::Int value))                                                \
    X(native::Uint,          glGetUniformBlockIndex,             (native::Uint program, const native::Char* name))                                         \
    X(void,                  glUniformBlockBinding,              (native::Uint program, native::Uint block, native::Uint binding))                        \
    X(void,                  glGenFramebuffers,                  (native::Sizei n, native::Uint* out))                                                     \
    X(void,                  glDeleteFramebuffers,               (native::Sizei n, const native::Uint* ids))                                               \
    X(void,                  glBindFramebuffer,                  (native::Enum target, native::Uint id))                                                   \
    X(void,                  glFramebufferTexture2D,             (native::Enum target, native::Enum attachment, native::Enum textarget, native::Uint tex, native::Int level)) \
    X(void,                  glFramebufferTextureLayer,          (native::Enum target, native::Enum attachment, native::Uint tex, native::Int level, native::Int layer)) \
    X(void,                  glFramebufferRenderbuffer,          (native::Enum target, native::Enum attachment, native::Enum rbtarget, native::Uint rb))   \
    X(native::Enum,          glCheckFramebufferStatus,           (native::Enum target))                                                                    \
    X(void,                  glBlitFramebuffer,                  (native::Int sx0, native::Int sy0, native::Int sx1, native::Int sy1, native::Int dx0, native::Int dy0, native::Int dx1, native::Int dy1, native::Bitfield mask, native::Enum filter)) \
    X(void,                  glDrawBuffers,                      (native::Sizei n, const native::Enum* bufs))                                              \
    X(void,                  glDrawBuffer,                       (native::Enum buf))                                                                       \
    X(void,                  glReadBuffer,                       (native::Enum buf))                                                                       \
    X(void,                  glGenRenderbuffers,                 (native::Sizei n, native::Uint* out))                                                     \
    X(void,                  glDeleteRenderbuffers,              (native::Sizei n, const native::Uint* ids))                                               \
    X(void,                  glBindRenderbuffer,                 (native::Enum target, native::Uint id))                                                   \
    X(void,                  glRenderbufferStorageMultisample,   (native::Enum target, native::Sizei samples, native::Enum internal, native::Sizei w, native::Sizei h)) \
    X(void,                  glGenQueries,                       (native::Sizei n, native::Uint* out))                                                     \
    X(void,                  glDeleteQueries,                    (native::Sizei n, const native::Uint* ids))                                               \
    X(void,                  glQueryCounter,                     (native::Uint id, native::Enum target))                                                   \
    X(void,                  glGetQueryObjectiv,                 (native::Uint id, native::Enum pname, native::Int* out))                                  \
    X(void,                  glGetQueryObjectui64v,              (native::Uint id, native::Enum pname, native::Uint64* out))                               \
    X(native::Sync,          glFenceSync,                        (native::Enum condition, native::Bitfield flags))                                         \
    X(native::Enum,          glClientWaitSync,                   (native::Sync sync, native::Bitfield flags, native::Uint64 timeout))                      \
    X(void,                  glDeleteSync,                       (native::Sync sync))

#define FIZMO_GL_OPTIONAL_FUNCTIONS(X)                                                                                                                     \
    X(void,                  glClipControl,                      (native::Enum origin, native::Enum depth))                                                \
    X(void,                  glDrawElementsInstancedBaseVertexBaseInstance, (native::Enum mode, native::Sizei count, native::Enum type, const void* offset, native::Sizei instances, native::Int base, native::Uint base_instance)) \
    X(void,                  glDrawArraysInstancedBaseInstance,  (native::Enum mode, native::Int first, native::Sizei count, native::Sizei instances, native::Uint base_instance)) \
    X(void,                  glMultiDrawElementsIndirect,        (native::Enum mode, native::Enum type, const void* offset, native::Sizei count, native::Sizei stride)) \
    X(void,                  glDispatchCompute,                  (native::Uint x, native::Uint y, native::Uint z))                                         \
    X(void,                  glMemoryBarrier,                    (native::Bitfield barriers))                                                              \
    X(void,                  glBindImageTexture,                 (native::Uint unit, native::Uint tex, native::Int level, native::Boolean layered, native::Int layer, native::Enum access, native::Enum format)) \
    X(void,                  glPushDebugGroup,                   (native::Enum source, native::Uint id, native::Sizei length, const native::Char* message)) \
    X(void,                  glPopDebugGroup,                    (void))                                                                                   \
    X(void,                  glObjectLabel,                      (native::Enum identifier, native::Uint name, native::Sizei length, const native::Char* label)) \
    X(void,                  glCopyImageSubData,                 (native::Uint src, native::Enum st, native::Int sl, native::Int sx, native::Int sy, native::Int sz, native::Uint dst, native::Enum dt, native::Int dl, native::Int dx, native::Int dy, native::Int dz, native::Sizei w, native::Sizei h, native::Sizei d)) \
    X(native::Uint,          glGetProgramResourceIndex,          (native::Uint program, native::Enum interface_type, const native::Char* name))           \
    X(void,                  glShaderStorageBlockBinding,        (native::Uint program, native::Uint block, native::Uint binding))                         \
    X(void,                  glDebugMessageCallback,             (native::DebugProc callback, const void* user))

#define FIZMO_GL_FUNCTIONS(X)       \
    FIZMO_GL_CORE_FUNCTIONS(X)      \
    FIZMO_GL_OPTIONAL_FUNCTIONS(X)

namespace fizmo {
namespace opengl {

using GetProcAddress = void* (*)(const char* name);

class Library {
private:
    native::LibraryHandle m_handle = nullptr;
    void*                 m_loader = nullptr;

public:
    Library() noexcept = default;
    ~Library() noexcept { unload(); }

    Library(const Library&) = delete;
    Library& operator=(const Library&) = delete;

    Library(Library&& o) noexcept : m_handle(std::exchange(o.m_handle, nullptr)), m_loader(std::exchange(o.m_loader, nullptr)) {}

    Library& operator=(Library&& o) noexcept {
        if (this != &o) {
            unload();
            m_handle = std::exchange(o.m_handle, nullptr);
            m_loader = std::exchange(o.m_loader, nullptr);
        }
        return *this;
    }

    bool load() noexcept {
        if (m_handle) return true;

    #if defined(OS_WINDOWS)
        m_handle = LoadLibraryA("opengl32.dll");
        if (!m_handle) return false;
        m_loader = reinterpret_cast<void*>(::GetProcAddress(m_handle, "wglGetProcAddress"));
    #else
        m_handle = dlopen("libGL.so.1", RTLD_NOW | RTLD_GLOBAL | RTLD_NODELETE);
        if (!m_handle) m_handle = dlopen("libGL.so", RTLD_NOW | RTLD_GLOBAL | RTLD_NODELETE);
        if (!m_handle) return false;
        m_loader = dlsym(m_handle, "glXGetProcAddressARB");
        if (!m_loader) m_loader = dlsym(m_handle, "glXGetProcAddress");
    #endif

        if (!m_loader) { unload(); return false; }
        return true;
    }

    void unload() noexcept {
        m_loader = nullptr;
        if (!m_handle) return;
    #if defined(OS_WINDOWS)
        FreeLibrary(m_handle);
    #else
        dlclose(m_handle);
    #endif
        m_handle = nullptr;
    }

    bool is_loaded() const noexcept { return m_handle != nullptr; }

    void* symbol(const char* name) const noexcept {
        if (!m_handle || !name) return nullptr;
    #if defined(OS_WINDOWS)
        return reinterpret_cast<void*>(::GetProcAddress(m_handle, name));
    #else
        return dlsym(m_handle, name);
    #endif
    }

    void* proc(const char* name) const noexcept {
        if (!m_loader || !name) return nullptr;

    #if defined(OS_WINDOWS)
        using Fn = PROC (WINAPI*)(LPCSTR);
        void* p = reinterpret_cast<void*>(reinterpret_cast<Fn>(m_loader)(name));
        const std::intptr_t v = reinterpret_cast<std::intptr_t>(p);
        if (v == 0 || v == 1 || v == 2 || v == 3 || v == -1) p = symbol(name);
        return p;
    #else
        using Fn = void* (*)(const unsigned char*);
        return reinterpret_cast<Fn>(m_loader)(reinterpret_cast<const unsigned char*>(name));
    #endif
    }
};

#define FIZMO_GL_DECLARE_PFN(ret, name, params) using PFN_##name = ret (FIZMO_GL_APIENTRY*) params; PFN_##name name = nullptr;

struct Functions {
    FIZMO_GL_FUNCTIONS(FIZMO_GL_DECLARE_PFN)

    bool load(const Library& lib) noexcept {
        #define FIZMO_GL_LOAD(ret, name, params) name = reinterpret_cast<PFN_##name>(lib.proc(#name));
        FIZMO_GL_FUNCTIONS(FIZMO_GL_LOAD)
        #undef FIZMO_GL_LOAD
        return has_core();
    }

    bool has_core() const noexcept {
        bool ok = true;
        #define FIZMO_GL_CHECK(ret, name, params) ok = ok && name != nullptr;
        FIZMO_GL_CORE_FUNCTIONS(FIZMO_GL_CHECK)
        #undef FIZMO_GL_CHECK
        return ok;
    }
};

#undef FIZMO_GL_DECLARE_PFN

} // namespace opengl
} // namespace fizmo

#endif // FIZMO_OPENGL_LOADER_HPP
