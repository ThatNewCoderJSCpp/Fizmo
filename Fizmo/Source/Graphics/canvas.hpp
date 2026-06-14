#ifndef FIZMO_CANVAS_HPP
#define FIZMO_CANVAS_HPP

#include "../Graphics/color.hpp"
#include "../Matrices/square_matrices.hpp"
#include "paint.hpp"
#include "gradient.hpp"
#include "texture.hpp"
#include "../Text/text_style.hpp"
#include "../Text/bitmap_font.hpp"
#include "../Text/utf_8.hpp"
#include "path_2d.hpp"
#include "../Standard Overloads/optional.hpp"

#include <vector>
#include <array>
#include <algorithm>
#include <cmath>
#include <functional>
#include <utility>
#include <cstdlib>

namespace fizmo {
namespace graphics {

class Framebuffer {
private:
    std::vector<Color> m_pixels;
    unsigned int m_width  = 0;
    unsigned int m_height = 0;

public:
    Framebuffer() = default;
    Framebuffer(unsigned int w, unsigned int h, const Color& fill = Color()) : m_pixels(w * h, fill), m_width(w), m_height(h) {}

    void resize(unsigned int w, unsigned int h, const Color& fill = Color()) {
        m_width  = w;
        m_height = h;
        m_pixels.assign(w * h, fill);
    }

    unsigned int width()  const noexcept { return m_width; }
    unsigned int height() const noexcept { return m_height; }

    bool in_bounds(int x, int y) const noexcept {
        return x >= 0 && x < static_cast<int>(m_width)
            && y >= 0 && y < static_cast<int>(m_height);
    }

    void set_pixel(int x, int y, const Color& c) noexcept {
        if (in_bounds(x, y)) m_pixels[y * m_width + x] = c;
    }

    Color get_pixel(int x, int y) const noexcept {
        if (in_bounds(x, y)) return m_pixels[y * m_width + x];
        return {};
    }

    void clear(const Color& c = Color()) noexcept {
        std::fill(m_pixels.begin(), m_pixels.end(), c);
    }

    const Color* data()  const noexcept { return m_pixels.data(); }
    Color*       data()        noexcept { return m_pixels.data(); }
    const std::vector<Color>& pixels() const noexcept { return m_pixels; }

    images::BitmapImage to_bitmap_image() const {
        images::BitmapImage img(m_width, m_height);
        for (unsigned int y = 0; y < m_height; ++y)
            for (unsigned int x = 0; x < m_width; ++x)
                img.set_pixel(x, y, m_pixels[y * m_width + x]);
        return img;
    }

    bool save_to_bmp(const std::string& path) const { return to_bitmap_image().save_to_file(path); }
    bool save_to_png(const std::string& path) const { return to_bitmap_image().save_to_png(path); }
    bool save_to_jpeg(const std::string& path, int quality = 90) const { return to_bitmap_image().save_to_jpeg(path, quality); }
};

class Canvas {
public:
    struct ClipRect {
        int x = 0, y = 0;
        unsigned int w = 0, h = 0;
    };

    struct State {
        math::Matrix3x3<double> transform = math::Matrix3x3<double>::identity();
        Optional<ClipRect> clip = null_option;
    };

private:
    Framebuffer* m_fb;
    std::vector<State> m_stack;

public:
    explicit Canvas(Framebuffer& fb) noexcept : m_fb(&fb) { m_stack.emplace_back(); }
    Framebuffer&       framebuffer()       noexcept { return *m_fb; }
    const Framebuffer& framebuffer() const noexcept { return *m_fb; }
    
    const math::Matrix3x3<double>& current_transform() const noexcept { return m_stack.back().transform; }
    const Optional<ClipRect>& current_clip() const noexcept { return m_stack.back().clip; }

public:
    void save()    { m_stack.push_back(m_stack.back()); }
    void restore() { if (m_stack.size() > 1) m_stack.pop_back(); }
    void set_transform(const math::Matrix3x3<double>& m) noexcept { m_stack.back().transform = m; }
    void reset_transform() noexcept { m_stack.back().transform = math::Matrix3x3<double>::identity(); }
    void reset_clip_rect() noexcept { m_stack.back().clip = null_option; }

    void set_clip_rect(int x, int y, unsigned int w, unsigned int h, RectOrigin origin = RectOrigin::TopLeft) noexcept {
        ClipRect cr;
        resolve_rect_origin(cr.x, cr.y, x, y, w, h, origin);
        cr.w = w;
        cr.h = h;
        auto& cur = m_stack.back().clip;

        if (cur.has_value()) {
            int ax1 = cur->x, ay1 = cur->y;
            int ax2 = ax1 + static_cast<int>(cur->w);
            int ay2 = ay1 + static_cast<int>(cur->h);
            int bx1 = cr.x, by1 = cr.y;
            int bx2 = bx1 + static_cast<int>(cr.w);
            int by2 = by1 + static_cast<int>(cr.h);
            int ix1 = std::max(ax1, bx1), iy1 = std::max(ay1, by1);
            int ix2 = std::min(ax2, bx2), iy2 = std::min(ay2, by2);

            if (ix1 >= ix2 || iy1 >= iy2) {
                cr.x = 0; cr.y = 0; cr.w = 0; cr.h = 0;
            } else {
                cr.x = ix1; cr.y = iy1;
                cr.w = static_cast<unsigned int>(ix2 - ix1);
                cr.h = static_cast<unsigned int>(iy2 - iy1);
            }
        }

        cur = cr;
    }

public:
    images::BitmapImage to_bitmap_image() const { return m_fb->to_bitmap_image(); }
    bool save_to_bmp(const std::string& path)  const { return m_fb->save_to_bmp(path); }
    bool save_to_png(const std::string& path)  const { return m_fb->save_to_png(path); }
    bool save_to_jpeg(const std::string& path, int quality = 90) const { return m_fb->save_to_jpeg(path, quality); }

public:
    bool has_clip() const noexcept { return m_stack.back().clip.has_value(); }

public:
    void transform(const math::Matrix3x3<double>& m) noexcept { m_stack.back().transform = m_stack.back().transform * m; }

