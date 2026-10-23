#ifndef FIZMO_RENDERER_PIXELS_IMPL_HPP
#define FIZMO_RENDERER_PIXELS_IMPL_HPP

#include "renderer_base.hpp"
#include "../../../Basic/fizmo_defines.hpp"

#if defined(OS_MACOS) || defined(OS_ANDROID)

#if defined(OS_MACOS)
    #include "../../../macos_compat.hpp"
#else
    #include "../../../Android/app.hpp"
#endif
#include "gpu_text.hpp"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <cwchar>
#include <string>
#include <vector>

namespace fizmo {
namespace graphics {
namespace detail {

class RendererImpl : public RendererImplBase {
private:
#if defined(OS_MACOS)
    using PixelTarget = const fizmo::macos::Handle*;
#else
    using PixelTarget = ANativeWindow*;
#endif

    PixelTarget                 m_handle = nullptr;
    std::vector<std::uint32_t>  m_px;
    unsigned int                m_width  = 0;
    unsigned int                m_height = 0;
    int                         m_cx0 = 0, m_cy0 = 0, m_cx1 = 0, m_cy1 = 0;
    gfx::TextEngine             m_text;
    bool                        m_text_ready = false;

    struct FPoint { double x, y; };

    static colors::Color with_opacity(colors::Color c, float opacity) noexcept {
        if (opacity < 1.0f) c.set_alpha(static_cast<std::uint8_t>(std::lround(c.alpha() * std::max(0.0f, opacity))));
        return c;
    }

    void reset_clip() noexcept {
        m_cx0 = 0;
        m_cy0 = 0;
        m_cx1 = static_cast<int>(m_width);
        m_cy1 = static_cast<int>(m_height);
    }

    void allocate(unsigned int w, unsigned int h) {
        m_width  = std::max(w, 1u);
        m_height = std::max(h, 1u);
        m_px.assign(static_cast<std::size_t>(m_width) * m_height, 0xFF000000u);
        reset_clip();
    }

    static std::uint32_t pack(std::uint8_t r, std::uint8_t g, std::uint8_t b) noexcept {
        return 0xFF000000u | (static_cast<std::uint32_t>(r) << 16) | (static_cast<std::uint32_t>(g) << 8) | b;
    }

    void blend(int x, int y, std::uint8_t r, std::uint8_t g, std::uint8_t b, unsigned int a) noexcept {
        if (a == 0 || x < m_cx0 || y < m_cy0 || x >= m_cx1 || y >= m_cy1) return;
        std::uint32_t& d = m_px[static_cast<std::size_t>(y) * m_width + static_cast<std::size_t>(x)];
        if (a >= 255) { d = pack(r, g, b); return; }
        const unsigned int inv = 255u - a;
        const unsigned int dr = (d >> 16) & 0xFFu, dg = (d >> 8) & 0xFFu, db = d & 0xFFu;
        d = pack(static_cast<std::uint8_t>((r * a + dr * inv + 127u) / 255u),
                 static_cast<std::uint8_t>((g * a + dg * inv + 127u) / 255u),
                 static_cast<std::uint8_t>((b * a + db * inv + 127u) / 255u));
    }

    void blend(int x, int y, const colors::Color& c, unsigned int coverage = 255) noexcept {
        blend(x, y, c.red(), c.green(), c.blue(), (c.alpha() * coverage + 127u) / 255u);
    }

    void fill_span(int y, int x0, int x1, const colors::Color& c) noexcept {
        if (y < m_cy0 || y >= m_cy1) return;
        x0 = std::max(x0, m_cx0);
        x1 = std::min(x1, m_cx1);
        for (int x = x0; x < x1; ++x) blend(x, y, c);
    }

    void fill_box(int x, int y, int w, int h, const colors::Color& c) noexcept {
        if (w <= 0 || h <= 0) return;
        for (int row = y; row < y + h; ++row) fill_span(row, x, x + w, c);
    }

