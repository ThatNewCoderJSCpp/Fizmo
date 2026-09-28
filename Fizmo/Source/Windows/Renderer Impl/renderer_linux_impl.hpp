#ifndef FIZMO_RENDERER_LINUX_IMPL_HPP
#define FIZMO_RENDERER_LINUX_IMPL_HPP

#include "renderer_base.hpp"
#include "../../Basic/fizmo_defines.hpp"

#ifdef OS_LINUX

#include "../../x11_compat.hpp"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <cwchar>
#include <vector>

namespace fizmo {
namespace windows {
namespace detail {

class RendererImpl : public RendererImplBase {
    static_assert(sizeof(wchar_t) == 4, "Linux backend assumes UCS-4 wchar_t");

private:
    x11::XDisplay*    m_display  = nullptr;
    x11::XWindowId    m_window   = 0;
    x11::XPixmapId    m_back     = 0;
    x11::XGCHandle    m_gc       = nullptr;   
    x11::XGCHandle    m_blit_gc  = nullptr;   
    XftDraw*          m_xft      = nullptr;
    x11::XVisual*     m_visual   = nullptr;
    x11::XColormapId  m_colormap = 0;
    int               m_screen   = 0;
    unsigned int      m_depth    = 24;
    unsigned int      m_width    = 0;
    unsigned int      m_height   = 0;

    static unsigned long scale_channel(unsigned long mask, std::uint8_t v) noexcept {
        if (mask == 0) return 0;
        int shift = 0;
        unsigned long m = mask;
        while ((m & 1UL) == 0UL) { m >>= 1; ++shift; }
        return ((static_cast<unsigned long>(v) * m) / 255UL) << shift;
    }

    static std::uint8_t extract_channel(unsigned long mask, unsigned long px) noexcept {
        if (mask == 0) return 0;
        int shift = 0;
        unsigned long m = mask;
        while ((m & 1UL) == 0UL) { m >>= 1; ++shift; }
        return static_cast<std::uint8_t>((((px >> shift) & m) * 255UL) / m);
    }

    unsigned long pack(std::uint8_t r, std::uint8_t g, std::uint8_t b) const noexcept {
        if (!m_visual) return 0;
        return scale_channel(m_visual->red_mask,   r)
             | scale_channel(m_visual->green_mask, g)
             | scale_channel(m_visual->blue_mask,  b);
    }

    unsigned long to_pixel(const graphics::Color& c) const noexcept {
        return pack(c.red(), c.green(), c.blue());
    }

    void create_back_buffer(unsigned int w, unsigned int h) noexcept {
        if (w == 0) w = 1;
        if (h == 0) h = 1;
        m_back = XCreatePixmap(m_display, m_window, w, h, m_depth);
        if (!m_back) return;
        m_width  = w;
        m_height = h;
        m_xft = XftDrawCreate(m_display, m_back, m_visual, m_colormap);
        XSetForeground(m_display, m_gc, BlackPixel(m_display, m_screen));
        XFillRectangle(m_display, m_back, m_gc, 0, 0, w, h);
    }

    void destroy_back_buffer() noexcept {
        if (m_xft)  { XftDrawDestroy(m_xft); m_xft = nullptr; }
        if (m_back) { XFreePixmap(m_display, m_back); m_back = 0; }
    }

    static int x_cap(graphics::LineCap cap) noexcept {
        switch (cap) {
            case graphics::LineCap::Round:  return x11::kCapRound;
            case graphics::LineCap::Square: return x11::kCapProjecting;
            default:                        return x11::kCapButt;
        }
    }

    static int x_join(graphics::LineJoin join) noexcept {
        switch (join) {
            case graphics::LineJoin::Round: return x11::kJoinRound;
            case graphics::LineJoin::Bevel: return x11::kJoinBevel;
            default:                        return x11::kJoinMiter;
        }
    }

    void apply_stroke(const graphics::Paint& p) noexcept {
        XSetForeground(m_display, m_gc, to_pixel(p.stroke_color()));

        XSetLineAttributes(
            m_display, m_gc,
            static_cast<unsigned int>(p.stroke_width()),
            x11::kLineSolid, 
            x_cap(p.line_cap()), 
            x_join(p.line_join())
        );
    }

public:
    RendererImpl() noexcept = default;
    ~RendererImpl() noexcept override { shutdown(); }

