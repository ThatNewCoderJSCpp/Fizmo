#ifndef FIZMO_OPENGL_CONTEXT_HPP
#define FIZMO_OPENGL_CONTEXT_HPP

#include "loader.hpp"
#include "types.hpp"
#include <algorithm>
#include <cstring>
#include <memory>
#include <string>
#include <unordered_set>

namespace fizmo {
namespace opengl {

struct Caps {
    std::uint32_t version            = 0;
    int           major              = 0;
    int           minor              = 0;
    bool          clip_control       = false;
    bool          base_instance      = false;
    bool          multi_draw         = false;
    bool          compute            = false;
    bool          debug              = false;
    bool          seamless_cube      = false;
    bool          memory_nvx         = false;
    bool          memory_ati         = false;
    bool          timer_query        = false;
    int           texture_units      = 16;
    int           combined_units     = 48;
    int           max_samples        = 1;
    int           max_texture_size   = 4096;
    std::uint64_t uniform_alignment  = 256;
    std::uint64_t max_uniform_block  = 16384;
    std::string   vendor;
    std::string   renderer;
    std::string   version_string;

    bool at_least(int ma, int mi) const noexcept { return major > ma || (major == ma && minor >= mi); }
};

struct ContextDesc {
    int  min_major = 3;
    int  min_minor = 3;
    int  max_major = 4;
    int  max_minor = 6;
    bool debug     = false;
    bool vsync     = true;
};

namespace detail {

#if defined(OS_LINUX)

using GLXFBConfig  = struct __GLXFBConfigRec*;
using GLXContext   = struct __GLXcontextRec*;
using GLXDrawable  = ::XID;

struct GlxFunctions {
    int          (*QueryVersion)(::Display*, int*, int*)                                       = nullptr;
    GLXFBConfig* (*ChooseFBConfig)(::Display*, int, const int*, int*)                          = nullptr;
    int          (*GetFBConfigAttrib)(::Display*, GLXFBConfig, int, int*)                      = nullptr;
    GLXContext   (*CreateNewContext)(::Display*, GLXFBConfig, int, GLXContext, int)            = nullptr;
    int          (*MakeContextCurrent)(::Display*, GLXDrawable, GLXDrawable, GLXContext)       = nullptr;
    void         (*SwapBuffers)(::Display*, GLXDrawable)                                       = nullptr;
    void         (*DestroyContext)(::Display*, GLXContext)                                     = nullptr;
    const char*  (*QueryExtensionsString)(::Display*, int)                                     = nullptr;
    GLXContext   (*GetCurrentContext)()                                                        = nullptr;
    GLXContext   (*CreateContextAttribsARB)(::Display*, GLXFBConfig, GLXContext, int, const int*) = nullptr;
    void         (*SwapIntervalEXT)(::Display*, GLXDrawable, int)                              = nullptr;
    int          (*SwapIntervalMESA)(unsigned int)                                             = nullptr;
    int          (*SwapIntervalSGI)(int)                                                       = nullptr;

    template <typename F>
    static void bind(F& out, void* p) noexcept { out = reinterpret_cast<F>(p); }

    bool load(const Library& lib) noexcept {
        bind(QueryVersion,          lib.symbol("glXQueryVersion"));
        bind(ChooseFBConfig,        lib.symbol("glXChooseFBConfig"));
        bind(GetFBConfigAttrib,     lib.symbol("glXGetFBConfigAttrib"));
        bind(CreateNewContext,      lib.symbol("glXCreateNewContext"));
        bind(MakeContextCurrent,    lib.symbol("glXMakeContextCurrent"));
        bind(SwapBuffers,           lib.symbol("glXSwapBuffers"));
        bind(DestroyContext,        lib.symbol("glXDestroyContext"));
        bind(QueryExtensionsString, lib.symbol("glXQueryExtensionsString"));
        bind(GetCurrentContext,     lib.symbol("glXGetCurrentContext"));
        return QueryVersion && ChooseFBConfig && GetFBConfigAttrib && MakeContextCurrent && SwapBuffers && DestroyContext && GetCurrentContext;
    }

