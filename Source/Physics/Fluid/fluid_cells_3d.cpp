#include "fizmo_library.hpp"
#include "fluid_3d.hpp"

namespace fizmo {
namespace physics {

namespace {

using V3 = RVec3;

constexpr int kChunk = 16;
constexpr Real kNoWater = -1.0e6_r;

int cells_along(Real length, Real cell) noexcept {
    return std::max(1, static_cast<int>(std::ceil(length / cell - 1e-3_r)));
}

} // namespace

void FluidVolume3D::init_cells() {
    m_cs = std::max(1e-3_r, m_def.cell_size);
    m_nx = cells_along(m_size.x, m_cs);
    m_ny = cells_along(m_size.y, m_cs);
    m_nz = cells_along(m_size.z, m_cs);
    const std::size_t n = static_cast<std::size_t>(m_nx) * static_cast<std::size_t>(m_ny) * static_cast<std::size_t>(m_nz);
    m_solid.assign(n, 0);
    m_source.assign(n, 0);
    m_fill.assign(n, 0.0_r);
    m_next.assign(n, 0.0_r);
    m_cell_vel.assign(n, V3{});
    m_occ_cell.assign(n, 0.0_r);
    if (m_def.level == FluidLevel::blocky) {
        m_block.assign(n, 0);
        m_block_next.assign(n, 0);
        m_spread_mask.assign(n, 0);
    } else {
        m_flux.assign(n * 3, 0.0_r);
        m_scale.assign(n, 1.0_r);
    }
    m_kx = (m_nx + kChunk - 1) / kChunk;
    m_ky = (m_ny + kChunk - 1) / kChunk;
    m_kz = (m_nz + kChunk - 1) / kChunk;
    const std::size_t chunks = static_cast<std::size_t>(m_kx) * static_cast<std::size_t>(m_ky) * static_cast<std::size_t>(m_kz);
    m_chunk_changed.assign(chunks, 1);
    m_chunk_active.assign(chunks, 1);
    m_chunk_flag_scratch.assign(chunks, 0);
    m_active_list.reserve(chunks);
    if (m_def.fill > 0.0_r) {
        const Real top = std::min(m_def.fill, 1.0_r) * m_size.y;
        for (int j = 0; j < m_ny; ++j) {
            const Real lo = static_cast<Real>(j) * m_cs;
            const Real part = std::clamp((top - lo) / m_cs, 0.0_r, 1.0_r);
            if (part <= 0.0_r) break;
            for (int k = 0; k < m_nz; ++k) {
                for (int i = 0; i < m_nx; ++i) {
                    const std::size_t c = cell_index(i, j, k);
                    if (m_def.level == FluidLevel::blocky) {
                        if (part >= 0.5_r) {
                            m_block[c] = 8;
                            m_source[c] = 1;
                            m_fill[c] = 1.0_r;
                        }
                    } else {
                        m_fill[c] = part * m_def.flowing.max_mass;
                    }
                }
            }
        }
    }
    m_all_dirty = true;
}

std::size_t FluidVolume3D::chunk_of(int i, int j, int k) const noexcept {
    return (static_cast<std::size_t>(j / kChunk) * static_cast<std::size_t>(m_kz) + static_cast<std::size_t>(k / kChunk)) * static_cast<std::size_t>(m_kx) + static_cast<std::size_t>(i / kChunk);
}

void FluidVolume3D::mark_cell_dirty(int i, int j, int k) noexcept {
    ++m_version;
    if (m_chunk_changed.empty() || !valid_cell(i, j, k)) return;
    m_chunk_changed[chunk_of(i, j, k)] = 1;
}

bool FluidVolume3D::cell_of(const V3& world, int& i, int& j, int& k) const noexcept {
    if (m_nx == 0) return false;
    const V3 l = to_local(world);
    i = static_cast<int>(std::floor(l.x / m_cs));
    j = static_cast<int>(std::floor(l.y / m_cs));
    k = static_cast<int>(std::floor(l.z / m_cs));
    return valid_cell(i, j, k);
}

V3 FluidVolume3D::cell_center(int i, int j, int k) const noexcept {
    return to_world(V3{ (static_cast<Real>(i) + 0.5_r) * m_cs, (static_cast<Real>(j) + 0.5_r) * m_cs, (static_cast<Real>(k) + 0.5_r) * m_cs });
}

void FluidVolume3D::set_solid(int i, int j, int k, bool s) {
    if (!valid_cell(i, j, k)) return;
    const std::size_t c = cell_index(i, j, k);
    m_solid[c] = s ? 1 : 0;
    if (s) {
        m_fill[c] = 0.0_r;
        if (!m_block.empty()) m_block[c] = 0;
        m_source[c] = 0;
    }
    mark_cell_dirty(i, j, k);
}

void FluidVolume3D::set_solid_box(const AABB3D& box, bool s) {
    if (m_def.level == FluidLevel::waves) {
        set_wall_box(box, s);
        return;
    }
    for (int j = 0; j < m_ny; ++j) {
        for (int k = 0; k < m_nz; ++k) {
            for (int i = 0; i < m_nx; ++i) {
                if (box.contains(cell_center(i, j, k))) set_solid(i, j, k, s);
            }
        }
    }
}

bool FluidVolume3D::solid(int i, int j, int k) const noexcept {
    return !valid_cell(i, j, k) || m_solid[cell_index(i, j, k)] != 0;
}

void FluidVolume3D::set_source(int i, int j, int k, bool s) {
    if (!valid_cell(i, j, k)) return;
    const std::size_t c = cell_index(i, j, k);
    if (m_solid[c]) return;
    m_source[c] = s ? 1 : 0;
    if (s) {
        if (!m_block.empty()) m_block[c] = 8;
        m_fill[c] = m_def.level == FluidLevel::blocky ? 1.0_r : std::max(m_fill[c], m_def.flowing.max_mass);
    }
    mark_cell_dirty(i, j, k);
}

bool FluidVolume3D::source(int i, int j, int k) const noexcept {
    return valid_cell(i, j, k) && m_source[cell_index(i, j, k)] != 0;
}

void FluidVolume3D::set_amount(int i, int j, int k, Real a) {
    if (!valid_cell(i, j, k)) return;
    const std::size_t c = cell_index(i, j, k);
    if (m_solid[c]) return;
    if (m_def.level == FluidLevel::blocky) {
        const int lvl = std::clamp(static_cast<int>(std::lround(a * 8.0_r)), 0, 8);
        m_block[c] = static_cast<std::uint8_t>(lvl);
        m_fill[c] = static_cast<Real>(lvl) / 8.0_r;
    } else {
        m_fill[c] = std::max(0.0_r, a);
    }
    mark_cell_dirty(i, j, k);
}

Real FluidVolume3D::amount(int i, int j, int k) const noexcept {
    if (!valid_cell(i, j, k)) return 0.0_r;
    return m_fill[cell_index(i, j, k)];
}

int FluidVolume3D::block_level(int i, int j, int k) const noexcept {
    if (!valid_cell(i, j, k) || m_block.empty()) return 0;
    return m_block[cell_index(i, j, k)];
}

void FluidVolume3D::fill_box(const AABB3D& box, Real a) {
    if (m_def.level == FluidLevel::realistic) {
        add_particle_box(box);
        return;
    }
    if (m_def.level == FluidLevel::waves) {
        for (int k = 0; k < m_cz; ++k) {
            for (int i = 0; i < m_cx; ++i) {
                const V3 lc{ (static_cast<Real>(i) + 0.5_r) * m_dx, 0.0_r, (static_cast<Real>(k) + 0.5_r) * m_dx };
                const V3 lo = to_local(box.min), hi = to_local(box.max);
                const V3 l0 = vec3::min(lo, hi), l1 = vec3::max(lo, hi);
                if (lc.x < l0.x || lc.x > l1.x || lc.z < l0.z || lc.z > l1.z) continue;
                const std::size_t c = column_index(i, k);
                if (m_wall[c]) continue;
                m_depth[c] = std::max(m_depth[c], (l1.y - m_ground[c]) * a);
                mark_column_dirty(i, k);
            }
        }
        m_heights_dirty = true;
        return;
    }
    if (m_def.level == FluidLevel::still) {
        set_still_level(vec3::dot(box.max, m_up));
        return;
    }
    for (int j = 0; j < m_ny; ++j) {
        for (int k = 0; k < m_nz; ++k) {
            for (int i = 0; i < m_nx; ++i) {
                if (box.contains(cell_center(i, j, k))) set_amount(i, j, k, a * (m_def.level == FluidLevel::flowing ? m_def.flowing.max_mass : 1.0_r));
            }
        }
    }
}

void FluidVolume3D::voxelize(Simulation3D& sim, bool static_only) {
    QueryFilter3D f;
    f.include_dynamic = !static_only;
    f.include_kinematic = !static_only;
    if (m_def.level == FluidLevel::waves) {
        for (int k = 0; k < m_cz; ++k) {
            for (int i = 0; i < m_cx; ++i) {
                const V3 top = to_world(V3{ (static_cast<Real>(i) + 0.5_r) * m_dx, m_size.y, (static_cast<Real>(k) + 0.5_r) * m_dx });
                const std::size_t c = column_index(i, k);
                auto hit = sim.raycast(top + m_up * 1e-3_r, m_up * -1.0_r, m_size.y + 1e-3_r, f);
                if (!hit) continue;
                const Real g = std::max(0.0_r, m_size.y - (hit->distance - 1e-3_r));
                if (hit->distance <= 2e-3_r) {
                    m_wall[c] = 1;
                    m_depth[c] = 0.0_r;
                } else {
                    const Real surface = m_ground[c] + m_depth[c];
                    m_ground[c] = g;
                    m_depth[c] = std::max(0.0_r, surface - g);
                }
                mark_column_dirty(i, k);
            }
        }
        m_heights_dirty = true;
        return;
    }
    if (m_nx == 0) return;
    std::vector<RigidBody3D*> bodies = sim.query_aabb(m_world_bounds, f);
    for (int j = 0; j < m_ny; ++j) {
        for (int k = 0; k < m_nz; ++k) {
            for (int i = 0; i < m_nx; ++i) {
                const V3 p = cell_center(i, j, k);
                bool hit = false;
                for (RigidBody3D* b : bodies) {
                    for (const Collider3D& c : b->colliders()) {
                        if (c.is_sensor || !c.world_aabb.contains(p)) continue;
                        const Transform3D xf = b->transform().compose(c.local_offset);
                        const V3 lp = xf.apply_inverse(p);
                        if (c.shape.type == ShapeType3D::Plane) {
                            hit = vec3::dot(lp, c.shape.plane.normal) <= c.shape.plane.offset;
                        } else if (c.shape.type == ShapeType3D::Heightfield) {
                            hit = c.shape.heightfield && lp.y <= c.shape.heightfield->height_at(lp.x, lp.z);
                        } else {
                            hit = detail::fluid_point_in_shape(c.shape, lp);
                        }
                        if (hit) break;
                    }
                    if (hit) break;
                }
                if (hit) set_solid(i, j, k, true);
            }
        }
    }
}

Real FluidVolume3D::cell_ambient(const RigidBody3D& body, RVec3* flow) const {
    if (m_nx == 0) return -1.0e30_r;
    V3 lo{}, hi{};
    if (!local_footprint(body, lo, hi)) return -1.0e30_r;
    const int i0 = static_cast<int>(std::floor(lo.x / m_cs)) - 1, i1 = static_cast<int>(std::floor(hi.x / m_cs)) + 1;
    const int k0 = static_cast<int>(std::floor(lo.z / m_cs)) - 1, k1 = static_cast<int>(std::floor(hi.z / m_cs)) + 1;
    const int j = std::clamp(static_cast<int>(std::floor(0.5_r * (lo.y + hi.y) / m_cs)), 0, m_ny - 1);
    Real sum = 0.0_r;
    V3 vel{};
    int count = 0;
    for (int k = k0; k <= k1; ++k) {
        for (int i = i0; i <= i1; ++i) {
            if (i != i0 && i != i1 && k != k0 && k != k1) continue;
            if (i < 0 || k < 0 || i >= m_nx || k >= m_nz) continue;
            if (m_solid[cell_index(i, j, k)]) continue;
            const Real s = cell_surface_local(i, j, k, 0.0_r);
            if (s < -1.0e5_r) continue;
            sum += s;
            const int top = std::clamp(std::min(j, static_cast<int>(std::floor(s / m_cs - 1e-3_r))), 0, m_ny - 1);
            vel = vel + m_cell_vel[cell_index(i, top, k)];
            ++count;
        }
    }
    if (count == 0) return -1.0e30_r;
    if (flow) *flow = m_def.current + world_dir(vel * (1.0_r / static_cast<Real>(count)));
    return sum / static_cast<Real>(count);
}

Real FluidVolume3D::cell_surface_local(int i, int j, int k, Real lh) const noexcept {
    static_cast<void>(lh);
    if (m_nx == 0) return kNoWater;
    const bool blocky = m_def.level == FluidLevel::blocky;
    const Real cap = blocky ? 1.0_r : std::max(m_def.flowing.max_mass, 1e-6_r);
    auto has = [&](int t) {
        const std::size_t c = cell_index(i, t, k);
        return blocky ? m_block[c] > 0 : m_fill[c] > 1e-4_r;
    };
    auto top_of = [&](int t) {
        while (t + 1 < m_ny && has(t + 1)) ++t;
        const std::size_t c = cell_index(i, t, k);
        const Real f = blocky ? static_cast<Real>(m_block[c]) / 8.0_r : std::min(m_fill[c] / cap, 1.0_r);
        return (static_cast<Real>(t) + f) * m_cs;
    };
    if (has(j)) return top_of(j);
    for (int t = j - 1; t >= 0; --t) {
        if (has(t)) return top_of(t);
        if (m_solid[cell_index(i, t, k)]) break;
    }
    return kNoWater;
}

void FluidVolume3D::refresh_chunks() {
    const std::size_t chunks = m_chunk_active.size();
    if (m_all_dirty || !m_def.allow_sleep) {
        std::fill(m_chunk_active.begin(), m_chunk_active.end(), std::uint8_t{ 1 });
        m_all_dirty = false;
    } else {
        std::fill(m_chunk_active.begin(), m_chunk_active.end(), std::uint8_t{ 0 });
        for (int cy = 0; cy < m_ky; ++cy) {
            for (int cz = 0; cz < m_kz; ++cz) {
                for (int cx = 0; cx < m_kx; ++cx) {
                    const std::size_t ch = (static_cast<std::size_t>(cy) * static_cast<std::size_t>(m_kz) + static_cast<std::size_t>(cz)) * static_cast<std::size_t>(m_kx) + static_cast<std::size_t>(cx);
                    if (!m_chunk_changed[ch]) continue;
                    for (int dy = -1; dy <= 1; ++dy) {
                        for (int dz = -1; dz <= 1; ++dz) {
                            for (int dx = -1; dx <= 1; ++dx) {
                                const int x = cx + dx, y = cy + dy, z = cz + dz;
                                if (x < 0 || y < 0 || z < 0 || x >= m_kx || y >= m_ky || z >= m_kz) continue;
                                m_chunk_active[(static_cast<std::size_t>(y) * static_cast<std::size_t>(m_kz) + static_cast<std::size_t>(z)) * static_cast<std::size_t>(m_kx) + static_cast<std::size_t>(x)] = 1;
                            }
                        }
                    }
                }
            }
        }
    }
    m_active_list.clear();
    for (std::size_t ch = 0; ch < chunks; ++ch) {
        if (m_chunk_active[ch]) m_active_list.push_back(static_cast<std::uint32_t>(ch));
    }
    std::fill(m_chunk_changed.begin(), m_chunk_changed.end(), std::uint8_t{ 0 });
}

int FluidVolume3D::blocky_reach() const noexcept {
    if (m_def.blocky.spread > 0) return std::min(m_def.blocky.spread, 8);
    const Real f = m_def.material.viscosity_factor();
    return std::clamp(static_cast<int>(7.0_r / std::sqrt(f) + 0.5_r), 2, 7);
}

Real FluidVolume3D::blocky_tick_dt() const noexcept {
    Real ticks = static_cast<Real>(std::max(1, m_def.blocky.tick_interval));
    if (m_def.blocky.viscosity_slows_ticks) ticks *= m_def.material.viscosity_factor();
    return std::max(1e-4_r, m_def.blocky.game_tick * ticks);
}

bool FluidVolume3D::blocky_supported(int i, int j, int k) const noexcept {
    if (j == 0) return true;
    const std::size_t b = cell_index(i, j - 1, k);
    return m_solid[b] != 0 || m_source[b] != 0;
}

void FluidVolume3D::step_blocky(detail::FluidTaskPool& pool, Real dt, FluidStats3D& stats) {
    m_accum += dt;
    const Real tick = blocky_tick_dt();
    int done = 0;
    const int budget = std::max(1, m_quality.max_ticks_per_frame);
    while (m_accum >= tick && done < budget) {
        m_accum -= tick;
        blocky_tick(pool);
        ++done;
    }
    if (done == budget && m_accum > tick) m_accum = std::fmod(m_accum, tick);
    stats.ticks += static_cast<std::size_t>(done);
}

void FluidVolume3D::blocky_tick(detail::FluidTaskPool& pool) {
    refresh_chunks();
    if (m_active_list.empty()) return;
    const int reach = blocky_reach();
    const int drop = std::max(1, 7 / reach);
    const int search = std::max(0, m_def.blocky.hole_search);
    const int di[4] = { 1, -1, 0, 0 };
    const int dk[4] = { 0, 0, 1, -1 };
    auto chunk_range = [&](std::uint32_t ch, int& i0, int& i1, int& j0, int& j1, int& k0, int& k1) {
        const int cx = static_cast<int>(ch % static_cast<std::uint32_t>(m_kx));
        const int rest = static_cast<int>(ch / static_cast<std::uint32_t>(m_kx));
        const int cz = rest % m_kz;
        const int cy = rest / m_kz;
        i0 = cx * kChunk; i1 = std::min(m_nx, i0 + kChunk);
        j0 = cy * kChunk; j1 = std::min(m_ny, j0 + kChunk);
        k0 = cz * kChunk; k1 = std::min(m_nz, k0 + kChunk);
    };
    auto open_below = [&](int i, int j, int k) {
        if (j == 0) return false;
        const std::size_t b = cell_index(i, j - 1, k);
        return m_solid[b] == 0 && m_source[b] == 0;
    };
    pool.parallel_for(m_active_list.size(), [&](std::size_t n) {
        int i0, i1, j0, j1, k0, k1;
        chunk_range(m_active_list[n], i0, i1, j0, j1, k0, k1);
        for (int j = j0; j < j1; ++j) {
            for (int k = k0; k < k1; ++k) {
                for (int i = i0; i < i1; ++i) {
                    const std::size_t c = cell_index(i, j, k);
                    std::uint8_t mask = 0;
                    if (m_block[c] > 0 && !m_solid[c] && blocky_supported(i, j, k)) {
                        int best = search + 1;
                        std::uint8_t pref = 0;
                        std::uint8_t open = 0;
                        for (int d = 0; d < 4; ++d) {
                            const int a = i + di[d], b = k + dk[d];
                            if (!valid_cell(a, j, b) || m_solid[cell_index(a, j, b)]) continue;
                            open |= static_cast<std::uint8_t>(1u << d);
                            for (int s = 1; s <= search; ++s) {
                                const int x = i + di[d] * s, z = k + dk[d] * s;
                                if (!valid_cell(x, j, z) || m_solid[cell_index(x, j, z)]) break;
                                if (open_below(x, j, z)) {
                                    if (s < best) {
                                        best = s;
                                        pref = 0;
                                    }
                                    if (s == best) pref |= static_cast<std::uint8_t>(1u << d);
                                    break;
                                }
                            }
                        }
                        mask = pref ? pref : open;
                    }
                    m_spread_mask[c] = mask;
                }
            }
        }
    });
    const bool infinite = m_def.blocky.infinite_sources;
    pool.parallel_for(m_active_list.size(), [&](std::size_t n) {
        int i0, i1, j0, j1, k0, k1;
        chunk_range(m_active_list[n], i0, i1, j0, j1, k0, k1);
        for (int j = j0; j < j1; ++j) {
            for (int k = k0; k < k1; ++k) {
                for (int i = i0; i < i1; ++i) {
                    const std::size_t c = cell_index(i, j, k);
                    m_next[c] = 0.0_r;
                    if (m_solid[c]) {
                        m_block_next[c] = 0;
                        continue;
                    }
                    if (m_source[c]) {
                        m_block_next[c] = 8;
                        continue;
                    }
                    int lvl = 0;
                    if (j + 1 < m_ny) {
                        const std::size_t a = cell_index(i, j + 1, k);
                        if (!m_solid[a] && m_block[a] > 0) lvl = 8;
                    }
                    int sources = 0;
                    if (lvl < 8) {
                        for (int d = 0; d < 4; ++d) {
                            const int a = i - di[d], b = k - dk[d];
                            if (!valid_cell(a, j, b)) continue;
                            const std::size_t nc = cell_index(a, j, b);
                            if (m_solid[nc]) continue;
                            if (m_source[nc]) ++sources;
                            if (!(m_spread_mask[nc] & (1u << d))) continue;
                            const int from = (m_source[nc] || m_block[nc] >= 8) ? 8 : m_block[nc];
                            lvl = std::max(lvl, from - drop);
                        }
                    } else {
                        for (int d = 0; d < 4; ++d) {
                            const int a = i - di[d], b = k - dk[d];
                            if (valid_cell(a, j, b) && m_source[cell_index(a, j, b)]) ++sources;
                        }
                    }
                    if (infinite && sources >= 2 && blocky_supported(i, j, k)) {
                        m_next[c] = 1.0_r;
                        lvl = 8;
                    }
                    m_block_next[c] = static_cast<std::uint8_t>(std::max(0, lvl));
                }
            }
        }
    });
    for (std::uint32_t ch : m_active_list) {
        int i0, i1, j0, j1, k0, k1;
        chunk_range(ch, i0, i1, j0, j1, k0, k1);
        bool changed = false;
        for (int j = j0; j < j1; ++j) {
            for (int k = k0; k < k1; ++k) {
                for (int i = i0; i < i1; ++i) {
                    const std::size_t c = cell_index(i, j, k);
                    if (m_next[c] > 0.0_r && !m_source[c]) {
                        m_source[c] = 1;
                        changed = true;
                    }
                    if (m_block_next[c] != m_block[c]) {
                        m_block[c] = m_block_next[c];
                        m_fill[c] = static_cast<Real>(m_block[c]) / 8.0_r;
                        changed = true;
                    }
                }
            }
        }
        if (changed) m_chunk_changed[ch] = 1;
    }
}

Real FluidVolume3D::flowing_stable(Real total) const noexcept {
    const Real M = std::max(m_def.flowing.max_mass, 1e-6_r);
    const Real C = std::max(m_def.flowing.compression * m_def.material.stiffness, 0.0_r) * M;
    if (total <= M) return total;
    if (total < 2.0_r * M + C) return (M * M + total * C) / (M + C);
    return (total + C) * 0.5_r;
}

void FluidVolume3D::step_flowing(detail::FluidTaskPool& pool, Real dt, FluidStats3D& stats) {
    m_accum += dt;
    const Real tick = 1.0_r / std::max(1.0_r, m_def.flowing.tick_rate);
    int done = 0;
    const int budget = std::max(1, m_quality.max_ticks_per_frame);
    while (m_accum >= tick && done < budget) {
        m_accum -= tick;
        flowing_tick(pool, tick);
        ++done;
    }
    if (done == budget && m_accum > tick) m_accum = std::fmod(m_accum, tick);
    stats.ticks += static_cast<std::size_t>(done);
}

void FluidVolume3D::begin_push() {
    if (m_def.level == FluidLevel::flowing) {
        for (std::uint32_t c : m_occ_touched) {
            if (m_occ_cell[c] > 0.0_r) mark_cell_dirty(static_cast<int>(c % static_cast<std::uint32_t>(m_nx)), static_cast<int>(c / (static_cast<std::uint32_t>(m_nx) * static_cast<std::uint32_t>(m_nz))), static_cast<int>((c / static_cast<std::uint32_t>(m_nx)) % static_cast<std::uint32_t>(m_nz)));
            m_occ_cell[c] = 0.0_r;
        }
        m_occ_touched.clear();
    } else if (m_def.level == FluidLevel::waves) {
        std::copy(m_occ_col.begin(), m_occ_col.end(), m_occ_prev.begin());
        for (std::uint32_t c : m_col_touched) m_occ_col[c] = 0.0_r;
        m_col_touched.clear();
    }
}

void FluidVolume3D::apply_cell_displacement() {
    if (m_occ_touched.empty()) return;
    const Real M = std::max(m_def.flowing.max_mass, 1e-6_r);
    for (std::uint32_t c : m_occ_touched) {
        const Real occ = std::min(m_occ_cell[c], 1.0_r);
        const Real cap = M * (1.0_r - occ);
        Real excess = m_fill[c] - cap;
        if (excess <= 1e-6_r) continue;
        const int i = static_cast<int>(c % static_cast<std::uint32_t>(m_nx));
        const int k = static_cast<int>((c / static_cast<std::uint32_t>(m_nx)) % static_cast<std::uint32_t>(m_nz));
        const int j = static_cast<int>(c / (static_cast<std::uint32_t>(m_nx) * static_cast<std::uint32_t>(m_nz)));
        m_fill[c] -= excess;
        const int di[4] = { 1, -1, 0, 0 };
        const int dk[4] = { 0, 0, 1, -1 };
        for (int d = 0; d < 4 && excess > 1e-7_r; ++d) {
            const int a = i + di[d], b = k + dk[d];
            if (!valid_cell(a, j, b)) continue;
            const std::size_t o = cell_index(a, j, b);
            if (m_solid[o]) continue;
            const Real room = std::max(0.0_r, M * (1.0_r - std::min(m_occ_cell[o], 1.0_r)) - m_fill[o]);
            const Real put = std::min(room, excess);
            m_fill[o] += put;
            excess -= put;
            mark_cell_dirty(a, j, b);
        }
        for (int t = j + 1; t < m_ny && excess > 1e-7_r; ++t) {
            const std::size_t u = cell_index(i, t, k);
            if (m_solid[u]) break;
            const Real room = std::max(0.0_r, M * (1.0_r - std::min(m_occ_cell[u], 1.0_r)) - m_fill[u]);
            const Real put = std::min(room, excess);
            m_fill[u] += put;
            excess -= put;
            mark_cell_dirty(i, t, k);
        }
        m_fill[c] += excess;
        mark_cell_dirty(i, j, k);
    }
}

void FluidVolume3D::flowing_tick(detail::FluidTaskPool& pool, Real tick_dt) {
    apply_cell_displacement();
    refresh_chunks();
    if (m_active_list.empty()) return;
    const FluidFlowingSettings& fs = m_def.flowing;
    const Real M = std::max(fs.max_mass, 1e-6_r);
    const Real fluid = m_def.material.fluidity();
    const Real rh = std::clamp(fs.horizontal_rate * fluid, 0.0_r, 0.24_r);
    const Real rv = std::clamp(fs.vertical_rate * std::sqrt(fluid), 0.0_r, 1.0_r);
    const Real min_flow = fs.min_flow;
    const std::size_t nx = static_cast<std::size_t>(m_nx), nxz = nx * static_cast<std::size_t>(m_nz);
    auto chunk_range = [&](std::uint32_t ch, int& i0, int& i1, int& j0, int& j1, int& k0, int& k1) {
        const int cx = static_cast<int>(ch % static_cast<std::uint32_t>(m_kx));
        const int rest = static_cast<int>(ch / static_cast<std::uint32_t>(m_kx));
        const int cz = rest % m_kz;
        const int cy = rest / m_kz;
        i0 = cx * kChunk; i1 = std::min(m_nx, i0 + kChunk);
        j0 = cy * kChunk; j1 = std::min(m_ny, j0 + kChunk);
        k0 = cz * kChunk; k1 = std::min(m_nz, k0 + kChunk);
    };
    auto eff = [&](std::size_t c) { return m_fill[c] + std::min(m_occ_cell[c], 1.0_r) * M; };
    auto supported = [&](int i, int j, int k) {
        if (j == 0) return true;
        const std::size_t b = cell_index(i, j - 1, k);
        return m_solid[b] != 0 || eff(b) >= 0.95_r * M;
    };
    const std::size_t columns = static_cast<std::size_t>(m_nx) * static_cast<std::size_t>(m_nz);
    pool.parallel_range(columns, 64, [&](std::size_t b, std::size_t e) {
        for (std::size_t col = b; col < e; ++col) {
            const int i = static_cast<int>(col % static_cast<std::size_t>(m_nx));
            const int k = static_cast<int>(col / static_cast<std::size_t>(m_nx));
            bool any = false;
            for (int cy = 0; cy < m_ky && !any; ++cy) any = m_chunk_active[chunk_of(i, cy * kChunk, k)] != 0;
            if (!any) continue;
            Real surface = -1.0_r;
            bool ceiling = false;
            for (int j = m_ny - 1; j >= 0; --j) {
                const std::size_t c = cell_index(i, j, k);
                m_scale[c] = 0.0_r;
                if (m_solid[c]) {
                    m_next[c] = 0.0_r;
                    surface = -1.0_r;
                    ceiling = true;
                    continue;
                }
                const Real e = eff(c);
                const Real own = (static_cast<Real>(j) + std::min(e / M, 1.0_r)) * m_cs;
                if (e <= 1e-4_r) {
                    m_next[c] = own;
                    surface = -1.0_r;
                    ceiling = false;
                } else if (ceiling && e >= 0.99_r * M) {
                    m_scale[c] = 1.0_r;
                    m_next[c] = (static_cast<Real>(j) + 1.0_r) * m_cs;
                } else if (surface < 0.0_r) {
                    m_next[c] = own;
                    surface = own;
                    ceiling = false;
                } else {
                    m_next[c] = std::max(surface, own);
                }
            }
        }
    });
    const Real head_k = rh * M / m_cs;
    const Real max_h = 0.1_r * M;
    m_confined.clear();
    for (std::uint32_t ch : m_active_list) {
        int i0, i1, j0, j1, k0, k1;
        chunk_range(ch, i0, i1, j0, j1, k0, k1);
        for (int j = j0; j < j1; ++j) {
            for (int k = k0; k < k1; ++k) {
                for (int i = i0; i < i1; ++i) {
                    const std::size_t c = cell_index(i, j, k);
                    if (m_scale[c] > 0.0_r) m_confined.push_back(static_cast<std::uint32_t>(c));
                }
            }
        }
    }
    if (!m_confined.empty()) {
        const std::ptrdiff_t steps[6] = { 1, -1, static_cast<std::ptrdiff_t>(nx), -static_cast<std::ptrdiff_t>(nx), static_cast<std::ptrdiff_t>(nxz), -static_cast<std::ptrdiff_t>(nxz) };
        auto neighbour = [&](std::size_t c, int d, std::size_t& o) {
            const int i = static_cast<int>(c % nx), k = static_cast<int>((c / nx) % static_cast<std::size_t>(m_nz)), j = static_cast<int>(c / nxz);
            const int ni = i + (d == 0 ? 1 : d == 1 ? -1 : 0), nk = k + (d == 2 ? 1 : d == 3 ? -1 : 0), nj = j + (d == 4 ? 1 : d == 5 ? -1 : 0);
            if (!valid_cell(ni, nj, nk)) return false;
            o = static_cast<std::size_t>(static_cast<std::ptrdiff_t>(c) + steps[d]);
            return !m_solid[o] && eff(o) > 1e-4_r && m_chunk_active[chunk_of(ni, nj, nk)];
        };
        const int sweeps = 24;
        for (int it = 0; it < sweeps; ++it) {
            for (std::uint32_t c : m_confined) {
                Real sum = 0.0_r;
                int count = 0;
                for (int d = 0; d < 6; ++d) {
                    std::size_t o = 0;
                    if (!neighbour(c, d, o)) continue;
                    sum += m_next[o];
                    ++count;
                }
                if (count > 0) m_next[c] = sum / static_cast<Real>(count);
            }
        }
        for (std::uint32_t c : m_confined) m_next[c] += 0.05_r * (eff(c) - M) / std::max(head_k, 1e-6_r);
    }
    auto horizontal = [&](std::size_t c, std::size_t o, int i, int j, int k, int oi, int ok) {
        const Real mc = m_fill[c], mo = m_fill[o];
        if (mc <= 0.0_r && mo <= 0.0_r) return 0.0_r;
        Real flow = 0.0_r;
        if (m_scale[c] > 0.0_r || m_scale[o] > 0.0_r) flow = std::clamp((m_next[c] - m_next[o]) * head_k * 0.5_r, -max_h, max_h);
        else flow = (eff(c) - eff(o)) * rh;
        const bool donor_ok = flow > 0.0_r ? supported(i, j, k) : supported(oi, j, ok);
        if (!donor_ok) flow *= 0.25_r;
        flow = std::clamp(flow, -mo, mc);
        return std::abs(flow) >= min_flow ? flow : 0.0_r;
    };
    pool.parallel_for(m_active_list.size(), [&](std::size_t n) {
        int i0, i1, j0, j1, k0, k1;
        chunk_range(m_active_list[n], i0, i1, j0, j1, k0, k1);
        for (int j = j0; j < j1; ++j) {
            for (int k = k0; k < k1; ++k) {
                for (int i = i0; i < i1; ++i) {
                    const std::size_t c = cell_index(i, j, k);
                    Real* f = &m_flux[c * 3];
                    f[0] = f[1] = f[2] = 0.0_r;
                    if (m_solid[c]) continue;
                    const Real mc = m_fill[c];
                    if (i + 1 < m_nx) {
                        const std::size_t o = c + 1;
                        if (!m_solid[o] && m_chunk_active[chunk_of(i + 1, j, k)]) f[0] = horizontal(c, o, i, j, k, i + 1, k);
                    }
                    if (j + 1 < m_ny) {
                        const std::size_t o = c + nxz;
                        if (!m_solid[o] && m_chunk_active[chunk_of(i, j + 1, k)]) {
                            const Real mo = m_fill[o];
                            if (mc > 0.0_r || mo > 0.0_r) {
                                const Real ec = eff(c), eo = eff(o);
                                const Real total = ec + eo;
                                const Real lower = flowing_stable(total);
                                Real flow = (ec - lower) * rv;
                                flow = std::clamp(flow, -mo, mc);
                                if (std::abs(flow) >= min_flow) f[1] = flow;
                            }
                        }
                    }
                    if (k + 1 < m_nz) {
                        const std::size_t o = c + nx;
                        if (!m_solid[o] && m_chunk_active[chunk_of(i, j, k + 1)]) f[2] = horizontal(c, o, i, j, k, i, k + 1);
                    }
                }
            }
        }
    });
    auto face_in = [&](int i, int j, int k, int axis) -> Real {
        int a = i, b = j, d = k;
        if (axis == 0) --a;
        else if (axis == 1) --b;
        else --d;
        if (a < 0 || b < 0 || d < 0) return 0.0_r;
        if (!m_chunk_active[chunk_of(a, b, d)]) return 0.0_r;
        return m_flux[cell_index(a, b, d) * 3 + static_cast<std::size_t>(axis)];
    };
    pool.parallel_for(m_active_list.size(), [&](std::size_t n) {
        int i0, i1, j0, j1, k0, k1;
        chunk_range(m_active_list[n], i0, i1, j0, j1, k0, k1);
        for (int j = j0; j < j1; ++j) {
            for (int k = k0; k < k1; ++k) {
                for (int i = i0; i < i1; ++i) {
                    const std::size_t c = cell_index(i, j, k);
                    Real out = 0.0_r;
                    for (int axis = 0; axis < 3; ++axis) {
                        out += std::max(m_flux[c * 3 + static_cast<std::size_t>(axis)], 0.0_r);
                        out += std::max(-face_in(i, j, k, axis), 0.0_r);
                    }
                    m_scale[c] = (out > m_fill[c] && out > 0.0_r) ? m_fill[c] / out : 1.0_r;
                }
            }
        }
    });
    const Real sleep = m_def.sleep_threshold;
    const Real vel_k = m_cs / std::max(tick_dt, 1e-6_r);
    pool.parallel_for(m_active_list.size(), [&](std::size_t n) {
        const std::uint32_t ch = m_active_list[n];
        int i0, i1, j0, j1, k0, k1;
        chunk_range(ch, i0, i1, j0, j1, k0, k1);
        bool changed = false;
        for (int j = j0; j < j1; ++j) {
            for (int k = k0; k < k1; ++k) {
                for (int i = i0; i < i1; ++i) {
                    const std::size_t c = cell_index(i, j, k);
                    if (m_solid[c]) {
                        m_next[c] = 0.0_r;
                        continue;
                    }
                    Real delta = 0.0_r;
                    Real flow[3] = { 0.0_r, 0.0_r, 0.0_r };
                    for (int axis = 0; axis < 3; ++axis) {
                        const std::size_t step = axis == 0 ? 1 : (axis == 1 ? nxz : nx);
                        const Real fo = m_flux[c * 3 + static_cast<std::size_t>(axis)];
                        if (fo != 0.0_r) {
                            const Real s = fo > 0.0_r ? m_scale[c] : m_scale[c + step];
                            delta -= fo * s;
                            flow[axis] += 0.5_r * fo * s;
                        }
                        const Real fi = face_in(i, j, k, axis);
                        if (fi != 0.0_r) {
                            const Real s = fi > 0.0_r ? m_scale[c - step] : m_scale[c];
                            delta += fi * s;
                            flow[axis] += 0.5_r * fi * s;
                        }
                    }
                    m_next[c] = m_fill[c] + delta;
                    const Real denom = std::max(m_fill[c], 0.1_r * M);
                    m_cell_vel[c] = V3{ flow[0], flow[1], flow[2] } * (vel_k / denom);
                    if (std::abs(delta) > sleep) changed = true;
                }
            }
        }
        if (changed) m_chunk_changed[ch] = 1;
    });
    const bool keep = fs.keep_sources_full;
    for (std::uint32_t ch : m_active_list) {
        int i0, i1, j0, j1, k0, k1;
        chunk_range(ch, i0, i1, j0, j1, k0, k1);
        for (int j = j0; j < j1; ++j) {
            for (int k = k0; k < k1; ++k) {
                for (int i = i0; i < i1; ++i) {
                    const std::size_t c = cell_index(i, j, k);
                    Real v = std::max(0.0_r, m_next[c]);
                    if (keep && m_source[c]) v = std::max(v, M);
                    m_fill[c] = v;
                }
            }
        }
    }
}

void FluidVolume3D::push_fluid(const RigidBody3D& body, const BodyCache& cache, Real dt) {
    static_cast<void>(dt);
    const Real scale = m_def.coupling.push_scale;
    if (scale <= 0.0_r) return;
    const Transform3D& xf = body.transform();
    const auto& colliders = body.colliders();
    if (m_def.level == FluidLevel::flowing) {
        const Real inv_cell = 1.0_r / (m_cs * m_cs * m_cs);
        auto add = [&](const V3& p, Real w) {
            int i = 0, j = 0, k = 0;
            if (!cell_of(p, i, j, k)) return;
            const std::size_t c = cell_index(i, j, k);
            if (m_occ_cell[c] == 0.0_r) m_occ_touched.push_back(static_cast<std::uint32_t>(c));
            m_occ_cell[c] += w * inv_cell * scale;
            mark_cell_dirty(i, j, k);
        };
        for (std::size_t ci = 0; ci < colliders.size() && ci + 1 < cache.collider_start.size(); ++ci) {
            const Collider3D& c = colliders[ci];
            if (c.is_sensor || c.shape.is_static_only()) continue;
            if (c.shape.type == ShapeType3D::Sphere) {
                const Transform3D cx = xf.compose(c.local_offset);
                const V3 center = cx.apply(c.shape.sphere.center);
                const Real r = c.shape.sphere.radius;
                const int n = std::max(1, static_cast<int>(std::ceil(2.0_r * r / m_cs)));
                const Real s = 2.0_r * r / static_cast<Real>(n);
                for (int z = 0; z < n; ++z) {
                    for (int y = 0; y < n; ++y) {
                        for (int x = 0; x < n; ++x) {
                            const V3 o{ (static_cast<Real>(x) + 0.5_r) * s - r, (static_cast<Real>(y) + 0.5_r) * s - r, (static_cast<Real>(z) + 0.5_r) * s - r };
                            if (vec3::dot(o, o) > r * r) continue;
                            add(center + o, s * s * s);
                        }
                    }
                }
                continue;
            }
            for (std::uint32_t q = cache.collider_start[ci]; q < cache.collider_start[ci + 1]; ++q) add(xf.apply(cache.points[q].local), cache.points[q].weight);
        }
        return;
    }
    if (m_def.level != FluidLevel::waves) return;
    const Real inv_area = scale / (m_dx * m_dx);
    auto ambient = [&](Real u0, Real w0, Real u1, Real w1) {
        const int i0 = std::max(0, static_cast<int>(std::floor(u0 / m_dx)) - 1);
        const int i1 = std::min(m_cx - 1, static_cast<int>(std::floor(u1 / m_dx)) + 1);
        const int k0 = std::max(0, static_cast<int>(std::floor(w0 / m_dx)) - 1);
        const int k1 = std::min(m_cz - 1, static_cast<int>(std::floor(w1 / m_dx)) + 1);
        Real sum = 0.0_r;
        int count = 0;
        auto take = [&](int i, int k) {
            const std::size_t c = column_index(i, k);
            if (m_wall[c]) return;
            sum += m_ground[c] + m_depth[c] + m_occ_prev[c];
            ++count;
        };
        for (int i = i0; i <= i1; ++i) {
            take(i, k0);
            if (k1 != k0) take(i, k1);
        }
        for (int k = k0 + 1; k < k1; ++k) {
            take(i0, k);
            if (i1 != i0) take(i1, k);
        }
        return count > 0 ? sum / static_cast<Real>(count) : 0.0_r;
    };
    auto splat = [&](Real lu, Real lw, Real amount) {
        const Real fu = lu / m_dx - 0.5_r, fw = lw / m_dx - 0.5_r;
        const int i0 = static_cast<int>(std::floor(fu)), k0 = static_cast<int>(std::floor(fw));
        const Real tu = fu - static_cast<Real>(i0), tw = fw - static_cast<Real>(k0);
        const Real wts[4] = { (1.0_r - tu) * (1.0_r - tw), tu * (1.0_r - tw), (1.0_r - tu) * tw, tu * tw };
        const int ii[4] = { i0, i0 + 1, i0, i0 + 1 };
        const int kk[4] = { k0, k0, k0 + 1, k0 + 1 };
        for (int q = 0; q < 4; ++q) {
            const int i = std::clamp(ii[q], 0, m_cx - 1), k = std::clamp(kk[q], 0, m_cz - 1);
            const std::size_t c = column_index(i, k);
            if (m_wall[c]) continue;
            if (m_occ_col[c] == 0.0_r) m_col_touched.push_back(static_cast<std::uint32_t>(c));
            m_occ_col[c] += amount * wts[q];
            mark_column_dirty(i, k);
        }
    };
    for (std::size_t ci = 0; ci < colliders.size() && ci + 1 < cache.collider_start.size(); ++ci) {
        const Collider3D& c = colliders[ci];
        if (c.is_sensor || c.shape.is_static_only()) continue;
        if (c.shape.type == ShapeType3D::Sphere) {
            const Transform3D cx = xf.compose(c.local_offset);
            const V3 lc = to_local(cx.apply(c.shape.sphere.center));
            const Real r = c.shape.sphere.radius;
            if (lc.x + r < 0.0_r || lc.z + r < 0.0_r || lc.x - r > m_size.x || lc.z - r > m_size.z) continue;
            const Real head = ambient(lc.x - r, lc.z - r, lc.x + r, lc.z + r);
            if (lc.y - r >= head) continue;
            const int i0 = std::max(0, static_cast<int>(std::floor((lc.x - r) / m_dx)));
            const int i1 = std::min(m_cx - 1, static_cast<int>(std::floor((lc.x + r) / m_dx)));
            const int k0 = std::max(0, static_cast<int>(std::floor((lc.z - r) / m_dx)));
            const int k1 = std::min(m_cz - 1, static_cast<int>(std::floor((lc.z + r) / m_dx)));
            for (int k = k0; k <= k1; ++k) {
                for (int i = i0; i <= i1; ++i) {
                    const std::size_t col = column_index(i, k);
                    if (m_wall[col]) continue;
                    const Real du = (static_cast<Real>(i) + 0.5_r) * m_dx - lc.x;
                    const Real dw = (static_cast<Real>(k) + 0.5_r) * m_dx - lc.z;
                    const Real rr = r * r - du * du - dw * dw;
                    if (rr <= 0.0_r) continue;
                    const Real half = std::sqrt(rr);
                    const Real lo = std::max(lc.y - half, m_ground[col]);
                    const Real hi = std::min(lc.y + half, head);
                    if (hi <= lo) continue;
                    if (m_occ_col[col] == 0.0_r) m_col_touched.push_back(static_cast<std::uint32_t>(col));
                    m_occ_col[col] += (hi - lo) * scale;
                    mark_column_dirty(i, k);
                }
            }
            continue;
        }
        const std::uint32_t q0 = cache.collider_start[ci], q1 = cache.collider_start[ci + 1];
        if (q0 == q1) continue;
        Real u0 = std::numeric_limits<Real>::max(), w0 = u0, u1 = -u0, w1 = -u0;
        for (std::uint32_t q = q0; q < q1; ++q) {
            const V3 l = to_local(xf.apply(cache.points[q].local));
            u0 = std::min(u0, l.x);
            u1 = std::max(u1, l.x);
            w0 = std::min(w0, l.z);
            w1 = std::max(w1, l.z);
        }
        const Real s = cache.spacing;
        if (u1 < 0.0_r || w1 < 0.0_r || u0 > m_size.x || w0 > m_size.z) continue;
        const Real head = ambient(u0 - 0.5_r * s, w0 - 0.5_r * s, u1 + 0.5_r * s, w1 + 0.5_r * s);
        for (std::uint32_t q = q0; q < q1; ++q) {
            const V3 l = to_local(xf.apply(cache.points[q].local));
            if (l.x < 0.0_r || l.z < 0.0_r || l.x > m_size.x || l.z > m_size.z) continue;
            const Real frac = std::clamp((head - l.y) / s + 0.5_r, 0.0_r, 1.0_r);
            if (frac <= 0.0_r) continue;
            splat(l.x, l.z, cache.points[q].weight * frac * inv_area);
        }
    }
}

} // namespace physics
} // namespace fizmo
