#ifndef FIZMO_RENDERER_BASE_HPP
#define FIZMO_RENDERER_BASE_HPP

#include "../../Graphics/color.hpp"
#include "../../Graphics/paint.hpp"
#include "../../Text/text_style.hpp"
#include <functional>
#include <string>

namespace fizmo {
namespace windows {
namespace detail {

class RendererImplBase {
public:
    virtual ~RendererImplBase() noexcept = default;
    virtual bool initialize(void* native_handle, unsigned int width, unsigned int height) noexcept = 0;
    virtual void shutdown() noexcept = 0;
    virtual void resize(unsigned int width, unsigned int height) noexcept = 0;
    virtual void begin_frame() noexcept = 0;
    virtual void present() noexcept = 0;
    virtual void clear(const graphics::Color& color) noexcept = 0;
    virtual void set_clip_rect(int x, int y, unsigned int w, unsigned int h, RectOrigin origin = RectOrigin::TopLeft) noexcept = 0;
    virtual void reset_clip_rect() noexcept = 0;
    virtual void draw_pixel(int x, int y, const graphics::Color& color) noexcept = 0;
    virtual void draw_line(int x1, int y1, int x2, int y2, const graphics::Paint& paint) noexcept = 0;
    virtual void paint(void* paint_dc) noexcept = 0;
    virtual void draw_rect(int x, int y, unsigned int w, unsigned int h, const graphics::Paint& paint) noexcept = 0;
    virtual void draw_ellipse(int cx, int cy, unsigned int rx, unsigned int ry, const graphics::Paint& paint) noexcept = 0;
    virtual void draw_arc(int cx, int cy, unsigned int rx, unsigned int ry, const graphics::Paint& p) noexcept = 0;
    virtual void draw_image(int dx, int dy, unsigned int dw, unsigned int dh, const images::BitmapImage& img, unsigned int sx, unsigned int sy, unsigned int sw, unsigned int sh) noexcept = 0;
    virtual void draw_texture(int dx, int dy, unsigned int dw, unsigned int dh, const graphics::Texture& tex, float opacity, const graphics::TextureRect& src) noexcept = 0;

    virtual void draw_text(
        int x, int y,
        const char* utf8, int len,
        const text::TextStyle& style
    ) noexcept = 0;

    virtual void draw_text(
        int x, int y,
        const wchar_t* str, int len,
        const text::TextStyle& style
    ) noexcept = 0;

    virtual text::TextMetrics measure_text(
        const char* utf8, int len,
        const text::TextStyle& style
    ) noexcept = 0;

    virtual text::TextMetrics measure_text(
        const wchar_t* str, int len,
        const text::TextStyle& style
    ) noexcept = 0;
};

} // namespace detail
} // namespace windows
} // namespace fizmo

#endif // FIZMO_RENDERER_BASE_HPP