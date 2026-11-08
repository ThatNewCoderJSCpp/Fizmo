#include "fizmo_library.hpp"
#include "fluid_2d.hpp"

namespace fizmo {
namespace physics {

namespace {

using V2 = RVec2;

constexpr std::uint8_t kAir = 0;
constexpr std::uint8_t kFluid = 1;
constexpr std::uint8_t kSolidCell = 2;
constexpr std::uint8_t kBubble = 3;

Real dot2(const V2& a, const V2& b) noexcept { return a.x * b.x + a.y * b.y; }
Real cross2(const V2& a, const V2& b) noexcept { return a.x * b.y - a.y * b.x; }

std::uint32_t hash32(std::uint32_t x) noexcept {
    x ^= x >> 16;
    x *= 0x7feb352du;
    x ^= x >> 15;
    x *= 0x846ca68bu;
    x ^= x >> 16;
    return x;
}

Real jitter(std::uint32_t seed) noexcept { return (static_cast<Real>(hash32(seed) >> 8) * (1.0_r / 16777216.0_r)) - 0.5_r; }

} // namespace

void FluidVolume2D::init_particles() {
    init_grid();
    if (m_nx < 2 || m_ny < 2) {
        m_cs = std::min(m_size.x, m_size.y) * 0.5_r;
        m_cs = std::max(m_cs, 1e-4_r);
        m_nx = std::max(2, static_cast<int>(std::ceil(m_size.x / m_cs - 1e-3_r)));
        m_ny = std::max(2, static_cast<int>(std::ceil(m_size.y / m_cs - 1e-3_r)));
        const std::size_t n = static_cast<std::size_t>(m_nx) * static_cast<std::size_t>(m_ny);
        m_solid.assign(n, 0);
        m_occ.assign(n, 0);
    }
    m_radius = 0.3_r * m_cs;
    m_spacing = 2.0_r * m_radius;
    m_rest = (m_cs / m_spacing) * (m_cs / m_spacing);
    const FluidParticleSettings2D& ps = m_def.particles;
    m_max_particles = ps.max_particles > 0 ? ps.max_particles : std::max<std::size_t>(1000, m_quality.max_particles * 4);
    const std::size_t n = static_cast<std::size_t>(m_nx) * static_cast<std::size_t>(m_ny);
    const std::size_t nu = static_cast<std::size_t>(m_nx + 1) * static_cast<std::size_t>(m_ny);
    const std::size_t nv = static_cast<std::size_t>(m_nx) * static_cast<std::size_t>(m_ny + 1);
    m_gu.assign(nu, 0.0_r);
    m_gu0.assign(nu, 0.0_r);
    m_guw.assign(nu, 0.0_r);
    m_gv.assign(nv, 0.0_r);
    m_gv0.assign(nv, 0.0_r);
    m_gvw.assign(nv, 0.0_r);
    m_gscratch.assign(std::max(nu, nv), 0.0_r);
    m_ctype.assign(n, kAir);
    m_cdens.assign(n, 0.0_r);
    m_crho.assign(n, m_materials[0].density);
    m_cmu.assign(n, m_materials[0].viscosity);
    m_cvisc.assign(n, 0.0_r);
    m_phi.assign(n, 0.0_r);
    m_fidx.assign(n, -1);
    m_cell_start.assign(n + 2, 0);
    m_cell_fill_count.assign(n + 2, 0);
    m_occ_vel.assign(n, V2{});
    m_flist.reserve(n);
    refresh_materials();
    if (m_def.fill > 0.0_r) {
        const Real top = std::clamp(m_def.fill, 0.0_r, 1.0_r) * m_size.y;
        const V2 a = to_world(V2{ 0.0_r, 0.0_r });
        const V2 b = to_world(V2{ m_size.x, top });
        add_particle_box(AABB(V2{ std::min(a.x, b.x), std::min(a.y, b.y) }, V2{ std::max(a.x, b.x), std::max(a.y, b.y) }));
    }
}

int FluidVolume2D::particle_cell(const V2& l) const noexcept {
    const int i = std::clamp(static_cast<int>(std::floor(l.x / m_cs)), 0, m_nx - 1);
    const int j = std::clamp(static_cast<int>(std::floor(l.y / m_cs)), 0, m_ny - 1);
    return j * m_nx + i;
}

Real FluidVolume2D::particle_mass(std::uint8_t id) const noexcept {
    const std::size_t m = id < m_mat_rho.size() ? id : 0;
    return (m_mat_rho.empty() ? m_materials[0].density : m_mat_rho[m]) * m_spacing * m_spacing;
}

bool FluidVolume2D::add_particle(const V2& position, const V2& velocity, std::uint8_t material_id, std::uint32_t color) {
    if (m_def.level != FluidLevel::realistic || m_px.size() >= m_max_particles) return false;
    const V2 l = to_local(position);
    if (!(l.x >= 0.0_r && l.y >= 0.0_r && l.x <= m_size.x && l.y <= m_size.y)) return false;
    const int c = particle_cell(l);
    if (static_solid(c % m_nx, c / m_nx)) return false;
    if (material_id >= m_materials.size()) material_id = 0;
    m_px.push_back(l);
    m_pv.push_back(local_dir(velocity));
    m_cu.push_back(V2{});
    m_cv.push_back(V2{});
    m_pmat.push_back(material_id);
    m_pcol.push_back(color ? color : m_mat_color[material_id]);
    m_world_dirty = true;
    m_asleep = false;
    m_calm_frames = 0;
    return true;
}

std::size_t FluidVolume2D::add_particle_box(const AABB& box, const V2& velocity, std::uint8_t material_id, std::uint32_t color) {
    if (m_def.level != FluidLevel::realistic) return 0;
    const V2 a = to_local(box.min), b = to_local(box.max);
    const V2 lo{ std::max(0.0_r, std::min(a.x, b.x)), std::max(0.0_r, std::min(a.y, b.y)) };
    const V2 hi{ std::min(m_size.x, std::max(a.x, b.x)), std::min(m_size.y, std::max(a.y, b.y)) };
    const Real s = m_spacing;
    const int nx = static_cast<int>(std::floor((hi.x - lo.x) / s + 1e-4_r));
    const int ny = static_cast<int>(std::floor((hi.y - lo.y) / s + 1e-4_r));
    std::size_t n = 0;
    for (int j = 0; j < ny; ++j) {
        for (int i = 0; i < nx; ++i) {
            const std::uint32_t seed = static_cast<std::uint32_t>(m_px.size() * 7919u + static_cast<std::size_t>(i * 31 + j));
            const V2 l{ lo.x + (static_cast<Real>(i) + 0.5_r + 0.02_r * jitter(seed)) * s, lo.y + (static_cast<Real>(j) + 0.5_r + 0.02_r * jitter(seed ^ 0xabcdu)) * s };
            if (add_particle(to_world(l), velocity, material_id, color)) ++n;
            else if (m_px.size() >= m_max_particles) return n;
        }
    }
    return n;
}

std::size_t FluidVolume2D::add_fluid_circle(const V2& center, Real radius, const V2& velocity, std::uint8_t material_id, std::uint32_t color) {
    if (m_def.level == FluidLevel::blocky) {
        return paint_cells(center, radius, m_def.sand.fill_cell, 1.0_r, false);
    }
    if (m_def.level == FluidLevel::flowing) {
        const V2 l = to_local(center);
        std::size_t n = 0;
        for (int j = 0; j < m_ny; ++j) {
            for (int i = 0; i < m_nx; ++i) {
                const Real dx = (static_cast<Real>(i) + 0.5_r) * m_cs - l.x;
                const Real dy = (static_cast<Real>(j) + 0.5_r) * m_cs - l.y;
                if (dx * dx + dy * dy > radius * radius) continue;
                const std::size_t c = cell_index(i, j);
                if (m_solid[c] || m_fill[c] >= m_def.flowing.max_mass) continue;
                m_fill[c] = m_def.flowing.max_mass;
                mark_flow_dirty(i, j);
                ++n;
            }
        }
        return n;
    }
    if (m_def.level == FluidLevel::waves) {
        disturb(center, radius * 0.5_r, radius);
        return 0;
    }
    if (m_def.level != FluidLevel::realistic) return 0;
    const V2 l = to_local(center);
    const Real s = m_spacing;
    const int i0 = static_cast<int>(std::floor((l.x - radius) / s));
    const int i1 = static_cast<int>(std::ceil((l.x + radius) / s));
    const int j0 = static_cast<int>(std::floor((l.y - radius) / s));
    const int j1 = static_cast<int>(std::ceil((l.y + radius) / s));
    const int gw = i1 - i0 + 3, gh = j1 - j0 + 3;
    std::vector<std::uint32_t>& head = m_bubble_queue;
    std::vector<std::uint32_t>& link = m_bubble_label;
    head.assign(static_cast<std::size_t>(gw) * static_cast<std::size_t>(gh), 0xffffffffu);
    link.resize(m_px.size() + static_cast<std::size_t>(gw) * static_cast<std::size_t>(gh));
    auto bucket = [&](const V2& p, int& bx, int& by) {
        bx = static_cast<int>(std::floor(p.x / s)) - i0 + 1;
        by = static_cast<int>(std::floor(p.y / s)) - j0 + 1;
        return bx >= 0 && by >= 0 && bx < gw && by < gh;
    };
    for (std::size_t q = 0; q < m_px.size(); ++q) {
        int bx = 0, by = 0;
        if (!bucket(m_px[q], bx, by)) continue;
        const std::size_t cell = static_cast<std::size_t>(by * gw + bx);
        link[q] = head[cell];
        head[cell] = static_cast<std::uint32_t>(q);
    }
    const Real min2 = 0.7_r * s * 0.7_r * s;
    std::size_t n = 0;
    for (int j = j0; j <= j1; ++j) {
        for (int i = i0; i <= i1; ++i) {
            const V2 p{ (static_cast<Real>(i) + 0.5_r) * s, (static_cast<Real>(j) + 0.5_r) * s };
            const V2 d = p - l;
            if (dot2(d, d) > radius * radius) continue;
            if (p.x < 0.0_r || p.y < 0.0_r || p.x > m_size.x || p.y > m_size.y) continue;
            const int c = particle_cell(p);
            if (any_solid(c % m_nx, c / m_nx)) continue;
            int bx = 0, by = 0;
            bucket(p, bx, by);
            bool crowded = false;
            for (int y = std::max(0, by - 1); y <= std::min(gh - 1, by + 1) && !crowded; ++y) {
                for (int x = std::max(0, bx - 1); x <= std::min(gw - 1, bx + 1) && !crowded; ++x) {
                    for (std::uint32_t q = head[static_cast<std::size_t>(y * gw + x)]; q != 0xffffffffu; q = link[q]) {
                        const V2 e = m_px[q] - p;
                        if (dot2(e, e) < min2) {
                            crowded = true;
                            break;
                        }
                    }
                }
            }
            if (crowded) continue;
            const std::uint32_t seed = static_cast<std::uint32_t>(m_px.size() * 2654435761u) ^ static_cast<std::uint32_t>(i * 73856093 ^ j * 19349663);
            const V2 q{ p.x + 0.05_r * s * jitter(seed), p.y + 0.05_r * s * jitter(seed ^ 0x55u) };
            if (!add_particle(to_world(q), velocity, material_id, color)) {
                if (m_px.size() >= m_max_particles) return n;
                continue;
            }
            ++n;
        }
    }
    return n;
}

std::size_t FluidVolume2D::remove_fluid_circle(const V2& center, Real radius) {
    if (m_def.level == FluidLevel::blocky) {
        const V2 l = to_local(center);
        std::size_t n = 0;
        for (int j = 0; j < m_ny; ++j) {
            for (int i = 0; i < m_nx; ++i) {
                const Real dx = (static_cast<Real>(i) + 0.5_r) * m_cs - l.x;
                const Real dy = (static_cast<Real>(j) + 0.5_r) * m_cs - l.y;
                if (dx * dx + dy * dy > radius * radius) continue;
                const std::size_t c = cell_index(i, j);
                const SandKind2D k = sand_cell_info(static_cast<SandCell2D>(m_sand[c])).kind;
                if (k == SandKind2D::empty || k == SandKind2D::solid) continue;
                sand_set(c, SandCell2D::empty, nullptr);
                sand_mark(i, j);
                ++n;
            }
        }
        return n;
    }
    if (m_def.level == FluidLevel::flowing) {
        const V2 l = to_local(center);
        std::size_t n = 0;
        for (int j = 0; j < m_ny; ++j) {
            for (int i = 0; i < m_nx; ++i) {
                const Real dx = (static_cast<Real>(i) + 0.5_r) * m_cs - l.x;
                const Real dy = (static_cast<Real>(j) + 0.5_r) * m_cs - l.y;
                if (dx * dx + dy * dy > radius * radius) continue;
                const std::size_t c = cell_index(i, j);
                if (m_fill[c] <= 0.0_r) continue;
                m_fill[c] = 0.0_r;
                m_source[c] = 0;
                mark_flow_dirty(i, j);
                ++n;
            }
        }
        return n;
    }
    if (m_def.level == FluidLevel::waves) {
        disturb(center, -radius * 0.5_r, radius);
        return 0;
    }
    const V2 l = to_local(center);
    const Real r2 = radius * radius;
    std::size_t w = 0;
    const std::size_t n = m_px.size();
    for (std::size_t i = 0; i < n; ++i) {
        const V2 d = m_px[i] - l;
        if (dot2(d, d) <= r2) continue;
        if (w != i) {
            m_px[w] = m_px[i];
            m_pv[w] = m_pv[i];
            m_cu[w] = m_cu[i];
            m_cv[w] = m_cv[i];
            m_pmat[w] = m_pmat[i];
            m_pcol[w] = m_pcol[i];
        }
        ++w;
    }
    const std::size_t removed = n - w;
    if (removed) {
        m_px.resize(w);
        m_pv.resize(w);
        m_cu.resize(w);
        m_cv.resize(w);
        m_pmat.resize(w);
        m_pcol.resize(w);
        m_world_dirty = true;
        m_asleep = false;
        m_calm_frames = 0;
    }
    return removed;
}

std::size_t FluidVolume2D::remove_material(std::uint8_t id) {
    std::size_t w = 0;
    const std::size_t n = m_px.size();
    for (std::size_t i = 0; i < n; ++i) {
        if (m_pmat[i] == id) continue;
        if (w != i) {
            m_px[w] = m_px[i];
            m_pv[w] = m_pv[i];
            m_cu[w] = m_cu[i];
            m_cv[w] = m_cv[i];
            m_pmat[w] = m_pmat[i];
            m_pcol[w] = m_pcol[i];
        }
        ++w;
    }
    m_px.resize(w);
    m_pv.resize(w);
    m_cu.resize(w);
    m_cv.resize(w);
    m_pmat.resize(w);
    m_pcol.resize(w);
    m_world_dirty = true;
    m_asleep = false;
    return n - w;
}

void FluidVolume2D::push(const V2& center, Real radius, const V2& velocity, Real strength) {
    if (radius <= 0.0_r) return;
    const V2 l = to_local(center);
    const V2 target = local_dir(velocity);
    if (m_def.level == FluidLevel::waves) {
        for (int i = 0; i < m_cols; ++i) {
            const Real d = std::abs((static_cast<Real>(i) + 0.5_r) * m_dx - l.x);
            if (d >= radius) continue;
            const std::size_t k = static_cast<std::size_t>(i);
            const Real w = (1.0_r - d / radius) * std::clamp(strength, 0.0_r, 1.0_r);
            const Real q = target.x * m_depth[k];
            m_flux[k] += (q - m_flux[k]) * w * 0.5_r;
            m_flux[k + 1] += (q - m_flux[k + 1]) * w * 0.5_r;
            m_tile_active[k / kWaveTile] = 1;
            m_tile_calm[k / kWaveTile] = 0;
        }
        return;
    }
    if (m_def.level != FluidLevel::realistic) {
        wake();
        return;
    }
    const Real r2 = radius * radius;
    for (std::size_t i = 0; i < m_px.size(); ++i) {
        const V2 d = m_px[i] - l;
        const Real dd = dot2(d, d);
        if (dd >= r2) continue;
        const Real w = std::clamp(strength * (1.0_r - std::sqrt(dd) / radius), 0.0_r, 1.0_r);
        m_pv[i] = m_pv[i] + (target - m_pv[i]) * w;
    }
    m_asleep = false;
    m_calm_frames = 0;
}

void FluidVolume2D::swirl(const V2& center, Real radius, Real spin, Real strength) {
    if (m_def.level != FluidLevel::realistic || radius <= 0.0_r) {
        wake();
        return;
    }
    const V2 l = to_local(center);
    const Real r2 = radius * radius;
    const Real sign = cross2(m_u, m_up) >= 0.0_r ? 1.0_r : -1.0_r;
    for (std::size_t i = 0; i < m_px.size(); ++i) {
        const V2 d = m_px[i] - l;
        const Real dd = dot2(d, d);
        if (dd >= r2) continue;
        const V2 target = V2{ -d.y, d.x } * (spin * sign);
        const Real w = std::clamp(strength * (1.0_r - std::sqrt(dd) / radius), 0.0_r, 1.0_r);
        m_pv[i] = m_pv[i] + (target - m_pv[i]) * w;
    }
    m_asleep = false;
    m_calm_frames = 0;
}

void FluidVolume2D::explode(const V2& center, Real radius, Real strength) {
    if (radius <= 0.0_r) return;
    const V2 l = to_local(center);
    if (m_def.level == FluidLevel::blocky) {
        const int i0 = std::max(0, static_cast<int>(std::floor((l.x - radius) / m_cs)));
        const int j0 = std::max(0, static_cast<int>(std::floor((l.y - radius) / m_cs)));
        const int i1 = std::min(m_nx - 1, static_cast<int>(std::floor((l.x + radius) / m_cs)));
        const int j1 = std::min(m_ny - 1, static_cast<int>(std::floor((l.y + radius) / m_cs)));
        const Real core = radius * std::clamp(0.3_r + 0.05_r * strength, 0.3_r, 0.8_r);
        for (int j = j0; j <= j1; ++j) {
            for (int i = i0; i <= i1; ++i) {
                const Real dx = (static_cast<Real>(i) + 0.5_r) * m_cs - l.x;
                const Real dy = (static_cast<Real>(j) + 0.5_r) * m_cs - l.y;
                const Real d = std::sqrt(dx * dx + dy * dy);
                if (d > radius) continue;
                const std::size_t c = cell_index(i, j);
                const SandCell2D s = static_cast<SandCell2D>(m_sand[c]);
                const SandCellInfo2D& info = sand_cell_info(s);
                if (s == SandCell2D::wall) continue;
                if (info.flammability > 0.0_r) {
                    sand_set(c, SandCell2D::fire, &m_brush_rng);
                } else if (d < core) {
                    if (s == SandCell2D::water) sand_set(c, SandCell2D::steam, &m_brush_rng);
                    else if (info.kind != SandKind2D::liquid || m_brush_rng.chance(0.5_r)) sand_set(c, m_brush_rng.chance(0.25_r) ? SandCell2D::fire : SandCell2D::smoke, &m_brush_rng);
                } else if (s == SandCell2D::empty && m_brush_rng.chance(0.15_r)) {
                    sand_set(c, SandCell2D::smoke, &m_brush_rng);
                }
                sand_mark(i, j);
            }
        }
        return;
    }
    if (m_def.level == FluidLevel::waves) {
        disturb(center, -std::abs(strength) * 0.05_r, radius);
        return;
    }
    if (m_def.level != FluidLevel::realistic) {
        wake();
        return;
    }
    const Real r2 = radius * radius;
    for (std::size_t i = 0; i < m_px.size(); ++i) {
        const V2 d = m_px[i] - l;
        const Real dd = dot2(d, d);
        if (dd >= r2) continue;
        const Real dist = std::sqrt(dd);
        const V2 dir = dist > 1e-6_r ? d * (1.0_r / dist) : V2{ 0.0_r, 1.0_r };
        m_pv[i] = m_pv[i] + dir * (strength * (1.0_r - dist / radius));
    }
    m_asleep = false;
    m_calm_frames = 0;
}

void FluidVolume2D::clear_particles() {
    m_px.clear();
    m_pv.clear();
    m_cu.clear();
    m_cv.clear();
    m_pmat.clear();
    m_pcol.clear();
    m_world_dirty = true;
}

std::size_t FluidVolume2D::particles_in(const AABB& box) const {
    std::size_t n = 0;
    const V2 a = to_local(box.min), b = to_local(box.max);
    const V2 lo{ std::min(a.x, b.x), std::min(a.y, b.y) }, hi{ std::max(a.x, b.x), std::max(a.y, b.y) };
    for (const V2& p : m_px) {
        if (p.x >= lo.x && p.x <= hi.x && p.y >= lo.y && p.y <= hi.y) ++n;
    }
    return n;
}

std::size_t FluidVolume2D::add_emitter(const FluidEmitter2D& e) {
    m_emitters.push_back(e);
    return m_emitters.size() - 1;
}

const std::vector<V2>& FluidVolume2D::positions() const {
    refresh_world_particles();
    return m_wpx;
}

const std::vector<V2>& FluidVolume2D::velocities() const {
    refresh_world_particles();
    return m_wpv;
}

void FluidVolume2D::refresh_world_particles() const {
    if (!m_world_dirty && m_wpx.size() == m_px.size()) return;
    const std::size_t n = m_px.size();
    m_wpx.resize(n);
    m_wpv.resize(n);
    for (std::size_t i = 0; i < n; ++i) {
        m_wpx[i] = to_world(m_px[i]);
        m_wpv[i] = world_dir(m_pv[i]);
    }
    m_world_dirty = false;
}

void FluidVolume2D::run_emitters(Real dt) {
    for (FluidEmitter2D& e : m_emitters) {
        if (!e.active || e.rate <= 0.0_r) continue;
        e.carry += e.rate * dt;
        int guard = 0;
        while (e.carry >= 1.0_r && guard++ < 4096) {
            const Real a = m_rng.unit() * 6.28318530718_r;
            const Real r = std::sqrt(m_rng.unit()) * e.radius;
            const V2 p = e.position + V2{ std::cos(a) * r, std::sin(a) * r };
            e.carry -= 1.0_r;
            if (!add_particle(p, e.velocity, e.material, e.color) && m_px.size() >= m_max_particles) {
                e.carry = 0.0_r;
                break;
            }
        }
        e.carry = std::min(e.carry, 8.0_r);
    }
}

Real FluidVolume2D::sample_grid_u(const V2& l) const noexcept {
    const Real inv = 1.0_r / m_cs;
    const Real fx = l.x * inv, fy = l.y * inv - 0.5_r;
    const int i0 = std::clamp(static_cast<int>(std::floor(fx)), 0, m_nx - 1);
    const int j0 = std::clamp(static_cast<int>(std::floor(fy)), 0, m_ny - 2);
    const Real tx = std::clamp(fx - static_cast<Real>(i0), 0.0_r, 1.0_r), ty = std::clamp(fy - static_cast<Real>(j0), 0.0_r, 1.0_r);
    const std::size_t s = static_cast<std::size_t>(m_nx + 1);
    const std::size_t f = static_cast<std::size_t>(j0) * s + static_cast<std::size_t>(i0);
    return (m_gu[f] * (1.0_r - tx) + m_gu[f + 1] * tx) * (1.0_r - ty) + (m_gu[f + s] * (1.0_r - tx) + m_gu[f + s + 1] * tx) * ty;
}

Real FluidVolume2D::sample_grid_v(const V2& l) const noexcept {
    const Real inv = 1.0_r / m_cs;
    const Real fx = l.x * inv - 0.5_r, fy = l.y * inv;
    const int i0 = std::clamp(static_cast<int>(std::floor(fx)), 0, m_nx - 2);
    const int j0 = std::clamp(static_cast<int>(std::floor(fy)), 0, m_ny - 1);
    const Real tx = std::clamp(fx - static_cast<Real>(i0), 0.0_r, 1.0_r), ty = std::clamp(fy - static_cast<Real>(j0), 0.0_r, 1.0_r);
    const std::size_t s = static_cast<std::size_t>(m_nx);
    const std::size_t f = static_cast<std::size_t>(j0) * s + static_cast<std::size_t>(i0);
    return (m_gv[f] * (1.0_r - tx) + m_gv[f + 1] * tx) * (1.0_r - ty) + (m_gv[f + s] * (1.0_r - tx) + m_gv[f + s + 1] * tx) * ty;
}

void FluidVolume2D::sort_particles() {
    const std::size_t n = m_px.size();
    const std::uint32_t N = static_cast<std::uint32_t>(m_nx) * static_cast<std::uint32_t>(m_ny);
    m_pkey.resize(n);
    const bool contain = m_def.particles.contain;
    std::fill(m_cell_start.begin(), m_cell_start.end(), 0u);
    for (std::size_t i = 0; i < n; ++i) {
        const V2 p = m_px[i];
        std::uint32_t key = N;
        if (std::isfinite(p.x) && std::isfinite(p.y)) {
            const bool out = p.x < 0.0_r || p.y < 0.0_r || p.x > m_size.x || p.y > m_size.y;
            if (!out || contain) {
                const int c = particle_cell(p);
                const int ci = c % m_nx, cj = c / m_nx;
                const bool buried = m_solid[static_cast<std::size_t>(c)] && static_solid(ci - 1, cj) && static_solid(ci + 1, cj) && static_solid(ci, cj - 1) && static_solid(ci, cj + 1);
                if (!buried) key = static_cast<std::uint32_t>(c);
            }
        }
        m_pkey[i] = key;
        ++m_cell_start[key + 1];
    }
    for (std::uint32_t c = 0; c <= N; ++c) m_cell_start[c + 1] += m_cell_start[c];
    std::copy(m_cell_start.begin(), m_cell_start.end(), m_cell_fill_count.begin());
    m_tmp_a.resize(n);
    m_tmp_b.resize(n);
    m_tmp_c.resize(n);
    m_tmp_d.resize(n);
    m_tmp_u8.resize(n);
    m_tmp_u32.resize(n);
    for (std::size_t i = 0; i < n; ++i) {
        const std::uint32_t d = m_cell_fill_count[m_pkey[i]]++;
        m_tmp_a[d] = m_px[i];
        m_tmp_b[d] = m_pv[i];
        m_tmp_c[d] = m_cu[i];
        m_tmp_d[d] = m_cv[i];
        m_tmp_u8[d] = m_pmat[i];
        m_tmp_u32[d] = m_pcol[i];
    }
    m_px.swap(m_tmp_a);
    m_pv.swap(m_tmp_b);
    m_cu.swap(m_tmp_c);
    m_cv.swap(m_tmp_d);
    m_pmat.swap(m_tmp_u8);
    m_pcol.swap(m_tmp_u32);
    const std::size_t alive = m_cell_start[N];
    if (alive != n) {
        m_px.resize(alive);
        m_pv.resize(alive);
        m_cu.resize(alive);
        m_cv.resize(alive);
        m_pmat.resize(alive);
        m_pcol.resize(alive);
        m_world_dirty = true;
    }
}

void FluidVolume2D::separate_particles(detail::FluidTaskPool& pool, int iterations) {
    const FluidParticleSettings2D& ps = m_def.particles;
    if (iterations <= 0 || m_px.empty()) return;
    const Real sep = std::clamp(ps.separation, 0.0_r, 1.0_r);
    const Real min_d = 2.0_r * m_radius;
    const Real min_d2 = min_d * min_d;
    const Real range = std::min(3.0_r * m_radius, m_cs);
    const Real range2 = range * range;
    const bool cohesion = m_any_cohesion && range > min_d * 1.01_r;
    const Real reach = cohesion ? range2 : min_d2;
    const Real span = std::max(range - min_d, 1e-9_r);
    V2* px = m_px.data();
    const std::uint8_t* mat = m_pmat.data();
    const Real* coh = m_mat_cohesion.data();
    const std::uint32_t* start = m_cell_start.data();
    const int nx = m_nx, ny = m_ny;
    auto row = [&](int cj) {
        const int di[4] = { 1, -1, 0, 1 };
        const int dj[4] = { 0, 1, 1, 1 };
        for (int ci = 0; ci < nx; ++ci) {
            const std::uint32_t c = static_cast<std::uint32_t>(cj * nx + ci);
            const std::uint32_t a0 = start[c], a1 = start[c + 1];
            if (a0 == a1) continue;
            for (std::uint32_t a = a0; a < a1; ++a) {
                V2 pa = px[a];
                const std::uint8_t ma = mat[a];
                auto pair = [&](std::uint32_t b) {
                    V2& pb = px[b];
                    const Real dx = pb.x - pa.x, dy = pb.y - pa.y;
                    const Real d2 = dx * dx + dy * dy;
                    if (d2 >= reach) return;
                    if (d2 < 1e-20_r) {
                        const Real e = 0.01_r * min_d;
                        pa.x -= e;
                        pb.x += e;
                        return;
                    }
                    const Real dist = std::sqrt(d2);
                    if (d2 < min_d2) {
                        const Real s = 0.5_r * sep * (min_d - dist) / dist;
                        pa.x -= dx * s;
                        pa.y -= dy * s;
                        pb.x += dx * s;
                        pb.y += dy * s;
                    } else if (cohesion && mat[b] == ma) {
                        const Real k = coh[ma];
                        if (k <= 0.0_r) return;
                        const Real s = 0.5_r * k * (dist - min_d) * (range - dist) / (span * dist);
                        pa.x += dx * s;
                        pa.y += dy * s;
                        pb.x -= dx * s;
                        pb.y -= dy * s;
                    }
                };
                for (std::uint32_t b = a + 1; b < a1; ++b) pair(b);
                for (int q = 0; q < 4; ++q) {
                    const int ni = ci + di[q], nj = cj + dj[q];
                    if (ni < 0 || ni >= nx || nj >= ny) continue;
                    const std::uint32_t nc = static_cast<std::uint32_t>(nj * nx + ni);
                    for (std::uint32_t b = start[nc]; b < start[nc + 1]; ++b) pair(b);
                }
                px[a] = pa;
            }
        }
    };
    const std::size_t half = static_cast<std::size_t>((ny + 1) / 2);
    for (int it = 0; it < iterations; ++it) {
        for (int phase = 0; phase < 2; ++phase) {
            const std::size_t rows = phase == 0 ? half : static_cast<std::size_t>(ny / 2);
            if (pool.size() > 1 && rows > 1) {
                pool.parallel_for(rows, [&row, phase](std::size_t k) { row(static_cast<int>(k) * 2 + phase); });
            } else {
                for (std::size_t k = 0; k < rows; ++k) row(static_cast<int>(k) * 2 + phase);
            }
        }
    }
}

void FluidVolume2D::sweep_walls(detail::FluidTaskPool& pool) {
    const std::size_t n = m_px.size();
    const Real cs = m_cs;
    const Real inv = 1.0_r / cs;
    pool.parallel_range(n, 2048, [&](std::size_t b0, std::size_t b1) {
        for (std::size_t i = b0; i < b1; ++i) {
            const V2 a = m_pdv[i];
            const V2 b = m_px[i];
            const V2 d = b - a;
            const Real len = std::max(std::abs(d.x), std::abs(d.y));
            if (len < 0.5_r * cs) continue;
            const int steps = static_cast<int>(std::ceil(len * inv * 2.0_r));
            const Real dt = 1.0_r / static_cast<Real>(steps);
            int pi = static_cast<int>(std::floor(a.x * inv)), pj = static_cast<int>(std::floor(a.y * inv));
            for (int k = 1; k <= steps; ++k) {
                const V2 q = a + d * (dt * static_cast<Real>(k));
                const int qi = static_cast<int>(std::floor(q.x * inv)), qj = static_cast<int>(std::floor(q.y * inv));
                if (qi == pi && qj == pj) continue;
                if (qi >= 0 && qj >= 0 && qi < m_nx && qj < m_ny && m_solid[static_cast<std::size_t>(qj * m_nx + qi)]) {
                    V2 stop = a + d * (dt * static_cast<Real>(k - 1));
                    if (qi != pi) stop.x = qi > pi ? static_cast<Real>(qi) * cs - 1e-3_r * cs : static_cast<Real>(qi + 1) * cs + 1e-3_r * cs;
                    if (qj != pj) stop.y = qj > pj ? static_cast<Real>(qj) * cs - 1e-3_r * cs : static_cast<Real>(qj + 1) * cs + 1e-3_r * cs;
                    m_px[i] = stop;
                    V2 v = m_pv[i];
                    if (qi != pi) v.x = 0.0_r;
                    if (qj != pj) v.y = 0.0_r;
                    m_pv[i] = v;
                    break;
                }
                pi = qi;
                pj = qj;
            }
        }
    });
}

void FluidVolume2D::collide_particles(detail::FluidTaskPool& pool, Real h) {
    static_cast<void>(h);
    const std::size_t n = m_px.size();
    m_pcontact.resize(n);
    m_pdv.resize(n);
    for (NearBody& nb : m_near) {
        const V2 a = to_local(nb.box.min), b = to_local(nb.box.max);
        nb.local = AABB(V2{ std::min(a.x, b.x) - m_radius, std::min(a.y, b.y) - m_radius }, V2{ std::max(a.x, b.x) + m_radius, std::max(a.y, b.y) + m_radius });
    }
    const Real r = m_radius;
    const Real cs = m_cs;
    const bool contain = m_def.particles.contain;
    const bool walls = m_solid_count > 0;
    const std::uint8_t* solid = m_solid.data();
    const int nx = m_nx, ny = m_ny;
    auto static_solid = [solid, contain, nx, ny](int i, int j) -> bool {
        if (i < 0 || j < 0 || i >= nx || j >= ny) return contain;
        return solid[j * nx + i] != 0;
    };
    pool.parallel_range(n, 1024, [&](std::size_t b0, std::size_t b1) {
        for (std::size_t i = b0; i < b1; ++i) {
            V2 p = m_px[i];
            V2 v = m_pv[i];
            const V2 v_in = v;
            const std::uint8_t mat = m_pmat[i];
            m_pcontact[i] = -1;
            int ci = std::clamp(static_cast<int>(std::floor(p.x / cs)), 0, m_nx - 1);
            int cj = std::clamp(static_cast<int>(std::floor(p.y / cs)), 0, m_ny - 1);
            V2 wn{};
            if (!walls) {
                if (contain) {
                    if (p.x < r) { p.x = r; wn.x += 1.0_r; }
                    if (p.x > m_size.x - r) { p.x = m_size.x - r; wn.x -= 1.0_r; }
                    if (p.y < r) { p.y = r; wn.y += 1.0_r; }
                    if (p.y > m_size.y - r) { p.y = m_size.y - r; wn.y -= 1.0_r; }
                }
            } else if (solid[static_cast<std::size_t>(cj * m_nx + ci)]) {
                const bool trapped = static_solid(ci - 1, cj) && static_solid(ci + 1, cj) && static_solid(ci, cj - 1) && static_solid(ci, cj + 1);
                if (trapped) {
                    const V2 old = m_pdv[i];
                    const int oi = std::clamp(static_cast<int>(std::floor(old.x / cs)), 0, m_nx - 1);
                    const int oj = std::clamp(static_cast<int>(std::floor(old.y / cs)), 0, m_ny - 1);
                    if (!solid[static_cast<std::size_t>(oj * m_nx + oi)]) {
                        p = old;
                        ci = oi;
                        cj = oj;
                        v = V2{};
                    } else {
                        int bi = -1, bj = -1;
                        Real bd = std::numeric_limits<Real>::max();
                        for (int dj = -2; dj <= 2; ++dj) {
                            for (int di = -2; di <= 2; ++di) {
                                const int qi = ci + di, qj = cj + dj;
                                if (qi < 0 || qj < 0 || qi >= m_nx || qj >= m_ny || solid[static_cast<std::size_t>(qj * m_nx + qi)]) continue;
                                const Real ex = (static_cast<Real>(qi) + 0.5_r) * cs - p.x, ey = (static_cast<Real>(qj) + 0.5_r) * cs - p.y;
                                const Real dd = ex * ex + ey * ey;
                                if (dd < bd) {
                                    bd = dd;
                                    bi = qi;
                                    bj = qj;
                                }
                            }
                        }
                        if (bi >= 0) {
                            p = V2{ (static_cast<Real>(bi) + 0.5_r) * cs, (static_cast<Real>(bj) + 0.5_r) * cs };
                            ci = bi;
                            cj = bj;
                            v = V2{};
                        }
                    }
                }
            }
            if (walls && solid[static_cast<std::size_t>(cj * m_nx + ci)]) {
                Real best = std::numeric_limits<Real>::max();
                int bd = -1;
                const Real dl = p.x - static_cast<Real>(ci) * cs, dr = static_cast<Real>(ci + 1) * cs - p.x;
                const Real dd = p.y - static_cast<Real>(cj) * cs, du = static_cast<Real>(cj + 1) * cs - p.y;
                if (!static_solid(ci - 1, cj) && dl < best) { best = dl; bd = 0; }
                if (!static_solid(ci + 1, cj) && dr < best) { best = dr; bd = 1; }
                if (!static_solid(ci, cj - 1) && dd < best) { best = dd; bd = 2; }
                if (!static_solid(ci, cj + 1) && du < best) { best = du; bd = 3; }
                const Real e = 1e-3_r * cs;
                if (bd == 0) { p.x = static_cast<Real>(ci) * cs - e; --ci; wn.x -= 1.0_r; }
                else if (bd == 1) { p.x = static_cast<Real>(ci + 1) * cs + e; ++ci; wn.x += 1.0_r; }
                else if (bd == 2) { p.y = static_cast<Real>(cj) * cs - e; --cj; wn.y -= 1.0_r; }
                else if (bd == 3) { p.y = static_cast<Real>(cj + 1) * cs + e; ++cj; wn.y += 1.0_r; }
            }
            if (walls) {
                if (static_solid(ci - 1, cj) && p.x < static_cast<Real>(ci) * cs + r) { p.x = static_cast<Real>(ci) * cs + r; wn.x += 1.0_r; }
                if (static_solid(ci + 1, cj) && p.x > static_cast<Real>(ci + 1) * cs - r) { p.x = static_cast<Real>(ci + 1) * cs - r; wn.x -= 1.0_r; }
                if (static_solid(ci, cj - 1) && p.y < static_cast<Real>(cj) * cs + r) { p.y = static_cast<Real>(cj) * cs + r; wn.y += 1.0_r; }
                if (static_solid(ci, cj + 1) && p.y > static_cast<Real>(cj + 1) * cs - r) { p.y = static_cast<Real>(cj + 1) * cs - r; wn.y -= 1.0_r; }
            }
            if (wn.x != 0.0_r || wn.y != 0.0_r) {
                const Real l = std::sqrt(dot2(wn, wn));
                const V2 nn = wn * (1.0_r / l);
                const Real vn = dot2(v, nn);
                if (vn < 0.0_r) {
                    const Real e = -vn > 0.5_r ? m_mat_restitution[mat] : 0.0_r;
                    V2 vt = v - nn * vn;
                    const Real lt = std::sqrt(dot2(vt, vt));
                    const Real mu = m_mat_friction[mat];
                    if (lt > 1e-9_r) vt = vt * std::max(0.0_r, 1.0_r - mu * (-vn) * (1.0_r + e) / lt);
                    v = vt - nn * (vn * e);
                }
            }
            Real best_depth = -1.0_r;
            for (std::size_t k = 0; k < m_near.size(); ++k) {
                const NearBody& nb = m_near[k];
                if (!nb.local.contains(p)) continue;
                const auto& cols = nb.body->colliders();
                V2 wp = to_world(p);
                for (std::size_t c = 0; c < cols.size(); ++c) {
                    if (cols[c].is_sensor || !cols[c].world_aabb.fatten(r).contains(wp)) continue;
                    V2 nrm{};
                    Real depth = 0.0_r;
                    if (!detail::fluid_collider_contact_2d(cols[c], nb.body->transform(), wp, r, nrm, depth)) continue;
                    wp = wp + nrm * depth;
                    p = to_local(wp);
                    const V2 ln = local_dir(nrm);
                    const V2 vb = local_dir(nb.body->get_velocity_at(wp));
                    const V2 rel = v - vb;
                    const Real vn = dot2(rel, ln);
                    if (vn < 0.0_r) {
                        const Collider2D& col = cols[c];
                        const Real e = -vn > std::min(0.3_r, col.material.restitution_threshold) ? std::max(m_mat_restitution[mat], col.material.restitution) : 0.0_r;
                        const Real mu = std::sqrt(std::max(m_mat_friction[mat], 0.0_r) * std::max(col.material.friction, 0.0_r));
                        V2 vt = rel - ln * vn;
                        const Real lt = std::sqrt(dot2(vt, vt));
                        if (lt > 1e-9_r) vt = vt * std::max(0.0_r, 1.0_r - mu * (-vn) * (1.0_r + e) / lt);
                        v = vb + vt - ln * (vn * e);
                    }
                    if (depth > best_depth) {
                        best_depth = depth;
                        m_pcontact[i] = static_cast<std::int32_t>(k);
                    }
                }
            }
            m_px[i] = p;
            m_pdv[i] = v - v_in;
            m_pv[i] = v;
        }
    });
    if (m_near.empty()) return;
    const Real push = m_def.coupling.two_way ? m_def.coupling.push_scale : 0.0_r;
    std::size_t contacts = 0;
    for (std::size_t i = 0; i < n; ++i) {
        const std::int32_t k = m_pcontact[i];
        if (k < 0) continue;
        ++contacts;
        NearBody& nb = m_near[static_cast<std::size_t>(k)];
        if (!nb.dynamic || push <= 0.0_r) continue;
        const V2 J = world_dir(m_pdv[i]) * (-particle_mass(m_pmat[i]) * push);
        nb.impulse = nb.impulse + J;
        nb.angular += cross2(to_world(m_px[i]) - nb.body->world_center(), J);
        nb.max_rel = std::max(nb.max_rel, std::sqrt(dot2(m_pdv[i], m_pdv[i])));
    }
    m_contact_count += contacts;
}

void FluidVolume2D::apply_body_impulses() {
    for (NearBody& nb : m_near) {
        if (!nb.dynamic || (nb.impulse.x == 0.0_r && nb.impulse.y == 0.0_r && nb.angular == 0.0_r)) continue;
        RigidBody2D& b = *nb.body;
        const Real im = b.inv_mass();
        const Real dv = std::sqrt(dot2(nb.impulse, nb.impulse)) * im;
        Real s = 1.0_r;
        if (dv > nb.max_rel && dv > 0.0_r) s = nb.max_rel / dv;
        const Real ii = b.inv_inertia();
        const Real L = std::sqrt(std::max(dot2(nb.box.extents(), nb.box.extents()), 1e-12_r));
        const Real dw = std::abs(nb.angular) * ii * s;
        Real sa = s;
        if (dw * L > nb.max_rel && dw > 0.0_r) sa = s * nb.max_rel / (dw * L);
        b.apply_linear_impulse(nb.impulse * s);
        if (ii > 0.0_r) b.apply_angular_impulse(nb.angular * sa);
        b.wake();
        nb.impulse = V2{};
        nb.angular = 0.0_r;
        nb.max_rel = 0.0_r;
    }
}

void FluidVolume2D::particles_to_grid() {
    std::fill(m_gu.begin(), m_gu.end(), 0.0_r);
    std::fill(m_gv.begin(), m_gv.end(), 0.0_r);
    std::fill(m_guw.begin(), m_guw.end(), 0.0_r);
    std::fill(m_gvw.begin(), m_gvw.end(), 0.0_r);
    std::fill(m_cdens.begin(), m_cdens.end(), 0.0_r);
    std::fill(m_crho.begin(), m_crho.end(), 0.0_r);
    std::fill(m_cmu.begin(), m_cmu.end(), 0.0_r);
    std::fill(m_cvisc.begin(), m_cvisc.end(), 0.0_r);
    std::fill(m_cell_fill_count.begin(), m_cell_fill_count.end(), 0u);
    const std::size_t n = m_px.size();
    const Real inv = 1.0_r / m_cs;
    const std::size_t su = static_cast<std::size_t>(m_nx + 1);
    const std::size_t sv = static_cast<std::size_t>(m_nx);
    const bool apic = m_def.particles.transfer == FluidTransfer2D::apic;
    for (std::size_t p = 0; p < n; ++p) {
        const V2 x = m_px[p];
        const V2 v = m_pv[p];
        const std::uint8_t m = m_pmat[p];
        ++m_cell_fill_count[static_cast<std::size_t>(particle_cell(x))];
        {
            const Real fx = x.x * inv, fy = x.y * inv - 0.5_r;
            const int i0 = std::clamp(static_cast<int>(std::floor(fx)), 0, m_nx - 1);
            const int j0 = std::clamp(static_cast<int>(std::floor(fy)), 0, m_ny - 2);
            const Real tx = std::clamp(fx - static_cast<Real>(i0), 0.0_r, 1.0_r), ty = std::clamp(fy - static_cast<Real>(j0), 0.0_r, 1.0_r);
            const std::size_t f = static_cast<std::size_t>(j0) * su + static_cast<std::size_t>(i0);
            const Real w[4] = { (1.0_r - tx) * (1.0_r - ty), tx * (1.0_r - ty), (1.0_r - tx) * ty, tx * ty };
            const std::size_t idx[4] = { f, f + 1, f + su, f + su + 1 };
            if (apic) {
                const V2 c = m_cu[p];
                const Real bx = static_cast<Real>(i0) * m_cs - x.x, by = (static_cast<Real>(j0) + 0.5_r) * m_cs - x.y;
                const Real dx[4] = { bx, bx + m_cs, bx, bx + m_cs };
                const Real dy[4] = { by, by, by + m_cs, by + m_cs };
                for (int q = 0; q < 4; ++q) {
                    m_gu[idx[q]] += w[q] * (v.x + c.x * dx[q] + c.y * dy[q]);
                    m_guw[idx[q]] += w[q];
                }
            } else {
                for (int q = 0; q < 4; ++q) {
                    m_gu[idx[q]] += w[q] * v.x;
                    m_guw[idx[q]] += w[q];
                }
            }
        }
        {
            const Real fx = x.x * inv - 0.5_r, fy = x.y * inv;
            const int i0 = std::clamp(static_cast<int>(std::floor(fx)), 0, m_nx - 2);
            const int j0 = std::clamp(static_cast<int>(std::floor(fy)), 0, m_ny - 1);
            const Real tx = std::clamp(fx - static_cast<Real>(i0), 0.0_r, 1.0_r), ty = std::clamp(fy - static_cast<Real>(j0), 0.0_r, 1.0_r);
            const std::size_t f = static_cast<std::size_t>(j0) * sv + static_cast<std::size_t>(i0);
            const Real w[4] = { (1.0_r - tx) * (1.0_r - ty), tx * (1.0_r - ty), (1.0_r - tx) * ty, tx * ty };
            const std::size_t idx[4] = { f, f + 1, f + sv, f + sv + 1 };
            if (apic) {
                const V2 c = m_cv[p];
                const Real bx = (static_cast<Real>(i0) + 0.5_r) * m_cs - x.x, by = static_cast<Real>(j0) * m_cs - x.y;
                const Real dx[4] = { bx, bx + m_cs, bx, bx + m_cs };
                const Real dy[4] = { by, by, by + m_cs, by + m_cs };
                for (int q = 0; q < 4; ++q) {
                    m_gv[idx[q]] += w[q] * (v.y + c.x * dx[q] + c.y * dy[q]);
                    m_gvw[idx[q]] += w[q];
                }
            } else {
                for (int q = 0; q < 4; ++q) {
                    m_gv[idx[q]] += w[q] * v.y;
                    m_gvw[idx[q]] += w[q];
                }
            }
        }
        {
            const Real fx = x.x * inv - 0.5_r, fy = x.y * inv - 0.5_r;
            const int i0 = std::clamp(static_cast<int>(std::floor(fx)), 0, m_nx - 2);
            const int j0 = std::clamp(static_cast<int>(std::floor(fy)), 0, m_ny - 2);
            const Real tx = std::clamp(fx - static_cast<Real>(i0), 0.0_r, 1.0_r), ty = std::clamp(fy - static_cast<Real>(j0), 0.0_r, 1.0_r);
            const std::size_t f = static_cast<std::size_t>(j0) * sv + static_cast<std::size_t>(i0);
            const Real w[4] = { (1.0_r - tx) * (1.0_r - ty), tx * (1.0_r - ty), (1.0_r - tx) * ty, tx * ty };
            const std::size_t idx[4] = { f, f + 1, f + sv, f + sv + 1 };
            const Real rho = m_mat_rho[m], mu = m_mat_mu[m], vis = m_mat_visc[m];
            for (int q = 0; q < 4; ++q) {
                m_cdens[idx[q]] += w[q];
                m_crho[idx[q]] += w[q] * rho;
                m_cmu[idx[q]] += w[q] * mu;
                m_cvisc[idx[q]] += w[q] * vis;
            }
        }
    }
    for (std::size_t f = 0; f < m_gu.size(); ++f) m_gu[f] = m_guw[f] > 0.0_r ? m_gu[f] / m_guw[f] : 0.0_r;
    for (std::size_t f = 0; f < m_gv.size(); ++f) m_gv[f] = m_gvw[f] > 0.0_r ? m_gv[f] / m_gvw[f] : 0.0_r;
    const Real rho0 = m_mat_rho[0], mu0 = m_mat_mu[0], vis0 = m_mat_visc[0];
    for (std::size_t c = 0; c < m_cdens.size(); ++c) {
        const Real d = m_cdens[c];
        if (d > 1e-6_r) {
            const Real k = 1.0_r / d;
            m_crho[c] *= k;
            m_cmu[c] *= k;
            m_cvisc[c] *= k;
        } else {
            m_crho[c] = rho0;
            m_cmu[c] = mu0;
            m_cvisc[c] = vis0;
        }
    }
}

void FluidVolume2D::classify_cells() {
    const std::size_t n = m_ctype.size();
    for (std::size_t c = 0; c < n; ++c) {
        if (m_solid[c] || m_occ[c]) m_ctype[c] = kSolidCell;
        else m_ctype[c] = m_cell_fill_count[c] > 0 ? kFluid : kAir;
    }
    const int limit = m_def.particles.bubble_cells;
    if (limit <= 0) return;
    std::vector<std::uint32_t>& label = m_bubble_label;
    label.assign(n, 0u);
    std::vector<std::uint32_t>& queue = m_bubble_queue;
    std::uint32_t next = 1;
    for (std::size_t c0 = 0; c0 < n; ++c0) {
        if (m_ctype[c0] != kAir || label[c0]) continue;
        queue.clear();
        queue.push_back(static_cast<std::uint32_t>(c0));
        label[c0] = next;
        bool open = false;
        std::size_t head = 0;
        while (head < queue.size()) {
            const std::uint32_t c = queue[head++];
            const int i = static_cast<int>(c % static_cast<std::uint32_t>(m_nx)), j = static_cast<int>(c / static_cast<std::uint32_t>(m_nx));
            if (j == m_ny - 1 || (!m_def.particles.contain && (i == 0 || j == 0 || i == m_nx - 1))) open = true;
            const int ni[4] = { i - 1, i + 1, i, i };
            const int nj[4] = { j, j, j - 1, j + 1 };
            for (int q = 0; q < 4; ++q) {
                if (ni[q] < 0 || nj[q] < 0 || ni[q] >= m_nx || nj[q] >= m_ny) continue;
                const std::size_t a = static_cast<std::size_t>(nj[q] * m_nx + ni[q]);
                if (m_ctype[a] != kAir || label[a]) continue;
                label[a] = next;
                queue.push_back(static_cast<std::uint32_t>(a));
            }
        }
        if (!open && static_cast<int>(queue.size()) <= limit) {
            for (std::uint32_t c : queue) m_ctype[c] = kBubble;
        }
        ++next;
    }
}

void FluidVolume2D::solid_face_velocities() {
    const bool contain = m_def.particles.contain;
    const std::size_t su = static_cast<std::size_t>(m_nx + 1);
    for (int j = 0; j < m_ny; ++j) {
        for (int i = 0; i <= m_nx; ++i) {
            const bool lv = i > 0, rv = i < m_nx;
            const std::size_t lc = static_cast<std::size_t>(j * m_nx + i - 1), rc = static_cast<std::size_t>(j * m_nx + i);
            const bool ls = lv ? m_ctype[lc] == kSolidCell : contain;
            const bool rs = rv ? m_ctype[rc] == kSolidCell : contain;
            if (!ls && !rs) continue;
            Real vel = 0.0_r;
            int cnt = 0;
            if (lv && m_occ[lc]) { vel += m_occ_vel[lc].x; ++cnt; }
            if (rv && m_occ[rc]) { vel += m_occ_vel[rc].x; ++cnt; }
            m_gu[static_cast<std::size_t>(j) * su + static_cast<std::size_t>(i)] = cnt ? vel / static_cast<Real>(cnt) : 0.0_r;
        }
    }
    const std::size_t sv = static_cast<std::size_t>(m_nx);
    for (int j = 0; j <= m_ny; ++j) {
        for (int i = 0; i < m_nx; ++i) {
            const bool dv = j > 0, uv = j < m_ny;
            const std::size_t dc = static_cast<std::size_t>((j - 1) * m_nx + i), uc = static_cast<std::size_t>(j * m_nx + i);
            const bool ds = dv ? m_ctype[dc] == kSolidCell : contain;
            const bool us = uv ? m_ctype[uc] == kSolidCell : contain;
            if (!ds && !us) continue;
            Real vel = 0.0_r;
            int cnt = 0;
            if (dv && m_occ[dc]) { vel += m_occ_vel[dc].y; ++cnt; }
            if (uv && m_occ[uc]) { vel += m_occ_vel[uc].y; ++cnt; }
            m_gv[static_cast<std::size_t>(j) * sv + static_cast<std::size_t>(i)] = cnt ? vel / static_cast<Real>(cnt) : 0.0_r;
        }
    }
    std::copy(m_gu.begin(), m_gu.end(), m_gu0.begin());
    std::copy(m_gv.begin(), m_gv.end(), m_gv0.begin());
}

void FluidVolume2D::grid_viscosity(Real h) {
    if (!m_any_viscous) return;
    const bool contain = m_def.particles.contain;
    auto ct = [&](int i, int j) -> std::uint8_t {
        if (i < 0 || j < 0 || i >= m_nx || j >= m_ny) return contain ? kSolidCell : kAir;
        const std::uint8_t t = m_ctype[static_cast<std::size_t>(j * m_nx + i)];
        return t == kBubble ? kAir : t;
    };
    auto cv = [&](int i, int j) -> Real { return (i < 0 || j < 0 || i >= m_nx || j >= m_ny) ? 0.0_r : m_cvisc[static_cast<std::size_t>(j * m_nx + i)]; };
    for (int axis = 0; axis < 2; ++axis) {
        std::vector<Real>& g = axis == 0 ? m_gu : m_gv;
        std::vector<Real>& src = m_gscratch;
        const int fw = axis == 0 ? m_nx + 1 : m_nx;
        const int fh = axis == 0 ? m_ny : m_ny + 1;
        auto face_kind = [&](int i, int j) -> int {
            if (i < 0 || j < 0 || i >= fw || j >= fh) return -1;
            const std::uint8_t a = axis == 0 ? ct(i - 1, j) : ct(i, j - 1);
            const std::uint8_t b = ct(i, j);
            if (a == kSolidCell || b == kSolidCell) return 2;
            if (a == kFluid || b == kFluid) return 1;
            return 0;
        };
        m_visc_faces.clear();
        m_visc_nb.clear();
        m_visc_d.clear();
        Real dmax = 0.0_r;
        for (int j = 0; j < fh; ++j) {
            for (int i = 0; i < fw; ++i) {
                if (face_kind(i, j) != 1) continue;
                Real sum = 0.0_r;
                int c = 0;
                const int ai = axis == 0 ? i - 1 : i, aj = axis == 0 ? j : j - 1;
                if (ct(ai, aj) == kFluid) { sum += cv(ai, aj); ++c; }
                if (ct(i, j) == kFluid) { sum += cv(i, j); ++c; }
                const Real d = c ? h * sum / static_cast<Real>(c) : 0.0_r;
                if (d <= 2e-3_r) continue;
                dmax = std::max(dmax, d);
                m_visc_faces.push_back(static_cast<std::uint32_t>(j * fw + i));
                m_visc_d.push_back(d);
                const int oi[4] = { -1, 1, 0, 0 };
                const int oj[4] = { 0, 0, -1, 1 };
                for (int q = 0; q < 4; ++q) {
                    const int ni = i + oi[q], nj = j + oj[q];
                    const int k = face_kind(ni, nj);
                    if (k == -1) m_visc_nb.push_back(contain ? -2 : -1);
                    else if (k == 0) m_visc_nb.push_back(-1);
                    else m_visc_nb.push_back(nj * fw + ni);
                }
            }
        }
        if (m_visc_faces.empty()) continue;
        const int base = m_def.particles.viscosity_iterations > 0 ? m_def.particles.viscosity_iterations : std::clamp(m_quality.solver_iterations * 3, 4, 40);
        const int iters = m_def.particles.viscosity_iterations > 0 ? base : std::clamp(static_cast<int>(std::ceil(static_cast<Real>(base) + 6.0_r * dmax)), base, 80);
        const std::size_t nf = m_visc_faces.size();
        for (std::uint32_t f : m_visc_faces) src[f] = g[f];
        for (int it = 0; it < iters; ++it) {
            const bool forward = (it & 1) == 0;
            for (std::size_t q = 0; q < nf; ++q) {
                const std::size_t k = forward ? q : nf - 1 - q;
                const std::size_t f = m_visc_faces[k];
                const std::int32_t* nb = &m_visc_nb[k * 4];
                Real sum = 0.0_r;
                int cnt = 0;
                for (int e = 0; e < 4; ++e) {
                    if (nb[e] == -1) continue;
                    ++cnt;
                    if (nb[e] >= 0) sum += g[static_cast<std::size_t>(nb[e])];
                }
                const Real d = m_visc_d[k];
                g[f] = (src[f] + d * sum) / (1.0_r + d * static_cast<Real>(cnt));
            }
        }
    }
}

int FluidVolume2D::pressure_solve(Real h, int max_iterations) {
    const std::size_t N = m_ctype.size();
    const bool contain = m_def.particles.contain;
    m_flist.clear();
    for (std::size_t c = 0; c < N; ++c) {
        if (m_ctype[c] == kBubble) m_ctype[c] = kFluid;
        if (m_ctype[c] == kFluid) {
            m_fidx[c] = static_cast<std::int32_t>(m_flist.size());
            m_flist.push_back(static_cast<std::uint32_t>(c));
        } else {
            m_fidx[c] = -1;
        }
    }
    const std::size_t M = m_flist.size();
    if (M == 0) return 0;
    m_pcg_b.resize(M);
    m_pcg_x.resize(M);
    m_pcg_r.resize(M);
    m_pcg_z.resize(M);
    m_pcg_s.resize(M);
    m_pcg_q.resize(M);
    m_pcg_diag.resize(M);
    m_pcg_ax.resize(M);
    m_pcg_ay.resize(M);
    m_pcg_pre.resize(M);
    m_pcg_nb.resize(M * 4);
    const std::size_t su = static_cast<std::size_t>(m_nx + 1);
    const std::size_t sv = static_cast<std::size_t>(m_nx);
    const double rho_ref = static_cast<double>(m_mat_rho[0]);
    const double drift = static_cast<double>(std::max(0.0_r, m_def.particles.drift_correction)) * 0.25 * static_cast<double>(m_cs / h);
    const double rest = static_cast<double>(m_rest);
    auto ctype = [&](int i, int j) -> std::uint8_t {
        if (i < 0 || j < 0 || i >= m_nx || j >= m_ny) return contain ? kSolidCell : kAir;
        return m_ctype[static_cast<std::size_t>(j * m_nx + i)];
    };
    double bmax = 0.0;
    const double warm = m_last_h > 0.0_r ? static_cast<double>(h / m_last_h) : 0.0;
    m_last_h = h;
    for (std::size_t r = 0; r < M; ++r) {
        const std::size_t c = m_flist[r];
        const int i = static_cast<int>(c % static_cast<std::size_t>(m_nx)), j = static_cast<int>(c / static_cast<std::size_t>(m_nx));
        const double div = static_cast<double>(m_gu[static_cast<std::size_t>(j) * su + static_cast<std::size_t>(i) + 1] - m_gu[static_cast<std::size_t>(j) * su + static_cast<std::size_t>(i)] + m_gv[static_cast<std::size_t>(j + 1) * sv + static_cast<std::size_t>(i)] - m_gv[static_cast<std::size_t>(j) * sv + static_cast<std::size_t>(i)]);
        const double rc = static_cast<double>(m_crho[c]);
        double diag = 1e-4;
        const int ni[4] = { i - 1, i + 1, i, i };
        const int nj[4] = { j, j, j - 1, j + 1 };
        m_pcg_ax[r] = 0.0;
        m_pcg_ay[r] = 0.0;
        bool interior = true;
        for (int q = 0; q < 4; ++q) {
            const std::uint8_t t = ctype(ni[q], nj[q]);
            m_pcg_nb[r * 4 + static_cast<std::size_t>(q)] = -1;
            if (t == kAir) interior = false;
            if (t == kSolidCell) continue;
            double rf = rc;
            if (t == kFluid) {
                const std::size_t nc = static_cast<std::size_t>(nj[q] * m_nx + ni[q]);
                rf = 0.5 * (rc + static_cast<double>(m_crho[nc]));
                m_pcg_nb[r * 4 + static_cast<std::size_t>(q)] = m_fidx[nc];
                const double coef = rho_ref / std::max(rf, 1e-6);
                if (q == 1) m_pcg_ax[r] = -coef;
                if (q == 3) m_pcg_ay[r] = -coef;
                diag += coef;
            } else {
                diag += rho_ref / std::max(rf, 1e-6);
            }
        }
        m_pcg_diag[r] = diag;
        double target = 0.0;
        const double dens = static_cast<double>(m_cdens[c]);
        if (drift > 0.0 && dens > rest) target = drift * (dens - rest) / rest;
        else if (drift > 0.0 && dens < 0.85 * rest && interior) target = -drift * 0.5 * (0.85 * rest - dens) / rest;
        m_pcg_b[r] = target - div;
        m_pcg_x[r] = static_cast<double>(m_phi[c]) * warm;
        bmax = std::max(bmax, std::abs(m_pcg_b[r]));
    }
    auto matvec = [&](const std::vector<double>& in, std::vector<double>& out) {
        for (std::size_t r = 0; r < M; ++r) {
            double s = m_pcg_diag[r] * in[r];
            const std::int32_t* nb = &m_pcg_nb[r * 4];
            if (nb[0] >= 0) s += m_pcg_ax[static_cast<std::size_t>(nb[0])] * in[static_cast<std::size_t>(nb[0])];
            if (nb[1] >= 0) s += m_pcg_ax[r] * in[static_cast<std::size_t>(nb[1])];
            if (nb[2] >= 0) s += m_pcg_ay[static_cast<std::size_t>(nb[2])] * in[static_cast<std::size_t>(nb[2])];
            if (nb[3] >= 0) s += m_pcg_ay[r] * in[static_cast<std::size_t>(nb[3])];
            out[r] = s;
        }
    };
    const double tau = 0.97, sigma = 0.25;
    for (std::size_t r = 0; r < M; ++r) {
        const std::int32_t* nb = &m_pcg_nb[r * 4];
        double e = m_pcg_diag[r];
        if (nb[0] >= 0) {
            const std::size_t l = static_cast<std::size_t>(nb[0]);
            const double a = m_pcg_ax[l] * m_pcg_pre[l];
            e -= a * a + tau * m_pcg_ax[l] * m_pcg_ay[l] * m_pcg_pre[l] * m_pcg_pre[l];
        }
        if (nb[2] >= 0) {
            const std::size_t d = static_cast<std::size_t>(nb[2]);
            const double a = m_pcg_ay[d] * m_pcg_pre[d];
            e -= a * a + tau * m_pcg_ay[d] * m_pcg_ax[d] * m_pcg_pre[d] * m_pcg_pre[d];
        }
        if (e < sigma * m_pcg_diag[r]) e = m_pcg_diag[r];
        m_pcg_pre[r] = 1.0 / std::sqrt(e);
    }
    auto precondition = [&](const std::vector<double>& in, std::vector<double>& out) {
        for (std::size_t r = 0; r < M; ++r) {
            const std::int32_t* nb = &m_pcg_nb[r * 4];
            double t = in[r];
            if (nb[0] >= 0) {
                const std::size_t l = static_cast<std::size_t>(nb[0]);
                t -= m_pcg_ax[l] * m_pcg_pre[l] * m_pcg_q[l];
            }
            if (nb[2] >= 0) {
                const std::size_t d = static_cast<std::size_t>(nb[2]);
                t -= m_pcg_ay[d] * m_pcg_pre[d] * m_pcg_q[d];
            }
            m_pcg_q[r] = t * m_pcg_pre[r];
        }
        for (std::size_t rr = M; rr-- > 0;) {
            const std::int32_t* nb = &m_pcg_nb[rr * 4];
            double t = m_pcg_q[rr];
            if (nb[1] >= 0) t -= m_pcg_ax[rr] * m_pcg_pre[rr] * out[static_cast<std::size_t>(nb[1])];
            if (nb[3] >= 0) t -= m_pcg_ay[rr] * m_pcg_pre[rr] * out[static_cast<std::size_t>(nb[3])];
            out[rr] = t * m_pcg_pre[rr];
        }
    };
    const double tol_rel = m_def.particles.pressure_tolerance > 0.0_r ? static_cast<double>(m_def.particles.pressure_tolerance) : 1e-3;
    const double tol = std::max(tol_rel * bmax, 1e-12);
    matvec(m_pcg_x, m_pcg_s);
    double rmax = 0.0;
    for (std::size_t r = 0; r < M; ++r) {
        m_pcg_r[r] = m_pcg_b[r] - m_pcg_s[r];
        rmax = std::max(rmax, std::abs(m_pcg_r[r]));
    }
    int it = 0;
    if (rmax > tol) {
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
    std::fill(m_phi.begin(), m_phi.end(), 0.0_r);
    for (std::size_t r = 0; r < M; ++r) {
        const double v = std::isfinite(m_pcg_x[r]) ? m_pcg_x[r] : 0.0;
        m_phi[m_flist[r]] = static_cast<Real>(v);
    }
    auto phi = [&](int i, int j) -> Real {
        if (i < 0 || j < 0 || i >= m_nx || j >= m_ny) return 0.0_r;
        return m_phi[static_cast<std::size_t>(j * m_nx + i)];
    };
    auto rho = [&](int i, int j) -> Real { return m_crho[static_cast<std::size_t>(j * m_nx + i)]; };
    const Real rref = m_mat_rho[0];
    for (int j = 0; j < m_ny; ++j) {
        for (int i = 0; i <= m_nx; ++i) {
            const std::uint8_t a = ctype(i - 1, j), b = ctype(i, j);
            if (a == kSolidCell || b == kSolidCell) continue;
            if (a != kFluid && b != kFluid) continue;
            Real rf = 0.0_r;
            if (a == kFluid && b == kFluid) rf = 0.5_r * (rho(i - 1, j) + rho(i, j));
            else rf = a == kFluid ? rho(i - 1, j) : rho(i, j);
            m_gu[static_cast<std::size_t>(j) * su + static_cast<std::size_t>(i)] -= rref / std::max(rf, 1e-6_r) * (phi(i, j) - phi(i - 1, j));
        }
    }
    for (int j = 0; j <= m_ny; ++j) {
        for (int i = 0; i < m_nx; ++i) {
            const std::uint8_t a = ctype(i, j - 1), b = ctype(i, j);
            if (a == kSolidCell || b == kSolidCell) continue;
            if (a != kFluid && b != kFluid) continue;
            Real rf = 0.0_r;
            if (a == kFluid && b == kFluid) rf = 0.5_r * (rho(i, j - 1) + rho(i, j));
            else rf = a == kFluid ? rho(i, j - 1) : rho(i, j);
            m_gv[static_cast<std::size_t>(j) * sv + static_cast<std::size_t>(i)] -= rref / std::max(rf, 1e-6_r) * (phi(i, j) - phi(i, j - 1));
        }
    }
    return it;
}

void FluidVolume2D::grid_to_particles(detail::FluidTaskPool& pool, Real h) {
    const std::size_t n = m_px.size();
    bool mixing = false;
    for (std::size_t m = 1; m < m_mat_rho.size(); ++m) mixing = mixing || m_mat_rho[m] != m_mat_rho[0];
    mixing = mixing && m_def.particles.density_drift > 0.0_r;
    const V2 drift_g = m_gl * h;
    const Real inv = 1.0_r / m_cs;
    const std::size_t su = static_cast<std::size_t>(m_nx + 1);
    const std::size_t sv = static_cast<std::size_t>(m_nx);
    const bool contain = m_def.particles.contain;
    const bool apic = m_def.particles.transfer == FluidTransfer2D::apic;
    auto nonair = [&](int i, int j) -> bool {
        if (i < 0 || j < 0 || i >= m_nx || j >= m_ny) return contain;
        return m_ctype[static_cast<std::size_t>(j * m_nx + i)] != kAir;
    };
    pool.parallel_range(n, 2048, [&](std::size_t b0, std::size_t b1) {
        for (std::size_t p = b0; p < b1; ++p) {
            const V2 x = m_px[p];
            const V2 vold = m_pv[p];
            const Real flip = m_mat_flip[m_pmat[p]];
            Real out[2] = { vold.x, vold.y };
            V2 cgrad[2] = { V2{}, V2{} };
            for (int axis = 0; axis < 2; ++axis) {
                const Real fx = x.x * inv - (axis == 0 ? 0.0_r : 0.5_r);
                const Real fy = x.y * inv - (axis == 0 ? 0.5_r : 0.0_r);
                const int i0 = std::clamp(static_cast<int>(std::floor(fx)), 0, axis == 0 ? m_nx - 1 : m_nx - 2);
                const int j0 = std::clamp(static_cast<int>(std::floor(fy)), 0, axis == 0 ? m_ny - 2 : m_ny - 1);
                const Real tx = std::clamp(fx - static_cast<Real>(i0), 0.0_r, 1.0_r), ty = std::clamp(fy - static_cast<Real>(j0), 0.0_r, 1.0_r);
                const std::size_t s = axis == 0 ? su : sv;
                const std::vector<Real>& g = axis == 0 ? m_gu : m_gv;
                const std::vector<Real>& g0 = axis == 0 ? m_gu0 : m_gv0;
                const std::size_t f = static_cast<std::size_t>(j0) * s + static_cast<std::size_t>(i0);
                const std::size_t idx[4] = { f, f + 1, f + s, f + s + 1 };
                const int fi[4] = { i0, i0 + 1, i0, i0 + 1 };
                const int fj[4] = { j0, j0, j0 + 1, j0 + 1 };
                const Real w[4] = { (1.0_r - tx) * (1.0_r - ty), tx * (1.0_r - ty), (1.0_r - tx) * ty, tx * ty };
                const Real gx[4] = { -(1.0_r - ty) * inv, (1.0_r - ty) * inv, -ty * inv, ty * inv };
                const Real gy[4] = { -(1.0_r - tx) * inv, -tx * inv, (1.0_r - tx) * inv, tx * inv };
                Real ws = 0.0_r, pic = 0.0_r, dv = 0.0_r;
                V2 cg{};
                int valid = 0;
                for (int q = 0; q < 4; ++q) {
                    const bool ok = axis == 0 ? (nonair(fi[q] - 1, fj[q]) || nonair(fi[q], fj[q])) : (nonair(fi[q], fj[q] - 1) || nonair(fi[q], fj[q]));
                    if (!ok) continue;
                    ++valid;
                    ws += w[q];
                    pic += w[q] * g[idx[q]];
                    dv += w[q] * (g[idx[q]] - g0[idx[q]]);
                    cg = cg + V2{ gx[q], gy[q] } * g[idx[q]];
                }
                if (ws <= 1e-9_r) continue;
                pic /= ws;
                dv /= ws;
                if (apic) {
                    out[axis] = pic;
                    cgrad[axis] = valid == 4 ? cg : V2{};
                } else {
                    out[axis] = (1.0_r - flip) * pic + flip * ((axis == 0 ? vold.x : vold.y) + dv);
                }
            }
            if (mixing) {
                const std::size_t c = static_cast<std::size_t>(particle_cell(x));
                const Real rc = m_crho[c];
                const Real rp = m_mat_rho[m_pmat[p]];
                if (rc > 0.0_r && rp != rc && m_ctype[c] == kFluid) {
                    const Real k = m_def.particles.density_drift * (1.0_r - rc / rp);
                    out[0] += drift_g.x * k;
                    out[1] += drift_g.y * k;
                }
            }
            m_pv[p] = V2{ out[0], out[1] };
            if (apic) {
                m_cu[p] = cgrad[0];
                m_cv[p] = cgrad[1];
            }
        }
    });
}

void FluidVolume2D::mix_colors(Real h) {
    const Real rate = m_def.particles.color_mixing;
    if (rate <= 0.0_r || m_px.empty()) return;
    const std::size_t N = m_ctype.size();
    m_csum.assign(N * 5, 0.0f);
    for (std::size_t p = 0; p < m_px.size(); ++p) {
        const std::size_t c = static_cast<std::size_t>(particle_cell(m_px[p]));
        const FluidColor col = fluid_unpack_color(m_pcol[p]);
        float* s = &m_csum[c * 5];
        s[0] += col.r;
        s[1] += col.g;
        s[2] += col.b;
        s[3] += col.a;
        s[4] += 1.0f;
    }
    const float t = static_cast<float>(std::min(1.0_r, rate * h));
    for (std::size_t p = 0; p < m_px.size(); ++p) {
        const std::size_t c = static_cast<std::size_t>(particle_cell(m_px[p]));
        const float* s = &m_csum[c * 5];
        if (s[4] < 1.5f) continue;
        const float k = 1.0f / s[4];
        FluidColor col = fluid_unpack_color(m_pcol[p]);
        col.r += (s[0] * k - col.r) * t;
        col.g += (s[1] * k - col.g) * t;
        col.b += (s[2] * k - col.b) * t;
        col.a += (s[3] * k - col.a) * t;
        m_pcol[p] = fluid_pack_color(col);
    }
}

void FluidVolume2D::particle_substep(detail::FluidTaskPool& pool, Real h, int iterations) {
    sort_particles();
    const int sep = m_def.particles.separation_iterations > 0 ? m_def.particles.separation_iterations : std::clamp(m_quality.solver_iterations - 2, 1, 4);
    separate_particles(pool, sep);
    particles_to_grid();
    classify_cells();
    solid_face_velocities();
    const V2 g = m_gl * h;
    if (g.x != 0.0_r) {
        for (int j = 0; j < m_ny; ++j) {
            for (int i = 1; i < m_nx; ++i) {
                const std::uint8_t a = m_ctype[static_cast<std::size_t>(j * m_nx + i - 1)], b = m_ctype[static_cast<std::size_t>(j * m_nx + i)];
                if (a == kSolidCell || b == kSolidCell || (a == kAir && b == kAir)) continue;
                m_gu[static_cast<std::size_t>(j * (m_nx + 1) + i)] += g.x;
            }
        }
    }
    if (g.y != 0.0_r) {
        for (int j = 1; j < m_ny; ++j) {
            for (int i = 0; i < m_nx; ++i) {
                const std::uint8_t a = m_ctype[static_cast<std::size_t>((j - 1) * m_nx + i)], b = m_ctype[static_cast<std::size_t>(j * m_nx + i)];
                if (a == kSolidCell || b == kSolidCell || (a == kAir && b == kAir)) continue;
                m_gv[static_cast<std::size_t>(j * m_nx + i)] += g.y;
            }
        }
    }
    grid_viscosity(h);
    m_last_iterations = std::max(m_last_iterations, pressure_solve(h, iterations));
    grid_to_particles(pool, h);
    mix_colors(h);
    const std::size_t n = m_px.size();
    const Real vlim = m_vlim_cells * m_cs / h;
    m_pdv.resize(n);
    pool.parallel_range(n, 4096, [&](std::size_t b0, std::size_t b1) {
        for (std::size_t i = b0; i < b1; ++i) {
            V2 v = m_pv[i];
            const Real s2 = dot2(v, v);
            if (s2 > vlim * vlim) v = v * (vlim / std::sqrt(s2));
            m_pv[i] = v;
            m_pdv[i] = m_px[i];
            m_px[i] = m_px[i] + v * h;
        }
    });
    if (m_solid_count > 0) sweep_walls(pool);
    collide_particles(pool, h);
}

void FluidVolume2D::step_particles(detail::FluidTaskPool& pool, Real dt, FluidStats2D& stats) {
    run_emitters(dt);
    m_contact_count = 0;
    m_last_iterations = 0;
    m_last_substeps = 0;
    if (m_px.empty()) {
        std::fill(m_ctype.begin(), m_ctype.end(), kAir);
        std::fill(m_cdens.begin(), m_cdens.end(), 0.0_r);
        m_all_dirty = false;
        return;
    }
    if (m_asleep && m_def.allow_sleep) {
        for (NearBody& nb : m_near) nb.impulse = V2{};
        return;
    }
    m_asleep = false;
    const FluidParticleSettings2D& ps = m_def.particles;
    std::array<std::uint32_t, 64> hist{};
    const Real bin = 8.0_r * dt / m_cs;
    for (const V2& v : m_pv) {
        const Real sp = std::sqrt(v.x * v.x + v.y * v.y) * bin;
        ++hist[static_cast<std::size_t>(std::min(sp, 63.0_r))];
    }
    const std::size_t keep = m_pv.size() - m_pv.size() / 200;
    std::size_t acc = 0, top = 0;
    for (; top < 64; ++top) {
        acc += hist[top];
        if (acc >= keep) break;
    }
    Real vmax = (static_cast<Real>(top) + 1.0_r) / bin + m_g * dt;
    for (const NearBody& nb : m_near) {
        const V2 bv = nb.body->linear_velocity();
        vmax = std::max(vmax, std::sqrt(dot2(bv, bv)));
    }
    const Real cfl = ps.cfl > 0.0_r ? ps.cfl : std::clamp(m_quality.cfl * 4.0_r, 0.5_r, 3.0_r);
    m_vlim_cells = std::max(1.0_r, cfl) * 0.999_r;
    m_solid_count = 0;
    for (std::uint8_t sc : m_solid) m_solid_count += sc ? 1u : 0u;
    const int cap = std::max(1, ps.max_substeps > 0 ? ps.max_substeps : m_quality.max_substeps);
    const int substeps = std::clamp(static_cast<int>(std::ceil(dt * vmax / (cfl * m_cs))), 1, cap);
    const Real h = dt / static_cast<Real>(substeps);
    const int iters = ps.pressure_iterations > 0 ? ps.pressure_iterations : std::clamp(m_quality.solver_iterations * 20, 20, 400);
    for (int s = 0; s < substeps; ++s) particle_substep(pool, h, iters);
    apply_body_impulses();
    m_last_substeps = substeps;
    std::array<std::uint32_t, 64> sh{};
    const Real sbin = 64.0_r / std::max(ps.settle_speed * 4.0_r, 1e-6_r);
    Real mx = 0.0_r;
    for (const V2& v : m_pv) {
        const Real s2 = v.x * v.x + v.y * v.y;
        mx = std::max(mx, s2);
        ++sh[static_cast<std::size_t>(std::min(std::sqrt(s2) * sbin, 63.0_r))];
    }
    m_max_speed = std::sqrt(mx);
    const std::size_t want = m_pv.size() - m_pv.size() / 100;
    std::size_t run = 0, b = 0;
    for (; b < 64; ++b) {
        run += sh[b];
        if (run >= want) break;
    }
    const Real p99 = (static_cast<Real>(b) + 1.0_r) / sbin;
    Real slowest = 1.0_r;
    for (std::size_t m = 0; m < m_materials.size(); ++m) slowest = std::max(slowest, m_materials[m].viscosity_factor());
    const bool interacting = !m_emitters.empty();
    if (ps.settle_rate > 0.0_r && p99 < ps.settle_speed / slowest && !interacting) {
        const Real k = std::exp(-ps.settle_rate * dt);
        for (V2& v : m_pv) v = v * k;
        m_max_speed *= k;
    }
    if (ps.damping > 0.0_r) {
        const Real k = std::exp(-ps.damping * dt);
        for (V2& v : m_pv) v = v * k;
    }
    bool bodies_moving = false;
    for (const NearBody& nb : m_near) {
        const V2 bv = nb.body->linear_velocity();
        if ((nb.dynamic || nb.body->has(PhysicsFlags::LinearVelocity)) && dot2(bv, bv) > ps.wake_speed * ps.wake_speed) bodies_moving = true;
    }
    if (m_max_speed < ps.sleep_speed && !bodies_moving && m_emitters.empty()) ++m_calm_frames;
    else m_calm_frames = 0;
    if (m_def.allow_sleep && m_calm_frames >= std::max(1, ps.sleep_frames)) {
        m_asleep = true;
        std::fill(m_pv.begin(), m_pv.end(), V2{});
        std::fill(m_cu.begin(), m_cu.end(), V2{});
        std::fill(m_cv.begin(), m_cv.end(), V2{});
    }
    m_world_dirty = true;
    stats.substeps = std::max(stats.substeps, substeps);
    stats.iterations = std::max(stats.iterations, m_last_iterations);
    stats.fluid_cells += m_flist.size();
    m_all_dirty = false;
}

void FluidVolume2D::compact_particles() { sort_particles(); }

} // namespace physics
} // namespace fizmo
