#include "fizmo_library.hpp"
#include "fluid_2d.hpp"

namespace fizmo {
namespace physics {

namespace {

constexpr int kCellCount = static_cast<int>(SandCell2D::count);

SandCellInfo2D make_info(SandKind2D kind, Real density, int dispersion, Real move, Real slide, Real flame, int life_min, int life_max, Real temp, bool emits, Real visc, FluidColor color, bool glows) {
    SandCellInfo2D i;
    i.kind = kind;
    i.density = density;
    i.dispersion = dispersion;
    i.move_chance = move;
    i.slide_chance = slide;
    i.flammability = flame;
    i.life_min = life_min;
    i.life_max = life_max;
    i.base_temperature = temp;
    i.emits_heat = emits;
    i.viscosity = visc;
    i.color = color;
    i.glows = glows;
    return i;
}

const std::array<SandCellInfo2D, kCellCount>& info_table() {
    static const std::array<SandCellInfo2D, kCellCount> table = [] {
        std::array<SandCellInfo2D, kCellCount> t{};
        t[0] = make_info(SandKind2D::empty, 1.2_r, 0, 1.0_r, 1.0_r, 0.0_r, 0, 0, 20.0_r, false, 1.8e-5_r, FluidColor{ 0.0f, 0.0f, 0.0f, 0.0f }, false);
        t[1] = make_info(SandKind2D::solid, 1.0e9_r, 0, 0.0_r, 0.0_r, 0.0_r, 0, 0, 20.0_r, false, 1.0_r, FluidColor{ 0.45f, 0.45f, 0.48f, 1.0f }, false);
        t[2] = make_info(SandKind2D::powder, 1600.0_r, 0, 1.0_r, 1.0_r, 0.0_r, 0, 0, 20.0_r, false, 5.0_r, FluidColor{ 0.86f, 0.74f, 0.45f, 1.0f }, false);
        t[3] = make_info(SandKind2D::liquid, 1000.0_r, 5, 1.0_r, 1.0_r, 0.0_r, 0, 0, 20.0_r, false, 1.0e-3_r, FluidColor{ 0.18f, 0.42f, 0.85f, 0.85f }, false);
        t[4] = make_info(SandKind2D::liquid, 900.0_r, 3, 1.0_r, 1.0_r, 0.35_r, 0, 0, 20.0_r, false, 0.08_r, FluidColor{ 0.42f, 0.33f, 0.1f, 0.92f }, false);
        t[5] = make_info(SandKind2D::liquid, 2600.0_r, 2, 0.3_r, 1.0_r, 0.0_r, 0, 0, 1200.0_r, true, 100.0_r, FluidColor{ 1.0f, 0.36f, 0.05f, 1.0f }, true);
        t[6] = make_info(SandKind2D::gas, 0.6_r, 3, 1.0_r, 1.0_r, 0.0_r, 90, 220, 110.0_r, false, 1.3e-5_r, FluidColor{ 0.85f, 0.88f, 0.93f, 0.5f }, false);
        t[7] = make_info(SandKind2D::gas, 1.0_r, 2, 0.8_r, 1.0_r, 0.0_r, 50, 140, 20.0_r, false, 1.8e-5_r, FluidColor{ 0.25f, 0.25f, 0.26f, 0.6f }, false);
        t[8] = make_info(SandKind2D::fire, 0.3_r, 1, 1.0_r, 1.0_r, 0.0_r, 12, 40, 900.0_r, true, 1.8e-5_r, FluidColor{ 1.0f, 0.55f, 0.1f, 1.0f }, true);
        t[9] = make_info(SandKind2D::solid, 917.0_r, 0, 0.0_r, 0.0_r, 0.0_r, 0, 0, -15.0_r, true, 1.0_r, FluidColor{ 0.7f, 0.88f, 1.0f, 0.9f }, false);
        t[10] = make_info(SandKind2D::powder, 2500.0_r, 0, 1.0_r, 0.0_r, 0.0_r, 0, 0, 20.0_r, false, 8.0_r, FluidColor{ 0.42f, 0.4f, 0.38f, 1.0f }, false);
        t[11] = make_info(SandKind2D::solid, 700.0_r, 0, 0.0_r, 0.0_r, 0.12_r, 0, 0, 20.0_r, false, 1.0_r, FluidColor{ 0.5f, 0.33f, 0.16f, 1.0f }, false);
        t[12] = make_info(SandKind2D::liquid, 1050.0_r, 4, 1.0_r, 1.0_r, 0.0_r, 0, 0, 20.0_r, false, 1.5e-3_r, FluidColor{ 0.45f, 0.95f, 0.2f, 0.85f }, true);
        return t;
    }();
    return table;
}

std::uint32_t hash32(std::uint32_t x) noexcept {
    x ^= x >> 16;
    x *= 0x7feb352du;
    x ^= x >> 15;
    x *= 0x846ca68bu;
    x ^= x >> 16;
    return x;
}

} // namespace

const SandCellInfo2D& sand_cell_info(SandCell2D cell) noexcept {
    const int i = static_cast<int>(cell);
    return info_table()[static_cast<std::size_t>(i >= 0 && i < kCellCount ? i : 0)];
}

const char* to_string(SandCell2D cell) noexcept {
    switch (cell) {
    case SandCell2D::empty: return "empty";
    case SandCell2D::wall: return "wall";
    case SandCell2D::sand: return "sand";
    case SandCell2D::water: return "water";
    case SandCell2D::oil: return "oil";
    case SandCell2D::lava: return "lava";
    case SandCell2D::steam: return "steam";
    case SandCell2D::smoke: return "smoke";
    case SandCell2D::fire: return "fire";
    case SandCell2D::ice: return "ice";
    case SandCell2D::stone: return "stone";
    case SandCell2D::wood: return "wood";
    case SandCell2D::acid: return "acid";
    case SandCell2D::count: return "count";
    }
    return "unknown";
}

const char* to_string(SandKind2D kind) noexcept {
    switch (kind) {
    case SandKind2D::empty: return "empty";
    case SandKind2D::solid: return "solid";
    case SandKind2D::powder: return "powder";
    case SandKind2D::liquid: return "liquid";
    case SandKind2D::gas: return "gas";
    case SandKind2D::fire: return "fire";
    }
    return "unknown";
}

std::uint32_t sand_cell_color(SandCell2D cell, std::uint8_t shade) noexcept {
    const SandCellInfo2D& info = sand_cell_info(cell);
    const float s = static_cast<float>(shade) / 255.0f;
    float k = 1.0f;
    switch (info.kind) {
    case SandKind2D::powder:
    case SandKind2D::solid: k = 0.82f + 0.36f * s; break;
    case SandKind2D::liquid: k = 0.93f + 0.14f * s; break;
    case SandKind2D::gas: k = 0.9f + 0.2f * s; break;
    case SandKind2D::fire: k = 0.75f + 0.5f * s; break;
    case SandKind2D::empty: k = 1.0f; break;
    }
    FluidColor c = info.color;
    c.r = std::min(1.0f, c.r * k);
    c.g = std::min(1.0f, c.g * k);
    c.b = std::min(1.0f, c.b * k);
    if (cell == SandCell2D::fire) c.g = std::min(1.0f, 0.25f + 0.6f * s);
    return fluid_pack_color(c);
}

void FluidVolume2D::init_sand() {
    const std::size_t n = static_cast<std::size_t>(m_nx) * static_cast<std::size_t>(m_ny);
    m_sand.assign(n, 0);
    m_life.assign(n, 0);
    m_shade.resize(n);
    for (std::size_t c = 0; c < n; ++c) m_shade[c] = static_cast<std::uint8_t>(hash32(static_cast<std::uint32_t>(c) ^ m_def.sand.seed) >> 24);
    m_clock.assign(n, 0);
    if (m_def.sand.heat) m_temp.assign(n, static_cast<float>(m_def.sand.ambient_temperature));
    else m_temp.clear();
    m_kx = (m_nx + kSandChunk - 1) / kSandChunk;
    m_ky = (m_ny + kSandChunk - 1) / kSandChunk;
    const std::size_t chunks = static_cast<std::size_t>(m_kx) * static_cast<std::size_t>(m_ky);
    m_rect_now.assign(chunks, SandRect{});
    m_rect_next.assign(chunks, SandRect{});
    m_rect_spill.assign(chunks, SandRect{});
    for (auto& p : m_sand_phase) {
        p.clear();
        p.reserve(chunks / 4 + 4);
    }
    if (m_def.fill > 0.0_r) {
        const int rows = std::clamp(static_cast<int>(std::round(m_def.fill * static_cast<Real>(m_ny))), 0, m_ny);
        for (int j = 0; j < rows; ++j) {
            for (int i = 0; i < m_nx; ++i) sand_set(cell_index(i, j), m_def.sand.fill_cell, &m_brush_rng);
        }
    }
    wake();
}

void FluidVolume2D::sand_set(std::size_t c, SandCell2D cell, SandRng* rng) noexcept {
    SandRng& r = rng ? *rng : m_brush_rng;
    const SandCellInfo2D& info = sand_cell_info(cell);
    m_sand[c] = static_cast<std::uint8_t>(cell);
    const int span = info.life_max - info.life_min;
    m_life[c] = static_cast<std::uint8_t>(std::clamp(info.life_min + (span > 0 ? static_cast<int>(r.next() % static_cast<std::uint32_t>(span + 1)) : 0), 0, 127));
    m_shade[c] = static_cast<std::uint8_t>(r.next() >> 24);
    if (!m_temp.empty()) {
        const float amb = static_cast<float>(m_def.sand.ambient_temperature);
        if (info.emits_heat || cell == SandCell2D::steam) m_temp[c] = static_cast<float>(info.base_temperature);
        else if (cell == SandCell2D::empty) m_temp[c] = amb;
    }
}

void FluidVolume2D::sand_swap(std::size_t a, std::size_t b) noexcept {
    std::swap(m_sand[a], m_sand[b]);
    std::swap(m_life[a], m_life[b]);
    std::swap(m_shade[a], m_shade[b]);
    if (!m_temp.empty()) std::swap(m_temp[a], m_temp[b]);
}

bool FluidVolume2D::sand_free(int x, int y) const noexcept {
    if (!valid_cell(x, y)) return false;
    const std::size_t c = cell_index(x, y);
    return m_sand[c] == 0 && m_occ[c] == 0;
}

void FluidVolume2D::sand_mark(int x, int y) noexcept {
    if (m_rect_next.empty()) return;
    const int x0 = std::max(0, x - 1), y0 = std::max(0, y - 1);
    const int x1 = std::min(m_nx - 1, x + 1), y1 = std::min(m_ny - 1, y + 1);
    if (x0 > x1 || y0 > y1) return;
    for (int cy = y0 / kSandChunk; cy <= y1 / kSandChunk; ++cy) {
        for (int cx = x0 / kSandChunk; cx <= x1 / kSandChunk; ++cx) {
            const int bx0 = cx * kSandChunk, by0 = cy * kSandChunk;
            const int bx1 = std::min(m_nx, bx0 + kSandChunk) - 1, by1 = std::min(m_ny, by0 + kSandChunk) - 1;
            m_rect_next[static_cast<std::size_t>(cy * m_kx + cx)].add(std::max(x0, bx0), std::max(y0, by0), std::min(x1, bx1), std::min(y1, by1));
        }
    }
}

SandCell2D FluidVolume2D::sand_cell(int i, int j) const noexcept {
    if (!valid_cell(i, j) || m_sand.empty()) return SandCell2D::empty;
    return static_cast<SandCell2D>(m_sand[cell_index(i, j)]);
}

void FluidVolume2D::set_sand_cell(int i, int j, SandCell2D cell) {
    if (!valid_cell(i, j) || m_sand.empty() || cell == SandCell2D::count) return;
    sand_set(cell_index(i, j), cell, &m_brush_rng);
    sand_mark(i, j);
    m_solids_changed = true;
}

std::size_t FluidVolume2D::paint_cells(const RVec2& center, Real radius, SandCell2D cell, Real probability, bool replace) {
    if (m_sand.empty() || cell == SandCell2D::count) return 0;
    const RVec2 l = to_local(center);
    const Real r = std::max(radius, 0.5_r * m_cs);
    const int i0 = std::max(0, static_cast<int>(std::floor((l.x - r) / m_cs)));
    const int j0 = std::max(0, static_cast<int>(std::floor((l.y - r) / m_cs)));
    const int i1 = std::min(m_nx - 1, static_cast<int>(std::floor((l.x + r) / m_cs)));
    const int j1 = std::min(m_ny - 1, static_cast<int>(std::floor((l.y + r) / m_cs)));
    const Real r2 = r * r;
    std::size_t n = 0;
    for (int j = j0; j <= j1; ++j) {
        for (int i = i0; i <= i1; ++i) {
            const Real dx = (static_cast<Real>(i) + 0.5_r) * m_cs - l.x;
            const Real dy = (static_cast<Real>(j) + 0.5_r) * m_cs - l.y;
            if (dx * dx + dy * dy > r2) continue;
            const std::size_t c = cell_index(i, j);
            if (!replace && m_sand[c] != 0) continue;
            if (static_cast<SandCell2D>(m_sand[c]) == cell) continue;
            if (!m_brush_rng.chance(probability)) continue;
            sand_set(c, cell, &m_brush_rng);
            sand_mark(i, j);
            ++n;
        }
    }
    if (n) m_solids_changed = true;
    return n;
}

std::size_t FluidVolume2D::paint_cells_box(const AABB& box, SandCell2D cell, Real probability, bool replace) {
    if (m_sand.empty() || cell == SandCell2D::count) return 0;
    const RVec2 a = to_local(box.min), b = to_local(box.max);
    const RVec2 lo{ std::min(a.x, b.x), std::min(a.y, b.y) }, hi{ std::max(a.x, b.x), std::max(a.y, b.y) };
    const int i0 = std::max(0, static_cast<int>(std::floor(lo.x / m_cs + 1e-4_r)));
    const int j0 = std::max(0, static_cast<int>(std::floor(lo.y / m_cs + 1e-4_r)));
    const int i1 = std::min(m_nx - 1, static_cast<int>(std::ceil(hi.x / m_cs - 1e-4_r)) - 1);
    const int j1 = std::min(m_ny - 1, static_cast<int>(std::ceil(hi.y / m_cs - 1e-4_r)) - 1);
    std::size_t n = 0;
    for (int j = j0; j <= j1; ++j) {
        for (int i = i0; i <= i1; ++i) {
            const std::size_t c = cell_index(i, j);
            if (!replace && m_sand[c] != 0) continue;
            if (!m_brush_rng.chance(probability)) continue;
            sand_set(c, cell, &m_brush_rng);
            sand_mark(i, j);
            ++n;
        }
    }
    if (n) m_solids_changed = true;
    return n;
}

std::size_t FluidVolume2D::erase_cells(const RVec2& center, Real radius) {
    if (m_def.level == FluidLevel::blocky) return paint_cells(center, radius, SandCell2D::empty, 1.0_r, true);
    return remove_fluid_circle(center, radius);
}

std::size_t FluidVolume2D::count_cells(SandCell2D cell) const noexcept {
    std::size_t n = 0;
    const std::uint8_t v = static_cast<std::uint8_t>(cell);
    for (std::uint8_t c : m_sand) n += c == v ? 1u : 0u;
    return n;
}

Real FluidVolume2D::temperature(int i, int j) const noexcept {
    if (!valid_cell(i, j) || m_sand.empty()) return m_def.sand.ambient_temperature;
    const std::size_t c = cell_index(i, j);
    if (!m_temp.empty()) return static_cast<Real>(m_temp[c]);
    const SandCellInfo2D& info = sand_cell_info(static_cast<SandCell2D>(m_sand[c]));
    return info.emits_heat ? info.base_temperature : m_def.sand.ambient_temperature;
}

void FluidVolume2D::add_heat(const RVec2& center, Real radius, Real degrees) {
    if (m_sand.empty()) return;
    const RVec2 l = to_local(center);
    const Real r = std::max(radius, 0.5_r * m_cs);
    const int i0 = std::max(0, static_cast<int>(std::floor((l.x - r) / m_cs)));
    const int j0 = std::max(0, static_cast<int>(std::floor((l.y - r) / m_cs)));
    const int i1 = std::min(m_nx - 1, static_cast<int>(std::floor((l.x + r) / m_cs)));
    const int j1 = std::min(m_ny - 1, static_cast<int>(std::floor((l.y + r) / m_cs)));
    for (int j = j0; j <= j1; ++j) {
        for (int i = i0; i <= i1; ++i) {
            const Real dx = (static_cast<Real>(i) + 0.5_r) * m_cs - l.x;
            const Real dy = (static_cast<Real>(j) + 0.5_r) * m_cs - l.y;
            if (dx * dx + dy * dy > r * r) continue;
            const std::size_t c = cell_index(i, j);
            sand_mark(i, j);
            if (!m_temp.empty()) {
                m_temp[c] += static_cast<float>(degrees);
                continue;
            }
            const SandCell2D s = static_cast<SandCell2D>(m_sand[c]);
            if (degrees > 0.0_r) {
                if (s == SandCell2D::water && degrees >= 80.0_r) sand_set(c, SandCell2D::steam, &m_brush_rng);
                else if (s == SandCell2D::ice) sand_set(c, SandCell2D::water, &m_brush_rng);
                else if ((s == SandCell2D::wood || s == SandCell2D::oil) && degrees >= 200.0_r) sand_set(c, SandCell2D::fire, &m_brush_rng);
                else if (s == SandCell2D::stone && degrees >= 1000.0_r) sand_set(c, SandCell2D::lava, &m_brush_rng);
            } else if (degrees < 0.0_r) {
                if (s == SandCell2D::water) sand_set(c, SandCell2D::ice, &m_brush_rng);
                else if (s == SandCell2D::steam) sand_set(c, SandCell2D::water, &m_brush_rng);
                else if (s == SandCell2D::lava && degrees <= -500.0_r) sand_set(c, SandCell2D::stone, &m_brush_rng);
                else if (s == SandCell2D::fire) sand_set(c, SandCell2D::smoke, &m_brush_rng);
            }
        }
    }
}

void FluidVolume2D::sand_displace() {
    if (!m_def.sand.displace_by_bodies || m_occ_list.empty()) return;
    std::vector<std::uint32_t>& queue = m_tmp_u32;
    std::vector<std::uint8_t>& seen = m_tmp_u8;
    const std::size_t n = m_sand.size();
    if (seen.size() != n) seen.assign(n, 0);
    const int limit = std::max(8, m_def.sand.displace_search * 8);
    bool touched = false;
    for (std::uint32_t oc : m_occ_list) {
        const SandCell2D s = static_cast<SandCell2D>(m_sand[oc]);
        const SandKind2D k = sand_cell_info(s).kind;
        if (k == SandKind2D::empty || k == SandKind2D::solid) continue;
        const int ox = static_cast<int>(oc % static_cast<std::uint32_t>(m_nx));
        const int oy = static_cast<int>(oc / static_cast<std::uint32_t>(m_nx));
        if (k == SandKind2D::gas || k == SandKind2D::fire) {
            sand_set(oc, SandCell2D::empty, nullptr);
            sand_mark(ox, oy);
            continue;
        }
        touched = true;
        queue.clear();
        queue.push_back(oc);
        seen[oc] = 1;
        std::size_t head = 0;
        std::uint32_t found = std::numeric_limits<std::uint32_t>::max();
        while (head < queue.size() && static_cast<int>(queue.size()) < limit) {
            const std::uint32_t c = queue[head++];
            const int x = static_cast<int>(c % static_cast<std::uint32_t>(m_nx));
            const int y = static_cast<int>(c / static_cast<std::uint32_t>(m_nx));
            const int dxs[4] = { 0, -1, 1, 0 };
            const int dys[4] = { 1, 0, 0, -1 };
            for (int d = 0; d < 4; ++d) {
                const int ax = x + dxs[d], ay = y + dys[d];
                if (!valid_cell(ax, ay)) continue;
                const std::uint32_t a = static_cast<std::uint32_t>(cell_index(ax, ay));
                if (seen[a]) continue;
                seen[a] = 1;
                const SandKind2D ak = sand_cell_info(static_cast<SandCell2D>(m_sand[a])).kind;
                if (ak == SandKind2D::solid || m_occ[a]) continue;
                if (ak == SandKind2D::empty || ak == SandKind2D::gas || ak == SandKind2D::fire) {
                    found = a;
                    break;
                }
                queue.push_back(a);
            }
            if (found != std::numeric_limits<std::uint32_t>::max()) break;
        }
        for (std::uint32_t c : queue) seen[c] = 0;
        if (found != std::numeric_limits<std::uint32_t>::max()) {
            seen[found] = 0;
            sand_set(found, SandCell2D::empty, nullptr);
            sand_swap(oc, found);
            sand_mark(ox, oy);
            sand_mark(static_cast<int>(found % static_cast<std::uint32_t>(m_nx)), static_cast<int>(found / static_cast<std::uint32_t>(m_nx)));
        }
    }
    if (touched) std::fill(seen.begin(), seen.end(), std::uint8_t{ 0 });
}

void FluidVolume2D::sand_heat(int x, int y, SandRect& spill) {
    const std::size_t c = cell_index(x, y);
    const SandCell2D s = static_cast<SandCell2D>(m_sand[c]);
    const SandCellInfo2D& info = sand_cell_info(s);
    const float amb = static_cast<float>(m_def.sand.ambient_temperature);
    float sum = 0.0f;
    const int dxs[4] = { 1, -1, 0, 0 };
    const int dys[4] = { 0, 0, 1, -1 };
    for (int d = 0; d < 4; ++d) {
        const int ax = x + dxs[d], ay = y + dys[d];
        sum += valid_cell(ax, ay) ? m_temp[cell_index(ax, ay)] : amb;
    }
    float t = m_temp[c];
    const float k = static_cast<float>(m_def.sand.heat_conduction) * (s == SandCell2D::empty ? 0.25f : 1.0f);
    t += k * (0.25f * sum - t);
    if (info.emits_heat) t += 0.2f * (static_cast<float>(info.base_temperature) - t);
    else t += 0.002f * (amb - t);
    m_temp[c] = t;
    SandRng local{ hash32(static_cast<std::uint32_t>(c) * 2654435761u + m_sand_tick) | 1u };
    switch (s) {
    case SandCell2D::water:
        if (t > 100.0f) sand_set(c, SandCell2D::steam, &local);
        else if (t < -2.0f) sand_set(c, SandCell2D::ice, &local);
        break;
    case SandCell2D::ice:
        if (t > 1.0f) sand_set(c, SandCell2D::water, &local);
        break;
    case SandCell2D::steam:
        if (t < 40.0f) sand_set(c, SandCell2D::water, &local);
        break;
    case SandCell2D::lava:
        if (t < 650.0f) sand_set(c, SandCell2D::stone, &local);
        break;
    case SandCell2D::stone:
        if (t > 1100.0f) sand_set(c, SandCell2D::lava, &local);
        break;
    case SandCell2D::wood:
        if (t > 300.0f) {
            sand_set(c, SandCell2D::fire, &local);
            m_life[c] = static_cast<std::uint8_t>(128 + 60 + static_cast<int>(local.next() % 60u));
        }
        break;
    case SandCell2D::oil:
        if (t > 250.0f) sand_set(c, SandCell2D::fire, &local);
        break;
    default:
        break;
    }
    if (std::abs(m_temp[c] - amb) > 1.5f) spill.add(std::max(0, x - 1), std::max(0, y - 1), std::min(m_nx - 1, x + 1), std::min(m_ny - 1, y + 1));
}

void FluidVolume2D::sand_cell_update(int x, int y, SandRng& rng, SandRect& spill, std::uint32_t tick) {
    const std::size_t c = cell_index(x, y);
    const std::uint8_t raw = m_sand[c];
    if (raw == 0 || m_clock[c] == static_cast<std::uint8_t>(tick) || m_occ[c]) return;
    const SandCell2D m = static_cast<SandCell2D>(raw);
    const SandCellInfo2D& info = sand_cell_info(m);
    const std::uint8_t tk = static_cast<std::uint8_t>(tick);
    const FluidSandSettings2D& st = m_def.sand;
    auto mark = [&](int ax, int ay) { spill.add(std::max(0, ax - 1), std::max(0, ay - 1), std::min(m_nx - 1, ax + 1), std::min(m_ny - 1, ay + 1)); };
    auto at = [&](int ax, int ay) -> SandCell2D { return valid_cell(ax, ay) ? static_cast<SandCell2D>(m_sand[cell_index(ax, ay)]) : SandCell2D::wall; };
    auto move_to = [&](int ax, int ay) {
        const std::size_t d = cell_index(ax, ay);
        sand_swap(c, d);
        m_clock[d] = tk;
        if (m_sand[c] != 0) m_clock[c] = tk;
        mark(x, y);
        mark(ax, ay);
    };
    auto can_enter = [&](int ax, int ay, bool rising, Real sink_chance) {
        if (!valid_cell(ax, ay)) return false;
        const std::size_t d = cell_index(ax, ay);
        if (m_occ[d]) return false;
        const std::uint8_t t = m_sand[d];
        if (t == 0) return true;
        const SandCellInfo2D& ti = sand_cell_info(static_cast<SandCell2D>(t));
        if (ti.kind != SandKind2D::liquid && ti.kind != SandKind2D::gas) return false;
        if (t == raw) return false;
        if (m_clock[d] == tk) return false;
        const bool ok = rising ? ti.density > info.density : ti.density < info.density;
        if (!ok) return false;
        if (ti.kind == SandKind2D::gas && !rising) return true;
        if (!rng.chance(sink_chance)) {
            mark(x, y);
            return false;
        }
        return true;
    };
    auto react4 = [&](auto&& fn) {
        const int dxs[4] = { 1, -1, 0, 0 };
        const int dys[4] = { 0, 0, 1, -1 };
        const int start = static_cast<int>(rng.next() & 3u);
        for (int q = 0; q < 4; ++q) {
            const int d = (start + q) & 3;
            const int ax = x + dxs[d], ay = y + dys[d];
            if (!valid_cell(ax, ay)) continue;
            if (fn(ax, ay, cell_index(ax, ay))) return true;
        }
        return false;
    };
    const Real rate = st.reaction_rate;
    switch (info.kind) {
    case SandKind2D::empty:
    case SandKind2D::solid:
        return;
    case SandKind2D::powder: {
        if (can_enter(x, y - 1, false, 0.6_r)) {
            move_to(x, y - 1);
            return;
        }
        const int dir = (rng.next() & 1u) ? 1 : -1;
        for (int q = 0; q < 2; ++q) {
            const int sx = q == 0 ? dir : -dir;
            if (info.slide_chance < 1.0_r && !sand_free(x + sx, y) && !(valid_cell(x + sx, y) && sand_cell_info(at(x + sx, y)).kind == SandKind2D::liquid)) continue;
            if (can_enter(x + sx, y - 1, false, 0.5_r)) {
                move_to(x + sx, y - 1);
                return;
            }
        }
        return;
    }
    case SandKind2D::liquid: {
        if (m == SandCell2D::lava) {
            const bool done = react4([&](int ax, int ay, std::size_t d) {
                const SandCell2D t = static_cast<SandCell2D>(m_sand[d]);
                if (t == SandCell2D::water) {
                    if (!rng.chance(0.5_r * rate)) {
                        mark(x, y);
                        return false;
                    }
                    sand_set(d, SandCell2D::steam, &rng);
                    sand_set(c, SandCell2D::stone, &rng);
                    m_clock[c] = tk;
                    m_clock[d] = tk;
                    mark(ax, ay);
                    mark(x, y);
                    return true;
                }
                if (t == SandCell2D::wood || t == SandCell2D::oil) {
                    if (rng.chance((t == SandCell2D::oil ? 0.3_r : 0.05_r) * st.fire_spread)) {
                        sand_set(d, SandCell2D::fire, &rng);
                        if (t == SandCell2D::wood) m_life[d] = static_cast<std::uint8_t>(128 + 60 + static_cast<int>(rng.next() % 60u));
                        m_clock[d] = tk;
                        mark(ax, ay);
                    } else {
                        mark(x, y);
                    }
                    return false;
                }
                if (t == SandCell2D::ice) {
                    if (rng.chance(0.1_r * rate)) {
                        sand_set(d, SandCell2D::water, &rng);
                        mark(ax, ay);
                    } else {
                        mark(x, y);
                    }
                    return false;
                }
                if (t == SandCell2D::empty && rng.chance(0.002_r)) {
                    mark(x, y);
                }
                return false;
            });
            if (done) return;
        } else if (m == SandCell2D::acid) {
            const bool done = react4([&](int ax, int ay, std::size_t d) {
                const SandCell2D t = static_cast<SandCell2D>(m_sand[d]);
                if (t == SandCell2D::sand || t == SandCell2D::stone || t == SandCell2D::wood || t == SandCell2D::ice) {
                    if (!rng.chance(0.05_r * rate)) {
                        mark(x, y);
                        return false;
                    }
                    sand_set(d, rng.chance(0.2_r) ? SandCell2D::smoke : SandCell2D::empty, &rng);
                    m_clock[d] = tk;
                    mark(ax, ay);
                    if (rng.chance(0.3_r)) {
                        sand_set(c, SandCell2D::empty, &rng);
                        mark(x, y);
                        return true;
                    }
                    mark(x, y);
                }
                return false;
            });
            if (done) return;
        }
        if (info.move_chance < 1.0_r && !rng.chance(info.move_chance)) {
            mark(x, y);
            return;
        }
        if (can_enter(x, y - 1, false, 0.35_r)) {
            move_to(x, y - 1);
            return;
        }
        const int dir = (rng.next() & 1u) ? 1 : -1;
        for (int q = 0; q < 2; ++q) {
            const int sx = q == 0 ? dir : -dir;
            if (sand_free(x + sx, y - 1) && (sand_free(x + sx, y) || at(x + sx, y) == m)) {
                move_to(x + sx, y - 1);
                return;
            }
        }
        const int disp = std::max(1, info.dispersion);
        for (int q = 0; q < 2; ++q) {
            const int sx = q == 0 ? dir : -dir;
            if (!sand_free(x + sx, y)) continue;
            int last = 0;
            bool drop = false;
            for (int k = 1; k <= disp; ++k) {
                if (!sand_free(x + sx * k, y)) break;
                last = k;
                if (sand_free(x + sx * k, y - 1)) {
                    drop = true;
                    break;
                }
            }
            const bool behind = at(x - sx, y) == m;
            if (last > 0 && (drop || behind)) {
                move_to(x + sx * last, y);
                return;
            }
        }
        return;
    }
    case SandKind2D::gas: {
        mark(x, y);
        if (m == SandCell2D::steam) {
            const bool cold = react4([&](int, int, std::size_t d) { return static_cast<SandCell2D>(m_sand[d]) == SandCell2D::ice; });
            if (cold && rng.chance(0.2_r * st.steam_condense)) {
                sand_set(c, SandCell2D::water, &rng);
                m_clock[c] = tk;
                return;
            }
        }
        if (m_life[c] > 0) {
            --m_life[c];
        } else {
            if (m == SandCell2D::steam) {
                if (rng.chance(0.5_r * st.steam_condense)) {
                    sand_set(c, SandCell2D::water, &rng);
                    m_clock[c] = tk;
                    return;
                }
                m_life[c] = static_cast<std::uint8_t>(10 + (rng.next() % 30u));
            } else {
                sand_set(c, SandCell2D::empty, &rng);
                return;
            }
        }
        if (info.move_chance < 1.0_r && !rng.chance(info.move_chance)) return;
        if (can_enter(x, y + 1, true, 0.7_r)) {
            move_to(x, y + 1);
            return;
        }
        const int dir = (rng.next() & 1u) ? 1 : -1;
        if (can_enter(x + dir, y + 1, true, 0.5_r)) {
            move_to(x + dir, y + 1);
            return;
        }
        if (can_enter(x - dir, y + 1, true, 0.5_r)) {
            move_to(x - dir, y + 1);
            return;
        }
        const int k = 1 + static_cast<int>(rng.next() % static_cast<std::uint32_t>(std::max(1, info.dispersion)));
        int last = 0;
        for (int s = 1; s <= k; ++s) {
            if (!sand_free(x + dir * s, y)) break;
            last = s;
        }
        if (last > 0) move_to(x + dir * last, y);
        return;
    }
    case SandKind2D::fire: {
        mark(x, y);
        const bool anchored = (m_life[c] & 128u) != 0;
        int life = m_life[c] & 127;
        if (life <= 0) {
            sand_set(c, rng.chance(0.35_r) ? SandCell2D::smoke : SandCell2D::empty, &rng);
            m_clock[c] = tk;
            return;
        }
        --life;
        m_life[c] = static_cast<std::uint8_t>(life | (anchored ? 128 : 0));
        const bool out = react4([&](int ax, int ay, std::size_t d) {
            const SandCell2D t = static_cast<SandCell2D>(m_sand[d]);
            const SandCellInfo2D& ti = sand_cell_info(t);
            if (t == SandCell2D::water) {
                if (rng.chance(0.3_r)) {
                    sand_set(d, SandCell2D::steam, &rng);
                    mark(ax, ay);
                }
                sand_set(c, SandCell2D::smoke, &rng);
                m_clock[c] = tk;
                return true;
            }
            if (t == SandCell2D::ice) {
                if (rng.chance(0.05_r * rate)) {
                    sand_set(d, SandCell2D::water, &rng);
                    mark(ax, ay);
                }
                return false;
            }
            if (ti.flammability > 0.0_r && rng.chance(ti.flammability * st.fire_spread)) {
                sand_set(d, SandCell2D::fire, &rng);
                if (t == SandCell2D::wood) m_life[d] = static_cast<std::uint8_t>(128 + 50 + static_cast<int>(rng.next() % 60u));
                else m_life[d] = static_cast<std::uint8_t>(15 + static_cast<int>(rng.next() % 25u));
                m_clock[d] = tk;
                mark(ax, ay);
            }
            return false;
        });
        if (out) return;
        const bool fuel = react4([&](int, int, std::size_t d) { return sand_cell_info(static_cast<SandCell2D>(m_sand[d])).flammability > 0.0_r; });
        if (fuel && !anchored) return;
        if (anchored) {
            if (rng.chance(0.08_r) && sand_free(x, y + 1)) {
                const std::size_t d = cell_index(x, y + 1);
                sand_set(d, SandCell2D::fire, &rng);
                m_life[d] = static_cast<std::uint8_t>(4 + static_cast<int>(rng.next() % 10u));
                m_clock[d] = tk;
                mark(x, y + 1);
            }
            return;
        }
        if (rng.chance(0.5_r) && sand_free(x, y + 1)) {
            move_to(x, y + 1);
            return;
        }
        const int dir = (rng.next() & 1u) ? 1 : -1;
        if (rng.chance(0.3_r) && sand_free(x + dir, y + 1)) {
            move_to(x + dir, y + 1);
            return;
        }
        if (rng.chance(0.2_r) && sand_free(x + dir, y)) move_to(x + dir, y);
        return;
    }
    }
}

void FluidVolume2D::sand_chunk(std::size_t chunk, std::uint32_t tick) {
    const SandRect r = m_rect_now[chunk];
    if (r.empty()) return;
    SandRng rng{ hash32(tick * 0x9e3779b9u ^ static_cast<std::uint32_t>(chunk) * 0x85ebca6bu ^ m_def.sand.seed) | 1u };
    SandRect& spill = m_rect_spill[chunk];
    const bool heat = !m_temp.empty();
    for (int y = r.y0; y <= r.y1; ++y) {
        const bool ltr = ((tick + static_cast<std::uint32_t>(y)) & 1u) != 0;
        const int w = r.x1 - r.x0 + 1;
        for (int k = 0; k < w; ++k) {
            const int x = ltr ? r.x0 + k : r.x1 - k;
            sand_cell_update(x, y, rng, spill, tick);
            if (heat) sand_heat(x, y, spill);
        }
    }
}

void FluidVolume2D::sand_tick(detail::FluidTaskPool& pool) {
    ++m_sand_tick;
    const std::uint32_t tick = m_sand_tick % 255u + 1u;
    m_rect_now.swap(m_rect_next);
    std::fill(m_rect_next.begin(), m_rect_next.end(), SandRect{});
    std::fill(m_rect_spill.begin(), m_rect_spill.end(), SandRect{});
    m_sand_active = 0;
    for (auto& p : m_sand_phase) p.clear();
    for (int cy = 0; cy < m_ky; ++cy) {
        for (int cx = 0; cx < m_kx; ++cx) {
            const std::size_t c = static_cast<std::size_t>(cy * m_kx + cx);
            if (m_rect_now[c].empty()) continue;
            ++m_sand_active;
            m_sand_phase[(cx & 1) + 2 * (cy & 1)].push_back(static_cast<std::uint32_t>(c));
        }
    }
    for (int phase = 0; phase < 4; ++phase) {
        const std::vector<std::uint32_t>& list = m_sand_phase[phase];
        if (list.empty()) continue;
        if (pool.size() > 1 && list.size() > 1) {
            pool.parallel_for(list.size(), [this, &list, tick](std::size_t q) { sand_chunk(list[q], tick); });
        } else {
            for (std::uint32_t c : list) sand_chunk(c, tick);
        }
    }
    for (int cy = 0; cy < m_ky; ++cy) {
        for (int cx = 0; cx < m_kx; ++cx) {
            const SandRect& s = m_rect_spill[static_cast<std::size_t>(cy * m_kx + cx)];
            if (s.empty()) continue;
            for (int ny = std::max(0, cy - 1); ny <= std::min(m_ky - 1, cy + 1); ++ny) {
                for (int nx = std::max(0, cx - 1); nx <= std::min(m_kx - 1, cx + 1); ++nx) {
                    const int bx0 = nx * kSandChunk, by0 = ny * kSandChunk;
                    const int bx1 = std::min(m_nx, bx0 + kSandChunk) - 1, by1 = std::min(m_ny, by0 + kSandChunk) - 1;
                    const int x0 = std::max(s.x0, bx0), y0 = std::max(s.y0, by0), x1 = std::min(s.x1, bx1), y1 = std::min(s.y1, by1);
                    if (x0 > x1 || y0 > y1) continue;
                    m_rect_next[static_cast<std::size_t>(ny * m_kx + nx)].add(x0, y0, x1, y1);
                }
            }
        }
    }
}

void FluidVolume2D::step_sand(detail::FluidTaskPool& pool, Real dt, FluidStats2D& stats) {
    if (m_sand.empty()) return;
    sand_displace();
    for (FluidEmitter2D& e : m_emitters) {
        if (!e.active || e.rate <= 0.0_r) continue;
        e.carry += e.rate * dt;
        const Real area = std::max(3.14159265_r * e.radius * e.radius, m_cs * m_cs);
        const Real prob = std::min(1.0_r, e.carry * m_cs * m_cs / area);
        if (prob <= 0.0_r) continue;
        paint_cells(e.position, e.radius, e.cell, prob, false);
        e.carry = 0.0_r;
    }
    m_accum += dt;
    const Real tick = 1.0_r / std::max(1.0_r, m_def.sand.tick_rate);
    const int budget = std::max(1, m_quality.max_ticks_per_frame);
    int done = 0;
    while (m_accum >= tick * 0.999_r && done < budget) {
        sand_tick(pool);
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
