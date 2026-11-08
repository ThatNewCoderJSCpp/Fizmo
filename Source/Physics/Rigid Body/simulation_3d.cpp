#include "fizmo_library.hpp"
#include "simulation_3d.hpp"

namespace fizmo {
namespace physics {
namespace narrowphase3d {

RVec3 closest_point_on_segment(const RVec3& p, const RVec3& a, const RVec3& b) {
    RVec3 ab = b - a;
    Real t = vec3::length_squared(ab);
    if (t <= constants::middle_epsilon()) return a;
    t = std::clamp(vec3::dot(p - a, ab) / t, 0.0_r, 1.0_r);
    return a + ab * t;
}

SegmentPair closest_segment_points(const RVec3& p1, const RVec3& q1, const RVec3& p2, const RVec3& q2) {
    RVec3 d1 = q1 - p1, d2 = q2 - p2, r = p1 - p2;
    Real a = vec3::dot(d1, d1), e = vec3::dot(d2, d2), f = vec3::dot(d2, r);
    Real s, t;
    const Real eps = constants::middle_epsilon();

    if (a <= eps && e <= eps) {
        s = t = 0.0_r;
    } else if (a <= eps) {
        s = 0.0_r; t = std::clamp(f / e, 0.0_r, 1.0_r);
    } else {
        Real c = vec3::dot(d1, r);

        if (e <= eps) {
            t = 0.0_r; s = std::clamp(-c / a, 0.0_r, 1.0_r);
        } else {
            Real b = vec3::dot(d1, d2);
            Real denom = a * e - b * b;
            s = (denom > eps) ? std::clamp((b * f - c * e) / denom, 0.0_r, 1.0_r) : 0.0_r;
            t = (b * s + f) / e;
            if (t < 0.0_r) { t = 0.0_r; s = std::clamp(-c / a, 0.0_r, 1.0_r); }
            else if (t > 1.0_r) { t = 1.0_r; s = std::clamp((b - c) / a, 0.0_r, 1.0_r); }
        }
    }

    SegmentPair out;
    out.closest1 = p1 + d1 * s;
    out.closest2 = p2 + d2 * t;
    out.fraction1 = s;
    out.fraction2 = t;
    out.distance_squared = vec3::length_squared(out.closest2 - out.closest1);
    return out;
}

void reduce_contacts(ContactManifold3D& m, const std::vector<Candidate>& c, const RVec3& normal) {
    m.point_count = 0;
    if (c.empty()) return;

    if (c.size() <= static_cast<std::size_t>(ContactManifold3D::MAX_POINTS)) {
        for (const auto& k : c) { m.points[m.point_count].position = k.position; m.points[m.point_count].separation = k.separation; ++m.point_count; }
        return;
    }

    std::size_t i0 = 0;
    for (std::size_t i = 1; i < c.size(); ++i) if (c[i].separation < c[i0].separation) i0 = i;
    std::size_t i1 = i0;
    Real best = -1.0_r;

    for (std::size_t i = 0; i < c.size(); ++i) {
        Real d = vec3::length_squared(c[i].position - c[i0].position);
        if (d > best) { best = d; i1 = i; }
    }

    std::size_t i2 = c.size(), i3 = c.size();
    Real max_area = 0.0_r, min_area = 0.0_r;
    const RVec3 e = c[i1].position - c[i0].position;

    for (std::size_t i = 0; i < c.size(); ++i) {
        Real area = vec3::dot(vec3::cross(e, c[i].position - c[i0].position), normal);
        if (area > max_area) { max_area = area; i2 = i; }
        if (area < min_area) { min_area = area; i3 = i; }
    }

    const std::size_t picks[4] = { i0, i1, i2, i3 };

    for (std::size_t k = 0; k < 4; ++k) {
        std::size_t i = picks[k];
        if (i >= c.size()) continue;
        bool dup = false;
        for (std::size_t q = 0; q < k; ++q) if (picks[q] == i) dup = true;
        if (dup) continue;
        m.points[m.point_count].position = c[i].position;
        m.points[m.point_count].separation = c[i].separation;
        ++m.point_count;
    }
}

bool point_in_face(const HullShape& h, const HullFace& f, const RVec3& q) {
    const std::size_t n = f.indices.size();

    for (std::size_t i = 0; i < n; ++i) {
        const RVec3& a = h.vertices[f.indices[i]];
        const RVec3& b = h.vertices[f.indices[(i + 1) % n]];
        if (vec3::dot(vec3::cross(b - a, q - a), f.normal) < 0.0_r) return false;
    }

    return true;
}

RVec3 closest_point_on_face(const HullShape& h, const HullFace& f, const RVec3& p) {
    RVec3 q = p - f.normal * (vec3::dot(f.normal, p) - f.offset);
    if (point_in_face(h, f, q)) return q;
    const std::size_t n = f.indices.size();
    Real best = std::numeric_limits<Real>::max();
    RVec3 out = q;

    for (std::size_t i = 0; i < n; ++i) {
        RVec3 c = closest_point_on_segment(p, h.vertices[f.indices[i]], h.vertices[f.indices[(i + 1) % n]]);
        Real d = vec3::length_squared(p - c);
        if (d < best) { best = d; out = c; }
    }

    return out;
}

PointHullResult closest_point_on_hull(const HullShape& h, const RVec3& p) {
    PointHullResult r;
    Real max_sep = -std::numeric_limits<Real>::max();
    std::size_t face = 0;

    for (std::size_t i = 0; i < h.faces.size(); ++i) {
        Real s = vec3::dot(h.faces[i].normal, p) - h.faces[i].offset;
        if (s > max_sep) { max_sep = s; face = i; }
    }

    if (max_sep <= 0.0_r) {
        r.inside = true;
        r.normal = h.faces[face].normal;
        r.point = p - r.normal * max_sep;
        r.distance = max_sep;
        return r;
    }

    Real best = std::numeric_limits<Real>::max();

    for (const auto& f : h.faces) {
        if (vec3::dot(f.normal, p) - f.offset <= 0.0_r) continue;
        RVec3 c = closest_point_on_face(h, f, p);
        Real d = vec3::length_squared(p - c);
        if (d < best) { best = d; r.point = c; }
    }

    r.distance = std::sqrt(best);
    r.normal = (r.distance > constants::middle_epsilon()) ? (p - r.point) * (1.0_r / r.distance) : h.faces[face].normal;
    return r;
}

bool segment_intersects_hull(const HullShape& h, const RVec3& p1, const RVec3& p2) {
    Real tmin = 0.0_r, tmax = 1.0_r;
    const RVec3 d = p2 - p1;

    for (const auto& f : h.faces) {
        Real denom = vec3::dot(f.normal, d);
        Real dist  = vec3::dot(f.normal, p1) - f.offset;

        if (std::abs(denom) <= 1e-15_r) {
            if (dist > 0.0_r) return false;
            continue;
        }

        Real t = -dist / denom;
        if (denom < 0.0_r) tmin = std::max(tmin, t);
        else             tmax = std::min(tmax, t);
        if (tmin > tmax) return false;
    }

    return true;
}

bool clip_segment_to_face(const HullShape& h, const HullFace& f, RVec3& a, RVec3& b) {
    Real tmin = 0.0_r, tmax = 1.0_r;
    const RVec3 d = b - a;
    const std::size_t n = f.indices.size();

    for (std::size_t i = 0; i < n; ++i) {
        const RVec3& v0 = h.vertices[f.indices[i]];
        const RVec3& v1 = h.vertices[f.indices[(i + 1) % n]];
        RVec3 side = vec3::cross(v1 - v0, f.normal); 
        Real dist  = vec3::dot(side, a - v0);
        Real denom = vec3::dot(side, d);

        if (std::abs(denom) <= 1e-15_r) {
            if (dist > 0.0_r) return false;
            continue;
        }

        Real t = -dist / denom;
        if (denom > 0.0_r) tmax = std::min(tmax, t);
        else             tmin = std::max(tmin, t);
        if (tmin > tmax) return false;
    }

    RVec3 a0 = a;
    a = a0 + d * tmin;
    b = a0 + d * tmax;
    return true;
}

int support_edge(const HullShape& h, const RVec3& dir, const RVec3& axis, Real sign) {
    int best = -1;
    Real best_d = -std::numeric_limits<Real>::max();

    for (std::size_t i = 0; i < h.edges.size(); ++i) {
        const RVec3& a = h.vertices[h.edges[i][0]];
        const RVec3& b = h.vertices[h.edges[i][1]];
        RVec3 e = vec3::normalize(b - a);
        if (std::abs(vec3::dot(e, dir)) < 1.0_r - 1e-6_r) continue;
        Real d = sign * vec3::dot(axis, (a + b) * 0.5_r);
        if (d > best_d) { best_d = d; best = static_cast<int>(i); }
    }

    return best;
}

void project_hull(const HullShape& h, const RVec3& axis, Real& lo, Real& hi) {
    lo = std::numeric_limits<Real>::max();
    hi = -lo;
    for (const auto& v : h.vertices) { Real d = vec3::dot(axis, v); lo = std::min(lo, d); hi = std::max(hi, d); }
}

void project_points(const std::vector<RVec3>& pts, const RVec3& axis, Real& lo, Real& hi) {
    lo = std::numeric_limits<Real>::max();
    hi = -lo;
    for (const auto& v : pts) { Real d = vec3::dot(axis, v); lo = std::min(lo, d); hi = std::max(hi, d); }
}

bool collide_sphere_sphere(
    const SphereShape& sa, const Transform3D& xa,
    const SphereShape& sb, const Transform3D& xb,
    ContactManifold3D& m, Real margin 
) {
    RVec3 cA = xa.apply(sa.center);
    RVec3 cB = xb.apply(sb.center);
    RVec3 d = cB - cA;
    Real dist_sq = vec3::length_squared(d);
    Real r_sum = sa.radius + sb.radius;
    if (dist_sq > (r_sum + margin) * (r_sum + margin)) return false;
    Real dist = std::sqrt(dist_sq);
    m.normal = (dist >= constants::middle_epsilon()) ? d * (1.0_r / dist) : RVec3{0.0_r, 1.0_r, 0.0_r};
    Real pen = r_sum - dist;
    m.point_count = 1;
    m.points[0].position = cA + m.normal * (sa.radius - pen * 0.5_r);
    m.points[0].separation = -pen;
    return true;
}

bool collide_sphere_point(const RVec3& c, Real ra, const RVec3& p, Real rb, ContactManifold3D& m, Real margin) {
    SphereShape a{ c, ra }, b{ p, rb };
    return collide_sphere_sphere(a, Transform3D{}, b, Transform3D{}, m, margin);
}

bool collide_sphere_capsule(
    const SphereShape& s, const Transform3D& xs,
    const CapsuleShape3D& k, const Transform3D& xk,
    ContactManifold3D& m, Real margin 
) {
    RVec3 c = xs.apply(s.center);
    RVec3 p = closest_point_on_segment(c, xk.apply(k.point1), xk.apply(k.point2));
    return collide_sphere_point(c, s.radius, p, k.radius, m, margin);
}

bool collide_sphere_hull(
    const SphereShape& s, const Transform3D& xs,
    const HullShape& h, const Transform3D& xh,
    ContactManifold3D& m, Real margin 
) {
    if (!h.valid()) return false;
    const RVec3 c = xh.apply_inverse(xs.apply(s.center)); 
    PointHullResult q = closest_point_on_hull(h, c);
    if (q.distance > s.radius + margin) return false;
    const RVec3 n = xh.rotate(q.normal); 
    const RVec3 cw = xs.apply(s.center);
    m.normal = n * -1.0_r;
    m.point_count = 1;
    m.points[0].position = cw - n * ((s.radius + q.distance) * 0.5_r);
    m.points[0].separation = q.distance - s.radius;
    return true;
}

bool collide_sphere_plane(
    const SphereShape& s, const Transform3D& xs,
    const PlaneShape& p, const Transform3D& xp,
    ContactManifold3D& m, Real margin 
) {
    RVec3 n; Real d;
    plane_in_world(p, xp, n, d);
    RVec3 c = xs.apply(s.center);
    Real dist = vec3::dot(n, c) - d;
    if (dist > s.radius + margin) return false;
    m.normal = n * -1.0_r;
    m.point_count = 1;
    m.points[0].position = c - n * ((s.radius + dist) * 0.5_r);
    m.points[0].separation = dist - s.radius;
    return true;
}

bool collide_capsule_capsule(
    const CapsuleShape3D& ka, const Transform3D& xa,
    const CapsuleShape3D& kb, const Transform3D& xb,
    ContactManifold3D& m, Real margin 
) {
    const RVec3 p1 = xa.apply(ka.point1), q1 = xa.apply(ka.point2);
    const RVec3 p2 = xb.apply(kb.point1), q2 = xb.apply(kb.point2);
    const Real R = ka.radius + kb.radius;
    SegmentPair sp = closest_segment_points(p1, q1, p2, q2);
    if (sp.distance_squared > (R + margin) * (R + margin)) return false;
    const Real dist = std::sqrt(sp.distance_squared);
    const RVec3 d1 = q1 - p1, d2 = q2 - p2;
    RVec3 n;

    if (dist > constants::middle_epsilon()) {
        n = (sp.closest2 - sp.closest1) * (1.0_r / dist);
    } else {
        RVec3 axis = vec3::normalize(d1), t1, t2;
        vec3::orthonormal_basis(axis, t1, t2);
        n = t1;
    }

    m.normal = n;
    m.point_count = 0;
    const Real l1 = vec3::length_squared(d1), l2 = vec3::length_squared(d2);

    if (l1 > constants::middle_epsilon() && l2 > constants::middle_epsilon() &&
        vec3::length_squared(vec3::cross(d1, d2)) < 0.0025_r * l1 * l2) {
        Real ta = vec3::dot(p2 - p1, d1) / l1, tb = vec3::dot(q2 - p1, d1) / l1;
        Real lo = std::max(0.0_r, std::min(ta, tb)), hi = std::min(1.0_r, std::max(ta, tb));

        if (hi - lo > 1e-6_r) {
            for (Real t : { lo, hi }) {
                RVec3 a = p1 + d1 * t;
                RVec3 b = closest_point_on_segment(a, p2, q2);
                Real sep = vec3::dot(b - a, n) - R;
                if (sep > margin) continue;
                m.points[m.point_count].position = ((a + n * ka.radius) + (b - n * kb.radius)) * 0.5_r;
                m.points[m.point_count].separation = sep;
                ++m.point_count;
            }

            if (m.point_count > 0) return true;
        }
    }

    m.point_count = 1;
    m.points[0].position = ((sp.closest1 + n * ka.radius) + (sp.closest2 - n * kb.radius)) * 0.5_r;
    m.points[0].separation = dist - R;
    return true;
}

bool collide_capsule_plane(
    const CapsuleShape3D& k, const Transform3D& xk,
    const PlaneShape& p, const Transform3D& xp,
    ContactManifold3D& m, Real margin 
) {
    RVec3 n; Real d;
    plane_in_world(p, xp, n, d);
    m.point_count = 0;

    for (const RVec3& e : { xk.apply(k.point1), xk.apply(k.point2) }) {
        Real dist = vec3::dot(n, e) - d;
        if (dist > k.radius + margin) continue;
        m.points[m.point_count].position = e - n * ((k.radius + dist) * 0.5_r);
        m.points[m.point_count].separation = dist - k.radius;
        ++m.point_count;
    }

    m.normal = n * -1.0_r;
    return m.point_count > 0;
}

bool collide_hull_plane(
    const HullShape& h, const Transform3D& xh,
    const PlaneShape& p, const Transform3D& xp,
    ContactManifold3D& m, Real margin 
) {
    if (!h.valid()) return false;
    RVec3 n; Real d;
    plane_in_world(p, xp, n, d);
    std::vector<Candidate> c;

    for (const auto& v : h.vertices) {
        RVec3 w = xh.apply(v);
        Real s = vec3::dot(n, w) - d;
        if (s <= margin) c.push_back({ w - n * (s * 0.5_r), s });
    }

    if (c.empty()) return false;
    m.normal = n * -1.0_r;
    reduce_contacts(m, c, m.normal);
    return m.point_count > 0;
}

bool collide_capsule_hull(
    const CapsuleShape3D& k, const Transform3D& xk,
    const HullShape& h, const Transform3D& xh,
    ContactManifold3D& m, Real margin 
) {
    if (!h.valid()) return false;
    const RVec3 p1 = xh.apply_inverse(xk.apply(k.point1)); 
    const RVec3 p2 = xh.apply_inverse(xk.apply(k.point2));
    const Real r = k.radius;
    RVec3 n_local{};           
    std::vector<Candidate> local; 

    auto add_face_points = [&](const HullFace& f) {
        RVec3 a = p1, b = p2;
        if (!clip_segment_to_face(h, f, a, b)) return;

        for (const RVec3& q : { a, b }) {
            Real s = vec3::dot(f.normal, q) - f.offset;
            if (s - r > margin) continue;
            local.push_back({ q - f.normal * ((r + s) * 0.5_r), s - r });
        }

        if (local.size() == 2 && vec3::length_squared(local[0].position - local[1].position) <= 1e-18_r) local.pop_back();
    };

    if (!segment_intersects_hull(h, p1, p2)) {
        Real best = std::numeric_limits<Real>::max();
        RVec3 on_seg{}, on_hull{};

        for (const RVec3& e : { p1, p2 }) {
            PointHullResult q = closest_point_on_hull(h, e);
            Real d = vec3::length_squared(e - q.point);
            if (d < best) { best = d; on_seg = e; on_hull = q.point; }
        }

        for (const auto& e : h.edges) {
            SegmentPair sp = closest_segment_points(p1, p2, h.vertices[e[0]], h.vertices[e[1]]);
            if (sp.distance_squared < best) { best = sp.distance_squared; on_seg = sp.closest1; on_hull = sp.closest2; }
        }

        if (best > (r + margin) * (r + margin)) return false;
        const Real dist = std::sqrt(best);

        if (dist > constants::middle_epsilon()) {
            n_local = (on_seg - on_hull) * (1.0_r / dist);
            
            for (const auto& f : h.faces) {
                if (vec3::dot(f.normal, n_local) > 1.0_r - 1e-6_r) { n_local = f.normal; add_face_points(f); break; }
            }

            if (local.empty()) local.push_back({ (on_hull + (on_seg - n_local * r)) * 0.5_r, dist - r });
        }
    }

    if (local.empty()) {
        const RVec3 d = p2 - p1;
        Real best = -std::numeric_limits<Real>::max();
        int best_face = -1;
        RVec3 best_axis{};
        int best_dir = -1;

        for (std::size_t i = 0; i < h.faces.size(); ++i) {
            const auto& f = h.faces[i];
            Real s = std::min(vec3::dot(f.normal, p1), vec3::dot(f.normal, p2)) - f.offset - r;
            if (s > best) { best = s; best_face = static_cast<int>(i); best_axis = f.normal; }
        }

        const Real face_best = best;
        const Real dl = vec3::length(d);

        for (std::size_t i = 0; i < h.edge_directions.size() && dl > constants::middle_epsilon(); ++i) {
            RVec3 L = vec3::cross(d, h.edge_directions[i]);
            Real len = vec3::length(L);
            if (len < 1e-6_r * dl) continue;
            L = L * (1.0_r / len);
            Real hlo, hhi;
            project_hull(h, L, hlo, hhi);
            Real slo = std::min(vec3::dot(L, p1), vec3::dot(L, p2)) - r;
            Real shi = std::max(vec3::dot(L, p1), vec3::dot(L, p2)) + r;
            Real s_pos = slo - hhi, s_neg = hlo - shi;
            Real s = std::max(s_pos, s_neg);

            if (s > best && s > 0.95_r * face_best + 0.0025_r) {
                best = s;
                best_face = -1;
                best_dir = static_cast<int>(i);
                best_axis = (s_pos >= s_neg) ? L : L * -1.0_r;
            }
        }

        if (best > margin) return false;
        n_local = best_axis;

        if (best_face >= 0) {
            const HullFace& f = h.faces[static_cast<std::size_t>(best_face)];
            add_face_points(f);

            if (local.empty()) {
                const RVec3& q = (vec3::dot(f.normal, p1) < vec3::dot(f.normal, p2)) ? p1 : p2;
                Real s = vec3::dot(f.normal, q) - f.offset;
                local.push_back({ q - f.normal * ((r + s) * 0.5_r), s - r });
            }
        } else {
            int e = support_edge(h, h.edge_directions[static_cast<std::size_t>(best_dir)], n_local, 1.0_r);
            if (e < 0) return false;
            SegmentPair sp = closest_segment_points(p1, p2, h.vertices[h.edges[e][0]], h.vertices[h.edges[e][1]]);
            Real s = vec3::dot(sp.closest1 - sp.closest2, n_local) - r;
            local.push_back({ (sp.closest2 + (sp.closest1 - n_local * r)) * 0.5_r, s });
        }
    }

    const RVec3 n_world = xh.rotate(n_local);
    m.normal = n_world * -1.0_r; 
    m.point_count = 0;

    for (const auto& c : local) {
        if (m.point_count >= ContactManifold3D::MAX_POINTS) break;
        m.points[m.point_count].position = xh.apply(c.position);
        m.points[m.point_count].separation = c.separation;
        ++m.point_count;
    }

    return m.point_count > 0;
}

} // namespace narrowphase3d
} // namespace physics
} // namespace fizmo