    void load_extensions(const Library& lib, const char* extensions) noexcept {
        auto has = [extensions](const char* name) { return extensions && std::strstr(extensions, name) != nullptr; };
        if (has("GLX_ARB_create_context")) bind(CreateContextAttribsARB, lib.proc("glXCreateContextAttribsARB"));
        if (has("GLX_EXT_swap_control"))   bind(SwapIntervalEXT, lib.proc("glXSwapIntervalEXT"));
        if (has("GLX_MESA_swap_control"))  bind(SwapIntervalMESA, lib.proc("glXSwapIntervalMESA"));
        if (has("GLX_SGI_swap_control"))   bind(SwapIntervalSGI, lib.proc("glXSwapIntervalSGI"));
    }
};

inline bool& x_error_flag() noexcept {
    static bool flag = false;
    return flag;
}

inline int on_x_error(::Display*, ::XErrorEvent*) {
    x_error_flag() = true;
    return 0;
}

#elif defined(OS_WINDOWS)

struct WglFunctions {
    HGLRC       (WINAPI* CreateContext)(HDC)                                                = nullptr;
    BOOL        (WINAPI* DeleteContext)(HGLRC)                                              = nullptr;
    BOOL        (WINAPI* MakeCurrent)(HDC, HGLRC)                                           = nullptr;
    HGLRC       (WINAPI* GetCurrentContext)()                                               = nullptr;
    HGLRC       (WINAPI* CreateContextAttribsARB)(HDC, HGLRC, const int*)                   = nullptr;
    BOOL        (WINAPI* ChoosePixelFormatARB)(HDC, const int*, const FLOAT*, UINT, int*, UINT*) = nullptr;
    BOOL        (WINAPI* SwapIntervalEXT)(int)                                              = nullptr;
    const char* (WINAPI* GetExtensionsStringARB)(HDC)                                       = nullptr;

    template <typename F>
    static void bind(F& out, void* p) noexcept { out = reinterpret_cast<F>(p); }

    bool load(const Library& lib) noexcept {
        bind(CreateContext,     lib.symbol("wglCreateContext"));
        bind(DeleteContext,     lib.symbol("wglDeleteContext"));
        bind(MakeCurrent,       lib.symbol("wglMakeCurrent"));
        bind(GetCurrentContext, lib.symbol("wglGetCurrentContext"));
        return CreateContext && DeleteContext && MakeCurrent && GetCurrentContext;
    }

    void load_extensions(const Library& lib) noexcept {
        bind(CreateContextAttribsARB, lib.proc("wglCreateContextAttribsARB"));
        bind(ChoosePixelFormatARB,    lib.proc("wglChoosePixelFormatARB"));
        bind(SwapIntervalEXT,         lib.proc("wglSwapIntervalEXT"));
        bind(GetExtensionsStringARB,  lib.proc("wglGetExtensionsStringARB"));
    }
};

#endif

} // namespace detail

struct ContextState {
    Library                         lib;
    Functions                       fn;
    Caps                            caps;
    bool                            alive = false;
    std::unordered_set<std::string> extensions;

#if defined(OS_LINUX)
    detail::GlxFunctions glx;
    ::Display*           display = nullptr;
    ::XID                window  = 0;
    detail::GLXContext   context = nullptr;
#elif defined(OS_WINDOWS)
    detail::WglFunctions wgl;
    HWND                 hwnd    = nullptr;
    HDC                  hdc     = nullptr;
    HGLRC                context = nullptr;
#endif

    bool has_extension(const char* name) const { return extensions.count(name) != 0; }

    static const ContextState*& current() noexcept {
        static thread_local const ContextState* state = nullptr;
        return state;
    }

    bool make_current() const noexcept {
        if (!alive) return false;
        if (current() == this) return true;
    #if defined(OS_LINUX)
        if (!glx.MakeContextCurrent(display, window, window, context)) return false;
    #elif defined(OS_WINDOWS)
        if (!wgl.MakeCurrent(hdc, context)) return false;
    #endif
        current() = this;
        return true;
    }
};

class Context {
private:
    std::shared_ptr<ContextState> m_state;

    static constexpr int kVersions[][2] = { { 4, 6 }, { 4, 5 }, { 4, 4 }, { 4, 3 }, { 4, 2 }, { 4, 1 }, { 4, 0 }, { 3, 3 } };

