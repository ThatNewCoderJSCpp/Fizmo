#define ALL_FIZMO
#include <fizmo/includes.hpp>
#include "simulation_3d.hpp"

namespace fizmo {
namespace physics {
namespace narrowphase3d {

vector3d closest_point_on_segment(const vector3d& p, const vector3d& a, const vector3d& b) {
    vector3d ab = b - a;
    double t = vec3::length_squared(ab);
    if (t <= constants::middle_epsilon()) return a;
    t = std::clamp(vec3::dot(p - a, ab) / t, 0.0, 1.0);
    return a + ab * t;
}

SegmentPair closest_segment_points(const vector3d& p1, const vector3d& q1, const vector3d& p2, const vector3d& q2) {
    vector3d d1 = q1 - p1, d2 = q2 - p2, r = p1 - p2;
    double a = vec3::dot(d1, d1), e = vec3::dot(d2, d2), f = vec3::dot(d2, r);
    double s, t;
    const double eps = constants::middle_epsilon();

    if (a <= eps && e <= eps) {
        s = t = 0.0;
    } else if (a <= eps) {
        s = 0.0; t = std::clamp(f / e, 0.0, 1.0);
    } else {
        double c = vec3::dot(d1, r);

        if (e <= eps) {
            t = 0.0; s = std::clamp(-c / a, 0.0, 1.0);
        } else {
            double b = vec3::dot(d1, d2);
            double denom = a * e - b * b;
            s = (denom > eps) ? std::clamp((b * f - c * e) / denom, 0.0, 1.0) : 0.0;
            t = (b * s + f) / e;
            if (t < 0.0) { t = 0.0; s = std::clamp(-c / a, 0.0, 1.0); }
            else if (t > 1.0) { t = 1.0; s = std::clamp((b - c) / a, 0.0, 1.0); }
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

void reduce_contacts(ContactManifold3D& m, const std::vector<Candidate>& c, const vector3d& normal) {
    m.point_count = 0;
    if (c.empty()) return;

    if (c.size() <= static_cast<std::size_t>(ContactManifold3D::MAX_POINTS)) {
        for (const auto& k : c) { m.points[m.point_count].position = k.position; m.points[m.point_count].separation = k.separation; ++m.point_count; }
        return;
    }

    std::size_t i0 = 0;
    for (std::size_t i = 1; i < c.size(); ++i) if (c[i].separation < c[i0].separation) i0 = i;
    std::size_t i1 = i0;
    double best = -1.0;

    for (std::size_t i = 0; i < c.size(); ++i) {
        double d = vec3::length_squared(c[i].position - c[i0].position);
        if (d > best) { best = d; i1 = i; }
    }

    std::size_t i2 = c.size(), i3 = c.size();
    double max_area = 0.0, min_area = 0.0;
    const vector3d e = c[i1].position - c[i0].position;

    for (std::size_t i = 0; i < c.size(); ++i) {
        double area = vec3::dot(vec3::cross(e, c[i].position - c[i0].position), normal);
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

bool point_in_face(const HullShape& h, const HullFace& f, const vector3d& q) {
    const std::size_t n = f.indices.size();

    for (std::size_t i = 0; i < n; ++i) {
        const vector3d& a = h.vertices[f.indices[i]];
        const vector3d& b = h.vertices[f.indices[(i + 1) % n]];
        if (vec3::dot(vec3::cross(b - a, q - a), f.normal) < 0.0) return false;
    }

    return true;
}

vector3d closest_point_on_face(const HullShape& h, const HullFace& f, const vector3d& p) {
    vector3d q = p - f.normal * (vec3::dot(f.normal, p) - f.offset);
    if (point_in_face(h, f, q)) return q;
    const std::size_t n = f.indices.size();
    double best = std::numeric_limits<double>::max();
    vector3d out = q;

    for (std::size_t i = 0; i < n; ++i) {
        vector3d c = closest_point_on_segment(p, h.vertices[f.indices[i]], h.vertices[f.indices[(i + 1) % n]]);
        double d = vec3::length_squared(p - c);
        if (d < best) { best = d; out = c; }
    }

    return out;
}

PointHullResult closest_point_on_hull(const HullShape& h, const vector3d& p) {
    PointHullResult r;
    double max_sep = -std::numeric_limits<double>::max();
    std::size_t face = 0;

    for (std::size_t i = 0; i < h.faces.size(); ++i) {
        double s = vec3::dot(h.faces[i].normal, p) - h.faces[i].offset;
        if (s > max_sep) { max_sep = s; face = i; }
    }

    if (max_sep <= 0.0) {
        r.inside = true;
        r.normal = h.faces[face].normal;
        r.point = p - r.normal * max_sep;
        r.distance = max_sep;
        return r;
    }

    double best = std::numeric_limits<double>::max();

    for (const auto& f : h.faces) {
        if (vec3::dot(f.normal, p) - f.offset <= 0.0) continue;
        vector3d c = closest_point_on_face(h, f, p);
        double d = vec3::length_squared(p - c);
        if (d < best) { best = d; r.point = c; }
    }

    r.distance = std::sqrt(best);
    r.normal = (r.distance > constants::middle_epsilon()) ? (p - r.point) * (1.0 / r.distance) : h.faces[face].normal;
    return r;
}

bool segment_intersects_hull(const HullShape& h, const vector3d& p1, const vector3d& p2) {
    double tmin = 0.0, tmax = 1.0;
    const vector3d d = p2 - p1;

    for (const auto& f : h.faces) {
        double denom = vec3::dot(f.normal, d);
        double dist  = vec3::dot(f.normal, p1) - f.offset;

        if (std::abs(denom) <= 1e-15) {
            if (dist > 0.0) return false;
            continue;
        }

        double t = -dist / denom;
        if (denom < 0.0) tmin = std::max(tmin, t);
        else             tmax = std::min(tmax, t);
        if (tmin > tmax) return false;
    }

    return true;
}

bool clip_segment_to_face(const HullShape& h, const HullFace& f, vector3d& a, vector3d& b) {
    double tmin = 0.0, tmax = 1.0;
    const vector3d d = b - a;
    const std::size_t n = f.indices.size();

    for (std::size_t i = 0; i < n; ++i) {
        const vector3d& v0 = h.vertices[f.indices[i]];
        const vector3d& v1 = h.vertices[f.indices[(i + 1) % n]];
        vector3d side = vec3::cross(v1 - v0, f.normal); 
        double dist  = vec3::dot(side, a - v0);
        double denom = vec3::dot(side, d);

        if (std::abs(denom) <= 1e-15) {
            if (dist > 0.0) return false;
            continue;
        }

        double t = -dist / denom;
        if (denom > 0.0) tmax = std::min(tmax, t);
        else             tmin = std::max(tmin, t);
        if (tmin > tmax) return false;
    }

    vector3d a0 = a;
    a = a0 + d * tmin;
    b = a0 + d * tmax;
    return true;
}

int support_edge(const HullShape& h, const vector3d& dir, const vector3d& axis, double sign) {
    int best = -1;
    double best_d = -std::numeric_limits<double>::max();

    for (std::size_t i = 0; i < h.edges.size(); ++i) {
        const vector3d& a = h.vertices[h.edges[i][0]];
        const vector3d& b = h.vertices[h.edges[i][1]];
        vector3d e = vec3::normalize(b - a);
        if (std::abs(vec3::dot(e, dir)) < 1.0 - 1e-6) continue;
        double d = sign * vec3::dot(axis, (a + b) * 0.5);
        if (d > best_d) { best_d = d; best = static_cast<int>(i); }
    }

    return best;
}

void project_hull(const HullShape& h, const vector3d& axis, double& lo, double& hi) {
    lo = std::numeric_limits<double>::max();
    hi = -lo;
    for (const auto& v : h.vertices) { double d = vec3::dot(axis, v); lo = std::min(lo, d); hi = std::max(hi, d); }
}

void project_points(const std::vector<vector3d>& pts, const vector3d& axis, double& lo, double& hi) {
    lo = std::numeric_limits<double>::max();
    hi = -lo;
    for (const auto& v : pts) { double d = vec3::dot(axis, v); lo = std::min(lo, d); hi = std::max(hi, d); }
}

bool collide_sphere_sphere(
    const SphereShape& sa, const Transform3D& xa,
    const SphereShape& sb, const Transform3D& xb,
    ContactManifold3D& m, double margin 
) {
    vector3d cA = xa.apply(sa.center);
    vector3d cB = xb.apply(sb.center);
    vector3d d = cB - cA;
    double dist_sq = vec3::length_squared(d);
    double r_sum = sa.radius + sb.radius;
    if (dist_sq > (r_sum + margin) * (r_sum + margin)) return false;
    double dist = std::sqrt(dist_sq);
    m.normal = (dist >= constants::middle_epsilon()) ? d * (1.0 / dist) : vector3d{0.0, 1.0, 0.0};
    double pen = r_sum - dist;
    m.point_count = 1;
    m.points[0].position = cA + m.normal * (sa.radius - pen * 0.5);
    m.points[0].separation = -pen;
    return true;
}

bool collide_sphere_point(const vector3d& c, double ra, const vector3d& p, double rb, ContactManifold3D& m, double margin) {
    SphereShape a{ c, ra }, b{ p, rb };
    return collide_sphere_sphere(a, Transform3D{}, b, Transform3D{}, m, margin);
}

bool collide_sphere_capsule(
    const SphereShape& s, const Transform3D& xs,
    const CapsuleShape3D& k, const Transform3D& xk,
    ContactManifold3D& m, double margin 
) {
    vector3d c = xs.apply(s.center);
    vector3d p = closest_point_on_segment(c, xk.apply(k.point1), xk.apply(k.point2));
    return collide_sphere_point(c, s.radius, p, k.radius, m, margin);
}

bool collide_sphere_hull(
    const SphereShape& s, const Transform3D& xs,
    const HullShape& h, const Transform3D& xh,
    ContactManifold3D& m, double margin 
) {
    if (!h.valid()) return false;
    const vector3d c = xh.apply_inverse(xs.apply(s.center)); 
    PointHullResult q = closest_point_on_hull(h, c);
    if (q.distance > s.radius + margin) return false;
    const vector3d n = xh.rotate(q.normal); 
    const vector3d cw = xs.apply(s.center);
    m.normal = n * -1.0;
    m.point_count = 1;
    m.points[0].position = cw - n * ((s.radius + q.distance) * 0.5);
    m.points[0].separation = q.distance - s.radius;
    return true;
}

bool collide_sphere_plane(
    const SphereShape& s, const Transform3D& xs,
    const PlaneShape& p, const Transform3D& xp,
    ContactManifold3D& m, double margin 
) {
    vector3d n; double d;
    plane_in_world(p, xp, n, d);
    vector3d c = xs.apply(s.center);
    double dist = vec3::dot(n, c) - d;
    if (dist > s.radius + margin) return false;
    m.normal = n * -1.0;
    m.point_count = 1;
    m.points[0].position = c - n * ((s.radius + dist) * 0.5);
    m.points[0].separation = dist - s.radius;
    return true;
}

bool collide_capsule_capsule(
    const CapsuleShape3D& ka, const Transform3D& xa,
    const CapsuleShape3D& kb, const Transform3D& xb,
    ContactManifold3D& m, double margin 
) {
    const vector3d p1 = xa.apply(ka.point1), q1 = xa.apply(ka.point2);
    const vector3d p2 = xb.apply(kb.point1), q2 = xb.apply(kb.point2);
    const double R = ka.radius + kb.radius;
    SegmentPair sp = closest_segment_points(p1, q1, p2, q2);
    if (sp.distance_squared > (R + margin) * (R + margin)) return false;
    const double dist = std::sqrt(sp.distance_squared);
    const vector3d d1 = q1 - p1, d2 = q2 - p2;
    vector3d n;

    if (dist > constants::middle_epsilon()) {
        n = (sp.closest2 - sp.closest1) * (1.0 / dist);
    } else {
        vector3d axis = vec3::normalize(d1), t1, t2;
        vec3::orthonormal_basis(axis, t1, t2);
        n = t1;
    }

    m.normal = n;
    m.point_count = 0;
    const double l1 = vec3::length_squared(d1), l2 = vec3::length_squared(d2);

    if (l1 > constants::middle_epsilon() && l2 > constants::middle_epsilon() &&
        vec3::length_squared(vec3::cross(d1, d2)) < 0.0025 * l1 * l2) {
        double ta = vec3::dot(p2 - p1, d1) / l1, tb = vec3::dot(q2 - p1, d1) / l1;
        double lo = std::max(0.0, std::min(ta, tb)), hi = std::min(1.0, std::max(ta, tb));

        if (hi - lo > 1e-6) {
            for (double t : { lo, hi }) {
                vector3d a = p1 + d1 * t;
                vector3d b = closest_point_on_segment(a, p2, q2);
                double sep = vec3::dot(b - a, n) - R;
                if (sep > margin) continue;
                m.points[m.point_count].position = ((a + n * ka.radius) + (b - n * kb.radius)) * 0.5;
                m.points[m.point_count].separation = sep;
                ++m.point_count;
            }

            if (m.point_count > 0) return true;
        }
    }

    m.point_count = 1;
    m.points[0].position = ((sp.closest1 + n * ka.radius) + (sp.closest2 - n * kb.radius)) * 0.5;
    m.points[0].separation = dist - R;
    return true;
}

bool collide_capsule_plane(
    const CapsuleShape3D& k, const Transform3D& xk,
    const PlaneShape& p, const Transform3D& xp,
    ContactManifold3D& m, double margin 
) {
    vector3d n; double d;
    plane_in_world(p, xp, n, d);
    m.point_count = 0;

    for (const vector3d& e : { xk.apply(k.point1), xk.apply(k.point2) }) {
        double dist = vec3::dot(n, e) - d;
        if (dist > k.radius + margin) continue;
        m.points[m.point_count].position = e - n * ((k.radius + dist) * 0.5);
        m.points[m.point_count].separation = dist - k.radius;
        ++m.point_count;
    }

    m.normal = n * -1.0;
    return m.point_count > 0;
}

bool collide_hull_plane(
    const HullShape& h, const Transform3D& xh,
    const PlaneShape& p, const Transform3D& xp,
    ContactManifold3D& m, double margin 
) {
    if (!h.valid()) return false;
    vector3d n; double d;
    plane_in_world(p, xp, n, d);
    std::vector<Candidate> c;

    for (const auto& v : h.vertices) {
        vector3d w = xh.apply(v);
        double s = vec3::dot(n, w) - d;
        if (s <= margin) c.push_back({ w - n * (s * 0.5), s });
    }

    if (c.empty()) return false;
    m.normal = n * -1.0;
    reduce_contacts(m, c, m.normal);
    return m.point_count > 0;
}

bool collide_capsule_hull(
    const CapsuleShape3D& k, const Transform3D& xk,
    const HullShape& h, const Transform3D& xh,
    ContactManifold3D& m, double margin 
) {
    if (!h.valid()) return false;
    const vector3d p1 = xh.apply_inverse(xk.apply(k.point1)); 
    const vector3d p2 = xh.apply_inverse(xk.apply(k.point2));
    const double r = k.radius;
    vector3d n_local{};           
    std::vector<Candidate> local; 

    auto add_face_points = [&](const HullFace& f) {
        vector3d a = p1, b = p2;
        if (!clip_segment_to_face(h, f, a, b)) return;

        for (const vector3d& q : { a, b }) {
            double s = vec3::dot(f.normal, q) - f.offset;
            if (s - r > margin) continue;
            local.push_back({ q - f.normal * ((r + s) * 0.5), s - r });
        }

        if (local.size() == 2 && vec3::length_squared(local[0].position - local[1].position) <= 1e-18) local.pop_back();
    };

    if (!segment_intersects_hull(h, p1, p2)) {
        double best = std::numeric_limits<double>::max();
        vector3d on_seg{}, on_hull{};

        for (const vector3d& e : { p1, p2 }) {
            PointHullResult q = closest_point_on_hull(h, e);
            double d = vec3::length_squared(e - q.point);
            if (d < best) { best = d; on_seg = e; on_hull = q.point; }
        }

        for (const auto& e : h.edges) {
            SegmentPair sp = closest_segment_points(p1, p2, h.vertices[e[0]], h.vertices[e[1]]);
            if (sp.distance_squared < best) { best = sp.distance_squared; on_seg = sp.closest1; on_hull = sp.closest2; }
        }

        if (best > (r + margin) * (r + margin)) return false;
        const double dist = std::sqrt(best);

        if (dist > constants::middle_epsilon()) {
            n_local = (on_seg - on_hull) * (1.0 / dist);
            
            for (const auto& f : h.faces) {
                if (vec3::dot(f.normal, n_local) > 1.0 - 1e-6) { n_local = f.normal; add_face_points(f); break; }
            }

            if (local.empty()) local.push_back({ (on_hull + (on_seg - n_local * r)) * 0.5, dist - r });
        }
    }

    if (local.empty()) {
        const vector3d d = p2 - p1;
        double best = -std::numeric_limits<double>::max();
        int best_face = -1;
        vector3d best_axis{};
        int best_dir = -1;

        for (std::size_t i = 0; i < h.faces.size(); ++i) {
            const auto& f = h.faces[i];
            double s = std::min(vec3::dot(f.normal, p1), vec3::dot(f.normal, p2)) - f.offset - r;
            if (s > best) { best = s; best_face = static_cast<int>(i); best_axis = f.normal; }
        }

        const double face_best = best;
        const double dl = vec3::length(d);

        for (std::size_t i = 0; i < h.edge_directions.size() && dl > constants::middle_epsilon(); ++i) {
            vector3d L = vec3::cross(d, h.edge_directions[i]);
            double len = vec3::length(L);
            if (len < 1e-6 * dl) continue;
            L = L * (1.0 / len);
            double hlo, hhi;
            project_hull(h, L, hlo, hhi);
            double slo = std::min(vec3::dot(L, p1), vec3::dot(L, p2)) - r;
            double shi = std::max(vec3::dot(L, p1), vec3::dot(L, p2)) + r;
            double s_pos = slo - hhi, s_neg = hlo - shi;
            double s = std::max(s_pos, s_neg);

            if (s > best && s > 0.95 * face_best + 0.0025) {
                best = s;
                best_face = -1;
                best_dir = static_cast<int>(i);
                best_axis = (s_pos >= s_neg) ? L : L * -1.0;
            }
        }

        if (best > margin) return false;
        n_local = best_axis;

        if (best_face >= 0) {
            const HullFace& f = h.faces[static_cast<std::size_t>(best_face)];
            add_face_points(f);

            if (local.empty()) {
                const vector3d& q = (vec3::dot(f.normal, p1) < vec3::dot(f.normal, p2)) ? p1 : p2;
                double s = vec3::dot(f.normal, q) - f.offset;
                local.push_back({ q - f.normal * ((r + s) * 0.5), s - r });
            }
        } else {
            int e = support_edge(h, h.edge_directions[static_cast<std::size_t>(best_dir)], n_local, 1.0);
            if (e < 0) return false;
            SegmentPair sp = closest_segment_points(p1, p2, h.vertices[h.edges[e][0]], h.vertices[h.edges[e][1]]);
            double s = vec3::dot(sp.closest1 - sp.closest2, n_local) - r;
            local.push_back({ (sp.closest2 + (sp.closest1 - n_local * r)) * 0.5, s });
        }
    }

    const vector3d n_world = xh.rotate(n_local);
    m.normal = n_world * -1.0; 
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

FaceQuery query_faces(const HullShape& a, const std::vector<vector3d>& other) {
    FaceQuery q;

    for (std::size_t i = 0; i < a.faces.size(); ++i) {
        const auto& f = a.faces[i];
        double s = std::numeric_limits<double>::max();
        for (const auto& v : other) s = std::min(s, vec3::dot(f.normal, v) - f.offset);
        if (s > q.separation) { q.separation = s; q.index = static_cast<int>(i); }
    }

    return q;
}

EdgeQuery query_edges(const HullShape& a, const std::vector<vector3d>& b_vertices, const std::vector<vector3d>& b_dirs) {
    EdgeQuery q;

    for (std::size_t i = 0; i < a.edge_directions.size(); ++i) {
        for (std::size_t j = 0; j < b_dirs.size(); ++j) {
            vector3d L = vec3::cross(a.edge_directions[i], b_dirs[j]);
            double len = vec3::length(L);
            if (len < 1e-6) continue;
            L = L * (1.0 / len);
            double alo, ahi, blo, bhi;
            project_hull(a, L, alo, ahi);
            project_points(b_vertices, L, blo, bhi);
            double s_pos = blo - ahi, s_neg = alo - bhi;
            double s = std::max(s_pos, s_neg);

            if (s > q.separation) {
                q.separation = s;
                q.dir_a = static_cast<int>(i);
                q.dir_b = static_cast<int>(j);
                q.axis = (s_pos >= s_neg) ? L : L * -1.0;
            }
        }
    }

    return q;
}

void clip_polygon(std::vector<vector3d>& poly, const vector3d& n, double d, std::vector<vector3d>& scratch) {
    scratch.clear();
    const std::size_t count = poly.size();

    for (std::size_t i = 0; i < count; ++i) {
        const vector3d& a = poly[i];
        const vector3d& b = poly[(i + 1) % count];
        double da = vec3::dot(n, a) - d;
        double db = vec3::dot(n, b) - d;
        if (da <= 0.0) scratch.push_back(a);
        if ((da < 0.0 && db > 0.0) || (da > 0.0 && db < 0.0)) scratch.push_back(a + (b - a) * (da / (da - db)));
    }

    poly.swap(scratch);
}

void face_contact(
    const HullShape& r, const Transform3D& xr, int rf,
    const HullShape& inc, const Transform3D& xi,
    std::vector<Candidate>& out, vector3d& normal, double margin
) {
    const HullFace& ref = r.faces[static_cast<std::size_t>(rf)];
    normal = xr.rotate(ref.normal);
    std::vector<vector3d> ref_poly;
    for (auto idx : ref.indices) ref_poly.push_back(xr.apply(r.vertices[idx]));
    const double offset = vec3::dot(normal, ref_poly[0]);
    const vector3d n_in_i = xi.rotate_inverse(normal);
    std::size_t inc_face = 0;
    double min_dot = std::numeric_limits<double>::max();

    for (std::size_t f = 0; f < inc.faces.size(); ++f) {
        double d = vec3::dot(inc.faces[f].normal, n_in_i);
        if (d < min_dot) { min_dot = d; inc_face = f; }
    }

    std::vector<vector3d> poly, scratch;
    for (auto idx : inc.faces[inc_face].indices) poly.push_back(xi.apply(inc.vertices[idx]));

    for (std::size_t k = 0; k < ref_poly.size() && !poly.empty(); ++k) {
        const vector3d& a = ref_poly[k];
        const vector3d& b = ref_poly[(k + 1) % ref_poly.size()];
        vector3d side = vec3::cross(b - a, normal);
        clip_polygon(poly, side, vec3::dot(side, a), scratch);
    }

    for (const auto& p : poly) {
        double s = vec3::dot(normal, p) - offset;
        if (s <= margin) out.push_back({ p - normal * (s * 0.5), s });
    }

    if (out.empty()) {
        double best = std::numeric_limits<double>::max();
        vector3d deepest{};

        for (const auto& v : inc.vertices) {
            vector3d w = xi.apply(v);
            double s = vec3::dot(normal, w) - offset;
            if (s < best) { best = s; deepest = w; }
        }

        if (best <= margin) out.push_back({ deepest - normal * (best * 0.5), best });
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
    ContactManifold3D& m, double margin 
) {
    using namespace detail_sat3d;
    if (!a.valid() || !b.valid()) return false;
    std::vector<vector3d> b_in_a, a_in_b, b_dirs;
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
    constexpr double rel_tolerance = 0.95, abs_tolerance = 0.0025; // m
    const double face_sep = std::max(fa.separation, fb.separation);

    if (eq.dir_a >= 0 && eq.separation > rel_tolerance * face_sep + abs_tolerance) {
        const vector3d& L = eq.axis; 
        int ea = support_edge(a, a.edge_directions[static_cast<std::size_t>(eq.dir_a)], L, 1.0);
        vector3d dir_b_local = xb.rotate_inverse(xa.rotate(b_dirs[static_cast<std::size_t>(eq.dir_b)]));
        vector3d L_in_b = xb.rotate_inverse(xa.rotate(L));
        int eb = support_edge(b, dir_b_local, L_in_b, -1.0);

        if (ea >= 0 && eb >= 0) {
            vector3d a0 = xa.apply(a.vertices[a.edges[ea][0]]), a1 = xa.apply(a.vertices[a.edges[ea][1]]);
            vector3d b0 = xb.apply(b.vertices[b.edges[eb][0]]), b1 = xb.apply(b.vertices[b.edges[eb][1]]);
            SegmentPair sp = closest_segment_points(a0, a1, b0, b1);
            vector3d n = xa.rotate(L);
            m.normal = n;
            m.point_count = 1;
            m.points[0].position = (sp.closest1 + sp.closest2) * 0.5;
            m.points[0].separation = vec3::dot(sp.closest2 - sp.closest1, n);
            return true;
        }
    }

    std::vector<Candidate> c;
    vector3d n;

    if (fb.separation > 0.98 * fa.separation + 0.001) {
        face_contact(b, xb, fb.index, a, xa, c, n, margin);
        n = n * -1.0; 
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
    ContactManifold3D& manifold, double margin 
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
        default: return false;
    }

    if (hit && flip) manifold.normal = manifold.normal * -1.0;
    return hit;
}

auto Simulation3D::remove_body(RigidBody3D& body) -> void {
        if (m_locked) { m_pending_removals.push_back(&body); return; }
        remove_body_now(&body);
        flush_pending_removals();
    }

auto Simulation3D::step(double dt) -> void {
        if (dt <= 0.0) return;
        m_locked = true;

        for (auto& bp : m_bodies) {
            RigidBody3D& b = *bp;
            if (!is_enabled(b) || !b.is_awake()) continue;
            b.ensure_mass_uptodate();
            b.integrate_forces(dt, m_cfg.gravity);
            if (m_cfg.gyroscopic) b.integrate_gyroscopic(dt);
        }

        broadphase();
        narrowphase();
        build_islands_and_wake();
        build_velocity_constraints(dt);
        if (m_cfg.warm_starting) warm_start();
        for (int i = 0; i < m_cfg.velocity_iters; ++i) { solve_velocity_constraints((i & 1) != 0); }
        store_impulses();

        for (auto& bp : m_bodies) {
            RigidBody3D& b = *bp;
            if (!is_enabled(b) || !b.is_awake()) continue;
            b.integrate_velocities(dt);
        }

        for (int i = 0; i < m_cfg.position_iters; ++i) { if (solve_position_constraints((i & 1) != 0)) break; }

        for (auto& bp : m_bodies) {
            RigidBody3D& b = *bp;
            if (!is_enabled(b)) continue;
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

auto Simulation3D::query_aabb(const AABB3D& region) -> std::vector<RigidBody3D*> {
        synchronize_all_colliders();
        std::vector<RigidBody3D*> ptrs;
        ptrs.reserve(m_bodies.size());
        for (auto& b : m_bodies) ptrs.push_back(b.get());
        std::vector<AABB3D> aabbs;
        aabbs.reserve(m_bodies.size());
        for (auto& b : m_bodies) aabbs.push_back(b->compute_body_aabb());
        m_tree.rebuild(world_bounds_of(m_cfg), ptrs.data(), aabbs.data(), ptrs.size());
        return m_tree.query(region);
    }

auto Simulation3D::point_query(const vector3d& point) -> RigidBody3D* {
        const vector3d e{0.001, 0.001, 0.001};
        auto results = query_aabb({ point - e, point + e });

        for (auto* b : results) {
            if (b->compute_body_aabb().contains(point)) return b;
        }

        return nullptr;
    }

auto Simulation3D::is_active(const RigidBody3D& b) noexcept -> bool {
        if (!b.is_awake()) return false;
        if (b.inv_mass() > 0.0) return true;
        return !vec3::is_zero(b.linear_velocity()) || !vec3::is_zero(b.angular_velocity());
    }

auto Simulation3D::responds(const RigidBody3D& a, const RigidBody3D& b) noexcept -> bool {
        return a.has(PhysicsFlags::CollisionResponse) && b.has(PhysicsFlags::CollisionResponse) &&
              !a.has(PhysicsFlags::Intangible) && !b.has(PhysicsFlags::Intangible);
    }

auto Simulation3D::wants_events(const RigidBody3D& a, const RigidBody3D& b, bool sensor) noexcept -> bool {
        if (a.has(PhysicsFlags::ContactEvents) || b.has(PhysicsFlags::ContactEvents)) return true;
        return sensor && (a.has(PhysicsFlags::SensorEvents) || b.has(PhysicsFlags::SensorEvents));
    }

auto Simulation3D::apply_impulse(RigidBody3D& a, RigidBody3D& b, const vector3d& rA, const vector3d& rB, const vector3d& P) -> void {
        a.set_linear_velocity(a.linear_velocity() - P * a.inv_mass());
        a.set_angular_velocity(a.angular_velocity() - a.apply_inv_inertia(vec3::cross(rA, P)));
        b.set_linear_velocity(b.linear_velocity() + P * b.inv_mass());
        b.set_angular_velocity(b.angular_velocity() + b.apply_inv_inertia(vec3::cross(rB, P)));
    }

auto Simulation3D::effective_inv_mass(const RigidBody3D& a, const RigidBody3D& b, const vector3d& rA, const vector3d& rB, const vector3d& u) -> double {
        vector3d ra_u = vec3::cross(rA, u), rb_u = vec3::cross(rB, u);
        return a.inv_mass() + b.inv_mass()
             + vec3::dot(ra_u, a.apply_inv_inertia(ra_u))
             + vec3::dot(rb_u, b.apply_inv_inertia(rb_u));
    }

auto Simulation3D::broadphase() -> void {
        m_pairs.clear();
        synchronize_all_colliders();
        m_body_ptrs.clear();
        m_body_aabbs.clear();
        m_body_ptrs.reserve(m_bodies.size());
        m_body_aabbs.reserve(m_bodies.size());

        for (auto& bp : m_bodies) {
            RigidBody3D& b = *bp;
            if (!is_enabled(b)) continue;
            if (!has_flag(b.flags(), PhysicsFlags::BroadphaseActive) && !has_flag(b.flags(), PhysicsFlags::CollisionResponse)) continue;
            b.ensure_mass_uptodate();
            m_body_ptrs.push_back(&b);
            m_body_aabbs.push_back(b.compute_body_aabb().fatten(m_cfg.broadphase_margin));
        }

        if (m_body_ptrs.size() < 2) return;
        m_tree.rebuild(world_bounds_of(m_cfg), m_body_ptrs.data(), m_body_aabbs.data(), m_body_ptrs.size());

        m_tree.find_pairs([this](RigidBody3D* a, RigidBody3D* b) {
            if (!a->can_collide_with(*b)) return;
            if (!is_active(*a) && !is_active(*b)) return;   
            if (!is_dynamic(*a) && !is_dynamic(*b)) return; 
            if (std::less<const RigidBody3D*>()(b, a)) std::swap(a, b);
            m_pairs.push_back({a, b});
        });
    }

auto Simulation3D::narrowphase() -> void {
        m_manifolds.clear();
        m_manifolds.reserve(m_pairs.size());
        m_touching.clear();
        for (auto& [a, b] : m_pairs) { generate_contacts(a, b); }
    }

auto Simulation3D::generate_contacts(RigidBody3D* a, RigidBody3D* b) -> void {
        const auto& colA = a->colliders();
        const auto& colB = b->colliders();

        for (std::size_t i = 0; i < colA.size(); ++i) {
            for (std::size_t j = 0; j < colB.size(); ++j) {
                const Collider3D& ca = colA[i];
                const Collider3D& cb = colB[j];
                if (!ca.world_aabb.fatten(m_cfg.speculative_distance).overlaps(cb.world_aabb)) continue;
                if (!ca.filter.should_collide(cb.filter)) continue;
                Transform3D wa = a->transform().compose(ca.local_offset);
                Transform3D wb = b->transform().compose(cb.local_offset);
                ContactManifold3D mf;
                mf.body_a = a;
                mf.body_b = b;
                mf.collider_a = i;
                mf.collider_b = j;
                const bool sensor = ca.is_sensor || cb.is_sensor || !responds(*a, *b);
                if (!collide_shapes(ca.shape, wa, cb.shape, wb, mf, sensor ? 0.0 : m_cfg.speculative_distance)) continue;
                mf.friction    = std::sqrt(ca.material.friction * cb.material.friction);
                mf.restitution = std::max(ca.material.restitution, cb.material.restitution);
                mf.restitution_threshold = std::min(ca.material.restitution_threshold, cb.material.restitution_threshold);

                for (int p = 0; p < mf.point_count; ++p) {
                    ContactPoint3D& cp = mf.points[p];
                    vector3d half = mf.normal * (cp.separation * 0.5);
                    cp.local_anchor_a = a->transform().apply_inverse(cp.position - half);
                    cp.local_anchor_b = b->transform().apply_inverse(cp.position + half);
                }

                int deepest = 0;
                for (int p = 1; p < mf.point_count; ++p) if (mf.points[p].separation < mf.points[deepest].separation) deepest = p;
                CachedContact& touch = m_touching[key_of(mf)];
                touch.body_a = a;
                touch.body_b = b;
                touch.sensor = sensor;
                touch.events = wants_events(*a, *b, sensor);
                touch.touching = mf.points[deepest].separation <= 0.0;
                touch.normal = mf.normal;
                touch.point  = mf.points[deepest].position;
                if (sensor) continue;
                if (m_pre_solve) { if (!m_pre_solve(*a, *b, mf)) continue; }
                m_manifolds.push_back(mf);
            }
        }
    }

auto Simulation3D::build_islands_and_wake() -> void {
        const std::size_t n = m_bodies.size();
        m_islands.reset(n);
        m_body_index.clear();
        m_carried.clear();
        for (std::size_t i = 0; i < n; ++i) m_body_index[m_bodies[i].get()] = i;

        auto link = [this](RigidBody3D* a, RigidBody3D* b) {
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
            const RigidBody3D& b = *m_bodies[i];
            if (is_enabled(b) && is_dynamic(b) && b.is_awake()) island_awake[m_islands.find(i)] = 1;
        }

        for (std::size_t i = 0; i < n; ++i) {
            RigidBody3D& b = *m_bodies[i];
            if (is_enabled(b) && is_dynamic(b) && !b.is_awake() && island_awake[m_islands.find(i)]) b.wake();
        }
    }

auto Simulation3D::sleep_islands(double dt) -> void {
        const std::size_t n = std::min(m_islands.size(), m_bodies.size());
        std::vector<double> min_timer(n, std::numeric_limits<double>::max());

        for (std::size_t i = 0; i < n; ++i) {
            RigidBody3D& b = *m_bodies[i];
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
            RigidBody3D& b = *m_bodies[i];
            if (!is_enabled(b) || !b.is_awake() || !is_dynamic(b)) continue;
            if (min_timer[m_islands.find(i)] >= RigidBody3D::SLEEP_TIME_THRESHOLD) b.put_to_sleep();
        }
    }

auto Simulation3D::build_velocity_constraints(double dt) -> void {
        m_constraints.clear();
        m_constraints.reserve(m_manifolds.size());
        const double match_sq = m_cfg.warm_start_match_distance * m_cfg.warm_start_match_distance;

        for (std::size_t mi = 0; mi < m_manifolds.size(); ++mi) {
            const auto& mf = m_manifolds[mi];
            RigidBody3D* a = mf.body_a;
            RigidBody3D* b = mf.body_b;
            a->ensure_mass_uptodate();
            b->ensure_mass_uptodate();
            VelocityConstraint3D vc;
            vc.body_a = a;
            vc.body_b = b;
            vc.normal = mf.normal;
            vec3::orthonormal_basis(mf.normal, vc.tangent1, vc.tangent2);
            vc.friction = mf.friction;
            vc.restitution = mf.restitution;
            vc.restitution_threshold = mf.restitution_threshold;
            vc.point_count = mf.point_count;
            vc.manifold_index = mi;
            const vector3d cA = a->world_center();
            const vector3d cB = b->world_center();
            const CachedContact* old = nullptr;

            if (m_cfg.warm_starting) {
                auto it = m_contact_cache.find(key_of(mf));
                if (it != m_contact_cache.end() && !it->second.sensor) old = &it->second;
            }

            for (int p = 0; p < mf.point_count; ++p) {
                auto& vcp = vc.points[p];
                vcp.rA = mf.points[p].position - cA;
                vcp.rB = mf.points[p].position - cB;
                double kn = effective_inv_mass(*a, *b, vcp.rA, vcp.rB, vc.normal);
                double k1 = effective_inv_mass(*a, *b, vcp.rA, vcp.rB, vc.tangent1);
                double k2 = effective_inv_mass(*a, *b, vcp.rA, vcp.rB, vc.tangent2);
                vcp.normal_mass     = (kn > 0.0) ? 1.0 / kn : 0.0;
                vcp.tangent_mass[0] = (k1 > 0.0) ? 1.0 / k1 : 0.0;
                vcp.tangent_mass[1] = (k2 > 0.0) ? 1.0 / k2 : 0.0;
                double vn_rel = vec3::dot(point_velocity(*b, vcp.rB) - point_velocity(*a, vcp.rA), vc.normal);
                const double sep = mf.points[p].separation;
                vcp.velocity_bias = (sep > 0.0) ? -sep / dt : 0.0;
                if (vn_rel < -vc.restitution_threshold && vn_rel * dt + std::max(sep, 0.0) < 0.0) {
                    vcp.velocity_bias = std::max(vcp.velocity_bias, -vc.restitution * vn_rel);
                }

                for (int q = 0; q <= p; ++q) {
                    const auto& o = vc.points[q];
                    vector3d ra_p = vec3::cross(vcp.rA, vc.normal), rb_p = vec3::cross(vcp.rB, vc.normal);
                    vector3d ra_q = vec3::cross(o.rA, vc.normal),   rb_q = vec3::cross(o.rB, vc.normal);
                    double k = a->inv_mass() + b->inv_mass() + vec3::dot(ra_p, a->apply_inv_inertia(ra_q)) + vec3::dot(rb_p, b->apply_inv_inertia(rb_q));
                    vc.normal_block[p][q] = vc.normal_block[q][p] = k;
                }

                if (old) {
                    double best = match_sq;

                    for (int q = 0; q < old->point_count; ++q) {
                        double d = vec3::length_squared(old->local_anchor_a[q] - mf.points[p].local_anchor_a);

                        if (d <= best) {
                            best = d;
                            vcp.normal_impulse     = old->normal_impulse[q];
                            vcp.tangent_impulse[0] = vec3::dot(old->friction_impulse[q], vc.tangent1);
                            vcp.tangent_impulse[1] = vec3::dot(old->friction_impulse[q], vc.tangent2);
                        }
                    }
                }
            }

            m_constraints.push_back(vc);
        }
    }

auto Simulation3D::warm_start() -> void {
        for (auto& vc : m_constraints) {
            for (int p = 0; p < vc.point_count; ++p) {
                const auto& vcp = vc.points[p];
                vector3d P = vc.normal * vcp.normal_impulse + vc.tangent1 * vcp.tangent_impulse[0] + vc.tangent2 * vcp.tangent_impulse[1];
                apply_impulse(*vc.body_a, *vc.body_b, vcp.rA, vcp.rB, P);
            }
        }
    }

auto Simulation3D::solve_velocity_constraints(bool reverse) -> void {
        const std::size_t count = m_constraints.size();

        for (std::size_t ci = 0; ci < count; ++ci) {
            auto& vc = m_constraints[reverse ? count - 1 - ci : ci];
            RigidBody3D& a = *vc.body_a;
            RigidBody3D& b = *vc.body_b;

            for (int pi = 0; pi < vc.point_count; ++pi) {
                auto& vcp = vc.points[reverse ? vc.point_count - 1 - pi : pi];
                vector3d dv = point_velocity(b, vcp.rB) - point_velocity(a, vcp.rA);
                double old1 = vcp.tangent_impulse[0], old2 = vcp.tangent_impulse[1];
                double new1 = old1 - vec3::dot(dv, vc.tangent1) * vcp.tangent_mass[0];
                double new2 = old2 - vec3::dot(dv, vc.tangent2) * vcp.tangent_mass[1];
                double max_friction = vc.friction * vcp.normal_impulse;
                double mag_sq = new1 * new1 + new2 * new2;

                if (mag_sq > max_friction * max_friction) {
                    double scale = (mag_sq > 0.0) ? max_friction / std::sqrt(mag_sq) : 0.0;
                    new1 *= scale;
                    new2 *= scale;
                }

                vcp.tangent_impulse[0] = new1;
                vcp.tangent_impulse[1] = new2;
                apply_impulse(a, b, vcp.rA, vcp.rB, vc.tangent1 * (new1 - old1) + vc.tangent2 * (new2 - old2));
            }

            if (vc.point_count >= 2 && solve_normal_block(vc)) continue;

            for (int pi = 0; pi < vc.point_count; ++pi) {
                auto& vcp = vc.points[reverse ? vc.point_count - 1 - pi : pi];
                double vn = vec3::dot(point_velocity(b, vcp.rB) - point_velocity(a, vcp.rA), vc.normal);
                double lambda = -(vn - vcp.velocity_bias) * vcp.normal_mass;
                double new_impulse = std::max(vcp.normal_impulse + lambda, 0.0);
                lambda = new_impulse - vcp.normal_impulse;
                vcp.normal_impulse = new_impulse;
                apply_impulse(a, b, vcp.rA, vcp.rB, vc.normal * lambda);
            }
        }
    }

auto Simulation3D::solve_normal_block(VelocityConstraint3D& vc) -> bool {
        constexpr int N = ContactManifold3D::MAX_POINTS;
        const int n = vc.point_count;
        RigidBody3D& a = *vc.body_a;
        RigidBody3D& b = *vc.body_b;
        double bvec[N], acc[N], x[N];
        double scale = 0.0;

        for (int i = 0; i < n; ++i) {
            const auto& c = vc.points[i];
            acc[i] = c.normal_impulse;
            bvec[i] = vec3::dot(point_velocity(b, c.rB) - point_velocity(a, c.rA), vc.normal) - c.velocity_bias;
            scale = std::max(scale, vc.normal_block[i][i]);
        }

        if (!(scale > 0.0)) return false;
        for (int i = 0; i < n; ++i) for (int j = 0; j < n; ++j) bvec[i] -= vc.normal_block[i][j] * acc[j];
        const double reg = 1e-6 * scale;
        double b_max = 0.0;
        for (int i = 0; i < n; ++i) b_max = std::max(b_max, std::abs(bvec[i]));
        const double vel_tol = 1e-9 * (1.0 + b_max); // m/s
        const double imp_tol = vel_tol / scale;      // N*s
        const int subsets = 1 << n;

        for (int size = n; size >= 0; --size) {
            for (int mask = subsets - 1; mask >= 0; --mask) {
                int bits = 0;
                for (int i = 0; i < n; ++i) bits += (mask >> i) & 1;
                if (bits != size) continue;
                int idx[N], m = 0;
                for (int i = 0; i < n; ++i) if (mask & (1 << i)) idx[m++] = i;
                double A[N][N + 1];

                for (int r = 0; r < m; ++r) {
                    for (int c = 0; c < m; ++c) A[r][c] = vc.normal_block[idx[r]][idx[c]] + (r == c ? reg : 0.0);
                    A[r][m] = -bvec[idx[r]];
                }

                bool ok = true;

                for (int col = 0; col < m && ok; ++col) { 
                    int piv = col;
                    for (int r = col + 1; r < m; ++r) if (std::abs(A[r][col]) > std::abs(A[piv][col])) piv = r;
                    if (std::abs(A[piv][col]) <= 1e-300) { ok = false; break; }
                    if (piv != col) for (int c = 0; c <= m; ++c) std::swap(A[piv][c], A[col][c]);

                    for (int r = col + 1; r < m; ++r) {
                        double f = A[r][col] / A[col][col];
                        for (int c = col; c <= m; ++c) A[r][c] -= f * A[col][c];
                    }
                }

                if (!ok) continue;
                for (int i = 0; i < n; ++i) x[i] = 0.0;

                for (int r = m - 1; r >= 0; --r) {
                    double v = A[r][m];
                    for (int c = r + 1; c < m; ++c) v -= A[r][c] * x[idx[c]];
                    x[idx[r]] = v / A[r][r];
                }

                for (int i = 0; i < n && ok; ++i) {
                    if (mask & (1 << i)) {
                        if (x[i] < -imp_tol) ok = false;
                    } else {
                        double w = bvec[i];
                        for (int j = 0; j < n; ++j) w += vc.normal_block[i][j] * x[j];
                        if (w < -vel_tol) ok = false;
                    }
                }

                if (!ok) continue;
                vector3d P_lin{}, ang_a{}, ang_b{};

                for (int i = 0; i < n; ++i) {
                    x[i] = std::max(x[i], 0.0);
                    vector3d P = vc.normal * (x[i] - acc[i]);
                    P_lin += P;
                    ang_a += vec3::cross(vc.points[i].rA, P);
                    ang_b += vec3::cross(vc.points[i].rB, P);
                    vc.points[i].normal_impulse = x[i];
                }

                a.set_linear_velocity(a.linear_velocity() - P_lin * a.inv_mass());
                a.set_angular_velocity(a.angular_velocity() - a.apply_inv_inertia(ang_a));
                b.set_linear_velocity(b.linear_velocity() + P_lin * b.inv_mass());
                b.set_angular_velocity(b.angular_velocity() + b.apply_inv_inertia(ang_b));
                return true;
            }
        }

        return false;
    }

auto Simulation3D::store_impulses() -> void {
        for (const auto& vc : m_constraints) {
            ContactManifold3D& mf = m_manifolds[vc.manifold_index];

            for (int p = 0; p < vc.point_count; ++p) {
                mf.points[p].normal_impulse   = vc.points[p].normal_impulse;
                mf.points[p].friction_impulse = vc.tangent1 * vc.points[p].tangent_impulse[0] + vc.tangent2 * vc.points[p].tangent_impulse[1];
            }
        }
    }

auto Simulation3D::solve_position_constraints(bool reverse) -> bool {
        double min_separation = 0.0;
        const std::size_t count = m_manifolds.size();

        for (std::size_t mi = 0; mi < count; ++mi) {
            auto& mf = m_manifolds[reverse ? count - 1 - mi : mi];
            RigidBody3D& a = *mf.body_a;
            RigidBody3D& b = *mf.body_b;

            for (int pi = 0; pi < mf.point_count; ++pi) {
                const int p = reverse ? mf.point_count - 1 - pi : pi;
                vector3d pA = a.transform().apply(mf.points[p].local_anchor_a);
                vector3d pB = b.transform().apply(mf.points[p].local_anchor_b);
                double separation = vec3::dot(pB - pA, mf.normal);
                min_separation = std::min(min_separation, separation);
                double C = std::clamp(m_cfg.baumgarte_factor * (separation + m_cfg.slop), -m_cfg.max_correction, 0.0);
                if (C == 0.0) continue;
                vector3d point = (pA + pB) * 0.5;
                vector3d rA = point - a.world_center();
                vector3d rB = point - b.world_center();
                double K = effective_inv_mass(a, b, rA, rB, mf.normal);
                double impulse = (K > 0.0) ? -C / K : 0.0;
                vector3d P = mf.normal * impulse;
                vector3d dwA = a.apply_inv_inertia(vec3::cross(rA, P)) * -1.0;
                vector3d dwB = b.apply_inv_inertia(vec3::cross(rB, P));
                a.apply_position_correction(P * -a.inv_mass(), dwA);
                b.apply_position_correction(P *  b.inv_mass(), dwB);
            }
        }

        return min_separation >= -3.0 * m_cfg.slop;
    }

auto Simulation3D::update_contact_cache() -> void {
        for (const auto& mf : m_manifolds) {
            auto it = m_touching.find(key_of(mf));
            if (it == m_touching.end()) continue;
            CachedContact& c = it->second;
            c.point_count = mf.point_count;

            for (int p = 0; p < mf.point_count; ++p) {
                c.local_anchor_a[p]   = mf.points[p].local_anchor_a;
                c.normal_impulse[p]   = mf.points[p].normal_impulse;
                c.friction_impulse[p] = mf.points[p].friction_impulse;
            }
        }

        for (const Key& key : m_carried) {
            auto old = m_contact_cache.find(key);
            if (old != m_contact_cache.end()) m_touching.emplace(key, old->second);
        }

        auto was_touching = [](const ContactCache& cache, const Key& key) {
            auto it = cache.find(key);
            return it != cache.end() && it->second.touching;
        };

        for (auto& [key, c] : m_touching) {
            if (!c.events || !c.touching || was_touching(m_contact_cache, key)) continue;
            double impulse = 0.0;
            for (int p = 0; p < c.point_count; ++p) impulse += c.normal_impulse[p];
            m_begin_events.push_back({ c.body_a, c.body_b, c.normal, c.point, impulse, c.sensor });
        }

        for (auto& [key, c] : m_contact_cache) {
            if (!c.events || !c.touching || was_touching(m_touching, key)) continue;
            m_end_events.push_back({ c.body_a, c.body_b, c.normal, c.point, 0.0, c.sensor });
        }

        m_contact_cache.swap(m_touching);
        m_touching.clear();
        m_carried.clear();
    }

auto Simulation3D::dispatch_events() -> void {
        if (m_begin_events.empty() && m_end_events.empty()) return;
        std::vector<ContactEvent3D> begins, ends;
        begins.swap(m_begin_events);
        ends.swap(m_end_events);
        m_locked = true;
        if (m_on_begin) for (const auto& e : begins) m_on_begin(e);
        if (m_on_end)   for (const auto& e : ends)   m_on_end(e);
        m_locked = false;
    }

auto Simulation3D::remove_body_now(RigidBody3D* body) -> void {
        auto find_owner = [this, body]() {
            return std::find_if(m_bodies.begin(), m_bodies.end(), [body](const std::unique_ptr<RigidBody3D>& p) { return p.get() == body; });
        };

        if (find_owner() == m_bodies.end()) return;

        for (auto it = m_contact_cache.begin(); it != m_contact_cache.end(); ) {
            if (it->first.body_a == body || it->first.body_b == body) {
                const CachedContact& c = it->second;
                if (c.events && c.touching) m_end_events.push_back({ c.body_a, c.body_b, c.normal, c.point, 0.0, c.sensor });
                it = m_contact_cache.erase(it);
            } else {
                ++it;
            }
        }

        auto refers = [body](const ContactManifold3D& mf) { return mf.body_a == body || mf.body_b == body; };
        m_manifolds.erase(std::remove_if(m_manifolds.begin(), m_manifolds.end(), refers), m_manifolds.end());
        m_constraints.clear();
        m_pairs.clear();
        m_body_ptrs.clear();
        m_body_aabbs.clear();
        dispatch_events(); 
        auto owner = find_owner();
        if (owner != m_bodies.end()) m_bodies.erase(owner);
    }

auto Simulation3D::flush_pending_removals() -> void {
        while (!m_pending_removals.empty()) {
            std::vector<RigidBody3D*> pending;
            pending.swap(m_pending_removals);
            std::sort(pending.begin(), pending.end(), std::less<RigidBody3D*>());
            pending.erase(std::unique(pending.begin(), pending.end()), pending.end());
            for (RigidBody3D* b : pending) remove_body_now(b);
        }
    }

auto Simulation3D::cleanup_bodies() -> void {
        const AABB3D world_bounds = world_bounds_of(m_cfg);

        for (auto& bp : m_bodies) {
            RigidBody3D& b = *bp;
            bool remove = false;
            if (has_flag(b.flags(), PhysicsFlags::DestroyOnSleep) && !b.is_awake()) remove = true;
            if (has_flag(b.flags(), PhysicsFlags::DestroyOffScreen) && !world_bounds.overlaps(b.compute_body_aabb())) remove = true;
            if (remove) m_pending_removals.push_back(&b);
        }

        flush_pending_removals();
    }

} // namespace physics
} // namespace fizmo