    void translate(double tx, double ty) noexcept {
        transform(math::Matrix3x3<double>::translation_2d(tx, ty));
    }

    void rotate(double angle, bool degrees = true) noexcept {
        transform(math::Matrix3x3<double>::rotation_2d(angle, degrees));
    }

    void scale(double sx, double sy) noexcept {
        transform(math::Matrix3x3<double>{
            sx,  0.0, 0.0,
            0.0, sy,  0.0,
            0.0, 0.0, 1.0
        });
    }

    void scale(double s) noexcept { scale(s, s); }
    void clear(const Color& c = Color()) noexcept { m_fb->clear(c); }

public:
    vector2d map_point(double x, double y) const noexcept {
        const auto& m = m_stack.back().transform;
        return { m.data[0]*x + m.data[1]*y + m.data[2], m.data[3]*x + m.data[4]*y + m.data[5] };
    }

    vector2d map_point(vector2d v) const noexcept { return map_point(v.x, v.y); }

    vector2d inverse_map_point(double sx, double sy) const noexcept {
        const auto& m = current_transform().data;
        double a = m[0], b = m[1], c = m[2];
        double d = m[3], e = m[4], f = m[5];
        double det = a * e - b * d;
        if (std::abs(det) <= constants::middle_epsilon()) return { sx, sy };
        double inv_det = 1.0 / det;
        double px = sx - c;
        double py = sy - f;

        return {
             (e * px - b * py) * inv_det,
            (-d * px + a * py) * inv_det
        };
    }

public:
    void draw_pixel(int x, int y, const Color& c) noexcept {
        auto [px, py] = map_point(x, y);
        put_pixel(static_cast<int>(std::round(px)), static_cast<int>(std::round(py)), c);
    }

public:
    void draw_line(int x1, int y1, int x2, int y2, const Paint& p) noexcept {
        if (!p.has_stroke()) return;
        auto [ax, ay] = map_point(x1, y1);
        auto [bx, by] = map_point(x2, y2);
        int sx = static_cast<int>(std::round(ax));
        int sy = static_cast<int>(std::round(ay));
        int ex = static_cast<int>(std::round(bx));
        int ey = static_cast<int>(std::round(by));

        if (p.stroke_width() <= 1) {
            bresenham(sx, sy, ex, ey, p.stroke_color());
        } else {
            thick_line(sx, sy, ex, ey, p);
        }
    }

public:
    void draw_rect(int x, int y, unsigned int w, unsigned int h, const Paint& p) noexcept {
        resolve_rect_origin(x, y, w, h, p.origin());
        int iw = static_cast<int>(w), ih = static_cast<int>(h);

        std::array<vector2d, 4> corners = {{
            map_point(x,      y),
            map_point(x + iw, y),
            map_point(x + iw, y + ih),
            map_point(x,      y + ih)
        }};

        if (p.has_fill())   fill_shape(corners, p);
        if (p.has_stroke()) stroke_polygon(corners, p);
    }

