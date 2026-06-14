#ifndef FIZMO_SIMULATION_2D_HPP
#define FIZMO_SIMULATION_2D_HPP

#include "physics_2d.hpp"
#include "quadtree.hpp"
#include <vector>
#include <cmath>
#include <algorithm>
#include <functional>
#include <limits>
#include <cstdint>
#include <utility>

namespace fizmo {
namespace physics {

struct ContactPoint {
    vector2d position{};          // world-space contact point (m)
    double separation  = 0.0;     // negative = penetrating (m)
    double normal_impulse  = 0.0; // accumulated normal impulse (N*s)
    double tangent_impulse = 0.0; // accumulated tangent impulse (N*s)
};

struct ContactManifold {
    RigidBody2D* body_a = nullptr;
    RigidBody2D* body_b = nullptr;
    vector2d normal{}; // unit vector from A toward B
    ContactPoint points[2];
    int point_count = 0;
    double friction    = 0.0;
    double restitution = 0.0;
    double restitution_threshold = 1.0; // m/s
};

namespace narrowphase {

inline void project_polygon(const PolygonShape& poly, const Transform2D& xf, const vector2d& axis, double& lo, double& hi) {
    vector2d v = xf.apply(poly.vertices[0]);
    lo = hi = axis.dot(v);

    for (std::size_t i = 1; i < poly.count; ++i) {
        v = xf.apply(poly.vertices[i]);
        double d = axis.dot(v);
        if (d < lo) lo = d;
        if (d > hi) hi = d;
    }
}

inline void project_circle(const CircleShape& circ, const Transform2D& xf, const vector2d& axis, double& lo, double& hi) {
    vector2d c = xf.apply(circ.center);
    double p = axis.dot(c);
    lo = p - circ.radius;
    hi = p + circ.radius;
}

inline void project_capsule(const CapsuleShape& cap, const Transform2D& xf, const vector2d& axis, double& lo, double& hi) {
    vector2d p1 = xf.apply(cap.point1);
    vector2d p2 = xf.apply(cap.point2);
    double d1 = axis.dot(p1);
    double d2 = axis.dot(p2);
    lo = std::min(d1, d2) - cap.radius;
    hi = std::max(d1, d2) + cap.radius;
}

inline vector2d closest_point_on_segment(const vector2d& p, const vector2d& a, const vector2d& b) {
    vector2d ab = b - a;
    double t = ab.magnitude_squared();
    if (t <= constants::middle_epsilon()) return a;
    t = std::clamp((p - a).dot(ab) / t, 0.0, 1.0);
    return a + ab * t;
}

inline bool collide_circle_circle(
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

inline bool collide_circle_polygon(
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
    double pen;

    if (best_sep <= constants::middle_epsilon()) {
        n_local = poly.normals[best_i];
        pen = circ.radius - best_sep;
    } else {
        double dist = std::sqrt(dist_sq);
        n_local = delta * (1.0 / dist);
        pen = circ.radius - dist;
    }

    vector2d n_world = xp.rotate(n_local);
    if (flip) n_world = n_world * -1.0;
    m.normal = n_world;
    m.point_count = 1;
    m.points[0].position = c_world - (flip ? n_world * -1.0 : n_world) * (circ.radius - pen * 0.5);
    m.points[0].separation = -pen;
    return true;
}

namespace detail_sat {

struct AxisResult {
    double depth;
    std::size_t edge_index;
};

inline AxisResult find_min_separation(
    const PolygonShape& a, const Transform2D& xa,
    const PolygonShape& b, const Transform2D& xb
) {
    AxisResult best{-std::numeric_limits<double>::max(), 0};

    for (std::size_t i = 0; i < a.count; ++i) {
        vector2d n = xa.rotate(a.normals[i]);
        vector2d v = xa.apply(a.vertices[i]);
        double min_proj = std::numeric_limits<double>::max();

        for (std::size_t j = 0; j < b.count; ++j) {
            double proj = n.dot(xb.apply(b.vertices[j]) - v);
            if (proj < min_proj) min_proj = proj;
        }

        if (min_proj > best.depth) { best.depth = min_proj; best.edge_index = i; }
    }

    return best;
}

inline int clip_segment_to_line(
    vector2d out[2], const vector2d in[2],
    const vector2d& n, double offset
) {
    int cnt = 0;
    double d0 = n.dot(in[0]) - offset;
    double d1 = n.dot(in[1]) - offset;
    if (d0 <= 0.0) out[cnt++] = in[0];
    if (d1 <= 0.0) out[cnt++] = in[1];

    if (d0 * d1 < 0.0) {
        double t = d0 / (d0 - d1);
        out[cnt++] = in[0] + (in[1] - in[0]) * t;
    }

    return cnt;
}

} // namespace detail_sat

inline bool collide_polygon_polygon(
    const PolygonShape& pa, const Transform2D& xa,
    const PolygonShape& pb, const Transform2D& xb,
    ContactManifold& m
) {
    auto sepA = detail_sat::find_min_separation(pa, xa, pb, xb);
    if (sepA.depth > 0.0) return false;
    auto sepB = detail_sat::find_min_separation(pb, xb, pa, xa);
    if (sepB.depth > 0.0) return false;
    const PolygonShape* ref_poly;
    const PolygonShape* inc_poly;
    const Transform2D* ref_xf;
    const Transform2D* inc_xf;
    std::size_t ref_edge;
    bool flip = false;
    constexpr double BIAS = 0.95;

    if (sepB.depth > BIAS * sepA.depth + 0.005) {
        ref_poly = &pb; ref_xf = &xb;
        inc_poly = &pa; inc_xf = &xa;
        ref_edge = sepB.edge_index;
        flip = true;
    } else {
        ref_poly = &pa; ref_xf = &xa;
        inc_poly = &pb; inc_xf = &xb;
        ref_edge = sepA.edge_index;
    }

    std::size_t i1 = ref_edge;
    std::size_t i2 = (i1 + 1) % ref_poly->count;
    vector2d rv1 = ref_xf->apply(ref_poly->vertices[i1]);
    vector2d rv2 = ref_xf->apply(ref_poly->vertices[i2]);
    vector2d ref_normal = ref_xf->rotate(ref_poly->normals[ref_edge]);
    vector2d ref_tangent = {ref_normal.y, -ref_normal.x};
    vector2d n_local_inc = inc_xf->rotate_inverse(ref_normal);
    double min_dot = std::numeric_limits<double>::max();
    std::size_t inc_edge = 0;

    for (std::size_t i = 0; i < inc_poly->count; ++i) {
        double d = n_local_inc.dot(inc_poly->normals[i]);
        if (d < min_dot) { min_dot = d; inc_edge = i; }
    }

    vector2d iv1 = inc_xf->apply(inc_poly->vertices[inc_edge]);
    vector2d iv2 = inc_xf->apply(inc_poly->vertices[(inc_edge + 1) % inc_poly->count]);
    double ref_c1 = ref_tangent.dot(rv1);
    double ref_c2 = ref_tangent.dot(rv2);
    vector2d clip_in[2] = { iv1, iv2 };
    vector2d clip_out[2];
    int n1 = detail_sat::clip_segment_to_line(clip_out, clip_in, ref_tangent * -1.0, -ref_c1);
    if (n1 < 2) return false;
    vector2d clip_out2[2];
    int n2 = detail_sat::clip_segment_to_line(clip_out2, clip_out, ref_tangent, ref_c2);
    if (n2 < 2) return false;
    double ref_offset = ref_normal.dot(rv1);
    m.normal = flip ? ref_normal * -1.0 : ref_normal;
    m.point_count = 0;

    for (int i = 0; i < n2 && m.point_count < 2; ++i) {
        double sep = ref_normal.dot(clip_out2[i]) - ref_offset;

        if (sep <= 0.0) {
            m.points[m.point_count].position = clip_out2[i];
            m.points[m.point_count].separation = sep;
            ++m.point_count;
        }
    }

    return m.point_count > 0;
}

inline bool collide_circle_capsule(
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

inline bool collide_polygon_capsule(
    const PolygonShape& poly, const Transform2D& xp,
    const CapsuleShape& cap, const Transform2D& xk,
    ContactManifold& m, bool flip
) {
    vector2d p1 = xk.apply(cap.point1);
    vector2d p2 = xk.apply(cap.point2);
    vector2d spine = p2 - p1;
    double spine_len = spine.magnitude();
    vector2d spine_dir = (spine_len >= constants::middle_epsilon()) ? spine * (1.0 / spine_len) : vector2d{1.0, 0.0};
    vector2d cap_normal = {-spine_dir.y, spine_dir.x};
    double best_depth = std::numeric_limits<double>::max();
    vector2d best_axis{};

    auto test_axis = [&](vector2d axis) -> bool {
        if (axis.magnitude_squared() <= constants::middle_epsilon()) return true;
        axis = axis.unit_vector();
        double plo, phi;
        project_polygon(poly, xp, axis, plo, phi);
        double clo, chi;
        project_capsule(cap, xk, axis, clo, chi);
        double overlap = std::min(phi, chi) - std::max(plo, clo);
        if (overlap <= 0.0) return false;

        if (overlap < best_depth) {
            best_depth = overlap;
            double cA = (plo + phi) * 0.5;
            double cB = (clo + chi) * 0.5;
            best_axis = (cB > cA) ? axis : axis * -1.0;
        }

        return true;
    };

    for (std::size_t i = 0; i < poly.count; ++i) { if (!test_axis(xp.rotate(poly.normals[i]))) return false; }
    if (!test_axis(cap_normal)) return false;

    for (std::size_t i = 0; i < poly.count; ++i) {
        vector2d v = xp.apply(poly.vertices[i]);
        vector2d d1 = v - p1, d2 = v - p2;
        if (!test_axis(d1)) return false;
        if (!test_axis(d2)) return false;
    }

    if (best_depth <= 0.0) return false;
    m.normal = flip ? best_axis * -1.0 : best_axis;
    m.point_count = 1;
    double deepest = std::numeric_limits<double>::max();
    vector2d deepest_pt{};

    for (std::size_t i = 0; i < poly.count; ++i) {
        vector2d v = xp.apply(poly.vertices[i]);
        double d = best_axis.dot(v);
        if (d < deepest) { deepest = d; deepest_pt = v; }
    }

    m.points[0].position = deepest_pt + best_axis * (best_depth * 0.5);
    m.points[0].separation = -best_depth;
    return true;
}

inline bool collide_capsule_capsule(
    const CapsuleShape& ca, const Transform2D& xa,
    const CapsuleShape& cb, const Transform2D& xb,
    ContactManifold& m
) {
    vector2d a1 = xa.apply(ca.point1), a2 = xa.apply(ca.point2);
    vector2d b1 = xb.apply(cb.point1), b2 = xb.apply(cb.point2);
    vector2d da = a2 - a1, db = b2 - b1, r = a1 - b1;
    double a = da.dot(da), e = db.dot(db), f = db.dot(r);
    double s, t;

    if (a <= constants::middle_epsilon() && e <= constants::middle_epsilon()) {
        s = t = 0.0;
    } else if (a <= constants::middle_epsilon()) {
        s = 0.0; t = std::clamp(f / e, 0.0, 1.0);
    } else {
        double c = da.dot(r);

        if (e <= constants::middle_epsilon()) {
            t = 0.0; s = std::clamp(-c / a, 0.0, 1.0);
        } else {
            double b_val = da.dot(db);
            double denom = a * e - b_val * b_val;
            s = (denom >= constants::middle_epsilon()) ? std::clamp((b_val * f - c * e) / denom, 0.0, 1.0) : 0.0;
            t = (b_val * s + f) / e;
            if (t < 0.0) { t = 0.0; s = std::clamp(-c / a, 0.0, 1.0); }
            else if (t > 1.0) { t = 1.0; s = std::clamp((b_val - c) / a, 0.0, 1.0); }
        }
    }

    vector2d cA = a1 + da * s;
    vector2d cB = b1 + db * t;
    CircleShape phA; phA.center = {}; phA.radius = ca.radius;
    CircleShape phB; phB.center = {}; phB.radius = cb.radius;
    Transform2D txA; txA.position = cA; txA.cos_a = 1.0; txA.sin_a = 0.0;
    Transform2D txB; txB.position = cB; txB.cos_a = 1.0; txB.sin_a = 0.0;
    return collide_circle_circle(phA, txA, phB, txB, m);
}

inline bool collide_edge_circle(
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
    m.points[0].position = closest;
    m.points[0].separation = dist - r;
    return true;
}

inline bool collide_edge_polygon(
    const EdgeShape& edge, const Transform2D& xe,
    const PolygonShape& poly, const Transform2D& xp,
    ContactManifold& m, bool flip
) {
    vector2d ev1 = xe.apply(edge.v1);
    vector2d ev2 = xe.apply(edge.v2);
    vector2d e_dir = ev2 - ev1;
    vector2d e_norm = vector2d(e_dir.y, -e_dir.x);
    double len = e_norm.magnitude();
    if (len <= constants::middle_epsilon()) return false;
    e_norm = e_norm * (1.0 / len);
    double best_depth = std::numeric_limits<double>::max();
    vector2d best_axis{};

    auto test_axis = [&](vector2d axis) -> bool {
        if (axis.magnitude_squared() <= constants::middle_epsilon()) return true;
        axis = axis.unit_vector();
        double elo = axis.dot(ev1), ehi = elo;
        double d2 = axis.dot(ev2);
        if (d2 < elo) elo = d2; else ehi = d2;
        double plo, phi;
        project_polygon(poly, xp, axis, plo, phi);
        double overlap = std::min(ehi, phi) - std::max(elo, plo);
        if (overlap <= 0.0) return false;

        if (overlap < best_depth) {
            best_depth = overlap;
            double cA = (elo + ehi) * 0.5;
            double cB = (plo + phi) * 0.5;
            best_axis = (cB > cA) ? axis : axis * -1.0;
        }

        return true;
    };

    if (!test_axis(e_norm)) return false;
    for (std::size_t i = 0; i < poly.count; ++i) { if (!test_axis(xp.rotate(poly.normals[i]))) return false; }
    if (best_depth <= 0.0) return false;
    m.normal = flip ? best_axis * -1.0 : best_axis;
    m.point_count = 1;
    double deepest = std::numeric_limits<double>::max();
    vector2d deepest_pt{};

    for (std::size_t i = 0; i < poly.count; ++i) {
        vector2d v = xp.apply(poly.vertices[i]);
        double d = best_axis.dot(v);
        if (d < deepest) { deepest = d; deepest_pt = v; }
    }

    m.points[0].position = deepest_pt + best_axis * (best_depth * 0.5);
    m.points[0].separation = -best_depth;
    return true;
}

inline bool collide_edge_capsule(
    const EdgeShape& edge, const Transform2D& xe,
    const CapsuleShape& cap, const Transform2D& xk,
    ContactManifold& m, bool flip
) {
    vector2d ev1 = xe.apply(edge.v1);
    vector2d ev2 = xe.apply(edge.v2);
    vector2d p1 = xk.apply(cap.point1);
    vector2d p2 = xk.apply(cap.point2);

    auto seg_closest = [](
        const vector2d& a1, const vector2d& a2,
        const vector2d& b1, const vector2d& b2,
        vector2d& cA, vector2d& cB
    ) {
        vector2d da = a2 - a1, db = b2 - b1, r = a1 - b1;
        double a = da.dot(da), e = db.dot(db), f = db.dot(r);
        double s, t;
        if (a <= constants::middle_epsilon() && e <= constants::middle_epsilon()) { s = t = 0.0; }
        else if (a <= constants::middle_epsilon()) { s = 0.0; t = std::clamp(f / e, 0.0, 1.0); }
        else {
            double c = da.dot(r);
            if (e <= constants::middle_epsilon()) { t = 0.0; s = std::clamp(-c / a, 0.0, 1.0); }
            else {
                double b_v = da.dot(db);
                double den = a * e - b_v * b_v;
                s = (den >= constants::middle_epsilon()) ? std::clamp((b_v * f - c * e) / den, 0.0, 1.0) : 0.0;
                t = (b_v * s + f) / e;
                if (t < 0.0) { t = 0.0; s = std::clamp(-c / a, 0.0, 1.0); }
                else if (t > 1.0) { t = 1.0; s = std::clamp((b_v - c) / a, 0.0, 1.0); }
            }
        }

        cA = a1 + da * s;
        cB = b1 + db * t;
    };

    vector2d cE, cK;
    seg_closest(ev1, ev2, p1, p2, cE, cK);
    vector2d d = cE - cK;
    double dist_sq = d.magnitude_squared();
    double r = cap.radius;
    if (dist_sq > r * r) return false;
    double dist = std::sqrt(dist_sq);
    vector2d n = (dist >= constants::middle_epsilon()) ? d * (1.0 / dist) : xe.rotate(edge.normal());
    m.normal = flip ? n * -1.0 : n;
    m.point_count = 1;
    m.points[0].position = cK + n * (r * 0.5);
    m.points[0].separation = dist - r;
    return true;
}

} // namespace narrowphase

inline bool collide_shapes(
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

    if (hit && flip) {
        manifold.normal = manifold.normal * -1.0;
        std::swap(manifold.body_a, manifold.body_b);
    }

    return hit;
}

struct VelocityConstraintPoint {
    vector2d rA{}; // contact − center_A (m)
    vector2d rB{}; // contact − center_B (m)
    double normal_mass  = 0.0;
    double tangent_mass = 0.0;
    double velocity_bias = 0.0;
    double normal_impulse  = 0.0; // warm-start accumulator (N*s)
    double tangent_impulse = 0.0;
};

struct VelocityConstraint {
    RigidBody2D* body_a = nullptr;
    RigidBody2D* body_b = nullptr;
    vector2d normal{};
    double friction = 0.0;
    double restitution = 0.0;
    double restitution_threshold = 1.0;
    VelocityConstraintPoint points[2];
    int point_count = 0;
    std::size_t manifold_index = 0;
};

struct ContactEvent {
    RigidBody2D* body_a = nullptr;
    RigidBody2D* body_b = nullptr;
    vector2d normal{};
    vector2d point{};
    double impulse = 0.0;
};

class Simulation2D {
public:
    struct Config {
        vector2d gravity           = {0.0, -9.81};   // m/s2
        double   fixed_dt          = 1.0 / 60.0;     // s
        int      velocity_iters    = 8;
        int      position_iters    = 3;
        double   baumgarte_factor  = 0.2;            // dimensionless
        double   slop              = 0.005;          // m (allowed penetration)
        double   broadphase_margin = 0.1;            // m (AABB fattening)
        bool     warm_starting     = true;
        bool     allow_sleeping    = true;
        double   world_half_extent = 500.0;          // m
    };

    explicit Simulation2D(const Config& cfg = {})
        : m_cfg(cfg),
          m_tree(AABB(
              {-cfg.world_half_extent, -cfg.world_half_extent},
              { cfg.world_half_extent,  cfg.world_half_extent}
          ))
    {}

    RigidBody2D& add_body(PhysicsFlags preset = PhysicsFlags::DefaultDynamic) {
        m_bodies.emplace_back(preset);
        return m_bodies.back();
    }

    RigidBody2D& add_body(const vector2d& pos, PhysicsFlags preset = PhysicsFlags::DefaultDynamic) {
        auto& b = add_body(preset);
        b.set_position(pos);
        return b;
    }

    void remove_body(std::size_t index) {
        if (index < m_bodies.size()) { m_bodies.erase(m_bodies.begin() + static_cast<std::ptrdiff_t>(index)); }
    }

    std::size_t body_count() const noexcept { return m_bodies.size(); }
    RigidBody2D& body(std::size_t i) { return m_bodies[i]; }
    const RigidBody2D& body(std::size_t i) const { return m_bodies[i]; }

    std::vector<RigidBody2D>& bodies() noexcept { return m_bodies; }
    const std::vector<RigidBody2D>& bodies() const noexcept { return m_bodies; }

    Config& config() noexcept { return m_cfg; }
    const Config& config() const noexcept { return m_cfg; }

    void set_gravity(const vector2d& g) noexcept { m_cfg.gravity = g; }
    vector2d gravity() const noexcept { return m_cfg.gravity; }

    void on_contact_begin(std::function<void(const ContactEvent&)> cb) { m_on_begin   = std::move(cb); }
    void on_contact_end(std::function<void(const ContactEvent&)> cb)   { m_on_end     = std::move(cb); }
    void on_pre_solve(std::function<bool(RigidBody2D& a, RigidBody2D& b, const ContactManifold&)> cb)    { m_pre_solve  = std::move(cb); }
    void on_post_solve(std::function<bool(RigidBody2D& a, RigidBody2D& b, const ContactManifold&)> cb)  { m_post_solve = std::move(cb); }

    const std::vector<ContactManifold>& contacts() const noexcept { return m_manifolds; }

    void step(double dt) {
        if (dt <= 0.0) return;

        for (auto& b : m_bodies) {
            if (!has_flag(b.flags(), PhysicsFlags::Enabled)) continue;
            if (!b.is_awake()) continue;
            b.ensure_mass_uptodate();
            b.integrate_forces(dt, m_cfg.gravity);
        }

        broadphase();
        narrowphase();
        build_velocity_constraints();
        if (m_cfg.warm_starting) warm_start();
        for (int i = 0; i < m_cfg.velocity_iters; ++i) { solve_velocity_constraints(); }

        for (auto& b : m_bodies) {
            if (!has_flag(b.flags(), PhysicsFlags::Enabled)) continue;
            if (!b.is_awake()) continue;
            b.integrate_velocities(dt);
        }

        for (int i = 0; i < m_cfg.position_iters; ++i) { if (solve_position_constraints()) break; }

        for (auto& b : m_bodies) {
            if (!has_flag(b.flags(), PhysicsFlags::Enabled)) continue;
            b.synchronize_colliders();
            b.clear_forces();
        }

        if (m_cfg.allow_sleeping) {
            for (auto& b : m_bodies) {
                if (!has_flag(b.flags(), PhysicsFlags::Enabled)) continue;
                b.update_sleep(dt);
            }
        }

        if (m_post_solve) {
            for (auto& mf : m_manifolds) {
                if (mf.body_a && mf.body_b) m_post_solve(*mf.body_a, *mf.body_b, mf);
            }
        }

        cleanup_bodies();
    }

    void step() { step(m_cfg.fixed_dt); }

    std::vector<RigidBody2D*> query_aabb(const AABB& region) {
        synchronize_all_colliders();
        std::vector<RigidBody2D*> ptrs;
        ptrs.reserve(m_bodies.size());
        for (auto& b : m_bodies) ptrs.push_back(&b);
        std::vector<AABB> aabbs;
        aabbs.reserve(m_bodies.size());
        for (auto& b : m_bodies) aabbs.push_back(b.compute_body_aabb());

        m_tree.rebuild(
            AABB({-m_cfg.world_half_extent, -m_cfg.world_half_extent},
                 { m_cfg.world_half_extent,  m_cfg.world_half_extent}),
            ptrs.data(), aabbs.data(), ptrs.size()
        );

        return m_tree.query(region);
    }

    RigidBody2D* point_query(const vector2d& point) {
        AABB tiny = {{point.x - 0.001, point.y - 0.001}, {point.x + 0.001, point.y + 0.001}};
        auto results = query_aabb(tiny);

        for (auto* b : results) {
            AABB body_bb = b->compute_body_aabb();
            if (body_bb.contains(point)) return b;
        }

        return nullptr;
    }

private:
    void synchronize_all_colliders() {
        for (auto& b : m_bodies) {
            if (has_flag(b.flags(), PhysicsFlags::Enabled)) b.synchronize_colliders();
        }
    }

    void broadphase() {
        m_pairs.clear();
        synchronize_all_colliders();
        m_body_ptrs.clear();
        m_body_aabbs.clear();
        m_body_ptrs.reserve(m_bodies.size());
        m_body_aabbs.reserve(m_bodies.size());

        for (auto& b : m_bodies) {
            if (!has_flag(b.flags(), PhysicsFlags::Enabled)) continue;
            if (!has_flag(b.flags(), PhysicsFlags::BroadphaseActive) && !has_flag(b.flags(), PhysicsFlags::CollisionResponse)) continue;
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
            if (!a->is_awake() && !b->is_awake()) return;
            m_pairs.push_back({a, b});
        });
    }

    void narrowphase() {
        m_manifolds.clear();
        m_manifolds.reserve(m_pairs.size());
        for (auto& [a, b] : m_pairs) { generate_contacts(a, b); }
    }

    void generate_contacts(RigidBody2D* a, RigidBody2D* b) {
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
                if (!collide_shapes(ca.shape, wa, cb.shape, wb, mf)) continue;
                mf.body_a = a;
                mf.body_b = b;
                mf.friction    = std::sqrt(ca.material.friction * cb.material.friction);
                mf.restitution = std::max(ca.material.restitution, cb.material.restitution);

                mf.restitution_threshold = std::min(
                    ca.material.restitution_threshold,
                    cb.material.restitution_threshold
                );

                if (ca.is_sensor || cb.is_sensor) {
                    if (m_on_begin) {
                        for (int p = 0; p < mf.point_count; ++p) {
                            m_on_begin({a, b, mf.normal, mf.points[p].position, 0.0});
                        }
                    }
                    continue;
                }

                if (m_pre_solve) { if (!m_pre_solve(*a, *b, mf)) continue; }
                m_manifolds.push_back(mf);
            }
        }
    }

