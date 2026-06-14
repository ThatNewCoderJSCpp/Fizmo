#ifndef FIZMO_RENDERER_HPP
#define FIZMO_RENDERER_HPP

#include "../Graphics/color.hpp"
#include "window.hpp"
#include "../Graphics/canvas.hpp"

#ifdef OS_WINDOWS
#include "Renderer Impl/renderer_windows_impl.hpp"
#endif

namespace fizmo {
namespace windows {

class Renderer {
private:
    Window* m_window;
    std::unique_ptr<detail::RendererImplBase> m_impl;

public:
    explicit Renderer(Window& window) noexcept : m_window(&window), m_impl(nullptr) {
    #ifdef OS_WINDOWS
        m_impl = std::make_unique<detail::RendererImpl>();
    #endif
    }

    ~Renderer() { unbind(); }
    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;
    Renderer(Renderer&&) = default;
    Renderer& operator=(Renderer&&) = default;

    bool bind() noexcept {
        if (!m_impl || !m_window) return false;
        void* handle = m_window->native_handle();
        if (!handle) return false;
        if (!m_impl->initialize(handle, m_window->width(), m_window->height())) { return false; }
        m_window->set_paint_callback([this](void* dc) { m_impl->paint(dc); });
        m_window->add_event_listener(WindowEventType::WindowResize, [this](const WindowEvent& e) { m_impl->resize(e.x, e.y); });
        return true;
    }

    void unbind() noexcept {
        if (m_window) {
            m_window->set_paint_callback(nullptr);
            m_window->remove_event_listeners(WindowEventType::WindowResize);
        }

        if (m_impl) { m_impl->shutdown(); }
    }

    void begin_frame() noexcept { if (m_impl) m_impl->begin_frame(); }
    void present() noexcept { if (m_impl) m_impl->present(); }
    void clear(const graphics::Color& color = graphics::Color()) noexcept { if (m_impl) m_impl->clear(color); }

    void blit_framebuffer(const graphics::Framebuffer& fb) noexcept {
        const auto* px = fb.data();
        
        for (unsigned y = 0; y < fb.height(); ++y)
            for (unsigned x = 0; x < fb.width(); ++x)
                draw_pixel(x, y, px[y * fb.width() + x]);
    }

    void set_clip_rect(int x, int y, unsigned int w, unsigned int h, RectOrigin origin = RectOrigin::TopLeft) noexcept { if (m_impl) m_impl->set_clip_rect(x, y, w, h, origin); }
    void reset_clip_rect() noexcept { if (m_impl) m_impl->reset_clip_rect(); }

    void draw_pixel(int x, int y, const fizmo::graphics::Color& color) noexcept { if (m_impl) m_impl->draw_pixel(x, y, color); }
    void draw_line(int x1, int y1, int x2, int y2, const graphics::Paint& paint) noexcept { if (m_impl) m_impl->draw_line(x1, y1, x2, y2, paint); }
    void draw_rect(int x, int y, unsigned int w, unsigned int h, const graphics::Paint& paint) noexcept { if (m_impl) m_impl->draw_rect(x, y, w, h, paint); }

    void draw_rect_corners(int x1, int y1, int x2, int y2, const graphics::Paint& paint) noexcept {
        const ResolvedRect r = resolve_corners(x1, y1, x2, y2);
        if (m_impl) m_impl->draw_rect(r.x, r.y, r.w, r.h, paint);
    }

    void draw_circle(int cx, int cy, unsigned int radius, const graphics::Paint& paint) noexcept { if (m_impl) m_impl->draw_ellipse(cx, cy, radius, radius, paint); }
    void draw_ellipse(int cx, int cy, unsigned int rx, unsigned int ry, const graphics::Paint& paint) noexcept { if (m_impl) m_impl->draw_ellipse(cx, cy, rx, ry, paint); }

    void draw_image(const fizmo::images::BitmapImage& img, int dx, int dy) noexcept {
        if (m_impl) m_impl->draw_image(dx, dy, img.width(), img.height(), img, 0, 0, img.width(), img.height());
    }

    void draw_image(
        const fizmo::images::BitmapImage& img,
        int dx, int dy,
        unsigned int dw, unsigned int dh
    ) noexcept {
        if (m_impl) m_impl->draw_image(dx, dy, dw, dh, img, 0, 0, img.width(), img.height());
    }

    void draw_image(
        const fizmo::images::BitmapImage& img,
        int dx, int dy,
        unsigned int dw, unsigned int dh,
        unsigned int sx, unsigned int sy,
        unsigned int sw, unsigned int sh
    ) noexcept {
        if (m_impl) m_impl->draw_image(dx, dy, dw, dh, img, sx, sy, sw, sh);
    }