    void fill_polygon(const std::vector<FPoint>& pts, const colors::Color& c) {
        if (pts.size() < 3 || c.alpha() == 0) return;
        double ymin = pts[0].y, ymax = pts[0].y;
        for (const FPoint& p : pts) { ymin = std::min(ymin, p.y); ymax = std::max(ymax, p.y); }
        const int y0 = std::max(m_cy0, static_cast<int>(std::floor(ymin)));
        const int y1 = std::min(m_cy1, static_cast<int>(std::ceil(ymax)));
        std::vector<double> xs;

        for (int y = y0; y < y1; ++y) {
            const double sy = y + 0.5;
            xs.clear();

            for (std::size_t i = 0, n = pts.size(); i < n; ++i) {
                const FPoint& a = pts[i];
                const FPoint& b = pts[(i + 1) % n];
                if ((a.y <= sy && b.y > sy) || (b.y <= sy && a.y > sy)) xs.push_back(a.x + (sy - a.y) * (b.x - a.x) / (b.y - a.y));
            }

            std::sort(xs.begin(), xs.end());

            for (std::size_t k = 0; k + 1 < xs.size(); k += 2) {
                fill_span(y, static_cast<int>(std::lround(xs[k])), static_cast<int>(std::lround(xs[k + 1])), c);
            }
        }
    }

    void fill_disc(double cx, double cy, double r, const colors::Color& c) {
        std::vector<FPoint> pts;
        const int n = std::max(12, static_cast<int>(r * 4.0));
        for (int i = 0; i < n; ++i) {
            const double a = 6.283185307179586 * i / n;
            pts.push_back({ cx + r * std::cos(a), cy + r * std::sin(a) });
        }
        fill_polygon(pts, c);
    }

    void stroke_segment(double x1, double y1, double x2, double y2, double width, graphics::LineCap cap, const colors::Color& c) {
        if (width <= 1.0) {
            int ax = static_cast<int>(std::lround(x1)), ay = static_cast<int>(std::lround(y1));
            const int bx = static_cast<int>(std::lround(x2)), by = static_cast<int>(std::lround(y2));
            const int dx = std::abs(bx - ax), sx = ax < bx ? 1 : -1;
            const int dy = -std::abs(by - ay), sy = ay < by ? 1 : -1;
            int err = dx + dy;

            for (;;) {
                blend(ax, ay, c);
                if (ax == bx && ay == by) break;
                const int e2 = 2 * err;
                if (e2 >= dy) { err += dy; ax += sx; }
                if (e2 <= dx) { err += dx; ay += sy; }
            }

            return;
        }

        const double half = width * 0.5;
        double dx = x2 - x1, dy = y2 - y1;
        const double len = std::sqrt(dx * dx + dy * dy);

        if (len < 1e-9) {
            if (cap == graphics::LineCap::Round) fill_disc(x1, y1, half, c);
            else fill_box(static_cast<int>(std::lround(x1 - half)), static_cast<int>(std::lround(y1 - half)), static_cast<int>(std::lround(width)), static_cast<int>(std::lround(width)), c);
            return;
        }

        dx /= len;
        dy /= len;
        const double ext = cap == graphics::LineCap::Square ? half : 0.0;
        const double ax = x1 - dx * ext, ay = y1 - dy * ext;
        const double bx = x2 + dx * ext, by = y2 + dy * ext;
        const double nx = -dy * half, ny = dx * half;
        fill_polygon({ { ax + nx, ay + ny }, { bx + nx, by + ny }, { bx - nx, by - ny }, { ax - nx, ay - ny } }, c);

        if (cap == graphics::LineCap::Round) {
            fill_disc(x1, y1, half, c);
            fill_disc(x2, y2, half, c);
        }
    }

    void stroke_path(const std::vector<FPoint>& pts, bool closed, const graphics::Paint& p) {
        const colors::Color c = with_opacity(p.stroke_color(), p.opacity());
        const double w = std::max(1u, p.stroke_width());
        const std::size_t n = pts.size();
        if (n < 2) return;
        const std::size_t segs = closed ? n : n - 1;
        for (std::size_t i = 0; i < segs; ++i) stroke_segment(pts[i].x, pts[i].y, pts[(i + 1) % n].x, pts[(i + 1) % n].y, w, w > 1.0 ? graphics::LineCap::Round : p.line_cap(), c);
    }