    static bool in_range(int ma, int mi, const ContextDesc& d) noexcept {
        const int v = ma * 10 + mi;
        return v >= d.min_major * 10 + d.min_minor && v <= d.max_major * 10 + d.max_minor;
    }

    void query_caps() {
        ContextState& s = *m_state;
        const Functions& fn = s.fn;
        Caps& c = s.caps;
        fn.glGetIntegerv(gl::MAJOR_VERSION, &c.major);
        fn.glGetIntegerv(gl::MINOR_VERSION, &c.minor);
        c.version = make_version(static_cast<std::uint32_t>(c.major), static_cast<std::uint32_t>(c.minor));
        auto str = [&fn](native::Enum e) { const native::Ubyte* p = fn.glGetString(e); return p ? std::string(reinterpret_cast<const char*>(p)) : std::string(); };
        c.vendor = str(gl::VENDOR);
        c.renderer = str(gl::RENDERER);
        c.version_string = str(gl::VERSION);
        native::Int count = 0;
        fn.glGetIntegerv(gl::NUM_EXTENSIONS, &count);

        for (native::Int i = 0; i < count; ++i) {
            const native::Ubyte* e = fn.glGetStringi(gl::EXTENSIONS, static_cast<native::Uint>(i));
            if (e) s.extensions.emplace(reinterpret_cast<const char*>(e));
        }

        auto ext = [&s](const char* n) { return s.has_extension(n); };
        c.clip_control  = (c.at_least(4, 5) || ext("GL_ARB_clip_control")) && fn.glClipControl;
        c.base_instance = (c.at_least(4, 2) || ext("GL_ARB_base_instance")) && fn.glDrawElementsInstancedBaseVertexBaseInstance && fn.glDrawArraysInstancedBaseInstance;
        c.multi_draw    = c.base_instance && (c.at_least(4, 3) || ext("GL_ARB_multi_draw_indirect")) && fn.glMultiDrawElementsIndirect;
        c.compute       = c.at_least(4, 3) && fn.glDispatchCompute && fn.glMemoryBarrier && fn.glBindImageTexture;
        c.debug         = (c.at_least(4, 3) || ext("GL_KHR_debug")) && fn.glPushDebugGroup && fn.glPopDebugGroup && fn.glObjectLabel;
        c.seamless_cube = c.at_least(3, 2);
        c.memory_nvx    = ext("GL_NVX_gpu_memory_info");
        c.memory_ati    = ext("GL_ATI_meminfo");
        c.timer_query   = c.at_least(3, 3) || ext("GL_ARB_timer_query");
        native::Int v = 0;
        fn.glGetIntegerv(gl::MAX_TEXTURE_IMAGE_UNITS, &v);          c.texture_units = v;
        fn.glGetIntegerv(gl::MAX_COMBINED_TEXTURE_IMAGE_UNITS, &v); c.combined_units = v;
        fn.glGetIntegerv(gl::MAX_SAMPLES, &v);                      c.max_samples = std::max(v, 1);
        fn.glGetIntegerv(gl::MAX_TEXTURE_SIZE, &v);                 c.max_texture_size = v;
        fn.glGetIntegerv(gl::UNIFORM_BUFFER_OFFSET_ALIGNMENT, &v);  c.uniform_alignment = static_cast<std::uint64_t>(std::max(v, 1));
        fn.glGetIntegerv(gl::MAX_UNIFORM_BLOCK_SIZE, &v);           c.max_uniform_block = static_cast<std::uint64_t>(std::max(v, 16384));
        fn.glGetError();
    }

#if defined(OS_LINUX)
    Result create_platform(void* native_window, const ContextDesc& desc) {
        ContextState& s = *m_state;
        const auto* h = static_cast<const x11::Handle*>(native_window);
        if (!h->display || !h->window) return Result::InvalidArgument;
        if (!s.glx.load(s.lib)) return Result::MissingEntryPoint;
        s.display = h->display;
        s.window  = h->window;
        int glx_major = 0, glx_minor = 0;
        if (!s.glx.QueryVersion(s.display, &glx_major, &glx_minor) || glx_major * 10 + glx_minor < 13) return Result::Unsupported;
        ::XWindowAttributes wa{};
        if (!XGetWindowAttributes(s.display, s.window, &wa) || !wa.visual) return Result::InvalidArgument;
        const int screen = XScreenNumberOfScreen(wa.screen);
        const ::VisualID want = XVisualIDFromVisual(wa.visual);
        s.glx.load_extensions(s.lib, s.glx.QueryExtensionsString ? s.glx.QueryExtensionsString(s.display, screen) : nullptr);

        const int attribs[] = {
            glx::X_RENDERABLE, 1, glx::DRAWABLE_TYPE, glx::WINDOW_BIT, glx::RENDER_TYPE, glx::RGBA_BIT,
            glx::DOUBLEBUFFER, 1, glx::RED_SIZE, 8, glx::GREEN_SIZE, 8, glx::BLUE_SIZE, 8, 0
        };

        int count = 0;
        detail::GLXFBConfig* configs = s.glx.ChooseFBConfig(s.display, screen, attribs, &count);
        if (!configs || count <= 0) return Result::NoSuitableConfig;
        detail::GLXFBConfig best = nullptr;
        int best_score = 1 << 30;

        for (int i = 0; i < count; ++i) {
            int id = 0, depth = 0, stencil = 0, samples = 0;
            s.glx.GetFBConfigAttrib(s.display, configs[i], glx::VISUAL_ID, &id);
            if (static_cast<::VisualID>(id) != want) continue;
            s.glx.GetFBConfigAttrib(s.display, configs[i], glx::DEPTH_SIZE, &depth);
            s.glx.GetFBConfigAttrib(s.display, configs[i], glx::STENCIL_SIZE, &stencil);
            s.glx.GetFBConfigAttrib(s.display, configs[i], glx::SAMPLES, &samples);
            const int score = depth + stencil + samples * 64;
            if (score < best_score) { best = configs[i]; best_score = score; }
        }

        XFree(configs);
        if (!best) return Result::NoSuitableConfig;
        if (!s.glx.CreateContextAttribsARB) return Result::Unsupported;
        auto old_handler = XSetErrorHandler(&detail::on_x_error);

        for (const auto& v : kVersions) {
            if (!in_range(v[0], v[1], desc)) continue;
            const int ctx_attribs[] = {
                glx::CONTEXT_MAJOR_VERSION_ARB, v[0], glx::CONTEXT_MINOR_VERSION_ARB, v[1],
                glx::CONTEXT_PROFILE_MASK_ARB, glx::CONTEXT_CORE_PROFILE_BIT_ARB,
                glx::CONTEXT_FLAGS_ARB, desc.debug ? glx::CONTEXT_DEBUG_BIT_ARB : 0, 0
            };

            detail::x_error_flag() = false;
            detail::GLXContext ctx = s.glx.CreateContextAttribsARB(s.display, best, nullptr, 1, ctx_attribs);
            XSync(s.display, 0);

            if (ctx && !detail::x_error_flag()) { s.context = ctx; break; }
            if (ctx) s.glx.DestroyContext(s.display, ctx);
        }

        XSync(s.display, 0);
        XSetErrorHandler(old_handler);
        if (!s.context) return Result::ContextCreationFailed;
        if (!s.glx.MakeContextCurrent(s.display, s.window, s.window, s.context)) return Result::ContextCreationFailed;
        return Result::Success;
    }