    void draw_rect_corners(int x1, int y1, int x2, int y2, const Paint& p) noexcept {
        int lx = (x1 < x2) ? x1 : x2;
        int ly = (y1 < y2) ? y1 : y2;
        unsigned int w = static_cast<unsigned int>(std::abs(x2 - x1));
        unsigned int h = static_cast<unsigned int>(std::abs(y2 - y1));
        
        std::array<vector2d, 4> corners = {{
            map_point(lx,                        ly),
            map_point(lx + static_cast<int>(w),  ly),
            map_point(lx + static_cast<int>(w),  ly + static_cast<int>(h)),
            map_point(lx,                        ly + static_cast<int>(h))
        }};

        if (p.has_fill())   fill_shape(corners, p);
        if (p.has_stroke()) stroke_polygon(corners, p);
    }

public:
    void draw_circle(int cx, int cy, unsigned int radius, const Paint& p) noexcept {
        draw_ellipse(cx, cy, radius, radius, p);
    }

public:
    void draw_ellipse(int cx, int cy, unsigned int rx, unsigned int ry, const Paint& p) noexcept {
        if (!p.has_fill() && !p.has_stroke()) return;
        constexpr int kSegments = 64;
        std::vector<vector2d> pts(kSegments);

        for (int i = 0; i < kSegments; ++i) {
            double angle = 2.0 * constants::pi() * i / kSegments;
            double ex = cx + rx * std::cos(angle);
            double ey = cy + ry * std::sin(angle);
            pts[i] = map_point(ex, ey);
        }

        if (p.has_fill())   fill_shape(pts, p);
        if (p.has_stroke()) stroke_polyline(pts, p, true);
    }

public:
    void draw_triangle(int x1, int y1, int x2, int y2, int x3, int y3, const Paint& p) noexcept {
        std::array<vector2d, 3> tri = {{
            map_point(x1, y1), map_point(x2, y2), map_point(x3, y3)
        }};

        if (p.has_fill()) {
            std::vector<vector2d> v(tri.begin(), tri.end());
            fill_shape(v, p);
        }

        if (p.has_stroke()) {
            for (int i = 0; i < 3; ++i) {
                int ni = (i + 1) % 3;

                thick_line(
                    static_cast<int>(std::round(tri[i].x)),
                    static_cast<int>(std::round(tri[i].y)),
                    static_cast<int>(std::round(tri[ni].x)),
                    static_cast<int>(std::round(tri[ni].y)),
                    p
                );
            }
        }
    }

public:
    void draw_polygon(const std::vector<vector2d>& local_pts, const Paint& p) noexcept {
        if (local_pts.size() < 3) return;
        std::vector<vector2d> mapped(local_pts.size());
        for (std::size_t i = 0; i < local_pts.size(); ++i) { mapped[i] = map_point(local_pts[i]); }
        if (p.has_fill())   fill_shape(mapped, p);
        if (p.has_stroke()) stroke_polyline(mapped, p, true);
    }

    void draw_polyline(const std::vector<vector2d>& local_pts, const Paint& p) noexcept {
        if (local_pts.size() < 2 || !p.has_stroke()) return;
        std::vector<vector2d> mapped(local_pts.size());
        for (std::size_t i = 0; i < local_pts.size(); ++i) { mapped[i] = map_point(local_pts[i]); }
        stroke_polyline(mapped, p, false);
    }

public:
    void draw_image(
        const fizmo::images::BitmapImage& img, 
        int dx, int dy, 
        RectOrigin origin = RectOrigin::TopLeft
    ) noexcept {
        draw_image(img, dx, dy, img.width(), img.height(), 0, 0, img.width(), img.height(), origin);
    }

    void draw_image(
        const fizmo::images::BitmapImage& img,
        int dx, int dy,
        unsigned int dw, unsigned int dh,
        RectOrigin origin = RectOrigin::TopLeft
    ) noexcept {
        draw_image(img, dx, dy, dw, dh, 0, 0, img.width(), img.height(), origin);
    }

    void draw_image(
        const fizmo::images::BitmapImage& img,
        int dx, int dy,
        unsigned int dw, unsigned int dh,
        unsigned int sx, unsigned int sy,
        unsigned int sw, unsigned int sh,
        RectOrigin origin = RectOrigin::TopLeft
    ) noexcept {
        if (sw == 0 || sh == 0 || dw == 0 || dh == 0) return;
        resolve_rect_origin(dx, dy, dw, dh, origin);

        for (unsigned int row = 0; row < dh; ++row) {
            for (unsigned int col = 0; col < dw; ++col) {
                unsigned int src_x = sx + static_cast<unsigned int>(static_cast<double>(col) * sw / dw);
                unsigned int src_y = sy + static_cast<unsigned int>(static_cast<double>(row) * sh / dh);
                src_x = std::min(src_x, sx + sw - 1);
                src_y = std::min(src_y, sy + sh - 1);
                auto color = img.get_pixel(src_x, src_y);
                auto [px, py] = map_point(dx + static_cast<int>(col), dy + static_cast<int>(row));
                put_pixel(static_cast<int>(std::round(px)), static_cast<int>(std::round(py)), color);
            }
        }
    }

public:
    void draw_texture(
        const Texture& tex, 
        int dx, int dy,
        float opacity = 1.0f,
        RectOrigin origin = RectOrigin::TopLeft
    ) noexcept {
        if (!tex.valid()) return;
        draw_texture(tex, dx, dy, tex.width(), tex.height(), tex.full_rect(), opacity, origin);
    }

    void draw_texture(
        const Texture& tex, 
        int dx, int dy,
        unsigned int dw, unsigned int dh,
        float opacity = 1.0f,
        RectOrigin origin = RectOrigin::TopLeft
    ) noexcept {
        if (!tex.valid()) return;
        draw_texture(tex, dx, dy, dw, dh, tex.full_rect(), opacity, origin);
    }