    bool initialize(void* native_handle, unsigned int w, unsigned int h) noexcept override {
        if (!native_handle) return false;
        const auto* handle = static_cast<const x11::Handle*>(native_handle);
        m_display = handle->display;
        m_window  = handle->window;
        if (!m_display || !m_window) return false;
        m_screen = DefaultScreen(m_display);
        XWindowAttributes attrs;
        if (XGetWindowAttributes(m_display, m_window, &attrs) == 0) return false;
        m_visual   = attrs.visual;
        m_colormap = attrs.colormap;
        m_depth    = static_cast<unsigned int>(attrs.depth);
        m_gc      = XCreateGC(m_display, m_window, 0, nullptr);
        m_blit_gc = XCreateGC(m_display, m_window, 0, nullptr);
        if (!m_gc || !m_blit_gc) { shutdown(); return false; }
        XSetGraphicsExposures(m_display, m_blit_gc, 0);
        create_back_buffer(w, h);
        return m_back != 0;
    }

    void shutdown() noexcept override {
        if (!m_display) { m_window = 0; return; }
        destroy_back_buffer();
        if (m_gc)      { XFreeGC(m_display, m_gc);      m_gc = nullptr; }
        if (m_blit_gc) { XFreeGC(m_display, m_blit_gc); m_blit_gc = nullptr; }
        m_display = nullptr;   
        m_window  = 0;
    }

    void resize(unsigned int w, unsigned int h) noexcept override {
        if (!m_display) return;
        if (w == m_width && h == m_height) return;
        destroy_back_buffer();
        create_back_buffer(w, h);
    }

    void begin_frame() noexcept override {}

    void present() noexcept override {
        if (!m_display || !m_back || !m_window) return;
        XCopyArea(m_display, m_back, m_window, m_blit_gc, 0, 0, m_width, m_height, 0, 0);
        XFlush(m_display);
    }

    void paint(void* /*paint_dc*/) noexcept override {
        if (!m_display || !m_back || !m_window) return;
        XCopyArea(m_display, m_back, m_window, m_blit_gc, 0, 0, m_width, m_height, 0, 0);
    }

    void clear(const graphics::Color& color) noexcept override {
        if (!m_display || !m_back) return;
        XSetForeground(m_display, m_gc, to_pixel(color));
        XFillRectangle(m_display, m_back, m_gc, 0, 0, m_width, m_height);
    }

    void set_clip_rect(int x, int y, unsigned int w, unsigned int h, RectOrigin origin) noexcept override {
        if (!m_display || !m_back) return;
        int rx, ry;
        resolve_rect_origin(rx, ry, x, y, w, h, origin);
        XRectangle r;
        r.x      = static_cast<short>(rx);
        r.y      = static_cast<short>(ry);
        r.width  = static_cast<unsigned short>(w);
        r.height = static_cast<unsigned short>(h);
        XSetClipRectangles(m_display, m_gc, 0, 0, &r, 1, x11::kUnsorted);
        if (m_xft) XftDrawSetClipRectangles(m_xft, 0, 0, &r, 1);
    }

    void reset_clip_rect() noexcept override {
        if (!m_display || !m_back) return;
        XSetClipMask(m_display, m_gc, x11::kNone);
        if (m_xft) XftDrawSetClip(m_xft, nullptr);
    }

    void draw_pixel(int x, int y, const graphics::Color& color) noexcept override {
        if (!m_display || !m_back) return;
        XSetForeground(m_display, m_gc, to_pixel(color));
        XDrawPoint(m_display, m_back, m_gc, x, y);
    }

    void draw_line(int x1, int y1, int x2, int y2, const graphics::Paint& p) noexcept override {
        if (!m_display || !m_back || !p.has_stroke()) return;
        apply_stroke(p);
        XDrawLine(m_display, m_back, m_gc, x1, y1, x2, y2);
    }