namespace fizmo {
namespace physics {
namespace narrowphase3d {
namespace detail_sat3d {

FaceQuery query_faces(const HullShape& a, const std::vector<RVec3>& other) {
    FaceQuery q;

    for (std::size_t i = 0; i < a.faces.size(); ++i) {
        const auto& f = a.faces[i];
        Real s = std::numeric_limits<Real>::max();
        for (const auto& v : other) s = std::min(s, vec3::dot(f.normal, v) - f.offset);
        if (s > q.separation) { q.separation = s; q.index = static_cast<int>(i); }
    }

    return q;
}

EdgeQuery query_edges(const HullShape& a, const std::vector<RVec3>& b_vertices, const std::vector<RVec3>& b_dirs) {
    EdgeQuery q;

    for (std::size_t i = 0; i < a.edge_directions.size(); ++i) {
        for (std::size_t j = 0; j < b_dirs.size(); ++j) {
            RVec3 L = vec3::cross(a.edge_directions[i], b_dirs[j]);
            Real len = vec3::length(L);
            if (len < 1e-6_r) continue;
            L = L * (1.0_r / len);
            Real alo, ahi, blo, bhi;
            project_hull(a, L, alo, ahi);
            project_points(b_vertices, L, blo, bhi);
            Real s_pos = blo - ahi, s_neg = alo - bhi;
            Real s = std::max(s_pos, s_neg);

            if (s > q.separation) {
                q.separation = s;
                q.dir_a = static_cast<int>(i);
                q.dir_b = static_cast<int>(j);
                q.axis = (s_pos >= s_neg) ? L : L * -1.0_r;
            }
        }
    }

    return q;
}

void clip_polygon(std::vector<RVec3>& poly, const RVec3& n, Real d, std::vector<RVec3>& scratch) {
    scratch.clear();
    const std::size_t count = poly.size();

    for (std::size_t i = 0; i < count; ++i) {
        const RVec3& a = poly[i];
        const RVec3& b = poly[(i + 1) % count];
        Real da = vec3::dot(n, a) - d;
        Real db = vec3::dot(n, b) - d;
        if (da <= 0.0_r) scratch.push_back(a);
        if ((da < 0.0_r && db > 0.0_r) || (da > 0.0_r && db < 0.0_r)) scratch.push_back(a + (b - a) * (da / (da - db)));
    }

    poly.swap(scratch);
}

void face_contact(
    const HullShape& r, const Transform3D& xr, int rf,
    const HullShape& inc, const Transform3D& xi,
    std::vector<Candidate>& out, RVec3& normal, Real margin
) {
    const HullFace& ref = r.faces[static_cast<std::size_t>(rf)];
    normal = xr.rotate(ref.normal);
    std::vector<RVec3> ref_poly;
    for (auto idx : ref.indices) ref_poly.push_back(xr.apply(r.vertices[idx]));
    const Real offset = vec3::dot(normal, ref_poly[0]);
    const RVec3 n_in_i = xi.rotate_inverse(normal);
    std::size_t inc_face = 0;
    Real min_dot = std::numeric_limits<Real>::max();

    for (std::size_t f = 0; f < inc.faces.size(); ++f) {
        Real d = vec3::dot(inc.faces[f].normal, n_in_i);
        if (d < min_dot) { min_dot = d; inc_face = f; }
    }

    std::vector<RVec3> poly, scratch;
    for (auto idx : inc.faces[inc_face].indices) poly.push_back(xi.apply(inc.vertices[idx]));

    for (std::size_t k = 0; k < ref_poly.size() && !poly.empty(); ++k) {
        const RVec3& a = ref_poly[k];
        const RVec3& b = ref_poly[(k + 1) % ref_poly.size()];
        RVec3 side = vec3::cross(b - a, normal);
        clip_polygon(poly, side, vec3::dot(side, a), scratch);
    }

    for (const auto& p : poly) {
        Real s = vec3::dot(normal, p) - offset;
        if (s <= margin) out.push_back({ p - normal * (s * 0.5_r), s });
    }

    if (out.empty()) {
        Real best = std::numeric_limits<Real>::max();
        RVec3 deepest{};

        for (const auto& v : inc.vertices) {
            RVec3 w = xi.apply(v);
            Real s = vec3::dot(normal, w) - offset;
            if (s < best) { best = s; deepest = w; }
        }

        if (best <= margin) out.push_back({ deepest - normal * (best * 0.5_r), best });
    }
}

} // namespace detail_sat3d
} // namespace narrowphase3d
} // namespace physics
} // namespace fizmo