    void draw_texture(
        const Texture& tex,
        int dx, int dy,
        unsigned int dw, unsigned int dh,
        const TextureRect& src,
        float opacity = 1.0f,
        RectOrigin origin = RectOrigin::TopLeft
    ) noexcept {
        if (!tex.valid() || dw == 0 || dh == 0 || src.is_empty()) return;
        resolve_rect_origin(dx, dy, dw, dh, origin);

        for (unsigned int row = 0; row < dh; ++row) {
            for (unsigned int col = 0; col < dw; ++col) {
                double u = src.x + (static_cast<double>(col) + 0.5) * src.w / dw;
                double v = src.y + (static_cast<double>(row) + 0.5) * src.h / dh;
                auto texel = tex.sample(u, v);
                auto [px, py] = map_point(dx + static_cast<int>(col), dy + static_cast<int>(row));
                int ix = static_cast<int>(std::round(px));
                int iy = static_cast<int>(std::round(py));

                if (opacity < 1.0f) {
                    texel.set_alpha(static_cast<std::uint8_t>(
                        std::round(texel.alpha() * opacity)
                    ));
                }

                if (!texel.is_opaque()) {
                    if (texel.is_transparent()) continue; 
                    auto dst = m_fb->get_pixel(ix, iy);
                    texel = dst.blend_over(texel); 
                }

                put_pixel(ix, iy, texel);
            }
        }
    }

public:
    void draw_path(const Path2D& path, const Paint& p, double tolerance = 0.5) noexcept {
        auto contours = path.flatten(tolerance);

        for (auto& contour : contours) {
            if (contour.points.size() < 2) continue;
            std::vector<vector2d> mapped(contour.points.size());

            for (std::size_t i = 0; i < contour.points.size(); ++i) {
                mapped[i] = map_point(contour.points[i]);
            }

            if (p.has_fill())   fill_shape(mapped, p);
            if (p.has_stroke()) stroke_polyline(mapped, p, contour.closed);
        }
    }

public:
    void clear_rect(
        int x, int y, unsigned int w, unsigned int h,
        RectOrigin origin = RectOrigin::TopLeft,
        const Color& c = Color()
    ) noexcept {
        draw_rect(x, y, w, h, Paint::fill(c).set_origin(origin));
    }

    void clear_rect_corners(int x1, int y1, int x2, int y2, const Color& c = Color()) noexcept {
        draw_rect_corners(x1, y1, x2, y2, Paint::fill(c));
    }

    void clear_circle(int cx, int cy, unsigned int radius, const Color& c = Color()) noexcept {
        draw_circle(cx, cy, radius, Paint::fill(c));
    }

    void clear_ellipse(int cx, int cy, unsigned int rx, unsigned int ry, const Color& c = Color()) noexcept {
        draw_ellipse(cx, cy, rx, ry, Paint::fill(c));
    }

    void clear_polygon(const std::vector<vector2d>& local_pts, const Color& c = Color()) noexcept {
        draw_polygon(local_pts, Paint::fill(c));
    }

public:
    void draw_text(
        int x, int y,
        const std::string& text,
        const text::TextStyle& style,
        RectOrigin origin = RectOrigin::TopLeft,
        const text::BitmapFont& font = text::BitmapFont::builtin()
    ) noexcept {
        if (text.empty() || !m_fb) return;
        const auto params = resolve_text_params(text, style);
        unsigned int total_w = text_line_width(params.num_cps, params.gw, params.extra_spacing);
        unsigned int total_h = params.gh;
        int dx = x, dy = y;
        resolve_rect_origin(dx, dy, total_w, total_h, origin);
        if (style.has_bg_color()) { draw_rect(dx, dy, total_w, total_h, Paint::fill(to_canvas_color(style.bg_color().value()))); }
        draw_text_line_glyphs(dx, dy, text, params, font);
    }

    void draw_text(
        int x, int y,
        const std::string& text,
        double size_px, const Color& color,
        RectOrigin origin = RectOrigin::TopLeft
    ) noexcept {
        text::TextStyle s(size_px, color);
        draw_text(x, y, text, s, origin);
    }

    void draw_text(
        int x, int y,
        const std::string& text,
        const Color& color,
        RectOrigin origin = RectOrigin::TopLeft
    ) noexcept {
        text::TextStyle s;
        s.set_fg_color(color);
        draw_text(x, y, text, s, origin);
    }

