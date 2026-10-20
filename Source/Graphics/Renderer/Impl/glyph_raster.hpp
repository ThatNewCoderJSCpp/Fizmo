#ifndef FIZMO_GLYPH_RASTER_HPP
#define FIZMO_GLYPH_RASTER_HPP

#include <cstdint>
#include <vector>

namespace fizmo {
namespace graphics {
namespace detail {
namespace gfx {

class OutlineRasterizer {
public:
    void reset(int width, int height, float origin_x, float origin_y);

    void move_to(float x, float y);
    void line_to(float x, float y);
    void quad_to(float cx, float cy, float x, float y);
    void cubic_to(float c1x, float c1y, float c2x, float c2y, float x, float y);
    void close();

    void coverage(std::vector<std::uint8_t>& out) const;

    int width() const noexcept { return m_width; }
    int height() const noexcept { return m_height; }

private:
    void segment(float x0, float y0, float x1, float y1);

    std::vector<float> m_acc;
    int                m_width   = 0;
    int                m_height  = 0;
    int                m_stride  = 0;
    float              m_ox      = 0.0f;
    float              m_oy      = 0.0f;
    float              m_start_x = 0.0f;
    float              m_start_y = 0.0f;
    float              m_x       = 0.0f;
    float              m_y       = 0.0f;
    bool               m_open    = false;
};

void downscale_rgba(const std::uint8_t* src, int sw, int sh, int src_stride, bool bgra, float scale, std::vector<std::uint8_t>& out, int& dw, int& dh);

} // namespace gfx
} // namespace detail
} // namespace graphics
} // namespace fizmo

#endif // FIZMO_GLYPH_RASTER_HPP