    void draw_rect(int x, int y, unsigned int w, unsigned int h, const graphics::Paint& p) noexcept override {
        if (!m_display || !m_back) return;
        if (w == 0 || h == 0) return;
        if (!p.has_fill() && !p.has_stroke()) return;
        int rx, ry;
        resolve_rect_origin(rx, ry, x, y, w, h, p.origin());

        if (p.has_fill()) {
            XSetForeground(m_display, m_gc, to_pixel(p.fill_color()));
            XFillRectangle(m_display, m_back, m_gc, rx, ry, w, h);
        }

        if (p.has_stroke()) {
            apply_stroke(p);
            XDrawRectangle(m_display, m_back, m_gc, rx, ry, w - 1, h - 1);
        }
    }

    void draw_ellipse(int cx, int cy, unsigned int rx, unsigned int ry, const graphics::Paint& p) noexcept override {
        if (!m_display || !m_back) return;
        if (!p.has_fill() && !p.has_stroke()) return;
        const int bx = cx - static_cast<int>(rx);
        const int by = cy - static_cast<int>(ry);
        const unsigned int bw = rx * 2u, bh = ry * 2u;
        const bool full = p.is_full_sweep();
        int start64, ext64;
        angles_to_x11(p, full, start64, ext64);

        if (p.has_fill()) {
            XSetArcMode(m_display, m_gc, x11::kArcPieSlice);
            XSetForeground(m_display, m_gc, to_pixel(p.fill_color()));
            XFillArc(m_display, m_back, m_gc, bx, by, bw, bh, start64, ext64);
        }

        if (p.has_stroke()) {
            apply_stroke(p);
            XDrawArc(m_display, m_back, m_gc, bx, by, bw, bh, start64, ext64);
            if (!full) stroke_pie_radii(cx, cy, rx, ry, p);
        }
    }

    void draw_arc(int cx, int cy, unsigned int rx, unsigned int ry, const graphics::Paint& p) noexcept override {
        if (!m_display || !m_back) return;
        if (!p.has_fill() && !p.has_stroke()) return;
        const int bx = cx - static_cast<int>(rx);
        const int by = cy - static_cast<int>(ry);
        const unsigned int bw = rx * 2u, bh = ry * 2u;
        int start64, ext64;
        angles_to_x11(p, false, start64, ext64);

        if (p.has_fill()) {
            XSetArcMode(m_display, m_gc, x11::kArcChord);
            XSetForeground(m_display, m_gc, to_pixel(p.fill_color()));
            XFillArc(m_display, m_back, m_gc, bx, by, bw, bh, start64, ext64);
        }

        if (p.has_stroke()) {
            apply_stroke(p);
            XDrawArc(m_display, m_back, m_gc, bx, by, bw, bh, start64, ext64);
        }
    }

    void draw_image(
        int dx, int dy, unsigned int dw, unsigned int dh,
        const images::BitmapImage& img,
        unsigned int sx, unsigned int sy,
        unsigned int sw, unsigned int sh
    ) noexcept override {
        if (!m_display || !m_back) return;
        const unsigned int iw = img.width(), ih = img.height();
        if (iw == 0 || ih == 0 || sw == 0 || sh == 0 || dw == 0 || dh == 0) return;
        XImage* xi = create_image(dw, dh);
        if (!xi) return;

        for (unsigned int row = 0; row < dh; ++row) {
            unsigned int src_y = sy + static_cast<unsigned int>(static_cast<std::uint64_t>(row) * sh / dh);
            if (src_y >= ih) src_y = ih - 1;

            for (unsigned int col = 0; col < dw; ++col) {
                unsigned int src_x = sx + static_cast<unsigned int>(static_cast<std::uint64_t>(col) * sw / dw);
                if (src_x >= iw) src_x = iw - 1;
                XPutPixel(xi, static_cast<int>(col), static_cast<int>(row), to_pixel(img.get_pixel(src_x, src_y)));
            }
        }

        XPutImage(m_display, m_back, m_gc, xi, 0, 0, dx, dy, dw, dh);
        XDestroyImage(xi);
    }