    void draw_text(
        int x, int y,
        unsigned int w, unsigned int h,
        const std::string& text,
        const text::TextStyle& style,
        RectOrigin origin = RectOrigin::TopLeft,
        const text::BitmapFont& font = text::BitmapFont::builtin()
    ) noexcept {
        if (text.empty() || !m_fb || w == 0 || h == 0) return;
        resolve_rect_origin(x, y, w, h, origin);
        const auto params = resolve_text_params(text, style);
        const auto overflow = style.has_text_overflow() ? style.text_overflow() : text::TextOverflow::Visible;
        double lh_mult = style.has_line_height() ? style.line_height() : 1.0;
        unsigned int line_h = std::max(1u, static_cast<unsigned int>(std::round(params.gh * lh_mult)));
        unsigned int glyphs_per_line = (params.gw > 0) ? (w + params.extra_spacing) / (params.gw + params.extra_spacing) : 0;
        if (glyphs_per_line == 0) glyphs_per_line = 1;
        unsigned int max_lines_fit = (line_h > 0) ? (h / line_h) : 1;
        if (max_lines_fit == 0) max_lines_fit = 1;
        unsigned int max_lines = style.has_max_lines() ? std::min(style.max_lines(), max_lines_fit) : max_lines_fit;
        if (overflow == text::TextOverflow::Visible) { max_lines = style.has_max_lines() ? style.max_lines() : 0xFFFFFFFF; }
        std::vector<std::uint32_t> cps;
        cps.reserve(params.num_cps);
        for (std::uint32_t cp : text::utf8::codepoints(text)) { cps.push_back(cp); }
        std::vector<std::vector<std::uint32_t>> lines;

        if (overflow == text::TextOverflow::WordWrap) {
            lines = word_wrap_codepoints(cps, glyphs_per_line);
        } else {
            lines.push_back(cps);
        }

        bool truncated = false;

        if (lines.size() > max_lines) {
            lines.resize(max_lines);
            truncated = true;
        }

        if ((overflow == text::TextOverflow::Ellipsis) || (truncated && overflow == text::TextOverflow::WordWrap)) {
            auto& last = lines.back();
            bool needs_ellipsis = truncated || (last.size() > glyphs_per_line);

            if (needs_ellipsis && glyphs_per_line >= 3) {
                if (last.size() > glyphs_per_line) last.resize(glyphs_per_line);
                std::size_t start = (last.size() >= 3) ? last.size() - 3 : 0;
                last.resize(start);
                last.push_back('.');
                last.push_back('.');
                last.push_back('.');
            } else if (needs_ellipsis) {
                last.resize(std::min(last.size(), static_cast<std::size_t>(glyphs_per_line)));
            }
        }

        if (overflow == text::TextOverflow::Clip || overflow == text::TextOverflow::Visible) {
            if (overflow == text::TextOverflow::Clip) {
                for (auto& line : lines) {
                    if (line.size() > glyphs_per_line) line.resize(glyphs_per_line);
                }
            }
        }

        unsigned int total_text_h = static_cast<unsigned int>(lines.size()) * line_h;
        int vert_offset = 0;
        text::VerticalAlign va = text::VerticalAlign::Top;
        if (style.has_vertical_align()) va = style.vertical_align();

        switch (va) {
            case text::VerticalAlign::Middle:
                vert_offset = (static_cast<int>(h) - static_cast<int>(total_text_h)) / 2;
                break;
            case text::VerticalAlign::Bottom:
                vert_offset = static_cast<int>(h) - static_cast<int>(total_text_h);
                break;
            default:
                vert_offset = 0;
                break;
        }

        if (style.has_bg_color()) {
            draw_rect(x, y, w, h, Paint::fill(to_canvas_color(style.bg_color().value())));
        }

        text::TextAlign align = style.has_text_align() ? style.text_align() : text::TextAlign::Left;

        for (std::size_t i = 0; i < lines.size(); ++i) {
            const auto& line = lines[i];
            if (line.empty()) continue;
            unsigned int line_w = text_line_width(line.size(), params.gw, params.extra_spacing);
            int lx = x;

            switch (align) {
                case text::TextAlign::Center:
                    lx = x + (static_cast<int>(w) - static_cast<int>(line_w)) / 2;
                    break;
                case text::TextAlign::Right:
                    lx = x + static_cast<int>(w) - static_cast<int>(line_w);
                    break;
                default:
                    lx = x;
                    break;
            }

            int ly = y + vert_offset + static_cast<int>(i * line_h);
            int advance = static_cast<int>(params.gw) + params.extra_spacing;
            int cx = lx;

            for (std::uint32_t cp : line) {
                draw_bitmap_glyph(cx, ly, cp, font, params.scale, params.fg, params.bold);
                cx += advance;
            }
        }
    }

    text::TextMetrics measure_text(
        const std::string& text,
        const text::TextStyle& style,
        const text::BitmapFont& font = text::BitmapFont::builtin()
    ) const noexcept {
        text::TextMetrics m;
        if (text.empty()) return m;
        const auto params = resolve_text_params(text, style);
        m.width   = text_line_width(params.num_cps, params.gw, params.extra_spacing);
        m.height  = params.gh;
        m.ascent  = static_cast<int>(params.gh);
        m.descent = 0;
        return m;
    }

    text::TextMetrics measure_text(
        const std::string& text,
        const text::TextStyle& style,
        unsigned int bound_width,
        const text::BitmapFont& font = text::BitmapFont::builtin()
    ) const noexcept {
        text::TextMetrics m;
        if (text.empty()) return m;
        const auto params = resolve_text_params(text, style);
        const auto overflow = style.has_text_overflow() ? style.text_overflow() : text::TextOverflow::Visible;
        double lh_mult = style.has_line_height() ? style.line_height() : 1.0;
        unsigned int line_h = std::max(1u, static_cast<unsigned int>(std::round(params.gh * lh_mult)));

        if (overflow == text::TextOverflow::WordWrap && bound_width > 0) {
            unsigned int glyphs_per_line = (params.gw > 0)
                ? (bound_width + params.extra_spacing) / (params.gw + params.extra_spacing)
                : 1;

            if (glyphs_per_line == 0) glyphs_per_line = 1;

            std::vector<std::uint32_t> cps;
            for (std::uint32_t cp : text::utf8::codepoints(text)) cps.push_back(cp);
            auto lines = word_wrap_codepoints(cps, glyphs_per_line);
            unsigned int max_w = 0;

            for (const auto& line : lines) {
                unsigned int lw = text_line_width(line.size(), params.gw, params.extra_spacing);
                if (lw > max_w) max_w = lw;
            }

            m.width  = max_w;
            m.height = static_cast<unsigned int>(lines.size()) * line_h;
        } else {
            m.width  = text_line_width(params.num_cps, params.gw, params.extra_spacing);
            m.height = params.gh;
        }

        m.ascent  = static_cast<int>(params.gh);
        m.descent = 0;
        return m;
    }

