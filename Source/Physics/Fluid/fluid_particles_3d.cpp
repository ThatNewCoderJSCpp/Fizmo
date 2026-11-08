#include "fizmo_library.hpp"
#include "fluid_3d.hpp"

namespace fizmo {
namespace physics {

namespace {

using V3 = RVec3;

constexpr Real kPi = 3.14159265358979323846_r;
constexpr std::int32_t kNoContact = -1;
constexpr std::int32_t kBoundsContact = -2;

Real norm(const V3& v) noexcept { return std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z); }

V3 closest_on_triangle(const V3& p, const V3& a, const V3& b, const V3& c) noexcept {
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

Real combine_friction(Real a, Real b) noexcept { return std::sqrt(std::max(a, 0.0_r) * std::max(b, 0.0_r)); }

} // namespace

namespace detail {

bool fluid_collider_contact(const Collider3D& c, const Transform3D& body_xf, const RVec3& p, Real r, RVec3& n, Real& depth) {
    const Shape3D& s = c.shape;
    const Transform3D xf = body_xf.compose(c.local_offset);
    switch (s.type) {
    case ShapeType3D::Sphere: {
        const V3 d = p - xf.apply(s.sphere.center);
        const Real dist = norm(d);
        const Real reach = s.sphere.radius + r;
        if (dist >= reach) return false;
        n = dist > 1e-9_r ? d * (1.0_r / dist) : xf.rotate(V3{ 0.0_r, 1.0_r, 0.0_r });
        depth = reach - dist;
        return true;
    }
    case ShapeType3D::Capsule: {
        const V3 q = narrowphase3d::closest_point_on_segment(p, xf.apply(s.capsule.point1), xf.apply(s.capsule.point2));
        const V3 d = p - q;
        const Real dist = norm(d);
        const Real reach = s.capsule.radius + r;
        if (dist >= reach) return false;
        n = dist > 1e-9_r ? d * (1.0_r / dist) : xf.rotate(V3{ 0.0_r, 1.0_r, 0.0_r });
        depth = reach - dist;
        return true;
    }
    case ShapeType3D::Hull: {
        if (!s.hull) return false;
        const narrowphase3d::PointHullResult res = narrowphase3d::closest_point_on_hull(*s.hull, xf.apply_inverse(p));
        if (res.distance >= r) return false;
        n = xf.rotate(res.normal);
        depth = r - res.distance;
        return true;
    }
    case ShapeType3D::Plane: {
        const V3 nw = xf.rotate(s.plane.normal);
        const Real sd = vec3::dot(p - xf.apply(s.plane.normal * s.plane.offset), nw) - r;
        if (sd >= 0.0_r) return false;
        n = nw;
        depth = -sd;
        return true;
    }
    case ShapeType3D::Heightfield: {
        if (!s.heightfield) return false;
        const V3 lp = xf.apply_inverse(p);
        const Real ground = s.heightfield->height_at(lp.x, lp.z) + r;
        if (lp.y >= ground) return false;
        const Real e = 0.5_r * std::min(s.heightfield->cell_x, s.heightfield->cell_z);
        const Real hx = s.heightfield->height_at(lp.x + e, lp.z) - s.heightfield->height_at(lp.x - e, lp.z);
        const Real hz = s.heightfield->height_at(lp.x, lp.z + e) - s.heightfield->height_at(lp.x, lp.z - e);
        n = vec3::normalize(xf.rotate(V3{ -hx / (2.0_r * e), 1.0_r, -hz / (2.0_r * e) }));
        depth = (ground - lp.y) * std::max(0.2_r, vec3::dot(n, xf.rotate(V3{ 0.0_r, 1.0_r, 0.0_r })));
        return true;
    }
    case ShapeType3D::Mesh: {
        if (!s.mesh || !s.mesh->valid()) return false;
        const V3 lp = xf.apply_inverse(p);
        const AABB3D region(lp - V3{ r, r, r }, lp + V3{ r, r, r });
        Real best = r;
        V3 out{};
        bool hit = false;
        s.mesh->for_each_triangle(region, [&](const V3& a, const V3& b, const V3& cc, std::uint32_t t) {
            const V3 q = closest_on_triangle(lp, a, b, cc);
            const V3 d = lp - q;
            const Real dist = norm(d);
            if (dist >= best) return true;
            best = dist;
            const V3 fn = t < s.mesh->normals.size() ? s.mesh->normals[t] : vec3::normalize(vec3::cross(b - a, cc - a));
            out = dist > 1e-9_r ? d * (1.0_r / dist) : fn;
            hit = true;
            return true;
        });
        if (!hit) return false;
        n = xf.rotate(out);
        depth = r - best;
        return true;
    }
    default: {
        ConvexProxy3D proxy;
        if (!make_convex_proxy(s, xf, proxy)) return false;
        const ConvexSeparation3D sep = convex_separation(ConvexProxy3D::point(p, r), proxy);
        if (!sep.valid || sep.separation >= 0.0_r) return false;
        n = sep.normal * -1.0_r;
        depth = -sep.separation;
        return true;
    }
    }
}

} // namespace detail

void FluidVolume3D::grid_coords(const V3& world, int& x, int& y, int& z) const noexcept {
    const V3 l = to_local(world);
    const Real inv = 1.0_r / m_gcell;
    x = std::clamp(static_cast<int>(std::floor(l.x * inv)), 0, m_gx - 1);
    y = std::clamp(static_cast<int>(std::floor(l.y * inv)), 0, m_gy - 1);
    z = std::clamp(static_cast<int>(std::floor(l.z * inv)), 0, m_gz - 1);
}

template <class Fn>
void FluidVolume3D::visit_near(const V3& world, Fn&& fn) const {
    int cx = 0, cy = 0, cz = 0;
    grid_coords(world, cx, cy, cz);
    const int x0 = std::max(cx - 1, 0), x1 = std::min(cx + 1, m_gx - 1);
    const int y0 = std::max(cy - 1, 0), y1 = std::min(cy + 1, m_gy - 1);
    const int z0 = std::max(cz - 1, 0), z1 = std::min(cz + 1, m_gz - 1);
    for (int y = y0; y <= y1; ++y) {
        for (int z = z0; z <= z1; ++z) {
            const std::uint32_t a = grid_index(x0, y, z), b = grid_index(x1, y, z);
            const std::uint32_t s0 = m_bucket_start[a], s1 = m_bucket_start[b] + m_bucket_count[b];
            for (std::uint32_t q = s0; q < s1; ++q) fn(m_sorted[q]);
        }
    }
}

Real FluidVolume3D::kernel_w(Real r2) const noexcept {
    const Real h2 = m_h * m_h;
    if (r2 >= h2) return 0.0_r;
    const Real d = h2 - r2;
    return m_poly6 * d * d * d;
}

Real FluidVolume3D::kernel_grad(Real r) const noexcept {
    if (r >= m_h || r <= 0.0_r) return 0.0_r;
    const Real d = m_h - r;
    return m_spiky * d * d;
}

void FluidVolume3D::init_particles() {
    m_radius = std::max(1e-3_r, m_def.particles.radius * m_quality.resolution_scale);
    m_spacing = 2.0_r * m_radius;
    m_h = 2.0_r * m_spacing;
    const Real h3 = m_h * m_h * m_h;
    m_poly6 = 315.0_r / (64.0_r * kPi * h3 * h3 * h3);
    m_spiky = -45.0_r / (kPi * h3 * h3);
    Real sum = 0.0_r;
    const int reach = static_cast<int>(std::ceil(m_h / m_spacing));
    for (int z = -reach; z <= reach; ++z) {
        for (int y = -reach; y <= reach; ++y) {
            for (int x = -reach; x <= reach; ++x) {
                const V3 d{ static_cast<Real>(x) * m_spacing, static_cast<Real>(y) * m_spacing, static_cast<Real>(z) * m_spacing };
                sum += kernel_w(vec3::dot(d, d));
            }
        }
    }
    m_pmass = m_def.material.density / std::max(sum, 1e-12_r);
    const Real dq = 0.2_r * m_h;
    m_wq = std::max(kernel_w(dq * dq), 1e-12_r);
    m_max_particles = m_def.particles.max_particles > 0 ? m_def.particles.max_particles : m_quality.max_particles;
    m_gcell = m_h;
    for (;;) {
        m_gx = std::max(1, static_cast<int>(std::ceil(m_size.x / m_gcell)));
        m_gy = std::max(1, static_cast<int>(std::ceil(m_size.y / m_gcell)));
        m_gz = std::max(1, static_cast<int>(std::ceil(m_size.z / m_gcell)));
        if (static_cast<double>(m_gx) * m_gy * m_gz <= 4194304.0) break;
        m_gcell *= 1.25_r;
    }
    const std::size_t table = static_cast<std::size_t>(m_gx) * static_cast<std::size_t>(m_gy) * static_cast<std::size_t>(m_gz);
    m_bucket_start.assign(table, 0);
    m_bucket_count.assign(table, 0);
    const std::size_t reserve = std::min<std::size_t>(m_max_particles, 1u << 16);
    m_px.reserve(reserve);
    m_pv.reserve(reserve);
    if (m_def.fill > 0.0_r) {
        AABB3D box = m_world_bounds;
        const V3 lo = to_local(box.min), hi = to_local(box.max);
        const V3 l0 = vec3::min(lo, hi), l1 = vec3::max(lo, hi);
        const V3 top = to_world(V3{ l1.x, l0.y + (l1.y - l0.y) * std::min(m_def.fill, 1.0_r), l1.z });
        const V3 bot = to_world(l0);
        add_particle_box(AABB3D(vec3::min(top, bot), vec3::max(top, bot)));
    }
}

bool FluidVolume3D::add_particle(const V3& position, const V3& velocity) {
    if (m_def.level != FluidLevel::realistic || m_px.size() >= m_max_particles) return false;
    m_px.push_back(position);
    m_pv.push_back(velocity);
    m_pp.push_back(position);
    m_pv0.push_back(velocity);
    m_dp.push_back(V3{});
    m_omega.push_back(V3{});
    m_lambda.push_back(0.0_r);
    m_density.push_back(m_def.material.density);
    m_calm.push_back(0);
    m_asleep.push_back(0);
    m_live.push_back(1);
    m_nbr_count.push_back(0);
    m_pkey.push_back(0);
    m_sorted.push_back(0);
    m_contact_of.push_back(kNoContact);
    m_contacts_scratch.push_back(ParticleContact{});
    if (m_nbr.size() < m_px.size() * kMaxNeighbours) m_nbr.resize(std::max(m_nbr.size() * 2, m_px.size() * kMaxNeighbours));
    return true;
}

std::size_t FluidVolume3D::add_particle_box(const AABB3D& box, const V3& velocity) {
    if (m_def.level != FluidLevel::realistic) return 0;
    const V3 a = to_local(box.min), b = to_local(box.max);
    V3 lo = vec3::min(a, b), hi = vec3::max(a, b);
    lo = vec3::max(lo, V3{ m_radius, m_radius, m_radius });
    hi = vec3::min(hi, m_size - V3{ m_radius, m_radius, m_radius });
    std::size_t added = 0;
    for (Real y = lo.y + m_radius; y <= hi.y + 1e-6_r; y += m_spacing) {
        for (Real z = lo.z + m_radius; z <= hi.z + 1e-6_r; z += m_spacing) {
            for (Real x = lo.x + m_radius; x <= hi.x + 1e-6_r; x += m_spacing) {
                if (!add_particle(to_world(V3{ x, y, z }), velocity)) return added;
                ++added;
            }
        }
    }
    return added;
}

void FluidVolume3D::clear_particles() {
    m_px.clear();
    m_pv.clear();
    m_pp.clear();
    m_pv0.clear();
    m_dp.clear();
    m_omega.clear();
    m_lambda.clear();
    m_density.clear();
    m_calm.clear();
    m_asleep.clear();
    m_live.clear();
    m_nbr_count.clear();
    m_pkey.clear();
    m_sorted.clear();
    m_contact_of.clear();
    m_contacts_scratch.clear();
    m_awake_count = 0;
}

std::size_t FluidVolume3D::add_emitter(const FluidEmitter3D& e) {
    m_emitters.push_back(e);
    return m_emitters.size() - 1;
}

std::size_t FluidVolume3D::particles_in(const AABB3D& box) const {
    std::size_t n = 0;
    for (const V3& p : m_px) {
        if (box.contains(p)) ++n;
    }
    return n;
}

void FluidVolume3D::run_emitters(Real dt) {
    for (FluidEmitter3D& e : m_emitters) {
        if (!e.active || e.rate <= 0.0_r) continue;
        e.carry += e.rate * dt;
        const V3 dir = vec3::is_zero(e.normal) ? vec3::normalize(e.velocity, m_up * -1.0_r) : vec3::normalize(e.normal);
        V3 t1, t2;
        vec3::orthonormal_basis(dir, t1, t2);
        const int ring = std::max(0, static_cast<int>(e.radius / m_spacing));
        while (e.carry >= 1.0_r) {
            bool any = false;
            for (int a = -ring; a <= ring && e.carry >= 1.0_r; ++a) {
                for (int b = -ring; b <= ring && e.carry >= 1.0_r; ++b) {
                    const V3 off = t1 * (static_cast<Real>(a) * m_spacing) + t2 * (static_cast<Real>(b) * m_spacing);
                    if (vec3::dot(off, off) > e.radius * e.radius + 1e-9_r) continue;
                    if (!add_particle(e.position + off, e.velocity)) {
                        e.carry = 0.0_r;
                        return;
                    }
                    e.carry -= 1.0_r;
                    any = true;
                }
            }
            if (!any) break;
        }
    }
}

void FluidVolume3D::gather_bodies(Simulation3D& sim) {
    m_near.clear();
    if (m_def.level != FluidLevel::realistic) return;
    QueryFilter3D f;
    f.layers = m_def.coupling.layers;
    const std::vector<RigidBody3D*> found = sim.query_aabb(m_world_bounds.fatten(m_h), f);
    for (RigidBody3D* b : found) {
        if (!b || b->colliders().empty()) continue;
        NearBody nb;
        nb.body = b;
        bool first = true;
        for (const Collider3D& c : b->colliders()) {
            if (c.is_sensor) continue;
            nb.box = first ? c.world_aabb : AABB3D::merge(nb.box, c.world_aabb);
            first = false;
        }
        if (first) continue;
        nb.box = nb.box.fatten(m_radius);
        nb.dynamic = b->inv_mass() > 0.0_r;
        m_near.push_back(nb);
    }
}

void FluidVolume3D::build_particle_grid(detail::FluidTaskPool& pool, const std::vector<V3>& at) {
    const std::size_t n = at.size();
    pool.parallel_range(n, 2048, [&](std::size_t b, std::size_t e) {
        for (std::size_t i = b; i < e; ++i) {
            int x = 0, y = 0, z = 0;
            grid_coords(at[i], x, y, z);
            m_pkey[i] = grid_index(x, y, z);
        }
    });
    std::fill(m_bucket_count.begin(), m_bucket_count.end(), 0u);
    for (std::size_t i = 0; i < n; ++i) ++m_bucket_count[m_pkey[i]];
    std::uint32_t run = 0;
    for (std::size_t b = 0; b < m_bucket_count.size(); ++b) {
        m_bucket_start[b] = run;
        run += m_bucket_count[b];
    }
    for (std::size_t i = 0; i < n; ++i) {
        const std::uint32_t key = m_pkey[i];
        m_sorted[m_bucket_start[key]++] = static_cast<std::uint32_t>(i);
    }
    for (std::size_t b = 0; b < m_bucket_count.size(); ++b) m_bucket_start[b] -= m_bucket_count[b];
}

void FluidVolume3D::reorder_particles(detail::FluidTaskPool& pool) {
    const std::size_t n = m_px.size();
    if (n < 2) return;
    build_particle_grid(pool, m_px);
    m_tmp_u8.resize(n);
    m_tmp_u16.resize(n);
    m_tmp_r.resize(n);
    for (std::size_t q = 0; q < n; ++q) {
        const std::uint32_t s = m_sorted[q];
        m_pp[q] = m_px[s];
        m_dp[q] = m_pv[s];
        m_tmp_u8[q] = m_asleep[s];
        m_tmp_u16[q] = m_calm[s];
        m_tmp_r[q] = m_density[s];
    }
    m_px.swap(m_pp);
    m_pv.swap(m_dp);
    m_asleep.swap(m_tmp_u8);
    m_calm.swap(m_tmp_u16);
    m_density.swap(m_tmp_r);
}

void FluidVolume3D::find_neighbours(detail::FluidTaskPool& pool) {
    const std::size_t n = m_pp.size();
    const Real h2 = m_h * m_h;
    pool.parallel_range(n, 512, [&](std::size_t b, std::size_t e) {
        for (std::size_t i = b; i < e; ++i) {
            m_nbr_count[i] = 0;
            if (!m_live[i]) continue;
            const V3 p = m_pp[i];
            std::uint32_t* out = &m_nbr[i * kMaxNeighbours];
            std::size_t count = 0;
            const V3* pos = m_pp.data();
            const std::uint32_t self = static_cast<std::uint32_t>(i);
            visit_near(p, [&](std::uint32_t j) {
                if (j == self || count >= kMaxNeighbours) return;
                const V3 d = pos[j] - p;
                if (d.x * d.x + d.y * d.y + d.z * d.z < h2) out[count++] = j;
            });
            m_nbr_count[i] = static_cast<std::uint16_t>(count);
        }
    });
}

void FluidVolume3D::collide_particles(detail::FluidTaskPool& pool, Real h, Real ri) {
    static_cast<void>(h);
    const std::size_t n = m_pp.size();
    const bool contain = m_def.particles.contain;
    pool.parallel_range(n, 1024, [&](std::size_t b, std::size_t e) {
        for (std::size_t i = b; i < e; ++i) {
            if (m_asleep[i]) continue;
            V3 p = m_pp[i];
            Real best = 0.0_r;
            for (std::size_t k = 0; k < m_near.size(); ++k) {
                const NearBody& nb = m_near[k];
                if (!nb.box.contains(p)) continue;
                const auto& cols = nb.body->colliders();
                for (std::size_t c = 0; c < cols.size(); ++c) {
                    if (cols[c].is_sensor || !cols[c].world_aabb.fatten(ri).contains(p)) continue;
                    V3 nrm{};
                    Real depth = 0.0_r;
                    if (!detail::fluid_collider_contact(cols[c], nb.body->transform(), p, ri, nrm, depth)) continue;
                    p = p + nrm * depth;
                    if (depth >= best) {
                        best = depth;
                        m_contact_of[i] = static_cast<std::int32_t>(k);
                        m_contacts_scratch[i].normal = nrm;
                        m_contacts_scratch[i].collider = static_cast<std::uint32_t>(c);
                        m_contacts_scratch[i].depth = depth;
                    }
                }
            }
            if (contain) {
                V3 l = to_local(p);
                V3 nrm{};
                bool hit = false;
                const Real lo = ri;
                const V3 hi = m_size - V3{ ri, ri, ri };
                if (l.x < lo) { l.x = lo; nrm = nrm + V3{ 1.0_r, 0.0_r, 0.0_r }; hit = true; }
                if (l.x > hi.x) { l.x = hi.x; nrm = nrm + V3{ -1.0_r, 0.0_r, 0.0_r }; hit = true; }
                if (l.y < lo) { l.y = lo; nrm = nrm + V3{ 0.0_r, 1.0_r, 0.0_r }; hit = true; }
                if (l.y > hi.y) { l.y = hi.y; nrm = nrm + V3{ 0.0_r, -1.0_r, 0.0_r }; hit = true; }
                if (l.z < lo) { l.z = lo; nrm = nrm + V3{ 0.0_r, 0.0_r, 1.0_r }; hit = true; }
                if (l.z > hi.z) { l.z = hi.z; nrm = nrm + V3{ 0.0_r, 0.0_r, -1.0_r }; hit = true; }
                if (hit) {
                    p = to_world(l);
                    if (m_contact_of[i] == kNoContact) {
                        m_contact_of[i] = kBoundsContact;
                        m_contacts_scratch[i].normal = vec3::normalize(world_dir(nrm));
                    }
                }
            }
            m_pp[i] = p;
        }
    });
}

void FluidVolume3D::respond_particles(Real h) {
    static_cast<void>(h);
    const FluidMaterial& mat = m_def.material;
    const Real coupling = m_def.coupling.two_way ? m_def.coupling.push_scale : 0.0_r;
    for (NearBody& nb : m_near) {
        nb.impulse = V3{};
        nb.angular = V3{};
    }
    std::size_t contacts = 0;
    for (std::size_t i = 0; i < m_px.size(); ++i) {
        const std::int32_t k = m_contact_of[i];
        if (k == kNoContact) continue;
        ++contacts;
        const V3 n = m_contacts_scratch[i].normal;
        V3 vb{};
        Real e = mat.restitution;
        Real mu = mat.friction;
        Real threshold = 0.3_r;
        NearBody* nb = nullptr;
        if (k >= 0) {
            nb = &m_near[static_cast<std::size_t>(k)];
            const Collider3D& c = nb->body->colliders()[m_contacts_scratch[i].collider];
            vb = nb->body->get_velocity_at(m_pp[i]);
            e = std::max(e, c.material.restitution);
            mu = combine_friction(mat.friction, c.material.friction);
            threshold = std::min(threshold, c.material.restitution_threshold);
        } else {
            mu = mat.friction;
        }
        const V3 rel_in = m_pv0[i] - vb;
        const Real vn_in = vec3::dot(rel_in, n);
        const V3 rel = m_pv[i] - vb;
        const Real vn = vec3::dot(rel, n);
        V3 vt = rel - n * vn;
        Real vn_out = std::max(vn, 0.0_r);
        if (vn_in < -threshold && e > 0.0_r) vn_out = std::max(vn_out, -e * vn_in);
        const Real jn = std::max(0.0_r, vn_out - std::min(vn_in, 0.0_r));
        const Real lt = norm(vt);
        if (lt > 1e-9_r) vt = vt * std::max(0.0_r, 1.0_r - mu * jn / lt);
        const V3 v_new = vb + n * vn_out + vt;
        if (nb && nb->dynamic && coupling > 0.0_r) {
            const V3 vt_in = rel - n * vn;
            const V3 J = (n * (vn_out - std::min(vn_in, 0.0_r)) + (vt - vt_in)) * (-m_pmass * coupling);
            nb->impulse = nb->impulse + J;
            nb->angular = nb->angular + vec3::cross(m_pp[i] - nb->body->world_center(), J);
        }
        m_pv[i] = v_new;
        if (nb && nb->dynamic && vec3::length_squared(vb) > m_def.particles.wake_speed * m_def.particles.wake_speed) {
            m_calm[i] = 0;
        }
    }
    for (NearBody& nb : m_near) {
        if (!nb.dynamic || vec3::is_zero(nb.impulse)) continue;
        nb.body->apply_linear_impulse(nb.impulse);
        nb.body->apply_angular_impulse(nb.angular);
        nb.body->wake();
    }
    m_contact_count += contacts;
}

void FluidVolume3D::particle_substep(detail::FluidTaskPool& pool, Simulation3D* sim, Real h, int iterations) {
    static_cast<void>(sim);
    const std::size_t n = m_px.size();
    const V3 g = m_up * -m_g;
    const Real ri = m_radius;
    pool.parallel_range(n, 2048, [&](std::size_t b, std::size_t e) {
        for (std::size_t i = b; i < e; ++i) {
            m_contact_of[i] = kNoContact;
            if (m_asleep[i]) {
                m_pv[i] = V3{};
                m_pv0[i] = V3{};
                m_pp[i] = m_px[i];
                continue;
            }
            m_pv[i] = m_pv[i] + g * h;
            m_pv0[i] = m_pv[i];
            m_pp[i] = m_px[i] + m_pv[i] * h;
        }
    });
    build_particle_grid(pool, m_pp);
    {
        std::vector<std::uint8_t>& awake_bucket = m_chunk_flag_scratch;
        if (awake_bucket.size() != m_bucket_count.size()) awake_bucket.assign(m_bucket_count.size(), 0);
        else std::fill(awake_bucket.begin(), awake_bucket.end(), std::uint8_t{ 0 });
        for (std::size_t i = 0; i < n; ++i) {
            if (!m_asleep[i]) awake_bucket[m_pkey[i]] = 1;
        }
        pool.parallel_range(n, 2048, [&](std::size_t b, std::size_t e) {
            for (std::size_t i = b; i < e; ++i) {
                if (!m_asleep[i]) {
                    m_live[i] = 1;
                    continue;
                }
                int cx = 0, cy = 0, cz = 0;
                grid_coords(m_pp[i], cx, cy, cz);
                std::uint8_t live = 0;
                for (int z = std::max(cz - 1, 0); z <= std::min(cz + 1, m_gz - 1) && !live; ++z) {
                    for (int y = std::max(cy - 1, 0); y <= std::min(cy + 1, m_gy - 1) && !live; ++y) {
                        for (int x = std::max(cx - 1, 0); x <= std::min(cx + 1, m_gx - 1) && !live; ++x) {
                            if (awake_bucket[grid_index(x, y, z)]) live = 1;
                        }
                    }
                }
                m_live[i] = live;
            }
        });
    }
    find_neighbours(pool);
    const Real rho0 = m_def.material.density;
    const Real mr = m_pmass / rho0;
    Real lattice_grad = 0.0_r;
    {
        const int reach = static_cast<int>(std::ceil(m_h / m_spacing));
        for (int z = -reach; z <= reach; ++z) {
            for (int y = -reach; y <= reach; ++y) {
                for (int x = -reach; x <= reach; ++x) {
                    const V3 d{ static_cast<Real>(x) * m_spacing, static_cast<Real>(y) * m_spacing, static_cast<Real>(z) * m_spacing };
                    const Real r = norm(d);
                    const Real gr = kernel_grad(r) * mr;
                    lattice_grad += gr * gr;
                }
            }
        }
    }
    const Real eps = 0.01_r * lattice_grad;
    const Real stiff = std::clamp(m_def.material.stiffness, 0.05_r, 1.0_r);
    const Real ap = m_def.particles.artificial_pressure >= 0.0_r ? m_def.particles.artificial_pressure : 0.01_r;
    const Real ap_unit = ap / std::max(lattice_grad, 1e-12_r);
    const Real w0 = kernel_w(0.0_r);
    for (int it = 0; it < iterations; ++it) {
        pool.parallel_range(n, 512, [&](std::size_t b, std::size_t e) {
            for (std::size_t i = b; i < e; ++i) {
                if (!m_live[i]) {
                    m_lambda[i] = 0.0_r;
                    continue;
                }
                const V3 p = m_pp[i];
                Real rho = w0;
                V3 gi{};
                Real sum2 = 0.0_r;
                const std::uint32_t* nb = &m_nbr[i * kMaxNeighbours];
                for (std::uint16_t q = 0; q < m_nbr_count[i]; ++q) {
                    const V3 d = p - m_pp[nb[q]];
                    const Real r2 = vec3::dot(d, d);
                    rho += kernel_w(r2);
                    const Real r = std::sqrt(r2);
                    if (r > 1e-9_r) {
                        const V3 gw = d * (kernel_grad(r) * mr / r);
                        gi = gi + gw;
                        sum2 += vec3::dot(gw, gw);
                    }
                }
                rho *= m_pmass;
                m_density[i] = rho;
                const Real C = std::max(rho / rho0 - 1.0_r, 0.0_r);
                m_lambda[i] = -stiff * C / (sum2 + vec3::dot(gi, gi) + eps);
            }
        });
        pool.parallel_range(n, 512, [&](std::size_t b, std::size_t e) {
            for (std::size_t i = b; i < e; ++i) {
                m_dp[i] = V3{};
                if (m_asleep[i] || !m_live[i]) continue;
                const V3 p = m_pp[i];
                V3 dp{};
                const std::uint32_t* nb = &m_nbr[i * kMaxNeighbours];
                for (std::uint16_t q = 0; q < m_nbr_count[i]; ++q) {
                    const std::uint32_t j = nb[q];
                    const V3 d = p - m_pp[j];
                    const Real r2 = vec3::dot(d, d);
                    const Real r = std::sqrt(r2);
                    if (r <= 1e-9_r) continue;
                    const Real ratio = kernel_w(r2) / m_wq;
                    const Real corr = -ap_unit * ratio * ratio * ratio * ratio;
                    dp = dp + d * ((m_lambda[i] + m_lambda[j] + corr) * kernel_grad(r) * mr / r);
                }
                m_dp[i] = dp;
            }
        });
        pool.parallel_range(n, 2048, [&](std::size_t b, std::size_t e) {
            for (std::size_t i = b; i < e; ++i) m_pp[i] = m_pp[i] + m_dp[i];
        });
        collide_particles(pool, h, ri);
    }
    const Real inv_h = 1.0_r / h;
    pool.parallel_range(n, 2048, [&](std::size_t b, std::size_t e) {
        for (std::size_t i = b; i < e; ++i) {
            if (m_asleep[i]) continue;
            m_pv[i] = (m_pp[i] - m_px[i]) * inv_h;
        }
    });
    respond_particles(h);
    const FluidMaterial& mat = m_def.material;
    const Real xsph = m_def.particles.xsph >= 0.0_r ? m_def.particles.xsph : std::clamp(0.02_r + 0.15_r * std::log10(std::max(mat.viscosity, 1e-9_r) / 1.0e-3_r), 0.02_r, 0.9_r);
    const Real coh = m_def.particles.cohesion >= 0.0_r ? m_def.particles.cohesion : 0.1_r * mat.surface_tension / 0.072_r;
    const bool vort = m_def.particles.vorticity && m_def.particles.vorticity_strength > 0.0_r;
    if (vort) {
        pool.parallel_range(n, 512, [&](std::size_t b, std::size_t e) {
            for (std::size_t i = b; i < e; ++i) {
                V3 w{};
                if (!m_asleep[i]) {
                    const std::uint32_t* nb = &m_nbr[i * kMaxNeighbours];
                    for (std::uint16_t q = 0; q < m_nbr_count[i]; ++q) {
                        const std::uint32_t j = nb[q];
                        const V3 d = m_pp[i] - m_pp[j];
                        const Real r = norm(d);
                        if (r <= 1e-9_r) continue;
                        w = w + vec3::cross(m_pv[j] - m_pv[i], d * (kernel_grad(r) / r));
                    }
                }
                m_omega[i] = w;
            }
        });
    }
    const Real coh_scale = coh * m_g * h / 10.0_r;
    const Real cut = m_spacing;
    pool.parallel_range(n, 512, [&](std::size_t b, std::size_t e) {
        for (std::size_t i = b; i < e; ++i) {
            m_dp[i] = m_pv[i];
            if (m_asleep[i]) continue;
            const std::uint32_t* nb = &m_nbr[i * kMaxNeighbours];
            V3 avg{};
            Real wsum = w0;
            V3 pull{};
            V3 eta{};
            for (std::uint16_t q = 0; q < m_nbr_count[i]; ++q) {
                const std::uint32_t j = nb[q];
                const V3 d = m_pp[j] - m_pp[i];
                const Real r2 = vec3::dot(d, d);
                const Real w = kernel_w(r2);
                avg = avg + (m_pv[j] - m_pv[i]) * w;
                wsum += w;
                const Real r = std::sqrt(r2);
                if (r > cut && r < m_h) {
                    const Real t = (r - cut) / (m_h - cut);
                    pull = pull + d * (4.0_r * t * (1.0_r - t) / r);
                }
                if (vort && r > 1e-9_r) eta = eta + d * (-norm(m_omega[j]) * kernel_grad(r) / r);
            }
            V3 v = m_pv[i] + avg * (xsph / wsum);
            if (coh > 0.0_r) v = v + pull * coh_scale;
            if (vort) {
                const Real el = norm(eta);
                if (el > 1e-9_r) v = v + vec3::cross(eta * (1.0_r / el), m_omega[i]) * (m_def.particles.vorticity_strength * h);
            }
            m_dp[i] = v;
        }
    });
    pool.parallel_range(n, 2048, [&](std::size_t b, std::size_t e) {
        for (std::size_t i = b; i < e; ++i) {
            if (m_asleep[i]) continue;
            m_pv[i] = m_dp[i];
            m_px[i] = m_pp[i];
        }
    });
}

void FluidVolume3D::step_particles(detail::FluidTaskPool& pool, Real dt, FluidStats3D& stats) {
    run_emitters(dt);
    m_contact_count = 0;
    const std::size_t n = m_px.size();
    if (n == 0) {
        m_sx = m_sz = 0;
        return;
    }
    if (m_all_dirty) {
        std::fill(m_asleep.begin(), m_asleep.end(), std::uint8_t{ 0 });
        std::fill(m_calm.begin(), m_calm.end(), std::uint16_t{ 0 });
        m_all_dirty = false;
    }
    reorder_particles(pool);
    for (const NearBody& nb : m_near) {
        if (!nb.dynamic) continue;
        const Real sp = vec3::length(nb.body->linear_velocity()) + vec3::length(nb.body->angular_velocity()) * nb.body->colliders().front().shape.bounding_radius;
        if (sp <= m_def.particles.wake_speed) continue;
        const AABB3D box = nb.box.fatten(m_h);
        for (std::size_t i = 0; i < n; ++i) {
            if (m_asleep[i] && box.contains(m_px[i])) {
                m_asleep[i] = 0;
                m_calm[i] = 0;
            }
        }
    }
    Real vmax = 0.0_r;
    std::size_t awake = 0;
    for (std::size_t i = 0; i < n; ++i) {
        if (m_asleep[i]) continue;
        ++awake;
        vmax = std::max(vmax, vec3::length_squared(m_pv[i]));
    }
    vmax = std::sqrt(vmax) + m_g * dt;
    int iters = m_def.particles.iterations > 0 ? m_def.particles.iterations : m_quality.solver_iterations;
    iters = std::max(1, iters);
    const int cap = std::max(1, m_def.particles.max_substeps > 0 ? m_def.particles.max_substeps : m_quality.max_substeps);
    const Real cfl = std::clamp(m_quality.cfl, 0.05_r, 1.0_r) * m_spacing;
    int sub = std::clamp(static_cast<int>(std::ceil(dt * vmax / std::max(cfl, 1e-6_r))), 1, cap);
    if (awake > 0) {
        const std::size_t budget = std::max<std::size_t>(m_quality.particle_work_budget, 1);
        while (awake * static_cast<std::size_t>(iters * sub) > budget && iters > 1) --iters;
        while (awake * static_cast<std::size_t>(iters * sub) > budget && sub > 1) --sub;
        const Real h = dt / static_cast<Real>(sub);
        for (int s = 0; s < sub; ++s) particle_substep(pool, nullptr, h, iters);
    }
    stats.substeps = std::max(stats.substeps, sub);
    stats.iterations = std::max(stats.iterations, iters);
    const FluidParticleSettings& ps = m_def.particles;
    if (m_def.allow_sleep) {
        const Real s2 = ps.sleep_speed * ps.sleep_speed;
        const Real w2 = ps.wake_speed * ps.wake_speed;
        for (std::size_t i = 0; i < n; ++i) {
            if (m_asleep[i]) {
                if (!m_live[i]) continue;
                const std::uint32_t* nb = &m_nbr[i * kMaxNeighbours];
                for (std::uint16_t q = 0; q < m_nbr_count[i]; ++q) {
                    const std::uint32_t j = nb[q];
                    if (!m_asleep[j] && vec3::length_squared(m_pv[j]) > w2) {
                        m_asleep[i] = 0;
                        m_calm[i] = 0;
                        break;
                    }
                }
                continue;
            }
            if (vec3::length_squared(m_pv[i]) < s2) {
                if (m_calm[i] < 65535) ++m_calm[i];
                if (m_calm[i] >= ps.sleep_frames) {
                    m_asleep[i] = 1;
                    m_pv[i] = V3{};
                }
            } else {
                m_calm[i] = 0;
            }
        }
    }
    if (ps.kill_outside && !ps.contain) {
        std::size_t w = 0;
        for (std::size_t i = 0; i < m_px.size(); ++i) {
            const V3 l = to_local(m_px[i]);
            const bool keep = l.x >= -m_h && l.z >= -m_h && l.y >= -m_h && l.x <= m_size.x + m_h && l.z <= m_size.z + m_h && l.y <= m_size.y + m_h;
            if (!keep) continue;
            if (w != i) {
                m_px[w] = m_px[i];
                m_pv[w] = m_pv[i];
                m_density[w] = m_density[i];
                m_calm[w] = m_calm[i];
                m_asleep[w] = m_asleep[i];
            }
            ++w;
        }
        if (w != m_px.size()) {
            const std::size_t keep = w;
            m_px.resize(keep);
            m_pv.resize(keep);
            m_pp.resize(keep);
            m_pv0.resize(keep);
            m_dp.resize(keep);
            m_omega.resize(keep);
            m_lambda.resize(keep);
            m_density.resize(keep);
            m_calm.resize(keep);
            m_asleep.resize(keep);
            m_live.resize(keep);
            m_nbr_count.resize(keep);
            m_pkey.resize(keep);
            m_sorted.resize(keep);
            m_contact_of.resize(keep);
            m_contacts_scratch.resize(keep);
            build_particle_grid(pool, m_px);
        }
    }
    m_awake_count = 0;
    for (std::uint8_t a : m_asleep) m_awake_count += a ? 0u : 1u;
    build_surface_grid(ps.surface_cell);
}

void FluidVolume3D::build_surface_grid(Real cell) {
    const Real c = cell > 0.0_r ? cell : 2.0_r * m_spacing;
    m_scell = c;
    m_sx = std::max(1, static_cast<int>(std::ceil(m_size.x / c)));
    m_sz = std::max(1, static_cast<int>(std::ceil(m_size.z / c)));
    m_sgrid.assign(static_cast<std::size_t>(m_sx) * static_cast<std::size_t>(m_sz), -1.0e6_r);
    for (const V3& p : m_px) {
        const V3 l = to_local(p);
        const int i = std::clamp(static_cast<int>(std::floor(l.x / c)), 0, m_sx - 1);
        const int k = std::clamp(static_cast<int>(std::floor(l.z / c)), 0, m_sz - 1);
        Real& s = m_sgrid[static_cast<std::size_t>(k) * static_cast<std::size_t>(m_sx) + static_cast<std::size_t>(i)];
        s = std::max(s, l.y + m_radius);
    }
}

Real FluidVolume3D::particle_density_at(const V3& point) const {
    if (m_px.empty() || m_bucket_count.empty() || m_sorted.size() != m_px.size()) return 0.0_r;
    Real rho = 0.0_r;
    visit_near(point, [&](std::uint32_t j) {
        const V3 d = m_px[j] - point;
        rho += kernel_w(vec3::dot(d, d));
    });
    return rho * m_pmass;
}

Real FluidVolume3D::particle_ambient(const RigidBody3D& body) const {
    if (m_sx <= 0 || m_sgrid.empty()) return -1.0e30_r;
    V3 lo{}, hi{};
    if (!local_footprint(body, lo, hi)) return -1.0e30_r;
    const int i0 = static_cast<int>(std::floor(lo.x / m_scell)) - 1, i1 = static_cast<int>(std::floor(hi.x / m_scell)) + 1;
    const int k0 = static_cast<int>(std::floor(lo.z / m_scell)) - 1, k1 = static_cast<int>(std::floor(hi.z / m_scell)) + 1;
    Real sum = 0.0_r;
    int count = 0;
    for (int k = k0; k <= k1; ++k) {
        for (int i = i0; i <= i1; ++i) {
            if (i != i0 && i != i1 && k != k0 && k != k1) continue;
            if (i < 0 || k < 0 || i >= m_sx || k >= m_sz) continue;
            const Real s = m_sgrid[static_cast<std::size_t>(k) * static_cast<std::size_t>(m_sx) + static_cast<std::size_t>(i)];
            if (s < -1.0e5_r) continue;
            sum += s;
            ++count;
        }
    }
    return count > 0 ? sum / static_cast<Real>(count) : -1.0e30_r;
}

V3 FluidVolume3D::particle_velocity_at(const V3& point) const {
    if (m_px.empty() || m_bucket_count.empty() || m_sorted.size() != m_px.size()) return V3{};
    V3 sum{};
    Real wsum = 0.0_r;
    visit_near(point, [&](std::uint32_t j) {
        const V3 d = m_px[j] - point;
        const Real w = kernel_w(vec3::dot(d, d));
        sum = sum + m_pv[j] * w;
        wsum += w;
    });
    return wsum > 0.0_r ? sum * (1.0_r / wsum) : V3{};
}

} // namespace physics
} // namespace fizmo
