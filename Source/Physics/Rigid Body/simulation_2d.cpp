#include "fizmo_library.hpp"
#include "simulation_2d.hpp"

namespace fizmo {
namespace physics {
namespace narrowphase {

void project_polygon(const PolygonShape& poly, const Transform2D& xf, const RVec2& axis, Real& lo, Real& hi) {
    RVec2 v = xf.apply(poly.vertices[0]);
    lo = hi = fast::dot(axis, v);

    for (std::size_t i = 1; i < poly.count; ++i) {
        v = xf.apply(poly.vertices[i]);
        Real d = fast::dot(axis, v);
        if (d < lo) lo = d;
        if (d > hi) hi = d;
    }
}

void project_capsule(const CapsuleShape& cap, const Transform2D& xf, const RVec2& axis, Real& lo, Real& hi) {
    RVec2 p1 = xf.apply(cap.point1);
    RVec2 p2 = xf.apply(cap.point2);
    Real d1 = fast::dot(axis, p1);
    Real d2 = fast::dot(axis, p2);
    lo = std::min(d1, d2) - cap.radius;
    hi = std::max(d1, d2) + cap.radius;
}

RVec2 closest_point_on_segment(const RVec2& p, const RVec2& a, const RVec2& b) {
    RVec2 ab = b - a;
    Real t = fast::length_squared(ab);
    if (t <= constants::middle_epsilon()) return a;
    t = std::clamp(fast::dot(p - a, ab) / t, 0.0_r, 1.0_r);
    return a + ab * t;
}

bool collide_circle_circle(
    const CircleShape& ca, const Transform2D& xa,
    const CircleShape& cb, const Transform2D& xb,
    ContactManifold& m
) {
    RVec2 cA = xa.apply(ca.center);
    RVec2 cB = xb.apply(cb.center);
    RVec2 d = cB - cA;
    Real dist_sq = fast::length_squared(d);
    Real r_sum = ca.radius + cb.radius;
    if (dist_sq >= r_sum * r_sum) return false;
    Real dist = std::sqrt(dist_sq);
    m.normal = (dist >= constants::middle_epsilon()) ? d * (1.0_r / dist) : RVec2{0.0_r, 1.0_r};
    Real pen = r_sum - dist;
    m.point_count = 1;
    m.points[0].position = cA + m.normal * (ca.radius - pen * 0.5_r);
    m.points[0].separation = -pen;
    return true;
}

bool collide_circle_polygon(
    const CircleShape& circ, const Transform2D& xc,
    const PolygonShape& poly, const Transform2D& xp,
    ContactManifold& m, bool flip
) {
    RVec2 c_world = xc.apply(circ.center);
    RVec2 c_local = xp.apply_inverse(c_world);
    Real best_sep = -std::numeric_limits<Real>::max();
    std::size_t best_i = 0;

    for (std::size_t i = 0; i < poly.count; ++i) {
        Real sep = fast::dot(poly.normals[i], c_local - poly.vertices[i]);
        if (sep > circ.radius) return false;
        if (sep > best_sep) { best_sep = sep; best_i = i; }
    }

    std::size_t i1 = best_i;
    std::size_t i2 = (i1 + 1) % poly.count;
    const RVec2& v1 = poly.vertices[i1];
    const RVec2& v2 = poly.vertices[i2];
    RVec2 closest;
    Real u1 = fast::dot(c_local - v1, v2 - v1);
    Real u2 = fast::dot(c_local - v2, v1 - v2);

    if (u1 <= 0.0_r) {
        closest = v1;
    } else if (u2 <= 0.0_r) {
        closest = v2;
    } else {
        closest = v1 + (v2 - v1) * (u1 / (u1 + u2));
    }

    RVec2 delta = c_local - closest;
    Real dist_sq = fast::length_squared(delta);
    if (dist_sq > circ.radius * circ.radius && best_sep > 0.0_r) return false;
    RVec2 n_local; 
    Real dist;      

    if (best_sep <= constants::middle_epsilon()) {
        n_local = poly.normals[best_i];
        dist = best_sep;
    } else {
        dist = std::sqrt(dist_sq);
        n_local = delta * (1.0_r / dist);
    }

    RVec2 n_out = xp.rotate(n_local);
    RVec2 n = n_out * -1.0_r; 
    m.normal = flip ? n * -1.0_r : n;
    m.point_count = 1;
    m.points[0].position = c_world - n_out * ((circ.radius + dist) * 0.5_r);
    m.points[0].separation = dist - circ.radius;
    return true;
}

} // namespace narrowphase
} // namespace physics
} // namespace fizmo

namespace fizmo {
namespace physics {
namespace narrowphase {
namespace detail_sat {

RoundPolygon make_round_polygon(const PolygonShape& p, const Transform2D& xf) {
    RoundPolygon r;
    r.count = p.count;

    for (std::size_t i = 0; i < p.count; ++i) {
        r.vertices[i] = xf.apply(p.vertices[i]);
        r.normals[i]  = xf.rotate(p.normals[i]);
    }

    return r;
}

RoundPolygon make_round_segment(const RVec2& a, const RVec2& b, Real radius) {
    RoundPolygon r;
    r.count = 2;
    r.radius = radius;
    r.vertices[0] = a;
    r.vertices[1] = b;
    RVec2 e = b - a;
    Real len = fast::length(e);
    RVec2 n = (len > constants::middle_epsilon()) ? RVec2(e.y / len, -e.x / len) : RVec2(0.0_r, 1.0_r);
    r.normals[0] = n;
    r.normals[1] = n * -1.0_r;
    return r;
}

AxisResult find_max_separation(const RoundPolygon& a, const RoundPolygon& b) {
    AxisResult best{-std::numeric_limits<Real>::max(), 0};

    for (std::size_t i = 0; i < a.count; ++i) {
        const RVec2& n = a.normals[i];
        const RVec2& v = a.vertices[i];
        Real si = std::numeric_limits<Real>::max();

        for (std::size_t j = 0; j < b.count; ++j) {
            Real d = fast::dot(n, b.vertices[j] - v);
            if (d < si) si = d;
        }

        if (si > best.separation) { best.separation = si; best.edge_index = i; }
    }

    return best;
}

SegmentDistance segment_distance(const RVec2& p1, const RVec2& q1, const RVec2& p2, const RVec2& q2) {
    RVec2 d1 = q1 - p1, d2 = q2 - p2, r = p1 - p2;
    Real a = fast::dot(d1, d1), e = fast::dot(d2, d2), f = fast::dot(d2, r);
    Real s, t;
    const Real eps = constants::middle_epsilon();

    if (a <= eps && e <= eps) {
        s = t = 0.0_r;
    } else if (a <= eps) {
        s = 0.0_r; t = std::clamp(f / e, 0.0_r, 1.0_r);
    } else {
        Real c = fast::dot(d1, r);

        if (e <= eps) {
            t = 0.0_r; s = std::clamp(-c / a, 0.0_r, 1.0_r);
        } else {
            Real b = fast::dot(d1, d2);
            Real denom = a * e - b * b;
            s = (denom > eps) ? std::clamp((b * f - c * e) / denom, 0.0_r, 1.0_r) : 0.0_r;
            t = (b * s + f) / e;
            if (t < 0.0_r) { t = 0.0_r; s = std::clamp(-c / a, 0.0_r, 1.0_r); }
            else if (t > 1.0_r) { t = 1.0_r; s = std::clamp((b - c) / a, 0.0_r, 1.0_r); }
        }
    }

    SegmentDistance out;
    out.closest1 = p1 + d1 * s;
    out.closest2 = p2 + d2 * t;
    out.fraction1 = s;
    out.fraction2 = t;
    out.distance_squared = fast::length_squared(out.closest2 - out.closest1);
    return out;
}

bool collide_round_polygons(const RoundPolygon& a, const RoundPolygon& b, ContactManifold& m) {
    constexpr Real feature_tolerance = 0.1_r * 0.005_r; // a tenth of the default linear slop
    const Real radius = a.radius + b.radius;
    AxisResult sep_a = find_max_separation(a, b);
    if (sep_a.separation > radius) return false;
    AxisResult sep_b = find_max_separation(b, a);
    if (sep_b.separation > radius) return false;
    const bool flip = sep_b.separation > sep_a.separation + feature_tolerance;
    const RoundPolygon& ref = flip ? b : a;
    const RoundPolygon& inc = flip ? a : b;
    const std::size_t i11 = flip ? sep_b.edge_index : sep_a.edge_index;
    const std::size_t i12 = (i11 + 1) % ref.count;
    const RVec2 normal = ref.normals[i11];
    std::size_t i21 = 0;
    Real min_dot = std::numeric_limits<Real>::max();

    for (std::size_t i = 0; i < inc.count; ++i) {
        Real d = fast::dot(normal, inc.normals[i]);
        if (d < min_dot) { min_dot = d; i21 = i; }
    }

    const std::size_t i22 = (i21 + 1) % inc.count;
    const RVec2 v11 = ref.vertices[i11], v12 = ref.vertices[i12];
    const RVec2 v21 = inc.vertices[i21], v22 = inc.vertices[i22];
    const Real r1 = ref.radius, r2 = inc.radius;
    m.point_count = 0;

    if (std::max(sep_a.separation, sep_b.separation) > feature_tolerance) {
        SegmentDistance sd = segment_distance(v11, v12, v21, v22);
        bool end1 = sd.fraction1 == 0.0_r || sd.fraction1 == 1.0_r;
        bool end2 = sd.fraction2 == 0.0_r || sd.fraction2 == 1.0_r;

        if (end1 && end2) { 
            if (sd.distance_squared > radius * radius) return false;
            Real dist = std::sqrt(sd.distance_squared);
            if (dist <= constants::middle_epsilon()) return false;
            RVec2 n = (sd.closest2 - sd.closest1) * (1.0_r / dist); 
            RVec2 p_ref = sd.closest1 + n * r1;
            RVec2 p_inc = sd.closest2 - n * r2;
            m.normal = flip ? n * -1.0_r : n;
            m.point_count = 1;
            m.points[0].position = (p_ref + p_inc) * 0.5_r;
            m.points[0].separation = dist - radius;
            return true;
        }
    }

    const RVec2 tangent(-normal.y, normal.x); 
    const Real lower1 = 0.0_r;
    const Real upper1 = fast::dot(v12 - v11, tangent);
    const Real upper2 = fast::dot(v21 - v11, tangent);
    const Real lower2 = fast::dot(v22 - v11, tangent);
    const Real span = upper2 - lower2;
    RVec2 v_lower = v22, v_upper = v21;
    if (lower2 < lower1 && span > constants::middle_epsilon()) v_lower = v22 + (v21 - v22) * ((lower1 - lower2) / span);
    if (upper2 > upper1 && span > constants::middle_epsilon()) v_upper = v22 + (v21 - v22) * ((upper1 - lower2) / span);
    const Real sep_lower = fast::dot(v_lower - v11, normal);
    const Real sep_upper = fast::dot(v_upper - v11, normal);
    v_lower = v_lower + normal * (0.5_r * (r1 - r2 - sep_lower));
    v_upper = v_upper + normal * (0.5_r * (r1 - r2 - sep_upper));

    if (sep_lower - radius <= 0.0_r) {
        m.points[m.point_count].position = v_lower;
        m.points[m.point_count].separation = sep_lower - radius;
        ++m.point_count;
    }

    if (sep_upper - radius <= 0.0_r) {
        m.points[m.point_count].position = v_upper;
        m.points[m.point_count].separation = sep_upper - radius;
        ++m.point_count;
    }

    m.normal = flip ? normal * -1.0_r : normal;
    return m.point_count > 0;
}

} // namespace detail_sat
} // namespace narrowphase
} // namespace physics
} // namespace fizmo

