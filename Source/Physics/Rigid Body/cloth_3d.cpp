#include "fizmo_library.hpp"
#include "cloth_3d.hpp"
#include "simulation_3d.hpp"

namespace fizmo {
namespace physics {

namespace {

using V3 = RVec3;

Real norm(const V3& v) noexcept { return std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z); }

V3 unit_or(const V3& v, const V3& fallback) noexcept {
    const Real l = norm(v);
    return l > 1e-12_r ? v * (1.0_r / l) : fallback;
}

V3 closest_point_on_triangle(const V3& p, const V3& a, const V3& b, const V3& c) noexcept {
    const V3 ab = b - a, ac = c - a, ap = p - a;
    const Real d1 = vec3::dot(ab, ap), d2 = vec3::dot(ac, ap);
    if (d1 <= 0.0_r && d2 <= 0.0_r) return a;
    const V3 bp = p - b;
    const Real d3 = vec3::dot(ab, bp), d4 = vec3::dot(ac, bp);
    if (d3 >= 0.0_r && d4 <= d3) return b;
    const Real vc = d1 * d4 - d3 * d2;
    if (vc <= 0.0_r && d1 >= 0.0_r && d3 <= 0.0_r) return a + ab * (d1 / (d1 - d3));
    const V3 cp = p - c;
    const Real d5 = vec3::dot(ab, cp), d6 = vec3::dot(ac, cp);
    if (d6 >= 0.0_r && d5 <= d6) return c;
    const Real vb = d5 * d2 - d1 * d6;
    if (vb <= 0.0_r && d2 >= 0.0_r && d6 <= 0.0_r) return a + ac * (d2 / (d2 - d6));
    const Real va = d3 * d6 - d5 * d4;
    if (va <= 0.0_r && (d4 - d3) >= 0.0_r && (d5 - d6) >= 0.0_r) return b + (c - b) * ((d4 - d3) / ((d4 - d3) + (d5 - d6)));
    const Real denom = 1.0_r / (va + vb + vc);
    return a + ab * (vb * denom) + ac * (vc * denom);
}

bool mesh_contact(const TriangleMeshShape& mesh, const V3& p, const V3& prev, Real thick, V3& out, Real& depth) noexcept {
    const Real reach = thick + norm(p - prev);
    const V3 r{ reach, reach, reach };
    const AABB3D region(V3{ std::min(p.x, prev.x), std::min(p.y, prev.y), std::min(p.z, prev.z) } - r,
                        V3{ std::max(p.x, prev.x), std::max(p.y, prev.y), std::max(p.z, prev.z) } + r);
    bool hit = false;
    depth = 0.0_r;
    mesh.for_each_triangle(region, [&](const V3& a, const V3& b, const V3& c, std::uint32_t t) {
        V3 n = (t < mesh.normals.size()) ? mesh.normals[t] : unit_or(vec3::cross(b - a, c - a), V3{ 0.0_r, 1.0_r, 0.0_r });
        const V3 q = closest_point_on_triangle(p, a, b, c);
        const V3 d = p - q;
        const Real dist = norm(d);
        if (dist >= reach) return true;
        const Real now = vec3::dot(p - a, n);
        const Real before = vec3::dot(prev - a, n);
        const Real side = mesh.two_sided ? (before >= 0.0_r ? 1.0_r : -1.0_r) : 1.0_r;
        if (!mesh.two_sided && before < -thick) return true;
        const bool crossed = now * side < 0.0_r;
        V3 dir{};
        Real pen = 0.0_r;
        if (crossed) {
            const V3 qp = closest_point_on_triangle(p - n * now, a, b, c);
            if (norm(qp - (p - n * now)) > thick) return true;
            dir = n * side;
            pen = thick + std::abs(now);
        } else if (dist < thick) {
            dir = dist > 1e-9_r ? d * (1.0_r / dist) : n * side;
            if (vec3::dot(dir, n) * side < 0.0_r) dir = n * side;
            pen = thick - dist;
        } else {
            return true;
        }
        if (pen > depth) {
            depth = pen;
            out = dir;
            hit = true;
        }
        return true;
    });
    return hit;
}

}

void ClothDef3D::set_resolution(int new_columns, int new_rows) {
    const Real w = width(), hgt = height();
    columns = std::max(1, new_columns);
    rows = std::max(1, new_rows);
    if (columns > 1 && w > 0.0_r) spacing = w / static_cast<Real>(columns - 1);
    if (rows > 1 && hgt > 0.0_r) spacing_along = hgt / static_cast<Real>(rows - 1);
}

ClothDef3D ClothDef3D::sheet(const V3& origin, const V3& across, const V3& along, Real width, Real height, int columns, int rows) {
    ClothDef3D d;
    d.columns = std::max(2, columns);
    d.rows = std::max(2, rows);
    d.origin = origin;
    d.across = unit_or(across, V3{ 1.0_r, 0.0_r, 0.0_r });
    d.along = unit_or(along, V3{ 0.0_r, 0.0_r, 1.0_r });
    d.spacing = std::max(1e-6_r, width) / static_cast<Real>(d.columns - 1);
    d.spacing_along = std::max(1e-6_r, height) / static_cast<Real>(d.rows - 1);
    d.scale_with_resolution = true;
    d.thickness = std::min(d.thickness, 0.5_r * std::min(d.spacing, d.spacing_along));
    return d;
}

ClothDef3D ClothDef3D::sheet_with_triangles(const V3& origin, const V3& across, const V3& along, Real width, Real height, std::size_t triangles) {
    const Real w = std::max(1e-6_r, width), hgt = std::max(1e-6_r, height);
    const Real cells = std::max(1.0_r, static_cast<Real>(triangles) * 0.5_r);
    const Real cell = std::sqrt(w * hgt / cells);
    const int columns = std::max(2, static_cast<int>(std::lround(w / cell)) + 1);
    const int rows = std::max(2, static_cast<int>(std::lround(hgt / cell)) + 1);
    return sheet(origin, across, along, w, hgt, columns, rows);
}

ClothDef3D ClothDef3D::rope(const V3& start, const V3& direction, int segments, Real length) {
    ClothDef3D d;
    const Real len = norm(direction);
    d.columns = std::max(2, segments + 1);
    d.rows = 1;
    d.spacing = std::max(1e-6_r, length) / static_cast<Real>(d.columns - 1);
    d.origin = start;
    d.across = len > 1e-12_r ? direction * (1.0_r / len) : V3{ 1.0_r, 0.0_r, 0.0_r };
    d.along = V3{ 0.0_r, 0.0_r, 1.0_r };
    d.mass = 0.1_r * std::max(1e-6_r, length);
    d.bend_compliance = 1e-2_r;
    return d;
}

Cloth3D::Cloth3D(const ClothDef3D& def) : m_def(def) {
    m_def.columns = std::max(1, m_def.columns);
    m_def.rows = std::max(1, m_def.rows);
    m_def.substeps = std::max(1, m_def.substeps);
    const std::size_t n = static_cast<std::size_t>(m_def.columns) * static_cast<std::size_t>(m_def.rows);
    m_particle_mass = std::max(1e-9_r, m_def.mass) / static_cast<Real>(n);
    m_x.resize(n);
    m_prev.resize(n);
    m_v.assign(n, V3{});
    m_w.assign(n, 1.0_r / m_particle_mass);
    m_pinned.assign(n, 0);
    m_pin_at.resize(n);
    for (int r = 0; r < m_def.rows; ++r) {
        for (int c = 0; c < m_def.columns; ++c) {
            const std::size_t i = index(c, r);
            m_x[i] = m_def.origin + m_def.across * (static_cast<Real>(c) * m_def.spacing) + m_def.along * (static_cast<Real>(r) * m_def.row_spacing());
            m_prev[i] = m_x[i];
        }
    }
    for (int r = 0; r < m_def.rows; ++r) {
        for (int c = 0; c < m_def.columns; ++c) {
            if (c + 1 < m_def.columns) add_link(index(c, r), index(c + 1, r), m_def.stretch_compliance, 0);
            if (r + 1 < m_def.rows) add_link(index(c, r), index(c, r + 1), m_def.stretch_compliance, 0);
        }
    }
    for (int r = 0; r + 1 < m_def.rows; ++r) {
        for (int c = 0; c + 1 < m_def.columns; ++c) {
            add_link(index(c, r), index(c + 1, r + 1), m_def.shear_compliance, 1);
            add_link(index(c + 1, r), index(c, r + 1), m_def.shear_compliance, 1);
        }
    }
    for (int r = 0; r < m_def.rows; ++r) {
        for (int c = 0; c < m_def.columns; ++c) {
            if (c + 1 < m_def.columns && r + 1 < m_def.rows) {
                const std::uint32_t a = static_cast<std::uint32_t>(index(c, r)), b = static_cast<std::uint32_t>(index(c + 1, r));
                const std::uint32_t d = static_cast<std::uint32_t>(index(c, r + 1)), e = static_cast<std::uint32_t>(index(c + 1, r + 1));
                m_triangles.push_back({ a, d, b });
                m_triangles.push_back({ b, d, e });
            }
        }
    }
    for (int r = 0; r < m_def.rows; ++r) {
        for (int c = 0; c < m_def.columns; ++c) {
            if (m_def.self_bending) {
                if (c + 2 < m_def.columns) add_link(index(c, r), index(c + 2, r), m_def.bend_compliance, 2);
                if (r + 2 < m_def.rows) add_link(index(c, r), index(c, r + 2), m_def.bend_compliance, 2);
            }
        }
    }
}

void Cloth3D::add_link(std::size_t a, std::size_t b, Real compliance, std::uint8_t kind) {
    Link l;
    l.a = static_cast<std::uint32_t>(a);
    l.b = static_cast<std::uint32_t>(b);
    l.rest = norm(m_x[b] - m_x[a]);
    l.kind = kind;
    const Real scale = (m_def.scale_with_resolution && m_def.reference_spacing > 0.0_r) ? l.rest / m_def.reference_spacing : 1.0_r;
    l.compliance = std::max(0.0_r, compliance) * scale;
    m_links.push_back(l);
}

Real Cloth3D::base_inv_mass(std::size_t i) const {
    if (m_pinned[i]) return 0.0_r;
    for (const Attachment& a : m_attachments) {
        if (a.particle == i) return 0.0_r;
    }
    return 1.0_r / m_particle_mass;
}

void Cloth3D::set_position(std::size_t i, const V3& p) {
    m_x.at(i) = p;
    m_prev[i] = p;
    if (m_pinned[i]) m_pin_at[i] = p;
}

void Cloth3D::set_velocity(std::size_t i, const V3& v) {
    m_v.at(i) = v;
}

void Cloth3D::pin(std::size_t i) {
    pin(i, m_x.at(i));
}

void Cloth3D::pin(std::size_t i, const V3& world_point) {
    m_pinned.at(i) = 1;
    m_pin_at[i] = world_point;
    m_x[i] = world_point;
    m_prev[i] = world_point;
    m_v[i] = V3{};
    m_w[i] = 0.0_r;
}

void Cloth3D::unpin(std::size_t i) {
    m_pinned.at(i) = 0;
    m_w[i] = base_inv_mass(i);
}

bool Cloth3D::pinned(std::size_t i) const {
    return m_pinned.at(i) != 0;
}

void Cloth3D::attach(std::size_t i, RigidBody3D& body) {
    detach(i);
    Attachment a;
    a.particle = static_cast<std::uint32_t>(i);
    a.body = body.handle();
    a.local = body.transform().apply_inverse(m_x.at(i));
    m_attachments.push_back(a);
    m_w[i] = 0.0_r;
}

void Cloth3D::detach(std::size_t i) {
    m_attachments.erase(std::remove_if(m_attachments.begin(), m_attachments.end(), [i](const Attachment& a) { return a.particle == i; }), m_attachments.end());
    m_w.at(i) = base_inv_mass(i);
}

bool Cloth3D::attached(std::size_t i) const {
    for (const Attachment& a : m_attachments) {
        if (a.particle == i) return true;
    }
    return false;
}

std::vector<V3> Cloth3D::normals() const {
    std::vector<V3> out(m_x.size(), V3{});
    for (const auto& t : m_triangles) {
        const V3 n = vec3::cross(m_x[t[1]] - m_x[t[0]], m_x[t[2]] - m_x[t[0]]);
        for (std::uint32_t k : t) out[k] = out[k] + n;
    }
    for (V3& n : out) {
        const Real l = norm(n);
        n = l > 1e-12_r ? n * (1.0_r / l) : V3{ 0.0_r, 1.0_r, 0.0_r };
    }
    return out;
}

AABB3D Cloth3D::bounds() const {
    V3 lo = m_x.empty() ? V3{} : m_x[0], hi = lo;
    for (const V3& p : m_x) {
        lo = V3{ std::min(lo.x, p.x), std::min(lo.y, p.y), std::min(lo.z, p.z) };
        hi = V3{ std::max(hi.x, p.x), std::max(hi.y, p.y), std::max(hi.z, p.z) };
    }
    return AABB3D(lo, hi);
}

Real Cloth3D::max_stretch() const {
    Real worst = 0.0_r;
    for (const Link& l : m_links) {
        if (l.kind != 0 || l.rest <= 0.0_r) continue;
        worst = std::max(worst, norm(m_x[l.b] - m_x[l.a]) / l.rest - 1.0_r);
    }
    return worst;
}

void Cloth3D::shift(const V3& offset) {
    for (std::size_t i = 0; i < m_x.size(); ++i) {
        m_x[i] = m_x[i] - offset;
        m_prev[i] = m_prev[i] - offset;
        m_pin_at[i] = m_pin_at[i] - offset;
    }
}

void Cloth3D::solve_links(Real h) {
    const Real inv_h2 = 1.0_r / (h * h);
    for (const Link& l : m_links) {
        const Real wa = m_w[l.a], wb = m_w[l.b];
        const Real ws = wa + wb;
        if (ws <= 0.0_r) continue;
        const V3 d = m_x[l.b] - m_x[l.a];
        const Real len = norm(d);
        if (len < 1e-12_r) continue;
        const Real alpha = l.compliance * inv_h2;
        const Real lambda = -(len - l.rest) / (ws + alpha);
        const V3 corr = d * (lambda / len);
        m_x[l.a] = m_x[l.a] - corr * wa;
        m_x[l.b] = m_x[l.b] + corr * wb;
    }
}

void Cloth3D::apply_attachments(Simulation3D& sim) {
    for (std::size_t k = 0; k < m_attachments.size();) {
        const Attachment& a = m_attachments[k];
        RigidBody3D* body = sim.get(a.body);
        if (!body) {
            const std::uint32_t p = a.particle;
            m_attachments.erase(m_attachments.begin() + static_cast<std::ptrdiff_t>(k));
            m_w[p] = base_inv_mass(p);
            continue;
        }
        const V3 p = body->transform().apply(a.local);
        m_x[a.particle] = p;
        ++k;
    }
    for (std::size_t i = 0; i < m_x.size(); ++i) {
        if (m_pinned[i]) m_x[i] = m_pin_at[i];
    }
}

void Cloth3D::aerodynamics(Real h) {
    if (m_def.drag <= 0.0_r && m_def.lift <= 0.0_r) return;
    for (const auto& t : m_triangles) {
        const V3 cross = vec3::cross(m_x[t[1]] - m_x[t[0]], m_x[t[2]] - m_x[t[0]]);
        const Real twice_area = norm(cross);
        if (twice_area < 1e-12_r) continue;
        const V3 n = cross * (1.0_r / twice_area);
        const V3 rel = (m_v[t[0]] + m_v[t[1]] + m_v[t[2]]) * (1.0_r / 3.0_r) - m_def.wind;
        const Real speed = norm(rel);
        if (speed < 1e-9_r) continue;
        const Real vn = vec3::dot(rel, n);
        const V3 drag = n * (-m_def.drag * 0.5_r * twice_area * vn * std::abs(vn));
        V3 lift{};
        if (m_def.lift > 0.0_r) {
            const V3 side = vec3::cross(vec3::cross(n, rel), rel);
            const Real sl = norm(side);
            if (sl > 1e-12_r) lift = side * (m_def.lift * 0.5_r * twice_area * vn * speed / sl);
        }
        const V3 dv = (drag + lift) * (h / (3.0_r * m_particle_mass));
        for (std::uint32_t k : t) {
            if (m_w[k] > 0.0_r) m_v[k] = m_v[k] + dv;
        }
    }
}

void Cloth3D::collide(Simulation3D& sim, const std::vector<RigidBody3D*>& bodies, Real h) {
    static_cast<void>(sim);
    const Real thick = std::max(0.0_r, m_def.thickness);
    for (std::size_t i = 0; i < m_x.size(); ++i) {
        if (m_w[i] <= 0.0_r) continue;
        for (RigidBody3D* body : bodies) {
            for (const Collider3D& c : body->colliders()) {
                if (c.is_sensor) continue;
                const AABB3D near_box = c.world_aabb.fatten(thick);
                if (!near_box.contains(m_x[i]) && !near_box.contains(m_prev[i])) continue;
                const Transform3D xf = body->transform().compose(c.local_offset);
                V3 out{};
                Real depth = 0.0_r;
                const Shape3D& s = c.shape;
                if (s.type == ShapeType3D::Sphere) {
                    const V3 d = m_x[i] - xf.apply(s.sphere.center);
                    const Real dist = norm(d);
                    const Real r = s.sphere.radius + thick;
                    if (dist >= r) continue;
                    out = dist > 1e-12_r ? d * (1.0_r / dist) : V3{ 0.0_r, 1.0_r, 0.0_r };
                    depth = r - dist;
                } else if (s.type == ShapeType3D::Plane) {
                    const V3 nw = xf.rotate(s.plane.normal);
                    const Real sd = vec3::dot(m_x[i] - xf.apply(s.plane.normal * s.plane.offset), nw) - thick;
                    if (sd >= 0.0_r) continue;
                    out = nw;
                    depth = -sd;
                } else if (s.type == ShapeType3D::Heightfield) {
                    if (!s.heightfield) continue;
                    const V3 lp = xf.apply_inverse(m_x[i]);
                    const Real ground = s.heightfield->height_at(lp.x, lp.z) + thick;
                    if (lp.y >= ground) continue;
                    out = xf.rotate(V3{ 0.0_r, 1.0_r, 0.0_r });
                    depth = ground - lp.y;
                } else if (s.is_convex()) {
                    ConvexProxy3D shape;
                    if (!make_convex_proxy(s, xf, shape)) continue;
                    const ConvexSeparation3D sep = convex_separation(ConvexProxy3D::point(m_x[i], thick), shape);
                    if (!sep.valid || sep.separation >= 0.0_r) continue;
                    out = sep.normal * -1.0_r;
                    depth = -sep.separation;
                } else if (s.type == ShapeType3D::Mesh) {
                    if (!s.mesh || !s.mesh->valid()) continue;
                    V3 lo{};
                    if (!mesh_contact(*s.mesh, xf.apply_inverse(m_x[i]), xf.apply_inverse(m_prev[i]), thick, lo, depth)) continue;
                    out = xf.rotate(lo);
                } else {
                    continue;
                }
                m_x[i] = m_x[i] + out * depth;
                const V3 moved = m_x[i] - m_prev[i];
                const V3 tangent = moved - out * vec3::dot(moved, out);
                const Real lt = norm(tangent);
                const Real limit = m_def.friction * depth;
                if (lt > 1e-12_r) m_x[i] = m_x[i] - (lt <= limit ? tangent : tangent * (limit / lt));
                ++m_contacts;
                if (m_def.coupling > 0.0_r && body->inv_mass() > 0.0_r) {
                    body->apply_impulse_at(out * (-depth * m_particle_mass / h * m_def.coupling), m_x[i]);
                    body->wake();
                }
            }
        }
    }
}

void Cloth3D::step(Simulation3D& sim, Real dt) {
    if (dt <= 0.0_r || m_x.empty()) return;
    m_contacts = 0;
    const int n = std::max(1, m_def.substeps);
    const Real h = dt / static_cast<Real>(n);
    const V3 g = sim.config().gravity * m_def.gravity_scale;
    std::vector<RigidBody3D*> bodies;
    if (m_def.collide) {
        Real fastest = 0.0_r;
        for (const V3& v : m_v) fastest = std::max(fastest, norm(v));
        QueryFilter3D f;
        f.layers = m_def.mask;
        for (RigidBody3D* b : sim.query_aabb(bounds().fatten(m_def.thickness + fastest * dt + 0.05_r), f)) {
            if ((b->collision_mask() & m_def.layer) != 0) bodies.push_back(b);
        }
    }
    const Real keep = std::pow(std::clamp(1.0_r - m_def.damping, 0.0_r, 1.0_r), h * 60.0_r);
    for (int sub = 0; sub < n; ++sub) {
        aerodynamics(h);
        for (std::size_t i = 0; i < m_x.size(); ++i) {
            m_prev[i] = m_x[i];
            if (m_w[i] <= 0.0_r) continue;
            m_v[i] = m_v[i] + g * h;
            m_x[i] = m_x[i] + m_v[i] * h;
        }
        apply_attachments(sim);
        solve_links(h);
        if (!bodies.empty()) collide(sim, bodies, h);
        for (std::size_t i = 0; i < m_x.size(); ++i) m_v[i] = (m_w[i] > 0.0_r) ? (m_x[i] - m_prev[i]) * (keep / h) : (m_x[i] - m_prev[i]) * (1.0_r / h);
    }
}

} // namespace physics
} // namespace fizmo