    void draw_texture(
        int dx, int dy, unsigned int dw, unsigned int dh,
        const graphics::Texture& tex, float opacity,
        const graphics::TextureRect& src
    ) noexcept override {
        if (!m_display || !m_back || !tex.valid()) return;
        if (dw == 0 || dh == 0 || src.is_empty()) return;

        if (opacity >= 1.0f && tex.filter() == graphics::SampleFilter::Nearest) {
            draw_image(
                dx, dy, dw, dh, *tex.image(),
                static_cast<unsigned int>(src.x),
                static_cast<unsigned int>(src.y),
                src.w, src.h
            );

            return;
        }

        const int x0 = std::max(dx, 0);
        const int y0 = std::max(dy, 0);
        const int x1 = std::min(dx + static_cast<int>(dw), static_cast<int>(m_width));
        const int y1 = std::min(dy + static_cast<int>(dh), static_cast<int>(m_height));
        if (x1 <= x0 || y1 <= y0) return;
        const unsigned int rw = static_cast<unsigned int>(x1 - x0);
        const unsigned int rh = static_cast<unsigned int>(y1 - y0);
        XImage* dst = XGetImage(m_display, m_back, x0, y0, rw, rh, x11::kAllPlanes, x11::kZPixmap);
        if (!dst) return;
        const unsigned int off_x = static_cast<unsigned int>(x0 - dx);
        const unsigned int off_y = static_cast<unsigned int>(y0 - dy);

        for (unsigned int row = 0; row < rh; ++row) {
            const double v = src.y + (static_cast<double>(off_y + row) + 0.5) * src.h / dh;
            
            for (unsigned int col = 0; col < rw; ++col) {
                const double u = src.x + (static_cast<double>(off_x + col) + 0.5) * src.w / dw;
                const auto texel = tex.sample(u, v);
                float a = texel.alpha() / 255.0f;
                if (opacity < 1.0f) a *= opacity;
                if (a <= 0.0f) continue;

                if (a >= 1.0f) {
                    XPutPixel(dst, static_cast<int>(col), static_cast<int>(row), to_pixel(texel));
                    continue;
                }

                const unsigned long bg = XGetPixel(dst, static_cast<int>(col), static_cast<int>(row));
                const float inv = 1.0f - a;
                
                auto mix = [a, inv](std::uint8_t s, std::uint8_t d) -> std::uint8_t {
                    return static_cast<std::uint8_t>(std::lround(s * a + d * inv));
                };

                const std::uint8_t r = mix(texel.red(),   extract_channel(m_visual->red_mask,   bg));
                const std::uint8_t g = mix(texel.green(), extract_channel(m_visual->green_mask, bg));
                const std::uint8_t b = mix(texel.blue(),  extract_channel(m_visual->blue_mask,  bg));
                XPutPixel(dst, static_cast<int>(col), static_cast<int>(row), pack(r, g, b));
            }
        }

        XPutImage(m_display, m_back, m_gc, dst, 0, 0, x0, y0, rw, rh);
        XDestroyImage(dst);
    }

