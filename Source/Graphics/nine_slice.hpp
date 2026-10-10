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

    static void emit(std::vector<SliceQuad>& out, double x, double y, double w, double h, const TextureRect& src, SliceFill fill_x, SliceFill fill_y);

public:
    NineSlice() = default;

    NineSlice(const Texture& texture, unsigned int border) : NineSlice(texture, border, border, border, border) {}

    NineSlice(const Texture& texture, unsigned int left, unsigned int top, unsigned int right, unsigned int bottom)
;

    NineSlice(const Texture& texture, const TextureRect& source, unsigned int left, unsigned int top, unsigned int right, unsigned int bottom)
;

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
    bool valid() const noexcept;

    void build(double x, double y, double w, double h, std::vector<SliceQuad>& out) const;
};

} // namespace graphics
} // namespace fizmo

#endif // FIZMO_NINE_SLICE_HPP