    void destroy_platform() noexcept {
        ContextState& s = *m_state;
        if (!s.display || !s.context) return;
        if (s.glx.GetCurrentContext && s.glx.GetCurrentContext() == s.context) s.glx.MakeContextCurrent(s.display, 0, 0, nullptr);
        s.glx.DestroyContext(s.display, s.context);
        s.context = nullptr;
    }

#elif defined(OS_WINDOWS)
    static LRESULT CALLBACK dummy_proc(HWND h, UINT m, WPARAM w, LPARAM l) { return DefWindowProcW(h, m, w, l); }

    static PIXELFORMATDESCRIPTOR basic_pfd() noexcept {
        PIXELFORMATDESCRIPTOR pfd{};
        pfd.nSize        = sizeof(pfd);
        pfd.nVersion     = 1;
        pfd.dwFlags      = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
        pfd.iPixelType   = PFD_TYPE_RGBA;
        pfd.cColorBits   = 32;
        pfd.cAlphaBits   = 8;
        pfd.iLayerType   = PFD_MAIN_PLANE;
        return pfd;
    }

    bool load_wgl_extensions() {
        ContextState& s = *m_state;
        HINSTANCE inst = GetModuleHandleW(nullptr);
        WNDCLASSEXW wc{};
        wc.cbSize        = sizeof(wc);
        wc.style         = CS_OWNDC;
        wc.lpfnWndProc   = &Context::dummy_proc;
        wc.hInstance     = inst;
        wc.lpszClassName = L"fizmo_gl_probe";
        RegisterClassExW(&wc);
        HWND probe = CreateWindowExW(0, L"fizmo_gl_probe", L"", WS_OVERLAPPEDWINDOW, 0, 0, 1, 1, nullptr, nullptr, inst, nullptr);
        if (!probe) return false;
        HDC dc = GetDC(probe);
        PIXELFORMATDESCRIPTOR pfd = basic_pfd();
        const int format = ChoosePixelFormat(dc, &pfd);
        bool ok = false;

        if (format && SetPixelFormat(dc, format, &pfd)) {
            HGLRC legacy = s.wgl.CreateContext(dc);

            if (legacy && s.wgl.MakeCurrent(dc, legacy)) {
                s.wgl.load_extensions(s.lib);
                ok = s.wgl.CreateContextAttribsARB != nullptr;
                s.wgl.MakeCurrent(nullptr, nullptr);
            }

            if (legacy) s.wgl.DeleteContext(legacy);
        }

        ReleaseDC(probe, dc);
        DestroyWindow(probe);
        UnregisterClassW(L"fizmo_gl_probe", inst);
        return ok;
    }