    void build_velocity_constraints() {
        m_constraints.clear();
        m_constraints.reserve(m_manifolds.size());

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

            for (int p = 0; p < mf.point_count; ++p) {
                auto& vcp = vc.points[p];
                vcp.rA = mf.points[p].position - cA;
                vcp.rB = mf.points[p].position - cB;
                double rnA = vcp.rA.perp_dot(mf.normal);
                double rnB = vcp.rB.perp_dot(mf.normal);
                double k_normal = inv_mA + inv_mB + inv_IA * rnA * rnA + inv_IB * rnB * rnB;
                vcp.normal_mass = (k_normal > 0.0) ? 1.0 / k_normal : 0.0;
                vector2d tangent = {-mf.normal.y, mf.normal.x};
                double rtA = vcp.rA.perp_dot(tangent);
                double rtB = vcp.rB.perp_dot(tangent);
                double k_tangent = inv_mA + inv_mB + inv_IA * rtA * rtA + inv_IB * rtB * rtB;
                vcp.tangent_mass = (k_tangent > 0.0) ? 1.0 / k_tangent : 0.0;
                vector2d vA = a->linear_velocity() + vector2d(-a->angular_velocity() * vcp.rA.y, a->angular_velocity() * vcp.rA.x);
                vector2d vB = b->linear_velocity() + vector2d(-b->angular_velocity() * vcp.rB.y, b->angular_velocity() * vcp.rB.x);
                double vn_rel = (vB - vA).dot(mf.normal);
                vcp.velocity_bias = 0.0;
                if (vn_rel < -vc.restitution_threshold) { vcp.velocity_bias = -vc.restitution * vn_rel; }
            }

            m_constraints.push_back(vc);
        }
    }