    static std::vector<FPoint> arc_points(double cx, double cy, double rx, double ry, double start_deg, double end_deg) {
        std::vector<FPoint> pts;
        const double span = end_deg - start_deg;
        const int n = std::max(8, static_cast<int>(std::ceil(std::abs(span) / 360.0 * std::max(24.0, (rx + ry) * 2.0))));
        pts.reserve(static_cast<std::size_t>(n) + 1);
        for (int i = 0; i <= n; ++i) {
            const double a = (start_deg + span * i / n) * 0.017453292519943295;
            pts.push_back({ cx + rx * std::cos(a), cy + ry * std::sin(a) });
        }
        return pts;
    }

    void draw_glyph(const gfx::GlyphImage& img, int gx, int gy, const colors::Color& c) noexcept {
        for (int row = 0; row < img.height; ++row) {
            for (int col = 0; col < img.width; ++col) {
                if (img.color) {
                    const std::uint8_t* s = &img.pixels[(static_cast<std::size_t>(row) * img.width + col) * 4];
                    blend(gx + col, gy + row, s[0], s[1], s[2], (s[3] * c.alpha() + 127u) / 255u);
                } else {
                    const unsigned int cov = img.pixels[static_cast<std::size_t>(row) * img.width + col];
                    if (cov) blend(gx + col, gy + row, c, cov);
                }
            }
        }
    }

    void render_list(const gfx::TextDrawList& list, int x, int y) {
        const int sx0 = m_cx0, sy0 = m_cy0, sx1 = m_cx1, sy1 = m_cy1;

        if (list.clip) {
            m_cx0 = std::max(m_cx0, x);
            m_cy0 = std::max(m_cy0, y);
            m_cx1 = std::min(m_cx1, x + static_cast<int>(std::ceil(list.clip_w)));
            m_cy1 = std::min(m_cy1, y + static_cast<int>(std::ceil(list.clip_h)));
        }

        gfx::GlyphImage img;

        for (const gfx::TextCmd& cmd : list.cmds) {
            if (cmd.kind == gfx::TextCmd::Kind::Rect) {
                fill_box(static_cast<int>(std::lround(x + cmd.x)), static_cast<int>(std::lround(y + cmd.y)),
                         std::max(1, static_cast<int>(std::lround(cmd.w))), std::max(1, static_cast<int>(std::lround(cmd.h))), cmd.color);
            } else if (cmd.kind == gfx::TextCmd::Kind::Glyph) {
                if (!m_text.rasterize(cmd.face, cmd.glyph, img) || img.width <= 0 || img.height <= 0) continue;
                const int gx = static_cast<int>(std::round(x + cmd.x)) + img.left;
                const int gy = static_cast<int>(std::round(y + cmd.y)) - img.top;
                draw_glyph(img, gx, gy, cmd.color);
            } else if (cmd.wave < list.waves.size()) {
                const std::vector<gfx::TextPoint>& w = list.waves[cmd.wave];
                for (std::size_t i = 0; i + 1 < w.size(); ++i) stroke_segment(x + w[i].x, y + w[i].y, x + w[i + 1].x, y + w[i + 1].y, std::max(1.0f, cmd.h), graphics::LineCap::Round, cmd.color);
            }
        }

        m_cx0 = sx0;
        m_cy0 = sy0;
        m_cx1 = sx1;
        m_cy1 = sy1;
    }

    bool layout_text(const text::RichText& rt, float box_w, float box_h, gfx::TextDrawList& list) {
        if (!m_text_ready) m_text_ready = m_text.initialize(nullptr);
        if (!m_text_ready) return false;
        return gfx::build_text_draw_list(m_text, rt, box_w, box_h, list);
    }

    void text_at(int x, int y, const std::string& utf8, const text::TextStyle& style) noexcept {
        try {
            gfx::TextDrawList list;
            if (layout_text(text::RichText(utf8, style), 0.0f, 0.0f, list)) render_list(list, x, y);
        } catch (...) {}
    }

