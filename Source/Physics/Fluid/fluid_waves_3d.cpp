#include "fizmo_library.hpp"
#include "fluid_3d.hpp"

namespace fizmo {
namespace physics {

namespace {

using V3 = RVec3;

constexpr int kTile = 16;
constexpr Real kPi = 3.14159265358979323846_r;

int columns_along(Real length, Real cell) noexcept {
    return std::max(2, static_cast<int>(std::ceil(length / cell - 1e-3_r)));
}

} // namespace

void FluidVolume3D::init_waves() {
    m_dx = std::max(1e-3_r, m_def.cell_size * m_quality.resolution_scale);
    m_cx = columns_along(m_size.x, m_dx);
    m_cz = columns_along(m_size.z, m_dx);
    const std::size_t n = static_cast<std::size_t>(m_cx) * static_cast<std::size_t>(m_cz);
    const Real d0 = std::clamp(m_def.fill > 0.0_r ? m_def.fill : 0.5_r, 0.0_r, 1.0_r) * m_size.y;
    m_ground.assign(n, 0.0_r);
    m_depth.assign(n, d0);
    m_occ_col.assign(n, 0.0_r);
    m_occ_prev.assign(n, 0.0_r);
    m_wall.assign(n, 0);
    m_pipe.assign(n * 4, 0.0_r);
    m_vel_u.assign(n, 0.0_r);
    m_vel_w.assign(n, 0.0_r);
    m_change.assign(n, 0.0_r);
    m_heights.assign(n, d0);
    m_normals.assign(n, V3{ 0.0_r, 1.0_r, 0.0_r });
    m_tx = (m_cx + kTile - 1) / kTile;
    m_tz = (m_cz + kTile - 1) / kTile;
    const std::size_t tiles = static_cast<std::size_t>(m_tx) * static_cast<std::size_t>(m_tz);
    m_tile_changed.assign(tiles, 1);
    m_tile_active.assign(tiles, 1);
    m_tile_list.reserve(tiles);
    m_all_dirty = true;
}

std::size_t FluidVolume3D::tile_of(int i, int k) const noexcept {
    return static_cast<std::size_t>(k / kTile) * static_cast<std::size_t>(m_tx) + static_cast<std::size_t>(i / kTile);
}

void FluidVolume3D::mark_column_dirty(int i, int k) noexcept {
    if (m_tile_changed.empty() || i < 0 || k < 0 || i >= m_cx || k >= m_cz) return;
    m_tile_changed[tile_of(i, k)] = 1;
    m_heights_dirty = true;
}

bool FluidVolume3D::column_of(const V3& world, int& i, int& k) const noexcept {
    if (m_cx == 0) return false;
    const V3 l = to_local(world);
    i = static_cast<int>(std::floor(l.x / m_dx));
    k = static_cast<int>(std::floor(l.z / m_dx));
    return i >= 0 && k >= 0 && i < m_cx && k < m_cz;
}

void FluidVolume3D::set_ground(int i, int k, Real height) {
    if (i < 0 || k < 0 || i >= m_cx || k >= m_cz) return;
    const std::size_t c = column_index(i, k);
    const Real surface = m_ground[c] + m_depth[c];
    m_ground[c] = std::max(0.0_r, height);
    m_depth[c] = std::max(0.0_r, surface - m_ground[c]);
    mark_column_dirty(i, k);
}

Real FluidVolume3D::ground(int i, int k) const noexcept {
    if (i < 0 || k < 0 || i >= m_cx || k >= m_cz) return 0.0_r;
    return m_ground[column_index(i, k)];
}

void FluidVolume3D::set_wall(int i, int k, bool w) {
    if (i < 0 || k < 0 || i >= m_cx || k >= m_cz) return;
    const std::size_t c = column_index(i, k);
    m_wall[c] = w ? 1 : 0;
    if (w) {
        m_depth[c] = 0.0_r;
        for (int d = 0; d < 4; ++d) m_pipe[c * 4 + static_cast<std::size_t>(d)] = 0.0_r;
    }
    mark_column_dirty(i, k);
}

void FluidVolume3D::set_wall_box(const AABB3D& box, bool w) {
    for (int k = 0; k < m_cz; ++k) {
        for (int i = 0; i < m_cx; ++i) {
            const V3 p = to_world(V3{ (static_cast<Real>(i) + 0.5_r) * m_dx, 0.0_r, (static_cast<Real>(k) + 0.5_r) * m_dx });
            const V3 l = to_local(box.min), h = to_local(box.max);
            const V3 lo = vec3::min(l, h), hi = vec3::max(l, h);
            const V3 lp = to_local(p);
            if (lp.x >= lo.x && lp.x <= hi.x && lp.z >= lo.z && lp.z <= hi.z) set_wall(i, k, w);
        }
    }
}

bool FluidVolume3D::wall(int i, int k) const noexcept {
    if (i < 0 || k < 0 || i >= m_cx || k >= m_cz) return true;
    return m_wall[column_index(i, k)] != 0;
}

void FluidVolume3D::set_depth(int i, int k, Real d) {
    if (i < 0 || k < 0 || i >= m_cx || k >= m_cz) return;
    const std::size_t c = column_index(i, k);
    if (m_wall[c]) return;
    m_depth[c] = std::max(0.0_r, d);
    mark_column_dirty(i, k);
}

Real FluidVolume3D::depth(int i, int k) const noexcept {
    if (i < 0 || k < 0 || i >= m_cx || k >= m_cz) return 0.0_r;
    return m_depth[column_index(i, k)];
}

Real FluidVolume3D::column_height(int i, int k) const noexcept {
    if (i < 0 || k < 0 || i >= m_cx || k >= m_cz) return 0.0_r;
    return column_head(column_index(i, k));
}

void FluidVolume3D::disturb(const V3& point, Real strength, Real radius) {
    if (m_def.level == FluidLevel::realistic) {
        const Real r = radius > 0.0_r ? radius : 4.0_r * m_h;
        for (std::size_t p = 0; p < m_px.size(); ++p) {
            const V3 d = m_px[p] - point;
            const Real l = std::sqrt(vec3::dot(d, d));
            if (l >= r) continue;
            m_pv[p] = m_pv[p] + (l > 1e-6_r ? d * (1.0_r / l) : m_up) * (strength * (1.0_r - l / r));
            m_asleep[p] = 0;
            m_calm[p] = 0;
        }
        return;
    }
    if (m_def.level != FluidLevel::waves) {
        wake();
        return;
    }
    const Real r = radius > 0.0_r ? radius : 3.0_r * m_dx;
    const V3 l = to_local(point);
    const int i0 = std::max(0, static_cast<int>(std::floor((l.x - r) / m_dx)));
    const int i1 = std::min(m_cx - 1, static_cast<int>(std::floor((l.x + r) / m_dx)));
    const int k0 = std::max(0, static_cast<int>(std::floor((l.z - r) / m_dx)));
    const int k1 = std::min(m_cz - 1, static_cast<int>(std::floor((l.z + r) / m_dx)));
    Real sum = 0.0_r;
    int count = 0;
    for (int k = k0; k <= k1; ++k) {
        for (int i = i0; i <= i1; ++i) {
            const std::size_t c = column_index(i, k);
            if (m_wall[c]) continue;
            const Real du = (static_cast<Real>(i) + 0.5_r) * m_dx - l.x, dw = (static_cast<Real>(k) + 0.5_r) * m_dx - l.z;
            const Real q = (du * du + dw * dw) / (r * r);
            if (q > 1.0_r) continue;
            sum += std::exp(-4.0_r * q);
            ++count;
        }
    }
    if (count == 0) return;
    const Real mean = sum / static_cast<Real>(count);
    for (int k = k0; k <= k1; ++k) {
        for (int i = i0; i <= i1; ++i) {
            const std::size_t c = column_index(i, k);
            if (m_wall[c]) continue;
            const Real du = (static_cast<Real>(i) + 0.5_r) * m_dx - l.x, dw = (static_cast<Real>(k) + 0.5_r) * m_dx - l.z;
            const Real q = (du * du + dw * dw) / (r * r);
            if (q > 1.0_r) continue;
            m_depth[c] = std::max(0.0_r, m_depth[c] + strength * (std::exp(-4.0_r * q) - mean));
            mark_column_dirty(i, k);
        }
    }
}

Real FluidVolume3D::swell_height(Real lu, Real lw) const noexcept {
    const FluidWaveSettings& ws = m_def.waves;
    if (ws.swell_amplitude <= 0.0_r) return 0.0_r;
    V3 wind = local_dir(ws.wind);
    Real wl = std::sqrt(wind.x * wind.x + wind.z * wind.z);
    Real du = wl > 1e-6_r ? wind.x / wl : 1.0_r, dw = wl > 1e-6_r ? wind.z / wl : 0.0_r;
    Real total = 0.0_r;
    Real amp = ws.swell_amplitude, len = std::max(ws.swell_wavelength, 0.1_r);
    for (int o = 0; o < std::max(1, ws.swell_octaves); ++o) {
        const Real k = 2.0_r * kPi / len;
        const Real om = std::sqrt(m_g * k);
        const Real th = k * (lu * du + lw * dw) - om * m_time + static_cast<Real>(o) * 1.7_r;
        total += amp * (std::cos(th) + 0.5_r * ws.swell_steepness * k * amp * std::cos(2.0_r * th));
        const Real ru = du * 0.8_r - dw * 0.6_r, rw = du * 0.6_r + dw * 0.8_r;
        du = ru;
        dw = rw;
        amp *= 0.55_r;
        len *= 0.6_r;
    }
    return total;
}

void FluidVolume3D::swell_velocity(Real lu, Real lw, Real lh, Real& vu, Real& vh, Real& vw) const noexcept {
    vu = vh = vw = 0.0_r;
    const FluidWaveSettings& ws = m_def.waves;
    if (ws.swell_amplitude <= 0.0_r || m_def.level != FluidLevel::waves) return;
    V3 wind = local_dir(ws.wind);
    Real wl = std::sqrt(wind.x * wind.x + wind.z * wind.z);
    Real du = wl > 1e-6_r ? wind.x / wl : 1.0_r, dw = wl > 1e-6_r ? wind.z / wl : 0.0_r;
    const Real surface = sample_waves(lu, lw, m_heights);
    Real amp = ws.swell_amplitude, len = std::max(ws.swell_wavelength, 0.1_r);
    for (int o = 0; o < std::max(1, ws.swell_octaves); ++o) {
        const Real k = 2.0_r * kPi / len;
        const Real om = std::sqrt(m_g * k);
        const Real th = k * (lu * du + lw * dw) - om * m_time + static_cast<Real>(o) * 1.7_r;
        const Real decay = std::exp(std::min(0.0_r, k * (lh - surface)));
        const Real s = amp * om * decay;
        vu += s * std::cos(th) * du;
        vw += s * std::cos(th) * dw;
        vh += s * std::sin(th);
        const Real ru = du * 0.8_r - dw * 0.6_r, rw = du * 0.6_r + dw * 0.8_r;
        du = ru;
        dw = rw;
        amp *= 0.55_r;
        len *= 0.6_r;
    }
}

Real FluidVolume3D::sample_waves(Real lu, Real lw, const std::vector<Real>& field) const noexcept {
    if (m_cx == 0 || field.empty()) return 0.0_r;
    const Real fu = std::clamp(lu / m_dx - 0.5_r, 0.0_r, static_cast<Real>(m_cx - 1));
    const Real fw = std::clamp(lw / m_dx - 0.5_r, 0.0_r, static_cast<Real>(m_cz - 1));
    const int i0 = std::min(static_cast<int>(fu), m_cx - 1), k0 = std::min(static_cast<int>(fw), m_cz - 1);
    const int i1 = std::min(i0 + 1, m_cx - 1), k1 = std::min(k0 + 1, m_cz - 1);
    const Real tu = fu - static_cast<Real>(i0), tw = fw - static_cast<Real>(k0);
    auto at = [&](int i, int k) -> Real {
        const std::size_t c = column_index(i, k);
        if (m_wall[c]) {
            const std::size_t o = column_index(i0, k0);
            return m_wall[o] ? field[c] : field[o];
        }
        return field[c];
    };
    const Real a = at(i0, k0) * (1.0_r - tu) + at(i1, k0) * tu;
    const Real b = at(i0, k1) * (1.0_r - tu) + at(i1, k1) * tu;
    return a * (1.0_r - tw) + b * tw;
}

const std::vector<Real>& FluidVolume3D::heights() const {
    if (m_def.level != FluidLevel::waves) return m_heights;
    if (m_heights_dirty || m_heights.size() != m_depth.size()) {
        m_heights.resize(m_depth.size());
        const bool swell = m_def.waves.swell_amplitude > 0.0_r;
        for (int k = 0; k < m_cz; ++k) {
            for (int i = 0; i < m_cx; ++i) {
                const std::size_t c = column_index(i, k);
                Real h = column_head(c);
                if (swell && !m_wall[c] && m_depth[c] > m_def.waves.min_depth) h += swell_height((static_cast<Real>(i) + 0.5_r) * m_dx, (static_cast<Real>(k) + 0.5_r) * m_dx);
                m_heights[c] = h;
            }
        }
        m_heights_dirty = false;
        m_normals.clear();
    }
    return m_heights;
}

const std::vector<RVec3>& FluidVolume3D::normals() const {
    if (m_def.level != FluidLevel::waves) return m_normals;
    const std::vector<Real>& h = heights();
    if (m_normals.size() == h.size()) return m_normals;
    m_normals.resize(h.size());
    const Real inv = 1.0_r / (2.0_r * m_dx);
    for (int k = 0; k < m_cz; ++k) {
        for (int i = 0; i < m_cx; ++i) {
            const Real hl = h[column_index(std::max(i - 1, 0), k)], hr = h[column_index(std::min(i + 1, m_cx - 1), k)];
            const Real hb = h[column_index(i, std::max(k - 1, 0))], hf = h[column_index(i, std::min(k + 1, m_cz - 1))];
            const V3 n{ -(hr - hl) * inv, 1.0_r, -(hf - hb) * inv };
            const Real l = std::sqrt(vec3::dot(n, n));
            m_normals[column_index(i, k)] = world_dir(n * (1.0_r / l));
        }
    }
    return m_normals;
}

void FluidVolume3D::refresh_tiles() {
    const std::size_t tiles = m_tile_active.size();
    if (m_all_dirty || !m_def.allow_sleep) {
        std::fill(m_tile_active.begin(), m_tile_active.end(), std::uint8_t{ 1 });
        m_all_dirty = false;
    } else {
        for (int tz = 0; tz < m_tz; ++tz) {
            for (int tx = 0; tx < m_tx; ++tx) {
                const std::size_t t = static_cast<std::size_t>(tz) * static_cast<std::size_t>(m_tx) + static_cast<std::size_t>(tx);
                bool on = false;
                for (int dz = -1; dz <= 1 && !on; ++dz) {
                    for (int dx = -1; dx <= 1 && !on; ++dx) {
                        const int x = tx + dx, z = tz + dz;
                        if (x < 0 || z < 0 || x >= m_tx || z >= m_tz) continue;
                        if (m_tile_changed[static_cast<std::size_t>(z) * static_cast<std::size_t>(m_tx) + static_cast<std::size_t>(x)]) on = true;
                    }
                }
                if (!on && m_tile_active[t]) {
                    const int i0 = tx * kTile, i1 = std::min(m_cx, i0 + kTile);
                    const int k0 = tz * kTile, k1 = std::min(m_cz, k0 + kTile);
                    for (int k = k0; k < k1; ++k) {
                        for (int i = i0; i < i1; ++i) {
                            const std::size_t c = column_index(i, k);
                            for (int d = 0; d < 4; ++d) m_pipe[c * 4 + static_cast<std::size_t>(d)] = 0.0_r;
                            m_vel_u[c] = 0.0_r;
                            m_vel_w[c] = 0.0_r;
                        }
                    }
                }
                m_tile_active[t] = on ? 1 : 0;
            }
        }
    }
    m_tile_list.clear();
    for (std::size_t t = 0; t < tiles; ++t) {
        if (m_tile_active[t]) m_tile_list.push_back(static_cast<std::uint32_t>(t));
    }
    std::fill(m_tile_changed.begin(), m_tile_changed.end(), std::uint8_t{ 0 });
}

void FluidVolume3D::step_waves(detail::FluidTaskPool& pool, Real dt, FluidStats3D& stats) {
    refresh_tiles();
    if (m_tile_list.empty()) return;
    Real dmax = 0.0_r;
    for (std::uint32_t t : m_tile_list) {
        const int i0 = static_cast<int>(t % static_cast<std::uint32_t>(m_tx)) * kTile, k0 = static_cast<int>(t / static_cast<std::uint32_t>(m_tx)) * kTile;
        const int i1 = std::min(m_cx, i0 + kTile), k1 = std::min(m_cz, k0 + kTile);
        for (int k = k0; k < k1; ++k) {
            for (int i = i0; i < i1; ++i) dmax = std::max(dmax, m_depth[column_index(i, k)] + m_occ_col[column_index(i, k)]);
        }
    }
    const Real speed = std::sqrt(m_g * std::max(dmax, 1e-3_r));
    const Real limit = m_dx / speed;
    const Real cfl = std::clamp(m_quality.cfl, 0.05_r, 0.9_r) * limit;
    int n = std::max(1, static_cast<int>(std::ceil(dt / cfl)));
    const int cap = std::max(1, m_quality.max_substeps);
    Real h = dt / static_cast<Real>(n);
    if (n > cap) {
        n = cap;
        h = std::min(dt / static_cast<Real>(n), 0.7_r * limit);
    }
    for (int s = 0; s < n; ++s) waves_substep(pool, h);
    stats.substeps = std::max(stats.substeps, n);
}

void FluidVolume3D::waves_substep(detail::FluidTaskPool& pool, Real h) {
    const FluidWaveSettings& ws = m_def.waves;
    const Real nu = m_def.material.kinematic_viscosity();
    const Real kmax = kPi / m_dx;
    const Real base_rate = std::max(0.0_r, ws.damping) + 2.0_r * nu * kmax * kmax;
    const Real g = m_g;
    const Real area = m_dx * m_dx;
    const Real min_depth = ws.min_depth;
    const int di[4] = { 1, -1, 0, 0 };
    const int dk[4] = { 0, 0, 1, -1 };
    auto tile_range = [&](std::uint32_t t, int& i0, int& i1, int& k0, int& k1) {
        i0 = static_cast<int>(t % static_cast<std::uint32_t>(m_tx)) * kTile;
        k0 = static_cast<int>(t / static_cast<std::uint32_t>(m_tx)) * kTile;
        i1 = std::min(m_cx, i0 + kTile);
        k1 = std::min(m_cz, k0 + kTile);
    };
    auto open = [&](int i, int k) {
        if (i < 0 || k < 0 || i >= m_cx || k >= m_cz) return false;
        if (m_wall[column_index(i, k)]) return false;
        return m_tile_active[tile_of(i, k)] != 0;
    };
    pool.parallel_for(m_tile_list.size(), [&](std::size_t n) {
        int i0, i1, k0, k1;
        tile_range(m_tile_list[n], i0, i1, k0, k1);
        for (int k = k0; k < k1; ++k) {
            for (int i = i0; i < i1; ++i) {
                const std::size_t c = column_index(i, k);
                Real* f = &m_pipe[c * 4];
                if (m_wall[c]) {
                    f[0] = f[1] = f[2] = f[3] = 0.0_r;
                    continue;
                }
                const Real dc = m_depth[c];
                const Real hc = column_head(c);
                const Real dd = std::max(dc, 0.05_r);
                const Real damp = std::exp(-(base_rate + 3.0_r * nu / (dd * dd)) * h);
                Real total = 0.0_r;
                for (int d = 0; d < 4; ++d) {
                    const int a = i + di[d], b = k + dk[d];
                    if (!open(a, b)) {
                        f[d] = 0.0_r;
                        continue;
                    }
                    const std::size_t o = column_index(a, b);
                    const Real ho = column_head(o);
                    const Real face = std::max(0.0_r, std::max(hc, ho) - std::max(m_ground[c], m_ground[o]));
                    const Real wet = std::min(face, std::max(dc, m_depth[o]));
                    Real v = f[d] * damp + h * g * wet * (hc - ho);
                    if (dc <= min_depth) v = std::min(v, 0.0_r);
                    f[d] = std::max(0.0_r, v);
                    total += f[d];
                }
                if (total * h > dc * area && total > 0.0_r) {
                    const Real k_scale = dc * area / (total * h);
                    for (int d = 0; d < 4; ++d) f[d] *= k_scale;
                }
            }
        }
    });
    const Real sleep = m_def.sleep_threshold;
    pool.parallel_for(m_tile_list.size(), [&](std::size_t n) {
        const std::uint32_t t = m_tile_list[n];
        int i0, i1, k0, k1;
        tile_range(t, i0, i1, k0, k1);
        bool changed = false;
        for (int k = k0; k < k1; ++k) {
            for (int i = i0; i < i1; ++i) {
                const std::size_t c = column_index(i, k);
                if (m_wall[c]) continue;
                const Real* f = &m_pipe[c * 4];
                Real in[4] = { 0.0_r, 0.0_r, 0.0_r, 0.0_r };
                for (int d = 0; d < 4; ++d) {
                    const int a = i + di[d], b = k + dk[d];
                    if (!open(a, b)) continue;
                    in[d] = m_pipe[column_index(a, b) * 4 + static_cast<std::size_t>(d ^ 1)];
                }
                const Real net = (in[0] + in[1] + in[2] + in[3]) - (f[0] + f[1] + f[2] + f[3]);
                const Real dv = h * net / area;
                m_change[c] = dv;
                if (std::abs(dv) > sleep * h || f[0] + f[1] + f[2] + f[3] > sleep * area) changed = true;
            }
        }
        if (changed) m_tile_changed[t] = 1;
    });
    pool.parallel_for(m_tile_list.size(), [&](std::size_t n) {
        int i0, i1, k0, k1;
        tile_range(m_tile_list[n], i0, i1, k0, k1);
        for (int k = k0; k < k1; ++k) {
            for (int i = i0; i < i1; ++i) {
                const std::size_t c = column_index(i, k);
                if (m_wall[c]) continue;
                const Real before = m_depth[c];
                const Real after = std::max(0.0_r, before + m_change[c]);
                m_depth[c] = after;
                const Real* f = &m_pipe[c * 4];
                const Real in_l = open(i - 1, k) ? m_pipe[column_index(i - 1, k) * 4 + 0] : 0.0_r;
                const Real in_r = open(i + 1, k) ? m_pipe[column_index(i + 1, k) * 4 + 1] : 0.0_r;
                const Real in_b = open(i, k - 1) ? m_pipe[column_index(i, k - 1) * 4 + 2] : 0.0_r;
                const Real in_f = open(i, k + 1) ? m_pipe[column_index(i, k + 1) * 4 + 3] : 0.0_r;
                const Real avg = 0.5_r * (before + after);
                if (avg > 1e-3_r) {
                    m_vel_u[c] = 0.5_r * ((in_l - f[1]) + (f[0] - in_r)) / (m_dx * avg);
                    m_vel_w[c] = 0.5_r * ((in_b - f[3]) + (f[2] - in_f)) / (m_dx * avg);
                } else {
                    m_vel_u[c] = 0.0_r;
                    m_vel_w[c] = 0.0_r;
                }
            }
        }
    });
    m_heights_dirty = true;
}

} // namespace physics
} // namespace fizmo