    void warm_start() {
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

    void solve_velocity_constraints() {
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
                vA = a->linear_velocity() + vector2d(-a->angular_velocity() * vcp.rA.y, a->angular_velocity() * vcp.rA.x);
                vB = b->linear_velocity() + vector2d(-b->angular_velocity() * vcp.rB.y, b->angular_velocity() * vcp.rB.x);
                dv = vB - vA;
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

    bool solve_position_constraints() {
        double min_separation = 0.0;

        for (auto& mf : m_manifolds) {
            RigidBody2D* a = mf.body_a;
            RigidBody2D* b = mf.body_b;
            a->ensure_mass_uptodate();
            b->ensure_mass_uptodate();
            double inv_mA = a->inv_mass();
            double inv_mB = b->inv_mass();
            double inv_IA = a->inv_inertia();
            double inv_IB = b->inv_inertia();
            vector2d cA = a->world_center();
            vector2d cB = b->world_center();

            for (int p = 0; p < mf.point_count; ++p) {
                vector2d rA = mf.points[p].position - cA;
                vector2d rB = mf.points[p].position - cB;
                double separation = mf.points[p].separation;
                min_separation = std::min(min_separation, separation);

                double C = std::clamp(
                    m_cfg.baumgarte_factor * (separation + m_cfg.slop),
                    -0.2, 0.0
                );

                double rnA = rA.perp_dot(mf.normal);
                double rnB = rB.perp_dot(mf.normal);
                double K = inv_mA + inv_mB + inv_IA * rnA * rnA + inv_IB * rnB * rnB;
                double impulse = (K > 0.0) ? -C / K : 0.0;
                vector2d P = mf.normal * impulse;
                a->set_position(a->position() - P * inv_mA);
                a->set_angle(a->angle() - inv_IA * rA.perp_dot(P));
                b->set_position(b->position() + P * inv_mB);
                b->set_angle(b->angle() + inv_IB * rB.perp_dot(P));
            }
        }

        return min_separation >= -3.0 * m_cfg.slop;
    }

    void cleanup_bodies() {
        AABB world_bounds(
            {-m_cfg.world_half_extent, -m_cfg.world_half_extent},
            { m_cfg.world_half_extent,  m_cfg.world_half_extent}
        );

        for (auto it = m_bodies.begin(); it != m_bodies.end(); ) {
            bool remove = false;

            if (has_flag(it->flags(), PhysicsFlags::DestroyOnSleep) && !it->is_awake()) { remove = true; }

            if (has_flag(it->flags(), PhysicsFlags::DestroyOffScreen)) {
                AABB bb = it->compute_body_aabb();
                if (!world_bounds.overlaps(bb)) remove = true;
            }

            if (remove) {
                it = m_bodies.erase(it);
            } else {
                ++it;
            }
        }
    }

private:
    Config m_cfg;

    std::vector<RigidBody2D> m_bodies;

    Quadtree<RigidBody2D>     m_tree;
    std::vector<RigidBody2D*> m_body_ptrs;
    std::vector<AABB>         m_body_aabbs;
    std::vector<std::pair<RigidBody2D*, RigidBody2D*>> m_pairs;

    std::vector<ContactManifold>    m_manifolds;
    std::vector<VelocityConstraint> m_constraints;

    std::function<void(const ContactEvent&)> m_on_begin;
    std::function<void(const ContactEvent&)> m_on_end;
    std::function<bool(RigidBody2D& a, RigidBody2D& b, const ContactManifold&)> m_pre_solve;
    std::function<bool(RigidBody2D& a, RigidBody2D& b, const ContactManifold&)> m_post_solve;
};

} // namespace physics
} // namespace fizmo

#endif // FIZMO_SIMULATION_2D_HPP