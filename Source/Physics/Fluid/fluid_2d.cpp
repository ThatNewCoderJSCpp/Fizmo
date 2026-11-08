#include "fizmo_library.hpp"
#include "fluid_2d.hpp"

#include <chrono>
#include <cstring>

namespace fizmo {
namespace physics {

namespace {

using V2 = RVec2;

constexpr Real kPi = 3.14159265358979323846_r;

Real dot2(const V2& a, const V2& b) noexcept { return a.x * b.x + a.y * b.y; }
Real len2(const V2& v) noexcept { return std::sqrt(v.x * v.x + v.y * v.y); }

V2 closest_on_segment(const V2& p, const V2& a, const V2& b) noexcept {
    const V2 ab = b - a;
    const Real l2 = dot2(ab, ab);
    if (l2 <= 1e-12_r) return a;
    const Real t = std::clamp(dot2(p - a, ab) / l2, 0.0_r, 1.0_r);
    return a + ab * t;
}

std::uint64_t mix(std::uint64_t h, std::uint64_t v) noexcept {
    h ^= v + 0x9e3779b97f4a7c15ULL + (h << 6) + (h >> 2);
    return h;
}

std::uint64_t real_bits(Real v) noexcept {
    const double d = static_cast<double>(v);
    std::uint64_t out = 0;
    std::memcpy(&out, &d, sizeof(out));
    return out;
}

Real aabb_distance(const AABB& b, const V2& p) noexcept {
    const Real dx = std::max({ b.min.x - p.x, 0.0_r, p.x - b.max.x });
    const Real dy = std::max({ b.min.y - p.y, 0.0_r, p.y - b.max.y });
    return std::sqrt(dx * dx + dy * dy);
}

std::uint8_t to_u8(float v) noexcept { return static_cast<std::uint8_t>(std::clamp(v, 0.0f, 1.0f) * 255.0f + 0.5f); }

Real viscosity_factor_of(Real mu) noexcept {
    const Real ratio = std::max(mu, 1e-9_r) / 1.0e-3_r;
    return std::max(1.0_r, 1.0_r + std::log10(ratio));
}

} // namespace

namespace detail {

bool fluid_point_in_shape_2d(const Shape2D& s, const RVec2& p) {
    switch (s.type) {
    case ShapeType::Circle: {
        const V2 d = p - s.circle.center;
        return dot2(d, d) <= s.circle.radius * s.circle.radius;
    }
    case ShapeType::Polygon: {
        if (s.polygon.count < 3) return false;
        for (std::size_t k = 0; k < s.polygon.count; ++k) {
            if (dot2(s.polygon.normals[k], p - s.polygon.vertices[k]) > 0.0_r) return false;
        }
        return true;
    }
    case ShapeType::Capsule: {
        const V2 q = closest_on_segment(p, s.capsule.point1, s.capsule.point2);
        const V2 d = p - q;
        return dot2(d, d) <= s.capsule.radius * s.capsule.radius;
    }
    case ShapeType::Edge:
        return false;
    }
    return false;
}

bool fluid_collider_contact_2d(const Collider2D& c, const Transform2D& body_xf, const RVec2& p, Real r, RVec2& n, Real& depth) {
    const Transform2D xf = body_xf.compose(c.local_offset);
    const V2 lp = xf.apply_inverse(p);
    const Shape2D& s = c.shape;
    V2 ln{ 0.0_r, 1.0_r };
    switch (s.type) {
    case ShapeType::Circle:
    case ShapeType::Capsule:
    case ShapeType::Edge: {
        V2 q{};
        Real rad = 0.0_r;
        if (s.type == ShapeType::Circle) {
            q = s.circle.center;
            rad = s.circle.radius;
        } else if (s.type == ShapeType::Capsule) {
            q = closest_on_segment(lp, s.capsule.point1, s.capsule.point2);
            rad = s.capsule.radius;
        } else {
            q = closest_on_segment(lp, s.edge.v1, s.edge.v2);
        }
        const V2 d = lp - q;
        const Real dist = len2(d);
        if (dist >= rad + r) return false;
        if (dist > 1e-9_r) ln = d * (1.0_r / dist);
        else if (s.type == ShapeType::Edge) ln = s.edge.normal();
        depth = rad + r - dist;
        break;
    }
    case ShapeType::Polygon: {
        const PolygonShape& poly = s.polygon;
        if (poly.count < 3) return false;
        Real sep = -std::numeric_limits<Real>::max();
        std::size_t best = 0;
        for (std::size_t k = 0; k < poly.count; ++k) {
            const Real d = dot2(poly.normals[k], lp - poly.vertices[k]);
            if (d > sep) {
                sep = d;
                best = k;
            }
        }
        if (sep > r) return false;
        if (sep <= 0.0_r) {
            ln = poly.normals[best];
            depth = r - sep;
            break;
        }
        Real bd = std::numeric_limits<Real>::max();
        V2 bq{};
        for (std::size_t k = 0; k < poly.count; ++k) {
            const V2 a = poly.vertices[k];
            const V2 b = poly.vertices[(k + 1) % poly.count];
            const V2 q = closest_on_segment(lp, a, b);
            const V2 d = lp - q;
            const Real dd = dot2(d, d);
            if (dd < bd) {
                bd = dd;
                bq = q;
            }
        }
        const Real dist = std::sqrt(bd);
        if (dist >= r) return false;
        ln = dist > 1e-9_r ? (lp - bq) * (1.0_r / dist) : poly.normals[best];
        depth = r - dist;
        break;
    }
    }
    n = xf.rotate(ln);
    return true;
}

} // namespace detail

const char* to_string(FluidTransfer2D t) noexcept {
    switch (t) {
    case FluidTransfer2D::flip: return "flip";
    case FluidTransfer2D::apic: return "apic";
    }
    return "unknown";
}

std::uint32_t fluid_pack_color(const FluidColor& c) noexcept {
    return static_cast<std::uint32_t>(to_u8(c.r)) | (static_cast<std::uint32_t>(to_u8(c.g)) << 8) | (static_cast<std::uint32_t>(to_u8(c.b)) << 16) | (static_cast<std::uint32_t>(to_u8(c.a)) << 24);
}

FluidColor fluid_unpack_color(std::uint32_t v) noexcept {
    const float k = 1.0f / 255.0f;
    return FluidColor{ static_cast<float>(v & 255u) * k, static_cast<float>((v >> 8) & 255u) * k, static_cast<float>((v >> 16) & 255u) * k, static_cast<float>((v >> 24) & 255u) * k };
}

FluidVolumeDef2D FluidVolumeDef2D::box(FluidLevel level, const AABB& bounds, const FluidMaterial& material, Real fill) {
    FluidVolumeDef2D d;
    d.level = level;
    d.bounds = bounds;
    d.material = material;
    d.fill = fill;
    return d;
}

FluidVolume2D::FluidVolume2D(const FluidVolumeDef2D& def, const RVec2& gravity, const FluidQualitySettings& quality) : m_def(def), m_quality(quality) {
    m_materials.push_back(m_def.material);
    m_rng.s = 0x9e3779b9u ^ (m_def.sand.seed * 2654435761u);
    if (m_rng.s == 0) m_rng.s = 1;
    m_brush_rng.s = m_rng.s ^ 0x5bd1e995u;
    if (m_brush_rng.s == 0) m_brush_rng.s = 7;
    build_frame(gravity);
    refresh_materials();
    switch (m_def.level) {
    case FluidLevel::blocky:
        init_grid();
        init_sand();
        break;
    case FluidLevel::flowing:
        init_grid();
        init_flowing();
        break;
    case FluidLevel::waves:
        init_waves();
        break;
    case FluidLevel::realistic:
        init_particles();
        break;
    case FluidLevel::still:
        m_still_level = (m_def.fill > 0.0_r ? std::min(m_def.fill, 1.0_r) : 1.0_r) * m_size.y;
        break;
    }
}

void FluidVolume2D::build_frame(const RVec2& gravity) {
    m_g = std::max(len2(gravity), 1e-6_r);
    V2 up = m_def.up;
    if (up.x == 0.0_r && up.y == 0.0_r) up = gravity * -1.0_r;
    if (up.x == 0.0_r && up.y == 0.0_r) up = V2{ 0.0_r, 1.0_r };
    if (std::abs(up.y) >= std::abs(up.x)) {
        m_up = V2{ 0.0_r, up.y >= 0.0_r ? 1.0_r : -1.0_r };
        m_u = V2{ 1.0_r, 0.0_r };
    } else {
        m_up = V2{ up.x >= 0.0_r ? 1.0_r : -1.0_r, 0.0_r };
        m_u = V2{ 0.0_r, 1.0_r };
    }
    const AABB& b = m_def.bounds;
    V2 lo{ std::numeric_limits<Real>::max(), std::numeric_limits<Real>::max() };
    V2 hi{ -std::numeric_limits<Real>::max(), -std::numeric_limits<Real>::max() };
    for (int c = 0; c < 4; ++c) {
        const V2 p{ (c & 1) ? b.max.x : b.min.x, (c & 2) ? b.max.y : b.min.y };
        const V2 l{ dot2(p, m_u), dot2(p, m_up) };
        lo = V2{ std::min(lo.x, l.x), std::min(lo.y, l.y) };
        hi = V2{ std::max(hi.x, l.x), std::max(hi.y, l.y) };
    }
    m_o = m_u * lo.x + m_up * lo.y;
    m_size = hi - lo;
    m_world_bounds = b;
    m_gl = local_dir(gravity);
}

void FluidVolume2D::init_grid() {
    if (m_def.level == FluidLevel::realistic) {
        const Real scale = std::max(m_quality.resolution_scale, 0.1_r);
        m_cs = m_def.particles.radius > 0.0_r ? m_def.particles.radius / 0.3_r : std::max(m_def.cell_size * scale, 1e-4_r);
    } else {
        m_cs = std::max(m_def.cell_size, 1e-4_r);
    }
    m_nx = std::max(1, static_cast<int>(std::ceil(m_size.x / m_cs - 1e-3_r)));
    m_ny = std::max(1, static_cast<int>(std::ceil(m_size.y / m_cs - 1e-3_r)));
    const std::size_t n = static_cast<std::size_t>(m_nx) * static_cast<std::size_t>(m_ny);
    m_solid.assign(n, 0);
    m_occ.assign(n, 0);
    m_occ_list.clear();
    m_occ_list.reserve(256);
}

void FluidVolume2D::refresh_materials() {
    const std::size_t n = m_materials.size();
    m_mat_rho.resize(n);
    m_mat_mu.resize(n);
    m_mat_visc.resize(n);
    m_mat_flip.resize(n);
    m_mat_cohesion.resize(n);
    m_mat_friction.resize(n);
    m_mat_restitution.resize(n);
    m_mat_color.resize(n);
    m_any_viscous = false;
    m_any_cohesion = false;
    const FluidParticleSettings2D& ps = m_def.particles;
    for (std::size_t i = 0; i < n; ++i) {
        const FluidMaterial& m = m_materials[i];
        const Real f = m.viscosity_factor() - 1.0_r;
        m_mat_rho[i] = std::max(m.density, 1e-3_r);
        m_mat_mu[i] = m.viscosity;
        const Real nu = m.kinematic_viscosity() / std::max(m_cs * m_cs, 1e-12_r);
        m_mat_visc[i] = std::max(0.0_r, ps.viscosity_scale) * (1.8_r * f * f * f + nu);
        if (m_mat_visc[i] > 0.5_r) m_any_viscous = true;
        m_mat_flip[i] = std::clamp(ps.flip_ratio * (1.0_r - 0.12_r * f), 0.0_r, 1.0_r);
        m_mat_cohesion[i] = ps.cohesion >= 0.0_r ? ps.cohesion : std::clamp(m.surface_tension / 0.072_r * 0.02_r, 0.0_r, 0.25_r);
        if (m_mat_cohesion[i] > 0.0_r) m_any_cohesion = true;
        m_mat_friction[i] = m.friction;
        m_mat_restitution[i] = m.restitution;
        m_mat_color[i] = fluid_pack_color(m.color);
    }
}

void FluidVolume2D::set_material(const FluidMaterial& m) {
    m_def.material = m;
    m_materials[0] = m;
    refresh_materials();
    wake();
}

void FluidVolume2D::set_material(std::uint8_t id, const FluidMaterial& m) {
    if (id >= m_materials.size()) return;
    m_materials[id] = m;
    if (id == 0) m_def.material = m;
    refresh_materials();
    wake();
}

std::uint8_t FluidVolume2D::add_material(const FluidMaterial& m) {
    if (m_materials.size() >= 255) return 0;
    m_materials.push_back(m);
    refresh_materials();
    return static_cast<std::uint8_t>(m_materials.size() - 1);
}

V2 FluidVolume2D::to_local(const V2& world) const noexcept {
    const V2 d = world - m_o;
    return V2{ dot2(d, m_u), dot2(d, m_up) };
}

V2 FluidVolume2D::to_world(const V2& l) const noexcept { return m_o + m_u * l.x + m_up * l.y; }

V2 FluidVolume2D::local_dir(const V2& d) const noexcept { return V2{ dot2(d, m_u), dot2(d, m_up) }; }

V2 FluidVolume2D::world_dir(const V2& l) const noexcept { return m_u * l.x + m_up * l.y; }

void FluidVolume2D::set_still_level(Real height_along_up) {
    m_still_level = std::clamp(height_along_up - dot2(m_o, m_up), 0.0_r, m_size.y);
}

bool FluidVolume2D::cell_of(const V2& world, int& i, int& j) const noexcept {
    if (m_nx <= 0) return false;
    const V2 l = to_local(world);
    i = static_cast<int>(std::floor(l.x / m_cs));
    j = static_cast<int>(std::floor(l.y / m_cs));
    return valid_cell(i, j);
}

V2 FluidVolume2D::cell_center(int i, int j) const noexcept { return to_world(V2{ (static_cast<Real>(i) + 0.5_r) * m_cs, (static_cast<Real>(j) + 0.5_r) * m_cs }); }

bool FluidVolume2D::static_solid(int i, int j) const noexcept {
    if (!valid_cell(i, j)) return m_def.level != FluidLevel::realistic || m_def.particles.contain;
    const std::size_t c = cell_index(i, j);
    if (m_def.level == FluidLevel::blocky) return sand_cell_info(static_cast<SandCell2D>(m_sand[c])).kind == SandKind2D::solid;
    return m_solid[c] != 0;
}

bool FluidVolume2D::any_solid(int i, int j) const noexcept {
    if (!valid_cell(i, j)) return static_solid(i, j);
    const std::size_t c = cell_index(i, j);
    return m_occ[c] != 0 || static_solid(i, j);
}

bool FluidVolume2D::solid(int i, int j) const noexcept {
    if (!valid_cell(i, j) || m_solid.empty()) return false;
    return static_solid(i, j);
}

void FluidVolume2D::set_solid(int i, int j, bool s) {
    if (!valid_cell(i, j)) return;
    if (m_def.level == FluidLevel::blocky) {
        if (s) set_sand_cell(i, j, SandCell2D::wall);
        else if (sand_cell_info(sand_cell(i, j)).kind == SandKind2D::solid) set_sand_cell(i, j, SandCell2D::empty);
        return;
    }
    const std::size_t c = cell_index(i, j);
    const std::uint8_t v = s ? 1 : 0;
    if (m_solid[c] == v) return;
    m_solid[c] = v;
    m_solids_changed = true;
    if (s && !m_fill.empty()) {
        m_fill[c] = 0.0_r;
        m_source[c] = 0;
    }
    if (m_def.level == FluidLevel::flowing) mark_flow_dirty(i, j);
    m_asleep = false;
    m_calm_frames = 0;
}

void FluidVolume2D::set_solid_box(const AABB& box, bool s) {
    if (m_nx <= 0) return;
    const V2 a = to_local(box.min), b = to_local(box.max);
    const V2 lo{ std::min(a.x, b.x), std::min(a.y, b.y) }, hi{ std::max(a.x, b.x), std::max(a.y, b.y) };
    const int i0 = std::max(0, static_cast<int>(std::floor(lo.x / m_cs + 1e-4_r)));
    const int j0 = std::max(0, static_cast<int>(std::floor(lo.y / m_cs + 1e-4_r)));
    const int i1 = std::min(m_nx - 1, static_cast<int>(std::ceil(hi.x / m_cs - 1e-4_r)) - 1);
    const int j1 = std::min(m_ny - 1, static_cast<int>(std::ceil(hi.y / m_cs - 1e-4_r)) - 1);
    for (int j = j0; j <= j1; ++j) {
        for (int i = i0; i <= i1; ++i) set_solid(i, j, s);
    }
}

void FluidVolume2D::paint_solid(const V2& center, Real radius, bool s) {
    if (m_nx <= 0) return;
    const V2 l = to_local(center);
    const int i0 = std::max(0, static_cast<int>(std::floor((l.x - radius) / m_cs)));
    const int j0 = std::max(0, static_cast<int>(std::floor((l.y - radius) / m_cs)));
    const int i1 = std::min(m_nx - 1, static_cast<int>(std::floor((l.x + radius) / m_cs)));
    const int j1 = std::min(m_ny - 1, static_cast<int>(std::floor((l.y + radius) / m_cs)));
    const Real r2 = radius * radius;
    for (int j = j0; j <= j1; ++j) {
        for (int i = i0; i <= i1; ++i) {
            const Real dx = (static_cast<Real>(i) + 0.5_r) * m_cs - l.x;
            const Real dy = (static_cast<Real>(j) + 0.5_r) * m_cs - l.y;
            if (dx * dx + dy * dy <= r2) set_solid(i, j, s);
        }
    }
}

void FluidVolume2D::voxelize(Simulation2D& sim, bool static_only) {
    if (m_nx <= 0) return;
    for (const RigidBody2D& b : sim.bodies()) {
        if (static_only && (b.inv_mass() > 0.0_r || b.linear_velocity().x != 0.0_r || b.linear_velocity().y != 0.0_r || b.angular_velocity() != 0.0_r)) continue;
        for (const Collider2D& c : b.colliders()) {
            if (c.is_sensor) continue;
            const V2 a = to_local(c.world_aabb.min), d = to_local(c.world_aabb.max);
            const int i0 = std::max(0, static_cast<int>(std::floor(std::min(a.x, d.x) / m_cs)));
            const int j0 = std::max(0, static_cast<int>(std::floor(std::min(a.y, d.y) / m_cs)));
            const int i1 = std::min(m_nx - 1, static_cast<int>(std::floor(std::max(a.x, d.x) / m_cs)));
            const int j1 = std::min(m_ny - 1, static_cast<int>(std::floor(std::max(a.y, d.y) / m_cs)));
            const Transform2D xf = b.transform().compose(c.local_offset);
            for (int j = j0; j <= j1; ++j) {
                for (int i = i0; i <= i1; ++i) {
                    const V2 w = cell_center(i, j);
                    const V2 lp = xf.apply_inverse(w);
                    bool in = detail::fluid_point_in_shape_2d(c.shape, lp);
                    if (!in && c.shape.type == ShapeType::Edge) {
                        const V2 q = closest_on_segment(lp, c.shape.edge.v1, c.shape.edge.v2);
                        in = len2(lp - q) <= 0.5_r * m_cs;
                    }
                    if (in) set_solid(i, j, true);
                }
            }
        }
    }
}

std::size_t FluidVolume2D::solid_rectangles(std::vector<AABB>& out) const {
    out.clear();
    if (m_nx <= 0) return 0;
    std::vector<std::uint8_t> used(static_cast<std::size_t>(m_nx) * static_cast<std::size_t>(m_ny), 0);
    for (int j = 0; j < m_ny; ++j) {
        for (int i = 0; i < m_nx; ++i) {
            const std::size_t c = cell_index(i, j);
            if (used[c] || !static_solid(i, j)) continue;
            int i1 = i;
            while (i1 + 1 < m_nx && !used[cell_index(i1 + 1, j)] && static_solid(i1 + 1, j)) ++i1;
            int j1 = j;
            for (;;) {
                if (j1 + 1 >= m_ny) break;
                bool ok = true;
                for (int x = i; x <= i1 && ok; ++x) ok = !used[cell_index(x, j1 + 1)] && static_solid(x, j1 + 1);
                if (!ok) break;
                ++j1;
            }
            for (int y = j; y <= j1; ++y) {
                for (int x = i; x <= i1; ++x) used[cell_index(x, y)] = 1;
            }
            const V2 a = to_world(V2{ static_cast<Real>(i) * m_cs, static_cast<Real>(j) * m_cs });
            const V2 b = to_world(V2{ static_cast<Real>(i1 + 1) * m_cs, static_cast<Real>(j1 + 1) * m_cs });
            out.push_back(AABB(V2{ std::min(a.x, b.x), std::min(a.y, b.y) }, V2{ std::max(a.x, b.x), std::max(a.y, b.y) }));
        }
    }
    return out.size();
}

RigidBody2D& FluidVolume2D::build_static_body(Simulation2D& sim, RigidBody2D* reuse, const Material& material) {
    RigidBody2D& body = reuse ? *reuse : sim.add_body(V2{ 0.0_r, 0.0_r }, PhysicsFlags::DefaultStatic);
    body.colliders_mut().clear();
    std::vector<AABB> rects;
    solid_rectangles(rects);
    for (const AABB& r : rects) {
        const V2 c = body.get_local_point(r.center());
        const V2 e = r.extents();
        body.add_collider(Shape2D::make_box(e.x, e.y), material, Transform2D(c, -body.angle()));
    }
    body.mark_mass_dirty();
    body.synchronize_colliders();
    return body;
}

void FluidVolume2D::set_source(int i, int j, bool s) {
    if (!valid_cell(i, j) || m_source.empty()) return;
    const std::size_t c = cell_index(i, j);
    m_source[c] = s ? 1 : 0;
    if (s && !m_solid[c]) m_fill[c] = std::max(m_fill[c], m_def.flowing.max_mass);
    mark_flow_dirty(i, j);
}

bool FluidVolume2D::source(int i, int j) const noexcept { return valid_cell(i, j) && !m_source.empty() && m_source[cell_index(i, j)] != 0; }

void FluidVolume2D::set_amount(int i, int j, Real a) {
    if (!valid_cell(i, j) || m_fill.empty()) return;
    const std::size_t c = cell_index(i, j);
    if (m_solid[c]) return;
    m_fill[c] = std::max(0.0_r, a);
    mark_flow_dirty(i, j);
}

Real FluidVolume2D::amount(int i, int j) const noexcept { return valid_cell(i, j) && !m_fill.empty() ? m_fill[cell_index(i, j)] : 0.0_r; }

void FluidVolume2D::fill_box(const AABB& box, Real a) {
    if (m_def.level == FluidLevel::realistic) {
        add_particle_box(box);
        return;
    }
    if (m_def.level == FluidLevel::blocky) {
        paint_cells_box(box, m_def.sand.fill_cell);
        return;
    }
    if (m_fill.empty()) return;
    const V2 p = to_local(box.min), q = to_local(box.max);
    const V2 lo{ std::min(p.x, q.x), std::min(p.y, q.y) }, hi{ std::max(p.x, q.x), std::max(p.y, q.y) };
    for (int j = 0; j < m_ny; ++j) {
        for (int i = 0; i < m_nx; ++i) {
            const Real cx = (static_cast<Real>(i) + 0.5_r) * m_cs, cy = (static_cast<Real>(j) + 0.5_r) * m_cs;
            if (cx >= lo.x && cx <= hi.x && cy >= lo.y && cy <= hi.y) set_amount(i, j, a);
        }
    }
}

Real FluidVolume2D::cell_frac(int i, int j) const noexcept {
    if (!valid_cell(i, j)) return 0.0_r;
    const std::size_t c = cell_index(i, j);
    switch (m_def.level) {
    case FluidLevel::flowing: return m_solid[c] ? 0.0_r : std::min(m_fill[c], 1.0_r);
    case FluidLevel::blocky: {
        const SandKind2D k = sand_cell_info(static_cast<SandCell2D>(m_sand[c])).kind;
        return (k == SandKind2D::liquid || k == SandKind2D::powder) ? 1.0_r : 0.0_r;
    }
    case FluidLevel::realistic: return m_ctype.empty() || m_ctype[c] != 1 ? 0.0_r : std::clamp(m_cdens[c] / std::max(m_rest, 1e-6_r), 0.0_r, 1.0_r);
    default: return 0.0_r;
    }
}

Real FluidVolume2D::cell_rho(int i, int j) const noexcept {
    if (!valid_cell(i, j)) return m_materials[0].density;
    const std::size_t c = cell_index(i, j);
    switch (m_def.level) {
    case FluidLevel::blocky: {
        const SandCellInfo2D& info = sand_cell_info(static_cast<SandCell2D>(m_sand[c]));
        return (info.kind == SandKind2D::liquid || info.kind == SandKind2D::powder) ? info.density : 0.0_r;
    }
    case FluidLevel::realistic: return m_crho.empty() ? m_materials[0].density : m_crho[c];
    default: return m_materials[0].density;
    }
}

Real FluidVolume2D::cell_mu(int i, int j) const noexcept {
    if (!valid_cell(i, j)) return m_materials[0].viscosity;
    const std::size_t c = cell_index(i, j);
    switch (m_def.level) {
    case FluidLevel::blocky: {
        const SandCellInfo2D& info = sand_cell_info(static_cast<SandCell2D>(m_sand[c]));
        return info.viscosity;
    }
    case FluidLevel::realistic: return m_cmu.empty() ? m_materials[0].viscosity : m_cmu[c];
    default: return m_materials[0].viscosity;
    }
}

V2 FluidVolume2D::cell_flow(int i, int j) const noexcept {
    if (!valid_cell(i, j)) return V2{};
    switch (m_def.level) {
    case FluidLevel::flowing: return m_cell_vel[cell_index(i, j)];
    case FluidLevel::realistic: {
        if (m_gu.empty()) return V2{};
        const std::size_t nu = static_cast<std::size_t>(m_nx + 1);
        const std::size_t ju = static_cast<std::size_t>(j) * nu + static_cast<std::size_t>(i);
        const std::size_t jv = static_cast<std::size_t>(j) * static_cast<std::size_t>(m_nx) + static_cast<std::size_t>(i);
        return V2{ 0.5_r * (m_gu[ju] + m_gu[ju + 1]), 0.5_r * (m_gv[jv] + m_gv[jv + static_cast<std::size_t>(m_nx)]) };
    }
    default: return V2{};
    }
}

bool FluidVolume2D::cell_blocked(int i, int j) const noexcept {
    if (!valid_cell(i, j)) return true;
    if (m_def.level == FluidLevel::blocky) return sand_cell_info(static_cast<SandCell2D>(m_sand[cell_index(i, j)])).kind == SandKind2D::solid || m_occ[cell_index(i, j)] != 0;
    return any_solid(i, j);
}

bool FluidVolume2D::scan_column(int i, int j0, Real& level) const noexcept {
    if (i < 0 || i >= m_nx) return false;
    int j = std::clamp(j0, 0, m_ny - 1);
    if (cell_blocked(i, j)) return false;
    if (cell_frac(i, j) < 0.5_r) {
        if (j > 0 && !cell_blocked(i, j - 1) && cell_frac(i, j - 1) >= 0.5_r) {
            --j;
        } else {
            return false;
        }
    }
    while (j + 1 < m_ny && !cell_blocked(i, j + 1) && cell_frac(i, j + 1) >= 0.5_r) ++j;
    const Real fj = cell_frac(i, j);
    Real top = static_cast<Real>(j) * m_cs + fj * m_cs;
    if (fj >= 0.95_r && j + 1 < m_ny && !cell_blocked(i, j + 1)) top += cell_frac(i, j + 1) * m_cs;
    level = top;
    return true;
}

FluidVolume2D::Ambient FluidVolume2D::ambient_for(const V2& lo, const V2& hi) const noexcept {
    Ambient a;
    if (m_nx <= 0) return a;
    const int i0 = static_cast<int>(std::floor(lo.x / m_cs));
    const int i1 = static_cast<int>(std::floor(hi.x / m_cs));
    const int jb = std::clamp(static_cast<int>(std::floor(lo.y / m_cs)), 0, m_ny - 1);
    Real sum = 0.0_r;
    int count = 0;
    for (int k = 1; k <= 4; ++k) {
        const int i = i0 - k;
        if (i < 0) break;
        Real l = 0.0_r;
        if (scan_column(i, jb, l)) {
            a.left = i;
            sum += l;
            ++count;
            break;
        }
        if (!cell_blocked(i, jb)) break;
    }
    for (int k = 1; k <= 4; ++k) {
        const int i = i1 + k;
        if (i >= m_nx) break;
        Real l = 0.0_r;
        if (scan_column(i, jb, l)) {
            a.right = i;
            sum += l;
            ++count;
            break;
        }
        if (!cell_blocked(i, jb)) break;
    }
    if (count == 0) {
        const int ic = std::clamp((i0 + i1) / 2, 0, m_nx - 1);
        const int jt = static_cast<int>(std::floor(hi.y / m_cs)) + 1;
        Real l = 0.0_r;
        if (jt >= 0 && jt < m_ny && cell_frac(ic, jt) >= 0.5_r && scan_column(ic, jt, l)) {
            a.left = ic;
            sum = l;
            count = 1;
        }
    }
    if (count == 0) return a;
    a.valid = true;
    a.level = sum / static_cast<Real>(count);
    return a;
}

Real FluidVolume2D::surface_height(Real a) const {
    const V2 c = m_world_bounds.center();
    const V2 p = std::abs(m_up.y) > 0.5_r ? V2{ a, c.y } : V2{ c.x, a };
    return surface_height(p);
}

Real FluidVolume2D::surface_height(const V2& point) const {
    const V2 s = surface_point(point);
    return dot2(s, m_up);
}

V2 FluidVolume2D::surface_point(const V2& point) const {
    V2 l = to_local(point);
    Real top = 0.0_r;
    switch (m_def.level) {
    case FluidLevel::still: {
        top = m_still_level;
        if (m_def.still.bob_amplitude > 0.0_r) {
            const Real k = 2.0_r * kPi / std::max(m_def.still.bob_wavelength, 1e-3_r);
            const Real om = 2.0_r * kPi / std::max(m_def.still.bob_period, 1e-3_r);
            top += m_def.still.bob_amplitude * std::sin(om * m_time + k * l.x);
        }
        break;
    }
    case FluidLevel::waves:
        top = sample_columns(l.x, heights());
        break;
    case FluidLevel::blocky:
    case FluidLevel::flowing:
    case FluidLevel::realistic: {
        if (m_nx <= 0) break;
        const int i = std::clamp(static_cast<int>(std::floor(l.x / m_cs)), 0, m_nx - 1);
        int j = std::clamp(static_cast<int>(std::floor(l.y / m_cs)), 0, m_ny - 1);
        auto liquid = [&](int x, int y) {
            if (m_def.level != FluidLevel::blocky) return cell_frac(x, y);
            if (!valid_cell(x, y)) return 0.0_r;
            return sand_cell_info(static_cast<SandCell2D>(m_sand[cell_index(x, y)])).kind == SandKind2D::liquid ? 1.0_r : 0.0_r;
        };
        if (liquid(i, j) >= 0.5_r) {
            while (j + 1 < m_ny && liquid(i, j + 1) >= 0.5_r) ++j;
            const Real fj = liquid(i, j);
            top = static_cast<Real>(j) * m_cs + fj * m_cs;
            if (fj >= 0.95_r && j + 1 < m_ny) top += liquid(i, j + 1) * m_cs;
        } else {
            int k = j;
            while (k >= 0 && liquid(i, k) < 0.5_r && !static_solid(i, k)) --k;
            if (k >= 0 && liquid(i, k) >= 0.5_r) {
                const Real fk = liquid(i, k);
                top = static_cast<Real>(k) * m_cs + fk * m_cs;
                if (fk >= 0.95_r && k + 1 < m_ny) top += liquid(i, k + 1) * m_cs;
            } else {
                top = (k < 0 ? liquid(i, 0) * m_cs : static_cast<Real>(k + 1) * m_cs);
            }
        }
        break;
    }
    }
    l.y = top;
    return to_world(l);
}

V2 FluidVolume2D::surface_normal(const V2& point) const {
    const Real e = std::max(0.02_r, (m_def.level == FluidLevel::waves ? m_dx : m_cs));
    const Real h0 = surface_height(point);
    const Real h1 = surface_height(point + m_u * e);
    const V2 n = world_dir(V2{ -(h1 - h0) / e, 1.0_r });
    const Real l = len2(n);
    return l > 1e-9_r ? n * (1.0_r / l) : m_up;
}

Real FluidVolume2D::depth_at(const V2& point) const {
    const V2 l = to_local(point);
    if (l.x < 0.0_r || l.x > m_size.x || l.y < 0.0_r || l.y > m_size.y + m_size.y) return -std::numeric_limits<Real>::max();
    const V2 s = surface_point(point);
    return dot2(s - point, m_up);
}

bool FluidVolume2D::inside(const V2& point) const { return depth_at(point) > 0.0_r; }

Real FluidVolume2D::density_at(const V2& point) const {
    if (!inside(point)) return 0.0_r;
    if (m_def.level == FluidLevel::realistic || m_def.level == FluidLevel::blocky) {
        int i = 0, j = 0;
        if (!cell_of(point, i, j)) return 0.0_r;
        return cell_frac(i, j) > 0.0_r ? cell_rho(i, j) : 0.0_r;
    }
    return m_materials[0].density;
}

V2 FluidVolume2D::velocity_at(const V2& point) const {
    V2 v = m_def.current;
    const V2 l = to_local(point);
    switch (m_def.level) {
    case FluidLevel::still:
    case FluidLevel::blocky:
        break;
    case FluidLevel::waves: {
        const Real vu = sample_columns(l.x, m_col_vel);
        v = v + world_dir(V2{ vu + swell_velocity(l.x, l.y), 0.0_r });
        break;
    }
    case FluidLevel::flowing: {
        int i = 0, j = 0;
        if (cell_of(point, i, j)) v = v + world_dir(m_cell_vel[cell_index(i, j)]);
        break;
    }
    case FluidLevel::realistic:
        if (!m_gu.empty()) v = v + world_dir(V2{ sample_grid_u(l), sample_grid_v(l) });
        break;
    }
    return v;
}

Real FluidVolume2D::total_volume() const {
    switch (m_def.level) {
    case FluidLevel::still: return m_size.x * m_still_level;
    case FluidLevel::waves: {
        double s = 0.0;
        for (Real d : m_depth) s += static_cast<double>(d);
        return static_cast<Real>(s * static_cast<double>(m_dx));
    }
    case FluidLevel::flowing: {
        double s = 0.0;
        for (Real f : m_fill) s += static_cast<double>(f);
        return static_cast<Real>(s * static_cast<double>(m_cs * m_cs));
    }
    case FluidLevel::blocky: {
        std::size_t n = 0;
        for (std::uint8_t c : m_sand) {
            if (sand_cell_info(static_cast<SandCell2D>(c)).kind == SandKind2D::liquid) ++n;
        }
        return static_cast<Real>(n) * m_cs * m_cs;
    }
    case FluidLevel::realistic: return static_cast<Real>(m_px.size()) * m_spacing * m_spacing;
    }
    return 0.0_r;
}

void FluidVolume2D::wake() noexcept {
    m_all_dirty = true;
    m_heights_dirty = true;
    m_asleep = false;
    m_calm_frames = 0;
    std::fill(m_flow_changed.begin(), m_flow_changed.end(), std::uint8_t{ 1 });
    std::fill(m_tile_active.begin(), m_tile_active.end(), std::uint8_t{ 1 });
    std::fill(m_tile_calm.begin(), m_tile_calm.end(), std::uint16_t{ 0 });
    if (m_def.level == FluidLevel::blocky) {
        for (int cy = 0; cy < m_ky; ++cy) {
            for (int cx = 0; cx < m_kx; ++cx) {
                SandRect& r = m_rect_next[static_cast<std::size_t>(cy * m_kx + cx)];
                r.add(cx * kSandChunk, cy * kSandChunk, std::min(m_nx, (cx + 1) * kSandChunk) - 1, std::min(m_ny, (cy + 1) * kSandChunk) - 1);
            }
        }
    }
}

bool FluidVolume2D::sleeping() const noexcept {
    switch (m_def.level) {
    case FluidLevel::still: return m_def.still.bob_amplitude <= 0.0_r;
    case FluidLevel::realistic: return !m_px.empty() && m_asleep;
    default: return !m_all_dirty && active_regions() == 0;
    }
}

std::size_t FluidVolume2D::active_regions() const noexcept {
    switch (m_def.level) {
    case FluidLevel::blocky: return m_sand_active;
    case FluidLevel::flowing: return m_flow_list.size();
    case FluidLevel::waves: {
        std::size_t n = 0;
        for (std::uint8_t t : m_tile_active) n += t ? 1u : 0u;
        return n;
    }
    case FluidLevel::realistic: return awake_particles();
    case FluidLevel::still: return 0;
    }
    return 0;
}

std::size_t FluidVolume2D::total_regions() const noexcept {
    switch (m_def.level) {
    case FluidLevel::blocky: return m_rect_now.size();
    case FluidLevel::flowing: return m_flow_active.size();
    case FluidLevel::waves: return m_tile_active.size();
    case FluidLevel::realistic: return m_px.size();
    case FluidLevel::still: return 1;
    }
    return 0;
}

void FluidVolume2D::step_level(detail::FluidTaskPool& pool, Real dt, FluidStats2D& stats) {
    if (dt <= 0.0_r) return;
    m_last_dt = dt;
    m_time += dt;
    switch (m_def.level) {
    case FluidLevel::still: break;
    case FluidLevel::blocky: step_sand(pool, dt, stats); break;
    case FluidLevel::flowing: step_flowing(pool, dt, stats); break;
    case FluidLevel::waves: step_waves(pool, dt, stats); break;
    case FluidLevel::realistic: step_particles(pool, dt, stats); break;
    }
    m_heights_dirty = true;
}

void FluidVolume2D::gather_bodies(Simulation2D& sim) {
    m_near.clear();
    const AABB area = m_world_bounds.fatten(std::max(m_cs, 0.05_r));
    const Real wake2 = m_def.particles.wake_speed * m_def.particles.wake_speed;
    for (RigidBody2D& b : sim.bodies()) {
        if (!b.has(PhysicsFlags::Enabled) || b.colliders().empty()) continue;
        if ((b.collision_layer() & m_def.coupling.layers) == 0) continue;
        NearBody nb;
        nb.body = &b;
        bool first = true;
        for (const Collider2D& c : b.colliders()) {
            if (c.is_sensor) continue;
            nb.box = first ? c.world_aabb : AABB::merge(nb.box, c.world_aabb);
            first = false;
        }
        if (first || !nb.box.overlaps(area)) continue;
        nb.dynamic = b.inv_mass() > 0.0_r;
        const V2 v = b.linear_velocity();
        const Real r = std::max(nb.box.extents().x, nb.box.extents().y);
        const Real sp2 = dot2(v, v) + b.angular_velocity() * b.angular_velocity() * r * r;
        if (sp2 > wake2 && (nb.dynamic || b.has(PhysicsFlags::LinearVelocity))) {
            m_asleep = false;
            m_calm_frames = 0;
        }
        m_near.push_back(nb);
    }
}

void FluidVolume2D::clear_occupancy() {
    for (std::uint32_t c : m_occ_list) m_occ[c] = 0;
    m_occ_list.clear();
}

void FluidVolume2D::rasterize_bodies() {
    if (m_nx <= 0 || m_occ.empty()) return;
    std::uint64_t before = 0;
    for (std::uint32_t c : m_occ_list) before = mix(before, c);
    const std::size_t old_count = m_occ_list.size();
    if (m_def.level == FluidLevel::blocky) {
        for (std::uint32_t c : m_occ_list) sand_mark(static_cast<int>(c % static_cast<std::uint32_t>(m_nx)), static_cast<int>(c / static_cast<std::uint32_t>(m_nx)));
    }
    clear_occupancy();
    if (m_def.coupling.push_fluid || m_def.level == FluidLevel::realistic) {
        const bool want_vel = m_def.level == FluidLevel::realistic;
        if (want_vel && m_occ_vel.size() != m_occ.size()) m_occ_vel.assign(m_occ.size(), V2{});
        for (const NearBody& nb : m_near) {
            const RigidBody2D& b = *nb.body;
            for (const Collider2D& c : b.colliders()) {
                if (c.is_sensor) continue;
                const V2 a = to_local(c.world_aabb.min), d = to_local(c.world_aabb.max);
                const int i0 = std::max(0, static_cast<int>(std::floor(std::min(a.x, d.x) / m_cs)));
                const int j0 = std::max(0, static_cast<int>(std::floor(std::min(a.y, d.y) / m_cs)));
                const int i1 = std::min(m_nx - 1, static_cast<int>(std::floor(std::max(a.x, d.x) / m_cs)));
                const int j1 = std::min(m_ny - 1, static_cast<int>(std::floor(std::max(a.y, d.y) / m_cs)));
                if (i0 > i1 || j0 > j1) continue;
                const Transform2D xf = b.transform().compose(c.local_offset);
                for (int j = j0; j <= j1; ++j) {
                    for (int i = i0; i <= i1; ++i) {
                        const std::size_t ci = cell_index(i, j);
                        if (m_occ[ci]) continue;
                        const V2 w = cell_center(i, j);
                        const V2 lp = xf.apply_inverse(w);
                        bool in = detail::fluid_point_in_shape_2d(c.shape, lp);
                        if (!in && c.shape.type == ShapeType::Edge) {
                            const V2 q = closest_on_segment(lp, c.shape.edge.v1, c.shape.edge.v2);
                            in = len2(lp - q) <= 0.5_r * m_cs;
                        }
                        if (!in) continue;
                        m_occ[ci] = 1;
                        m_occ_list.push_back(static_cast<std::uint32_t>(ci));
                        if (want_vel) m_occ_vel[ci] = local_dir(b.get_velocity_at(w));
                    }
                }
            }
        }
    }
    std::uint64_t after = 0;
    for (std::uint32_t c : m_occ_list) after = mix(after, c);
    const bool changed = before != after || old_count != m_occ_list.size();
    if (!changed) return;
    if (m_def.level == FluidLevel::blocky) {
        for (std::uint32_t c : m_occ_list) sand_mark(static_cast<int>(c % static_cast<std::uint32_t>(m_nx)), static_cast<int>(c / static_cast<std::uint32_t>(m_nx)));
    } else if (m_def.level == FluidLevel::flowing) {
        for (std::uint32_t c : m_occ_list) mark_flow_dirty(static_cast<int>(c % static_cast<std::uint32_t>(m_nx)), static_cast<int>(c / static_cast<std::uint32_t>(m_nx)));
    } else if (m_def.level == FluidLevel::realistic) {
        m_asleep = false;
        m_calm_frames = 0;
    }
}

const FluidVolume2D::BodyCache& FluidVolume2D::body_cache(const RigidBody2D& body, int samples) {
    std::uint64_t sig = mix(0, static_cast<std::uint64_t>(samples));
    for (const Collider2D& c : body.colliders()) {
        sig = mix(sig, static_cast<std::uint64_t>(c.shape.type));
        sig = mix(sig, real_bits(c.shape.bounding_radius));
        sig = mix(sig, real_bits(c.local_offset.position.x + c.local_offset.position.y * 3.0_r + c.local_offset.angle * 7.0_r));
        sig = mix(sig, c.is_sensor ? 1u : 0u);
    }
    BodyCache& cache = m_cache[body.handle().packed()];
    cache.seen = m_frame;
    if (cache.signature == sig && !cache.collider_start.empty()) return cache;
    cache.signature = sig;
    cache.points.clear();
    cache.collider_start.clear();
    cache.area = 0.0_r;
    cache.spacing = std::numeric_limits<Real>::max();
    const int n = std::max(3, samples * 2);
    for (const Collider2D& c : body.colliders()) {
        cache.collider_start.push_back(static_cast<std::uint32_t>(cache.points.size()));
        const Shape2D& s = c.shape;
        if (c.is_sensor || s.type == ShapeType::Edge) continue;
        if (s.type == ShapeType::Circle) {
            cache.area += kPi * s.circle.radius * s.circle.radius;
            cache.spacing = std::min(cache.spacing, s.circle.radius);
            continue;
        }
        const AABB box = s.local_aabb;
        const V2 ext = box.max - box.min;
        const Real longest = std::max({ ext.x, ext.y, 1e-6_r });
        const Real step = longest / static_cast<Real>(n);
        const int cx = std::max(1, static_cast<int>(std::ceil(ext.x / step - 1e-3_r)));
        const int cy = std::max(1, static_cast<int>(std::ceil(ext.y / step - 1e-3_r)));
        const V2 cell{ ext.x / static_cast<Real>(cx), ext.y / static_cast<Real>(cy) };
        const Real w = cell.x * cell.y;
        const std::size_t first = cache.points.size();
        for (int y = 0; y < cy; ++y) {
            for (int x = 0; x < cx; ++x) {
                const V2 p = box.min + V2{ (static_cast<Real>(x) + 0.5_r) * cell.x, (static_cast<Real>(y) + 0.5_r) * cell.y };
                if (!detail::fluid_point_in_shape_2d(s, p)) continue;
                cache.points.push_back(SamplePoint{ c.local_offset.apply(p), w });
            }
        }
        if (cache.points.size() == first) cache.points.push_back(SamplePoint{ c.local_offset.apply(box.center()), std::max(w, ext.x * ext.y) });
        Real total = 0.0_r;
        for (std::size_t q = first; q < cache.points.size(); ++q) total += cache.points[q].weight;
        const Real exact = compute_shape_area(s);
        if (exact > 0.0_r && total > 0.0_r) {
            const Real k = exact / total;
            for (std::size_t q = first; q < cache.points.size(); ++q) cache.points[q].weight *= k;
            total = exact;
        }
        cache.area += total;
        cache.spacing = std::min(cache.spacing, std::min(cell.x, cell.y));
    }
    cache.collider_start.push_back(static_cast<std::uint32_t>(cache.points.size()));
    if (cache.spacing == std::numeric_limits<Real>::max()) cache.spacing = 0.1_r;
    if (cache.area <= 0.0_r) cache.area = 1e-9_r;
    return cache;
}

bool FluidVolume2D::local_footprint(const RigidBody2D& body, V2& lo, V2& hi) const {
    lo = V2{ std::numeric_limits<Real>::max(), std::numeric_limits<Real>::max() };
    hi = lo * -1.0_r;
    bool any = false;
    for (const Collider2D& c : body.colliders()) {
        if (c.is_sensor) continue;
        for (int q = 0; q < 4; ++q) {
            const V2 w{ (q & 1) ? c.world_aabb.max.x : c.world_aabb.min.x, (q & 2) ? c.world_aabb.max.y : c.world_aabb.min.y };
            const V2 l = to_local(w);
            lo = V2{ std::min(lo.x, l.x), std::min(lo.y, l.y) };
            hi = V2{ std::max(hi.x, l.x), std::max(hi.y, l.y) };
            any = true;
        }
    }
    return any;
}

FluidForces2D FluidVolume2D::measure(const RigidBody2D& body, const BodyCache& cache, const RVec2& gravity) const {
    FluidForces2D out;
    out.body_area = cache.area;
    V2 lo{}, hi{};
    if (!local_footprint(body, lo, hi)) return out;
    if (hi.x < 0.0_r || lo.x > m_size.x || hi.y < 0.0_r || lo.y > m_size.y) return out;
    const bool cells = m_def.level == FluidLevel::blocky || m_def.level == FluidLevel::flowing || m_def.level == FluidLevel::realistic;
    Ambient amb;
    if (cells) {
        amb = ambient_for(lo, hi);
        if (!amb.valid) return out;
    }
    const FluidMaterial& m0 = m_materials[0];
    auto sample = [&](const V2& world, Real& depth, Real& rho, Real& mu, V2& flow) {
        const V2 l = to_local(world);
        rho = m0.density;
        mu = m0.viscosity;
        flow = m_def.current;
        if (l.x < 0.0_r || l.x > m_size.x || l.y < 0.0_r) {
            depth = -std::numeric_limits<Real>::max();
            return;
        }
        switch (m_def.level) {
        case FluidLevel::still: {
            Real top = m_still_level;
            if (m_def.still.bob_amplitude > 0.0_r) {
                const Real k = 2.0_r * kPi / std::max(m_def.still.bob_wavelength, 1e-3_r);
                const Real om = 2.0_r * kPi / std::max(m_def.still.bob_period, 1e-3_r);
                top += m_def.still.bob_amplitude * std::sin(om * m_time + k * l.x);
            }
            depth = top - l.y;
            break;
        }
        case FluidLevel::waves: {
            depth = sample_columns(l.x, heights()) - l.y;
            flow = flow + world_dir(V2{ sample_columns(l.x, m_col_vel) + swell_velocity(l.x, l.y), 0.0_r });
            break;
        }
        default: {
            depth = amb.level - l.y;
            const int row = std::clamp(static_cast<int>(std::floor(l.y / m_cs)), 0, m_ny - 1);
            const int top_row = std::clamp(static_cast<int>(std::floor(amb.level / m_cs)), 0, m_ny - 1);
            Real rs = 0.0_r, ms = 0.0_r;
            V2 fs{};
            int n = 0;
            for (int side = 0; side < 2; ++side) {
                const int i = side == 0 ? amb.left : amb.right;
                if (i < 0) continue;
                int r = std::min(row, top_row);
                for (int k = 0; k < 6 && r >= 0; ++k, --r) {
                    if (cell_frac(i, r) > 0.25_r) break;
                }
                if (r < 0 || cell_frac(i, r) <= 0.25_r) continue;
                const Real cr = cell_rho(i, r);
                if (cr <= 0.0_r) continue;
                rs += cr;
                ms += cell_mu(i, r);
                fs = fs + cell_flow(i, r);
                ++n;
            }
            if (n > 0) {
                rho = rs / static_cast<Real>(n);
                mu = ms / static_cast<Real>(n);
                flow = flow + world_dir(fs * (1.0_r / static_cast<Real>(n)));
            }
            break;
        }
        }
    };
    const Transform2D& xf = body.transform();
    Real vsub = 0.0_r, msub = 0.0_r, musub = 0.0_r;
    V2 centroid{}, flow{};
    const auto& colliders = body.colliders();
    for (std::size_t ci = 0; ci < colliders.size() && ci + 1 < cache.collider_start.size(); ++ci) {
        const Collider2D& c = colliders[ci];
        if (c.is_sensor || c.shape.type == ShapeType::Edge) continue;
        if (c.shape.type == ShapeType::Circle) {
            const Transform2D cx = xf.compose(c.local_offset);
            const V2 center = cx.apply(c.shape.circle.center);
            const Real r = c.shape.circle.radius;
            Real d = 0.0_r, rho = 0.0_r, mu = 0.0_r;
            V2 fl{};
            sample(center, d, rho, mu, fl);
            if (d <= -r) continue;
            const Real h = std::clamp(d + r, 0.0_r, 2.0_r * r);
            const Real theta = 2.0_r * std::acos(std::clamp((r - h) / r, -1.0_r, 1.0_r));
            const Real a = 0.5_r * r * r * (theta - std::sin(theta));
            if (a <= 0.0_r) continue;
            const Real st = std::sin(0.5_r * theta);
            const Real denom = 3.0_r * (theta - std::sin(theta));
            const Real off = denom > 1e-9_r ? 4.0_r * r * st * st * st / denom : r;
            const V2 cb = center - m_up * off;
            vsub += a;
            msub += a * rho;
            musub += a * mu;
            centroid = centroid + cb * a;
            flow = flow + fl * a;
            continue;
        }
        const Real s = cache.spacing;
        for (std::uint32_t q = cache.collider_start[ci]; q < cache.collider_start[ci + 1]; ++q) {
            const V2 p = xf.apply(cache.points[q].local);
            Real d = 0.0_r, rho = 0.0_r, mu = 0.0_r;
            V2 fl{};
            sample(p, d, rho, mu, fl);
            if (d <= -s) continue;
            const Real frac = std::clamp(d / s + 0.5_r, 0.0_r, 1.0_r);
            if (frac <= 0.0_r) continue;
            const Real v = cache.points[q].weight * frac;
            vsub += v;
            msub += v * rho;
            musub += v * mu;
            centroid = centroid + p * v;
            flow = flow + fl * v;
        }
    }
    if (vsub <= 0.0_r) return out;
    const FluidCouplingSettings& cs = m_def.coupling;
    out.touching = true;
    out.submerged_area = vsub;
    out.submerged_fraction = cache.area > 0.0_r ? std::min(1.0_r, vsub / cache.area) : 1.0_r;
    out.center_of_buoyancy = centroid * (1.0_r / vsub);
    out.fluid_velocity = flow * (1.0_r / vsub);
    const Real rho = msub / vsub;
    const Real mu = musub / vsub;
    out.fluid_density = rho;
    if (cs.buoyancy) out.buoyancy = gravity * (-msub * cs.buoyancy_scale);
    if (cs.drag) {
        const V2 rel = body.get_velocity_at(out.center_of_buoyancy) - out.fluid_velocity;
        const Real speed = len2(rel);
        const Real len = std::sqrt(vsub);
        const Real mass = body.inv_mass() > 0.0_r ? 1.0_r / body.inv_mass() : 0.0_r;
        const Real displaced = std::min(msub, mass);
        const Real c = cs.drag_scale * (m0.linear_drag * 6.0_r * kPi * mu + 0.5_r * rho * m0.quadratic_drag * len * speed) + cs.linear_damping * displaced;
        out.drag = rel * -c;
        const Real ratio = mass > 0.0_r ? displaced / mass : 1.0_r;
        const Real k = (cs.drag_scale * m0.angular_drag * viscosity_factor_of(mu) + cs.angular_damping) * out.submerged_fraction * std::min(1.0_r, 4.0_r * ratio);
        out.angular_drag = body.angular_velocity() * -k;
        const bool radiates = m_def.level == FluidLevel::waves || m_def.level == FluidLevel::realistic;
        if (!radiates && m_def.heave_damping > 0.0_r && mass > 0.0_r) {
            const Real width = std::max(hi.x - lo.x, 1e-3_r);
            const Real stiffness = rho * m_g * width;
            out.heave_damping = 2.0_r * m_def.heave_damping * std::sqrt(stiffness * mass) * std::min(1.0_r, 4.0_r * out.submerged_fraction);
        }
    }
    return out;
}

FluidWorld2D::FluidWorld2D(const Config& cfg) : m_cfg(cfg), m_quality(FluidQualitySettings::preset(cfg.quality)) {
    set_threads(cfg.threads);
}

void FluidWorld2D::set_quality(FluidQuality q) {
    m_cfg.quality = q;
    m_quality = FluidQualitySettings::preset(q);
    for (auto& v : m_volumes) v->m_quality = m_quality;
}

void FluidWorld2D::set_threads(int threads) {
    if (threads <= 0) threads = static_cast<int>(std::max(1u, std::thread::hardware_concurrency()));
    m_cfg.threads = threads;
    m_pool.resize(threads);
}

FluidVolume2D& FluidWorld2D::add_volume(const FluidVolumeDef2D& def) {
    m_volumes.push_back(std::make_unique<FluidVolume2D>(def, m_cfg.gravity, m_quality));
    m_volumes.back()->m_id = m_next_id++;
    return *m_volumes.back();
}

FluidVolume2D& FluidWorld2D::add_volume(const AABB& bounds, FluidLevel level, const FluidMaterial& material, Real fill) {
    return add_volume(FluidVolumeDef2D::box(level, bounds, material, fill));
}

void FluidWorld2D::remove_volume(FluidVolume2D& volume) {
    m_volumes.erase(std::remove_if(m_volumes.begin(), m_volumes.end(), [&volume](const std::unique_ptr<FluidVolume2D>& v) { return v.get() == &volume; }), m_volumes.end());
}

void FluidWorld2D::schedule(FluidVolume2D& v, Real dt, Real& use_dt, bool& run) {
    run = true;
    use_dt = dt;
    if (v.m_frozen) {
        run = false;
        return;
    }
    const Real dist = m_has_focus ? aabb_distance(v.m_world_bounds, m_focus) : 0.0_r;
    if (m_cfg.freeze_distance > 0.0_r && dist > m_cfg.freeze_distance) {
        run = false;
        v.m_lod_accum = 0.0_r;
        return;
    }
    const bool distant = (m_cfg.lod_distance > 0.0_r && dist > m_cfg.lod_distance) || (!v.m_visible && m_cfg.step_offscreen_less);
    if (!distant) {
        use_dt = dt + v.m_lod_accum;
        v.m_lod_accum = 0.0_r;
        v.m_lod_counter = 0;
        return;
    }
    v.m_lod_accum += dt;
    if (++v.m_lod_counter < std::max(1, m_cfg.lod_interval)) {
        run = false;
        return;
    }
    use_dt = v.m_lod_accum;
    v.m_lod_accum = 0.0_r;
    v.m_lod_counter = 0;
}

void FluidWorld2D::finish_stats(FluidVolume2D& v) {
    if (v.sleeping()) ++m_stats.sleeping;
    m_stats.active_regions += v.active_regions();
    m_stats.total_regions += v.total_regions();
    m_stats.particles += v.particle_count();
    m_stats.awake_particles += v.awake_particles();
}

void FluidWorld2D::couple(Simulation2D& sim, Real dt) {
    const RVec2 g = sim.config().gravity;
    for (auto& vp : m_volumes) {
        FluidVolume2D& v = *vp;
        if (!v.m_def.coupling.buoyancy && !v.m_def.coupling.drag) continue;
        for (const FluidVolume2D::NearBody& nb : v.m_near) {
            RigidBody2D* b = nb.body;
            if (!b || !nb.dynamic || b->inv_mass() <= 0.0_r) continue;
            if (m_cfg.require_buoyancy_flag && !b->has(PhysicsFlags::Buoyancy)) continue;
            const FluidVolume2D::BodyCache& cache = v.body_cache(*b, m_quality.buoyancy_samples);
            const FluidForces2D fo = v.measure(*b, cache, g);
            if (!fo.touching) continue;
            ++m_stats.bodies;
            FluidForces2D& acc = m_body_forces[b->handle().packed()];
            acc.touching = true;
            acc.body_area = std::max(acc.body_area, fo.body_area);
            const Real prev = acc.submerged_area;
            acc.submerged_area += fo.submerged_area;
            acc.submerged_fraction = std::min(1.0_r, acc.submerged_fraction + fo.submerged_fraction);
            if (acc.submerged_area > 0.0_r) {
                acc.center_of_buoyancy = (acc.center_of_buoyancy * prev + fo.center_of_buoyancy * fo.submerged_area) * (1.0_r / acc.submerged_area);
                acc.fluid_velocity = (acc.fluid_velocity * prev + fo.fluid_velocity * fo.submerged_area) * (1.0_r / acc.submerged_area);
                acc.fluid_density = (acc.fluid_density * prev + fo.fluid_density * fo.submerged_area) * (1.0_r / acc.submerged_area);
            }
            acc.buoyancy = acc.buoyancy + fo.buoyancy;
            acc.drag = acc.drag + fo.drag;
            acc.angular_drag = acc.angular_drag + fo.angular_drag;
            acc.heave_damping += fo.heave_damping;
            if (fo.buoyancy.x != 0.0_r || fo.buoyancy.y != 0.0_r) b->apply_force_at(fo.buoyancy, fo.center_of_buoyancy);
            const Real mass = 1.0_r / b->inv_mass();
            const V2 rel = b->get_velocity_at(fo.center_of_buoyancy) - fo.fluid_velocity;
            const Real speed = len2(rel);
            if (speed > 1e-9_r && (fo.drag.x != 0.0_r || fo.drag.y != 0.0_r)) {
                const Real c = len2(fo.drag) / speed;
                const Real keep = 1.0_r - std::exp(-c * dt / mass);
                b->apply_impulse_at(rel * (-mass * keep), fo.center_of_buoyancy);
            }
            if (fo.heave_damping > 0.0_r) {
                const RVec2 up = v.m_up;
                const Real vu = dot2(b->get_velocity_at(fo.center_of_buoyancy) - fo.fluid_velocity, up);
                const Real keep = 1.0_r - std::exp(-fo.heave_damping * dt / mass);
                b->apply_impulse_at(up * (-vu * mass * keep), fo.center_of_buoyancy);
            }
            const Real w = b->angular_velocity();
            if (std::abs(w) > 1e-9_r && fo.angular_drag != 0.0_r) {
                const Real k = std::abs(fo.angular_drag) / std::abs(w);
                const Real dw = w * -(1.0_r - std::exp(-k * dt));
                if (b->inv_inertia() > 0.0_r) b->apply_angular_impulse(dw / b->inv_inertia());
            }
        }
    }
}

void FluidWorld2D::step(Simulation2D& sim, Real dt) {
    if (dt <= 0.0_r) return;
    const auto t0 = std::chrono::steady_clock::now();
    m_stats = FluidStats2D{};
    m_stats.volumes = m_volumes.size();
    m_body_forces.clear();
    const RVec2 g = sim.config().gravity;
    for (auto& vp : m_volumes) {
        FluidVolume2D& v = *vp;
        ++v.m_frame;
        v.m_g = std::max(len2(g), 1e-6_r);
        v.m_gl = v.local_dir(g);
        v.gather_bodies(sim);
        if (v.m_def.level != FluidLevel::still && v.m_def.level != FluidLevel::waves) v.rasterize_bodies();
    }
    for (auto& vp : m_volumes) {
        FluidVolume2D& v = *vp;
        Real use = dt;
        bool run = true;
        schedule(v, dt, use, run);
        if (run) {
            ++m_stats.stepped;
            v.step_level(m_pool, use, m_stats);
        }
        finish_stats(v);
    }
    couple(sim, dt);
    for (auto& vp : m_volumes) {
        FluidVolume2D& v = *vp;
        if (v.m_frame % 120 != 0) continue;
        for (auto it = v.m_cache.begin(); it != v.m_cache.end();) {
            if (it->second.seen + 240 < v.m_frame) it = v.m_cache.erase(it);
            else ++it;
        }
    }
    m_stats.milliseconds = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
}

void FluidWorld2D::step(Real dt) {
    if (dt <= 0.0_r) return;
    const auto t0 = std::chrono::steady_clock::now();
    m_stats = FluidStats2D{};
    m_stats.volumes = m_volumes.size();
    for (auto& vp : m_volumes) {
        FluidVolume2D& v = *vp;
        ++v.m_frame;
        v.m_g = std::max(len2(m_cfg.gravity), 1e-6_r);
        v.m_gl = v.local_dir(m_cfg.gravity);
        v.m_near.clear();
        if (v.m_def.level != FluidLevel::still && v.m_def.level != FluidLevel::waves) v.rasterize_bodies();
        Real use = dt;
        bool run = true;
        schedule(v, dt, use, run);
        if (run) {
            ++m_stats.stepped;
            v.step_level(m_pool, use, m_stats);
        }
        finish_stats(v);
    }
    m_stats.milliseconds = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
}

void FluidWorld2D::explode(const RVec2& center, Real radius, Real strength) {
    for (auto& v : m_volumes) {
        if (aabb_distance(v->m_world_bounds, center) <= radius) v->explode(center, radius, strength);
    }
}

void FluidWorld2D::explode(Simulation2D& sim, const RVec2& center, Real radius, Real strength) {
    explode(center, radius, strength);
    if (radius <= 0.0_r) return;
    for (RigidBody2D& b : sim.bodies()) {
        if (b.inv_mass() <= 0.0_r) continue;
        const V2 d = b.world_center() - center;
        const Real l = len2(d);
        if (l >= radius) continue;
        const V2 dir = l > 1e-6_r ? d * (1.0_r / l) : V2{ 0.0_r, 1.0_r };
        b.apply_linear_impulse(dir * (strength * (1.0_r - l / radius) / b.inv_mass()));
        b.wake();
    }
}

void FluidWorld2D::push(const RVec2& center, Real radius, const RVec2& velocity, Real strength) {
    for (auto& v : m_volumes) {
        if (aabb_distance(v->m_world_bounds, center) <= radius) v->push(center, radius, velocity, strength);
    }
}

FluidVolume2D* FluidWorld2D::volume_at(const RVec2& point) {
    for (auto& v : m_volumes) {
        if (v->inside(point)) return v.get();
    }
    return nullptr;
}

const FluidVolume2D* FluidWorld2D::volume_at(const RVec2& point) const {
    for (const auto& v : m_volumes) {
        if (v->inside(point)) return v.get();
    }
    return nullptr;
}

Real FluidWorld2D::surface_height(const RVec2& point) const {
    Real best = -std::numeric_limits<Real>::max();
    for (const auto& v : m_volumes) {
        const V2 l = v->to_local(point);
        if (l.x < 0.0_r || l.x > v->m_size.x) continue;
        best = std::max(best, v->surface_height(point));
    }
    return best;
}

Real FluidWorld2D::surface_height(Real a) const {
    Real best = -std::numeric_limits<Real>::max();
    for (const auto& v : m_volumes) {
        const V2 p = std::abs(v->m_up.y) > 0.5_r ? V2{ a, v->m_world_bounds.center().y } : V2{ v->m_world_bounds.center().x, a };
        const V2 l = v->to_local(p);
        if (l.x < 0.0_r || l.x > v->m_size.x) continue;
        best = std::max(best, v->surface_height(p));
    }
    return best;
}

Real FluidWorld2D::depth_at(const RVec2& point) const {
    Real best = -std::numeric_limits<Real>::max();
    for (const auto& v : m_volumes) best = std::max(best, v->depth_at(point));
    return best;
}

RVec2 FluidWorld2D::velocity_at(const RVec2& point) const {
    const FluidVolume2D* v = volume_at(point);
    return v ? v->velocity_at(point) : RVec2{};
}

bool FluidWorld2D::inside(const RVec2& point) const { return volume_at(point) != nullptr; }

FluidForces2D FluidWorld2D::forces_on(const RigidBody2D& body) {
    FluidForces2D acc;
    AABB merged{};
    bool any = false;
    for (const Collider2D& c : body.colliders()) {
        if (c.is_sensor) continue;
        merged = any ? AABB::merge(merged, c.world_aabb) : c.world_aabb;
        any = true;
    }
    if (!any) return acc;
    for (auto& vp : m_volumes) {
        FluidVolume2D& v = *vp;
        if (!merged.overlaps(v.m_world_bounds)) continue;
        const FluidVolume2D::BodyCache& cache = v.body_cache(body, m_quality.buoyancy_samples);
        const FluidForces2D fo = v.measure(body, cache, m_cfg.gravity);
        if (!fo.touching) continue;
        const Real prev = acc.submerged_area;
        acc.touching = true;
        acc.body_area = std::max(acc.body_area, fo.body_area);
        acc.submerged_area += fo.submerged_area;
        acc.submerged_fraction = std::min(1.0_r, acc.submerged_fraction + fo.submerged_fraction);
        acc.center_of_buoyancy = (acc.center_of_buoyancy * prev + fo.center_of_buoyancy * fo.submerged_area) * (1.0_r / acc.submerged_area);
        acc.fluid_velocity = (acc.fluid_velocity * prev + fo.fluid_velocity * fo.submerged_area) * (1.0_r / acc.submerged_area);
        acc.fluid_density = (acc.fluid_density * prev + fo.fluid_density * fo.submerged_area) * (1.0_r / acc.submerged_area);
        acc.buoyancy = acc.buoyancy + fo.buoyancy;
        acc.drag = acc.drag + fo.drag;
        acc.angular_drag = acc.angular_drag + fo.angular_drag;
    }
    return acc;
}

const FluidForces2D* FluidWorld2D::body_forces(const RigidBody2D& body) const {
    auto it = m_body_forces.find(body.handle().packed());
    return it == m_body_forces.end() ? nullptr : &it->second;
}

} // namespace physics
} // namespace fizmo