    Result create_platform(void* native_window, const ContextDesc& desc) {
        ContextState& s = *m_state;
        s.hwnd = static_cast<HWND>(native_window);
        if (!s.wgl.load(s.lib)) return Result::MissingEntryPoint;
        if (!load_wgl_extensions()) return Result::Unsupported;
        s.hdc = GetDC(s.hwnd);
        if (!s.hdc) return Result::InvalidArgument;

        if (GetPixelFormat(s.hdc) == 0) {
            int format = 0;
            UINT found = 0;

            if (s.wgl.ChoosePixelFormatARB) {
                const int attribs[] = {
                    wgl::DRAW_TO_WINDOW_ARB, 1, wgl::SUPPORT_OPENGL_ARB, 1, wgl::DOUBLE_BUFFER_ARB, 1,
                    wgl::PIXEL_TYPE_ARB, wgl::TYPE_RGBA_ARB, wgl::ACCELERATION_ARB, wgl::FULL_ACCELERATION_ARB,
                    wgl::COLOR_BITS_ARB, 24, wgl::ALPHA_BITS_ARB, 8, wgl::DEPTH_BITS_ARB, 0, wgl::STENCIL_BITS_ARB, 0, 0
                };

                if (!s.wgl.ChoosePixelFormatARB(s.hdc, attribs, nullptr, 1, &format, &found) || found == 0) format = 0;
            }

            PIXELFORMATDESCRIPTOR pfd = basic_pfd();
            if (!format) format = ChoosePixelFormat(s.hdc, &pfd);
            if (!format) return Result::NoSuitableConfig;
            DescribePixelFormat(s.hdc, format, sizeof(pfd), &pfd);
            if (!SetPixelFormat(s.hdc, format, &pfd)) return Result::NoSuitableConfig;
        }

        for (const auto& v : kVersions) {
            if (!in_range(v[0], v[1], desc)) continue;
            const int ctx_attribs[] = {
                wgl::CONTEXT_MAJOR_VERSION_ARB, v[0], wgl::CONTEXT_MINOR_VERSION_ARB, v[1],
                wgl::CONTEXT_PROFILE_MASK_ARB, wgl::CONTEXT_CORE_PROFILE_BIT_ARB,
                wgl::CONTEXT_FLAGS_ARB, desc.debug ? wgl::CONTEXT_DEBUG_BIT_ARB : 0, 0
            };

            s.context = s.wgl.CreateContextAttribsARB(s.hdc, nullptr, ctx_attribs);
            if (s.context) break;
        }

        if (!s.context) return Result::ContextCreationFailed;
        if (!s.wgl.MakeCurrent(s.hdc, s.context)) return Result::ContextCreationFailed;
        s.wgl.load_extensions(s.lib);
        return Result::Success;
    }