    text::TextMetrics measure_utf8(const std::string& utf8, const text::TextStyle& style) noexcept {
        try {
            gfx::TextDrawList list;
            if (layout_text(text::RichText(utf8, style), 0.0f, 0.0f, list)) return list.metrics();
        } catch (...) {}
        return {};
    }

public:
    RendererImpl() noexcept = default;
    ~RendererImpl() noexcept override { shutdown(); }

    bool initialize(void* native_handle, unsigned int w, unsigned int h) noexcept override {
        if (!native_handle) return false;
#if defined(OS_MACOS)
        m_handle = static_cast<const fizmo::macos::Handle*>(native_handle);
        if (!m_handle->view) { m_handle = nullptr; return false; }
#else
        m_handle = static_cast<ANativeWindow*>(native_handle);
#endif
        try { allocate(w, h); } catch (...) { m_handle = nullptr; return false; }
        return true;
    }

    void shutdown() noexcept override {
        m_handle = nullptr;
        m_px.clear();
        m_px.shrink_to_fit();
        m_width = m_height = 0;
        if (m_text_ready) { m_text.shutdown(); m_text_ready = false; }
    }

    void resize(unsigned int w, unsigned int h) noexcept override {
        if (!m_handle || (w == m_width && h == m_height)) return;
        try { allocate(w, h); } catch (...) {}
    }

    void begin_frame() noexcept override {}

    void present() noexcept override {
        if (!m_handle || m_px.empty()) return;
#if defined(OS_MACOS)
        fizmo::macos::present_pixels(m_handle, m_px.data(), m_width, m_height, m_width * 4u);
#else
        fizmo::android::present_pixels(m_handle, m_px.data(), m_width, m_height, m_width * 4u);
#endif
    }

    void paint(void*) noexcept override { present(); }

    void clear(const colors::Color& color) noexcept override {
        std::fill(m_px.begin(), m_px.end(), pack(color.red(), color.green(), color.blue()));
    }

    void set_clip_rect(int x, int y, unsigned int w, unsigned int h, RectOrigin origin) noexcept override {
        int rx, ry;
        resolve_rect_origin(rx, ry, x, y, w, h, origin);
        m_cx0 = std::max(0, rx);
        m_cy0 = std::max(0, ry);
        m_cx1 = std::min(static_cast<int>(m_width), rx + static_cast<int>(w));
        m_cy1 = std::min(static_cast<int>(m_height), ry + static_cast<int>(h));
    }

    void reset_clip_rect() noexcept override { reset_clip(); }

    void draw_pixel(int x, int y, const colors::Color& color) noexcept override { blend(x, y, color); }

    void draw_line(int x1, int y1, int x2, int y2, const graphics::Paint& p) noexcept override {
        if (!p.has_stroke()) return;
        try { stroke_segment(x1, y1, x2, y2, std::max(1u, p.stroke_width()), p.line_cap(), with_opacity(p.stroke_color(), p.opacity())); } catch (...) {}
    }

    void draw_rect(int x, int y, unsigned int w, unsigned int h, const graphics::Paint& p) noexcept override {
        if (w == 0 || h == 0 || (!p.has_fill() && !p.has_stroke())) return;
        int rx, ry;
        resolve_rect_origin(rx, ry, x, y, w, h, p.origin());
        const int iw = static_cast<int>(w), ih = static_cast<int>(h);
        if (p.has_fill()) fill_box(rx, ry, iw, ih, with_opacity(p.fill_color(), p.opacity()));

        if (p.has_stroke()) {
            const colors::Color c = with_opacity(p.stroke_color(), p.opacity());
            const int t = std::max(1, std::min(static_cast<int>(p.stroke_width()), std::min(iw, ih)));
            fill_box(rx, ry, iw, t, c);
            fill_box(rx, ry + ih - t, iw, t, c);
            fill_box(rx, ry + t, t, ih - 2 * t, c);
            fill_box(rx + iw - t, ry + t, t, ih - 2 * t, c);
        }
    }

