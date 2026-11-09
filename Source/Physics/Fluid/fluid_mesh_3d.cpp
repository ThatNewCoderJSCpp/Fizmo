#include "fizmo_library.hpp"
#include "fluid_3d.hpp"

#include <chrono>

namespace fizmo {
namespace physics {

namespace {

using V3 = RVec3;

constexpr std::uint8_t kMeshDeep = 4;

inline int ifloor(Real f) noexcept {
    const int i = static_cast<int>(f);
    return i - (f < static_cast<Real>(i) ? 1 : 0);
}

struct TetTable {
    int corners[6][4];
    int count[6][16];
    int edge[6][16][2][3][2];
};

int bits_of(int c) noexcept { return (c & 1) + ((c >> 1) & 1) + ((c >> 2) & 1); }

V3 corner_pos(int c) noexcept { return V3{ static_cast<Real>(c & 1), static_cast<Real>((c >> 1) & 1), static_cast<Real>((c >> 2) & 1) }; }

TetTable make_tet_table() {
    TetTable t{};
    const int perms[6][3] = { { 1, 2, 4 }, { 1, 4, 2 }, { 2, 1, 4 }, { 2, 4, 1 }, { 4, 1, 2 }, { 4, 2, 1 } };
    for (int q = 0; q < 6; ++q) {
        t.corners[q][0] = 0;
        t.corners[q][1] = perms[q][0];
        t.corners[q][2] = perms[q][0] | perms[q][1];
        t.corners[q][3] = 7;
        for (int m = 0; m < 16; ++m) {
            int in[4], out[4], ni = 0, no = 0;
            for (int v = 0; v < 4; ++v) {
                if (m & (1 << v)) in[ni++] = t.corners[q][v];
                else out[no++] = t.corners[q][v];
            }
            t.count[q][m] = 0;
            if (ni == 0 || ni == 4) continue;
            int tris[2][3][2];
            int nt = 0;
            auto pair = [](int a, int b, int e[2]) {
                if (bits_of(a) <= bits_of(b)) {
                    e[0] = a;
                    e[1] = b;
                } else {
                    e[0] = b;
                    e[1] = a;
                }
            };
            if (ni == 1) {
                for (int k = 0; k < 3; ++k) pair(in[0], out[k], tris[0][k]);
                nt = 1;
            } else if (ni == 3) {
                for (int k = 0; k < 3; ++k) pair(out[0], in[k], tris[0][k]);
                nt = 1;
            } else {
                int ac[2], ad[2], bd[2], bc[2];
                pair(in[0], out[0], ac);
                pair(in[0], out[1], ad);
                pair(in[1], out[1], bd);
                pair(in[1], out[0], bc);
                const int* first[3] = { ac, ad, bd };
                const int* second[3] = { ac, bd, bc };
                for (int k = 0; k < 3; ++k) {
                    tris[0][k][0] = first[k][0];
                    tris[0][k][1] = first[k][1];
                    tris[1][k][0] = second[k][0];
                    tris[1][k][1] = second[k][1];
                }
                nt = 2;
            }
            V3 gi{}, go{};
            for (int k = 0; k < ni; ++k) gi = gi + corner_pos(in[k]);
            for (int k = 0; k < no; ++k) go = go + corner_pos(out[k]);
            const V3 g = go * (1.0_r / static_cast<Real>(no)) - gi * (1.0_r / static_cast<Real>(ni));
            for (int k = 0; k < nt; ++k) {
                V3 mp[3];
                for (int e = 0; e < 3; ++e) mp[e] = (corner_pos(tris[k][e][0]) + corner_pos(tris[k][e][1])) * 0.5_r;
                const V3 nrm = vec3::cross(mp[1] - mp[0], mp[2] - mp[0]);
                if (vec3::dot(nrm, g) < 0.0_r) {
                    std::swap(tris[k][1][0], tris[k][2][0]);
                    std::swap(tris[k][1][1], tris[k][2][1]);
                }
                for (int e = 0; e < 3; ++e) {
                    t.edge[q][m][k][e][0] = tris[k][e][0];
                    t.edge[q][m][k][e][1] = tris[k][e][1];
                }
            }
            t.count[q][m] = nt;
        }
    }
    return t;
}

const TetTable& tet_table() {
    static const TetTable table = make_tet_table();
    return table;
}

} // namespace

Real FluidVolume3D::surface_mesh_cell(int detail) const noexcept {
    const int d = std::clamp(detail, 0, 4);
    Real base = m_cs;
    if (m_def.level == FluidLevel::realistic && !flip_solver()) base = m_spacing;
    base = std::max(base, 1e-4_r);
    return d == 0 ? 2.0_r * base : base / static_cast<Real>(d);
}

const FluidSurfaceMesh3D& FluidVolume3D::surface_mesh(int detail) const {
    if (m_mesh.version == m_version && m_mesh.detail == detail) return m_mesh;
    build_surface_mesh(m_mesh, detail);
    return m_mesh;
}

void FluidVolume3D::mesh_field_particles(const std::vector<V3>& local, const std::vector<V3>* vel, Real inv_rest, bool deep) const {
    const std::size_t total = static_cast<std::size_t>(m_mdim[0]) * static_cast<std::size_t>(m_mdim[1]) * static_cast<std::size_t>(m_mdim[2]);
    const bool foam = m_def.surface.foam && vel != nullptr;
    m_mfield.assign(total, 0.0f);
    if (foam) {
        m_mspeed.assign(total, 0.0f);
        m_mweight.assign(total, 0.0f);
    }
    const Real inv = 1.0_r / m_mh;
    const int dx = m_mdim[0], dy = m_mdim[1], dz = m_mdim[2];
    const float scale = static_cast<float>(inv_rest);
    const bool mirror = m_def.surface.close_walls && (m_def.level != FluidLevel::realistic || m_def.particles.contain);
    const Real reach = 1.5_r * m_mh;
    const Real size[3] = { m_size.x, m_size.y, m_size.z };
    for (std::size_t p = 0; p < local.size(); ++p) {
        const V3 base = local[p];
        Real alt[3][2];
        int na[3];
        const Real c[3] = { base.x, base.y, base.z };
        for (int a = 0; a < 3; ++a) {
            alt[a][0] = c[a];
            na[a] = 1;
            if (!mirror) continue;
            if (c[a] < reach) alt[a][na[a]++] = -c[a];
            else if (c[a] > size[a] - reach) alt[a][na[a]++] = 2.0_r * size[a] - c[a];
        }
        for (int mx = 0; mx < na[0]; ++mx) {
        for (int my = 0; my < na[1]; ++my) {
        for (int mz = 0; mz < na[2]; ++mz) {
        const V3 x{ alt[0][mx], alt[1][my], alt[2][mz] };
        const Real ux = x.x * inv + 1.0_r - static_cast<Real>(m_mlo[0]);
        const Real uy = x.y * inv + 1.0_r - static_cast<Real>(m_mlo[1]);
        const Real uz = x.z * inv + 1.0_r - static_cast<Real>(m_mlo[2]);
        const int i0 = ifloor(ux), j0 = ifloor(uy), k0 = ifloor(uz);
        if (i0 < 0 || j0 < 0 || k0 < 0 || i0 + 1 >= dx || j0 + 1 >= dy || k0 + 1 >= dz) continue;
        const float tx = static_cast<float>(ux - static_cast<Real>(i0)), ty = static_cast<float>(uy - static_cast<Real>(j0)), tz = static_cast<float>(uz - static_cast<Real>(k0));
        const float wx[2] = { 1.0f - tx, tx }, wy[2] = { 1.0f - ty, ty }, wz[2] = { 1.0f - tz, tz };
        const float sp = foam ? static_cast<float>(vec3::length((*vel)[p])) : 0.0f;
        for (int q = 0; q < 8; ++q) {
            const int a = q & 1, b = (q >> 1) & 1, c = (q >> 2) & 1;
            const std::size_t idx = (static_cast<std::size_t>(j0 + b) * static_cast<std::size_t>(dz) + static_cast<std::size_t>(k0 + c)) * static_cast<std::size_t>(dx) + static_cast<std::size_t>(i0 + a);
            const float w = wx[a] * wy[b] * wz[c];
            m_mfield[idx] += w * scale;
            if (foam) {
                m_mspeed[idx] += w * sp;
                m_mweight[idx] += w;
            }
        }
        }
        }
        }
    }
    if (!deep || m_ltype.empty()) return;
    const Real h = m_mh;
    const Real icell = 1.0_r / m_cs;
    for (int j = 0; j < dy; ++j) {
        int cj = ifloor((static_cast<Real>(j + m_mlo[1] - 1) * h) * icell);
        if (mirror && cj < 0 && cj >= -1) cj = 0;
        if (mirror && cj >= m_ny && cj <= m_ny) cj = m_ny - 1;
        if (cj < 0 || cj >= m_ny) continue;
        for (int k = 0; k < dz; ++k) {
            int ck = ifloor((static_cast<Real>(k + m_mlo[2] - 1) * h) * icell);
            if (mirror && ck < 0 && ck >= -1) ck = 0;
            if (mirror && ck >= m_nz && ck <= m_nz) ck = m_nz - 1;
            if (ck < 0 || ck >= m_nz) continue;
            for (int i = 0; i < dx; ++i) {
                int ci = ifloor((static_cast<Real>(i + m_mlo[0] - 1) * h) * icell);
                if (mirror && ci < 0 && ci >= -1) ci = 0;
                if (mirror && ci >= m_nx && ci <= m_nx) ci = m_nx - 1;
                if (ci < 0 || ci >= m_nx) continue;
                const std::size_t c = cell_index(ci, cj, ck);
                if (!(m_ltype[c] & kMeshDeep)) continue;
                const float room = std::max(0.0f, 1.0f - static_cast<float>(m_lcount[c]) / 8.0f);
                m_mfield[(static_cast<std::size_t>(j) * static_cast<std::size_t>(dz) + static_cast<std::size_t>(k)) * static_cast<std::size_t>(dx) + static_cast<std::size_t>(i)] += room;
            }
        }
    }
}

void FluidVolume3D::mesh_field_cells() const {
    const std::size_t total = static_cast<std::size_t>(m_mdim[0]) * static_cast<std::size_t>(m_mdim[1]) * static_cast<std::size_t>(m_mdim[2]);
    m_mfield.assign(total, 0.0f);
    if (m_nx <= 0 || m_fill.empty()) return;
    const Real cap = m_def.level == FluidLevel::blocky ? 1.0_r : std::max(m_def.flowing.max_mass, 1e-6_r);
    const bool close = m_def.surface.close_walls;
    const Real h = m_mh;
    const Real icell = 1.0_r / m_cs;
    const int dx = m_mdim[0], dy = m_mdim[1], dz = m_mdim[2];
    auto value = [&](int i, int j, int k) -> Real {
        if (i < 0 || j < 0 || k < 0 || i >= m_nx || j >= m_ny || k >= m_nz) return 0.0_r;
        const std::size_t c = cell_index(i, j, k);
        if (m_solid[c]) return 0.0_r;
        if (!m_block.empty() && m_def.level == FluidLevel::blocky) return static_cast<Real>(m_block[c]) / 8.0_r;
        return std::min(m_fill[c] / cap, 1.0_r);
    };
    for (int j = 0; j < dy; ++j) {
        const Real fy = static_cast<Real>(j + m_mlo[1] - 1) * h * icell - 0.5_r;
        const int j0 = ifloor(fy);
        const Real ty = fy - static_cast<Real>(j0);
        for (int k = 0; k < dz; ++k) {
            const Real fz = static_cast<Real>(k + m_mlo[2] - 1) * h * icell - 0.5_r;
            const int k0 = ifloor(fz);
            const Real tz = fz - static_cast<Real>(k0);
            for (int i = 0; i < dx; ++i) {
                const Real fx = static_cast<Real>(i + m_mlo[0] - 1) * h * icell - 0.5_r;
                const int i0 = ifloor(fx);
                const Real tx = fx - static_cast<Real>(i0);
                const int ci = std::clamp(i0, -1, m_nx), cj = std::clamp(j0, -1, m_ny), ck = std::clamp(k0, -1, m_nz);
                Real acc = 0.0_r;
                for (int q = 0; q < 8; ++q) {
                    const int a = q & 1, b = (q >> 1) & 1, c = (q >> 2) & 1;
                    const Real w = (a ? tx : 1.0_r - tx) * (b ? ty : 1.0_r - ty) * (c ? tz : 1.0_r - tz);
                    if (w <= 0.0_r) continue;
                    int ai = ci + a, bj = cj + b, ck2 = ck + c;
                    const Real px = static_cast<Real>(i + m_mlo[0] - 1) * h, py = static_cast<Real>(j + m_mlo[1] - 1) * h, pz = static_cast<Real>(k + m_mlo[2] - 1) * h;
                    if (close || (px >= 0.0_r && px <= m_size.x)) ai = std::clamp(ai, 0, m_nx - 1);
                    if (close || (py >= 0.0_r && py <= m_size.y)) bj = std::clamp(bj, 0, m_ny - 1);
                    if (close || (pz >= 0.0_r && pz <= m_size.z)) ck2 = std::clamp(ck2, 0, m_nz - 1);
                    acc += w * value(ai, bj, ck2);
                }
                m_mfield[(static_cast<std::size_t>(j) * static_cast<std::size_t>(dz) + static_cast<std::size_t>(k)) * static_cast<std::size_t>(dx) + static_cast<std::size_t>(i)] = static_cast<float>(acc);
            }
        }
    }
}

void FluidVolume3D::mesh_blur(int passes) const {
    const int dx = m_mdim[0], dy = m_mdim[1], dz = m_mdim[2];
    const std::size_t total = static_cast<std::size_t>(dx) * static_cast<std::size_t>(dy) * static_cast<std::size_t>(dz);
    if (total == 0) return;
    m_mscratch.resize(total);
    const std::size_t sz = static_cast<std::size_t>(dx), sy = static_cast<std::size_t>(dx) * static_cast<std::size_t>(dz);
    passes = std::clamp(passes, 0, 6);
    auto blur = [&](std::vector<float>& f) {
        float* src = f.data();
        float* dst = m_mscratch.data();
        for (int j = 0; j < dy; ++j) {
            for (int k = 0; k < dz; ++k) {
                const std::size_t row = static_cast<std::size_t>(j) * sy + static_cast<std::size_t>(k) * sz;
                const float* in = src + row;
                float* out = dst + row;
                out[0] = 0.5f * in[0] + 0.25f * (dx > 1 ? in[1] : 0.0f);
                for (int i = 1; i + 1 < dx; ++i) out[i] = 0.5f * in[i] + 0.25f * (in[i - 1] + in[i + 1]);
                if (dx > 1) out[dx - 1] = 0.5f * in[dx - 1] + 0.25f * in[dx - 2];
            }
        }
        for (int j = 0; j < dy; ++j) {
            for (int k = 0; k < dz; ++k) {
                const std::size_t row = static_cast<std::size_t>(j) * sy + static_cast<std::size_t>(k) * sz;
                const float* in = dst + row;
                float* out = src + row;
                const float* lo = k > 0 ? in - sz : nullptr;
                const float* hi = k + 1 < dz ? in + sz : nullptr;
                for (int i = 0; i < dx; ++i) out[i] = 0.5f * in[i] + 0.25f * ((lo ? lo[i] : 0.0f) + (hi ? hi[i] : 0.0f));
            }
        }
        for (int j = 0; j < dy; ++j) {
            for (int k = 0; k < dz; ++k) {
                const std::size_t row = static_cast<std::size_t>(j) * sy + static_cast<std::size_t>(k) * sz;
                const float* in = src + row;
                float* out = dst + row;
                const float* lo = j > 0 ? in - sy : nullptr;
                const float* hi = j + 1 < dy ? in + sy : nullptr;
                for (int i = 0; i < dx; ++i) out[i] = 0.5f * in[i] + 0.25f * ((lo ? lo[i] : 0.0f) + (hi ? hi[i] : 0.0f));
            }
        }
        f.swap(m_mscratch);
    };
    for (int pass = 0; pass < passes; ++pass) blur(m_mfield);
    if (m_mweight.size() == total && passes > 0) {
        blur(m_mspeed);
        blur(m_mweight);
    }
    const bool close = m_def.surface.close_walls;
    for (int j = 0; j < dy; ++j) {
        for (int k = 0; k < dz; ++k) {
            float* row = m_mfield.data() + static_cast<std::size_t>(j) * sy + static_cast<std::size_t>(k) * sz;
            const int gj = j + m_mlo[1], gk = k + m_mlo[2];
            bool plane = j == 0 || k == 0 || j == dy - 1 || k == dz - 1;
            if (close) plane = plane || gj == 0 || gk == 0 || gj == m_mnodes[1] - 1 || gk == m_mnodes[2] - 1;
            if (plane) {
                for (int i = 0; i < dx; ++i) row[i] = 0.0f;
                continue;
            }
            row[0] = 0.0f;
            row[dx - 1] = 0.0f;
            if (close) {
                if (m_mlo[0] == 0) row[0] = 0.0f;
                const int last = m_mnodes[0] - 1 - m_mlo[0];
                if (last >= 0 && last < dx) row[last] = 0.0f;
            }
        }
    }
}

void FluidVolume3D::mesh_extract(FluidSurfaceMesh3D& out) const {
    const int dx = m_mdim[0], dy = m_mdim[1], dz = m_mdim[2];
    if (dx < 2 || dy < 2 || dz < 2) return;
    const std::size_t total = static_cast<std::size_t>(dx) * static_cast<std::size_t>(dy) * static_cast<std::size_t>(dz);
    if (m_mvid.size() < total * 7) m_mvid.assign(total * 7, -1);
    m_mtouched.clear();
    const float iso = static_cast<float>(m_miso);
    const TetTable& tt = tet_table();
    const std::size_t sy = static_cast<std::size_t>(dx) * static_cast<std::size_t>(dz), sz = static_cast<std::size_t>(dx);
    const bool foam = m_mweight.size() == total;
    const FluidSurfaceSettings3D& ss = m_def.surface;
    const Real h = m_mh;
    auto value = [&](int i, int j, int k) -> float {
        if (i < 0 || j < 0 || k < 0 || i >= dx || j >= dy || k >= dz) return 0.0f;
        return m_mfield[(static_cast<std::size_t>(j) * static_cast<std::size_t>(dz) + static_cast<std::size_t>(k)) * static_cast<std::size_t>(dx) + static_cast<std::size_t>(i)];
    };
    auto gradient = [&](int i, int j, int k) -> V3 { return V3{ static_cast<Real>(value(i + 1, j, k) - value(i - 1, j, k)), static_cast<Real>(value(i, j + 1, k) - value(i, j - 1, k)), static_cast<Real>(value(i, j, k + 1) - value(i, j, k - 1)) }; };
    auto vertex = [&](int i, int j, int k, int lo, int hi) -> std::uint32_t {
        const int ai = i + (lo & 1), aj = j + ((lo >> 1) & 1), ak = k + ((lo >> 2) & 1);
        const int dir = hi ^ lo;
        const std::size_t node = (static_cast<std::size_t>(aj) * static_cast<std::size_t>(dz) + static_cast<std::size_t>(ak)) * static_cast<std::size_t>(dx) + static_cast<std::size_t>(ai);
        const std::size_t key = node * 7 + static_cast<std::size_t>(dir - 1);
        if (m_mvid[key] >= 0) return static_cast<std::uint32_t>(m_mvid[key]);
        const int bi = ai + (dir & 1), bj = aj + ((dir >> 1) & 1), bk = ak + ((dir >> 2) & 1);
        const float fa = value(ai, aj, ak), fb = value(bi, bj, bk);
        const float den = fb - fa;
        Real t = std::abs(den) > 1e-12f ? static_cast<Real>((iso - fa) / den) : 0.5_r;
        t = std::clamp(t, 0.001_r, 0.999_r);
        const V3 ga{ static_cast<Real>(ai + m_mlo[0] - 1), static_cast<Real>(aj + m_mlo[1] - 1), static_cast<Real>(ak + m_mlo[2] - 1) };
        const V3 gb{ static_cast<Real>(bi + m_mlo[0] - 1), static_cast<Real>(bj + m_mlo[1] - 1), static_cast<Real>(bk + m_mlo[2] - 1) };
        V3 l = (ga + (gb - ga) * t) * h;
        l.x = std::clamp(l.x, 0.0_r, m_size.x);
        l.y = std::clamp(l.y, 0.0_r, m_size.y);
        l.z = std::clamp(l.z, 0.0_r, m_size.z);
        const V3 na = gradient(ai, aj, ak), nb = gradient(bi, bj, bk);
        const V3 gn = (na + (nb - na) * t) * -1.0_r;
        const std::uint32_t id = static_cast<std::uint32_t>(out.positions.size());
        out.positions.push_back(to_world(l));
        out.normals.push_back(vec3::normalize(world_dir(gn), m_up));
        Real speed = 0.0_r;
        if (foam) {
            const std::size_t nbi = (static_cast<std::size_t>(bj) * static_cast<std::size_t>(dz) + static_cast<std::size_t>(bk)) * static_cast<std::size_t>(dx) + static_cast<std::size_t>(bi);
            const float wa = m_mweight[node], wb = m_mweight[nbi];
            const Real sa = wa > 1e-6f ? static_cast<Real>(m_mspeed[node] / wa) : -1.0_r;
            const Real sb = wb > 1e-6f ? static_cast<Real>(m_mspeed[nbi] / wb) : -1.0_r;
            if (sa >= 0.0_r && sb >= 0.0_r) speed = sa + (sb - sa) * t;
            else speed = std::max({ sa, sb, 0.0_r });
        }
        out.speeds.push_back(speed);
        out.foam.push_back(std::clamp((speed - ss.foam_speed) / std::max(ss.foam_range, 1e-6_r), 0.0_r, 1.0_r));
        m_mvid[key] = static_cast<std::int32_t>(id);
        m_mtouched.push_back(static_cast<std::uint32_t>(key));
        return id;
    };
    m_minside.resize(total);
    for (std::size_t q = 0; q < total; ++q) m_minside[q] = m_mfield[q] > iso ? 1 : 0;
    const std::uint8_t* ins = m_minside.data();
    for (int j = 0; j + 1 < dy; ++j) {
        for (int k = 0; k + 1 < dz; ++k) {
            const std::size_t row = (static_cast<std::size_t>(j) * static_cast<std::size_t>(dz) + static_cast<std::size_t>(k)) * static_cast<std::size_t>(dx);
            const std::uint8_t* r0 = ins + row;
            const std::uint8_t* r1 = r0 + sz;
            const std::uint8_t* r2 = r0 + sy;
            const std::uint8_t* r3 = r0 + sy + sz;
            int prev = r0[0] | (r2[0] << 1) | (r1[0] << 2) | (r3[0] << 3);
            for (int i = 0; i + 1 < dx; ++i) {
                const int next = r0[i + 1] | (r2[i + 1] << 1) | (r1[i + 1] << 2) | (r3[i + 1] << 3);
                const int both = prev | next;
                const int all = prev & next;
                const int lo = prev, hi = next;
                prev = next;
                if (both == 0 || all == 15) continue;
                const int mask = (lo & 1) | ((hi & 1) << 1) | (((lo >> 1) & 1) << 2) | (((hi >> 1) & 1) << 3) | (((lo >> 2) & 1) << 4) | (((hi >> 2) & 1) << 5) | (((lo >> 3) & 1) << 6) | (((hi >> 3) & 1) << 7);
                if (mask == 0 || mask == 255) continue;
                for (int q = 0; q < 6; ++q) {
                    int m = 0;
                    for (int v = 0; v < 4; ++v) {
                        if (mask & (1 << tt.corners[q][v])) m |= 1 << v;
                    }
                    const int nt = tt.count[q][m];
                    for (int tri = 0; tri < nt; ++tri) {
                        for (int e = 0; e < 3; ++e) out.indices.push_back(vertex(i, j, k, tt.edge[q][m][tri][e][0], tt.edge[q][m][tri][e][1]));
                    }
                }
            }
        }
    }
    for (std::uint32_t key : m_mtouched) m_mvid[key] = -1;
    m_mtouched.clear();
}

void FluidVolume3D::build_surface_mesh(FluidSurfaceMesh3D& out, int detail) const {
    const auto t0 = std::chrono::steady_clock::now();
    out.clear();
    out.detail = detail;
    out.version = m_version;
    const bool particles = m_def.level == FluidLevel::realistic;
    const bool cells = m_def.level == FluidLevel::flowing || m_def.level == FluidLevel::blocky;
    if ((!particles && !cells) || m_size.x <= 0.0_r || m_size.y <= 0.0_r || m_size.z <= 0.0_r) {
        out.cell = 0.0_r;
        out.milliseconds = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
        return;
    }
    const Real h = surface_mesh_cell(detail);
    out.cell = h;
    m_mh = h;
    m_miso = std::clamp(m_def.surface.iso, 0.05_r, 0.95_r);
    const int smoothing = std::clamp(m_def.surface.smoothing, 0, 6);
    const int nodes[3] = { static_cast<int>(std::ceil(m_size.x / h - 1e-4_r)) + 3, static_cast<int>(std::ceil(m_size.y / h - 1e-4_r)) + 3, static_cast<int>(std::ceil(m_size.z / h - 1e-4_r)) + 3 };
    for (int a = 0; a < 3; ++a) m_mnodes[a] = nodes[a];
    Real lo[3] = { 0.0_r, 0.0_r, 0.0_r };
    Real hi[3] = { m_size.x, m_size.y, m_size.z };
    const std::vector<V3>* local = nullptr;
    const std::vector<V3>* vel = nullptr;
    Real inv_rest = 1.0_r;
    bool deep = false;
    if (particles) {
        if (flip_solver()) {
            local = &m_fx;
            vel = &m_fv;
            const Real ratio = h / m_cs;
            inv_rest = 1.0_r / (8.0_r * ratio * ratio * ratio);
            deep = m_deep_count > 0;
            if (!m_box.empty()) {
                for (int a = 0; a < 3; ++a) {
                    lo[a] = static_cast<Real>(m_box.lo[a]) * m_cs;
                    hi[a] = static_cast<Real>(m_box.hi[a] + 1) * m_cs;
                }
            }
        } else {
            m_mlocal.resize(m_px.size());
            for (std::size_t p = 0; p < m_px.size(); ++p) m_mlocal[p] = to_local(m_px[p]);
            local = &m_mlocal;
            vel = &m_pv;
            const Real ratio = h / std::max(m_spacing, 1e-6_r);
            inv_rest = 1.0_r / (ratio * ratio * ratio);
        }
        if (local->empty() && !deep) {
            out.milliseconds = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
            return;
        }
        if (!flip_solver() || m_box.empty()) {
            Real plo[3] = { std::numeric_limits<Real>::max(), std::numeric_limits<Real>::max(), std::numeric_limits<Real>::max() };
            Real phi_[3] = { -std::numeric_limits<Real>::max(), -std::numeric_limits<Real>::max(), -std::numeric_limits<Real>::max() };
            for (const V3& p : *local) {
                const Real c[3] = { p.x, p.y, p.z };
                for (int a = 0; a < 3; ++a) {
                    plo[a] = std::min(plo[a], c[a]);
                    phi_[a] = std::max(phi_[a], c[a]);
                }
            }
            for (int a = 0; a < 3; ++a) {
                lo[a] = plo[a];
                hi[a] = phi_[a];
            }
        }
    }
    const int margin = smoothing + 3;
    for (int a = 0; a < 3; ++a) {
        const int g0 = std::max(0, ifloor(lo[a] / h) + 1 - margin);
        const int g1 = std::min(nodes[a] - 1, static_cast<int>(std::ceil(hi[a] / h)) + 1 + margin);
        m_mlo[a] = g0;
        m_mdim[a] = std::max(0, g1 - g0 + 1);
    }
    if (m_mdim[0] < 2 || m_mdim[1] < 2 || m_mdim[2] < 2) {
        out.milliseconds = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
        return;
    }
    if (particles) {
        if (!m_def.surface.foam) {
            m_mspeed.clear();
            m_mweight.clear();
        }
        mesh_field_particles(*local, vel, inv_rest, deep);
    } else {
        m_mspeed.clear();
        m_mweight.clear();
        mesh_field_cells();
    }
    mesh_blur(smoothing);
    mesh_extract(out);
    if (particles && m_def.surface.spray) {
        const Real inv = 1.0_r / h;
        const int dx = m_mdim[0], dy = m_mdim[1], dz = m_mdim[2];
        const float iso = static_cast<float>(m_miso);
        for (std::size_t p = 0; p < local->size(); ++p) {
            const V3 x = (*local)[p];
            const Real ux = x.x * inv + 1.0_r - static_cast<Real>(m_mlo[0]);
            const Real uy = x.y * inv + 1.0_r - static_cast<Real>(m_mlo[1]);
            const Real uz = x.z * inv + 1.0_r - static_cast<Real>(m_mlo[2]);
            const int i = ifloor(ux), j = ifloor(uy), k = ifloor(uz);
            if (i < 0 || j < 0 || k < 0 || i + 1 >= dx || j + 1 >= dy || k + 1 >= dz) continue;
            const float tx = static_cast<float>(ux - static_cast<Real>(i)), ty = static_cast<float>(uy - static_cast<Real>(j)), tz = static_cast<float>(uz - static_cast<Real>(k));
            float f = 0.0f;
            for (int q = 0; q < 8; ++q) {
                const int a = q & 1, b = (q >> 1) & 1, c = (q >> 2) & 1;
                const float w = (a ? tx : 1.0f - tx) * (b ? ty : 1.0f - ty) * (c ? tz : 1.0f - tz);
                f += w * m_mfield[(static_cast<std::size_t>(j + b) * static_cast<std::size_t>(dz) + static_cast<std::size_t>(k + c)) * static_cast<std::size_t>(dx) + static_cast<std::size_t>(i + a)];
            }
            if (f >= 0.5f * iso) continue;
            out.spray.push_back(to_world(x));
        }
    }
    out.milliseconds = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
}

} // namespace physics
} // namespace fizmo