namespace fizmo {
namespace physics {
namespace narrowphase {

bool collide_polygon_polygon(
    const PolygonShape& pa, const Transform2D& xa,
    const PolygonShape& pb, const Transform2D& xb,
    ContactManifold& m
) {
    return detail_sat::collide_round_polygons(detail_sat::make_round_polygon(pa, xa), detail_sat::make_round_polygon(pb, xb), m);
}

bool collide_circle_capsule(
    const CircleShape& circ, const Transform2D& xc,
    const CapsuleShape& cap, const Transform2D& xk,
    ContactManifold& m, bool flip
) {
    RVec2 cc = xc.apply(circ.center);
    RVec2 p1 = xk.apply(cap.point1);
    RVec2 p2 = xk.apply(cap.point2);
    RVec2 closest = closest_point_on_segment(cc, p1, p2);
    CircleShape phantom;
    phantom.center = {};
    phantom.radius = cap.radius;
    Transform2D phantom_xf;
    phantom_xf.position = closest;
    phantom_xf.cos_a = 1.0_r; phantom_xf.sin_a = 0.0_r;
    bool hit = collide_circle_circle(circ, xc, phantom, phantom_xf, m);
    if (hit && flip) m.normal = m.normal * -1.0_r;
    return hit;
}

bool collide_polygon_capsule(
    const PolygonShape& poly, const Transform2D& xp,
    const CapsuleShape& cap, const Transform2D& xk,
    ContactManifold& m, bool flip
) {
    bool hit = detail_sat::collide_round_polygons(
        detail_sat::make_round_polygon(poly, xp),
        detail_sat::make_round_segment(xk.apply(cap.point1), xk.apply(cap.point2), cap.radius),
        m
    );

    if (hit && flip) m.normal = m.normal * -1.0_r;
    return hit;
}

bool collide_capsule_capsule(
    const CapsuleShape& ca, const Transform2D& xa,
    const CapsuleShape& cb, const Transform2D& xb,
    ContactManifold& m
) {
    return detail_sat::collide_round_polygons(
        detail_sat::make_round_segment(xa.apply(ca.point1), xa.apply(ca.point2), ca.radius),
        detail_sat::make_round_segment(xb.apply(cb.point1), xb.apply(cb.point2), cb.radius),
        m
    );
}

bool collide_edge_circle(
    const EdgeShape& edge, const Transform2D& xe,
    const CircleShape& circ, const Transform2D& xc,
    ContactManifold& m, bool flip
) {
    RVec2 c = xc.apply(circ.center);
    RVec2 v1 = xe.apply(edge.v1);
    RVec2 v2 = xe.apply(edge.v2);
    RVec2 closest = closest_point_on_segment(c, v1, v2);
    RVec2 d = c - closest;
    Real dist_sq = fast::length_squared(d);
    Real r = circ.radius;
    if (dist_sq > r * r) return false;
    Real dist = std::sqrt(dist_sq);
    RVec2 n = (dist >= constants::middle_epsilon()) ? d * (1.0_r / dist) : xe.rotate(edge.normal());
    m.normal = flip ? n * -1.0_r : n;
    m.point_count = 1;
    m.points[0].position = (closest + (c - n * r)) * 0.5_r;
    m.points[0].separation = dist - r;
    return true;
}

bool collide_edge_polygon(
    const EdgeShape& edge, const Transform2D& xe,
    const PolygonShape& poly, const Transform2D& xp,
    ContactManifold& m, bool flip
) {
    bool hit = detail_sat::collide_round_polygons(
        detail_sat::make_round_segment(xe.apply(edge.v1), xe.apply(edge.v2), 0.0_r),
        detail_sat::make_round_polygon(poly, xp),
        m
    );

    if (hit && flip) m.normal = m.normal * -1.0_r;
    return hit;
}

bool collide_edge_capsule(
    const EdgeShape& edge, const Transform2D& xe,
    const CapsuleShape& cap, const Transform2D& xk,
    ContactManifold& m, bool flip
) {
    bool hit = detail_sat::collide_round_polygons(
        detail_sat::make_round_segment(xe.apply(edge.v1), xe.apply(edge.v2), 0.0_r),
        detail_sat::make_round_segment(xk.apply(cap.point1), xk.apply(cap.point2), cap.radius),
        m
    );

    if (hit && flip) m.normal = m.normal * -1.0_r;
    return hit;
}

} // namespace narrowphase
} // namespace physics
} // namespace fizmo