    void draw_ellipse(int cx, int cy, unsigned int rx, unsigned int ry, const graphics::Paint& p) noexcept override {
        if (!p.has_fill() && !p.has_stroke()) return;
        const bool full = p.is_full_sweep() || std::abs(p.end_angle() - p.start_angle()) < 1e-9;

        try {
            const std::vector<FPoint> arc = full ? arc_points(cx, cy, rx, ry, 0.0, 360.0) : arc_points(cx, cy, rx, ry, p.start_angle(), p.end_angle());

            if (p.has_fill()) {
                std::vector<FPoint> poly = arc;
                if (!full) poly.push_back({ static_cast<double>(cx), static_cast<double>(cy) });
                fill_polygon(poly, with_opacity(p.fill_color(), p.opacity()));
            }

            if (p.has_stroke()) {
                if (full) {
                    stroke_path(arc, true, p);
                } else {
                    std::vector<FPoint> outline;
                    outline.push_back({ static_cast<double>(cx), static_cast<double>(cy) });
                    outline.insert(outline.end(), arc.begin(), arc.end());
                    stroke_path(outline, true, p);
                }
            }
        } catch (...) {}
    }

    void draw_arc(int cx, int cy, unsigned int rx, unsigned int ry, const graphics::Paint& p) noexcept override {
        if (!p.has_fill() && !p.has_stroke()) return;

        try {
            const std::vector<FPoint> arc = arc_points(cx, cy, rx, ry, p.start_angle(), p.end_angle());
            if (p.has_fill()) fill_polygon(arc, with_opacity(p.fill_color(), p.opacity()));
            if (p.has_stroke()) stroke_path(arc, false, p);
        } catch (...) {}
    }

    void draw_image(
        int dx, int dy, unsigned int dw, unsigned int dh,
        const images::BitmapImage& img,
        unsigned int sx, unsigned int sy,
        unsigned int sw, unsigned int sh
    ) noexcept override {
        const unsigned int iw = img.width(), ih = img.height();
        if (iw == 0 || ih == 0 || sw == 0 || sh == 0 || dw == 0 || dh == 0) return;
        const int x0 = std::max(dx, m_cx0), y0 = std::max(dy, m_cy0);
        const int x1 = std::min(dx + static_cast<int>(dw), m_cx1), y1 = std::min(dy + static_cast<int>(dh), m_cy1);

        for (int y = y0; y < y1; ++y) {
            unsigned int src_y = sy + static_cast<unsigned int>(static_cast<std::uint64_t>(y - dy) * sh / dh);
            if (src_y >= ih) src_y = ih - 1;

            for (int x = x0; x < x1; ++x) {
                unsigned int src_x = sx + static_cast<unsigned int>(static_cast<std::uint64_t>(x - dx) * sw / dw);
                if (src_x >= iw) src_x = iw - 1;
                blend(x, y, img.get_pixel(src_x, src_y));
            }
        }
    }

    void draw_texture(
        int dx, int dy, unsigned int dw, unsigned int dh,
        const graphics::Texture& tex, float opacity,
        const graphics::TextureRect& src
    ) noexcept override {
        if (!tex.valid() || dw == 0 || dh == 0 || src.is_empty()) return;

        if (opacity >= 1.0f && tex.filter() == graphics::SampleFilter::Nearest) {
            draw_image(dx, dy, dw, dh, *tex.image(), static_cast<unsigned int>(src.x), static_cast<unsigned int>(src.y), src.w, src.h);
            return;
        }

        const int x0 = std::max(dx, m_cx0), y0 = std::max(dy, m_cy0);
        const int x1 = std::min(dx + static_cast<int>(dw), m_cx1), y1 = std::min(dy + static_cast<int>(dh), m_cy1);

        for (int y = y0; y < y1; ++y) {
            const double v = src.y + (static_cast<double>(y - dy) + 0.5) * src.h / dh;

            for (int x = x0; x < x1; ++x) {
                const double u = src.x + (static_cast<double>(x - dx) + 0.5) * src.w / dw;
                blend(x, y, with_opacity(tex.sample(u, v), opacity));
            }
        }
    }