    void destroy_platform() noexcept {
        ContextState& s = *m_state;

        if (s.context) {
            if (s.wgl.GetCurrentContext() == s.context) s.wgl.MakeCurrent(nullptr, nullptr);
            s.wgl.DeleteContext(s.context);
            s.context = nullptr;
        }

        if (s.hdc) { ReleaseDC(s.hwnd, s.hdc); s.hdc = nullptr; }
    }
#endif

public:
    Context() noexcept = default;
    ~Context() noexcept { destroy(); }

    Context(const Context&) = delete;
    Context& operator=(const Context&) = delete;
    Context(Context&&) noexcept = default;
    Context& operator=(Context&& o) noexcept {
        if (this != &o) { destroy(); m_state = std::move(o.m_state); }
        return *this;
    }

    Result create(void* native_window, const ContextDesc& desc = {}) {
        destroy();
        if (!native_window) return Result::InvalidArgument;
        m_state = std::make_shared<ContextState>();
        if (!m_state->lib.load()) { m_state.reset(); return Result::LibraryNotFound; }
        Result r = Result::Unsupported;

    #if defined(OS_LINUX) || defined(OS_WINDOWS)
        try { r = create_platform(native_window, desc); } catch (...) { r = Result::ContextCreationFailed; }
    #endif

        if (failed(r)) { destroy_platform(); m_state.reset(); return r; }
        m_state->alive = true;
        ContextState::current() = m_state.get();

        if (!m_state->fn.load(m_state->lib)) { destroy(); return Result::MissingEntryPoint; }

        try { query_caps(); } catch (...) { destroy(); return Result::OutOfMemory; }

        if (!m_state->caps.at_least(desc.min_major, desc.min_minor)) { destroy(); return Result::VersionTooLow; }
        set_swap_interval(desc.vsync ? 1 : 0);
        return Result::Success;
    }

    void destroy() noexcept {
        if (!m_state) return;
        if (ContextState::current() == m_state.get()) ContextState::current() = nullptr;
        m_state->alive = false;
    #if defined(OS_LINUX) || defined(OS_WINDOWS)
        destroy_platform();
    #endif
        m_state.reset();
    }

    bool valid() const noexcept { return m_state && m_state->alive; }
    bool make_current() const noexcept { return m_state && m_state->make_current(); }

    void swap_buffers() const noexcept {
        if (!valid()) return;
    #if defined(OS_LINUX)
        m_state->glx.SwapBuffers(m_state->display, m_state->window);
    #elif defined(OS_WINDOWS)
        SwapBuffers(m_state->hdc);
    #endif
    }

    bool set_swap_interval(int interval) const noexcept {
        if (!valid() || !make_current()) return false;
        const ContextState& s = *m_state;
    #if defined(OS_LINUX)
        if (s.glx.SwapIntervalEXT) { s.glx.SwapIntervalEXT(s.display, s.window, interval); return true; }
        if (s.glx.SwapIntervalMESA) return s.glx.SwapIntervalMESA(static_cast<unsigned int>(interval)) == 0;
        if (s.glx.SwapIntervalSGI && interval > 0) return s.glx.SwapIntervalSGI(interval) == 0;
    #elif defined(OS_WINDOWS)
        if (s.wgl.SwapIntervalEXT) return s.wgl.SwapIntervalEXT(interval) != 0;
    #endif
        return false;
    }

    const std::shared_ptr<ContextState>& state() const noexcept { return m_state; }
    const Functions& fn() const noexcept { return m_state->fn; }
    const Caps& caps() const noexcept { return m_state->caps; }
};

} // namespace opengl
} // namespace fizmo

#endif // FIZMO_OPENGL_CONTEXT_HPP