    void draw_pixel_buffer(
        int dx, int dy, unsigned int dw, unsigned int dh,
        const graphics::Color* pixels, unsigned int pw, unsigned int ph,
        bool /*smooth*/, std::uint64_t /*version*/
    ) noexcept override {
        if (!m_display || !m_back || !pixels || pw == 0 || ph == 0 || dw == 0 || dh == 0) return;
        const int x0 = std::max(dx, 0), y0 = std::max(dy, 0);
        const int x1 = std::min(dx + static_cast<int>(dw), static_cast<int>(m_width));
        const int y1 = std::min(dy + static_cast<int>(dh), static_cast<int>(m_height));
        if (x1 <= x0 || y1 <= y0) return;
        const unsigned int rw = static_cast<unsigned int>(x1 - x0), rh = static_cast<unsigned int>(y1 - y0);
        XImage* dst = XGetImage(m_display, m_back, x0, y0, rw, rh, x11::kAllPlanes, x11::kZPixmap);
        if (!dst) return;

        for (unsigned int row = 0; row < rh; ++row) {
            const unsigned int sy = static_cast<unsigned int>(static_cast<std::uint64_t>(y0 - dy + static_cast<int>(row)) * ph / dh);

            for (unsigned int col = 0; col < rw; ++col) {
                const unsigned int sx = static_cast<unsigned int>(static_cast<std::uint64_t>(x0 - dx + static_cast<int>(col)) * pw / dw);
                const graphics::Color& c = pixels[static_cast<std::size_t>(std::min(sy, ph - 1)) * pw + std::min(sx, pw - 1)];
                const unsigned int a = c.alpha();
                if (a == 0) continue;
                if (a == 255) { XPutPixel(dst, static_cast<int>(col), static_cast<int>(row), to_pixel(c)); continue; }
                const unsigned long bg = XGetPixel(dst, static_cast<int>(col), static_cast<int>(row));
                auto mix = [a](unsigned int s, unsigned int d) { return static_cast<std::uint8_t>((s * a + d * (255u - a) + 127u) / 255u); };
                XPutPixel(dst, static_cast<int>(col), static_cast<int>(row), pack(
                    mix(c.red(),   extract_channel(m_visual->red_mask,   bg)),
                    mix(c.green(), extract_channel(m_visual->green_mask, bg)),
                    mix(c.blue(),  extract_channel(m_visual->blue_mask,  bg))));
            }
        }

        XPutImage(m_display, m_back, m_gc, dst, 0, 0, x0, y0, rw, rh);
        XDestroyImage(dst);
    }

    void draw_text(int x, int y, const wchar_t* str, int len, const text::TextStyle& style) noexcept override {
        if (!str || len == 0) return;
        if (len < 0) len = static_cast<int>(std::wcslen(str));
        draw_text32(x, y, reinterpret_cast<const FcChar32*>(str), len, style);
    }

    void draw_text(int x, int y, const char* utf8, int len, const text::TextStyle& style) noexcept override {
        if (!utf8 || len == 0) return;
        if (len < 0) len = static_cast<int>(std::strlen(utf8));
        const std::vector<FcChar32> cps = decode_utf8(utf8, len);
        if (cps.empty()) return;
        draw_text32(x, y, cps.data(), static_cast<int>(cps.size()), style);
    }

    text::TextMetrics measure_text(const wchar_t* str, int len, const text::TextStyle& style) noexcept override {
        if (!str || len == 0) return {};
        if (len < 0) len = static_cast<int>(std::wcslen(str));
        return measure_text32(reinterpret_cast<const FcChar32*>(str), len, style);
    }

    text::TextMetrics measure_text(const char* utf8, int len, const text::TextStyle& style) noexcept override {
        if (!utf8 || len == 0) return {};
        if (len < 0) len = static_cast<int>(std::strlen(utf8));
        const std::vector<FcChar32> cps = decode_utf8(utf8, len);
        if (cps.empty()) return {};
        return measure_text32(cps.data(), static_cast<int>(cps.size()), style);
    }

    bool load_font_file(const char* path) noexcept override {
        return path && FcConfigAppFontAddFile(FcConfigGetCurrent(), reinterpret_cast<const FcChar8*>(path));
    }

private:
    static void angles_to_x11(const graphics::Paint& p, bool full, int& start64, int& ext64) noexcept {
        if (full) { start64 = 0; ext64 = 360 * 64; return; }
        start64 = static_cast<int>(std::lround(-p.start_angle() * 64.0));
        ext64   = static_cast<int>(std::lround(-(p.end_angle() - p.start_angle()) * 64.0));
    }

    void stroke_pie_radii(int cx, int cy, unsigned int rx, unsigned int ry, const graphics::Paint& p) noexcept {
        constexpr double kPi_180 = constants::pi_180();
        const double sa = p.start_angle() * kPi_180;
        const double ea = p.end_angle()   * kPi_180;
        const int sxp = cx + static_cast<int>(std::lround(rx * std::cos(sa)));
        const int syp = cy + static_cast<int>(std::lround(ry * std::sin(sa)));
        const int exp_ = cx + static_cast<int>(std::lround(rx * std::cos(ea)));
        const int eyp = cy + static_cast<int>(std::lround(ry * std::sin(ea)));
        XDrawLine(m_display, m_back, m_gc, cx, cy, sxp, syp);
        XDrawLine(m_display, m_back, m_gc, cx, cy, exp_, eyp);
    }

