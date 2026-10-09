#ifndef FIZMO_NINE_SLICE_HPP
#define FIZMO_NINE_SLICE_HPP

#include "texture.hpp"
#include <algorithm>
#include <cmath>
#include <vector>

namespace fizmo {
namespace graphics {

enum class SliceFill : std::uint8_t { Stretch = 0, Tile };

struct SliceQuad {
    double      x = 0.0, y = 0.0, w = 0.0, h = 0.0;
    TextureRect source;
};

class NineSlice {
private:
    Texture      m_texture;
    TextureRect  m_source;
    unsigned int m_left = 0, m_top = 0, m_right = 0, m_bottom = 0;
    SliceFill    m_center = SliceFill::Stretch;
    SliceFill    m_edges  = SliceFill::Stretch;
    double       m_border_scale = 1.0;
    bool         m_draw_center  = true;

    static void emit(std::vector<SliceQuad>& out, double x, double y, double w, double h, const TextureRect& src, SliceFill fill_x, SliceFill fill_y) {
        if (w <= 0.0 || h <= 0.0 || src.is_empty()) return;
        const double tw = fill_x == SliceFill::Tile ? static_cast<double>(src.w) : w;
        const double th = fill_y == SliceFill::Tile ? static_cast<double>(src.h) : h;

        for (double oy = 0.0; oy < h - 1e-9; oy += th) {
            const double ch = std::min(th, h - oy);
            const unsigned int sh = fill_y == SliceFill::Tile ? std::max(1u, static_cast<unsigned int>(std::lround(src.h * (ch / th)))) : src.h;

            for (double ox = 0.0; ox < w - 1e-9; ox += tw) {
                const double cw = std::min(tw, w - ox);
                const unsigned int sw = fill_x == SliceFill::Tile ? std::max(1u, static_cast<unsigned int>(std::lround(src.w * (cw / tw)))) : src.w;
                out.push_back({ x + ox, y + oy, cw, ch, TextureRect(src.x, src.y, sw, sh) });
            }
        }
    }

public:
    NineSlice() = default;

    NineSlice(const Texture& texture, unsigned int border) : NineSlice(texture, border, border, border, border) {}

    NineSlice(const Texture& texture, unsigned int left, unsigned int top, unsigned int right, unsigned int bottom)
        : m_texture(texture), m_source(texture.full_rect()), m_left(left), m_top(top), m_right(right), m_bottom(bottom) {}

    NineSlice(const Texture& texture, const TextureRect& source, unsigned int left, unsigned int top, unsigned int right, unsigned int bottom)
        : m_texture(texture), m_source(source), m_left(left), m_top(top), m_right(right), m_bottom(bottom) {}

    const Texture& texture() const noexcept { return m_texture; }
    const TextureRect& source() const noexcept { return m_source; }
    unsigned int left() const noexcept { return m_left; }
    unsigned int top() const noexcept { return m_top; }
    unsigned int right() const noexcept { return m_right; }
    unsigned int bottom() const noexcept { return m_bottom; }

    NineSlice& set_texture(const Texture& t) { m_texture = t; m_source = t.full_rect(); return *this; }
    NineSlice& set_source(const TextureRect& r) noexcept { m_source = r; return *this; }
    NineSlice& set_borders(unsigned int l, unsigned int t, unsigned int r, unsigned int b) noexcept { m_left = l; m_top = t; m_right = r; m_bottom = b; return *this; }
    NineSlice& set_center_fill(SliceFill f) noexcept { m_center = f; return *this; }
    NineSlice& set_edge_fill(SliceFill f) noexcept { m_edges = f; return *this; }
    NineSlice& set_border_scale(double s) noexcept { m_border_scale = s > 0.0 ? s : 1.0; return *this; }
    NineSlice& set_draw_center(bool d) noexcept { m_draw_center = d; return *this; }

    double min_width() const noexcept { return (m_left + m_right) * m_border_scale; }
    double min_height() const noexcept { return (m_top + m_bottom) * m_border_scale; }
    bool valid() const noexcept { return m_texture.valid() && !m_source.is_empty() && m_left + m_right <= m_source.w && m_top + m_bottom <= m_source.h; }

    void build(double x, double y, double w, double h, std::vector<SliceQuad>& out) const {
        out.clear();
        if (!valid() || w <= 0.0 || h <= 0.0) return;
        double l = m_left * m_border_scale, r = m_right * m_border_scale, t = m_top * m_border_scale, b = m_bottom * m_border_scale;
        if (l + r > w) { const double k = w / (l + r); l *= k; r *= k; }
        if (t + b > h) { const double k = h / (t + b); t *= k; b *= k; }
        const double cw = w - l - r, ch = h - t - b;
        const unsigned int sw = m_source.w - m_left - m_right, sh = m_source.h - m_top - m_bottom;
        const int sx0 = m_source.x, sx1 = m_source.x + static_cast<int>(m_left), sx2 = m_source.x + static_cast<int>(m_source.w - m_right);
        const int sy0 = m_source.y, sy1 = m_source.y + static_cast<int>(m_top), sy2 = m_source.y + static_cast<int>(m_source.h - m_bottom);
        const double xs[3] = { x, x + l, x + l + cw };
        const double ys[3] = { y, y + t, y + t + ch };
        const double ws[3] = { l, cw, r };
        const double hs[3] = { t, ch, b };
        const int sxs[3] = { sx0, sx1, sx2 };
        const int sys[3] = { sy0, sy1, sy2 };
        const unsigned int sws[3] = { m_left, sw, m_right };
        const unsigned int shs[3] = { m_top, sh, m_bottom };
        const double bs = m_border_scale;

        for (int row = 0; row < 3; ++row) {
            for (int col = 0; col < 3; ++col) {
                if (row == 1 && col == 1 && !m_draw_center) continue;
                const TextureRect src(sxs[col], sys[row], sws[col], shs[row]);
                SliceFill fx = SliceFill::Stretch, fy = SliceFill::Stretch;
                if (row == 1 && col == 1) { fx = fy = m_center; }
                else if (row == 1) fy = m_edges;
                else if (col == 1) fx = m_edges;

                if (fx == SliceFill::Tile || fy == SliceFill::Tile) {
                    const std::size_t first = out.size();
                    emit(out, xs[col] / bs, ys[row] / bs, ws[col] / bs, hs[row] / bs, src, fx, fy);
                    for (std::size_t i = first; i < out.size(); ++i) { out[i].x *= bs; out[i].y *= bs; out[i].w *= bs; out[i].h *= bs; }
                } else {
                    emit(out, xs[col], ys[row], ws[col], hs[row], src, fx, fy);
                }
            }
        }
    }
};

} // namespace graphics
} // namespace fizmo

#endif // FIZMO_NINE_SLICE_HPP