    text::TextMetrics measure_text(
        const std::string& text,
        double size_px
    ) const noexcept {
        text::TextStyle s;
        s.set_size(size_px);
        return measure_text(text, s);
    }

private:
    void bresenham(int x0, int y0, int x1, int y1, const Color& c) noexcept {
        int dx = std::abs(x1 - x0);
        int dy = -std::abs(y1 - y0);
        int sx = x0 < x1 ? 1 : -1;
        int sy = y0 < y1 ? 1 : -1;
        int err = dx + dy;

        for (;;) {
            put_pixel(x0, y0, c);            
            if (x0 == x1 && y0 == y1) break;
            int e2 = 2 * err;
            if (e2 >= dy) { err += dy; x0 += sx; }
            if (e2 <= dx) { err += dx; y0 += sy; }
        }
    }

    void thick_line(int x0, int y0, int x1, int y1, const Paint& p) noexcept {
        int r = static_cast<int>(p.stroke_width()) / 2;
        const auto& c = p.stroke_color();
        int dx = std::abs(x1 - x0);
        int dy = -std::abs(y1 - y0);
        int sx = x0 < x1 ? 1 : -1;
        int sy = y0 < y1 ? 1 : -1;
        int err = dx + dy;

        for (;;) {
            stamp_disc(x0, y0, r, c);
            if (x0 == x1 && y0 == y1) break;
            int e2 = 2 * err;
            if (e2 >= dy) { err += dy; x0 += sx; }
            if (e2 <= dx) { err += dx; y0 += sy; }
        }
    }

    void stamp_disc(int cx, int cy, int r, const Color& c) noexcept {
        for (int dy = -r; dy <= r; ++dy) {
            int half = static_cast<int>(std::sqrt(r * r - dy * dy));
            for (int dx = -half; dx <= half; ++dx) { put_pixel(cx + dx, cy + dy, c); }
        }
    }

    void fill_shape(const std::vector<vector2d>& pts, const Paint& p) noexcept {
        if (p.has_fill_gradient()) {
            scanline_fill_gradient(pts, *p.fill_gradient());
        } else {
            scanline_fill(pts, p.fill_color());
        }
    }

    void fill_shape(const std::array<vector2d, 4>& quad, const Paint& p) noexcept {
        std::vector<vector2d> v(quad.begin(), quad.end());
        fill_shape(v, p);
    }

    void fill_shape(const std::array<vector2d, 3>& tri, const Paint& p) noexcept {
        std::vector<vector2d> v(tri.begin(), tri.end());
        fill_shape(v, p);
    }

    void fill_convex_quad(const std::array<vector2d, 4>& q, const Color& c) noexcept {
        std::vector<vector2d> v(q.begin(), q.end());
        scanline_fill(v, c);
    }

    void scanline_fill(const std::vector<vector2d>& pts, const Color& c) noexcept {
        if (pts.size() < 3) return;
        double ymin_d = pts[0].y, ymax_d = pts[0].y;

        for (auto& p : pts) {
            if (p.y < ymin_d) ymin_d = p.y;
            if (p.y > ymax_d) ymax_d = p.y;
        }

        int ymin = static_cast<int>(std::floor(ymin_d));
        int ymax = static_cast<int>(std::ceil(ymax_d));
        const auto& clip = m_stack.back().clip;
        int clip_x0 = 0, clip_y0 = 0;
        int clip_x1 = static_cast<int>(m_fb->width());
        int clip_y1 = static_cast<int>(m_fb->height());

        if (clip.has_value()) {
            clip_x0 = clip->x;
            clip_y0 = clip->y;
            clip_x1 = clip->x + static_cast<int>(clip->w);
            clip_y1 = clip->y + static_cast<int>(clip->h);
        }

        if (ymin < clip_y0) ymin = clip_y0;
        if (ymax >= clip_y1) ymax = clip_y1 - 1;

        int n = static_cast<int>(pts.size());
        std::vector<double> xs;

        for (int y = ymin; y <= ymax; ++y) {
            xs.clear();
            double yd = y + 0.5;

            for (int i = 0; i < n; ++i) {
                int j = (i + 1) % n;
                double y0 = pts[i].y, y1 = pts[j].y;

                if ((y0 <= yd && y1 > yd) || (y1 <= yd && y0 > yd)) {
                    double t = (yd - y0) / (y1 - y0);
                    xs.push_back(pts[i].x + t * (pts[j].x - pts[i].x));
                }
            }

            std::sort(xs.begin(), xs.end());

            for (std::size_t k = 0; k + 1 < xs.size(); k += 2) {
                int xstart = static_cast<int>(std::ceil(xs[k]));
                int xend   = static_cast<int>(std::floor(xs[k + 1]));
                if (xstart < clip_x0) xstart = clip_x0;
                if (xend >= clip_x1)  xend   = clip_x1 - 1;
                for (int x = xstart; x <= xend; ++x) { put_pixel(x, y, c); }
            }
        }
    }
    
