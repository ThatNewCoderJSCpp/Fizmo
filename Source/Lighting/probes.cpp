#include "probes.hpp"

namespace fizmo {
namespace lighting {

void ProbeVolume::configure(const Vec3& lo, const Vec3& hi, float spacing, std::size_t max_probes) {
    m_requested = spacing;
    const Vec3f a(lo), b(hi);
    m_req_lo = a;
    m_req_hi = b;
    float cell = std::max(spacing, 0.05f);
    const Vec3f size = vmax(b - a, Vec3f{ 1e-3f, 1e-3f, 1e-3f });
    for (int guard = 0; guard < 64; ++guard) {
        const std::size_t count = static_cast<std::size_t>(std::floor(size.x / cell) + 2) * static_cast<std::size_t>(std::floor(size.y / cell) + 2) * static_cast<std::size_t>(std::floor(size.z / cell) + 2);
        if (count <= std::max<std::size_t>(max_probes, 8)) break;
        cell *= 1.25f;
    }
    m_nx = static_cast<int>(std::floor(size.x / cell)) + 2;
    m_ny = static_cast<int>(std::floor(size.y / cell)) + 2;
    m_nz = static_cast<int>(std::floor(size.z / cell)) + 2;
    const Vec3f center = (a + b) * 0.5f;
    const Vec3f span{ static_cast<float>(m_nx - 1) * cell, static_cast<float>(m_ny - 1) * cell, static_cast<float>(m_nz - 1) * cell };
    m_lo = center - span * 0.5f;
    m_hi = center + span * 0.5f;
    m_cell = { cell, cell, cell };
    m_sh.assign(static_cast<std::size_t>(m_nx) * static_cast<std::size_t>(m_ny) * static_cast<std::size_t>(m_nz), Sh9());
    m_live.assign(m_sh.size(), 1);
    m_updates = 0;
}

void ProbeVolume::clear() {
    m_sh.clear();
    m_live.clear();
    m_nx = m_ny = m_nz = 0;
    m_updates = 0;
}

bool ProbeVolume::matches(const Vec3f& lo, const Vec3f& hi, float spacing) const noexcept {
    if (m_sh.empty() || spacing != m_requested) return false;
    const float slack = m_cell.x * 0.5f;
    return lo.x >= m_lo.x - slack && lo.y >= m_lo.y - slack && lo.z >= m_lo.z - slack && hi.x <= m_hi.x + slack && hi.y <= m_hi.y + slack && hi.z <= m_hi.z + slack;
}

Vec3f ProbeVolume::position_f(std::size_t i) const noexcept {
    const std::size_t nx = static_cast<std::size_t>(m_nx), ny = static_cast<std::size_t>(m_ny);
    const std::size_t x = i % nx, y = (i / nx) % ny, z = i / (nx * ny);
    return { m_lo.x + static_cast<float>(x) * m_cell.x, m_lo.y + static_cast<float>(y) * m_cell.y, m_lo.z + static_cast<float>(z) * m_cell.z };
}

Rgb ProbeVolume::irradiance_f(const Vec3f& p_in, const Vec3f& n) const noexcept {
    if (m_sh.empty()) return Rgb();
    const Vec3f p = p_in + n * (0.25f * m_cell.x);
    const Vec3f f{ (p.x - m_lo.x) / m_cell.x, (p.y - m_lo.y) / m_cell.y, (p.z - m_lo.z) / m_cell.z };
    const int x0 = std::clamp(static_cast<int>(std::floor(f.x)), 0, m_nx - 2);
    const int y0 = std::clamp(static_cast<int>(std::floor(f.y)), 0, m_ny - 2);
    const int z0 = std::clamp(static_cast<int>(std::floor(f.z)), 0, m_nz - 2);
    const float tx = clamp01(f.x - static_cast<float>(x0)), ty = clamp01(f.y - static_cast<float>(y0)), tz = clamp01(f.z - static_cast<float>(z0));
    Rgb sum;
    float wsum = 0.0f;
    Rgb fallback;
    float fallback_w = 0.0f;
    for (int c = 0; c < 8; ++c) {
        const int dx = c & 1, dy = (c >> 1) & 1, dz = (c >> 2) & 1;
        const std::size_t i = index(x0 + dx, y0 + dy, z0 + dz);
        const float tri = (dx ? tx : 1.0f - tx) * (dy ? ty : 1.0f - ty) * (dz ? tz : 1.0f - tz);
        const Vec3f q = position_f(i);
        const Vec3f dir = normalize(q - p_in);
        const float facing = 0.5f * (dot(dir, n) + 1.0f);
        const float w = tri * (facing * facing + 0.05f);
        const Rgb e = m_sh[i].irradiance(n);
        fallback += e * tri;
        fallback_w += tri;
        if (!m_live[i]) continue;
        sum += e * w;
        wsum += w;
    }
    if (wsum > 1e-6f) return sum / wsum;
    return fallback_w > 0.0f ? fallback / fallback_w : Rgb();
}

} // namespace lighting
} // namespace fizmo
