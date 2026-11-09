#include "fizmo_library.hpp"
#include "fluid_3d.hpp"

#include <array>

namespace fizmo {
namespace physics {

namespace {

using V3 = RVec3;

constexpr std::uint8_t kAir = 0;
constexpr std::uint8_t kLiquid = 1;
constexpr std::uint8_t kSolid = 2;
constexpr std::uint8_t kTypeMask = 3;
constexpr std::uint8_t kDeep = 4;
constexpr std::uint8_t kSurface = 8;
constexpr std::uint8_t kLonely = 16;
constexpr Real kAirPhi = 0.5_r;
constexpr int kPerCell = 8;

std::uint32_t hash32(std::uint32_t x) noexcept {
    x ^= x >> 16;
    x *= 0x7feb352du;
    x ^= x >> 15;
    x *= 0x846ca68bu;
    x ^= x >> 16;
    return x;
}

Real jitter(std::uint32_t seed) noexcept { return static_cast<Real>(hash32(seed) >> 8) * (1.0_r / 16777216.0_r) - 0.5_r; }

int flip_cells_along(Real length, Real cell) noexcept { return std::max(2, static_cast<int>(std::ceil(length / cell - 1e-3_r))); }

inline int ifloor(Real f) noexcept {
    const int i = static_cast<int>(f);
    return i - (f < static_cast<Real>(i) ? 1 : 0);
}

struct Axis {
    int i = 0;
    Real t = 0.0_r;
};

inline Axis axis_at(Real f, int n) noexcept {
    int i = ifloor(f);
    i = std::clamp(i, 0, n - 2);
    return Axis{ i, std::clamp(f - static_cast<Real>(i), 0.0_r, 1.0_r) };
}

inline Real trilinear(const Real* f, int sx, int sy, int sz, Real fx, Real fy, Real fz) noexcept {
    const Axis a = axis_at(fx, sx), b = axis_at(fy, sy), c = axis_at(fz, sz);
    const std::size_t dj = static_cast<std::size_t>(sx) * static_cast<std::size_t>(sz), dk = static_cast<std::size_t>(sx);
    const std::size_t base = (static_cast<std::size_t>(b.i) * static_cast<std::size_t>(sz) + static_cast<std::size_t>(c.i)) * static_cast<std::size_t>(sx) + static_cast<std::size_t>(a.i);
    const Real x00 = f[base] + (f[base + 1] - f[base]) * a.t;
    const Real x01 = f[base + dk] + (f[base + dk + 1] - f[base + dk]) * a.t;
    const Real x10 = f[base + dj] + (f[base + dj + 1] - f[base + dj]) * a.t;
    const Real x11 = f[base + dj + dk] + (f[base + dj + dk + 1] - f[base + dj + dk]) * a.t;
    const Real y0 = x00 + (x01 - x00) * c.t;
    const Real y1 = x10 + (x11 - x10) * c.t;
    return y0 + (y1 - y0) * b.t;
}

struct Lerp2 {
    int i[2];
    Real t[2];
};

inline void lerp_axis(Real f, int cells, Lerp2& out) noexcept {
    int a = static_cast<int>(f + 1.0_r) - 1;
    a = std::clamp(a, 0, cells - 1);
    out.i[0] = a;
    out.t[0] = std::clamp(f - static_cast<Real>(a), 0.0_r, 1.0_r);
    const Real g = f - 0.5_r;
    int b = static_cast<int>(g + 1.0_r) - 1;
    b = std::clamp(b, 0, cells - 2);
    out.i[1] = b;
    out.t[1] = std::clamp(g - static_cast<Real>(b), 0.0_r, 1.0_r);
}

void box_union(int lo[3], int hi[3], const int alo[3], const int ahi[3]) noexcept {
    for (int a = 0; a < 3; ++a) {
        lo[a] = std::min(lo[a], alo[a]);
        hi[a] = std::max(hi[a], ahi[a]);
    }
}

} // namespace

const char* to_string(FluidSolver3D solver) noexcept {
    switch (solver) {
    case FluidSolver3D::flip: return "flip";
    case FluidSolver3D::apic: return "apic";
    case FluidSolver3D::pbf: return "pbf";
    }
    return "unknown";
}

int FluidVolume3D::flip_cell_of(const V3& l) const noexcept {
    const Real inv = 1.0_r / m_cs;
    const int i = std::clamp(ifloor(l.x * inv), 0, m_nx - 1);
    const int j = std::clamp(ifloor(l.y * inv), 0, m_ny - 1);
    const int k = std::clamp(ifloor(l.z * inv), 0, m_nz - 1);
    return static_cast<int>(cell_index(i, j, k));
}

bool FluidVolume3D::flip_static_solid(int i, int j, int k) const noexcept {
    if (!valid_cell(i, j, k)) return m_def.particles.contain;
    return m_solid[cell_index(i, j, k)] != 0;
}

Real FluidVolume3D::flip_sample_u(const V3& l, const std::vector<Real>& f) const noexcept {
    const Real inv = 1.0_r / m_cs;
    return trilinear(f.data(), m_nx + 1, m_ny, m_nz, l.x * inv, l.y * inv - 0.5_r, l.z * inv - 0.5_r);
}

Real FluidVolume3D::flip_sample_v(const V3& l, const std::vector<Real>& f) const noexcept {
    const Real inv = 1.0_r / m_cs;
    return trilinear(f.data(), m_nx, m_ny + 1, m_nz, l.x * inv - 0.5_r, l.y * inv, l.z * inv - 0.5_r);
}

Real FluidVolume3D::flip_sample_w(const V3& l, const std::vector<Real>& f) const noexcept {
    const Real inv = 1.0_r / m_cs;
    return trilinear(f.data(), m_nx, m_ny, m_nz + 1, l.x * inv - 0.5_r, l.y * inv - 0.5_r, l.z * inv);
}

Real FluidVolume3D::flip_sample_cell(const V3& l, const std::vector<Real>& f) const noexcept {
    const Real inv = 1.0_r / m_cs;
    return trilinear(f.data(), m_nx, m_ny, m_nz, l.x * inv - 0.5_r, l.y * inv - 0.5_r, l.z * inv - 0.5_r);
}

V3 FluidVolume3D::flip_velocity_local(const V3& l) const noexcept {
    if (m_gu.empty()) return V3{};
    return V3{ flip_sample_u(l, m_gu), flip_sample_v(l, m_gv), flip_sample_w(l, m_gw) };
}

void FluidVolume3D::flip_refresh_materials() {
    const FluidMaterial& m = m_def.material;
    const FluidParticleSettings& ps = m_def.particles;
    const Real f = m.viscosity_factor() - 1.0_r;
    const Real nu = m.kinematic_viscosity() / std::max(m_cs * m_cs, 1e-12_r);
    m_visc_coef = std::max(0.0_r, ps.viscosity_scale) * (1.8_r * f * f * f + nu);
    m_flip_mix = std::clamp(ps.flip_ratio * (1.0_r - 0.12_r * f), 0.0_r, 1.0_r);
    const Real tension = ps.cohesion >= 0.0_r ? ps.cohesion * 10.0_r : std::max(0.0_r, m.surface_tension / 0.072_r - 1.0_r);
    m_cohesion = std::clamp(std::max(0.0_r, ps.surface_tension_scale) * tension * 0.6_r, 0.0_r, 8.0_r);
}

void FluidVolume3D::init_flip() {
    const FluidParticleSettings& ps = m_def.particles;
    m_solver = ps.solver;
    m_radius = std::max(1e-3_r, ps.radius * m_quality.resolution_scale);
    m_cs = 2.0_r * m_radius;
    const Real smallest = std::min({ m_size.x, m_size.y, m_size.z });
    if (smallest < 2.0_r * m_cs) m_cs = std::max(1e-4_r, 0.5_r * smallest);
    for (int guard = 0; guard < 64; ++guard) {
        m_nx = flip_cells_along(m_size.x, m_cs);
        m_ny = flip_cells_along(m_size.y, m_cs);
        m_nz = flip_cells_along(m_size.z, m_cs);
        if (static_cast<double>(m_nx) * m_ny * m_nz <= 4194304.0) break;
        m_cs *= 1.25_r;
    }
    m_spacing = 0.5_r * m_cs;
    m_pr = 0.5_r * m_spacing;
    const Real sizes[3] = { m_size.x, m_size.y, m_size.z };
    const int dims[3] = { m_nx, m_ny, m_nz };
    for (int a = 0; a < 3; ++a) {
        int lim = 0;
        while (lim < dims[a] && (static_cast<Real>(lim) + 0.5_r) * m_cs < sizes[a] - 1e-4_r * m_cs) ++lim;
        m_flim[a] = std::max(1, lim);
    }
    m_h = m_cs;
    m_gcell = m_cs;
    m_pmass = m_def.material.density * m_spacing * m_spacing * m_spacing;
    const std::size_t n = static_cast<std::size_t>(m_nx) * static_cast<std::size_t>(m_ny) * static_cast<std::size_t>(m_nz);
    const std::size_t nu = static_cast<std::size_t>(m_nx + 1) * static_cast<std::size_t>(m_ny) * static_cast<std::size_t>(m_nz);
    const std::size_t nv = static_cast<std::size_t>(m_nx) * static_cast<std::size_t>(m_ny + 1) * static_cast<std::size_t>(m_nz);
    const std::size_t nw = static_cast<std::size_t>(m_nx) * static_cast<std::size_t>(m_ny) * static_cast<std::size_t>(m_nz + 1);
    m_solid.assign(n, 0);
    m_source.assign(n, 0);
    m_fill.assign(n, 0.0_r);
    m_cell_vel.assign(n, V3{});
    m_lphi.assign(n, kAirPhi);
    m_lphi_adv.assign(n, kAirPhi);
    m_ltype.assign(n, kAir);
    m_ldens.assign(n, 0.0_r);
    m_lpress.assign(n, 0.0_r);
    m_locc.assign(n, 0);
    m_locc_vel.assign(n, V3{});
    m_lidx.assign(n, -1);
    m_lcount.assign(n, 0);
    m_gu.assign(nu, 0.0_r);
    m_gu0.assign(nu, 0.0_r);
    m_guw.assign(nu, 0.0_r);
    m_gua.assign(nu, 0.0_r);
    m_gv.assign(nv, 0.0_r);
    m_gv0.assign(nv, 0.0_r);
    m_gvw.assign(nv, 0.0_r);
    m_gva.assign(nv, 0.0_r);
    m_gw.assign(nw, 0.0_r);
    m_gw0.assign(nw, 0.0_r);
    m_gww.assign(nw, 0.0_r);
    m_gwa.assign(nw, 0.0_r);
    m_gscratch.assign(std::max({ nu, nv, nw }), 0.0_r);
    m_lsolid_nbr.assign(n, 0);
    m_fvalid_u.assign(nu, 0);
    m_fvalid_v.assign(nv, 0);
    m_fvalid_w.assign(nw, 0);
    m_max_particles = ps.max_particles > 0 ? ps.max_particles : std::max<std::size_t>(4000, m_quality.max_particles * 4);
    const std::size_t reserve = std::min<std::size_t>(m_max_particles, 32768);
    m_fx.reserve(reserve);
    m_fv.reserve(reserve);
    m_px.reserve(reserve);
    m_pv.reserve(reserve);
    m_density.reserve(reserve);
    flip_refresh_materials();
    m_work.lo[0] = m_work.lo[1] = m_work.lo[2] = 0;
    m_work.hi[0] = m_nx - 1;
    m_work.hi[1] = m_ny - 1;
    m_work.hi[2] = m_nz - 1;
    m_box = m_work;
    m_box_prev = m_work;
    if (m_def.fill > 0.0_r) {
        const Real top = std::min(m_def.fill, 1.0_r) * m_size.y;
        const Real band = ps.narrow_band ? std::max(1.5_r, ps.band_cells) : 1.0e9_r;
        for (int j = 0; j < m_ny; ++j) {
            const Real cy = (static_cast<Real>(j) + 0.5_r) * m_cs;
            const Real depth = (top - cy) / m_cs;
            if (depth <= -0.5_r) break;
            for (int k = 0; k < m_nz; ++k) {
                for (int i = 0; i < m_nx; ++i) {
                    const std::size_t c = cell_index(i, j, k);
                    m_lphi[c] = depth > 0.0_r ? -depth : kAirPhi;
                    if (depth > band + 0.5_r) continue;
                    for (int s = 0; s < kPerCell; ++s) {
                        const std::uint32_t seed = static_cast<std::uint32_t>(c * 8u + static_cast<std::size_t>(s));
                        const V3 l{ (static_cast<Real>(i) + 0.25_r + 0.5_r * static_cast<Real>(s & 1) + 0.1_r * jitter(seed)) * m_cs,
                                    (static_cast<Real>(j) + 0.25_r + 0.5_r * static_cast<Real>((s >> 1) & 1) + 0.1_r * jitter(seed ^ 0x9e37u)) * m_cs,
                                    (static_cast<Real>(k) + 0.25_r + 0.5_r * static_cast<Real>((s >> 2) & 1) + 0.1_r * jitter(seed ^ 0x51edu)) * m_cs };
                        if (l.y >= top) continue;
                        if (m_fx.size() >= m_max_particles) break;
                        flip_push(l, V3{});
                    }
                }
            }
        }
    }
    m_lphi_adv = m_lphi;
    flip_active_box();
    flip_sort();
    flip_classify();
    flip_redistance();
    flip_finish_frame();
}

void FluidVolume3D::flip_resize_particles(std::size_t n) {
    m_fx.resize(n);
    m_fv.resize(n);
    if (m_solver == FluidSolver3D::apic) {
        m_fcu.resize(n);
        m_fcv.resize(n);
        m_fcw.resize(n);
    }
}

void FluidVolume3D::flip_push(const V3& l, const V3& lv) {
    m_fx.push_back(l);
    m_fv.push_back(lv);
    if (m_solver == FluidSolver3D::apic) {
        m_fcu.push_back(V3{});
        m_fcv.push_back(V3{});
        m_fcw.push_back(V3{});
    }
    m_px.push_back(to_world(l));
    m_pv.push_back(world_dir(lv));
    m_density.push_back(m_def.material.density);
}

void FluidVolume3D::flip_rasterize_bodies() {
    for (std::uint32_t c : m_locc_list) {
        m_locc[c] = 2;
        m_locc_vel[c] = V3{};
    }
    auto was_occupied = [&](std::size_t c) { return m_locc[c] == 2; };
    for (NearBody& nb : m_near) {
        V3 lo{ std::numeric_limits<Real>::max(), std::numeric_limits<Real>::max(), std::numeric_limits<Real>::max() };
        V3 hi = lo * -1.0_r;
        for (int q = 0; q < 8; ++q) {
            const V3 w{ (q & 1) ? nb.box.max.x : nb.box.min.x, (q & 2) ? nb.box.max.y : nb.box.min.y, (q & 4) ? nb.box.max.z : nb.box.min.z };
            const V3 l = to_local(w);
            lo = vec3::min(lo, l);
            hi = vec3::max(hi, l);
        }
        nb.local_lo = lo - V3{ m_pr, m_pr, m_pr };
        nb.local_hi = hi + V3{ m_pr, m_pr, m_pr };
        nb.max_rel = 0.0_r;
        nb.impulse = V3{};
        nb.angular = V3{};
        const RigidBody3D& b = *nb.body;
        for (const Collider3D& c : b.colliders()) {
            if (c.is_sensor || c.shape.type == ShapeType3D::Mesh) continue;
            V3 clo{ std::numeric_limits<Real>::max(), std::numeric_limits<Real>::max(), std::numeric_limits<Real>::max() };
            V3 chi = clo * -1.0_r;
            for (int q = 0; q < 8; ++q) {
                const V3 w{ (q & 1) ? c.world_aabb.max.x : c.world_aabb.min.x, (q & 2) ? c.world_aabb.max.y : c.world_aabb.min.y, (q & 4) ? c.world_aabb.max.z : c.world_aabb.min.z };
                const V3 l = to_local(w);
                clo = vec3::min(clo, l);
                chi = vec3::max(chi, l);
            }
            const Real inv = 1.0_r / m_cs;
            const int i0 = std::max(0, ifloor(clo.x * inv - 0.5_r)), i1 = std::min(m_flim[0] - 1, static_cast<int>(std::ceil(chi.x * inv - 0.5_r)));
            const int j0 = std::max(0, ifloor(clo.y * inv - 0.5_r)), j1 = std::min(m_flim[1] - 1, static_cast<int>(std::ceil(chi.y * inv - 0.5_r)));
            const int k0 = std::max(0, ifloor(clo.z * inv - 0.5_r)), k1 = std::min(m_flim[2] - 1, static_cast<int>(std::ceil(chi.z * inv - 0.5_r)));
            if (i0 > i1 || j0 > j1 || k0 > k1) continue;
            const Transform3D xf = b.transform().compose(c.local_offset);
            for (int j = j0; j <= j1; ++j) {
                for (int k = k0; k <= k1; ++k) {
                    for (int i = i0; i <= i1; ++i) {
                        const std::size_t ci = cell_index(i, j, k);
                        if (m_locc[ci] == 3) continue;
                        const V3 w = cell_center(i, j, k);
                        if (!c.world_aabb.contains(w)) continue;
                        const V3 lp = xf.apply_inverse(w);
                        bool in = false;
                        if (c.shape.type == ShapeType3D::Plane) in = vec3::dot(lp, c.shape.plane.normal) <= c.shape.plane.offset;
                        else if (c.shape.type == ShapeType3D::Heightfield) in = c.shape.heightfield && lp.y <= c.shape.heightfield->height_at(lp.x, lp.z);
                        else in = detail::fluid_point_in_shape(c.shape, lp);
                        if (!in) continue;
                        if (!nb.dynamic && (m_ltype[ci] & kTypeMask) == kLiquid && !was_occupied(ci) && vec3::length_squared(b.linear_velocity()) < 1e-6_r) m_target_volume -= (m_ltype[ci] & kDeep) ? std::max<double>(kPerCell, m_lcount[ci]) : static_cast<double>(m_lcount[ci]);
                        m_locc[ci] = 3;
                        m_locc_vel[ci] = local_dir(b.get_velocity_at(w));
                        m_locc_list.push_back(static_cast<std::uint32_t>(ci));
                    }
                }
            }
        }
    }
    std::size_t w = 0;
    for (std::size_t q = 0; q < m_locc_list.size(); ++q) {
        const std::uint32_t c = m_locc_list[q];
        if (m_locc[c] == 3) {
            m_locc[c] = 1;
            m_locc_list[w++] = c;
        } else if (m_locc[c] == 2) {
            m_locc[c] = 0;
        }
    }
    m_locc_list.resize(w);
}

void FluidVolume3D::flip_active_box() {
    int lo[3] = { std::numeric_limits<int>::max(), std::numeric_limits<int>::max(), std::numeric_limits<int>::max() };
    int hi[3] = { std::numeric_limits<int>::min(), std::numeric_limits<int>::min(), std::numeric_limits<int>::min() };
    const Real inv = 1.0_r / m_cs;
    for (const V3& p : m_fx) {
        const int c[3] = { ifloor(p.x * inv), ifloor(p.y * inv), ifloor(p.z * inv) };
        for (int a = 0; a < 3; ++a) {
            lo[a] = std::min(lo[a], c[a]);
            hi[a] = std::max(hi[a], c[a]);
        }
    }
    if (!m_work.empty()) {
        for (int j = m_work.lo[1]; j <= m_work.hi[1]; ++j) {
            for (int k = m_work.lo[2]; k <= m_work.hi[2]; ++k) {
                const std::size_t row = cell_index(0, j, k);
                for (int i = m_work.lo[0]; i <= m_work.hi[0]; ++i) {
                    if (m_lphi[row + static_cast<std::size_t>(i)] >= 0.0_r) continue;
                    lo[0] = std::min(lo[0], i);
                    hi[0] = std::max(hi[0], i);
                    lo[1] = std::min(lo[1], j);
                    hi[1] = std::max(hi[1], j);
                    lo[2] = std::min(lo[2], k);
                    hi[2] = std::max(hi[2], k);
                }
            }
        }
    }
    const int dims[3] = { m_nx, m_ny, m_nz };
    if (lo[0] > hi[0]) {
        m_box = CellBox{};
    } else {
        const int margin = 3;
        for (int a = 0; a < 3; ++a) {
            m_box.lo[a] = std::max(0, lo[a] - margin);
            m_box.hi[a] = std::min(dims[a] - 1, hi[a] + margin);
        }
    }
    if (m_box.empty()) {
        m_work = m_box_prev;
    } else if (m_box_prev.empty()) {
        m_work = m_box;
    } else {
        m_work = m_box;
        box_union(m_work.lo, m_work.hi, m_box_prev.lo, m_box_prev.hi);
    }
    m_box_prev = m_box;
}

void FluidVolume3D::flip_advect_particles(detail::FluidTaskPool& pool, Real h) {
    const std::size_t n = m_fx.size();
    m_fprev.resize(n);
    m_fdv.resize(n);
    m_fcontact.resize(n);
    m_fspray.resize(n, 1);
    const Real r = m_pr;
    const Real cs = m_cs;
    const Real inv = 1.0_r / cs;
    const bool contain = m_def.particles.contain;
    const bool walls = m_solid_count > 0;
    const V3 g{ 0.0_r, -m_g * h, 0.0_r };
    const FluidMaterial& mat = m_def.material;
    const int nx = m_nx, ny = m_ny, nz = m_nz;
    const Real sweep_r = 0.5_r * cs;
    auto blocked = [&](int i, int j, int k) -> bool {
        if (i < 0 || j < 0 || k < 0 || i >= nx || j >= ny || k >= nz) return false;
        return m_solid[cell_index(i, j, k)] != 0;
    };
    auto respond = [&](const NearBody& nb, const Collider3D& col, const V3& wp, const V3& nrm, V3& v) {
        const V3 ln = local_dir(nrm);
        const V3 vb = local_dir(nb.body->get_velocity_at(wp));
        const V3 rel = v - vb;
        const Real vn = vec3::dot(rel, ln);
        if (vn >= 0.0_r) return;
        const Real e = -vn > std::min(0.3_r, col.material.restitution_threshold) ? std::max(mat.restitution, col.material.restitution) : 0.0_r;
        const Real mu = std::sqrt(std::max(mat.friction, 0.0_r) * std::max(col.material.friction, 0.0_r));
        V3 vt = rel - ln * vn;
        const Real lt = vec3::length(vt);
        if (lt > 1e-9_r) vt = vt * std::max(0.0_r, 1.0_r - mu * (-vn) * (1.0_r + e) / lt);
        v = vb + vt - ln * (vn * e);
    };
    auto wall = [&](int i, int j, int k) -> bool {
        if (i < 0 || j < 0 || k < 0 || i >= nx || j >= ny || k >= nz) return contain;
        return m_solid[cell_index(i, j, k)] != 0;
    };
    pool.parallel_range(n, 1024, [&](std::size_t b0, std::size_t b1) {
        for (std::size_t p = b0; p < b1; ++p) {
            V3 x = m_fx[p];
            V3 v = m_fv[p];
            if (m_fspray[p]) v = v + g;
            const V3 a = x;
            m_fprev[p] = a;
            x = x + v * h;
            const V3 v_in = v;
            std::int32_t contact = -1;
            const V3 d = x - a;
            const Real len = std::max({ std::abs(d.x), std::abs(d.y), std::abs(d.z) });
            if (walls && len > 0.5_r * cs) {
                const int steps = std::min(64, static_cast<int>(std::ceil(len * inv * 2.0_r)));
                const Real st = 1.0_r / static_cast<Real>(steps);
                int pi = ifloor(a.x * inv), pj = ifloor(a.y * inv), pk = ifloor(a.z * inv);
                for (int q = 1; q <= steps; ++q) {
                    const V3 sp = a + d * (st * static_cast<Real>(q));
                    const int qi = ifloor(sp.x * inv), qj = ifloor(sp.y * inv), qk = ifloor(sp.z * inv);
                    if (qi == pi && qj == pj && qk == pk) continue;
                    if (blocked(qi, qj, qk)) {
                        x = a + d * (st * static_cast<Real>(q - 1));
                        if (qi != pi) v.x = 0.0_r;
                        if (qj != pj) v.y = 0.0_r;
                        if (qk != pk) v.z = 0.0_r;
                        break;
                    }
                    pi = qi;
                    pj = qj;
                    pk = qk;
                }
            }
            if (!m_near.empty() && len > r) {
                const V3 lo = vec3::min(a, x), hi = vec3::max(a, x);
                const int steps = std::min(64, static_cast<int>(std::ceil(len / sweep_r)));
                bool hit = false;
                for (std::size_t k = 0; k < m_near.size() && !hit; ++k) {
                    const NearBody& nb = m_near[k];
                    if (hi.x < nb.local_lo.x - sweep_r || hi.y < nb.local_lo.y - sweep_r || hi.z < nb.local_lo.z - sweep_r || lo.x > nb.local_hi.x + sweep_r || lo.y > nb.local_hi.y + sweep_r || lo.z > nb.local_hi.z + sweep_r) continue;
                    const auto& cols = nb.body->colliders();
                    bool resting = false;
                    {
                        const V3 wa = to_world(a);
                        for (std::size_t c = 0; c < cols.size() && !resting; ++c) {
                            if (cols[c].is_sensor || !cols[c].world_aabb.fatten(sweep_r).contains(wa)) continue;
                            V3 nrm{};
                            Real depth = 0.0_r;
                            if (detail::fluid_collider_contact(cols[c], nb.body->transform(), wa, sweep_r, nrm, depth) && sweep_r - depth < 1.5_r * r) resting = true;
                        }
                    }
                    if (resting) continue;
                    for (int q = 1; q <= steps && !hit; ++q) {
                        const V3 sp = a + (x - a) * (static_cast<Real>(q) / static_cast<Real>(steps));
                        const V3 wp = to_world(sp);
                        for (std::size_t c = 0; c < cols.size(); ++c) {
                            if (cols[c].is_sensor || !cols[c].world_aabb.fatten(sweep_r).contains(wp)) continue;
                            V3 nrm{};
                            Real depth = 0.0_r;
                            if (!detail::fluid_collider_contact(cols[c], nb.body->transform(), wp, sweep_r, nrm, depth)) continue;
                            const Real gap = sweep_r - depth;
                            if (gap >= r) continue;
                            const V3 wq = wp - nrm * (gap - r);
                            x = to_local(wq);
                            respond(nb, cols[c], wq, nrm, v);
                            contact = static_cast<std::int32_t>(k);
                            hit = true;
                            break;
                        }
                    }
                }
            }
            V3 wn{};
            if (contain) {
                if (x.x < r) { x.x = r; wn.x += 1.0_r; }
                if (x.x > m_size.x - r) { x.x = m_size.x - r; wn.x -= 1.0_r; }
                if (x.y < r) { x.y = r; wn.y += 1.0_r; }
                if (x.y > m_size.y - r) { x.y = m_size.y - r; wn.y -= 1.0_r; }
                if (x.z < r) { x.z = r; wn.z += 1.0_r; }
                if (x.z > m_size.z - r) { x.z = m_size.z - r; wn.z -= 1.0_r; }
            }
            if (walls) {
                int ci = std::clamp(ifloor(x.x * inv), 0, nx - 1);
                int cj = std::clamp(ifloor(x.y * inv), 0, ny - 1);
                int ck = std::clamp(ifloor(x.z * inv), 0, nz - 1);
                if (m_solid[cell_index(ci, cj, ck)]) {
                    const Real e = 1e-3_r * cs;
                    const Real dist[6] = { x.x - static_cast<Real>(ci) * cs, static_cast<Real>(ci + 1) * cs - x.x, x.y - static_cast<Real>(cj) * cs, static_cast<Real>(cj + 1) * cs - x.y, x.z - static_cast<Real>(ck) * cs, static_cast<Real>(ck + 1) * cs - x.z };
                    const int oi[6] = { -1, 1, 0, 0, 0, 0 }, oj[6] = { 0, 0, -1, 1, 0, 0 }, ok[6] = { 0, 0, 0, 0, -1, 1 };
                    int best = -1;
                    Real bd = std::numeric_limits<Real>::max();
                    for (int q = 0; q < 6; ++q) {
                        if (wall(ci + oi[q], cj + oj[q], ck + ok[q]) || dist[q] >= bd) continue;
                        bd = dist[q];
                        best = q;
                    }
                    if (best < 0) {
                        const int pi = std::clamp(ifloor(a.x * inv), 0, nx - 1);
                        const int pj = std::clamp(ifloor(a.y * inv), 0, ny - 1);
                        const int pk = std::clamp(ifloor(a.z * inv), 0, nz - 1);
                        if (!m_solid[cell_index(pi, pj, pk)]) {
                            x = a;
                            ci = pi;
                            cj = pj;
                            ck = pk;
                        }
                        v = V3{};
                    } else {
                        if (best == 0) { x.x = static_cast<Real>(ci) * cs - e; --ci; wn.x -= 1.0_r; }
                        else if (best == 1) { x.x = static_cast<Real>(ci + 1) * cs + e; ++ci; wn.x += 1.0_r; }
                        else if (best == 2) { x.y = static_cast<Real>(cj) * cs - e; --cj; wn.y -= 1.0_r; }
                        else if (best == 3) { x.y = static_cast<Real>(cj + 1) * cs + e; ++cj; wn.y += 1.0_r; }
                        else if (best == 4) { x.z = static_cast<Real>(ck) * cs - e; --ck; wn.z -= 1.0_r; }
                        else { x.z = static_cast<Real>(ck + 1) * cs + e; ++ck; wn.z += 1.0_r; }
                    }
                }
                if (wall(ci - 1, cj, ck) && x.x < static_cast<Real>(ci) * cs + r) { x.x = static_cast<Real>(ci) * cs + r; wn.x += 1.0_r; }
                if (wall(ci + 1, cj, ck) && x.x > static_cast<Real>(ci + 1) * cs - r) { x.x = static_cast<Real>(ci + 1) * cs - r; wn.x -= 1.0_r; }
                if (wall(ci, cj - 1, ck) && x.y < static_cast<Real>(cj) * cs + r) { x.y = static_cast<Real>(cj) * cs + r; wn.y += 1.0_r; }
                if (wall(ci, cj + 1, ck) && x.y > static_cast<Real>(cj + 1) * cs - r) { x.y = static_cast<Real>(cj + 1) * cs - r; wn.y -= 1.0_r; }
                if (wall(ci, cj, ck - 1) && x.z < static_cast<Real>(ck) * cs + r) { x.z = static_cast<Real>(ck) * cs + r; wn.z += 1.0_r; }
                if (wall(ci, cj, ck + 1) && x.z > static_cast<Real>(ck + 1) * cs - r) { x.z = static_cast<Real>(ck + 1) * cs - r; wn.z -= 1.0_r; }
            }
            if (wn.x != 0.0_r || wn.y != 0.0_r || wn.z != 0.0_r) {
                const V3 nn = vec3::normalize(wn);
                const Real vn = vec3::dot(v, nn);
                if (vn < 0.0_r) {
                    const Real e = -vn > 0.5_r ? mat.restitution : 0.0_r;
                    V3 vt = v - nn * vn;
                    const Real lt = vec3::length(vt);
                    if (lt > 1e-9_r) vt = vt * std::max(0.0_r, 1.0_r - mat.friction * (-vn) * (1.0_r + e) / lt);
                    v = vt - nn * (vn * e);
                }
            }
            Real best_depth = -1.0_r;
            for (std::size_t k = 0; k < m_near.size(); ++k) {
                const NearBody& nb = m_near[k];
                if (x.x < nb.local_lo.x || x.y < nb.local_lo.y || x.z < nb.local_lo.z || x.x > nb.local_hi.x || x.y > nb.local_hi.y || x.z > nb.local_hi.z) continue;
                const auto& cols = nb.body->colliders();
                V3 wp = to_world(x);
                for (std::size_t c = 0; c < cols.size(); ++c) {
                    if (cols[c].is_sensor || !cols[c].world_aabb.fatten(r).contains(wp)) continue;
                    V3 nrm{};
                    Real depth = 0.0_r;
                    if (!detail::fluid_collider_contact(cols[c], nb.body->transform(), wp, r, nrm, depth)) continue;
                    wp = wp + nrm * depth;
                    x = to_local(wp);
                    respond(nb, cols[c], wp, nrm, v);
                    if (depth > best_depth) {
                        best_depth = depth;
                        contact = static_cast<std::int32_t>(k);
                    }
                }
            }
            if (contain) {
                x.x = std::clamp(x.x, 0.5_r * r, m_size.x - 0.5_r * r);
                x.y = std::clamp(x.y, 0.5_r * r, m_size.y - 0.5_r * r);
                x.z = std::clamp(x.z, 0.5_r * r, m_size.z - 0.5_r * r);
            }
            m_fx[p] = x;
            m_fv[p] = v;
            m_fdv[p] = v - v_in;
            m_fcontact[p] = contact;
        }
    });
    if (m_near.empty()) return;
    const Real push = m_def.coupling.two_way ? m_def.coupling.push_scale : 0.0_r;
    std::size_t contacts = 0;
    for (std::size_t p = 0; p < n; ++p) {
        const std::int32_t k = m_fcontact[p];
        if (k < 0) continue;
        ++contacts;
        NearBody& nb = m_near[static_cast<std::size_t>(k)];
        if (!nb.dynamic || push <= 0.0_r) continue;
        const V3 J = world_dir(m_fdv[p]) * (-m_pmass * push);
        nb.impulse = nb.impulse + J;
        nb.angular = nb.angular + vec3::cross(to_world(m_fx[p]) - nb.body->world_center(), J);
        nb.max_rel = std::max(nb.max_rel, vec3::length(m_fdv[p]));
    }
    m_contact_count += contacts;
}

void FluidVolume3D::flip_body_impulses() {
    for (NearBody& nb : m_near) {
        if (!nb.dynamic || (vec3::is_zero(nb.impulse) && vec3::is_zero(nb.angular))) continue;
        RigidBody3D& b = *nb.body;
        const Real dv = vec3::length(nb.impulse) * b.inv_mass();
        Real s = 1.0_r;
        if (dv > nb.max_rel && dv > 0.0_r) s = nb.max_rel / dv;
        b.apply_linear_impulse(nb.impulse * s);
        const Real L = vec3::length(nb.box.extents());
        const Real dw = vec3::length(b.apply_inv_inertia(nb.angular * s));
        Real sa = 1.0_r;
        if (dw * L > nb.max_rel && dw > 0.0_r) sa = nb.max_rel / (dw * L);
        b.apply_angular_impulse(nb.angular * (s * sa));
        b.wake();
        nb.impulse = V3{};
        nb.angular = V3{};
        nb.max_rel = 0.0_r;
    }
}

void FluidVolume3D::flip_advect_grid(detail::FluidTaskPool& pool, Real h) {
    if (m_work.empty()) return;
    const CellBox& wb = m_work;
    const Real cs = m_cs;
    const Real inv = 1.0_r / cs;
    const int nx = m_nx, ny = m_ny, nz = m_nz;
    const Real core = m_def.particles.narrow_band ? std::max(1.5_r, m_def.particles.band_cells) : 1.0e9_r;
    const std::size_t rows = static_cast<std::size_t>(wb.hi[1] - wb.lo[1] + 1);
    pool.parallel_for(rows, [&](std::size_t r) {
        const int j = wb.lo[1] + static_cast<int>(r);
        for (int k = wb.lo[2]; k <= wb.hi[2]; ++k) {
            for (int i = wb.lo[0]; i <= wb.hi[0]; ++i) {
                const std::size_t c = cell_index(i, j, k);
                const Real here = m_lphi[c];
                if (here >= kAirPhi) {
                    bool any = false;
                    for (int q = 0; q < 6 && !any; ++q) {
                        const int a = i + (q == 0 ? -1 : q == 1 ? 1 : 0), b = j + (q == 2 ? -1 : q == 3 ? 1 : 0), d = k + (q == 4 ? -1 : q == 5 ? 1 : 0);
                        if (a < 0 || b < 0 || d < 0 || a >= nx || b >= ny || d >= nz) continue;
                        any = m_lphi[cell_index(a, b, d)] < kAirPhi;
                    }
                    if (!any) {
                        m_lphi_adv[c] = kAirPhi;
                        continue;
                    }
                }
                const V3 vel{ 0.5_r * (m_gu[ucell(i, j, k)] + m_gu[ucell(i + 1, j, k)]), 0.5_r * (m_gv[vcell(i, j, k)] + m_gv[vcell(i, j + 1, k)]), 0.5_r * (m_gw[wcell(i, j, k)] + m_gw[wcell(i, j, k + 1)]) };
                const V3 at{ (static_cast<Real>(i) + 0.5_r) * cs - vel.x * h, (static_cast<Real>(j) + 0.5_r) * cs - vel.y * h, (static_cast<Real>(k) + 0.5_r) * cs - vel.z * h };
                m_lphi_adv[c] = std::min(kAirPhi, trilinear(m_lphi.data(), nx, ny, nz, at.x * inv - 0.5_r, at.y * inv - 0.5_r, at.z * inv - 0.5_r));
            }
        }
    });
    if (!m_def.particles.narrow_band) return;
    auto deep = [&](int i, int j, int k) -> bool {
        if (i < 0 || j < 0 || k < 0 || i >= nx || j >= ny || k >= nz) return false;
        return m_lphi_adv[cell_index(i, j, k)] < -core;
    };
    pool.parallel_for(rows + 1, [&](std::size_t r) {
        const int j = wb.lo[1] + static_cast<int>(r);
        for (int k = wb.lo[2]; k <= wb.hi[2] + 1; ++k) {
            for (int i = wb.lo[0]; i <= wb.hi[0] + 1; ++i) {
                if (j <= wb.hi[1] && k <= wb.hi[2] && (deep(i - 1, j, k) || deep(i, j, k))) {
                    const V3 at{ static_cast<Real>(i) * cs, (static_cast<Real>(j) + 0.5_r) * cs, (static_cast<Real>(k) + 0.5_r) * cs };
                    const V3 vel = flip_velocity_local(at);
                    m_gua[ucell(i, j, k)] = flip_sample_u(at - vel * h, m_gu);
                }
                if (i <= wb.hi[0] && k <= wb.hi[2] && (deep(i, j - 1, k) || deep(i, j, k))) {
                    const V3 at{ (static_cast<Real>(i) + 0.5_r) * cs, static_cast<Real>(j) * cs, (static_cast<Real>(k) + 0.5_r) * cs };
                    const V3 vel = flip_velocity_local(at);
                    m_gva[vcell(i, j, k)] = flip_sample_v(at - vel * h, m_gv);
                }
                if (i <= wb.hi[0] && j <= wb.hi[1] && (deep(i, j, k - 1) || deep(i, j, k))) {
                    const V3 at{ (static_cast<Real>(i) + 0.5_r) * cs, (static_cast<Real>(j) + 0.5_r) * cs, static_cast<Real>(k) * cs };
                    const V3 vel = flip_velocity_local(at);
                    m_gwa[wcell(i, j, k)] = flip_sample_w(at - vel * h, m_gw);
                }
            }
        }
    });
}

void FluidVolume3D::flip_sort() {
    const std::size_t n = m_fx.size();
    if (m_work.empty()) {
        flip_resize_particles(0);
        return;
    }
    const CellBox& wb = m_work;
    const int bx = wb.hi[0] - wb.lo[0] + 1, by = wb.hi[1] - wb.lo[1] + 1, bz = wb.hi[2] - wb.lo[2] + 1;
    const std::uint32_t N = static_cast<std::uint32_t>(bx) * static_cast<std::uint32_t>(by) * static_cast<std::uint32_t>(bz);
    m_cstart.assign(static_cast<std::size_t>(N) + 2, 0u);
    m_fkey.resize(n);
    const Real inv = 1.0_r / m_cs;
    const bool contain = m_def.particles.contain;
    for (std::size_t p = 0; p < n; ++p) {
        const V3 x = m_fx[p];
        std::uint32_t key = N;
        if (std::isfinite(x.x) && std::isfinite(x.y) && std::isfinite(x.z)) {
            const bool out = x.x < 0.0_r || x.y < 0.0_r || x.z < 0.0_r || x.x > m_size.x || x.y > m_size.y || x.z > m_size.z;
            if (!out || contain) {
                const int i = std::clamp(ifloor(x.x * inv), 0, m_nx - 1) - wb.lo[0];
                const int j = std::clamp(ifloor(x.y * inv), 0, m_ny - 1) - wb.lo[1];
                const int k = std::clamp(ifloor(x.z * inv), 0, m_nz - 1) - wb.lo[2];
                if (i >= 0 && j >= 0 && k >= 0 && i < bx && j < by && k < bz) key = (static_cast<std::uint32_t>(j) * static_cast<std::uint32_t>(bz) + static_cast<std::uint32_t>(k)) * static_cast<std::uint32_t>(bx) + static_cast<std::uint32_t>(i);
            }
        }
        m_fkey[p] = key;
        ++m_cstart[static_cast<std::size_t>(key) + 1];
    }
    for (std::uint32_t c = 0; c <= N; ++c) m_cstart[static_cast<std::size_t>(c) + 1] += m_cstart[c];
    m_cfill.assign(m_cstart.begin(), m_cstart.end());
    const bool apic = m_solver == FluidSolver3D::apic;
    m_ftmp_a.resize(n);
    m_ftmp_b.resize(n);
    if (apic) {
        m_ftmp_c.resize(n);
        m_ftmp_d.resize(n);
        m_ftmp_e.resize(n);
    }
    for (std::size_t p = 0; p < n; ++p) {
        const std::uint32_t d = m_cfill[m_fkey[p]]++;
        m_ftmp_a[d] = m_fx[p];
        m_ftmp_b[d] = m_fv[p];
        if (apic) {
            m_ftmp_c[d] = m_fcu[p];
            m_ftmp_d[d] = m_fcv[p];
            m_ftmp_e[d] = m_fcw[p];
        }
    }
    m_fx.swap(m_ftmp_a);
    m_fv.swap(m_ftmp_b);
    if (apic) {
        m_fcu.swap(m_ftmp_c);
        m_fcv.swap(m_ftmp_d);
        m_fcw.swap(m_ftmp_e);
    }
    const std::size_t alive = m_cstart[N];
    if (alive != n) {
        flip_resize_particles(alive);
        m_target_volume -= static_cast<double>(n - alive);
    }
    for (int j = wb.lo[1]; j <= wb.hi[1]; ++j) {
        for (int k = wb.lo[2]; k <= wb.hi[2]; ++k) {
            const std::uint32_t row = (static_cast<std::uint32_t>(j - wb.lo[1]) * static_cast<std::uint32_t>(bz) + static_cast<std::uint32_t>(k - wb.lo[2])) * static_cast<std::uint32_t>(bx);
            const std::size_t g = cell_index(wb.lo[0], j, k);
            for (int i = 0; i < bx; ++i) {
                const std::uint32_t cnt = m_cstart[row + static_cast<std::uint32_t>(i) + 1] - m_cstart[row + static_cast<std::uint32_t>(i)];
                m_lcount[g + static_cast<std::size_t>(i)] = static_cast<std::uint16_t>(std::min<std::uint32_t>(cnt, 65535u));
            }
        }
    }
}

bool FluidVolume3D::flip_band(Real h) {
    static_cast<void>(h);
    const FluidParticleSettings& ps = m_def.particles;
    if (!ps.narrow_band || m_work.empty()) return false;
    const Real band = std::max(1.5_r, ps.band_cells);
    const std::size_t n = m_fx.size();
    std::size_t w = 0;
    const bool apic = m_solver == FluidSolver3D::apic;
    m_fspray.resize(n);
    for (std::size_t p = 0; p < n; ++p) {
        const std::size_t c = static_cast<std::size_t>(flip_cell_of(m_fx[p]));
        const int cnt = m_lcount[c];
        const bool was_deep = (m_ltype[c] & kDeep) != 0;
        m_fspray[p] = m_lphi_adv[c] < -band && m_lphi[c] < 1.0_r - band && cnt <= kPerCell + 1 && (was_deep || cnt >= kPerCell - 2) && !m_solid[c] && !m_locc[c] ? 1 : 0;
    }
    for (std::size_t p = 0; p < n; ++p) {
        if (m_fspray[p]) {
            const std::size_t c = static_cast<std::size_t>(flip_cell_of(m_fx[p]));
            if (m_lcount[c] > 0) --m_lcount[c];
            continue;
        }
        if (w != p) {
            m_fx[w] = m_fx[p];
            m_fv[w] = m_fv[p];
            if (apic) {
                m_fcu[w] = m_fcu[p];
                m_fcv[w] = m_fcv[p];
                m_fcw[w] = m_fcw[p];
            }
        }
        ++w;
    }
    bool changed = w != n;
    if (changed) flip_resize_particles(w);
    if (!ps.reseed) return changed;
    const std::size_t budget = std::max<std::size_t>(4096, w);
    std::size_t added = 0;
    const Real cs = m_cs;
    const CellBox& bb = m_work;
    for (int j = bb.lo[1]; j <= bb.hi[1] && added < budget; ++j) {
        for (int k = bb.lo[2]; k <= bb.hi[2] && added < budget; ++k) {
            for (int i = bb.lo[0]; i <= bb.hi[0] && added < budget; ++i) {
                const std::size_t c = cell_index(i, j, k);
                const Real phi = m_lphi_adv[c];
                if (phi < -band || m_solid[c] || m_locc[c]) continue;
                const int cnt = m_lcount[c];
                const bool was_deep = (m_ltype[c] & kDeep) != 0;
                if (!was_deep && (phi > -(band - 1.0_r) || cnt >= 3)) continue;
                if (cnt >= kPerCell) continue;
                for (int s = cnt; s < kPerCell && m_fx.size() < m_max_particles; ++s) {
                    const std::uint32_t seed = hash32(m_seed++ * 2654435761u) ^ static_cast<std::uint32_t>(c);
                    const V3 l{ (static_cast<Real>(i) + 0.25_r + 0.5_r * static_cast<Real>(s & 1) + 0.2_r * jitter(seed)) * cs,
                                (static_cast<Real>(j) + 0.25_r + 0.5_r * static_cast<Real>((s >> 1) & 1) + 0.2_r * jitter(seed ^ 0x9e37u)) * cs,
                                (static_cast<Real>(k) + 0.25_r + 0.5_r * static_cast<Real>((s >> 2) & 1) + 0.2_r * jitter(seed ^ 0x51edu)) * cs };
                    m_fx.push_back(l);
                    m_fv.push_back(flip_velocity_local(l));
                    if (apic) {
                        m_fcu.push_back(V3{});
                        m_fcv.push_back(V3{});
                        m_fcw.push_back(V3{});
                    }
                    ++added;
                }
            }
        }
    }
    return changed || added > 0;
}

void FluidVolume3D::flip_classify() {
    m_liquid_count = 0;
    m_deep_count = 0;
    if (m_work.empty()) return;
    const CellBox& wb = m_work;
    const FluidParticleSettings& ps = m_def.particles;
    const Real core = ps.narrow_band ? std::max(1.5_r, ps.band_cells) : 1.0e9_r;
    const int thr = std::max(0, ps.spray_neighbours);
    const int nx = m_nx, ny = m_ny, nz = m_nz;
    const bool contain = ps.contain;
    for (int j = wb.lo[1]; j <= wb.hi[1]; ++j) {
        for (int k = wb.lo[2]; k <= wb.hi[2]; ++k) {
            for (int i = wb.lo[0]; i <= wb.hi[0]; ++i) {
                const std::size_t c = cell_index(i, j, k);
                std::uint8_t t = kAir;
                if (m_solid[c] || m_locc[c]) {
                    t = kSolid;
                    if (m_lcount[c] > 0) {
                        int sum = m_lcount[c];
                        const int oi[6] = { -1, 1, 0, 0, 0, 0 }, oj[6] = { 0, 0, -1, 1, 0, 0 }, ok[6] = { 0, 0, 0, 0, -1, 1 };
                        for (int q = 0; q < 6 && sum <= thr; ++q) {
                            const int a = i + oi[q], b = j + oj[q], d = k + ok[q];
                            if (a < 0 || b < 0 || d < 0 || a >= nx || b >= ny || d >= nz) continue;
                            const std::size_t e = cell_index(a, b, d);
                            sum += (m_lphi_adv[e] < -core && m_lphi[e] < 1.0_r - core) ? kPerCell : m_lcount[e];
                        }
                        if (sum <= thr) t |= kLonely;
                    }
                } else if (m_lphi_adv[c] < -core && m_lphi[c] < 1.0_r - core && (m_lcount[c] == 0 || (m_lcount[c] >= kPerCell - 2 && m_lcount[c] <= kPerCell + 1))) {
                    t = kLiquid | kDeep;
                    ++m_deep_count;
                } else if (m_lcount[c] > 0) {
                    int sum = m_lcount[c];
                    if (sum <= thr) {
                        const int oi[6] = { -1, 1, 0, 0, 0, 0 }, oj[6] = { 0, 0, -1, 1, 0, 0 }, ok[6] = { 0, 0, 0, 0, -1, 1 };
                        for (int q = 0; q < 6 && sum <= thr; ++q) {
                            const int a = i + oi[q], b = j + oj[q], d = k + ok[q];
                            if (a < 0 || b < 0 || d < 0 || a >= nx || b >= ny || d >= nz) continue;
                            const std::size_t e = cell_index(a, b, d);
                            sum += (m_lphi_adv[e] < -core && m_lphi[e] < 1.0_r - core) ? kPerCell : m_lcount[e];
                        }
                    }
                    t = sum > thr ? kLiquid : kAir;
                }
                m_ltype[c] = t;
                if ((t & kTypeMask) == kLiquid) ++m_liquid_count;
            }
        }
    }
    auto kind = [&](int i, int j, int k) -> std::uint8_t {
        if (i < 0 || j < 0 || k < 0 || i >= nx || j >= ny || k >= nz) return contain ? kSolid : kAir;
        return m_ltype[cell_index(i, j, k)] & kTypeMask;
    };
    for (int j = wb.lo[1]; j <= wb.hi[1]; ++j) {
        for (int k = wb.lo[2]; k <= wb.hi[2]; ++k) {
            for (int i = wb.lo[0]; i <= wb.hi[0]; ++i) {
                const std::size_t c = cell_index(i, j, k);
                if ((m_ltype[c] & kTypeMask) != kLiquid) {
                    m_lsolid_nbr[c] = 0;
                    continue;
                }
                const std::uint8_t nb[6] = { kind(i - 1, j, k), kind(i + 1, j, k), kind(i, j - 1, k), kind(i, j + 1, k), kind(i, j, k - 1), kind(i, j, k + 1) };
                std::uint8_t bits = 0;
                bool surface = false;
                for (int q = 0; q < 6; ++q) {
                    if (nb[q] == kSolid) bits |= static_cast<std::uint8_t>(1u << q);
                    if (nb[q] == kAir) surface = true;
                }
                m_lsolid_nbr[c] = bits;
                if (surface) m_ltype[c] |= kSurface;
            }
        }
    }
    const std::size_t n = m_fx.size();
    m_fspray.resize(n);
    for (std::size_t p = 0; p < n; ++p) {
        const std::uint8_t t = m_ltype[static_cast<std::size_t>(flip_cell_of(m_fx[p]))];
        m_fspray[p] = ((t & kTypeMask) == kAir || (t & kLonely)) ? 1 : 0;
    }
}

void FluidVolume3D::flip_redistance() {
    if (m_work.empty()) return;
    const CellBox& wb = m_work;
    const int nx = m_nx, ny = m_ny, nz = m_nz;
    const bool contain = m_def.particles.contain;
    const Real cap = std::max(1.5_r, m_def.particles.band_cells) + 4.0_r;
    const Real big = cap + 2.0_r;
    for (int j = wb.lo[1]; j <= wb.hi[1]; ++j) {
        for (int k = wb.lo[2]; k <= wb.hi[2]; ++k) {
            for (int i = wb.lo[0]; i <= wb.hi[0]; ++i) {
                const std::size_t c = cell_index(i, j, k);
                Real d = (m_ltype[c] & kTypeMask) == kAir ? 0.0_r : big;
                if (!contain && d > 0.0_r && (i == 0 || j == 0 || k == 0 || i == nx - 1 || j == ny - 1 || k == nz - 1)) d = 1.0_r;
                m_lphi[c] = d;
            }
        }
    }
    const Real w1 = 1.0_r, w2 = 1.41421356_r, w3 = 1.73205081_r;
    auto at = [&](int i, int j, int k) -> Real {
        if (i < 0 || j < 0 || k < 0 || i >= nx || j >= ny || k >= nz) return big;
        if (i < wb.lo[0] || j < wb.lo[1] || k < wb.lo[2] || i > wb.hi[0] || j > wb.hi[1] || k > wb.hi[2]) return 0.0_r;
        return m_lphi[cell_index(i, j, k)];
    };
    for (int pass = 0; pass < 2; ++pass) {
        const int s = pass == 0 ? -1 : 1;
        const int jb = pass == 0 ? wb.lo[1] : wb.hi[1], je = pass == 0 ? wb.hi[1] + 1 : wb.lo[1] - 1;
        const int kb = pass == 0 ? wb.lo[2] : wb.hi[2], ke = pass == 0 ? wb.hi[2] + 1 : wb.lo[2] - 1;
        const int ib = pass == 0 ? wb.lo[0] : wb.hi[0], ie = pass == 0 ? wb.hi[0] + 1 : wb.lo[0] - 1;
        const int st = -s;
        for (int j = jb; j != je; j += st) {
            for (int k = kb; k != ke; k += st) {
                for (int i = ib; i != ie; i += st) {
                    const std::size_t c = cell_index(i, j, k);
                    Real d = m_lphi[c];
                    if (d <= 0.0_r) continue;
                    for (int dk = -1; dk <= 1; ++dk) {
                        for (int di = -1; di <= 1; ++di) {
                            const int nn = (di != 0) + (dk != 0) + 1;
                            const Real wt = nn == 1 ? w1 : nn == 2 ? w2 : w3;
                            d = std::min(d, at(i + di, j + s, k + dk) + wt);
                        }
                    }
                    for (int di = -1; di <= 1; ++di) d = std::min(d, at(i + di, j, k + s) + (di != 0 ? w2 : w1));
                    d = std::min(d, at(i + s, j, k) + w1);
                    m_lphi[c] = d;
                }
            }
        }
    }
    for (int j = wb.lo[1]; j <= wb.hi[1]; ++j) {
        for (int k = wb.lo[2]; k <= wb.hi[2]; ++k) {
            for (int i = wb.lo[0]; i <= wb.hi[0]; ++i) {
                const std::size_t c = cell_index(i, j, k);
                const Real d = m_lphi[c];
                m_lphi[c] = d <= 0.0_r ? kAirPhi : -std::min(d - 0.5_r, cap);
            }
        }
    }
}

void FluidVolume3D::flip_lerps(detail::FluidTaskPool& pool) {
    const std::size_t n = m_fx.size();
    m_lerp_i.resize(n * 6);
    m_lerp_t.resize(n * 6);
    const Real inv = 1.0_r / m_cs;
    const int nx = m_nx, ny = m_ny, nz = m_nz;
    const V3* xs = m_fx.data();
    std::int32_t* li = m_lerp_i.data();
    Real* lt = m_lerp_t.data();
    pool.parallel_range(n, 4096, [=](std::size_t b0, std::size_t b1) {
        for (std::size_t p = b0; p < b1; ++p) {
            const V3 x = xs[p];
            Lerp2 l[3];
            lerp_axis(x.x * inv, nx, l[0]);
            lerp_axis(x.y * inv, ny, l[1]);
            lerp_axis(x.z * inv, nz, l[2]);
            std::int32_t* oi = li + p * 6;
            Real* ot = lt + p * 6;
            for (int a = 0; a < 3; ++a) {
                oi[a * 2] = l[a].i[0];
                oi[a * 2 + 1] = l[a].i[1];
                ot[a * 2] = l[a].t[0];
                ot[a * 2 + 1] = l[a].t[1];
            }
        }
    });
}

void FluidVolume3D::flip_p2g(detail::FluidTaskPool& pool) {
    if (m_box.empty()) return;
    flip_lerps(pool);
    const CellBox& wb = m_work;
    const std::size_t n = m_fx.size();
    const Real cs = m_cs;
    const bool apic = m_solver == FluidSolver3D::apic;
    const int nx = m_nx, ny = m_ny, nz = m_nz;
    const std::uint8_t* spray = m_fspray.data();
    const std::int32_t* li = m_lerp_i.data();
    const Real* lt = m_lerp_t.data();
    const V3* xs = m_fx.data();
    const V3* vs = m_fv.data();
    const std::uint8_t* types = m_ltype.data();
    const std::uint16_t* counts = m_lcount.data();
    auto deep_room = [=](int i, int j, int k) -> Real {
        if (i < 0 || j < 0 || k < 0 || i >= nx || j >= ny || k >= nz) return 0.0_r;
        const std::size_t c = (static_cast<std::size_t>(j) * static_cast<std::size_t>(nz) + static_cast<std::size_t>(k)) * static_cast<std::size_t>(nx) + static_cast<std::size_t>(i);
        if (!(types[c] & kDeep)) return 0.0_r;
        return std::max(0.0_r, 1.0_r - static_cast<Real>(counts[c]) / static_cast<Real>(kPerCell));
    };
    const CellBox bb = m_box;
    auto clear_axis = [&](int axis) {
        Real* gd = axis == 0 ? m_gu.data() : axis == 1 ? m_gv.data() : m_gw.data();
        Real* wd = axis == 0 ? m_guw.data() : axis == 1 ? m_gvw.data() : m_gww.data();
        const int sx = nx + (axis == 0 ? 1 : 0), sz = nz + (axis == 2 ? 1 : 0);
        const int ex = axis == 0 ? 1 : 0, ey = axis == 1 ? 1 : 0, ez = axis == 2 ? 1 : 0;
        for (int j = wb.lo[1]; j <= wb.hi[1] + ey; ++j) {
            for (int k = wb.lo[2]; k <= wb.hi[2] + ez; ++k) {
                const std::size_t row = (static_cast<std::size_t>(j) * static_cast<std::size_t>(sz) + static_cast<std::size_t>(k)) * static_cast<std::size_t>(sx);
                for (int i = wb.lo[0]; i <= wb.hi[0] + ex; ++i) {
                    gd[row + static_cast<std::size_t>(i)] = 0.0_r;
                    wd[row + static_cast<std::size_t>(i)] = 0.0_r;
                }
            }
        }
    };
    auto finish_axis = [&](int axis) {
        Real* gd = axis == 0 ? m_gu.data() : axis == 1 ? m_gv.data() : m_gw.data();
        Real* wd = axis == 0 ? m_guw.data() : axis == 1 ? m_gvw.data() : m_gww.data();
        const Real* ga = axis == 0 ? m_gua.data() : axis == 1 ? m_gva.data() : m_gwa.data();
        const int sx = nx + (axis == 0 ? 1 : 0), sz = nz + (axis == 2 ? 1 : 0);
        const int ex = axis == 0 ? 1 : 0, ey = axis == 1 ? 1 : 0, ez = axis == 2 ? 1 : 0;
        for (int j = wb.lo[1]; j <= wb.hi[1] + ey; ++j) {
            for (int k = wb.lo[2]; k <= wb.hi[2] + ez; ++k) {
                const std::size_t row = (static_cast<std::size_t>(j) * static_cast<std::size_t>(sz) + static_cast<std::size_t>(k)) * static_cast<std::size_t>(sx);
                for (int i = wb.lo[0]; i <= wb.hi[0] + ex; ++i) {
                    const std::size_t f = row + static_cast<std::size_t>(i);
                    const Real room = m_deep_count > 0 ? deep_room(i - ex, j - ey, k - ez) + deep_room(i, j, k) : 0.0_r;
                    const Real wdeep = 4.0_r * room;
                    const Real wsum = wd[f] + wdeep;
                    gd[f] = wsum > 1e-9_r ? (gd[f] + wdeep * ga[f]) / wsum : 0.0_r;
                    wd[f] = wsum;
                }
            }
        }
    };
    auto scatter_all = [&]() {
        Real* gds[3] = { m_gu.data(), m_gv.data(), m_gw.data() };
        Real* wds[3] = { m_guw.data(), m_gvw.data(), m_gww.data() };
        Real* dens = m_ldens.data();
        const std::size_t ck = static_cast<std::size_t>(nx), cj = static_cast<std::size_t>(nx) * static_cast<std::size_t>(nz);
        std::size_t dks[3], djs[3];
        int sxs[3], szs[3];
        for (int axis = 0; axis < 3; ++axis) {
            sxs[axis] = nx + (axis == 0 ? 1 : 0);
            szs[axis] = nz + (axis == 2 ? 1 : 0);
            dks[axis] = static_cast<std::size_t>(sxs[axis]);
            djs[axis] = static_cast<std::size_t>(sxs[axis]) * static_cast<std::size_t>(szs[axis]);
        }
        for (std::size_t p = 0; p < n; ++p) {
            if (spray[p]) continue;
            const std::int32_t* ip = li + p * 6;
            const Real* tp = lt + p * 6;
            const Real f0[3] = { 1.0_r - tp[0], 1.0_r - tp[2], 1.0_r - tp[4] };
            const Real h0[3] = { 1.0_r - tp[1], 1.0_r - tp[3], 1.0_r - tp[5] };
            const Real vel[3] = { vs[p].x, vs[p].y, vs[p].z };
            for (int axis = 0; axis < 3; ++axis) {
                const int hx = axis == 0 ? 0 : 1, hy = axis == 1 ? 0 : 1, hz = axis == 2 ? 0 : 1;
                const Real tx = tp[hx], ty = tp[2 + hy], tz = tp[4 + hz];
                const Real ax = hx ? h0[0] : f0[0], ay = hy ? h0[1] : f0[1], az = hz ? h0[2] : f0[2];
                const std::size_t dk = dks[axis], dj = djs[axis];
                const std::size_t base = (static_cast<std::size_t>(ip[2 + hy]) * static_cast<std::size_t>(szs[axis]) + static_cast<std::size_t>(ip[4 + hz])) * static_cast<std::size_t>(sxs[axis]) + static_cast<std::size_t>(ip[hx]);
                Real* gd = gds[axis] + base;
                Real* wd = wds[axis] + base;
                const Real v = vel[axis];
                const Real w0 = ax * ay * az, w1 = tx * ay * az, w2 = ax * ay * tz, w3 = tx * ay * tz, w4 = ax * ty * az, w5 = tx * ty * az, w6 = ax * ty * tz, w7 = tx * ty * tz;
                gd[0] += w0 * v; wd[0] += w0;
                gd[1] += w1 * v; wd[1] += w1;
                gd[dk] += w2 * v; wd[dk] += w2;
                gd[dk + 1] += w3 * v; wd[dk + 1] += w3;
                gd[dj] += w4 * v; wd[dj] += w4;
                gd[dj + 1] += w5 * v; wd[dj + 1] += w5;
                gd[dj + dk] += w6 * v; wd[dj + dk] += w6;
                gd[dj + dk + 1] += w7 * v; wd[dj + dk + 1] += w7;
            }
            const Real tx = tp[1], ty = tp[3], tz = tp[5];
            const Real ax = h0[0], ay = h0[1], az = h0[2];
            Real* d0 = dens + (static_cast<std::size_t>(ip[3]) * static_cast<std::size_t>(nz) + static_cast<std::size_t>(ip[5])) * static_cast<std::size_t>(nx) + static_cast<std::size_t>(ip[1]);
            d0[0] += ax * ay * az;
            d0[1] += tx * ay * az;
            d0[ck] += ax * ay * tz;
            d0[ck + 1] += tx * ay * tz;
            d0[cj] += ax * ty * az;
            d0[cj + 1] += tx * ty * az;
            d0[cj + ck] += ax * ty * tz;
            d0[cj + ck + 1] += tx * ty * tz;
        }
    };
    const bool merged = pool.size() <= 1 && !apic;
    pool.parallel_for(merged ? 1 : 4, [&](std::size_t task) {
        if (merged) {
            Real* dens = m_ldens.data();
            for (int j = wb.lo[1]; j <= wb.hi[1]; ++j) {
                for (int k = wb.lo[2]; k <= wb.hi[2]; ++k) {
                    Real* row = dens + cell_index(0, j, k);
                    for (int i = wb.lo[0]; i <= wb.hi[0]; ++i) row[i] = 0.0_r;
                }
            }
            for (int axis = 0; axis < 3; ++axis) clear_axis(axis);
            scatter_all();
            for (int axis = 0; axis < 3; ++axis) finish_axis(axis);
            task = 3;
        }
        if (task == 3) {
            Real* dens = m_ldens.data();
            const std::size_t dk = static_cast<std::size_t>(nx), dj = static_cast<std::size_t>(nx) * static_cast<std::size_t>(nz);
            if (!merged) {
                for (int j = wb.lo[1]; j <= wb.hi[1]; ++j) {
                    for (int k = wb.lo[2]; k <= wb.hi[2]; ++k) {
                        Real* row = dens + cell_index(0, j, k);
                        for (int i = wb.lo[0]; i <= wb.hi[0]; ++i) row[i] = 0.0_r;
                    }
                }
            }
            for (std::size_t p = 0; p < n && !merged; ++p) {
                if (spray[p]) continue;
                const std::int32_t* ip = li + p * 6;
                const Real* tp = lt + p * 6;
                const Real tx = tp[1], ty = tp[3], tz = tp[5];
                const std::size_t base = (static_cast<std::size_t>(ip[3]) * static_cast<std::size_t>(nz) + static_cast<std::size_t>(ip[5])) * static_cast<std::size_t>(nx) + static_cast<std::size_t>(ip[1]);
                const Real ax = 1.0_r - tx, ay = 1.0_r - ty, az = 1.0_r - tz;
                Real* d0 = dens + base;
                d0[0] += ax * ay * az;
                d0[1] += tx * ay * az;
                d0[dk] += ax * ay * tz;
                d0[dk + 1] += tx * ay * tz;
                d0[dj] += ax * ty * az;
                d0[dj + 1] += tx * ty * az;
                d0[dj + dk] += ax * ty * tz;
                d0[dj + dk + 1] += tx * ty * tz;
            }
            const Real kw[3] = { 0.125_r, 0.75_r, 0.125_r };
            for (int j = bb.lo[1]; j <= bb.hi[1]; ++j) {
                for (int k = bb.lo[2]; k <= bb.hi[2]; ++k) {
                    for (int i = bb.lo[0]; i <= bb.hi[0]; ++i) {
                        const Real room = deep_room(i, j, k);
                        if (room <= 0.0_r) continue;
                        const Real amount = room * static_cast<Real>(kPerCell);
                        for (int dj2 = -1; dj2 <= 1; ++dj2) {
                            const int b = j + dj2;
                            if (b < 0 || b >= ny) continue;
                            for (int dk2 = -1; dk2 <= 1; ++dk2) {
                                const int d = k + dk2;
                                if (d < 0 || d >= nz) continue;
                                Real* row = dens + cell_index(0, b, d);
                                const Real wjk = amount * kw[dj2 + 1] * kw[dk2 + 1];
                                for (int di = -1; di <= 1; ++di) {
                                    const int a = i + di;
                                    if (a < 0 || a >= nx) continue;
                                    row[a] += wjk * kw[di + 1];
                                }
                            }
                        }
                    }
                }
            }
            return;
        }
        const int axis = static_cast<int>(task);
        Real* gd = axis == 0 ? m_gu.data() : axis == 1 ? m_gv.data() : m_gw.data();
        Real* wd = axis == 0 ? m_guw.data() : axis == 1 ? m_gvw.data() : m_gww.data();
        const V3* cvec = apic ? (axis == 0 ? m_fcu.data() : axis == 1 ? m_fcv.data() : m_fcw.data()) : nullptr;
        const int sx = nx + (axis == 0 ? 1 : 0), sz = nz + (axis == 2 ? 1 : 0);
        const int hx = axis == 0 ? 0 : 1, hy = axis == 1 ? 0 : 1, hz = axis == 2 ? 0 : 1;
        const std::size_t dk = static_cast<std::size_t>(sx), dj = static_cast<std::size_t>(sx) * static_cast<std::size_t>(sz);
        clear_axis(axis);
        const Real ox = axis == 0 ? 0.0_r : 0.5_r, oy = axis == 1 ? 0.0_r : 0.5_r, oz = axis == 2 ? 0.0_r : 0.5_r;
        for (std::size_t p = 0; p < n; ++p) {
            if (spray[p]) continue;
            const std::int32_t* ip = li + p * 6;
            const Real* tp = lt + p * 6;
            const int i0 = ip[hx], j0 = ip[2 + hy], k0 = ip[4 + hz];
            const Real tx = tp[hx], ty = tp[2 + hy], tz = tp[4 + hz];
            const std::size_t base = (static_cast<std::size_t>(j0) * static_cast<std::size_t>(sz) + static_cast<std::size_t>(k0)) * static_cast<std::size_t>(sx) + static_cast<std::size_t>(i0);
            const Real vel = axis == 0 ? vs[p].x : axis == 1 ? vs[p].y : vs[p].z;
            const Real ax = 1.0_r - tx, ay = 1.0_r - ty, az = 1.0_r - tz;
            const Real w[8] = { ax * ay * az, tx * ay * az, ax * ay * tz, tx * ay * tz, ax * ty * az, tx * ty * az, ax * ty * tz, tx * ty * tz };
            const std::size_t offs[8] = { 0, 1, dk, dk + 1, dj, dj + 1, dj + dk, dj + dk + 1 };
            if (cvec) {
                const V3 x = xs[p];
                const V3 cc = cvec[p];
                const Real bx = (static_cast<Real>(i0) + ox) * cs - x.x, by = (static_cast<Real>(j0) + oy) * cs - x.y, bz = (static_cast<Real>(k0) + oz) * cs - x.z;
                for (int q = 0; q < 8; ++q) {
                    const Real dx = bx + ((q & 1) ? cs : 0.0_r), dz = bz + ((q & 2) ? cs : 0.0_r), dy = by + ((q & 4) ? cs : 0.0_r);
                    gd[base + offs[q]] += w[q] * (vel + cc.x * dx + cc.y * dy + cc.z * dz);
                    wd[base + offs[q]] += w[q];
                }
            } else {
                for (int q = 0; q < 8; ++q) {
                    gd[base + offs[q]] += w[q] * vel;
                    wd[base + offs[q]] += w[q];
                }
            }
        }
        finish_axis(axis);
    });
}

void FluidVolume3D::flip_forces(Real h) {
    if (m_work.empty()) return;
    const CellBox& wb = m_work;
    const int nx = m_nx, ny = m_ny, nz = m_nz;
    const bool contain = m_def.particles.contain;
    auto type = [&](int i, int j, int k) -> std::uint8_t {
        if (i < 0 || j < 0 || k < 0 || i >= nx || j >= ny || k >= nz) return contain ? kSolid : kAir;
        return m_ltype[cell_index(i, j, k)] & kTypeMask;
    };
    auto occ = [&](int i, int j, int k, int axis, Real& sum, int& cnt) {
        if (i < 0 || j < 0 || k < 0 || i >= nx || j >= ny || k >= nz) return;
        const std::size_t c = cell_index(i, j, k);
        if (!m_locc[c]) return;
        sum += axis == 0 ? m_locc_vel[c].x : axis == 1 ? m_locc_vel[c].y : m_locc_vel[c].z;
        ++cnt;
    };
    const Real gy = -m_g * h;
    for (int axis = 0; axis < 3; ++axis) {
        std::vector<Real>& g = axis == 0 ? m_gu : axis == 1 ? m_gv : m_gw;
        std::vector<Real>& g0 = axis == 0 ? m_gu0 : axis == 1 ? m_gv0 : m_gw0;
        std::vector<std::uint8_t>& valid = axis == 0 ? m_fvalid_u : axis == 1 ? m_fvalid_v : m_fvalid_w;
        const int ex = axis == 0 ? 1 : 0, ey = axis == 1 ? 1 : 0, ez = axis == 2 ? 1 : 0;
        for (int j = wb.lo[1]; j <= wb.hi[1] + ey; ++j) {
            for (int k = wb.lo[2]; k <= wb.hi[2] + ez; ++k) {
                for (int i = wb.lo[0]; i <= wb.hi[0] + ex; ++i) {
                    const std::size_t f = axis == 0 ? ucell(i, j, k) : axis == 1 ? vcell(i, j, k) : wcell(i, j, k);
                    const std::uint8_t a = type(i - ex, j - ey, k - ez), b = type(i, j, k);
                    valid[f] = (a != kAir || b != kAir) ? 1 : 0;
                    if (a == kSolid || b == kSolid) {
                        Real sum = 0.0_r;
                        int cnt = 0;
                        occ(i - ex, j - ey, k - ez, axis, sum, cnt);
                        occ(i, j, k, axis, sum, cnt);
                        g[f] = cnt ? sum / static_cast<Real>(cnt) : 0.0_r;
                    } else if (a == kAir && b == kAir) {
                        g[f] = 0.0_r;
                    }
                    g0[f] = g[f];
                    if (axis == 1 && a != kSolid && b != kSolid && (a == kLiquid || b == kLiquid)) g[f] += gy;
                }
            }
        }
    }
    const Real fric = std::max(0.0_r, m_def.particles.wall_friction) * std::sqrt(std::max(m_def.material.friction, 0.0_r) / 0.1_r) * h / m_cs;
    if (fric <= 0.0_r) return;
    for (int axis = 0; axis < 3; ++axis) {
        std::vector<Real>& g = axis == 0 ? m_gu : axis == 1 ? m_gv : m_gw;
        const int ex = axis == 0 ? 1 : 0, ey = axis == 1 ? 1 : 0, ez = axis == 2 ? 1 : 0;
        for (int j = wb.lo[1]; j <= wb.hi[1] + ey; ++j) {
            for (int k = wb.lo[2]; k <= wb.hi[2] + ez; ++k) {
                for (int i = wb.lo[0]; i <= wb.hi[0] + ex; ++i) {
                    const std::uint8_t a = type(i - ex, j - ey, k - ez), b = type(i, j, k);
                    if (a == kSolid || b == kSolid || (a != kLiquid && b != kLiquid)) continue;
                    const std::uint8_t ba = a == kLiquid ? m_lsolid_nbr[cell_index(i - ex, j - ey, k - ez)] : 0;
                    const std::uint8_t bb = b == kLiquid ? m_lsolid_nbr[cell_index(i, j, k)] : 0;
                    const std::uint8_t tangent = static_cast<std::uint8_t>((ba | bb) & ~(3u << (2 * axis)));
                    if (!tangent) continue;
                    const int walls = ((tangent & 1) ? 1 : 0) + ((tangent & 2) ? 1 : 0) + ((tangent & 4) ? 1 : 0) + ((tangent & 8) ? 1 : 0) + ((tangent & 16) ? 1 : 0) + ((tangent & 32) ? 1 : 0);
                    const std::size_t f = axis == 0 ? ucell(i, j, k) : axis == 1 ? vcell(i, j, k) : wcell(i, j, k);
                    g[f] /= 1.0_r + fric * static_cast<Real>(walls) * std::abs(g[f]);
                }
            }
        }
    }
}

void FluidVolume3D::flip_viscosity(Real h) {
    const Real dcoef = h * m_visc_coef;
    if (dcoef <= 2e-3_r || m_box.empty()) return;
    const CellBox& wb = m_box;
    const int nx = m_nx, ny = m_ny, nz = m_nz;
    const bool contain = m_def.particles.contain;
    auto type = [&](int i, int j, int k) -> std::uint8_t {
        if (i < 0 || j < 0 || k < 0 || i >= nx || j >= ny || k >= nz) return contain ? kSolid : kAir;
        return m_ltype[cell_index(i, j, k)] & kTypeMask;
    };
    const FluidParticleSettings& ps = m_def.particles;
    const int base = ps.viscosity_iterations > 0 ? ps.viscosity_iterations : std::clamp(m_quality.solver_iterations * 3, 4, 40);
    const int iters = ps.viscosity_iterations > 0 ? base : std::clamp(static_cast<int>(std::ceil(static_cast<Real>(base) + 6.0_r * dcoef)), base, 80);
    for (int axis = 0; axis < 3; ++axis) {
        std::vector<Real>& g = axis == 0 ? m_gu : axis == 1 ? m_gv : m_gw;
        std::vector<Real>& src = m_gscratch;
        const int ex = axis == 0 ? 1 : 0, ey = axis == 1 ? 1 : 0, ez = axis == 2 ? 1 : 0;
        const int fx = nx + ex, fy = ny + ey, fz = nz + ez;
        auto face_index = [&](int i, int j, int k) -> std::int32_t { return static_cast<std::int32_t>((static_cast<std::size_t>(j) * static_cast<std::size_t>(fz) + static_cast<std::size_t>(k)) * static_cast<std::size_t>(fx) + static_cast<std::size_t>(i)); };
        auto kind = [&](int i, int j, int k) -> int {
            if (i < 0 || j < 0 || k < 0 || i >= fx || j >= fy || k >= fz) return -1;
            const std::uint8_t a = type(i - ex, j - ey, k - ez), b = type(i, j, k);
            if (a == kSolid || b == kSolid) return 2;
            if (a == kLiquid || b == kLiquid) return 1;
            return 0;
        };
        m_visc_faces.clear();
        m_visc_nb.clear();
        for (int j = wb.lo[1]; j <= wb.hi[1] + ey; ++j) {
            for (int k = wb.lo[2]; k <= wb.hi[2] + ez; ++k) {
                for (int i = wb.lo[0]; i <= wb.hi[0] + ex; ++i) {
                    if (kind(i, j, k) != 1) continue;
                    m_visc_faces.push_back(static_cast<std::uint32_t>(face_index(i, j, k)));
                    const int oi[6] = { -1, 1, 0, 0, 0, 0 }, oj[6] = { 0, 0, -1, 1, 0, 0 }, ok[6] = { 0, 0, 0, 0, -1, 1 };
                    for (int q = 0; q < 6; ++q) {
                        const int a = i + oi[q], b = j + oj[q], d = k + ok[q];
                        const int kk = kind(a, b, d);
                        if (kk == -1) m_visc_nb.push_back(contain ? -2 : -1);
                        else if (kk == 0) m_visc_nb.push_back(-1);
                        else m_visc_nb.push_back(face_index(a, b, d));
                    }
                }
            }
        }
        const std::size_t nf = m_visc_faces.size();
        if (nf == 0) continue;
        for (std::uint32_t f : m_visc_faces) src[f] = g[f];
        for (int it = 0; it < iters; ++it) {
            const bool forward = (it & 1) == 0;
            for (std::size_t q = 0; q < nf; ++q) {
                const std::size_t e = forward ? q : nf - 1 - q;
                const std::size_t f = m_visc_faces[e];
                const std::int32_t* nb = &m_visc_nb[e * 6];
                Real sum = 0.0_r;
                int cnt = 0;
                for (int t = 0; t < 6; ++t) {
                    if (nb[t] == -1) continue;
                    ++cnt;
                    if (nb[t] >= 0) sum += g[static_cast<std::size_t>(nb[t])];
                }
                g[f] = (src[f] + dcoef * sum) / (1.0_r + dcoef * static_cast<Real>(cnt));
            }
        }
    }
}

int FluidVolume3D::flip_pressure(Real h, int max_iterations) {
    m_llist.clear();
    if (m_work.empty()) return 0;
    const CellBox& wb = m_work;
    const int nx = m_nx, ny = m_ny, nz = m_nz;
    const bool contain = m_def.particles.contain;
    const double warm = m_last_h > 0.0_r ? static_cast<double>(h / m_last_h) : 0.0;
    m_last_h = h;
    for (int j = wb.lo[1]; j <= wb.hi[1]; ++j) {
        for (int k = wb.lo[2]; k <= wb.hi[2]; ++k) {
            for (int i = wb.lo[0]; i <= wb.hi[0]; ++i) {
                const std::size_t c = cell_index(i, j, k);
                if ((m_ltype[c] & kTypeMask) == kLiquid) {
                    m_lidx[c] = static_cast<std::int32_t>(m_llist.size());
                    m_llist.push_back(static_cast<std::uint32_t>(c));
                } else {
                    m_lidx[c] = -1;
                }
            }
        }
    }
    const std::size_t M = m_llist.size();
    if (M == 0) {
        for (int j = wb.lo[1]; j <= wb.hi[1]; ++j) {
            for (int k = wb.lo[2]; k <= wb.hi[2]; ++k) {
                for (int i = wb.lo[0]; i <= wb.hi[0]; ++i) m_lpress[cell_index(i, j, k)] = 0.0_r;
            }
        }
        return 0;
    }
    m_pcg_b.resize(M);
    m_pcg_x.resize(M);
    m_pcg_r.resize(M);
    m_pcg_z.resize(M);
    m_pcg_s.resize(M);
    m_pcg_q.resize(M);
    m_pcg_diag.resize(M);
    m_pcg_ax.resize(M);
    m_pcg_ay.resize(M);
    m_pcg_az.resize(M);
    m_pcg_pre.resize(M);
    m_pcg_nb.resize(M * 6);
    auto type = [&](int i, int j, int k) -> std::uint8_t {
        if (i < 0 || j < 0 || k < 0 || i >= nx || j >= ny || k >= nz) return contain ? kSolid : kAir;
        return m_ltype[cell_index(i, j, k)] & kTypeMask;
    };
    const double drift = static_cast<double>(std::max(0.0_r, m_def.particles.drift_correction) * m_drift_scale) * 0.25 * static_cast<double>(m_cs / h);
    const double rest = static_cast<double>(kPerCell);
    auto frac = [&](int i, int j, int k) -> double {
        if (i < 0 || j < 0 || k < 0 || i >= nx || j >= ny || k >= nz) return 0.0;
        return std::clamp(static_cast<double>(m_ldens[cell_index(i, j, k)]) / rest, 0.0, 1.0);
    };
    auto theta = [&](double fc, double fa) -> double { return std::clamp((fc - 0.5) / std::max(fc - fa, 1e-3), 0.1, 1.0); };
    double bmax = 0.0;
    for (std::size_t r = 0; r < M; ++r) {
        const std::size_t c = m_llist[r];
        const int i = static_cast<int>(c % static_cast<std::size_t>(nx));
        const int k = static_cast<int>((c / static_cast<std::size_t>(nx)) % static_cast<std::size_t>(nz));
        const int j = static_cast<int>(c / (static_cast<std::size_t>(nx) * static_cast<std::size_t>(nz)));
        const double div = static_cast<double>(m_gu[ucell(i + 1, j, k)] - m_gu[ucell(i, j, k)] + m_gv[vcell(i, j + 1, k)] - m_gv[vcell(i, j, k)] + m_gw[wcell(i, j, k + 1)] - m_gw[wcell(i, j, k)]);
        const int ni[6] = { i - 1, i + 1, i, i, i, i };
        const int nj[6] = { j, j, j - 1, j + 1, j, j };
        const int nk[6] = { k, k, k, k, k - 1, k + 1 };
        double diag = 1e-6;
        bool interior = true;
        m_pcg_ax[r] = 0.0;
        m_pcg_ay[r] = 0.0;
        m_pcg_az[r] = 0.0;
        for (int q = 0; q < 6; ++q) {
            const std::uint8_t t = type(ni[q], nj[q], nk[q]);
            m_pcg_nb[r * 6 + static_cast<std::size_t>(q)] = -1;
            if (t == kSolid) continue;
            if (t == kAir) {
                diag += 1.0 / theta(frac(i, j, k), frac(ni[q], nj[q], nk[q]));
                interior = false;
                continue;
            }
            diag += 1.0;
            m_pcg_nb[r * 6 + static_cast<std::size_t>(q)] = m_lidx[cell_index(ni[q], nj[q], nk[q])];
            if (q == 1) m_pcg_ax[r] = -1.0;
            if (q == 3) m_pcg_ay[r] = -1.0;
            if (q == 5) m_pcg_az[r] = -1.0;
        }
        m_pcg_diag[r] = diag;
        double target = 0.0;
        const double dens = static_cast<double>(m_ldens[c]);
        if (drift > 0.0 && dens > rest) target = drift * std::min(dens - rest, rest) / rest;
        else if (drift > 0.0 && dens < 0.85 * rest && interior) target = -drift * 0.5 * (0.85 * rest - dens) / rest;
        m_pcg_b[r] = target - div;
        m_pcg_x[r] = static_cast<double>(m_lpress[c]) * warm;
        bmax = std::max(bmax, std::abs(m_pcg_b[r]));
    }
    for (int j = wb.lo[1]; j <= wb.hi[1]; ++j) {
        for (int k = wb.lo[2]; k <= wb.hi[2]; ++k) {
            for (int i = wb.lo[0]; i <= wb.hi[0]; ++i) m_lpress[cell_index(i, j, k)] = 0.0_r;
        }
    }
    auto matvec = [&](const std::vector<double>& in, std::vector<double>& out) {
        for (std::size_t r = 0; r < M; ++r) {
            double s = m_pcg_diag[r] * in[r];
            const std::int32_t* nb = &m_pcg_nb[r * 6];
            if (nb[0] >= 0) s += m_pcg_ax[static_cast<std::size_t>(nb[0])] * in[static_cast<std::size_t>(nb[0])];
            if (nb[1] >= 0) s += m_pcg_ax[r] * in[static_cast<std::size_t>(nb[1])];
            if (nb[2] >= 0) s += m_pcg_ay[static_cast<std::size_t>(nb[2])] * in[static_cast<std::size_t>(nb[2])];
            if (nb[3] >= 0) s += m_pcg_ay[r] * in[static_cast<std::size_t>(nb[3])];
            if (nb[4] >= 0) s += m_pcg_az[static_cast<std::size_t>(nb[4])] * in[static_cast<std::size_t>(nb[4])];
            if (nb[5] >= 0) s += m_pcg_az[r] * in[static_cast<std::size_t>(nb[5])];
            out[r] = s;
        }
    };
    const double tau = 0.97, sigma = 0.25;
    for (std::size_t r = 0; r < M; ++r) {
        const std::int32_t* nb = &m_pcg_nb[r * 6];
        double e = m_pcg_diag[r];
        if (nb[0] >= 0) {
            const std::size_t l = static_cast<std::size_t>(nb[0]);
            const double a = m_pcg_ax[l] * m_pcg_pre[l];
            e -= a * a + tau * m_pcg_ax[l] * (m_pcg_ay[l] + m_pcg_az[l]) * m_pcg_pre[l] * m_pcg_pre[l];
        }
        if (nb[2] >= 0) {
            const std::size_t l = static_cast<std::size_t>(nb[2]);
            const double a = m_pcg_ay[l] * m_pcg_pre[l];
            e -= a * a + tau * m_pcg_ay[l] * (m_pcg_ax[l] + m_pcg_az[l]) * m_pcg_pre[l] * m_pcg_pre[l];
        }
        if (nb[4] >= 0) {
            const std::size_t l = static_cast<std::size_t>(nb[4]);
            const double a = m_pcg_az[l] * m_pcg_pre[l];
            e -= a * a + tau * m_pcg_az[l] * (m_pcg_ax[l] + m_pcg_ay[l]) * m_pcg_pre[l] * m_pcg_pre[l];
        }
        if (e < sigma * m_pcg_diag[r]) e = m_pcg_diag[r];
        m_pcg_pre[r] = 1.0 / std::sqrt(e);
    }
    auto precondition = [&](const std::vector<double>& in, std::vector<double>& out) {
        for (std::size_t r = 0; r < M; ++r) {
            const std::int32_t* nb = &m_pcg_nb[r * 6];
            double t = in[r];
            if (nb[0] >= 0) {
                const std::size_t l = static_cast<std::size_t>(nb[0]);
                t -= m_pcg_ax[l] * m_pcg_pre[l] * m_pcg_q[l];
            }
            if (nb[2] >= 0) {
                const std::size_t l = static_cast<std::size_t>(nb[2]);
                t -= m_pcg_ay[l] * m_pcg_pre[l] * m_pcg_q[l];
            }
            if (nb[4] >= 0) {
                const std::size_t l = static_cast<std::size_t>(nb[4]);
                t -= m_pcg_az[l] * m_pcg_pre[l] * m_pcg_q[l];
            }
            m_pcg_q[r] = t * m_pcg_pre[r];
        }
        for (std::size_t rr = M; rr-- > 0;) {
            const std::int32_t* nb = &m_pcg_nb[rr * 6];
            double t = m_pcg_q[rr];
            if (nb[1] >= 0) t -= m_pcg_ax[rr] * m_pcg_pre[rr] * out[static_cast<std::size_t>(nb[1])];
            if (nb[3] >= 0) t -= m_pcg_ay[rr] * m_pcg_pre[rr] * out[static_cast<std::size_t>(nb[3])];
            if (nb[5] >= 0) t -= m_pcg_az[rr] * m_pcg_pre[rr] * out[static_cast<std::size_t>(nb[5])];
            out[rr] = t * m_pcg_pre[rr];
        }
    };
    const double tol_rel = m_def.particles.pressure_tolerance > 0.0_r ? static_cast<double>(m_def.particles.pressure_tolerance) : 1e-3;
    const double tol = std::max(tol_rel * bmax, 1e-10);
    matvec(m_pcg_x, m_pcg_s);
    double rmax = 0.0;
    for (std::size_t r = 0; r < M; ++r) {
        m_pcg_r[r] = m_pcg_b[r] - m_pcg_s[r];
        rmax = std::max(rmax, std::abs(m_pcg_r[r]));
    }
    int it = 0;
    if (rmax > tol && std::isfinite(rmax)) {
        precondition(m_pcg_r, m_pcg_z);
        double rz = 0.0;
        for (std::size_t r = 0; r < M; ++r) {
            m_pcg_s[r] = m_pcg_z[r];
            rz += m_pcg_r[r] * m_pcg_z[r];
        }
        std::vector<double>& As = m_pcg_b;
        const int cap = std::max(1, max_iterations);
        for (it = 1; it <= cap; ++it) {
            matvec(m_pcg_s, As);
            double sAs = 0.0;
            for (std::size_t r = 0; r < M; ++r) sAs += m_pcg_s[r] * As[r];
            if (!(std::abs(sAs) > 1e-300)) break;
            const double alpha = rz / sAs;
            rmax = 0.0;
            for (std::size_t r = 0; r < M; ++r) {
                m_pcg_x[r] += alpha * m_pcg_s[r];
                m_pcg_r[r] -= alpha * As[r];
                rmax = std::max(rmax, std::abs(m_pcg_r[r]));
            }
            if (rmax <= tol || !std::isfinite(rmax)) break;
            precondition(m_pcg_r, m_pcg_z);
            double rz2 = 0.0;
            for (std::size_t r = 0; r < M; ++r) rz2 += m_pcg_r[r] * m_pcg_z[r];
            const double beta = rz > 0.0 ? rz2 / rz : 0.0;
            rz = rz2;
            for (std::size_t r = 0; r < M; ++r) m_pcg_s[r] = m_pcg_z[r] + beta * m_pcg_s[r];
        }
        it = std::min(it, cap);
    }
    for (std::size_t r = 0; r < M; ++r) {
        const double v = std::isfinite(m_pcg_x[r]) ? m_pcg_x[r] : 0.0;
        m_lpress[m_llist[r]] = static_cast<Real>(v);
    }
    auto pres = [&](int i, int j, int k) -> Real {
        if (i < 0 || j < 0 || k < 0 || i >= nx || j >= ny || k >= nz) return 0.0_r;
        return m_lpress[cell_index(i, j, k)];
    };
    for (int axis = 0; axis < 3; ++axis) {
        std::vector<Real>& g = axis == 0 ? m_gu : axis == 1 ? m_gv : m_gw;
        const int ex = axis == 0 ? 1 : 0, ey = axis == 1 ? 1 : 0, ez = axis == 2 ? 1 : 0;
        for (int j = wb.lo[1]; j <= wb.hi[1] + ey; ++j) {
            for (int k = wb.lo[2]; k <= wb.hi[2] + ez; ++k) {
                for (int i = wb.lo[0]; i <= wb.hi[0] + ex; ++i) {
                    const std::uint8_t a = type(i - ex, j - ey, k - ez), b = type(i, j, k);
                    if (a == kSolid || b == kSolid) continue;
                    if (a != kLiquid && b != kLiquid) continue;
                    const std::size_t f = axis == 0 ? ucell(i, j, k) : axis == 1 ? vcell(i, j, k) : wcell(i, j, k);
                    if (a == kLiquid && b == kLiquid) g[f] -= pres(i, j, k) - pres(i - ex, j - ey, k - ez);
                    else if (a == kLiquid) g[f] += pres(i - ex, j - ey, k - ez) / static_cast<Real>(theta(frac(i - ex, j - ey, k - ez), frac(i, j, k)));
                    else g[f] -= pres(i, j, k) / static_cast<Real>(theta(frac(i, j, k), frac(i - ex, j - ey, k - ez)));
                }
            }
        }
    }
    return it;
}

void FluidVolume3D::flip_g2p(detail::FluidTaskPool& pool, Real h) {
    const std::size_t n = m_fx.size();
    const Real cs = m_cs;
    const Real inv = 1.0_r / cs;
    const bool apic = m_solver == FluidSolver3D::apic;
    const int nx = m_nx, ny = m_ny, nz = m_nz;
    const Real mix = m_flip_mix * m_calm_scale;
    const Real vlim = m_vlim_cells * cs / h;
    const Real coh = m_cohesion * h * cs / static_cast<Real>(kPerCell);
    const std::uint8_t* spray = m_fspray.data();
    const std::int32_t* li = m_lerp_i.data();
    const Real* lt = m_lerp_t.data();
    const V3* xs = m_fx.data();
    V3* vs = m_fv.data();
    V3* cvs[3] = { apic ? m_fcu.data() : nullptr, apic ? m_fcv.data() : nullptr, apic ? m_fcw.data() : nullptr };
    const Real* gs[3] = { m_gu.data(), m_gv.data(), m_gw.data() };
    const Real* g0s[3] = { m_gu0.data(), m_gv0.data(), m_gw0.data() };
    const std::uint8_t* vms[3] = { m_fvalid_u.data(), m_fvalid_v.data(), m_fvalid_w.data() };
    const std::uint8_t* types = m_ltype.data();
    const Real* dens = m_ldens.data();
    pool.parallel_range(n, 1024, [=](std::size_t b0, std::size_t b1) {
        for (std::size_t p = b0; p < b1; ++p) {
            if (spray[p]) continue;
            const V3 vold = vs[p];
            const Real old[3] = { vold.x, vold.y, vold.z };
            Real out[3] = { vold.x, vold.y, vold.z };
            V3 grad[3] = { V3{}, V3{}, V3{} };
            const std::int32_t* ip = li + p * 6;
            const Real* tp = lt + p * 6;
            for (int axis = 0; axis < 3; ++axis) {
                const int sx = nx + (axis == 0 ? 1 : 0), sz = nz + (axis == 2 ? 1 : 0);
                const int hx = axis == 0 ? 0 : 1, hy = axis == 1 ? 0 : 1, hz = axis == 2 ? 0 : 1;
                const std::size_t dk = static_cast<std::size_t>(sx), dj = static_cast<std::size_t>(sx) * static_cast<std::size_t>(sz);
                const Real tx = tp[hx], ty = tp[2 + hy], tz = tp[4 + hz];
                const std::size_t base = (static_cast<std::size_t>(ip[2 + hy]) * static_cast<std::size_t>(sz) + static_cast<std::size_t>(ip[4 + hz])) * static_cast<std::size_t>(sx) + static_cast<std::size_t>(ip[hx]);
                const Real* g = gs[axis];
                const Real* g0 = g0s[axis];
                const std::uint8_t* vm = vms[axis];
                const Real ax = 1.0_r - tx, ay = 1.0_r - ty, az = 1.0_r - tz;
                const Real w[8] = { ax * ay * az, tx * ay * az, ax * ay * tz, tx * ay * tz, ax * ty * az, tx * ty * az, ax * ty * tz, tx * ty * tz };
                const std::size_t offs[8] = { 0, 1, dk, dk + 1, dj, dj + 1, dj + dk, dj + dk + 1 };
                Real ws = 0.0_r, pic = 0.0_r, dv = 0.0_r;
                int valid = 0;
                for (int q = 0; q < 8; ++q) {
                    const std::size_t f = base + offs[q];
                    const Real wq = vm[f] ? w[q] : 0.0_r;
                    valid += vm[f];
                    const Real gv = g[f];
                    ws += wq;
                    pic += wq * gv;
                    dv += wq * (gv - g0[f]);
                }
                if (ws <= 1e-9_r) continue;
                const Real iw = 1.0_r / ws;
                pic *= iw;
                dv *= iw;
                if (apic) {
                    out[axis] = pic;
                    if (valid == 8) {
                        V3 cg{};
                        const Real gx[2] = { -inv, inv };
                        for (int q = 0; q < 8; ++q) {
                            const int a = q & 1, c = (q >> 1) & 1, b = (q >> 2) & 1;
                            const Real wx = a ? tx : ax, wy = b ? ty : ay, wz = c ? tz : az;
                            const Real gv = g[base + offs[q]];
                            cg = cg + V3{ gx[a] * wy * wz, wx * gx[b] * wz, wx * wy * gx[c] } * gv;
                        }
                        grad[axis] = cg;
                    }
                } else {
                    out[axis] = (1.0_r - mix) * pic + mix * (old[axis] + dv);
                }
            }
            V3 v{ out[0], out[1], out[2] };
            if (coh > 0.0_r) {
                const V3 x = xs[p];
                const std::size_t c = (static_cast<std::size_t>(std::clamp(static_cast<int>(x.y * inv), 0, ny - 1)) * static_cast<std::size_t>(nz) + static_cast<std::size_t>(std::clamp(static_cast<int>(x.z * inv), 0, nz - 1))) * static_cast<std::size_t>(nx) + static_cast<std::size_t>(std::clamp(static_cast<int>(x.x * inv), 0, nx - 1));
                if (types[c] & kSurface) {
                    const Real tx = tp[1], ty = tp[3], tz = tp[5];
                    const std::size_t dk = static_cast<std::size_t>(nx), dj = static_cast<std::size_t>(nx) * static_cast<std::size_t>(nz);
                    const std::size_t base = (static_cast<std::size_t>(ip[3]) * static_cast<std::size_t>(nz) + static_cast<std::size_t>(ip[5])) * static_cast<std::size_t>(nx) + static_cast<std::size_t>(ip[1]);
                    V3 gd{};
                    const Real gx[2] = { -inv, inv };
                    for (int q = 0; q < 8; ++q) {
                        const int a = q & 1, cz = (q >> 1) & 1, b = (q >> 2) & 1;
                        const Real wx = a ? tx : 1.0_r - tx, wy = b ? ty : 1.0_r - ty, wz = cz ? tz : 1.0_r - tz;
                        const Real d = dens[base + (a ? 1u : 0u) + (cz ? dk : 0u) + (b ? dj : 0u)];
                        gd = gd + V3{ gx[a] * wy * wz, wx * gx[b] * wz, wx * wy * gx[cz] } * d;
                    }
                    v = v + gd * coh;
                }
            }
            const Real s2 = vec3::dot(v, v);
            if (s2 > vlim * vlim) v = v * (vlim / std::sqrt(s2));
            vs[p] = v;
            if (apic) {
                cvs[0][p] = grad[0];
                cvs[1][p] = grad[1];
                cvs[2][p] = grad[2];
            }
        }
    });
}

void FluidVolume3D::flip_substep(detail::FluidTaskPool& pool, Real h, int iterations) {
    flip_advect_particles(pool, h);
    flip_active_box();
    flip_advect_grid(pool, h);
    flip_sort();
    if (flip_band(h)) flip_sort();
    flip_classify();
    flip_p2g(pool);
    flip_redistance();
    flip_forces(h);
    flip_viscosity(h);
    m_last_iterations = std::max(m_last_iterations, flip_pressure(h, iterations));
    flip_g2p(pool, h);
}

void FluidVolume3D::step_flip(detail::FluidTaskPool& pool, Real dt, FluidStats3D& stats) {
    run_emitters(dt);
    m_contact_count = 0;
    m_last_iterations = 0;
    m_last_substeps = 0;
    const FluidParticleSettings& ps = m_def.particles;
    if (m_all_dirty) {
        m_flip_asleep = false;
        m_calm_frames = 0;
        m_all_dirty = false;
    }
    if (m_fx.empty() && m_deep_count == 0) {
        m_awake_count = 0;
        m_max_speed = 0.0_r;
        return;
    }
    flip_refresh_materials();
    m_solid_count = 0;
    for (std::uint8_t s : m_solid) m_solid_count += s ? 1u : 0u;
    Real body_speed = 0.0_r;
    bool bodies_moving = false;
    for (const NearBody& nb : m_near) {
        const Real sp = vec3::length(nb.body->linear_velocity()) + vec3::length(nb.body->angular_velocity()) * std::max(nb.body->colliders().front().shape.bounding_radius, 0.0_r);
        body_speed = std::max(body_speed, sp);
        if ((nb.dynamic || nb.body->has(PhysicsFlags::LinearVelocity)) && sp > ps.wake_speed) bodies_moving = true;
    }
    bool active_emitters = false;
    for (const FluidEmitter3D& e : m_emitters) active_emitters = active_emitters || (e.active && e.rate > 0.0_r);
    if (m_flip_asleep && m_def.allow_sleep) {
        if (!bodies_moving && !active_emitters) {
            for (NearBody& nb : m_near) {
                nb.impulse = V3{};
                nb.angular = V3{};
            }
            m_awake_count = 0;
            return;
        }
        m_flip_asleep = false;
        m_calm_frames = 0;
    }
    flip_rasterize_bodies();
    std::array<std::uint32_t, 64> hist{};
    const Real bin = 8.0_r * dt / m_cs;
    for (const V3& v : m_fv) ++hist[static_cast<std::size_t>(std::min(vec3::length(v) * bin, 63.0_r))];
    const std::size_t keep = m_fv.size() - m_fv.size() / 200;
    std::size_t acc = 0, top = 0;
    for (; top < 64; ++top) {
        acc += hist[top];
        if (acc >= keep) break;
    }
    const Real vmax = std::max((static_cast<Real>(top) + 1.0_r) / bin + m_g * dt, body_speed);
    const Real cfl = ps.cfl > 0.0_r ? ps.cfl : std::clamp(m_quality.cfl * 4.0_r, 0.5_r, 3.0_r);
    m_vlim_cells = std::max(1.0_r, cfl) * 0.999_r;
    const int cap = std::max(1, ps.max_substeps > 0 ? ps.max_substeps : m_quality.max_substeps);
    const int substeps = std::clamp(static_cast<int>(std::ceil(dt * vmax / (cfl * m_cs))), 1, cap);
    const Real h = dt / static_cast<Real>(substeps);
    const int iters = ps.pressure_iterations > 0 ? ps.pressure_iterations : std::clamp(m_quality.solver_iterations * 20, 20, 400);
    for (int s = 0; s < substeps; ++s) flip_substep(pool, h, iters);
    flip_body_impulses();
    m_last_substeps = substeps;
    const Real slowest = std::max(1.0_r, m_def.material.viscosity_factor());
    const Real settle_at = ps.settle_speed / slowest;
    const Real s2 = ps.sleep_speed * ps.sleep_speed, t2 = settle_at * settle_at;
    std::array<std::uint32_t, 64> sh{};
    const Real sbin = 64.0_r / std::max(settle_at * 4.0_r, 1e-6_r);
    std::size_t over_sleep = 0, over_settle = 0;
    Real mx = 0.0_r;
    for (const V3& v : m_fv) {
        const Real q = vec3::dot(v, v);
        mx = std::max(mx, q);
        if (q > s2) ++over_sleep;
        if (q > t2) ++over_settle;
        ++sh[static_cast<std::size_t>(std::min(std::sqrt(q) * sbin, 63.0_r))];
    }
    m_max_speed = std::sqrt(mx);
    const std::size_t want90 = m_fv.size() - m_fv.size() / 10;
    std::size_t run = 0, b90 = 0;
    for (; b90 < 63; ++b90) {
        run += sh[b90];
        if (run >= want90) break;
    }
    const Real p90 = (static_cast<Real>(b90) + 0.5_r) / sbin;
    const bool calm = !active_emitters && !bodies_moving;
    m_drift_scale = calm ? std::clamp(p90 / std::max(settle_at, 1e-6_r), 0.05_r, 1.0_r) : 1.0_r;
    m_calm_scale = calm ? std::clamp(p90 / std::max(settle_at, 1e-6_r), 0.0_r, 1.0_r) : 1.0_r;
    if (ps.settle_rate > 0.0_r && over_settle * 10 <= m_fv.size() && calm) {
        const Real k = std::exp(-ps.settle_rate * dt);
        for (V3& v : m_fv) v = v * k;
        m_max_speed *= k;
    }
    if (ps.damping > 0.0_r) {
        const Real k = std::exp(-ps.damping * dt);
        for (V3& v : m_fv) v = v * k;
    }
    if (over_sleep * 100 <= m_fv.size() * 3 && calm) ++m_calm_frames;
    else m_calm_frames = 0;
    if (m_def.allow_sleep && m_calm_frames >= std::max(1, ps.sleep_frames)) {
        m_flip_asleep = true;
        std::fill(m_fv.begin(), m_fv.end(), V3{});
        if (m_solver == FluidSolver3D::apic) {
            std::fill(m_fcu.begin(), m_fcu.end(), V3{});
            std::fill(m_fcv.begin(), m_fcv.end(), V3{});
            std::fill(m_fcw.begin(), m_fcw.end(), V3{});
        }
        if (!m_work.empty()) {
            const CellBox& wb = m_work;
            for (int j = wb.lo[1]; j <= wb.hi[1] + 1; ++j) {
                for (int k = wb.lo[2]; k <= wb.hi[2] + 1; ++k) {
                    for (int i = wb.lo[0]; i <= wb.hi[0] + 1; ++i) {
                        if (j <= wb.hi[1] && k <= wb.hi[2]) m_gu[ucell(i, j, k)] = 0.0_r;
                        if (i <= wb.hi[0] && k <= wb.hi[2]) m_gv[vcell(i, j, k)] = 0.0_r;
                        if (i <= wb.hi[0] && j <= wb.hi[1]) m_gw[wcell(i, j, k)] = 0.0_r;
                    }
                }
            }
        }
    }
    flip_finish_frame();
    stats.substeps = std::max(stats.substeps, substeps);
    stats.iterations = std::max(stats.iterations, m_last_iterations);
    stats.fluid_cells += m_liquid_count;
    stats.deep_cells += m_deep_count;
}

double FluidVolume3D::flip_count_volume() const {
    if (m_work.empty()) return static_cast<double>(m_fx.size());
    double cells = 0.0;
    const CellBox& wb = m_work;
    for (int j = wb.lo[1]; j <= wb.hi[1]; ++j) {
        for (int k = wb.lo[2]; k <= wb.hi[2]; ++k) {
            const std::size_t row = cell_index(0, j, k);
            for (int i = wb.lo[0]; i <= wb.hi[0]; ++i) {
                const std::size_t c = row + static_cast<std::size_t>(i);
                const double cnt = static_cast<double>(m_lcount[c]);
                cells += (m_ltype[c] & kDeep) ? std::max(static_cast<double>(kPerCell), cnt) : cnt;
            }
        }
    }
    return cells;
}

void FluidVolume3D::flip_correct_volume() {
    if (!m_def.particles.narrow_band || m_work.empty()) return;
    const double now = flip_count_volume();
    if (!m_target_valid) {
        m_target_volume = now;
        m_target_valid = true;
        return;
    }
    double deficit = m_target_volume - now;
    const double limit = std::max(64.0, 0.02 * now);
    deficit = std::clamp(deficit, -limit, limit);
    if (std::abs(deficit) < 4.0) return;
    const CellBox& wb = m_box.empty() ? m_work : m_box;
    const Real cs = m_cs;
    const bool apic = m_solver == FluidSolver3D::apic;
    if (deficit > 0.0) {
        for (int pass = 0; pass < 2 && deficit >= 1.0; ++pass) {
            const int want = pass == 0 ? kPerCell - 2 : kPerCell;
            for (int j = wb.lo[1]; j <= wb.hi[1] && deficit >= 1.0; ++j) {
                for (int k = wb.lo[2]; k <= wb.hi[2] && deficit >= 1.0; ++k) {
                    for (int i = wb.lo[0]; i <= wb.hi[0] && deficit >= 1.0; ++i) {
                        const std::size_t c = cell_index(i, j, k);
                        const std::uint8_t t = m_ltype[c];
                        if ((t & kTypeMask) != kLiquid || (t & (kDeep | kSurface)) || m_lphi[c] > -1.0_r) continue;
                        int cnt = m_lcount[c];
                        for (int s = cnt; s < want && deficit >= 1.0 && m_fx.size() < m_max_particles; ++s) {
                            const std::uint32_t seed = hash32(m_seed++ * 2246822519u) ^ static_cast<std::uint32_t>(c);
                            const V3 l{ (static_cast<Real>(i) + 0.5_r + 0.8_r * jitter(seed)) * cs, (static_cast<Real>(j) + 0.5_r + 0.8_r * jitter(seed ^ 0x9e37u)) * cs, (static_cast<Real>(k) + 0.5_r + 0.8_r * jitter(seed ^ 0x51edu)) * cs };
                            m_fx.push_back(l);
                            m_fv.push_back(flip_velocity_local(l));
                            if (apic) {
                                m_fcu.push_back(V3{});
                                m_fcv.push_back(V3{});
                                m_fcw.push_back(V3{});
                            }
                            ++cnt;
                            deficit -= 1.0;
                        }
                        m_lcount[c] = static_cast<std::uint16_t>(cnt);
                    }
                }
            }
        }
        return;
    }
    const std::size_t n = m_fx.size();
    if (n == 0) return;
    m_fspray.resize(n);
    std::fill(m_fspray.begin(), m_fspray.end(), std::uint8_t{ 0 });
    double excess = -deficit;
    for (std::size_t p = 0; p < n && excess >= 1.0; ++p) {
        const std::size_t c = static_cast<std::size_t>(flip_cell_of(m_fx[p]));
        if ((m_ltype[c] & kTypeMask) != kLiquid || (m_ltype[c] & kSurface) || m_lcount[c] <= kPerCell) continue;
        --m_lcount[c];
        m_fspray[p] = 1;
        excess -= 1.0;
    }
    std::size_t w = 0;
    for (std::size_t p = 0; p < n; ++p) {
        if (m_fspray[p]) continue;
        if (w != p) {
            m_fx[w] = m_fx[p];
            m_fv[w] = m_fv[p];
            if (apic) {
                m_fcu[w] = m_fcu[p];
                m_fcv[w] = m_fcv[p];
                m_fcw[w] = m_fcw[p];
            }
        }
        ++w;
    }
    flip_resize_particles(w);
    m_fspray.assign(w, 0);
}

void FluidVolume3D::flip_finish_frame() {
    flip_correct_volume();
    const std::size_t n = m_fx.size();
    m_px.resize(n);
    m_pv.resize(n);
    m_density.resize(n);
    const Real rho = m_def.material.density;
    const Real inv = 1.0_r / m_cs;
    for (std::size_t p = 0; p < n; ++p) {
        const V3 l = m_fx[p];
        m_px[p] = to_world(l);
        m_pv[p] = world_dir(m_fv[p]);
        const Real d = trilinear(m_ldens.data(), m_nx, m_ny, m_nz, l.x * inv - 0.5_r, l.y * inv - 0.5_r, l.z * inv - 0.5_r);
        if (d > 1e-6_r) m_density[p] = rho * std::min(d / static_cast<Real>(kPerCell), 2.0_r);
    }
    m_awake_count = m_flip_asleep ? 0 : n;
    if (!m_work.empty()) {
        const CellBox& wb = m_work;
        for (int j = wb.lo[1]; j <= wb.hi[1]; ++j) {
            for (int k = wb.lo[2]; k <= wb.hi[2]; ++k) {
                for (int i = wb.lo[0]; i <= wb.hi[0]; ++i) {
                    const std::size_t c = cell_index(i, j, k);
                    const std::uint8_t t = m_ltype[c];
                    Real f = 0.0_r;
                    if (t & kDeep) f = 1.0_r;
                    else if ((t & kTypeMask) == kLiquid) f = std::min(1.0_r, static_cast<Real>(m_lcount[c]) / static_cast<Real>(kPerCell));
                    m_fill[c] = f;
                    m_cell_vel[c] = f > 0.0_r ? V3{ 0.5_r * (m_gu[ucell(i, j, k)] + m_gu[ucell(i + 1, j, k)]), 0.5_r * (m_gv[vcell(i, j, k)] + m_gv[vcell(i, j + 1, k)]), 0.5_r * (m_gw[wcell(i, j, k)] + m_gw[wcell(i, j, k + 1)]) } : V3{};
                }
            }
        }
    }
    build_surface_grid(m_def.particles.surface_cell);
    if (m_sx > 0 && !m_work.empty() && m_deep_count > 0) {
        const CellBox& wb = m_work;
        for (int k = wb.lo[2]; k <= wb.hi[2]; ++k) {
            for (int i = wb.lo[0]; i <= wb.hi[0]; ++i) {
                Real top = -1.0e6_r;
                for (int j = wb.hi[1]; j >= wb.lo[1]; --j) {
                    if (m_ltype[cell_index(i, j, k)] & kDeep) {
                        top = (static_cast<Real>(j) + 1.0_r) * m_cs;
                        break;
                    }
                }
                if (top < -1.0e5_r) continue;
                const V3 lc{ (static_cast<Real>(i) + 0.5_r) * m_cs, 0.0_r, (static_cast<Real>(k) + 0.5_r) * m_cs };
                const int si = std::clamp(ifloor(lc.x / m_scell), 0, m_sx - 1);
                const int sk = std::clamp(ifloor(lc.z / m_scell), 0, m_sz - 1);
                Real& s = m_sgrid[static_cast<std::size_t>(sk) * static_cast<std::size_t>(m_sx) + static_cast<std::size_t>(si)];
                s = std::max(s, top);
            }
        }
    }
    ++m_version;
}

Real FluidVolume3D::flip_depth(const V3& point) const {
    const V3 l = to_local(point);
    if (m_lphi.empty()) return -std::numeric_limits<Real>::max();
    const Real phi = flip_sample_cell(l, m_lphi);
    if (phi > 0.0_r) return -phi * m_cs;
    const V3 s = surface_point(point);
    return std::max(vec3::dot(s - point, m_up), -phi * m_cs);
}

Real FluidVolume3D::flip_volume() const {
    if (m_work.empty()) return 0.0_r;
    double cells = 0.0;
    const CellBox& wb = m_work;
    for (int j = wb.lo[1]; j <= wb.hi[1]; ++j) {
        for (int k = wb.lo[2]; k <= wb.hi[2]; ++k) {
            for (int i = wb.lo[0]; i <= wb.hi[0]; ++i) {
                const std::size_t c = cell_index(i, j, k);
                const double cnt = static_cast<double>(m_lcount[c]) / static_cast<double>(kPerCell);
                cells += (m_ltype[c] & kDeep) ? std::max(1.0, cnt) : cnt;
            }
        }
    }
    return static_cast<Real>(cells * static_cast<double>(m_cs) * static_cast<double>(m_cs) * static_cast<double>(m_cs));
}

} // namespace physics
} // namespace fizmo