    void scanline_fill_gradient(
        const std::vector<vector2d>& pts,
        const Gradient& grad
    ) noexcept {
        if (pts.size() < 3) return;
        double ymin_d = pts[0].y, ymax_d = pts[0].y;

        for (auto& p : pts) {
            if (p.y < ymin_d) ymin_d = p.y;
            if (p.y > ymax_d) ymax_d = p.y;
        }

        int ymin = static_cast<int>(std::floor(ymin_d));
        int ymax = static_cast<int>(std::ceil(ymax_d));
        const auto& clip = m_stack.back().clip;
        int clip_x0 = 0, clip_y0 = 0;
        int clip_x1 = static_cast<int>(m_fb->width());
        int clip_y1 = static_cast<int>(m_fb->height());

        if (clip.has_value()) {
            clip_x0 = clip->x;
            clip_y0 = clip->y;
            clip_x1 = clip->x + static_cast<int>(clip->w);
            clip_y1 = clip->y + static_cast<int>(clip->h);
        }

        if (ymin < clip_y0) ymin = clip_y0;
        if (ymax >= clip_y1) ymax = clip_y1 - 1;
        int n = static_cast<int>(pts.size());
        std::vector<double> xs;

        for (int y = ymin; y <= ymax; ++y) {
            xs.clear();
            double yd = y + 0.5;

            for (int i = 0; i < n; ++i) {
                int j = (i + 1) % n;
                double y0 = pts[i].y, y1 = pts[j].y;

                if ((y0 <= yd && y1 > yd) || (y1 <= yd && y0 > yd)) {
                    double t = (yd - y0) / (y1 - y0);
                    xs.push_back(pts[i].x + t * (pts[j].x - pts[i].x));
                }
            }

            std::sort(xs.begin(), xs.end());

            for (std::size_t k = 0; k + 1 < xs.size(); k += 2) {
                int xstart = static_cast<int>(std::ceil(xs[k]));
                int xend   = static_cast<int>(std::floor(xs[k + 1]));
                if (xstart < clip_x0) xstart = clip_x0;
                if (xend >= clip_x1)  xend   = clip_x1 - 1;

                for (int x = xstart; x <= xend; ++x) {
                    auto local = inverse_map_point(static_cast<double>(x), static_cast<double>(y));
                    Color cl = grad.sample(local.x, local.y);
                    if (cl.is_transparent()) continue;

                    if (!cl.is_opaque()) {
                        auto dst = m_fb->get_pixel(x, y);
                        cl = dst.blend_over(cl);
                    }

                    put_pixel(x, y, cl);
                }
            }
        }
    }

    void stroke_polygon(const std::array<vector2d, 4>& q, const Paint& p) noexcept {
        for (int i = 0; i < 4; ++i) {
            int ni = (i + 1) % 4;
            int ax = static_cast<int>(std::round(q[i].x));
            int ay = static_cast<int>(std::round(q[i].y));
            int bx = static_cast<int>(std::round(q[ni].x));
            int by = static_cast<int>(std::round(q[ni].y));

            if (p.stroke_width() <= 1)
                bresenham(ax, ay, bx, by, p.stroke_color());
            else
                thick_line(ax, ay, bx, by, p);
        }
    }

    void stroke_polyline(const std::vector<vector2d>& pts, const Paint& p, bool closed) noexcept {
        int n = static_cast<int>(pts.size());
        int limit = closed ? n : n - 1;

        for (int i = 0; i < limit; ++i) {
            int ni = (i + 1) % n;
            int ax = static_cast<int>(std::round(pts[i].x));
            int ay = static_cast<int>(std::round(pts[i].y));
            int bx = static_cast<int>(std::round(pts[ni].x));
            int by = static_cast<int>(std::round(pts[ni].y));

            if (p.stroke_width() <= 1)
                bresenham(ax, ay, bx, by, p.stroke_color());
            else
                thick_line(ax, ay, bx, by, p);
        }
    }

    static Color blend_over(const Color& src, const Color& dst, float alpha) noexcept {
        auto mix = [alpha](std::uint8_t s, std::uint8_t d) -> std::uint8_t {
            return static_cast<std::uint8_t>(std::round(s * alpha + d * (1.0f - alpha)));
        };

        return Color(
            mix(src.red(), dst.red()),
            mix(src.green(), dst.green()),
            mix(src.blue(),  dst.blue())
        );
    }

    struct TextParams {
        Color        fg;
        unsigned int scale;
        unsigned int gw;
        unsigned int gh;
        int          extra_spacing;
        bool         bold;
        std::size_t  num_cps;
    };

    TextParams resolve_text_params(
        const std::string& text,
        const text::TextStyle& style
    ) const noexcept {
        TextParams p;
        p.fg = style.has_fg_color() ? to_canvas_color(style.fg_color().value()) : Color(255, 255, 255);
        p.scale = 1;

        if (style.has_size() && style.size() > 0.0) {
            p.scale = std::max(1u, static_cast<unsigned int>(
                std::round(style.size() / text::BitmapFont::kGlyphH)
            ));
        }

        p.gw = text::BitmapFont::kGlyphW * p.scale;
        p.gh = text::BitmapFont::kGlyphH * p.scale;
        p.bold = style.has_weight() && text::is_bold(style.weight());
        p.extra_spacing = style.has_letter_spacing() ? static_cast<int>(std::round(style.letter_spacing())) : 0;
        p.num_cps = text::utf8::count_codepoints(text);
        return p;
    }