    XImage* create_image(unsigned int w, unsigned int h) noexcept {
        void* data = std::calloc(static_cast<std::size_t>(w) * h * 4u, 1);
        if (!data) return nullptr;
        XImage* xi = XCreateImage(m_display, m_visual, m_depth, x11::kZPixmap, 0, static_cast<char*>(data), w, h, 32, 0);
        if (!xi) { std::free(data); return nullptr; }
        return xi;   
    }

    static int fc_weight(text::FontWeight w) noexcept {
        switch (w) {
            case text::FontWeight::Thin:       return FC_WEIGHT_THIN;
            case text::FontWeight::ExtraLight: return FC_WEIGHT_EXTRALIGHT;
            case text::FontWeight::Light:      return FC_WEIGHT_LIGHT;
            case text::FontWeight::Normal:     return FC_WEIGHT_REGULAR;
            case text::FontWeight::Medium:     return FC_WEIGHT_MEDIUM;
            case text::FontWeight::SemiBold:   return FC_WEIGHT_DEMIBOLD;
            case text::FontWeight::Bold:       return FC_WEIGHT_BOLD;
            case text::FontWeight::ExtraBold:  return FC_WEIGHT_EXTRABOLD;
            case text::FontWeight::Black:      return FC_WEIGHT_BLACK;
            default:                           return FC_WEIGHT_REGULAR;
        }
    }

    XftFont* open_font(const text::TextStyle& style) noexcept {
        const double px = style.has_size() ? static_cast<double>(style.size()) : 16.0;
        const int weight = style.has_weight() ? fc_weight(style.weight()) : FC_WEIGHT_REGULAR;
        const int slant  = (style.has_slant() && style.slant() != text::FontSlant::Normal) ? FC_SLANT_ITALIC : FC_SLANT_ROMAN;
        const char* face = "sans-serif";
        if (style.has_font() && style.font() != text::Font::None) { face = text::font_css_name(style.font()); }

        return XftFontOpen(
            m_display, m_screen,
            FC_FAMILY,     XftTypeString,  face,
            FC_PIXEL_SIZE, XftTypeDouble,  px,
            FC_WEIGHT,     XftTypeInteger, weight,
            FC_SLANT,      XftTypeInteger, slant,
            static_cast<char*>(nullptr)
        );
    }

    bool alloc_color(std::uint8_t r, std::uint8_t g, std::uint8_t b, std::uint8_t a, XftColor& out) noexcept {
        XRenderColor rc;
        const unsigned int af = a;
        rc.red   = static_cast<unsigned short>(r * 257u * af / 255u);
        rc.green = static_cast<unsigned short>(g * 257u * af / 255u);
        rc.blue  = static_cast<unsigned short>(b * 257u * af / 255u);
        rc.alpha = static_cast<unsigned short>(a * 257u);
        return XftColorAllocValue(m_display, m_visual, m_colormap, &rc, &out) != 0;
    }

    int text_advance(XftFont* font, const FcChar32* s, int len, int spacing) noexcept {
        XGlyphInfo gi;
        XftTextExtents32(m_display, font, s, len, &gi);
        return gi.xOff + spacing * len;   
    }

