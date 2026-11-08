#include "fizmo_library.hpp"
#include "fluid_2d.hpp"

namespace fizmo {
namespace physics {

namespace {

constexpr Real kPi = 3.14159265358979323846_r;

float smooth(float e0, float e1, float x) noexcept {
    if (e1 <= e0) return x >= e0 ? 1.0f : 0.0f;
    const float t = std::clamp((x - e0) / (e1 - e0), 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

} // namespace

void FluidRenderer2D::setup(const AABB& fallback, const FluidImageSettings2D& s) {
    const bool has_region = s.region.max.x > s.region.min.x && s.region.max.y > s.region.min.y;
    m_region = has_region ? s.region : fallback;
    if (!(m_region.max.x > m_region.min.x)) m_region.max.x = m_region.min.x + 1.0_r;
    if (!(m_region.max.y > m_region.min.y)) m_region.max.y = m_region.min.y + 1.0_r;
    const Real rw = m_region.max.x - m_region.min.x, rh = m_region.max.y - m_region.min.y;
    m_w = std::max(1, s.width);
    m_h = s.height > 0 ? s.height : std::max(1, static_cast<int>(std::round(static_cast<Real>(m_w) * rh / rw)));
    m_px = rw / static_cast<Real>(m_w);
    m_py = rh / static_cast<Real>(m_h);
    m_flip = s.flip_y;
    const std::size_t n = static_cast<std::size_t>(m_w) * static_cast<std::size_t>(m_h);
    m_rgba.resize(n * 4);
    m_density.assign(n, 0.0f);
    m_color.assign(n * 4, 0.0f);
}

void FluidRenderer2D::put(std::size_t p, float r, float g, float b, float a, const FluidImageSettings2D& s) {
    const FluidColor& bg = s.background;
    const float oa = a + bg.a * (1.0f - a);
    float orr = 0.0f, og = 0.0f, ob = 0.0f;
    if (oa > 1e-6f) {
        orr = (r * a + bg.r * bg.a * (1.0f - a)) / oa;
        og = (g * a + bg.g * bg.a * (1.0f - a)) / oa;
        ob = (b * a + bg.b * bg.a * (1.0f - a)) / oa;
    }
    if (s.premultiplied) {
        orr *= oa;
        og *= oa;
        ob *= oa;
    }
    std::uint8_t* o = &m_rgba[p * 4];
    o[0] = static_cast<std::uint8_t>(std::clamp(orr, 0.0f, 1.0f) * 255.0f + 0.5f);
    o[1] = static_cast<std::uint8_t>(std::clamp(og, 0.0f, 1.0f) * 255.0f + 0.5f);
    o[2] = static_cast<std::uint8_t>(std::clamp(ob, 0.0f, 1.0f) * 255.0f + 0.5f);
    o[3] = static_cast<std::uint8_t>(std::clamp(oa, 0.0f, 1.0f) * 255.0f + 0.5f);
}

void FluidRenderer2D::splat(const std::vector<RVec2>& pos, const std::vector<std::uint32_t>& colors, Real kernel) {
    const Real inv_px = 1.0_r / m_px, inv_py = 1.0_r / m_py;
    const Real kr2 = kernel * kernel;
    const int rx = static_cast<int>(std::ceil(kernel * inv_px));
    const int ry = static_cast<int>(std::ceil(kernel * inv_py));
    const bool have_colors = colors.size() == pos.size();
    for (std::size_t p = 0; p < pos.size(); ++p) {
        const Real fx = (pos[p].x - m_region.min.x) * inv_px - 0.5_r;
        const Real fy0 = (pos[p].y - m_region.min.y) * inv_py - 0.5_r;
        const int cx = static_cast<int>(std::floor(fx + 0.5_r));
        const int cy = static_cast<int>(std::floor(fy0 + 0.5_r));
        if (cx + rx < 0 || cx - rx >= m_w || cy + ry < 0 || cy - ry >= m_h) continue;
        float cr = 0.2f, cg = 0.45f, cb = 0.8f, ca = 1.0f;
        if (have_colors) {
            const std::uint32_t c = colors[p];
            cr = static_cast<float>(c & 255u) * (1.0f / 255.0f);
            cg = static_cast<float>((c >> 8) & 255u) * (1.0f / 255.0f);
            cb = static_cast<float>((c >> 16) & 255u) * (1.0f / 255.0f);
            ca = static_cast<float>((c >> 24) & 255u) * (1.0f / 255.0f);
        }
        for (int y = std::max(0, cy - ry); y <= std::min(m_h - 1, cy + ry); ++y) {
            const Real dy = (static_cast<Real>(y) - fy0) * m_py;
            const Real dy2 = dy * dy;
            if (dy2 >= kr2) continue;
            const int row = m_flip ? (m_h - 1 - y) : y;
            for (int x = std::max(0, cx - rx); x <= std::min(m_w - 1, cx + rx); ++x) {
                const Real dx = (static_cast<Real>(x) - fx) * m_px;
                const Real q = (dx * dx + dy2) / kr2;
                if (q >= 1.0_r) continue;
                const float w = static_cast<float>((1.0_r - q) * (1.0_r - q));
                const std::size_t i = static_cast<std::size_t>(row) * static_cast<std::size_t>(m_w) + static_cast<std::size_t>(x);
                m_density[i] += w;
                float* c = &m_color[i * 4];
                c[0] += w * cr;
                c[1] += w * cg;
                c[2] += w * cb;
                c[3] += w * ca;
            }
        }
    }
}

void FluidRenderer2D::shade(const FluidImageSettings2D& s, Real level) {
    const std::size_t n = m_density.size();
    const float th = static_cast<float>(s.threshold);
    const float soft = static_cast<float>(std::max(s.softness, 0.0_r));
    const float lv = static_cast<float>(level);
    for (std::size_t p = 0; p < n; ++p) {
        const float raw = m_density[p];
        const float d = raw * lv;
        const float* c = &m_color[p * 4];
        float r = 0.0f, g = 0.0f, b = 0.0f, a = 0.0f;
        if (raw > 1e-8f) {
            const float k = 1.0f / raw;
            r = c[0] * k;
            g = c[1] * k;
            b = c[2] * k;
            a = c[3] * k;
            if (!s.blend_colors) a = std::max(a, 0.0f);
        }
        float cover = smooth(th - 0.5f * soft, th + 0.5f * soft, d);
        if (s.shade_edges && cover > 0.0f) {
            const float inner = smooth(th, th + 0.8f, d);
            const float f = 1.0f - static_cast<float>(s.edge_darken) * (1.0f - inner);
            r *= f;
            g *= f;
            b *= f;
            a = std::min(1.0f, a + 0.25f * (1.0f - inner));
        }
        m_density[p] = d;
        put(p, r, g, b, cover * a * static_cast<float>(s.opacity), s);
    }
}

const std::vector<std::uint8_t>& FluidRenderer2D::render_particles(const std::vector<RVec2>& pos, const std::vector<std::uint32_t>& colors, Real radius, Real rest_count, const FluidImageSettings2D& s) {
    AABB fallback{};
    if (!pos.empty()) {
        fallback = AABB(pos[0], pos[0]);
        for (const RVec2& p : pos) fallback = AABB::merge(fallback, AABB(p, p));
        fallback = fallback.fatten(radius * 3.0_r);
    }
    setup(fallback, s);
    const Real r = std::max(radius, 1e-6_r);
    const Real kernel = s.kernel_radius > 0.0_r ? s.kernel_radius : 2.4_r * r;
    splat(pos, colors, kernel);
    const Real area = (2.0_r * r) * (2.0_r * r) / std::max(rest_count, 1e-6_r);
    const Real integral = kPi * kernel * kernel / 3.0_r;
    shade(s, area / integral);
    return m_rgba;
}

const std::vector<std::uint8_t>& FluidRenderer2D::render_cells(const FluidVolume2D& v, const FluidImageSettings2D& s) {
    FluidImageSettings2D st = s;
    if (st.width <= 0) st.width = std::max(1, v.level() == FluidLevel::waves ? v.columns() : v.cells_x());
    setup(v.bounds(), st);
    const std::size_t n = static_cast<std::size_t>(m_w) * static_cast<std::size_t>(m_h);
    const FluidMaterial& m = v.material();
    for (int y = 0; y < m_h; ++y) {
        const int row = m_flip ? (m_h - 1 - y) : y;
        const Real wy = m_region.min.y + (static_cast<Real>(y) + 0.5_r) * m_py;
        for (int x = 0; x < m_w; ++x) {
            const Real wx = m_region.min.x + (static_cast<Real>(x) + 0.5_r) * m_px;
            const RVec2 w{ wx, wy };
            const std::size_t p = static_cast<std::size_t>(row) * static_cast<std::size_t>(m_w) + static_cast<std::size_t>(x);
            float r = 0.0f, g = 0.0f, b = 0.0f, a = 0.0f, d = 0.0f;
            switch (v.level()) {
            case FluidLevel::blocky: {
                int i = 0, j = 0;
                if (!v.cell_of(w, i, j)) break;
                const std::size_t c = v.cell_index(i, j);
                const SandCell2D cell = static_cast<SandCell2D>(v.sand_cells()[c]);
                if (cell == SandCell2D::empty) break;
                const FluidColor col = fluid_unpack_color(sand_cell_color(cell, st.cell_noise ? v.cell_shade()[c] : static_cast<std::uint8_t>(128)));
                r = col.r;
                g = col.g;
                b = col.b;
                a = col.a;
                d = sand_cell_info(cell).kind == SandKind2D::liquid ? 1.0f : 0.0f;
                break;
            }
            case FluidLevel::flowing: {
                int i = 0, j = 0;
                if (!v.cell_of(w, i, j)) break;
                const std::size_t c = v.cell_index(i, j);
                if (v.cell_solids()[c]) {
                    r = 0.45f;
                    g = 0.45f;
                    b = 0.48f;
                    a = 1.0f;
                    break;
                }
                const RVec2 l = v.to_local(w);
                const Real fill = std::min(v.cell_fill()[c], 1.0_r);
                const Real in_cell = l.y / v.cell_size() - static_cast<Real>(j);
                const bool full_above = v.valid_cell(i, j + 1) && v.cell_fill()[v.cell_index(i, j + 1)] > 0.05_r;
                if (fill <= 0.0_r || (!full_above && in_cell > fill)) break;
                r = m.color.r;
                g = m.color.g;
                b = m.color.b;
                a = m.color.a;
                d = 1.0f;
                break;
            }
            case FluidLevel::waves:
            case FluidLevel::still:
            case FluidLevel::realistic: {
                const Real depth = v.depth_at(w);
                if (depth <= 0.0_r) break;
                r = m.color.r;
                g = m.color.g;
                b = m.color.b;
                a = m.color.a;
                d = 1.0f;
                break;
            }
            }
            m_density[p] = d;
            put(p, r, g, b, a * static_cast<float>(st.opacity), st);
        }
    }
    static_cast<void>(n);
    return m_rgba;
}

const std::vector<std::uint8_t>& FluidRenderer2D::render(const FluidVolume2D& v, const FluidImageSettings2D& s) {
    if (v.level() == FluidLevel::realistic) {
        FluidImageSettings2D st = s;
        const bool has_region = s.region.max.x > s.region.min.x && s.region.max.y > s.region.min.y;
        if (!has_region) st.region = v.bounds();
        return render_particles(v.positions(), v.colors(), v.particle_radius(), 1.0_r, st);
    }
    return render_cells(v, s);
}

std::size_t FluidRenderer2D::contour(Real threshold, std::vector<RVec2>& out) const {
    out.clear();
    if (m_w < 2 || m_h < 2 || m_density.size() != static_cast<std::size_t>(m_w) * static_cast<std::size_t>(m_h)) return 0;
    const float t = static_cast<float>(threshold);
    auto at = [&](int x, int y) { return m_density[static_cast<std::size_t>(y) * static_cast<std::size_t>(m_w) + static_cast<std::size_t>(x)]; };
    auto world = [&](Real x, Real y) {
        const Real wx = m_region.min.x + (x + 0.5_r) * m_px;
        const Real wy = m_flip ? m_region.max.y - (y + 0.5_r) * m_py : m_region.min.y + (y + 0.5_r) * m_py;
        return RVec2{ wx, wy };
    };
    auto lerp = [&](float a, float b) -> Real {
        const float d = b - a;
        return std::abs(d) > 1e-12f ? static_cast<Real>(std::clamp((t - a) / d, 0.0f, 1.0f)) : 0.5_r;
    };
    for (int y = 0; y + 1 < m_h; ++y) {
        for (int x = 0; x + 1 < m_w; ++x) {
            const float v0 = at(x, y), v1 = at(x + 1, y), v2 = at(x + 1, y + 1), v3 = at(x, y + 1);
            const int code = (v0 >= t ? 1 : 0) | (v1 >= t ? 2 : 0) | (v2 >= t ? 4 : 0) | (v3 >= t ? 8 : 0);
            if (code == 0 || code == 15) continue;
            const Real fx = static_cast<Real>(x), fy = static_cast<Real>(y);
            const RVec2 e0 = world(fx + lerp(v0, v1), fy);
            const RVec2 e1 = world(fx + 1.0_r, fy + lerp(v1, v2));
            const RVec2 e2 = world(fx + lerp(v3, v2), fy + 1.0_r);
            const RVec2 e3 = world(fx, fy + lerp(v0, v3));
            auto seg = [&](const RVec2& a, const RVec2& b) {
                out.push_back(a);
                out.push_back(b);
            };
            const bool center = 0.25f * (v0 + v1 + v2 + v3) >= t;
            switch (code) {
            case 1: case 14: seg(e3, e0); break;
            case 2: case 13: seg(e0, e1); break;
            case 3: case 12: seg(e3, e1); break;
            case 4: case 11: seg(e1, e2); break;
            case 6: case 9: seg(e0, e2); break;
            case 7: case 8: seg(e3, e2); break;
            case 5:
                if (center) { seg(e3, e2); seg(e0, e1); }
                else { seg(e3, e0); seg(e1, e2); }
                break;
            case 10:
                if (center) { seg(e3, e0); seg(e1, e2); }
                else { seg(e0, e1); seg(e3, e2); }
                break;
            default: break;
            }
        }
    }
    return out.size() / 2;
}

} // namespace physics
} // namespace fizmo