namespace fizmo {
namespace physics {

bool collide_shapes(
    const Shape2D& sa, const Transform2D& xa,
    const Shape2D& sb, const Transform2D& xb,
    ContactManifold& manifold
) {
    using ST = ShapeType;
    auto type_pair = [](ST a, ST b) -> int { return (static_cast<int>(a) << 8) | static_cast<int>(b); };
    bool flip = false;
    const Shape2D* a = &sa;
    const Shape2D* b = &sb;
    const Transform2D* ta = &xa;
    const Transform2D* tb = &xb;

    if (static_cast<int>(a->type) > static_cast<int>(b->type)) {
        std::swap(a, b);
        std::swap(ta, tb);
        flip = true;
    }

    bool hit = false;

    switch (type_pair(a->type, b->type)) {
        case 0x0000: // circle-circle
            hit = narrowphase::collide_circle_circle(a->circle, *ta, b->circle, *tb, manifold);
            break;
        case 0x0001: // circle-polygon
            hit = narrowphase::collide_circle_polygon(a->circle, *ta, b->polygon, *tb, manifold, false);
            break;
        case 0x0002: // circle-capsule
            hit = narrowphase::collide_circle_capsule(a->circle, *ta, b->capsule, *tb, manifold, false);
            break;
        case 0x0003: // circle-edge
            hit = narrowphase::collide_edge_circle(b->edge, *tb, a->circle, *ta, manifold, true);
            break;
        case 0x0101: // polygon-polygon
            hit = narrowphase::collide_polygon_polygon(a->polygon, *ta, b->polygon, *tb, manifold);
            break;
        case 0x0102: // polygon-capsule
            hit = narrowphase::collide_polygon_capsule(a->polygon, *ta, b->capsule, *tb, manifold, false);
            break;
        case 0x0103: // polygon-edge
            hit = narrowphase::collide_edge_polygon(b->edge, *tb, a->polygon, *ta, manifold, true);
            break;
        case 0x0202: // capsule-capsule
            hit = narrowphase::collide_capsule_capsule(a->capsule, *ta, b->capsule, *tb, manifold);
            break;
        case 0x0203: // capsule-edge
            hit = narrowphase::collide_edge_capsule(b->edge, *tb, a->capsule, *ta, manifold, true);
            break;
        case 0x0303: // edge-edge (no response)
            return false;
        default:
            return false;
    }

    if (hit && flip) manifold.normal = manifold.normal * -1.0_r;
    return hit;
}

Simulation2D::Simulation2D(const Config& cfg) : m_cfg(cfg),
          m_tree(AABB(
              {-cfg.world_half_extent, -cfg.world_half_extent},
              { cfg.world_half_extent,  cfg.world_half_extent}
          ))
    {}

void Simulation2D::remove_body(RigidBody2D& body) {
    if (m_locked) { m_pending_removals.push_back(&body); return; }
    remove_body_now(&body);
    flush_pending_removals();
}

void Simulation2D::step(Real dt) {
    if (dt <= 0.0_r) return;
    const int n = std::max(1, m_cfg.substeps);
    const Real h = dt / static_cast<Real>(n);
    for (int i = 0; i < n; ++i) step_substep(h);
}

void Simulation2D::step_substep(Real dt) {
    ++m_query_epoch;
    m_locked = true;
    m_last_begin.clear();
    m_last_end.clear();
    m_stats = Simulation2DStats{};

    for (auto& bp : m_bodies) {
        RigidBody2D& b = *bp;
        if (!has_flag(b.flags(), PhysicsFlags::Enabled)) continue;
        if (!b.is_awake()) continue;
        b.ensure_mass_uptodate();
        b.integrate_forces(dt, m_cfg.gravity);
    }

    if (m_cfg.dynamic_tree) tree_broadphase();
    else broadphase();
    narrowphase();
    build_islands_and_wake();
    build_velocity_constraints();
    if (m_cfg.warm_starting) warm_start();
    for (int i = 0; i < m_cfg.velocity_iters; ++i) { solve_velocity_constraints(); }
    store_impulses();

    for (auto& bp : m_bodies) {
        RigidBody2D& b = *bp;
        if (!has_flag(b.flags(), PhysicsFlags::Enabled)) continue;
        if (!b.is_awake()) continue;
        b.integrate_velocities(dt);
    }

    if (m_cfg.continuous) solve_continuous();

    for (int i = 0; i < m_cfg.position_iters; ++i) { if (solve_position_constraints()) break; }

    for (auto& bp : m_bodies) {
        RigidBody2D& b = *bp;
        if (!has_flag(b.flags(), PhysicsFlags::Enabled)) continue;
        b.synchronize_colliders();
        b.clear_forces();
    }

    if (m_cfg.allow_sleeping) sleep_islands(dt);
    update_contact_cache();

    if (m_post_solve) {
        for (auto& mf : m_manifolds) {
            if (mf.body_a && mf.body_b) m_post_solve(*mf.body_a, *mf.body_b, mf);
        }
    }

    m_stats.bodies = m_bodies.size();
    m_stats.proxies = m_dtree.size();
    m_stats.pairs = m_pairs.size();
    m_stats.manifolds = m_manifolds.size();
    m_stats.tree_height = m_dtree.height();

    m_locked = false;
    dispatch_events();
    flush_pending_removals();
    cleanup_bodies();
}

auto Simulation2D::query_aabb(const AABB& region) -> std::vector<RigidBody2D*> {
    synchronize_all_colliders();
    std::vector<RigidBody2D*> ptrs;
    ptrs.reserve(m_bodies.size());
    for (auto& b : m_bodies) ptrs.push_back(b.get());
    std::vector<AABB> aabbs;
    aabbs.reserve(m_bodies.size());
    for (auto& b : m_bodies) aabbs.push_back(b->compute_body_aabb());

    m_tree.rebuild(
        AABB({-m_cfg.world_half_extent, -m_cfg.world_half_extent},
             { m_cfg.world_half_extent,  m_cfg.world_half_extent}),
        ptrs.data(), aabbs.data(), ptrs.size()
    );

    return m_tree.query(region);
}

auto Simulation2D::point_query(const RVec2& point) -> RigidBody2D* {
    AABB tiny = {{point.x - 0.001_r, point.y - 0.001_r}, {point.x + 0.001_r, point.y + 0.001_r}};
    auto results = query_aabb(tiny);

    for (auto* b : results) {
        AABB body_bb = b->compute_body_aabb();
        if (body_bb.contains(point)) return b;
    }

    return nullptr;
}

bool Simulation2D::is_active(const RigidBody2D& b) noexcept {
    if (!b.is_awake()) return false;
    if (b.inv_mass() > 0.0_r) return true;
    RVec2 v = b.linear_velocity();
    return v.x != 0.0_r || v.y != 0.0_r || b.angular_velocity() != 0.0_r;
}

bool Simulation2D::responds(const RigidBody2D& a, const RigidBody2D& b) noexcept {
    return a.has(PhysicsFlags::CollisionResponse) && b.has(PhysicsFlags::CollisionResponse) &&
          !a.has(PhysicsFlags::Intangible) && !b.has(PhysicsFlags::Intangible);
}

bool Simulation2D::wants_events(const RigidBody2D& a, const RigidBody2D& b, bool sensor) noexcept {
    if (a.has(PhysicsFlags::ContactEvents) || b.has(PhysicsFlags::ContactEvents)) return true;
    return sensor && (a.has(PhysicsFlags::SensorEvents) || b.has(PhysicsFlags::SensorEvents));
}

void Simulation2D::broadphase() {
    m_pairs.clear();
    synchronize_all_colliders();
    m_body_ptrs.clear();
    m_body_aabbs.clear();
    m_body_ptrs.reserve(m_bodies.size());
    m_body_aabbs.reserve(m_bodies.size());

    for (auto& bp : m_bodies) {
        RigidBody2D& b = *bp;
        if (!is_enabled(b)) continue;
        if (!has_flag(b.flags(), PhysicsFlags::BroadphaseActive) && !has_flag(b.flags(), PhysicsFlags::CollisionResponse)) continue;
        if (b.collision_layer() == 0) continue;
        b.ensure_mass_uptodate();
        m_body_ptrs.push_back(&b);
        m_body_aabbs.push_back(b.compute_body_aabb().fatten(m_cfg.broadphase_margin));
    }

    if (m_body_ptrs.size() < 2) return;

    AABB world_bounds(
        {-m_cfg.world_half_extent, -m_cfg.world_half_extent},
        { m_cfg.world_half_extent,  m_cfg.world_half_extent}
    );

    m_tree.rebuild(
        world_bounds,
        m_body_ptrs.data(),
        m_body_aabbs.data(),
        m_body_ptrs.size()
    );

    m_tree.find_pairs([this](RigidBody2D* a, RigidBody2D* b) {
        if (!layers_can_collide(a->collision_layer(), a->collision_mask(), b->collision_layer(), b->collision_mask())) {
            ++m_stats.layer_rejects;
            return;
        }
        if (!a->can_collide_with(*b)) return;
        if (!is_active(*a) && !is_active(*b)) return;   
        if (!is_dynamic(*a) && !is_dynamic(*b)) return; 
        if (std::less<const RigidBody2D*>()(b, a)) std::swap(a, b); 
        m_pairs.push_back({a, b});
    });
}

void Simulation2D::narrowphase() {
    m_manifolds.clear();
    m_manifolds.reserve(m_pairs.size());
    m_touching.clear();
    for (auto& [a, b] : m_pairs) { generate_contacts(a, b); }
}

void Simulation2D::generate_contacts(RigidBody2D* a, RigidBody2D* b) {
    const auto& colA = a->colliders();
    const auto& colB = b->colliders();

    for (std::size_t i = 0; i < colA.size(); ++i) {
        for (std::size_t j = 0; j < colB.size(); ++j) {
            const Collider2D& ca = colA[i];
            const Collider2D& cb = colB[j];
            if (!ca.world_aabb.overlaps(cb.world_aabb)) continue;
            if (!ca.filter.should_collide(cb.filter)) continue;
            Transform2D wa = a->transform().compose(ca.local_offset);
            Transform2D wb = b->transform().compose(cb.local_offset);
            ContactManifold mf;
            mf.body_a = a;
            mf.body_b = b;
            mf.collider_a = i;
            mf.collider_b = j;
            if (!collide_shapes(ca.shape, wa, cb.shape, wb, mf)) continue;
            mf.friction    = combined_friction(ca, cb);
            mf.restitution = combined_restitution(ca, cb);

            mf.restitution_threshold = std::min(
                ca.material.restitution_threshold,
                cb.material.restitution_threshold
            );

            for (int p = 0; p < mf.point_count; ++p) {
                ContactPoint& cp = mf.points[p];
                RVec2 half = mf.normal * (cp.separation * 0.5_r);
                cp.local_anchor_a = a->transform().apply_inverse(cp.position - half);
                cp.local_anchor_b = b->transform().apply_inverse(cp.position + half);
            }

            const bool sensor = ca.is_sensor || cb.is_sensor || !responds(*a, *b);
            CachedContact& touch = m_touching[key_of(mf)];
            touch.body_a = a;
            touch.body_b = b;
            touch.sensor = sensor;
            touch.events = wants_events(*a, *b, sensor);
            touch.normal = mf.normal;
            touch.point  = mf.points[0].position;
            if (sensor) continue;
            if (m_pre_solve) { if (!m_pre_solve(*a, *b, mf)) continue; }
            m_manifolds.push_back(mf);
        }
    }
}

void Simulation2D::build_islands_and_wake() {
    const std::size_t n = m_bodies.size();
    m_islands.reset(n);
    m_carried.clear();
    for (std::size_t i = 0; i < n; ++i) m_bodies[i]->m_sim_index = i;

    auto link = [this](RigidBody2D* a, RigidBody2D* b) {
        const bool da = is_dynamic(*a), db = is_dynamic(*b);

        if (da && db) {
            m_islands.unite(a->m_sim_index, b->m_sim_index);
        } else if (da && is_active(*b)) {
            a->wake(); 
        } else if (db && is_active(*a)) {
            b->wake();
        }
    };

    for (auto& [key, c] : m_contact_cache) {
        if (m_touching.count(key)) continue;
        if (!is_enabled(*c.body_a) || !is_enabled(*c.body_b)) continue;
        if (is_active(*c.body_a) || is_active(*c.body_b)) continue;
        m_carried.push_back(key);
        if (!c.sensor) link(c.body_a, c.body_b);
    }

    for (auto& mf : m_manifolds) link(mf.body_a, mf.body_b);
    std::vector<char> island_awake(n, 0);

    for (std::size_t i = 0; i < n; ++i) {
        const RigidBody2D& b = *m_bodies[i];
        if (is_enabled(b) && is_dynamic(b) && b.is_awake()) island_awake[m_islands.find(i)] = 1;
    }

    for (std::size_t i = 0; i < n; ++i) {
        RigidBody2D& b = *m_bodies[i];
        if (is_enabled(b) && is_dynamic(b) && !b.is_awake() && island_awake[m_islands.find(i)]) b.wake();
    }
}

void Simulation2D::sleep_islands(Real dt) {
    const std::size_t n = std::min(m_islands.size(), m_bodies.size());
    std::vector<Real> min_timer(n, std::numeric_limits<Real>::max());

    for (std::size_t i = 0; i < n; ++i) {
        RigidBody2D& b = *m_bodies[i];
        if (!is_enabled(b) || !b.is_awake()) continue;

        if (!is_dynamic(b)) {
            b.update_sleep(dt); 
            continue;
        }

        Real t = b.advance_sleep_timer(dt);
        std::size_t r = m_islands.find(i);
        if (t < min_timer[r]) min_timer[r] = t;
    }

    for (std::size_t i = 0; i < n; ++i) {
        RigidBody2D& b = *m_bodies[i];
        if (!is_enabled(b) || !b.is_awake() || !is_dynamic(b)) continue;
        if (min_timer[m_islands.find(i)] >= RigidBody2D::SLEEP_TIME_THRESHOLD) b.put_to_sleep();
    }
}

void Simulation2D::build_velocity_constraints() {
    m_constraints.clear();
    m_constraints.reserve(m_manifolds.size());
    const Real match_sq = m_cfg.warm_start_match_distance * m_cfg.warm_start_match_distance;

    for (std::size_t mi = 0; mi < m_manifolds.size(); ++mi) {
        const auto& mf = m_manifolds[mi];
        RigidBody2D* a = mf.body_a;
        RigidBody2D* b = mf.body_b;
        a->ensure_mass_uptodate();
        b->ensure_mass_uptodate();
        VelocityConstraint vc;
        vc.body_a = a;
        vc.body_b = b;
        vc.normal = mf.normal;
        vc.friction = mf.friction;
        vc.restitution = mf.restitution;
        vc.restitution_threshold = mf.restitution_threshold;
        vc.point_count = mf.point_count;
        vc.manifold_index = mi;
        Real inv_mA = a->inv_mass();
        Real inv_mB = b->inv_mass();
        Real inv_IA = a->inv_inertia();
        Real inv_IB = b->inv_inertia();
        RVec2 cA = a->world_center();
        RVec2 cB = b->world_center();
        RVec2 tangent = {-mf.normal.y, mf.normal.x};
        const CachedContact* old = nullptr;

        if (m_cfg.warm_starting) {
            auto it = m_contact_cache.find(key_of(mf));
            if (it != m_contact_cache.end() && !it->second.sensor) old = &it->second;
        }

        for (int p = 0; p < mf.point_count; ++p) {
            auto& vcp = vc.points[p];
            vcp.rA = mf.points[p].position - cA;
            vcp.rB = mf.points[p].position - cB;
            Real rnA = fast::perp_dot(vcp.rA, mf.normal);
            Real rnB = fast::perp_dot(vcp.rB, mf.normal);
            Real k_normal = inv_mA + inv_mB + inv_IA * rnA * rnA + inv_IB * rnB * rnB;
            vcp.normal_mass = (k_normal > 0.0_r) ? 1.0_r / k_normal : 0.0_r;
            Real rtA = fast::perp_dot(vcp.rA, tangent);
            Real rtB = fast::perp_dot(vcp.rB, tangent);
            Real k_tangent = inv_mA + inv_mB + inv_IA * rtA * rtA + inv_IB * rtB * rtB;
            vcp.tangent_mass = (k_tangent > 0.0_r) ? 1.0_r / k_tangent : 0.0_r;
            RVec2 vA = a->linear_velocity() + RVec2(-a->angular_velocity() * vcp.rA.y, a->angular_velocity() * vcp.rA.x);
            RVec2 vB = b->linear_velocity() + RVec2(-b->angular_velocity() * vcp.rB.y, b->angular_velocity() * vcp.rB.x);
            Real vn_rel = fast::dot(vB - vA, mf.normal);
            vcp.velocity_bias = 0.0_r;
            if (vn_rel < -vc.restitution_threshold) { vcp.velocity_bias = -vc.restitution * vn_rel; }

            if (old) {
                Real best = match_sq;

                for (int q = 0; q < old->point_count; ++q) {
                    Real d = fast::length_squared(old->local_anchor_a[q] - mf.points[p].local_anchor_a);

                    if (d <= best) {
                        best = d;
                        vcp.normal_impulse  = old->normal_impulse[q];
                        vcp.tangent_impulse = old->tangent_impulse[q];
                    }
                }
            }
        }

        m_constraints.push_back(vc);
    }
}

void Simulation2D::warm_start() {
    for (auto& vc : m_constraints) {
        RigidBody2D* a = vc.body_a;
        RigidBody2D* b = vc.body_b;
        Real inv_mA = a->inv_mass();
        Real inv_mB = b->inv_mass();
        Real inv_IA = a->inv_inertia();
        Real inv_IB = b->inv_inertia();

        for (int p = 0; p < vc.point_count; ++p) {
            auto& vcp = vc.points[p];
            RVec2 tangent = {-vc.normal.y, vc.normal.x};
            RVec2 P = vc.normal * vcp.normal_impulse + tangent * vcp.tangent_impulse;
            a->set_linear_velocity(a->linear_velocity() - P * inv_mA);
            a->set_angular_velocity(a->angular_velocity() - inv_IA * fast::perp_dot(vcp.rA, P));
            b->set_linear_velocity(b->linear_velocity() + P * inv_mB);
            b->set_angular_velocity(b->angular_velocity() + inv_IB * fast::perp_dot(vcp.rB, P));
        }
    }
}

void Simulation2D::solve_velocity_constraints() {
    for (auto& vc : m_constraints) {
        RigidBody2D* a = vc.body_a;
        RigidBody2D* b = vc.body_b;
        Real inv_mA = a->inv_mass();
        Real inv_mB = b->inv_mass();
        Real inv_IA = a->inv_inertia();
        Real inv_IB = b->inv_inertia();

        for (int p = 0; p < vc.point_count; ++p) {
            auto& vcp = vc.points[p];
            RVec2 vA = a->linear_velocity() + RVec2(-a->angular_velocity() * vcp.rA.y, a->angular_velocity() * vcp.rA.x);
            RVec2 vB = b->linear_velocity() + RVec2(-b->angular_velocity() * vcp.rB.y, b->angular_velocity() * vcp.rB.x);
            RVec2 dv = vB - vA;
            RVec2 tangent = {-vc.normal.y, vc.normal.x};
            Real vt = fast::dot(dv, tangent);
            Real lambda_t = -vt * vcp.tangent_mass;
            Real max_friction = vc.friction * vcp.normal_impulse;

            Real new_impulse_t = std::clamp(
                vcp.tangent_impulse + lambda_t,
                -max_friction, max_friction
            );

            lambda_t = new_impulse_t - vcp.tangent_impulse;
            vcp.tangent_impulse = new_impulse_t;
            RVec2 Pt = tangent * lambda_t;
            a->set_linear_velocity(a->linear_velocity() - Pt * inv_mA);
            a->set_angular_velocity(a->angular_velocity() - inv_IA * fast::perp_dot(vcp.rA, Pt));
            b->set_linear_velocity(b->linear_velocity() + Pt * inv_mB);
            b->set_angular_velocity(b->angular_velocity() + inv_IB * fast::perp_dot(vcp.rB, Pt));
        }

        if (vc.point_count == 2 && solve_block(vc)) continue;

        for (int p = 0; p < vc.point_count; ++p) {
            auto& vcp = vc.points[p];
            RVec2 vA = a->linear_velocity() + RVec2(-a->angular_velocity() * vcp.rA.y, a->angular_velocity() * vcp.rA.x);
            RVec2 vB = b->linear_velocity() + RVec2(-b->angular_velocity() * vcp.rB.y, b->angular_velocity() * vcp.rB.x);
            RVec2 dv = vB - vA;
            Real vn = fast::dot(dv, vc.normal);
            Real lambda_n = -(vn - vcp.velocity_bias) * vcp.normal_mass;
            Real new_impulse_n = std::max(vcp.normal_impulse + lambda_n, 0.0_r);
            lambda_n = new_impulse_n - vcp.normal_impulse;
            vcp.normal_impulse = new_impulse_n;
            RVec2 Pn = vc.normal * lambda_n;
            a->set_linear_velocity(a->linear_velocity() - Pn * inv_mA);
            a->set_angular_velocity(a->angular_velocity() - inv_IA * fast::perp_dot(vcp.rA, Pn));
            b->set_linear_velocity(b->linear_velocity() + Pn * inv_mB);
            b->set_angular_velocity(b->angular_velocity() + inv_IB * fast::perp_dot(vcp.rB, Pn));
        }
    }
}

bool Simulation2D::solve_block(VelocityConstraint& vc) {
    RigidBody2D* a = vc.body_a;
    RigidBody2D* b = vc.body_b;
    const Real inv_mA = a->inv_mass(), inv_mB = b->inv_mass();
    const Real inv_IA = a->inv_inertia(), inv_IB = b->inv_inertia();
    VelocityConstraintPoint& c1 = vc.points[0];
    VelocityConstraintPoint& c2 = vc.points[1];
    const RVec2& n = vc.normal;
    const Real rn1A = fast::perp_dot(c1.rA, n), rn1B = fast::perp_dot(c1.rB, n);
    const Real rn2A = fast::perp_dot(c2.rA, n), rn2B = fast::perp_dot(c2.rB, n);
    const Real k11 = inv_mA + inv_mB + inv_IA * rn1A * rn1A + inv_IB * rn1B * rn1B;
    const Real k22 = inv_mA + inv_mB + inv_IA * rn2A * rn2A + inv_IB * rn2B * rn2B;
    const Real k12 = inv_mA + inv_mB + inv_IA * rn1A * rn2A + inv_IB * rn1B * rn2B;
    const Real det = k11 * k22 - k12 * k12;
    constexpr Real max_condition = 1000.0_r;
    if (!(k11 * k11 < max_condition * det)) return false;

    auto normal_velocity = [&](const VelocityConstraintPoint& c) {
        RVec2 vA = a->linear_velocity() + RVec2(-a->angular_velocity() * c.rA.y, a->angular_velocity() * c.rA.x);
        RVec2 vB = b->linear_velocity() + RVec2(-b->angular_velocity() * c.rB.y, b->angular_velocity() * c.rB.x);
        return fast::dot(vB - vA, n);
    };

    const Real a1 = c1.normal_impulse, a2 = c2.normal_impulse;
    const Real b1 = normal_velocity(c1) - c1.velocity_bias - (k11 * a1 + k12 * a2);
    const Real b2 = normal_velocity(c2) - c2.velocity_bias - (k12 * a1 + k22 * a2);
    Real x1, x2;

    for (;;) {
        x1 = -( k22 * b1 - k12 * b2) / det;
        x2 = -(-k12 * b1 + k11 * b2) / det;
        if (x1 >= 0.0_r && x2 >= 0.0_r) break;
        x1 = -b1 / k11; x2 = 0.0_r;
        if (x1 >= 0.0_r && k12 * x1 + b2 >= 0.0_r) break;
        x1 = 0.0_r; x2 = -b2 / k22;
        if (x2 >= 0.0_r && k12 * x2 + b1 >= 0.0_r) break;
        x1 = 0.0_r; x2 = 0.0_r;
        if (b1 >= 0.0_r && b2 >= 0.0_r) break;
        return false; 
    }

    const RVec2 P1 = n * (x1 - a1);
    const RVec2 P2 = n * (x2 - a2);
    const RVec2 P = P1 + P2;
    a->set_linear_velocity(a->linear_velocity() - P * inv_mA);
    a->set_angular_velocity(a->angular_velocity() - inv_IA * (fast::perp_dot(c1.rA, P1) + fast::perp_dot(c2.rA, P2)));
    b->set_linear_velocity(b->linear_velocity() + P * inv_mB);
    b->set_angular_velocity(b->angular_velocity() + inv_IB * (fast::perp_dot(c1.rB, P1) + fast::perp_dot(c2.rB, P2)));
    c1.normal_impulse = x1;
    c2.normal_impulse = x2;
    return true;
}

void Simulation2D::store_impulses() {
    for (const auto& vc : m_constraints) {
        ContactManifold& mf = m_manifolds[vc.manifold_index];

        for (int p = 0; p < vc.point_count; ++p) {
            mf.points[p].normal_impulse  = vc.points[p].normal_impulse;
            mf.points[p].tangent_impulse = vc.points[p].tangent_impulse;
        }
    }
}

bool Simulation2D::solve_position_constraints() {
    Real min_separation = 0.0_r;

    for (auto& mf : m_manifolds) {
        RigidBody2D* a = mf.body_a;
        RigidBody2D* b = mf.body_b;
        Real inv_mA = a->inv_mass();
        Real inv_mB = b->inv_mass();
        Real inv_IA = a->inv_inertia();
        Real inv_IB = b->inv_inertia();

        for (int p = 0; p < mf.point_count; ++p) {
            RVec2 pA = a->transform().apply(mf.points[p].local_anchor_a);
            RVec2 pB = b->transform().apply(mf.points[p].local_anchor_b);
            Real separation = fast::dot(pB - pA, mf.normal);
            min_separation = std::min(min_separation, separation);

            Real C = std::clamp(
                m_cfg.baumgarte_factor * (separation + m_cfg.slop),
                -m_cfg.max_correction, 0.0_r
            );

            RVec2 point = (pA + pB) * 0.5_r;
            RVec2 rA = point - a->world_center();
            RVec2 rB = point - b->world_center();
            Real rnA = fast::perp_dot(rA, mf.normal);
            Real rnB = fast::perp_dot(rB, mf.normal);
            Real K = inv_mA + inv_mB + inv_IA * rnA * rnA + inv_IB * rnB * rnB;
            Real impulse = (K > 0.0_r) ? -C / K : 0.0_r;
            RVec2 P = mf.normal * impulse;
            a->apply_position_correction(P * -inv_mA, -inv_IA * fast::perp_dot(rA, P));
            b->apply_position_correction(P *  inv_mB,  inv_IB * fast::perp_dot(rB, P));
        }
    }

    return min_separation >= -3.0_r * m_cfg.slop;
}

void Simulation2D::update_contact_cache() {
    for (const auto& mf : m_manifolds) {
        auto it = m_touching.find(key_of(mf));
        if (it == m_touching.end()) continue;
        CachedContact& c = it->second;
        c.point_count = mf.point_count;

        for (int p = 0; p < mf.point_count; ++p) {
            c.local_anchor_a[p]  = mf.points[p].local_anchor_a;
            c.normal_impulse[p]  = mf.points[p].normal_impulse;
            c.tangent_impulse[p] = mf.points[p].tangent_impulse;
        }
    }

    for (const Key& key : m_carried) {
        auto old = m_contact_cache.find(key);
        if (old != m_contact_cache.end()) m_touching.emplace(key, old->second);
    }

    for (auto& [key, c] : m_touching) {
        if (!c.events || m_contact_cache.count(key)) continue;
        Real impulse = 0.0_r;
        for (int p = 0; p < c.point_count; ++p) impulse += c.normal_impulse[p];
        m_begin_events.push_back({ c.body_a, c.body_b, c.normal, c.point, impulse, c.sensor });
    }

    for (auto& [key, c] : m_contact_cache) {
        if (!c.events || m_touching.count(key)) continue;
        m_end_events.push_back({ c.body_a, c.body_b, c.normal, c.point, 0.0_r, c.sensor });
    }

    m_contact_cache.swap(m_touching);
    m_touching.clear();
    m_carried.clear();
}

void Simulation2D::dispatch_events() {
    if (m_begin_events.empty() && m_end_events.empty()) return;
    std::vector<ContactEvent> begins, ends;
    begins.swap(m_begin_events);
    ends.swap(m_end_events);
    m_last_begin.insert(m_last_begin.end(), begins.begin(), begins.end());
    m_last_end.insert(m_last_end.end(), ends.begin(), ends.end());
    if (!m_cfg.event_callbacks) return;
    m_locked = true;
    if (m_on_begin) for (const auto& e : begins) m_on_begin(e);
    if (m_on_end)   for (const auto& e : ends)   m_on_end(e);
    m_locked = false;
}

void Simulation2D::remove_body_now(RigidBody2D* body) {
    auto owner = std::find_if(m_bodies.begin(), m_bodies.end(), [body](const std::unique_ptr<RigidBody2D>& p) { return p.get() == body; });
    if (owner == m_bodies.end()) return;

    for (auto it = m_contact_cache.begin(); it != m_contact_cache.end(); ) {
        if (it->first.body_a == body || it->first.body_b == body) {
            const CachedContact& c = it->second;
            if (c.events) m_end_events.push_back({ c.body_a, c.body_b, c.normal, c.point, 0.0_r, c.sensor });
            it = m_contact_cache.erase(it);
        } else {
            ++it;
        }
    }

    auto refers = [body](const ContactManifold& mf) { return mf.body_a == body || mf.body_b == body; };
    m_manifolds.erase(std::remove_if(m_manifolds.begin(), m_manifolds.end(), refers), m_manifolds.end());
    m_constraints.clear();
    m_pairs.clear();
    m_body_ptrs.clear();
    m_body_aabbs.clear();
    drop_proxy(body);
    auto stale = [body](const ContactEvent& e) { return e.body_a == body || e.body_b == body; };
    m_last_begin.erase(std::remove_if(m_last_begin.begin(), m_last_begin.end(), stale), m_last_begin.end());
    m_last_end.erase(std::remove_if(m_last_end.begin(), m_last_end.end(), stale), m_last_end.end());
    dispatch_events();
    m_last_end.erase(std::remove_if(m_last_end.begin(), m_last_end.end(), stale), m_last_end.end());
    owner = std::find_if(m_bodies.begin(), m_bodies.end(), [body](const std::unique_ptr<RigidBody2D>& p) { return p.get() == body; });
    if (owner != m_bodies.end()) {
        m_handles.remove(body->m_handle);
        m_bodies.erase(owner);
    }
}

void Simulation2D::flush_pending_removals() {
    while (!m_pending_removals.empty()) {
        std::vector<RigidBody2D*> pending;
        pending.swap(m_pending_removals);
        std::sort(pending.begin(), pending.end(), std::less<RigidBody2D*>());
        pending.erase(std::unique(pending.begin(), pending.end()), pending.end());
        for (RigidBody2D* b : pending) remove_body_now(b);
    }
}

void Simulation2D::cleanup_bodies() {
    AABB world_bounds(
        {-m_cfg.world_half_extent, -m_cfg.world_half_extent},
        { m_cfg.world_half_extent,  m_cfg.world_half_extent}
    );

    for (auto& bp : m_bodies) {
        RigidBody2D& b = *bp;
        bool remove = false;

        if (has_flag(b.flags(), PhysicsFlags::DestroyOnSleep) && !b.is_awake()) { remove = true; }

        if (has_flag(b.flags(), PhysicsFlags::DestroyOffScreen)) {
            AABB bb = b.compute_body_aabb();
            if (!world_bounds.overlaps(bb)) remove = true;
        }

        if (remove) m_pending_removals.push_back(&b);
    }

    flush_pending_removals();
}

Real Simulation2D::combined_friction(const Collider2D& a, const Collider2D& b) const {
    const CombineMode m = resolve_combine(a.material.friction_combine, b.material.friction_combine, m_cfg.friction_combine);
    if (m == CombineMode::Custom && m_friction_combiner) return m_friction_combiner(a, b);
    return combine_values(m, a.material.friction, b.material.friction);
}

Real Simulation2D::combined_restitution(const Collider2D& a, const Collider2D& b) const {
    const CombineMode m = resolve_combine(a.material.restitution_combine, b.material.restitution_combine, m_cfg.restitution_combine);
    if (m == CombineMode::Custom && m_restitution_combiner) return m_restitution_combiner(a, b);
    return combine_values(m, a.material.restitution, b.material.restitution);
}

namespace {

std::uint64_t tree_pair_key(std::int32_t a, std::int32_t b) noexcept {
    const std::uint32_t lo = static_cast<std::uint32_t>(std::min(a, b));
    const std::uint32_t hi = static_cast<std::uint32_t>(std::max(a, b));
    return (static_cast<std::uint64_t>(hi) << 32) | lo;
}

}

bool Simulation2D::can_collide(const RigidBody2D& a, const RigidBody2D& b) const noexcept {
    if (!layers_can_collide(a.collision_layer(), a.collision_mask(), b.collision_layer(), b.collision_mask())) return false;
    return a.can_collide_with(b);
}

bool Simulation2D::sweeps(const RigidBody2D& b) const noexcept {
    if (!m_cfg.continuous || !is_dynamic(b)) return false;
    return m_cfg.continuous_all || has_flag(b.flags(), PhysicsFlags::Bullet);
}

void Simulation2D::drop_proxy(RigidBody2D* body) {
    auto it = m_proxies.find(body);
    if (it == m_proxies.end()) return;
    const std::int32_t proxy = it->second;
    for (std::size_t i = 0; i < m_tree_pairs.size();) {
        if (m_tree_pairs[i].first == proxy || m_tree_pairs[i].second == proxy) {
            m_tree_pair_index.erase(tree_pair_key(m_tree_pairs[i].first, m_tree_pairs[i].second));
            if (i + 1 != m_tree_pairs.size()) {
                m_tree_pairs[i] = m_tree_pairs.back();
                m_tree_pair_index[tree_pair_key(m_tree_pairs[i].first, m_tree_pairs[i].second)] = i;
            }
            m_tree_pairs.pop_back();
        } else {
            ++i;
        }
    }
    m_dtree.remove(proxy);
    m_proxies.erase(it);
}

void Simulation2D::sync_tree(bool for_query) {
    for (auto& bp : m_bodies) {
        RigidBody2D& b = *bp;
        const bool want = is_enabled(b) && !b.colliders().empty() &&
            (has_flag(b.flags(), PhysicsFlags::BroadphaseActive) || has_flag(b.flags(), PhysicsFlags::CollisionResponse)) &&
            b.collision_layer() != 0;
        auto it = m_proxies.find(&b);
        if (!want) {
            if (it != m_proxies.end()) drop_proxy(&b);
            continue;
        }
        if (for_query && it != m_proxies.end() && b.m_query_step == m_query_epoch && b.m_query_colliders == b.m_colliders.size() &&
            b.m_transform.position == b.m_query_position && b.m_transform.angle == b.m_query_angle) continue;
        b.ensure_mass_uptodate();
        if (for_query) {
            b.synchronize_colliders();
            b.m_query_step = m_query_epoch;
            b.m_query_colliders = b.m_colliders.size();
            b.m_query_position = b.m_transform.position;
            b.m_query_angle = b.m_transform.angle;
        }
        const AABB tight = b.compute_body_aabb();
        if (it == m_proxies.end()) {
            m_proxies.emplace(&b, m_dtree.insert(tight.fatten(m_cfg.broadphase_margin), &b));
            continue;
        }
        if (!for_query && !b.is_awake() && is_dynamic(b)) continue;
        AABB displaced = tight;
        if (!for_query && sweeps(b)) {
            const RVec2 d = b.linear_velocity() * m_cfg.fixed_dt;
            const Real reach = std::max(0.0_r, m_cfg.max_continuous_distance);
            const RVec2 move{ std::clamp(d.x, -reach, reach), std::clamp(d.y, -reach, reach) };
            displaced = AABB(tight.min + move, tight.max + move);
        }
        m_dtree.move(it->second, tight, m_cfg.broadphase_margin, displaced);
    }
}

void Simulation2D::tree_broadphase() {
    m_pairs.clear();
    synchronize_all_colliders();
    sync_tree(false);
    m_dtree.for_each_leaf([&](std::int32_t proxy, RigidBody2D* body) {
        if (!m_dtree.moved(proxy)) return;
        m_dtree.query(m_dtree.box(proxy), [&](std::int32_t other, RigidBody2D* ob) {
            if (other == proxy) return true;
            if (!is_dynamic(*body) && !is_dynamic(*ob)) return true;
            const std::uint64_t key = tree_pair_key(proxy, other);
            if (m_tree_pair_index.count(key)) return true;
            m_tree_pair_index.emplace(key, m_tree_pairs.size());
            m_tree_pairs.push_back({ std::min(proxy, other), std::max(proxy, other) });
            return true;
        });
    });
    for (std::size_t i = 0; i < m_tree_pairs.size();) {
        const auto pr = m_tree_pairs[i];
        const bool keep = m_dtree.valid(pr.first) && m_dtree.valid(pr.second) && m_dtree.box(pr.first).overlaps(m_dtree.box(pr.second));
        if (!keep) {
            m_tree_pair_index.erase(tree_pair_key(pr.first, pr.second));
            if (i + 1 != m_tree_pairs.size()) {
                m_tree_pairs[i] = m_tree_pairs.back();
                m_tree_pair_index[tree_pair_key(m_tree_pairs[i].first, m_tree_pairs[i].second)] = i;
            }
            m_tree_pairs.pop_back();
            continue;
        }
        ++i;
        RigidBody2D* a = m_dtree.item(pr.first);
        RigidBody2D* b = m_dtree.item(pr.second);
        if (!layers_can_collide(a->collision_layer(), a->collision_mask(), b->collision_layer(), b->collision_mask())) {
            ++m_stats.layer_rejects;
            continue;
        }
        if (!a->can_collide_with(*b)) continue;
        if (!is_active(*a) && !is_active(*b)) continue;
        if (!is_dynamic(*a) && !is_dynamic(*b)) continue;
        if (std::less<const RigidBody2D*>()(b, a)) std::swap(a, b);
        m_pairs.push_back({ a, b });
    }
    m_dtree.for_each_leaf([&](std::int32_t proxy, RigidBody2D*) { m_dtree.clear_moved(proxy); });
}

void Simulation2D::solve_continuous() {
    const Real target = std::max(m_cfg.slop, 1e-4_r);
    for (auto& bp : m_bodies) {
        RigidBody2D& b = *bp;
        if (!is_enabled(b) || !b.is_awake() || !sweeps(b)) continue;
        const Sweep2D sweep = b.sweep();
        const Real travel = std::sqrt(fast::length_squared(sweep.position1 - sweep.position0));
        Real extent = std::numeric_limits<Real>::max();
        for (const Collider2D& c : b.colliders()) {
            const AABB box = c.shape.local_aabb;
            extent = std::min(extent, std::min(box.max.x - box.min.x, box.max.y - box.min.y) * 0.5_r);
        }
        const Real spin = std::abs(sweep.angle1 - sweep.angle0) * b.compute_body_aabb().perimeter() * 0.25_r;
        if (travel + spin < 0.5_r * extent) continue;
        const Transform2D x0 = sweep.get_transform(0.0_r);
        const Transform2D x1 = sweep.get_transform(1.0_r);
        AABB swept;
        bool first = true;
        for (const Collider2D& c : b.colliders()) {
            const AABB a0 = c.shape.compute_aabb(x0.compose(c.local_offset));
            const AABB a1 = c.shape.compute_aabb(x1.compose(c.local_offset));
            const AABB m = AABB::merge(a0, a1);
            swept = first ? m : AABB::merge(swept, m);
            first = false;
        }
        if (first) continue;
        Real best = 1.0_r;
        RVec2 best_normal{};
        Real best_restitution = 0.0_r;
        auto test = [&](RigidBody2D* other) {
            if (other == &b || !is_enabled(*other)) return;
            if (sweeps(*other) && is_dynamic(*other)) return;
            if (!can_collide(b, *other) || !responds(b, *other)) return;
            Sweep2D still;
            still.center_local = other->sweep().center_local;
            still.position0 = still.position1 = other->transform().apply(still.center_local);
            still.angle0 = still.angle1 = other->transform().angle;
            for (const Collider2D& ca : b.colliders()) {
                if (ca.is_sensor) continue;
                for (const Collider2D& cb : other->colliders()) {
                    if (cb.is_sensor || !ca.filter.should_collide(cb.filter)) continue;
                    if (!cb.world_aabb.overlaps(swept)) continue;
                    RVec2 n{};
                    const Real t = time_of_impact(ca.shape, sweep, cb.shape, still, ca.local_offset, cb.local_offset, target, n);
                    if (t < best) {
                        best = t;
                        best_normal = n;
                        best_restitution = combined_restitution(ca, cb);
                    }
                }
            }
        };
        if (m_cfg.dynamic_tree) {
            m_dtree.query(swept, [&](std::int32_t, RigidBody2D* other) {
                test(other);
                return true;
            });
        } else {
            for (auto& op : m_bodies) {
                if (op->compute_body_aabb().overlaps(swept)) test(op.get());
            }
        }
        if (best >= 1.0_r) continue;
        const Real vn = fast::dot(b.linear_velocity(), best_normal);
        if (vn > 0.0_r) b.set_linear_velocity(b.linear_velocity() - best_normal * (vn * (1.0_r + best_restitution)));
        b.transform() = sweep.get_transform(best);
        b.sweep().position1 = sweep.position0 * (1.0_r - best) + sweep.position1 * best;
        b.sweep().angle1 = sweep.angle0 * (1.0_r - best) + sweep.angle1 * best;
        b.synchronize_colliders();
        ++m_stats.continuous_hits;
    }
}

bool Simulation2D::accepts(const RigidBody2D& b, const Collider2D& c, const QueryFilter2D& f) const {
    if (!is_enabled(b)) return false;
    if (&b == f.ignore) return false;
    if ((b.collision_layer() & f.layers) == 0) return false;
    if (c.is_sensor && !f.include_sensors) return false;
    const bool dyn = is_dynamic(b);
    const bool kin = !dyn && (has_flag(b.flags(), PhysicsFlags::LinearVelocity) || has_flag(b.flags(), PhysicsFlags::AngularVelocity));
    if (dyn && !f.include_dynamic) return false;
    if (kin && !f.include_kinematic) return false;
    if (!dyn && !kin && !f.include_static) return false;
    if (f.predicate && !f.predicate(b, c)) return false;
    return true;
}

auto Simulation2D::query_aabb(const AABB& region, const QueryFilter2D& filter) -> std::vector<RigidBody2D*> {
    sync_tree(true);
    std::vector<RigidBody2D*> out;
    m_dtree.query(region, [&](std::int32_t, RigidBody2D* b) {
        for (const Collider2D& c : b->colliders()) {
            if (accepts(*b, c, filter) && c.world_aabb.overlaps(region)) {
                out.push_back(b);
                break;
            }
        }
        return true;
    });
    return out;
}

std::optional<RayHit2D> Simulation2D::raycast(const RVec2& origin, const RVec2& direction, Real max_distance, const QueryFilter2D& filter) {
    const Real len = std::sqrt(fast::length_squared(direction));
    if (len <= 1e-30_r || max_distance <= 0.0_r) return std::nullopt;
    const RVec2 dir = direction * (1.0_r / len);
    sync_tree(true);
    Real best = max_distance;
    std::optional<RayHit2D> result;
    const RVec2 end = origin + dir * max_distance;
    const AABB sweep(RVec2{ std::min(origin.x, end.x), std::min(origin.y, end.y) }, RVec2{ std::max(origin.x, end.x), std::max(origin.y, end.y) });
    m_dtree.query(sweep, [&](std::int32_t, RigidBody2D* b) {
        const auto& cols = b->colliders();
        for (std::size_t i = 0; i < cols.size(); ++i) {
            if (!accepts(*b, cols[i], filter)) continue;
            ShapeRayHit2D hit;
            if (raycast_shape(cols[i].shape, b->transform().compose(cols[i].local_offset), origin, dir, best, hit) && hit.t <= best) {
                best = hit.t;
                result = RayHit2D{ b, i, hit.t, hit.t / max_distance, hit.point, hit.normal };
            }
        }
        return true;
    });
    return result;
}

std::vector<RayHit2D> Simulation2D::raycast_all(const RVec2& origin, const RVec2& direction, Real max_distance, const QueryFilter2D& filter) {
    std::vector<RayHit2D> out;
    const Real len = std::sqrt(fast::length_squared(direction));
    if (len <= 1e-30_r || max_distance <= 0.0_r) return out;
    const RVec2 dir = direction * (1.0_r / len);
    sync_tree(true);
    const RVec2 end = origin + dir * max_distance;
    const AABB sweep(RVec2{ std::min(origin.x, end.x), std::min(origin.y, end.y) }, RVec2{ std::max(origin.x, end.x), std::max(origin.y, end.y) });
    m_dtree.query(sweep, [&](std::int32_t, RigidBody2D* b) {
        const auto& cols = b->colliders();
        for (std::size_t i = 0; i < cols.size(); ++i) {
            if (!accepts(*b, cols[i], filter)) continue;
            ShapeRayHit2D hit;
            if (raycast_shape(cols[i].shape, b->transform().compose(cols[i].local_offset), origin, dir, max_distance, hit)) out.push_back(RayHit2D{ b, i, hit.t, hit.t / max_distance, hit.point, hit.normal });
        }
        return true;
    });
    std::sort(out.begin(), out.end(), [](const RayHit2D& x, const RayHit2D& y) { return x.distance < y.distance; });
    return out;
}

std::optional<ShapeHit2D> Simulation2D::shape_cast(const Shape2D& shape, const Transform2D& start, const RVec2& translation, const QueryFilter2D& filter) {
    sync_tree(true);
    const AABB a0 = shape.compute_aabb(start);
    const AABB sweep = AABB::merge(a0, AABB(a0.min + translation, a0.max + translation));
    std::optional<ShapeHit2D> result;
    Real best = 1.0_r;
    m_dtree.query(sweep, [&](std::int32_t, RigidBody2D* b) {
        const auto& cols = b->colliders();
        for (std::size_t i = 0; i < cols.size(); ++i) {
            if (!accepts(*b, cols[i], filter)) continue;
            Real f = 0.0_r;
            RVec2 n, p;
            if (cast_shape(shape, start, translation, cols[i].shape, b->transform().compose(cols[i].local_offset), f, n, p) && f <= best) {
                best = f;
                result = ShapeHit2D{ b, i, f, p, n };
            }
        }
        return true;
    });
    return result;
}

std::vector<RigidBody2D*> Simulation2D::overlap(const Shape2D& shape, const Transform2D& xf, const QueryFilter2D& filter) {
    sync_tree(true);
    std::vector<RigidBody2D*> out;
    const AABB box = shape.compute_aabb(xf);
    m_dtree.query(box, [&](std::int32_t, RigidBody2D* b) {
        for (const Collider2D& c : b->colliders()) {
            if (!accepts(*b, c, filter) || !c.world_aabb.overlaps(box)) continue;
            if (overlap_shapes(shape, xf, c.shape, b->transform().compose(c.local_offset))) {
                out.push_back(b);
                break;
            }
        }
        return true;
    });
    return out;
}

std::vector<RigidBody2D*> Simulation2D::overlap_circle(const RVec2& center, Real radius, const QueryFilter2D& filter) {
    return overlap(Shape2D::make_circle({ 0.0_r, 0.0_r }, radius), Transform2D(center, 0.0_r), filter);
}

void Simulation2D::shift_origin(const RVec2& offset) {
    for (auto& bp : m_bodies) {
        RigidBody2D& b = *bp;
        b.transform().position = b.transform().position - offset;
        b.sweep().position0 = b.sweep().position0 - offset;
        b.sweep().position1 = b.sweep().position1 - offset;
        b.synchronize_colliders();
    }
    m_dtree.shift(offset * -1.0_r);
    for (auto& [key, c] : m_contact_cache) c.point = c.point - offset;
    for (auto& mf : m_manifolds) {
        for (int p = 0; p < mf.point_count; ++p) mf.points[p].position = mf.points[p].position - offset;
    }
    for (auto& e : m_last_begin) e.point = e.point - offset;
    for (auto& e : m_last_end) e.point = e.point - offset;
}

} // namespace physics
} // namespace fizmo
