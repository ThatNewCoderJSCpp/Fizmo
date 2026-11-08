#include "fizmo_library.hpp"
#include "fluid_2d.hpp"

namespace fizmo {
namespace physics {

void FluidVolume2D::init_flowing() {
    const std::size_t n = static_cast<std::size_t>(m_nx) * static_cast<std::size_t>(m_ny);
    m_fill.assign(n, 0.0_r);
    m_next.assign(n, 0.0_r);
    m_source.assign(n, 0);
    m_cell_vel.assign(n, RVec2{});
    m_flow_rem.assign(n, 0.0_r);
    m_flow_scale.assign(n, 1.0_r);
    m_flow_hflux.assign(static_cast<std::size_t>(m_nx + 1) * static_cast<std::size_t>(m_ny), 0.0_r);
    m_fkx = (m_nx + kFlowChunk - 1) / kFlowChunk;
    m_fky = (m_ny + kFlowChunk - 1) / kFlowChunk;
    const std::size_t chunks = static_cast<std::size_t>(m_fkx) * static_cast<std::size_t>(m_fky);
    m_flow_changed.assign(chunks, 1);
    m_flow_active.assign(chunks, 1);
    m_flow_list.clear();
    m_flow_list.reserve(chunks);
    if (m_def.fill > 0.0_r) {
        const Real level = std::clamp(m_def.fill, 0.0_r, 1.0_r) * static_cast<Real>(m_ny);
        for (int j = 0; j < m_ny; ++j) {
            const Real a = std::clamp(level - static_cast<Real>(j), 0.0_r, 1.0_r) * m_def.flowing.max_mass;
            for (int i = 0; i < m_nx; ++i) m_fill[cell_index(i, j)] = a;
        }
    }
}

Real FluidVolume2D::flowing_stable(Real total) const noexcept {
    const Real M = std::max(m_def.flowing.max_mass, 1e-6_r);
    const Real C = std::max(m_def.flowing.compression * m_materials[0].stiffness, 0.0_r) * M;
    if (total <= M) return total;
    if (total < 2.0_r * M + C) return (M * M + total * C) / (M + C);
    return (total + C) * 0.5_r;
}

void FluidVolume2D::mark_flow_dirty(int i, int j) noexcept {
    if (m_flow_changed.empty() || !valid_cell(i, j)) return;
    m_flow_changed[static_cast<std::size_t>((j / kFlowChunk) * m_fkx + i / kFlowChunk)] = 1;
}

void FluidVolume2D::flowing_displace() {
    if (m_occ_list.empty() || !m_def.coupling.push_fluid) return;
    std::vector<std::uint32_t>& queue = m_tmp_u32;
    std::vector<std::uint8_t>& seen = m_tmp_u8;
    if (seen.size() != m_fill.size()) seen.assign(m_fill.size(), 0);
    const Real M = m_def.flowing.max_mass;
    for (std::uint32_t oc : m_occ_list) {
        Real amount = m_fill[oc];
        if (amount <= m_def.flowing.min_flow) continue;
        m_fill[oc] = 0.0_r;
        queue.clear();
        queue.push_back(oc);
        seen[oc] = 1;
        std::size_t head = 0;
        while (head < queue.size() && queue.size() < 512 && amount > 0.0_r) {
            const std::uint32_t c = queue[head++];
            const int x = static_cast<int>(c % static_cast<std::uint32_t>(m_nx));
            const int y = static_cast<int>(c / static_cast<std::uint32_t>(m_nx));
            const int dxs[4] = { 0, -1, 1, 0 };
            const int dys[4] = { 1, 0, 0, -1 };
            for (int d = 0; d < 4 && amount > 0.0_r; ++d) {
                const int ax = x + dxs[d], ay = y + dys[d];
                if (!valid_cell(ax, ay)) continue;
                const std::uint32_t a = static_cast<std::uint32_t>(cell_index(ax, ay));
                if (seen[a] || m_solid[a] || m_occ[a]) continue;
                seen[a] = 1;
                queue.push_back(a);
                const Real room = M - m_fill[a];
                if (room <= 0.0_r) continue;
                const Real take = std::min(room, amount);
                m_fill[a] += take;
                amount -= take;
                mark_flow_dirty(ax, ay);
            }
        }
        for (std::uint32_t c : queue) seen[c] = 0;
        if (amount > 0.0_r) m_fill[oc] = amount;
        mark_flow_dirty(static_cast<int>(oc % static_cast<std::uint32_t>(m_nx)), static_cast<int>(oc / static_cast<std::uint32_t>(m_nx)));
    }
}

void FluidVolume2D::flowing_tick() {
    const std::size_t chunks = m_flow_active.size();
    m_flow_list.clear();
    m_flow_ready.assign(chunks, 0);
    for (int cy = 0; cy < m_fky; ++cy) {
        for (int cx = 0; cx < m_fkx; ++cx) {
            bool on = false;
            for (int y = std::max(0, cy - 1); y <= std::min(m_fky - 1, cy + 1) && !on; ++y) {
                for (int x = std::max(0, cx - 1); x <= std::min(m_fkx - 1, cx + 1) && !on; ++x) on = m_flow_changed[static_cast<std::size_t>(y * m_fkx + x)] != 0;
            }
            const std::size_t k = static_cast<std::size_t>(cy * m_fkx + cx);
            if (!on && m_flow_active[k]) {
                for (int j = cy * kFlowChunk; j < std::min(m_ny, (cy + 1) * kFlowChunk); ++j) {
                    for (int i = cx * kFlowChunk; i <= std::min(m_nx, (cx + 1) * kFlowChunk); ++i) m_flow_hflux[static_cast<std::size_t>(j * (m_nx + 1) + i)] = 0.0_r;
                }
            }
            m_flow_active[k] = on ? 1 : 0;
            if (!on) continue;
            m_flow_list.push_back(static_cast<std::uint32_t>(k));
            for (int y = std::max(0, cy - 1); y <= std::min(m_fky - 1, cy + 1); ++y) {
                for (int x = std::max(0, cx - 1); x <= std::min(m_fkx - 1, cx + 1); ++x) m_flow_ready[static_cast<std::size_t>(y * m_fkx + x)] = 1;
            }
        }
    }
    std::fill(m_flow_changed.begin(), m_flow_changed.end(), std::uint8_t{ 0 });
    if (m_flow_list.empty()) return;
    const FluidFlowingSettings& fs = m_def.flowing;
    const Real visc = m_materials[0].viscosity_factor();
    const Real hr = std::clamp(fs.horizontal_rate / visc, 0.0_r, 0.5_r);
    const Real vr = std::clamp(fs.vertical_rate / std::sqrt(visc), 0.0_r, 1.0_r);
    const Real keep = std::clamp(fs.momentum, 0.0_r, 0.98_r) / std::sqrt(visc);
    const Real min_flow = std::max(fs.min_flow, 1e-7_r);
    const std::size_t su = static_cast<std::size_t>(m_nx + 1);
    auto blocked = [&](int i, int j) { return !valid_cell(i, j) || m_solid[cell_index(i, j)] || m_occ[cell_index(i, j)]; };
    auto chunk_cells = [&](std::size_t k, auto&& fn) {
        const int cx = static_cast<int>(k % static_cast<std::size_t>(m_fkx)), cy = static_cast<int>(k / static_cast<std::size_t>(m_fkx));
        for (int j = cy * kFlowChunk; j < std::min(m_ny, (cy + 1) * kFlowChunk); ++j) {
            for (int i = cx * kFlowChunk; i < std::min(m_nx, (cx + 1) * kFlowChunk); ++i) fn(i, j, cell_index(i, j));
        }
    };
    auto on_cells = [&](auto&& fn) {
        for (std::uint32_t k : m_flow_list) chunk_cells(k, fn);
    };
    auto is_on = [&](int i, int j) { return valid_cell(i, j) && m_flow_active[static_cast<std::size_t>((j / kFlowChunk) * m_fkx + i / kFlowChunk)] != 0; };
    for (std::size_t k = 0; k < chunks; ++k) {
        if (!m_flow_ready[k]) continue;
        chunk_cells(k, [&](int, int, std::size_t c) {
            m_next[c] = m_fill[c];
            m_flow_rem[c] = m_fill[c];
            m_flow_scale[c] = 1.0_r;
            m_cell_vel[c] = RVec2{};
        });
    }
    on_cells([&](int i, int j, std::size_t c) {
        if (m_solid[c] || m_occ[c] || blocked(i, j - 1)) return;
        const Real rem = m_flow_rem[c];
        if (rem <= min_flow) return;
        const std::size_t b = cell_index(i, j - 1);
        Real flow = flowing_stable(rem + m_fill[b]) - m_fill[b];
        if (flow <= min_flow) return;
        flow = std::min(flow * vr, rem);
        m_next[c] -= flow;
        m_next[b] += flow;
        m_flow_rem[c] -= flow;
        m_cell_vel[c].y -= flow;
        m_cell_vel[b].y -= flow;
    });
    auto face = [&](int i, int j) {
        const std::size_t f = static_cast<std::size_t>(j) * su + static_cast<std::size_t>(i + 1);
        if (blocked(i, j) || blocked(i + 1, j)) {
            m_flow_hflux[f] = 0.0_r;
            return;
        }
        const Real ra = m_flow_rem[cell_index(i, j)], rb = m_flow_rem[cell_index(i + 1, j)];
        Real q = m_flow_hflux[f] * keep + (ra - rb) * hr;
        if ((q > 0.0_r && ra <= min_flow) || (q < 0.0_r && rb <= min_flow)) q = 0.0_r;
        m_flow_hflux[f] = q;
    };
    on_cells([&](int i, int j, std::size_t) {
        face(i, j);
        if (i > 0 && !is_on(i - 1, j)) face(i - 1, j);
    });
    for (std::size_t k = 0; k < chunks; ++k) {
        if (!m_flow_ready[k]) continue;
        chunk_cells(k, [&](int i, int j, std::size_t c) {
            const std::size_t f = static_cast<std::size_t>(j) * su + static_cast<std::size_t>(i);
            const Real out = std::max(m_flow_hflux[f + 1], 0.0_r) + std::max(-m_flow_hflux[f], 0.0_r);
            const Real rem = std::max(m_flow_rem[c], 0.0_r);
            m_flow_scale[c] = out > rem && out > 0.0_r ? rem / out : 1.0_r;
        });
    }
    auto apply = [&](int i, int j) {
        const std::size_t f = static_cast<std::size_t>(j) * su + static_cast<std::size_t>(i + 1);
        Real q = m_flow_hflux[f];
        if (q == 0.0_r || i + 1 >= m_nx || i < 0) return;
        const std::size_t c = cell_index(i, j), o = cell_index(i + 1, j);
        q *= q > 0.0_r ? m_flow_scale[c] : m_flow_scale[o];
        m_flow_hflux[f] = q;
        m_next[c] -= q;
        m_next[o] += q;
        m_cell_vel[c].x += q;
        m_cell_vel[o].x += q;
    };
    on_cells([&](int i, int j, std::size_t) {
        apply(i, j);
        if (i > 0 && !is_on(i - 1, j)) apply(i - 1, j);
    });
    on_cells([&](int i, int j, std::size_t c) {
        if (m_solid[c] || m_occ[c] || blocked(i, j + 1)) return;
        const std::size_t f = static_cast<std::size_t>(j) * su + static_cast<std::size_t>(i);
        const Real rem = m_flow_rem[c] - std::max(m_flow_hflux[f + 1], 0.0_r) - std::max(-m_flow_hflux[f], 0.0_r);
        if (rem <= min_flow) return;
        const std::size_t a = cell_index(i, j + 1);
        Real up = rem - flowing_stable(rem + m_fill[a]);
        if (up <= min_flow) return;
        up = std::min(up * vr, rem);
        m_next[c] -= up;
        m_next[a] += up;
        m_cell_vel[c].y += up;
        m_cell_vel[a].y += up;
    });
    const Real vscale = 0.5_r * m_cs * std::max(1.0_r, fs.tick_rate);
    const Real vcap = 2.0_r * m_cs * std::max(1.0_r, fs.tick_rate) * 0.25_r;
    for (std::size_t k = 0; k < chunks; ++k) {
        if (!m_flow_ready[k]) continue;
        bool changed = false;
        chunk_cells(k, [&](int, int, std::size_t c) {
            Real v = m_next[c];
            if (m_source[c] && fs.keep_sources_full) v = std::max(v, fs.max_mass);
            if (v < min_flow * 0.01_r) v = 0.0_r;
            if (std::abs(v - m_fill[c]) > m_def.sleep_threshold) changed = true;
            m_fill[c] = v;
            RVec2 cv = m_cell_vel[c] * (vscale / std::max(v, 0.5_r));
            const Real sp = std::sqrt(cv.x * cv.x + cv.y * cv.y);
            if (sp > vcap) cv = cv * (vcap / sp);
            m_cell_vel[c] = v > min_flow ? cv : RVec2{};
        });
        if (changed) m_flow_changed[k] = 1;
    }
}

void FluidVolume2D::step_flowing(detail::FluidTaskPool& pool, Real dt, FluidStats2D& stats) {
    static_cast<void>(pool);
    if (m_fill.empty()) return;
    flowing_displace();
    for (FluidEmitter2D& e : m_emitters) {
        if (!e.active || e.rate <= 0.0_r) continue;
        e.carry += e.rate * dt;
        int i = 0, j = 0;
        if (!cell_of(e.position, i, j)) continue;
        const std::size_t c = cell_index(i, j);
        if (m_solid[c]) continue;
        const Real add = std::min(e.carry, std::max(0.0_r, m_def.flowing.max_mass * 1.5_r - m_fill[c]));
        m_fill[c] += add;
        e.carry -= add;
        e.carry = std::min(e.carry, m_def.flowing.max_mass * 4.0_r);
        mark_flow_dirty(i, j);
    }
    if (m_all_dirty) std::fill(m_flow_changed.begin(), m_flow_changed.end(), std::uint8_t{ 1 });
    m_accum += dt;
    const Real tick = 1.0_r / std::max(1.0_r, m_def.flowing.tick_rate);
    const int budget = std::max(1, m_quality.max_ticks_per_frame);
    int done = 0;
    while (m_accum >= tick * 0.999_r && done < budget) {
        flowing_tick();
        m_accum -= tick;
        ++done;
    }
    if (done == budget) m_accum = std::min(m_accum, tick);
    if (m_accum < 0.0_r) m_accum = 0.0_r;
    stats.ticks += static_cast<std::size_t>(done);
    m_all_dirty = false;
}

} // namespace physics
} // namespace fizmo