    void draw_text32(int x, int y, const FcChar32* s, int len, const text::TextStyle& style) noexcept {
        if (!m_display || !m_back || !m_xft || len <= 0) return;
        XftFont* font = open_font(style);
        if (!font) return;
        const int spacing = style.has_letter_spacing() ? static_cast<int>(style.letter_spacing()) : 0;
        const int advance = text_advance(font, s, len, spacing);
        int ox = x;

        if (style.has_text_align()) {
            switch (style.text_align()) {
                case text::TextAlign::Center: ox = x - advance / 2; break;
                case text::TextAlign::Right:  ox = x - advance;     break;
                default: break;
            }
        }

        const int top      = y;
        const int baseline = top + font->ascent;

        if (style.has_bg_color()) {
            const auto bg = style.bg_color().value();
            XSetForeground(m_display, m_gc, to_pixel(bg));
            
            XFillRectangle(
                m_display, m_back, m_gc, ox, top,
                static_cast<unsigned int>(std::max(advance, 0)),
                static_cast<unsigned int>(font->ascent + font->descent)
            );
        }

        std::uint8_t fr = 0, fg = 0, fb = 0, fa = 255;
        
        if (style.has_fg_color()) {
            const auto c = style.fg_color().value();
            fr = c.red(); fg = c.green(); fb = c.blue(); fa = c.alpha();
        }
        
        XftColor color;
        if (!alloc_color(fr, fg, fb, fa, color)) { XftFontClose(m_display, font); return; }

        if (spacing == 0) {
            XftDrawString32(m_xft, &color, font, ox, baseline, s, len);
        } else {
            int pen = ox;
            
            for (int i = 0; i < len; ++i) {
                XftDrawString32(m_xft, &color, font, pen, baseline, s + i, 1);
                XGlyphInfo one;
                XftTextExtents32(m_display, font, s + i, 1, &one);
                pen += one.xOff + spacing;
            }
        }

        if (style.is_underlined() || style.is_strikethrough()) {
            XSetForeground(m_display, m_gc, pack(fr, fg, fb));
            XSetLineAttributes(m_display, m_gc, 1, x11::kLineSolid, x11::kCapButt, x11::kJoinMiter);
            
            if (style.is_underlined()) {
                const int uy = baseline + std::max(1, font->descent / 3);
                XDrawLine(m_display, m_back, m_gc, ox, uy, ox + advance, uy);
            }

            if (style.is_strikethrough()) {
                const int sy = baseline - font->ascent / 3;
                XDrawLine(m_display, m_back, m_gc, ox, sy, ox + advance, sy);
            }
        }

        XftColorFree(m_display, m_visual, m_colormap, &color);
        XftFontClose(m_display, font);
    }

    text::TextMetrics measure_text32(const FcChar32* s, int len, const text::TextStyle& style) noexcept {
        text::TextMetrics m;
        if (!m_display || len <= 0) return m;
        XftFont* font = open_font(style);
        if (!font) return m;
        const int spacing = style.has_letter_spacing() ? static_cast<int>(style.letter_spacing()) : 0;
        const int advance = text_advance(font, s, len, spacing);
        m.width   = static_cast<unsigned int>(std::max(advance, 0));
        m.height  = static_cast<unsigned int>(font->ascent + font->descent);
        m.ascent  = font->ascent;
        m.descent = font->descent;
        XftFontClose(m_display, font);
        return m;
    }

    static std::vector<FcChar32> decode_utf8(const char* s, int len) noexcept {
        std::vector<FcChar32> out;
        if (!s || len <= 0) return out;
        out.reserve(static_cast<std::size_t>(len));
        const unsigned char* p   = reinterpret_cast<const unsigned char*>(s);
        const unsigned char* end = p + len;

        while (p < end) {
            const unsigned char c = *p;
            FcChar32 cp;
            int extra;
            if (c < 0x80)              { cp = c;          extra = 0; }
            else if ((c >> 5) == 0x06) { cp = c & 0x1Fu;  extra = 1; }
            else if ((c >> 4) == 0x0E) { cp = c & 0x0Fu;  extra = 2; }
            else if ((c >> 3) == 0x1E) { cp = c & 0x07u;  extra = 3; }
            else { ++p; continue; }  

            ++p;
            bool ok = true;

            for (int i = 0; i < extra; ++i) {
                if (p >= end || (*p & 0xC0u) != 0x80u) { ok = false; break; }
                cp = (cp << 6) | (*p & 0x3Fu);
                ++p;
            }
            
            if (ok) out.push_back(cp);
        }
        return out;
    }
};

} // namespace detail
} // namespace windows
} // namespace fizmo

#endif // OS_LINUX
#endif // FIZMO_RENDERER_LINUX_IMPL_HPP