    void draw_texture(const graphics::Texture& tex, int dx, int dy, float opacity = 1.0f) noexcept {
        if (m_impl && tex.valid()) m_impl->draw_texture(dx, dy, tex.width(), tex.height(), tex, opacity, tex.full_rect());
    }

    void draw_texture(
        const graphics::Texture& tex,
        int dx, int dy,
        unsigned int dw, unsigned int dh,
        float opacity = 1.0f
    ) noexcept {
        if (m_impl && tex.valid()) m_impl->draw_texture(dx, dy, dw, dh, tex, opacity, tex.full_rect());
    }

    void draw_texture(
        const graphics::Texture& tex,
        int dx, int dy,
        unsigned int dw, unsigned int dh,
        float opacity,
        const graphics::TextureRect& src
    ) noexcept {
        if (m_impl && tex.valid()) m_impl->draw_texture(dx, dy, dw, dh, tex, opacity, src);
    }

    void draw_texture(
        const graphics::Texture& tex,
        int dx, int dy,
        unsigned int dw, unsigned int dh,
        const graphics::TextureRect& src
    ) noexcept {
        if (m_impl && tex.valid()) m_impl->draw_texture(dx, dy, dw, dh, tex, 1.0f, src);
    }

    void draw_text(int x, int y, const char* utf8, int len, const text::TextStyle& style) noexcept {
        if (m_impl) m_impl->draw_text(x, y, utf8, len, style);
    }

    void draw_text(int x, int y, const wchar_t* str, int len, const text::TextStyle& style) noexcept {
        if (m_impl) m_impl->draw_text(x, y, str, len, style);
    }

    void draw_text(int x, int y, const std::string& text, const text::TextStyle& style) noexcept {
        if (m_impl) m_impl->draw_text(x, y, text.c_str(), static_cast<int>(text.size()), style);
    }

    void draw_text(int x, int y, const std::wstring& text, const text::TextStyle& style) noexcept {
        if (m_impl) m_impl->draw_text(x, y, text.c_str(), static_cast<int>(text.size()), style);
    }

    text::TextMetrics measure_text(const char* utf8, int len, const text::TextStyle& style) noexcept {
        if (m_impl) return m_impl->measure_text(utf8, len, style);
        return {};
    }

    text::TextMetrics measure_text(const wchar_t* str, int len, const text::TextStyle& style) noexcept {
        if (m_impl) return m_impl->measure_text(str, len, style);
        return {};
    }
    
    text::TextMetrics measure_text(const std::string& text, const text::TextStyle& style) noexcept {
        if (m_impl) return m_impl->measure_text(text.c_str(), static_cast<int>(text.size()), style);
        return {};
    }

    text::TextMetrics measure_text(const std::wstring& text, const text::TextStyle& style) noexcept {
        if (m_impl) return m_impl->measure_text(text.c_str(), static_cast<int>(text.size()), style);
        return {};
    }

private:
    struct ResolvedRect { int x; int y; unsigned int w; unsigned int h; };

    static ResolvedRect resolve_corners(int x1, int y1, int x2, int y2) noexcept {
        int lx = (x1 < x2) ? x1 : x2;
        int ly = (y1 < y2) ? y1 : y2;
        unsigned int w = static_cast<unsigned int>((x1 < x2) ? (x2 - x1) : (x1 - x2));
        unsigned int h = static_cast<unsigned int>((y1 < y2) ? (y2 - y1) : (y1 - y2));
        return { lx, ly, w, h };
    }
};

#ifdef OS_WINDOWS
static void blit_framebuffer(void* hdc_raw, const graphics::Framebuffer& fb) {
    HDC hdc = static_cast<HDC>(hdc_raw);
    unsigned int w = fb.width(), h = fb.height();
    BITMAPINFO bmi = {};
    bmi.bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth       = static_cast<LONG>(w);
    bmi.bmiHeader.biHeight      = -static_cast<LONG>(h);
    bmi.bmiHeader.biPlanes      = 1;
    bmi.bmiHeader.biBitCount    = 32;
    bmi.bmiHeader.biCompression = BI_RGB;
    std::vector<std::uint8_t> bits(w * h * 4);
    const graphics::Color* px = fb.data();

    for (unsigned int i = 0; i < w * h; ++i) {
        bits[i * 4 + 0] = px[i].blue();
        bits[i * 4 + 1] = px[i].green();
        bits[i * 4 + 2] = px[i].red();
        bits[i * 4 + 3] = px[i].alpha();
    }

    SetDIBitsToDevice(
        hdc, 0, 0, w, h,
        0, 0, 0, h,
        bits.data(), &bmi, DIB_RGB_COLORS
    );
}
#endif // OS_WINDOWS

} // namespace windows
} // namespace fizmo

#endif // FIZMO_RENDERER_HPP