    static unsigned int text_line_width(
        std::size_t num_glyphs,
        unsigned int gw,
        int extra_spacing
    ) noexcept {
        if (num_glyphs == 0) return 0;
        return static_cast<unsigned int>(num_glyphs * gw + (num_glyphs - 1) * extra_spacing);
    }

    static Color to_canvas_color(const graphics::Color& c) noexcept {
        return Color(c.red(), c.green(), c.blue(), c.alpha());
    }

    void draw_text_line_glyphs(
        int x, int y,
        const std::string& text,
        const TextParams& params,
        const text::BitmapFont& font
    ) noexcept {
        int advance = static_cast<int>(params.gw) + params.extra_spacing;
        int cx = x;

        for (std::uint32_t cp : text::utf8::codepoints(text)) {
            draw_bitmap_glyph(cx, y, cp, font, params.scale, params.fg, params.bold);
            cx += advance;
        }
    }

    void draw_bitmap_glyph(
        int x, int y,
        std::uint32_t codepoint,
        const text::BitmapFont& font,
        unsigned int scale,
        const Color& color,
        bool bold
    ) noexcept {
        for (unsigned int gy = 0; gy < text::BitmapFont::kGlyphH; ++gy) {
            for (unsigned int gx = 0; gx < text::BitmapFont::kGlyphW; ++gx) {
                bool lit = font.pixel(codepoint, gx, gy);
                if (!lit && bold && gx > 0) { lit = font.pixel(codepoint, gx - 1, gy); }
                if (!lit) continue;

                for (unsigned int sy = 0; sy < scale; ++sy) {
                    for (unsigned int sx = 0; sx < scale; ++sx) {
                        int px = x + static_cast<int>(gx * scale + sx);
                        int py = y + static_cast<int>(gy * scale + sy);
                        auto [mx, my] = map_point(px, py);
                        int ix = static_cast<int>(std::round(mx));
                        int iy = static_cast<int>(std::round(my));

                        if (color.is_opaque()) {
                            put_pixel(ix, iy, color);
                        } else if (!color.is_transparent()) {
                            auto dst = m_fb->get_pixel(ix, iy);
                            put_pixel(ix, iy, dst.blend_over(color));
                        }
                    }
                }
            }
        }
    }

    static std::vector<std::vector<std::uint32_t>> word_wrap_codepoints(
        const std::vector<std::uint32_t>& cps,
        unsigned int max_per_line
    ) {
        std::vector<std::vector<std::uint32_t>> lines;
        std::vector<std::uint32_t> current_line;
        std::vector<std::vector<std::uint32_t>> words;
        std::vector<std::uint32_t> current_word;

        for (std::uint32_t cp : cps) {
            if (cp == '\n') {
                if (!current_word.empty()) {
                    words.push_back(std::move(current_word));
                    current_word.clear();
                }

                words.push_back({}); 
            } else if (cp == ' ') {
                if (!current_word.empty()) {
                    words.push_back(std::move(current_word));
                    current_word.clear();
                }
            } else {
                current_word.push_back(cp);
            }
        }

        if (!current_word.empty()) words.push_back(std::move(current_word));

        for (const auto& word : words) {
            if (word.empty()) {
                lines.push_back(std::move(current_line));
                current_line.clear();
                continue;
            }

            std::size_t space_needed = word.size();
            std::size_t with_space = current_line.empty() ? space_needed : current_line.size() + 1 + space_needed;

            if (with_space <= max_per_line) {
                if (!current_line.empty()) current_line.push_back(' ');
                current_line.insert(current_line.end(), word.begin(), word.end());
            } else if (current_line.empty()) {
                std::size_t pos = 0;

                while (pos < word.size()) {
                    std::size_t chunk = std::min(static_cast<std::size_t>(max_per_line), word.size() - pos);
                    current_line.assign(word.begin() + pos, word.begin() + pos + chunk);
                    pos += chunk;

                    if (pos < word.size()) {
                        lines.push_back(std::move(current_line));
                        current_line.clear();
                    }
                }
            } else {
                lines.push_back(std::move(current_line));
                current_line.clear();

                if (word.size() <= max_per_line) {
                    current_line.assign(word.begin(), word.end());
                } else {
                    std::size_t pos = 0;

                    while (pos < word.size()) {
                        std::size_t chunk = std::min(static_cast<std::size_t>(max_per_line), word.size() - pos);
                        current_line.assign(word.begin() + pos, word.begin() + pos + chunk);
                        pos += chunk;

                        if (pos < word.size()) {
                            lines.push_back(std::move(current_line));
                            current_line.clear();
                        }
                    }
                }
            }
        }

        if (!current_line.empty()) { lines.push_back(std::move(current_line)); }
        if (lines.empty()) lines.push_back({});
        return lines;
    }

    void put_pixel(int x, int y, const Color& c) noexcept {
        const auto& clip = m_stack.back().clip;

        if (clip.has_value()) {
            if (
                x < clip->x || y < clip->y ||
                x >= clip->x + static_cast<int>(clip->w) ||
                y >= clip->y + static_cast<int>(clip->h)
            ) return;
        }

        put_pixel(x, y, c);
    }
};

} // namespace graphics
} // namespace fizmo

#endif // FIZMO_CANVAS_HPP