#include "fizmo_library.hpp"
#include "fluid_2d.hpp"

namespace fizmo {
namespace physics {

namespace {

constexpr Real kPi = 3.14159265358979323846_r;

} // namespace

void FluidVolume2D::init_waves() {
    const Real scale = std::max(m_quality.resolution_scale, 0.1_r);
    m_dx = std::max(m_def.cell_size * scale, 1e-3_r);
    m_cols = std::max(1, static_cast<int>(std::ceil(m_size.x / m_dx - 1e-3_r)));
    const std::size_t n = static_cast<std::size_t>(m_cols);
    m_ground.assign(n, 0.0_r);
    m_depth.assign(n, std::clamp(m_def.fill, 0.0_r, 1.0_r) * m_size.y);
    m_flux.assign(n + 1, 0.0_r);
    m_occ_col.assign(n, 0.0_r);
    m_occ_prev.assign(n, 0.0_r);
    m_occ_push.assign(n, 0.0_r);
    m_wall_col.assign(n, 0);
    m_col_vel.assign(n, 0.0_r);
    m_col_scale.assign(n, 1.0_r);
    m_col_change.assign(n, 0.0_r);
    const std::size_t tiles = (n + kWaveTile - 1) / kWaveTile;
    m_tile_active.assign(tiles, 1);
    m_tile_calm.assign(tiles, 0);
    m_heights.assign(n, 0.0_r);
    m_heights_dirty = true;
}

bool FluidVolume2D::column_of(const RVec2& world, int& i) const noexcept {
    if (m_cols <= 0) return false;
    const RVec2 l = to_local(world);
    i = static_cast<int>(std::floor(l.x / m_dx));
    return i >= 0 && i < m_cols;
}

void FluidVolume2D::set_ground(int i, Real h) {
    if (i < 0 || i >= m_cols) return;
    m_ground[static_cast<std::size_t>(i)] = h;
    wake();
}

Real FluidVolume2D::ground(int i) const noexcept { return i >= 0 && i < m_cols ? m_ground[static_cast<std::size_t>(i)] : 0.0_r; }

void FluidVolume2D::set_wall(int i, bool w) {
    if (i < 0 || i >= m_cols) return;
    m_wall_col[static_cast<std::size_t>(i)] = w ? 1 : 0;
    if (w) m_depth[static_cast<std::size_t>(i)] = 0.0_r;
    wake();
}

void FluidVolume2D::set_wall_box(const AABB& box, bool w) {
    if (m_cols <= 0) return;
    const RVec2 a = to_local(box.min), b = to_local(box.max);
    const Real lo = std::min(a.x, b.x), hi = std::max(a.x, b.x);
    for (int i = 0; i < m_cols; ++i) {
        const Real c = (static_cast<Real>(i) + 0.5_r) * m_dx;
        if (c >= lo && c <= hi) set_wall(i, w);
    }
}

bool FluidVolume2D::wall(int i) const noexcept { return i >= 0 && i < m_cols && m_wall_col[static_cast<std::size_t>(i)] != 0; }

void FluidVolume2D::set_depth(int i, Real d) {
    if (i < 0 || i >= m_cols || m_wall_col[static_cast<std::size_t>(i)]) return;
    m_depth[static_cast<std::size_t>(i)] = std::max(0.0_r, d);
    m_heights_dirty = true;
    const std::size_t t = static_cast<std::size_t>(i / kWaveTile);
    m_tile_active[t] = 1;
    m_tile_calm[t] = 0;
}

Real FluidVolume2D::depth(int i) const noexcept { return i >= 0 && i < m_cols ? m_depth[static_cast<std::size_t>(i)] : 0.0_r; }

Real FluidVolume2D::column_height(int i) const noexcept {
    if (i < 0 || i >= m_cols) return 0.0_r;
    return column_head(static_cast<std::size_t>(i)) + swell_height((static_cast<Real>(i) + 0.5_r) * m_dx);
}

Real FluidVolume2D::swell_height(Real lu) const noexcept {
    const FluidWaveSettings2D& w = m_def.waves;
    if (w.swell_amplitude <= 0.0_r) return 0.0_r;
    Real h = 0.0_r;
    Real amp = w.swell_amplitude;
    Real lambda = std::max(w.swell_wavelength, 1e-3_r);
    const Real dir = w.wind >= 0.0_r ? 1.0_r : -1.0_r;
    for (int o = 0; o < std::max(1, w.swell_octaves); ++o) {
        const Real k = 2.0_r * kPi / lambda;
        const Real om = std::sqrt(m_g * k);
        const Real ph = static_cast<Real>(o) * 1.7_r;
        const Real s = std::sin(k * lu * dir - om * m_time + ph);
        h += amp * (s + w.swell_steepness * 0.5_r * (s * s - 0.5_r));
        amp *= 0.5_r;
        lambda *= 0.6_r;
    }
    return h;
}

Real FluidVolume2D::swell_velocity(Real lu, Real lh) const noexcept {
    const FluidWaveSettings2D& w = m_def.waves;
    if (w.swell_amplitude <= 0.0_r || m_cols <= 0) return 0.0_r;
    Real v = 0.0_r;
    Real amp = w.swell_amplitude;
    Real lambda = std::max(w.swell_wavelength, 1e-3_r);
    const Real dir = w.wind >= 0.0_r ? 1.0_r : -1.0_r;
    const int i = std::clamp(static_cast<int>(std::floor(lu / m_dx)), 0, m_cols - 1);
    const Real top = column_head(static_cast<std::size_t>(i));
    for (int o = 0; o < std::max(1, w.swell_octaves); ++o) {
        const Real k = 2.0_r * kPi / lambda;
        const Real om = std::sqrt(m_g * k);
        const Real ph = static_cast<Real>(o) * 1.7_r;
        const Real decay = std::exp(std::min(0.0_r, k * (lh - top)));
        v += amp * om * decay * std::sin(k * lu * dir - om * m_time + ph) * dir;
        amp *= 0.5_r;
        lambda *= 0.6_r;
    }
    return v;
}

const std::vector<Real>& FluidVolume2D::heights() const {
    if (!m_heights_dirty && m_heights.size() == static_cast<std::size_t>(m_cols)) return m_heights;
    m_heights.resize(static_cast<std::size_t>(m_cols));
    for (int i = 0; i < m_cols; ++i) m_heights[static_cast<std::size_t>(i)] = column_height(i);
    m_heights_dirty = false;
    return m_heights;
}

Real FluidVolume2D::sample_columns(Real lu, const std::vector<Real>& f) const noexcept {
    if (m_cols <= 0 || f.size() < static_cast<std::size_t>(m_cols)) return 0.0_r;
    const Real x = lu / m_dx - 0.5_r;
    const int i0 = std::clamp(static_cast<int>(std::floor(x)), 0, m_cols - 1);
    const int i1 = std::min(i0 + 1, m_cols - 1);
    const Real t = std::clamp(x - static_cast<Real>(i0), 0.0_r, 1.0_r);
    return f[static_cast<std::size_t>(i0)] * (1.0_r - t) + f[static_cast<std::size_t>(i1)] * t;
}

void FluidVolume2D::disturb(const RVec2& point, Real strength, Real radius) {
    if (m_def.level == FluidLevel::realistic) {
        explode(point, radius > 0.0_r ? radius : 4.0_r * m_cs, strength);
        return;
    }
    if (m_def.level != FluidLevel::waves) {
        wake();
        return;
    }
    const Real r = radius > 0.0_r ? radius : 3.0_r * m_dx;
    const RVec2 l = to_local(point);
    const int i0 = std::max(0, static_cast<int>(std::floor((l.x - r) / m_dx)));
    const int i1 = std::min(m_cols - 1, static_cast<int>(std::floor((l.x + r) / m_dx)));
    if (i0 > i1) return;
    Real sum = 0.0_r;
    int count = 0;
    for (int i = i0; i <= i1; ++i) {
        const Real d = std::abs((static_cast<Real>(i) + 0.5_r) * m_dx - l.x) / r;
        const Real c = std::cos(0.5_r * kPi * std::min(d, 1.0_r));
        m_col_change[static_cast<std::size_t>(i)] = c * c;
        sum += c * c;
        ++count;
    }
    const Real mean = count > 0 ? sum / static_cast<Real>(count) : 0.0_r;
    for (int i = i0; i <= i1; ++i) {
        const std::size_t c = static_cast<std::size_t>(i);
        if (m_wall_col[c]) continue;
        m_depth[c] = std::max(0.0_r, m_depth[c] + strength * (m_col_change[c] - mean));
        m_col_change[c] = 0.0_r;
        m_tile_active[c / kWaveTile] = 1;
        m_tile_calm[c / kWaveTile] = 0;
    }
    m_heights_dirty = true;
}

void FluidVolume2D::waves_occupancy(Real dt) {
    std::swap(m_occ_prev, m_occ_col);
    std::fill(m_occ_col.begin(), m_occ_col.end(), 0.0_r);
    if (!m_def.coupling.push_fluid) return;
    const Real ripple = std::max(0.0_r, m_def.waves.ripple_strength);
    for (const NearBody& nb : m_near) {
        const RigidBody2D& b = *nb.body;
        const BodyCache& cache = body_cache(b, m_quality.buoyancy_samples);
        const auto& cols = b.colliders();
        const Transform2D& xf = b.transform();
        RVec2 flo{}, fhi{};
        if (!local_footprint(b, flo, fhi)) continue;
        int il = static_cast<int>(std::floor(flo.x / m_dx)) - 1;
        int ir = static_cast<int>(std::floor(fhi.x / m_dx)) + 1;
        const bool okl = il >= 0 && !m_wall_col[static_cast<std::size_t>(il)];
        const bool okr = ir < m_cols && !m_wall_col[static_cast<std::size_t>(ir)];
        Real hl = 0.0_r, hr = 0.0_r;
        if (!okl && !okr) {
            const int ic = std::clamp(static_cast<int>(std::floor(0.5_r * (flo.x + fhi.x) / m_dx)), 0, m_cols - 1);
            hl = hr = m_ground[static_cast<std::size_t>(ic)] + m_depth[static_cast<std::size_t>(ic)];
        } else {
            hl = okl ? column_head(static_cast<std::size_t>(il)) : column_head(static_cast<std::size_t>(ir));
            hr = okr ? column_head(static_cast<std::size_t>(ir)) : hl;
        }
        BodyCache& bc = m_cache[b.handle().packed()];
        const Real now = 0.5_r * (hl + hr);
        if (!bc.has_ref) bc.ref = now;
        else bc.ref += (now - bc.ref) * std::clamp(dt / std::max(m_def.waves.reference_time, 1e-3_r), 0.0_r, 1.0_r);
        bc.has_ref = true;
        const Real ref = bc.ref;
        for (std::size_t ci = 0; ci < cols.size() && ci + 1 < cache.collider_start.size(); ++ci) {
            const Collider2D& c = cols[ci];
            if (c.is_sensor || c.shape.type == ShapeType::Edge) continue;
            if (c.shape.type == ShapeType::Circle) {
                const RVec2 center = to_local(xf.compose(c.local_offset).apply(c.shape.circle.center));
                const Real r = c.shape.circle.radius;
                const int i0 = std::max(0, static_cast<int>(std::floor((center.x - r) / m_dx)));
                const int i1 = std::min(m_cols - 1, static_cast<int>(std::floor((center.x + r) / m_dx)));
                for (int i = i0; i <= i1; ++i) {
                    const std::size_t q = static_cast<std::size_t>(i);
                    const Real x = (static_cast<Real>(i) + 0.5_r) * m_dx - center.x;
                    if (std::abs(x) >= r) continue;
                    const Real half = std::sqrt(r * r - x * x);
                    const Real top = ref;
                    const Real sub = std::clamp(top - (center.y - half), 0.0_r, 2.0_r * half);
                    m_occ_col[q] += sub * ripple;
                }
                continue;
            }
            const Real s = cache.spacing;
            for (std::uint32_t p = cache.collider_start[ci]; p < cache.collider_start[ci + 1]; ++p) {
                const RVec2 l = to_local(xf.apply(cache.points[p].local));
                const int i = static_cast<int>(std::floor(l.x / m_dx));
                if (i < 0 || i >= m_cols) continue;
                const std::size_t q = static_cast<std::size_t>(i);
                const Real top = ref;
                const Real frac = std::clamp((top - l.y) / s + 0.5_r, 0.0_r, 1.0_r);
                m_occ_col[q] += cache.points[p].weight * frac / m_dx * ripple;
            }
        }
    }
    const Real blend = std::clamp(m_def.waves.occupancy_response * dt, 0.0_r, 1.0_r);
    for (int i = 0; i < m_cols; ++i) {
        const std::size_t q = static_cast<std::size_t>(i);
        m_occ_col[q] = m_occ_prev[q] + (m_occ_col[q] - m_occ_prev[q]) * blend;
        if (std::abs(m_occ_col[q] - m_occ_prev[q]) > m_def.sleep_threshold) {
            m_tile_active[q / kWaveTile] = 1;
            m_tile_calm[q / kWaveTile] = 0;
        }
    }
}

void FluidVolume2D::waves_substep(Real h) {
    const std::size_t n = static_cast<std::size_t>(m_cols);
    const std::size_t tiles = m_tile_active.size();
    const FluidMaterial& m = m_materials[0];
    const Real nu = m.kinematic_viscosity() * (1.0_r + 3.0_r * (m.viscosity_factor() - 1.0_r));
    const Real kk = kPi / m_dx;
    const Real g = m_g;
    const Real min_depth = std::max(m_def.waves.min_depth, 1e-7_r);
    for (std::size_t t = 0; t < tiles; ++t) {
        const bool on = m_tile_active[t] || (t > 0 && m_tile_active[t - 1]) || (t + 1 < tiles && m_tile_active[t + 1]);
        if (!on) continue;
        const std::size_t a = std::max<std::size_t>(1, t * kWaveTile);
        const std::size_t b = std::min(n, (t + 1) * kWaveTile + 1);
        for (std::size_t k = a; k < b && k < n; ++k) {
            if (m_wall_col[k - 1] || m_wall_col[k]) {
                m_flux[k] = 0.0_r;
                continue;
            }
            const Real dh = column_head(k - 1) - column_head(k);
            const Real up = dh > 0.0_r ? m_depth[k - 1] : m_depth[k];
            Real q = m_flux[k];
            if (up > min_depth) q += h * g * up * dh / m_dx;
            const Real davg = std::max(0.5_r * (m_depth[k - 1] + m_depth[k]), 0.01_r);
            const Real damp = m_def.waves.damping + 3.0_r * nu / (davg * davg) + 2.0_r * nu * kk * kk;
            q /= (1.0_r + h * damp);
            m_flux[k] = q;
        }
    }
    m_flux[0] = 0.0_r;
    m_flux[n] = 0.0_r;
    for (std::size_t i = 0; i < n; ++i) {
        const Real out = h / m_dx * (std::max(m_flux[i + 1], 0.0_r) + std::max(-m_flux[i], 0.0_r));
        m_col_scale[i] = out > m_depth[i] && out > 0.0_r ? m_depth[i] / out : 1.0_r;
    }
    for (std::size_t k = 1; k < n; ++k) {
        if (m_flux[k] > 0.0_r) m_flux[k] *= m_col_scale[k - 1];
        else m_flux[k] *= m_col_scale[k];
    }
    for (std::size_t i = 0; i < n; ++i) {
        if (m_flux[i + 1] == 0.0_r && m_flux[i] == 0.0_r) continue;
        const Real d = m_depth[i] - h / m_dx * (m_flux[i + 1] - m_flux[i]);
        const Real nd = std::max(0.0_r, d);
        m_col_change[i] = std::max(m_col_change[i], std::abs(nd - m_depth[i]));
        m_depth[i] = nd;
    }
}

void FluidVolume2D::step_waves(detail::FluidTaskPool& pool, Real dt, FluidStats2D& stats) {
    static_cast<void>(pool);
    if (m_cols <= 0) return;
    waves_occupancy(dt);
    bool any = false;
    for (std::uint8_t t : m_tile_active) any = any || t != 0;
    if (!any && !m_all_dirty) return;
    Real dmax = 0.0_r, vmax = 0.0_r;
    for (std::size_t i = 0; i < m_depth.size(); ++i) {
        dmax = std::max(dmax, m_depth[i] + m_occ_col[i]);
        vmax = std::max(vmax, std::abs(m_col_vel[i]));
    }
    const Real speed = std::sqrt(m_g * dmax) + vmax + 1e-6_r;
    const Real cfl = std::clamp(m_quality.cfl, 0.05_r, 1.0_r);
    const int cap = std::max(1, m_quality.max_substeps * 2);
    int n = std::max(1, static_cast<int>(std::ceil(dt * speed / (cfl * m_dx))));
    Real h = dt / static_cast<Real>(n);
    if (n > cap) {
        n = cap;
        h = cfl * m_dx / speed;
    }
    std::fill(m_col_change.begin(), m_col_change.end(), 0.0_r);
    for (int s = 0; s < n; ++s) waves_substep(h);
    const Real min_depth = std::max(m_def.waves.min_depth, 1e-6_r);
    for (std::size_t i = 0; i < m_depth.size(); ++i) {
        const Real q = 0.5_r * (m_flux[i] + m_flux[i + 1]);
        m_col_vel[i] = m_depth[i] > min_depth * 10.0_r ? q / m_depth[i] : 0.0_r;
    }
    for (std::size_t t = 0; t < m_tile_active.size(); ++t) {
        Real change = 0.0_r;
        for (std::size_t i = t * kWaveTile; i < std::min(m_depth.size(), (t + 1) * kWaveTile); ++i) change = std::max(change, m_col_change[i] + std::abs(m_flux[i]) * h);
        if (change > m_def.sleep_threshold) {
            m_tile_calm[t] = 0;
            m_tile_active[t] = 1;
        } else if (m_tile_active[t] && ++m_tile_calm[t] > 30 && m_def.allow_sleep) {
            m_tile_active[t] = 0;
            for (std::size_t k = t * kWaveTile + 1; k < std::min(m_depth.size(), (t + 1) * kWaveTile); ++k) m_flux[k] = 0.0_r;
            for (std::size_t i = t * kWaveTile; i < std::min(m_depth.size(), (t + 1) * kWaveTile); ++i) m_col_vel[i] = 0.0_r;
        }
    }
    stats.substeps = std::max(stats.substeps, n);
    m_all_dirty = false;
    m_heights_dirty = true;
}

} // namespace physics
} // namespace fizmo
