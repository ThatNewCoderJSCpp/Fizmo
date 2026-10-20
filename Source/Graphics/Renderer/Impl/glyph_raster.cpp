#include "glyph_raster.hpp"

#include <algorithm>
#include <cmath>

namespace fizmo {
namespace graphics {
namespace detail {
namespace gfx {

void OutlineRasterizer::reset(int width, int height, float origin_x, float origin_y) {
    m_width  = std::max(0, width);
    m_height = std::max(0, height);
    m_stride = m_width + 3;
    m_ox     = origin_x;
    m_oy     = origin_y;
    m_acc.assign(static_cast<std::size_t>(m_stride) * static_cast<std::size_t>(m_height), 0.0f);
    m_open   = false;
}

void OutlineRasterizer::move_to(float x, float y) {
    close();
    m_start_x = m_x = x - m_ox;
    m_start_y = m_y = y - m_oy;
    m_open    = true;
}

void OutlineRasterizer::line_to(float x, float y) {
    x -= m_ox;
    y -= m_oy;
    segment(m_x, m_y, x, y);
    m_x = x;
    m_y = y;
}

void OutlineRasterizer::quad_to(float cx, float cy, float x, float y) {
    cx -= m_ox;
    cy -= m_oy;
    x  -= m_ox;
    y  -= m_oy;
    const float x0 = m_x, y0 = m_y;
    const float devx = x0 - 2.0f * cx + x;
    const float devy = y0 - 2.0f * cy + y;
    const float dev = std::sqrt(devx * devx + devy * devy);
    if (dev < 0.08f) {
        segment(x0, y0, x, y);
    } else {
        const int n = std::min(64, static_cast<int>(std::ceil(std::sqrt(dev * 12.5f))));
        const float step = 1.0f / static_cast<float>(n);
        float px = x0, py = y0;
        for (int i = 1; i <= n; ++i) {
            const float t = i == n ? 1.0f : step * static_cast<float>(i);
            const float u = 1.0f - t;
            const float nx = u * u * x0 + 2.0f * u * t * cx + t * t * x;
            const float ny = u * u * y0 + 2.0f * u * t * cy + t * t * y;
            segment(px, py, nx, ny);
            px = nx;
            py = ny;
        }
    }
    m_x = x;
    m_y = y;
}

void OutlineRasterizer::cubic_to(float c1x, float c1y, float c2x, float c2y, float x, float y) {
    c1x -= m_ox;
    c1y -= m_oy;
    c2x -= m_ox;
    c2y -= m_oy;
    x   -= m_ox;
    y   -= m_oy;
    const float x0 = m_x, y0 = m_y;
    const float ax = x0 - 2.0f * c1x + c2x, ay = y0 - 2.0f * c1y + c2y;
    const float bx = c1x - 2.0f * c2x + x, by = c1y - 2.0f * c2y + y;
    const float dev = std::sqrt(std::max(ax * ax + ay * ay, bx * bx + by * by));
    if (dev < 0.03f) {
        segment(x0, y0, x, y);
    } else {
        const int n = std::min(96, static_cast<int>(std::ceil(std::sqrt(dev * 37.5f))));
        const float step = 1.0f / static_cast<float>(n);
        float px = x0, py = y0;
        for (int i = 1; i <= n; ++i) {
            const float t = i == n ? 1.0f : step * static_cast<float>(i);
            const float u = 1.0f - t;
            const float a = u * u * u, b = 3.0f * u * u * t, c = 3.0f * u * t * t, d = t * t * t;
            const float nx = a * x0 + b * c1x + c * c2x + d * x;
            const float ny = a * y0 + b * c1y + c * c2y + d * y;
            segment(px, py, nx, ny);
            px = nx;
            py = ny;
        }
    }
    m_x = x;
    m_y = y;
}

void OutlineRasterizer::close() {
    if (!m_open) return;
    if (m_x != m_start_x || m_y != m_start_y) segment(m_x, m_y, m_start_x, m_start_y);
    m_x    = m_start_x;
    m_y    = m_start_y;
    m_open = false;
}

void OutlineRasterizer::segment(float x0, float y0, float x1, float y1) {
    if (y0 == y1 || m_height == 0) return;
    float dir = 1.0f;
    if (y0 > y1) {
        std::swap(x0, x1);
        std::swap(y0, y1);
        dir = -1.0f;
    }
    if (y1 <= 0.0f || y0 >= static_cast<float>(m_height)) return;
    const float limit = static_cast<float>(m_width) + 0.999f;
    x0 = std::clamp(x0, 0.0f, limit);
    x1 = std::clamp(x1, 0.0f, limit);
    const float dxdy = (x1 - x0) / (y1 - y0);
    float x = x0;
    if (y0 < 0.0f) {
        x -= y0 * dxdy;
        y0 = 0.0f;
    }
    const float bottom = std::min(y1, static_cast<float>(m_height));
    const int first = static_cast<int>(y0);
    const int last  = std::min(m_height, static_cast<int>(std::ceil(bottom)));
    for (int yi = first; yi < last; ++yi) {
        float* row = &m_acc[static_cast<std::size_t>(yi) * static_cast<std::size_t>(m_stride)];
        const float fy = static_cast<float>(yi);
        const float dy = std::min(fy + 1.0f, bottom) - std::max(fy, y0);
        if (dy <= 0.0f) continue;
        const float xnext = std::clamp(x + dxdy * dy, 0.0f, limit);
        const float d = dy * dir;
        const float xa = std::min(x, xnext);
        const float xb = std::max(x, xnext);
        const float xa_floor = std::floor(xa);
        const int xai = static_cast<int>(xa_floor);
        const float xb_ceil = std::ceil(xb);
        const int xbi = static_cast<int>(xb_ceil);
        if (xbi <= xai + 1) {
            const float xmf = 0.5f * (x + xnext) - xa_floor;
            row[xai] += d - d * xmf;
            row[xai + 1] += d * xmf;
        } else {
            const float s = 1.0f / (xb - xa);
            const float xaf = xa - xa_floor;
            const float a0 = 0.5f * s * (1.0f - xaf) * (1.0f - xaf);
            const float xbf = xb - xb_ceil + 1.0f;
            const float am = 0.5f * s * xbf * xbf;
            row[xai] += d * a0;
            if (xbi == xai + 2) {
                row[xai + 1] += d * (1.0f - a0 - am);
            } else {
                const float a1 = s * (1.5f - xaf);
                row[xai + 1] += d * (a1 - a0);
                for (int xi = xai + 2; xi < xbi - 1; ++xi) row[xi] += d * s;
                const float a2 = a1 + static_cast<float>(xbi - xai - 3) * s;
                row[xbi - 1] += d * (1.0f - a2 - am);
            }
            row[xbi] += d * am;
        }
        x = xnext;
    }
}

void OutlineRasterizer::coverage(std::vector<std::uint8_t>& out) const {
    out.assign(static_cast<std::size_t>(m_width) * static_cast<std::size_t>(m_height), 0);
    for (int y = 0; y < m_height; ++y) {
        const float* row = &m_acc[static_cast<std::size_t>(y) * static_cast<std::size_t>(m_stride)];
        std::uint8_t* dst = &out[static_cast<std::size_t>(y) * static_cast<std::size_t>(m_width)];
        float sum = 0.0f;
        for (int x = 0; x < m_width; ++x) {
            sum += row[x];
            const float a = std::min(1.0f, std::fabs(sum));
            dst[x] = static_cast<std::uint8_t>(a * 255.0f + 0.5f);
        }
    }
}

void downscale_rgba(const std::uint8_t* src, int sw, int sh, int src_stride, bool bgra, float scale, std::vector<std::uint8_t>& out, int& dw, int& dh) {
    dw = std::max(1, static_cast<int>(std::lround(sw * scale)));
    dh = std::max(1, static_cast<int>(std::lround(sh * scale)));
    out.assign(static_cast<std::size_t>(dw) * static_cast<std::size_t>(dh) * 4, 0);
    const float fx = static_cast<float>(sw) / static_cast<float>(dw);
    const float fy = static_cast<float>(sh) / static_cast<float>(dh);
    const int r = bgra ? 2 : 0;
    const int b = bgra ? 0 : 2;
    for (int y = 0; y < dh; ++y) {
        const float sy0 = y * fy, sy1 = (y + 1) * fy;
        for (int x = 0; x < dw; ++x) {
            const float sx0 = x * fx, sx1 = (x + 1) * fx;
            float acc[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
            float total = 0.0f;
            for (int yy = static_cast<int>(sy0); yy < sh && static_cast<float>(yy) < sy1; ++yy) {
                const float wy = std::min(sy1, static_cast<float>(yy + 1)) - std::max(sy0, static_cast<float>(yy));
                if (wy <= 0.0f) continue;
                const std::uint8_t* line = src + static_cast<std::size_t>(yy) * static_cast<std::size_t>(src_stride);
                for (int xx = static_cast<int>(sx0); xx < sw && static_cast<float>(xx) < sx1; ++xx) {
                    const float wx = std::min(sx1, static_cast<float>(xx + 1)) - std::max(sx0, static_cast<float>(xx));
                    if (wx <= 0.0f) continue;
                    const float w = wx * wy;
                    const std::uint8_t* p = line + static_cast<std::size_t>(xx) * 4;
                    acc[0] += p[r] * w;
                    acc[1] += p[1] * w;
                    acc[2] += p[b] * w;
                    acc[3] += p[3] * w;
                    total += w;
                }
            }
            std::uint8_t* d = &out[(static_cast<std::size_t>(y) * static_cast<std::size_t>(dw) + static_cast<std::size_t>(x)) * 4];
            if (total <= 0.0f) continue;
            for (int k = 0; k < 4; ++k) d[k] = static_cast<std::uint8_t>(std::min(255.0f, acc[k] / total + 0.5f));
        }
    }
}

} // namespace gfx
} // namespace detail
} // namespace graphics
} // namespace fizmo