namespace fizmo {
namespace physics {
namespace narrowphase3d {

bool collide_hull_hull(
    const HullShape& a, const Transform3D& xa,
    const HullShape& b, const Transform3D& xb,
    ContactManifold3D& m, Real margin 
) {
    using namespace detail_sat3d;
    if (!a.valid() || !b.valid()) return false;
    std::vector<RVec3> b_in_a, a_in_b, b_dirs;
    b_in_a.reserve(b.vertices.size());
    a_in_b.reserve(a.vertices.size());
    for (const auto& v : b.vertices) b_in_a.push_back(xa.apply_inverse(xb.apply(v)));
    for (const auto& v : a.vertices) a_in_b.push_back(xb.apply_inverse(xa.apply(v)));
    FaceQuery fa = query_faces(a, b_in_a);
    if (fa.separation > margin) return false;
    FaceQuery fb = query_faces(b, a_in_b);
    if (fb.separation > margin) return false;
    for (const auto& d : b.edge_directions) b_dirs.push_back(xa.rotate_inverse(xb.rotate(d)));
    EdgeQuery eq = query_edges(a, b_in_a, b_dirs);
    if (eq.separation > margin) return false;
    constexpr Real rel_tolerance = 0.95_r, abs_tolerance = 0.0025_r; // m
    const Real face_sep = std::max(fa.separation, fb.separation);

    if (eq.dir_a >= 0 && eq.separation > rel_tolerance * face_sep + abs_tolerance) {
        const RVec3& L = eq.axis; 
        int ea = support_edge(a, a.edge_directions[static_cast<std::size_t>(eq.dir_a)], L, 1.0_r);
        RVec3 dir_b_local = xb.rotate_inverse(xa.rotate(b_dirs[static_cast<std::size_t>(eq.dir_b)]));
        RVec3 L_in_b = xb.rotate_inverse(xa.rotate(L));
        int eb = support_edge(b, dir_b_local, L_in_b, -1.0_r);

        if (ea >= 0 && eb >= 0) {
            RVec3 a0 = xa.apply(a.vertices[a.edges[ea][0]]), a1 = xa.apply(a.vertices[a.edges[ea][1]]);
            RVec3 b0 = xb.apply(b.vertices[b.edges[eb][0]]), b1 = xb.apply(b.vertices[b.edges[eb][1]]);
            SegmentPair sp = closest_segment_points(a0, a1, b0, b1);
            RVec3 n = xa.rotate(L);
            m.normal = n;
            m.point_count = 1;
            m.points[0].position = (sp.closest1 + sp.closest2) * 0.5_r;
            m.points[0].separation = vec3::dot(sp.closest2 - sp.closest1, n);
            return true;
        }
    }

    std::vector<Candidate> c;
    RVec3 n;

    if (fb.separation > 0.98_r * fa.separation + 0.001_r) {
        face_contact(b, xb, fb.index, a, xa, c, n, margin);
        n = n * -1.0_r; 
    } else {
        face_contact(a, xa, fa.index, b, xb, c, n, margin);
    }

    if (c.empty()) return false;
    m.normal = n;
    reduce_contacts(m, c, n);
    return m.point_count > 0;
}

} // namespace narrowphase3d
} // namespace physics
} // namespace fizmo