    void draw_pixel_buffer(
        int dx, int dy, unsigned int dw, unsigned int dh,
        const colors::Color* pixels, unsigned int pw, unsigned int ph,
        bool, std::uint64_t
    ) noexcept override {
        if (!pixels || pw == 0 || ph == 0 || dw == 0 || dh == 0) return;
        const int x0 = std::max(dx, m_cx0), y0 = std::max(dy, m_cy0);
        const int x1 = std::min(dx + static_cast<int>(dw), m_cx1), y1 = std::min(dy + static_cast<int>(dh), m_cy1);

        for (int y = y0; y < y1; ++y) {
            const unsigned int sy = std::min(ph - 1, static_cast<unsigned int>(static_cast<std::uint64_t>(y - dy) * ph / dh));

            for (int x = x0; x < x1; ++x) {
                const unsigned int sx = std::min(pw - 1, static_cast<unsigned int>(static_cast<std::uint64_t>(x - dx) * pw / dw));
                blend(x, y, pixels[static_cast<std::size_t>(sy) * pw + sx]);
            }
        }
    }

    void draw_text(int x, int y, const wchar_t* str, int len, const text::TextStyle& style) noexcept override {
        if (!str || len == 0) return;
        if (len < 0) len = static_cast<int>(std::wcslen(str));
        try { text_at(x, y, gfx::wide_to_utf8(str, len), style); } catch (...) {}
    }

    void draw_text(int x, int y, const char* utf8, int len, const text::TextStyle& style) noexcept override {
        if (!utf8 || len == 0) return;
        if (len < 0) len = static_cast<int>(std::strlen(utf8));
        try { text_at(x, y, std::string(utf8, static_cast<std::size_t>(len)), style); } catch (...) {}
    }

    text::TextMetrics measure_text(const wchar_t* str, int len, const text::TextStyle& style) noexcept override {
        if (!str || len == 0) return {};
        if (len < 0) len = static_cast<int>(std::wcslen(str));
        try { return measure_utf8(gfx::wide_to_utf8(str, len), style); } catch (...) { return {}; }
    }

    text::TextMetrics measure_text(const char* utf8, int len, const text::TextStyle& style) noexcept override {
        if (!utf8 || len == 0) return {};
        if (len < 0) len = static_cast<int>(std::strlen(utf8));
        try { return measure_utf8(std::string(utf8, static_cast<std::size_t>(len)), style); } catch (...) { return {}; }
    }

    void draw_rich_text(int x, int y, unsigned int w, unsigned int h, const text::RichText& rt) noexcept override {
        try {
            gfx::TextDrawList list;
            if (layout_text(rt, static_cast<float>(w), static_cast<float>(h), list)) render_list(list, x, y);
        } catch (...) {}
    }

    text::TextMetrics measure_rich_text(const text::RichText& rt, unsigned int max_width) noexcept override {
        try {
            gfx::TextDrawList list;
            if (layout_text(rt, static_cast<float>(max_width), 0.0f, list)) return list.metrics();
        } catch (...) {}
        return {};
    }

    bool load_font_file(const char* path) noexcept override {
        if (!path) return false;
        if (!m_text_ready) m_text_ready = m_text.initialize(nullptr);
        try { return m_text_ready && m_text.load_font_file(path); } catch (...) { return false; }
    }

    bool capture(images::BitmapImage& out) noexcept override {
        if (m_px.empty()) return false;

        try {
            images::BitmapImage img(m_width, m_height);
            std::vector<colors::Color>& dst = img.mutable_pixels();
            for (std::size_t i = 0; i < m_px.size(); ++i) {
                const std::uint32_t v = m_px[i];
                dst[i] = colors::Color(static_cast<std::uint8_t>((v >> 16) & 0xFFu), static_cast<std::uint8_t>((v >> 8) & 0xFFu), static_cast<std::uint8_t>(v & 0xFFu), 255);
            }
            out = std::move(img);
            return true;
        } catch (...) {
            return false;
        }
    }
};

} // namespace detail
} // namespace graphics
} // namespace fizmo

#endif // OS_MACOS || OS_ANDROID
#endif // FIZMO_RENDERER_PIXELS_IMPL_HPP
