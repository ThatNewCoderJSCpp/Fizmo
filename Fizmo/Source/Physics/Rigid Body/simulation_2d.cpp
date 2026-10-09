#define ALL_FIZMO
#include <fizmo/includes.hpp>
#include "simulation_2d.hpp"

namespace fizmo {
namespace physics {
namespace narrowphase {

void project_polygon(const PolygonShape& poly, const Transform2D& xf, const vector2d& axis, double& lo, double& hi) {
    vector2d v = xf.apply(poly.vertices[0]);
    lo = hi = axis.dot(v);

    for (std::size_t i = 1; i < poly.count; ++i) {
        v = xf.apply(poly.vertices[i]);
        double d = axis.dot(v);
        if (d < lo) lo = d;
        if (d > hi) hi = d;
    }
}

void project_capsule(const CapsuleShape& cap, const Transform2D& xf, const vector2d& axis, double& lo, double& hi) {
    vector2d p1 = xf.apply(cap.point1);
    vector2d p2 = xf.apply(cap.point2);
    double d1 = axis.dot(p1);
    double d2 = axis.dot(p2);
    lo = std::min(d1, d2) - cap.radius;
    hi = std::max(d1, d2) + cap.radius;
}

vector2d closest_point_on_segment(const vector2d& p, const vector2d& a, const vector2d& b) {
    vector2d ab = b - a;
    double t = ab.magnitude_squared();
    if (t <= constants::middle_epsilon()) return a;
    t = std::clamp((p - a).dot(ab) / t, 0.0, 1.0);
    return a + ab * t;
}

bool collide_circle_circle(
    const CircleShape& ca, const Transform2D& xa,
    const CircleShape& cb, const Transform2D& xb,
    ContactManifold& m
) {
    vector2d cA = xa.apply(ca.center);
    vector2d cB = xb.apply(cb.center);
    vector2d d = cB - cA;
    double dist_sq = d.magnitude_squared();
    double r_sum = ca.radius + cb.radius;
    if (dist_sq >= r_sum * r_sum) return false;
    double dist = std::sqrt(dist_sq);
    m.normal = (dist >= constants::middle_epsilon()) ? d * (1.0 / dist) : vector2d{0.0, 1.0};
    double pen = r_sum - dist;
    m.point_count = 1;
    m.points[0].position = cA + m.normal * (ca.radius - pen * 0.5);
    m.points[0].separation = -pen;
    return true;
}

bool collide_circle_polygon(
    const CircleShape& circ, const Transform2D& xc,
    const PolygonShape& poly, const Transform2D& xp,
    ContactManifold& m, bool flip
) {
    vector2d c_world = xc.apply(circ.center);
    vector2d c_local = xp.apply_inverse(c_world);
    double best_sep = -std::numeric_limits<double>::max();
    std::size_t best_i = 0;

    for (std::size_t i = 0; i < poly.count; ++i) {
        double sep = poly.normals[i].dot(c_local - poly.vertices[i]);
        if (sep > circ.radius) return false;
        if (sep > best_sep) { best_sep = sep; best_i = i; }
    }

    std::size_t i1 = best_i;
    std::size_t i2 = (i1 + 1) % poly.count;
    const vector2d& v1 = poly.vertices[i1];
    const vector2d& v2 = poly.vertices[i2];
    vector2d closest;
    double u1 = (c_local - v1).dot(v2 - v1);
    double u2 = (c_local - v2).dot(v1 - v2);

    if (u1 <= 0.0) {
        closest = v1;
    } else if (u2 <= 0.0) {
        closest = v2;
    } else {
        closest = v1 + (v2 - v1) * (u1 / (u1 + u2));
    }

    vector2d delta = c_local - closest;
    double dist_sq = delta.magnitude_squared();
    if (dist_sq > circ.radius * circ.radius && best_sep > 0.0) return false;
    vector2d n_local; 
    double dist;      

    if (best_sep <= constants::middle_epsilon()) {
        n_local = poly.normals[best_i];
        dist = best_sep;
    } else {
        dist = std::sqrt(dist_sq);
        n_local = delta * (1.0 / dist);
    }

    vector2d n_out = xp.rotate(n_local);
    vector2d n = n_out * -1.0; 
    m.normal = flip ? n * -1.0 : n;
    m.point_count = 1;
    m.points[0].position = c_world - n_out * ((circ.radius + dist) * 0.5);
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

RoundPolygon make_round_segment(const vector2d& a, const vector2d& b, double radius) {
    RoundPolygon r;
    r.count = 2;
    r.radius = radius;
    r.vertices[0] = a;
    r.vertices[1] = b;
    vector2d e = b - a;
    double len = e.magnitude();
    vector2d n = (len > constants::middle_epsilon()) ? vector2d(e.y / len, -e.x / len) : vector2d(0.0, 1.0);
    r.normals[0] = n;
    r.normals[1] = n * -1.0;
    return r;
}

AxisResult find_max_separation(const RoundPolygon& a, const RoundPolygon& b) {
    AxisResult best{-std::numeric_limits<double>::max(), 0};

    for (std::size_t i = 0; i < a.count; ++i) {
        const vector2d& n = a.normals[i];
        const vector2d& v = a.vertices[i];
        double si = std::numeric_limits<double>::max();

        for (std::size_t j = 0; j < b.count; ++j) {
            double d = n.dot(b.vertices[j] - v);
            if (d < si) si = d;
        }

        if (si > best.separation) { best.separation = si; best.edge_index = i; }
    }

    return best;
}

SegmentDistance segment_distance(const vector2d& p1, const vector2d& q1, const vector2d& p2, const vector2d& q2) {
    vector2d d1 = q1 - p1, d2 = q2 - p2, r = p1 - p2;
    double a = d1.dot(d1), e = d2.dot(d2), f = d2.dot(r);
    double s, t;
    const double eps = constants::middle_epsilon();

    if (a <= eps && e <= eps) {
        s = t = 0.0;
    } else if (a <= eps) {
        s = 0.0; t = std::clamp(f / e, 0.0, 1.0);
    } else {
        double c = d1.dot(r);

        if (e <= eps) {
            t = 0.0; s = std::clamp(-c / a, 0.0, 1.0);
        } else {
            double b = d1.dot(d2);
            double denom = a * e - b * b;
            s = (denom > eps) ? std::clamp((b * f - c * e) / denom, 0.0, 1.0) : 0.0;
            t = (b * s + f) / e;
            if (t < 0.0) { t = 0.0; s = std::clamp(-c / a, 0.0, 1.0); }
            else if (t > 1.0) { t = 1.0; s = std::clamp((b - c) / a, 0.0, 1.0); }
        }
    }

    SegmentDistance out;
    out.closest1 = p1 + d1 * s;
    out.closest2 = p2 + d2 * t;
    out.fraction1 = s;
    out.fraction2 = t;
    out.distance_squared = (out.closest2 - out.closest1).magnitude_squared();
    return out;
}

bool collide_round_polygons(const RoundPolygon& a, const RoundPolygon& b, ContactManifold& m) {
    constexpr double feature_tolerance = 0.1 * 0.005; // a tenth of the default linear slop
    const double radius = a.radius + b.radius;
    AxisResult sep_a = find_max_separation(a, b);
    if (sep_a.separation > radius) return false;
    AxisResult sep_b = find_max_separation(b, a);
    if (sep_b.separation > radius) return false;
    const bool flip = sep_b.separation > sep_a.separation + feature_tolerance;
    const RoundPolygon& ref = flip ? b : a;
    const RoundPolygon& inc = flip ? a : b;
    const std::size_t i11 = flip ? sep_b.edge_index : sep_a.edge_index;
    const std::size_t i12 = (i11 + 1) % ref.count;
    const vector2d normal = ref.normals[i11];
    std::size_t i21 = 0;
    double min_dot = std::numeric_limits<double>::max();

    for (std::size_t i = 0; i < inc.count; ++i) {
        double d = normal.dot(inc.normals[i]);
        if (d < min_dot) { min_dot = d; i21 = i; }
    }

    const std::size_t i22 = (i21 + 1) % inc.count;
    const vector2d v11 = ref.vertices[i11], v12 = ref.vertices[i12];
    const vector2d v21 = inc.vertices[i21], v22 = inc.vertices[i22];
    const double r1 = ref.radius, r2 = inc.radius;
    m.point_count = 0;

    if (std::max(sep_a.separation, sep_b.separation) > feature_tolerance) {
        SegmentDistance sd = segment_distance(v11, v12, v21, v22);
        bool end1 = sd.fraction1 == 0.0 || sd.fraction1 == 1.0;
        bool end2 = sd.fraction2 == 0.0 || sd.fraction2 == 1.0;

        if (end1 && end2) { 
            if (sd.distance_squared > radius * radius) return false;
            double dist = std::sqrt(sd.distance_squared);
            if (dist <= constants::middle_epsilon()) return false;
            vector2d n = (sd.closest2 - sd.closest1) * (1.0 / dist); 
            vector2d p_ref = sd.closest1 + n * r1;
            vector2d p_inc = sd.closest2 - n * r2;
            m.normal = flip ? n * -1.0 : n;
            m.point_count = 1;
            m.points[0].position = (p_ref + p_inc) * 0.5;
            m.points[0].separation = dist - radius;
            return true;
        }
    }

    const vector2d tangent(-normal.y, normal.x); 
    const double lower1 = 0.0;
    const double upper1 = (v12 - v11).dot(tangent);
    const double upper2 = (v21 - v11).dot(tangent);
    const double lower2 = (v22 - v11).dot(tangent);
    const double span = upper2 - lower2;
    vector2d v_lower = v22, v_upper = v21;
    if (lower2 < lower1 && span > constants::middle_epsilon()) v_lower = v22 + (v21 - v22) * ((lower1 - lower2) / span);
    if (upper2 > upper1 && span > constants::middle_epsilon()) v_upper = v22 + (v21 - v22) * ((upper1 - lower2) / span);
    const double sep_lower = (v_lower - v11).dot(normal);
    const double sep_upper = (v_upper - v11).dot(normal);
    v_lower = v_lower + normal * (0.5 * (r1 - r2 - sep_lower));
    v_upper = v_upper + normal * (0.5 * (r1 - r2 - sep_upper));

    if (sep_lower - radius <= 0.0) {
        m.points[m.point_count].position = v_lower;
        m.points[m.point_count].separation = sep_lower - radius;
        ++m.point_count;
    }

    if (sep_upper - radius <= 0.0) {
        m.points[m.point_count].position = v_upper;
        m.points[m.point_count].separation = sep_upper - radius;
        ++m.point_count;
    }

    m.normal = flip ? normal * -1.0 : normal;
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
    vector2d cc = xc.apply(circ.center);
    vector2d p1 = xk.apply(cap.point1);
    vector2d p2 = xk.apply(cap.point2);
    vector2d closest = closest_point_on_segment(cc, p1, p2);
    CircleShape phantom;
    phantom.center = {};
    phantom.radius = cap.radius;
    Transform2D phantom_xf;
    phantom_xf.position = closest;
    phantom_xf.cos_a = 1.0; phantom_xf.sin_a = 0.0;
    bool hit = collide_circle_circle(circ, xc, phantom, phantom_xf, m);
    if (hit && flip) m.normal = m.normal * -1.0;
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

    if (hit && flip) m.normal = m.normal * -1.0;
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
    vector2d c = xc.apply(circ.center);
    vector2d v1 = xe.apply(edge.v1);
    vector2d v2 = xe.apply(edge.v2);
    vector2d closest = closest_point_on_segment(c, v1, v2);
    vector2d d = c - closest;
    double dist_sq = d.magnitude_squared();
    double r = circ.radius;
    if (dist_sq > r * r) return false;
    double dist = std::sqrt(dist_sq);
    vector2d n = (dist >= constants::middle_epsilon()) ? d * (1.0 / dist) : xe.rotate(edge.normal());
    m.normal = flip ? n * -1.0 : n;
    m.point_count = 1;
    m.points[0].position = (closest + (c - n * r)) * 0.5;
    m.points[0].separation = dist - r;
    return true;
}

bool collide_edge_polygon(
    const EdgeShape& edge, const Transform2D& xe,
    const PolygonShape& poly, const Transform2D& xp,
    ContactManifold& m, bool flip
) {
    bool hit = detail_sat::collide_round_polygons(
        detail_sat::make_round_segment(xe.apply(edge.v1), xe.apply(edge.v2), 0.0),
        detail_sat::make_round_polygon(poly, xp),
        m
    );

    if (hit && flip) m.normal = m.normal * -1.0;
    return hit;
}

bool collide_edge_capsule(
    const EdgeShape& edge, const Transform2D& xe,
    const CapsuleShape& cap, const Transform2D& xk,
    ContactManifold& m, bool flip
) {
    bool hit = detail_sat::collide_round_polygons(
        detail_sat::make_round_segment(xe.apply(edge.v1), xe.apply(edge.v2), 0.0),
        detail_sat::make_round_segment(xk.apply(cap.point1), xk.apply(cap.point2), cap.radius),
        m
    );

    if (hit && flip) m.normal = m.normal * -1.0;
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

    if (hit && flip) manifold.normal = manifold.normal * -1.0;
    return hit;
}

Simulation2D::Simulation2D(const Config& cfg) : m_cfg(cfg),
          m_tree(AABB(
              {-cfg.world_half_extent, -cfg.world_half_extent},
              { cfg.world_half_extent,  cfg.world_half_extent}
          ))
    {}

auto Simulation2D::remove_body(RigidBody2D& body) -> void {
        if (m_locked) { m_pending_removals.push_back(&body); return; }
        remove_body_now(&body);
        flush_pending_removals();
    }

auto Simulation2D::step(double dt) -> void {
        if (dt <= 0.0) return;
        m_locked = true;

        for (auto& bp : m_bodies) {
            RigidBody2D& b = *bp;
            if (!has_flag(b.flags(), PhysicsFlags::Enabled)) continue;
            if (!b.is_awake()) continue;
            b.ensure_mass_uptodate();
            b.integrate_forces(dt, m_cfg.gravity);
        }

        broadphase();
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

auto Simulation2D::point_query(const vector2d& point) -> RigidBody2D* {
        AABB tiny = {{point.x - 0.001, point.y - 0.001}, {point.x + 0.001, point.y + 0.001}};
        auto results = query_aabb(tiny);

        for (auto* b : results) {
            AABB body_bb = b->compute_body_aabb();
            if (body_bb.contains(point)) return b;
        }

        return nullptr;
    }

auto Simulation2D::is_active(const RigidBody2D& b) noexcept -> bool {
        if (!b.is_awake()) return false;
        if (b.inv_mass() > 0.0) return true;
        vector2d v = b.linear_velocity();
        return v.x != 0.0 || v.y != 0.0 || b.angular_velocity() != 0.0;
    }

auto Simulation2D::responds(const RigidBody2D& a, const RigidBody2D& b) noexcept -> bool {
        return a.has(PhysicsFlags::CollisionResponse) && b.has(PhysicsFlags::CollisionResponse) &&
              !a.has(PhysicsFlags::Intangible) && !b.has(PhysicsFlags::Intangible);
    }

auto Simulation2D::wants_events(const RigidBody2D& a, const RigidBody2D& b, bool sensor) noexcept -> bool {
        if (a.has(PhysicsFlags::ContactEvents) || b.has(PhysicsFlags::ContactEvents)) return true;
        return sensor && (a.has(PhysicsFlags::SensorEvents) || b.has(PhysicsFlags::SensorEvents));
    }

auto Simulation2D::broadphase() -> void {
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
            if (!a->can_collide_with(*b)) return;
            if (!is_active(*a) && !is_active(*b)) return;   
            if (!is_dynamic(*a) && !is_dynamic(*b)) return; 
            if (std::less<const RigidBody2D*>()(b, a)) std::swap(a, b); 
            m_pairs.push_back({a, b});
        });
    }

auto Simulation2D::narrowphase() -> void {
        m_manifolds.clear();
        m_manifolds.reserve(m_pairs.size());
        m_touching.clear();
        for (auto& [a, b] : m_pairs) { generate_contacts(a, b); }
    }

auto Simulation2D::generate_contacts(RigidBody2D* a, RigidBody2D* b) -> void {
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
                mf.friction    = std::sqrt(ca.material.friction * cb.material.friction);
                mf.restitution = std::max(ca.material.restitution, cb.material.restitution);

                mf.restitution_threshold = std::min(
                    ca.material.restitution_threshold,
                    cb.material.restitution_threshold
                );

                for (int p = 0; p < mf.point_count; ++p) {
                    ContactPoint& cp = mf.points[p];
                    vector2d half = mf.normal * (cp.separation * 0.5);
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

auto Simulation2D::build_islands_and_wake() -> void {
        const std::size_t n = m_bodies.size();
        m_islands.reset(n);
        m_body_index.clear();
        m_carried.clear();
        for (std::size_t i = 0; i < n; ++i) m_body_index[m_bodies[i].get()] = i;

        auto link = [this](RigidBody2D* a, RigidBody2D* b) {
            const bool da = is_dynamic(*a), db = is_dynamic(*b);

            if (da && db) {
                m_islands.unite(m_body_index[a], m_body_index[b]);
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

auto Simulation2D::sleep_islands(double dt) -> void {
        const std::size_t n = std::min(m_islands.size(), m_bodies.size());
        std::vector<double> min_timer(n, std::numeric_limits<double>::max());

        for (std::size_t i = 0; i < n; ++i) {
            RigidBody2D& b = *m_bodies[i];
            if (!is_enabled(b) || !b.is_awake()) continue;

            if (!is_dynamic(b)) {
                b.update_sleep(dt); 
                continue;
            }

            double t = b.advance_sleep_timer(dt);
            std::size_t r = m_islands.find(i);
            if (t < min_timer[r]) min_timer[r] = t;
        }

        for (std::size_t i = 0; i < n; ++i) {
            RigidBody2D& b = *m_bodies[i];
            if (!is_enabled(b) || !b.is_awake() || !is_dynamic(b)) continue;
            if (min_timer[m_islands.find(i)] >= RigidBody2D::SLEEP_TIME_THRESHOLD) b.put_to_sleep();
        }
    }

auto Simulation2D::build_velocity_constraints() -> void {
        m_constraints.clear();
        m_constraints.reserve(m_manifolds.size());
        const double match_sq = m_cfg.warm_start_match_distance * m_cfg.warm_start_match_distance;

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
            double inv_mA = a->inv_mass();
            double inv_mB = b->inv_mass();
            double inv_IA = a->inv_inertia();
            double inv_IB = b->inv_inertia();
            vector2d cA = a->world_center();
            vector2d cB = b->world_center();
            vector2d tangent = {-mf.normal.y, mf.normal.x};
            const CachedContact* old = nullptr;

            if (m_cfg.warm_starting) {
                auto it = m_contact_cache.find(key_of(mf));
                if (it != m_contact_cache.end() && !it->second.sensor) old = &it->second;
            }

            for (int p = 0; p < mf.point_count; ++p) {
                auto& vcp = vc.points[p];
                vcp.rA = mf.points[p].position - cA;
                vcp.rB = mf.points[p].position - cB;
                double rnA = vcp.rA.perp_dot(mf.normal);
                double rnB = vcp.rB.perp_dot(mf.normal);
                double k_normal = inv_mA + inv_mB + inv_IA * rnA * rnA + inv_IB * rnB * rnB;
                vcp.normal_mass = (k_normal > 0.0) ? 1.0 / k_normal : 0.0;
                double rtA = vcp.rA.perp_dot(tangent);
                double rtB = vcp.rB.perp_dot(tangent);
                double k_tangent = inv_mA + inv_mB + inv_IA * rtA * rtA + inv_IB * rtB * rtB;
                vcp.tangent_mass = (k_tangent > 0.0) ? 1.0 / k_tangent : 0.0;
                vector2d vA = a->linear_velocity() + vector2d(-a->angular_velocity() * vcp.rA.y, a->angular_velocity() * vcp.rA.x);
                vector2d vB = b->linear_velocity() + vector2d(-b->angular_velocity() * vcp.rB.y, b->angular_velocity() * vcp.rB.x);
                double vn_rel = (vB - vA).dot(mf.normal);
                vcp.velocity_bias = 0.0;
                if (vn_rel < -vc.restitution_threshold) { vcp.velocity_bias = -vc.restitution * vn_rel; }

                if (old) {
                    double best = match_sq;

                    for (int q = 0; q < old->point_count; ++q) {
                        double d = (old->local_anchor_a[q] - mf.points[p].local_anchor_a).magnitude_squared();

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

auto Simulation2D::warm_start() -> void {
        for (auto& vc : m_constraints) {
            RigidBody2D* a = vc.body_a;
            RigidBody2D* b = vc.body_b;
            double inv_mA = a->inv_mass();
            double inv_mB = b->inv_mass();
            double inv_IA = a->inv_inertia();
            double inv_IB = b->inv_inertia();

            for (int p = 0; p < vc.point_count; ++p) {
                auto& vcp = vc.points[p];
                vector2d tangent = {-vc.normal.y, vc.normal.x};
                vector2d P = vc.normal * vcp.normal_impulse + tangent * vcp.tangent_impulse;
                a->set_linear_velocity(a->linear_velocity() - P * inv_mA);
                a->set_angular_velocity(a->angular_velocity() - inv_IA * vcp.rA.perp_dot(P));
                b->set_linear_velocity(b->linear_velocity() + P * inv_mB);
                b->set_angular_velocity(b->angular_velocity() + inv_IB * vcp.rB.perp_dot(P));
            }
        }
    }

auto Simulation2D::solve_velocity_constraints() -> void {
        for (auto& vc : m_constraints) {
            RigidBody2D* a = vc.body_a;
            RigidBody2D* b = vc.body_b;
            double inv_mA = a->inv_mass();
            double inv_mB = b->inv_mass();
            double inv_IA = a->inv_inertia();
            double inv_IB = b->inv_inertia();

            for (int p = 0; p < vc.point_count; ++p) {
                auto& vcp = vc.points[p];
                vector2d vA = a->linear_velocity() + vector2d(-a->angular_velocity() * vcp.rA.y, a->angular_velocity() * vcp.rA.x);
                vector2d vB = b->linear_velocity() + vector2d(-b->angular_velocity() * vcp.rB.y, b->angular_velocity() * vcp.rB.x);
                vector2d dv = vB - vA;
                vector2d tangent = {-vc.normal.y, vc.normal.x};
                double vt = dv.dot(tangent);
                double lambda_t = -vt * vcp.tangent_mass;
                double max_friction = vc.friction * vcp.normal_impulse;

                double new_impulse_t = std::clamp(
                    vcp.tangent_impulse + lambda_t,
                    -max_friction, max_friction
                );

                lambda_t = new_impulse_t - vcp.tangent_impulse;
                vcp.tangent_impulse = new_impulse_t;
                vector2d Pt = tangent * lambda_t;
                a->set_linear_velocity(a->linear_velocity() - Pt * inv_mA);
                a->set_angular_velocity(a->angular_velocity() - inv_IA * vcp.rA.perp_dot(Pt));
                b->set_linear_velocity(b->linear_velocity() + Pt * inv_mB);
                b->set_angular_velocity(b->angular_velocity() + inv_IB * vcp.rB.perp_dot(Pt));
            }

            if (vc.point_count == 2 && solve_block(vc)) continue;

            for (int p = 0; p < vc.point_count; ++p) {
                auto& vcp = vc.points[p];
                vector2d vA = a->linear_velocity() + vector2d(-a->angular_velocity() * vcp.rA.y, a->angular_velocity() * vcp.rA.x);
                vector2d vB = b->linear_velocity() + vector2d(-b->angular_velocity() * vcp.rB.y, b->angular_velocity() * vcp.rB.x);
                vector2d dv = vB - vA;
                double vn = dv.dot(vc.normal);
                double lambda_n = -(vn - vcp.velocity_bias) * vcp.normal_mass;
                double new_impulse_n = std::max(vcp.normal_impulse + lambda_n, 0.0);
                lambda_n = new_impulse_n - vcp.normal_impulse;
                vcp.normal_impulse = new_impulse_n;
                vector2d Pn = vc.normal * lambda_n;
                a->set_linear_velocity(a->linear_velocity() - Pn * inv_mA);
                a->set_angular_velocity(a->angular_velocity() - inv_IA * vcp.rA.perp_dot(Pn));
                b->set_linear_velocity(b->linear_velocity() + Pn * inv_mB);
                b->set_angular_velocity(b->angular_velocity() + inv_IB * vcp.rB.perp_dot(Pn));
            }
        }
    }

auto Simulation2D::solve_block(VelocityConstraint& vc) -> bool {
        RigidBody2D* a = vc.body_a;
        RigidBody2D* b = vc.body_b;
        const double inv_mA = a->inv_mass(), inv_mB = b->inv_mass();
        const double inv_IA = a->inv_inertia(), inv_IB = b->inv_inertia();
        VelocityConstraintPoint& c1 = vc.points[0];
        VelocityConstraintPoint& c2 = vc.points[1];
        const vector2d& n = vc.normal;
        const double rn1A = c1.rA.perp_dot(n), rn1B = c1.rB.perp_dot(n);
        const double rn2A = c2.rA.perp_dot(n), rn2B = c2.rB.perp_dot(n);
        const double k11 = inv_mA + inv_mB + inv_IA * rn1A * rn1A + inv_IB * rn1B * rn1B;
        const double k22 = inv_mA + inv_mB + inv_IA * rn2A * rn2A + inv_IB * rn2B * rn2B;
        const double k12 = inv_mA + inv_mB + inv_IA * rn1A * rn2A + inv_IB * rn1B * rn2B;
        const double det = k11 * k22 - k12 * k12;
        constexpr double max_condition = 1000.0;
        if (!(k11 * k11 < max_condition * det)) return false;

        auto normal_velocity = [&](const VelocityConstraintPoint& c) {
            vector2d vA = a->linear_velocity() + vector2d(-a->angular_velocity() * c.rA.y, a->angular_velocity() * c.rA.x);
            vector2d vB = b->linear_velocity() + vector2d(-b->angular_velocity() * c.rB.y, b->angular_velocity() * c.rB.x);
            return (vB - vA).dot(n);
        };

        const double a1 = c1.normal_impulse, a2 = c2.normal_impulse;
        const double b1 = normal_velocity(c1) - c1.velocity_bias - (k11 * a1 + k12 * a2);
        const double b2 = normal_velocity(c2) - c2.velocity_bias - (k12 * a1 + k22 * a2);
        double x1, x2;

        for (;;) {
            x1 = -( k22 * b1 - k12 * b2) / det;
            x2 = -(-k12 * b1 + k11 * b2) / det;
            if (x1 >= 0.0 && x2 >= 0.0) break;
            x1 = -b1 / k11; x2 = 0.0;
            if (x1 >= 0.0 && k12 * x1 + b2 >= 0.0) break;
            x1 = 0.0; x2 = -b2 / k22;
            if (x2 >= 0.0 && k12 * x2 + b1 >= 0.0) break;
            x1 = 0.0; x2 = 0.0;
            if (b1 >= 0.0 && b2 >= 0.0) break;
            return false; 
        }

        const vector2d P1 = n * (x1 - a1);
        const vector2d P2 = n * (x2 - a2);
        const vector2d P = P1 + P2;
        a->set_linear_velocity(a->linear_velocity() - P * inv_mA);
        a->set_angular_velocity(a->angular_velocity() - inv_IA * (c1.rA.perp_dot(P1) + c2.rA.perp_dot(P2)));
        b->set_linear_velocity(b->linear_velocity() + P * inv_mB);
        b->set_angular_velocity(b->angular_velocity() + inv_IB * (c1.rB.perp_dot(P1) + c2.rB.perp_dot(P2)));
        c1.normal_impulse = x1;
        c2.normal_impulse = x2;
        return true;
    }

auto Simulation2D::store_impulses() -> void {
        for (const auto& vc : m_constraints) {
            ContactManifold& mf = m_manifolds[vc.manifold_index];

            for (int p = 0; p < vc.point_count; ++p) {
                mf.points[p].normal_impulse  = vc.points[p].normal_impulse;
                mf.points[p].tangent_impulse = vc.points[p].tangent_impulse;
            }
        }
    }

auto Simulation2D::solve_position_constraints() -> bool {
        double min_separation = 0.0;

        for (auto& mf : m_manifolds) {
            RigidBody2D* a = mf.body_a;
            RigidBody2D* b = mf.body_b;
            double inv_mA = a->inv_mass();
            double inv_mB = b->inv_mass();
            double inv_IA = a->inv_inertia();
            double inv_IB = b->inv_inertia();

            for (int p = 0; p < mf.point_count; ++p) {
                vector2d pA = a->transform().apply(mf.points[p].local_anchor_a);
                vector2d pB = b->transform().apply(mf.points[p].local_anchor_b);
                double separation = (pB - pA).dot(mf.normal);
                min_separation = std::min(min_separation, separation);

                double C = std::clamp(
                    m_cfg.baumgarte_factor * (separation + m_cfg.slop),
                    -m_cfg.max_correction, 0.0
                );

                vector2d point = (pA + pB) * 0.5;
                vector2d rA = point - a->world_center();
                vector2d rB = point - b->world_center();
                double rnA = rA.perp_dot(mf.normal);
                double rnB = rB.perp_dot(mf.normal);
                double K = inv_mA + inv_mB + inv_IA * rnA * rnA + inv_IB * rnB * rnB;
                double impulse = (K > 0.0) ? -C / K : 0.0;
                vector2d P = mf.normal * impulse;
                a->apply_position_correction(P * -inv_mA, -inv_IA * rA.perp_dot(P));
                b->apply_position_correction(P *  inv_mB,  inv_IB * rB.perp_dot(P));
            }
        }

        return min_separation >= -3.0 * m_cfg.slop;
    }

auto Simulation2D::update_contact_cache() -> void {
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
            double impulse = 0.0;
            for (int p = 0; p < c.point_count; ++p) impulse += c.normal_impulse[p];
            m_begin_events.push_back({ c.body_a, c.body_b, c.normal, c.point, impulse, c.sensor });
        }

        for (auto& [key, c] : m_contact_cache) {
            if (!c.events || m_touching.count(key)) continue;
            m_end_events.push_back({ c.body_a, c.body_b, c.normal, c.point, 0.0, c.sensor });
        }

        m_contact_cache.swap(m_touching);
        m_touching.clear();
        m_carried.clear();
    }

auto Simulation2D::dispatch_events() -> void {
        if (m_begin_events.empty() && m_end_events.empty()) return;
        std::vector<ContactEvent> begins, ends;
        begins.swap(m_begin_events);
        ends.swap(m_end_events);
        m_locked = true;
        if (m_on_begin) for (const auto& e : begins) m_on_begin(e);
        if (m_on_end)   for (const auto& e : ends)   m_on_end(e);
        m_locked = false;
    }

auto Simulation2D::remove_body_now(RigidBody2D* body) -> void {
        auto owner = std::find_if(m_bodies.begin(), m_bodies.end(), [body](const std::unique_ptr<RigidBody2D>& p) { return p.get() == body; });
        if (owner == m_bodies.end()) return;

        for (auto it = m_contact_cache.begin(); it != m_contact_cache.end(); ) {
            if (it->first.body_a == body || it->first.body_b == body) {
                const CachedContact& c = it->second;
                if (c.events) m_end_events.push_back({ c.body_a, c.body_b, c.normal, c.point, 0.0, c.sensor });
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
        dispatch_events();
        owner = std::find_if(m_bodies.begin(), m_bodies.end(), [body](const std::unique_ptr<RigidBody2D>& p) { return p.get() == body; });
        if (owner != m_bodies.end()) m_bodies.erase(owner);
    }

auto Simulation2D::flush_pending_removals() -> void {
        while (!m_pending_removals.empty()) {
            std::vector<RigidBody2D*> pending;
            pending.swap(m_pending_removals);
            std::sort(pending.begin(), pending.end(), std::less<RigidBody2D*>());
            pending.erase(std::unique(pending.begin(), pending.end()), pending.end());
            for (RigidBody2D* b : pending) remove_body_now(b);
        }
    }

auto Simulation2D::cleanup_bodies() -> void {
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

} // namespace physics
} // namespace fizmo