namespace fizmo {
namespace physics {

bool collide_shapes(
    const Shape3D& sa, const Transform3D& xa,
    const Shape3D& sb, const Transform3D& xb,
    ContactManifold3D& manifold, Real margin 
) {
    using ST = ShapeType3D;
    using namespace narrowphase3d;
    auto type_pair = [](ST a, ST b) -> int { return (static_cast<int>(a) << 8) | static_cast<int>(b); };
    bool flip = false;
    const Shape3D* a = &sa;
    const Shape3D* b = &sb;
    const Transform3D* ta = &xa;
    const Transform3D* tb = &xb;

    if (static_cast<int>(a->type) > static_cast<int>(b->type)) {
        std::swap(a, b);
        std::swap(ta, tb);
        flip = true;
    }

    bool hit = false;

    switch (type_pair(a->type, b->type)) {
        case 0x0000: hit = collide_sphere_sphere(a->sphere, *ta, b->sphere, *tb, manifold, margin); break;
        case 0x0001: hit = collide_sphere_capsule(a->sphere, *ta, b->capsule, *tb, manifold, margin); break;
        case 0x0002: hit = b->hull && collide_sphere_hull(a->sphere, *ta, *b->hull, *tb, manifold, margin); break;
        case 0x0003: hit = collide_sphere_plane(a->sphere, *ta, b->plane, *tb, manifold, margin); break;
        case 0x0101: hit = collide_capsule_capsule(a->capsule, *ta, b->capsule, *tb, manifold, margin); break;
        case 0x0102: hit = b->hull && collide_capsule_hull(a->capsule, *ta, *b->hull, *tb, manifold, margin); break;
        case 0x0103: hit = collide_capsule_plane(a->capsule, *ta, b->plane, *tb, manifold, margin); break;
        case 0x0202: hit = a->hull && b->hull && collide_hull_hull(*a->hull, *ta, *b->hull, *tb, manifold, margin); break;
        case 0x0203: hit = a->hull && collide_hull_plane(*a->hull, *ta, b->plane, *tb, manifold, margin); break;
        case 0x0303: return false; // plane-plane (no response)
        default: {
            if (a->type == ST::Heightfield || a->type == ST::Mesh || b->type == ST::Heightfield || b->type == ST::Mesh) {
                std::vector<ContactManifold3D> out;
                if (!collide_shapes_multi(*a, *ta, *b, *tb, margin, out)) return false;
                std::size_t best = 0;
                for (std::size_t i = 1; i < out.size(); ++i) {
                    if (out[i].points[0].separation < out[best].points[0].separation) best = i;
                }
                manifold.normal = out[best].normal;
                manifold.point_count = out[best].point_count;
                for (int p = 0; p < out[best].point_count; ++p) manifold.points[p] = out[best].points[p];
                manifold.feature = out[best].feature;
                hit = true;
                break;
            }
            if (a->type == ST::Plane || b->type == ST::Plane) {
                const Shape3D& conv = a->type == ST::Plane ? *b : *a;
                const Transform3D& xc = a->type == ST::Plane ? *tb : *ta;
                const Shape3D& plane = a->type == ST::Plane ? *a : *b;
                const Transform3D& xp = a->type == ST::Plane ? *ta : *tb;
                ConvexProxy3D proxy;
                if (!make_convex_proxy(conv, xc, proxy)) return false;
                hit = collide_convex_plane(proxy, plane.plane, xp, manifold, margin);
                if (hit && a->type == ST::Plane) manifold.normal = manifold.normal * -1.0_r;
                break;
            }
            ConvexProxy3D pa, pb;
            if (!make_convex_proxy(*a, *ta, pa) || !make_convex_proxy(*b, *tb, pb)) return false;
            hit = collide_convex(pa, pb, manifold, margin);
            break;
        }
    }

    if (hit && flip) manifold.normal = manifold.normal * -1.0_r;
    return hit;
}

} // namespace physics
} // namespace fizmo
