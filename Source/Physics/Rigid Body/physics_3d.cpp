#include "fizmo_library.hpp"
#include "physics_3d.hpp"

namespace fizmo {
namespace physics {
namespace vec3 {

RVec3 mul(const RMat3& m, const RVec3& v) noexcept {
    const auto& d = m.data;
    return { d[0] * v.x + d[1] * v.y + d[2] * v.z,
             d[3] * v.x + d[4] * v.y + d[5] * v.z,
             d[6] * v.x + d[7] * v.y + d[8] * v.z };
}

RVec3 mul_transpose(const RMat3& m, const RVec3& v) noexcept {
    const auto& d = m.data;
    return { d[0] * v.x + d[3] * v.y + d[6] * v.z,
             d[1] * v.x + d[4] * v.y + d[7] * v.z,
             d[2] * v.x + d[5] * v.y + d[8] * v.z };
}

void orthonormal_basis(const RVec3& n, RVec3& t1, RVec3& t2) noexcept {
    const Real sign = std::copysign(1.0_r, n.z);
    const Real a = -1.0_r / (sign + n.z);
    const Real b = n.x * n.y * a;
    t1 = { 1.0_r + sign * n.x * n.x * a, sign * b, -sign * n.x };
    t2 = { b, sign + n.y * n.y * a, -n.y };
}

RQuat integrate_rotation(const RQuat& q, const RVec3& w, Real dt) noexcept {
    const Real speed = length(w);
    if (speed * dt <= 1e-15_r) return q;
    RQuat dq = fast::from_axis_angle(w.x, w.y, w.z, speed * dt);
    return fast::normalize(fast::mul(dq, q));
}

} // namespace vec3
} // namespace physics
} // namespace fizmo

namespace fizmo {
namespace physics {

bool AABB3D::overlaps(const AABB3D& o) const noexcept {
    return min.x <= o.max.x && max.x >= o.min.x &&
           min.y <= o.max.y && max.y >= o.min.y &&
           min.z <= o.max.z && max.z >= o.min.z;
}

bool AABB3D::contains(const AABB3D& o) const noexcept {
    return min.x <= o.min.x && min.y <= o.min.y && min.z <= o.min.z &&
           max.x >= o.max.x && max.y >= o.max.y && max.z >= o.max.z;
}

auto AABB3D::fatten(Real margin) const noexcept -> AABB3D {
    return { { min.x - margin, min.y - margin, min.z - margin }, { max.x + margin, max.y + margin, max.z + margin } };
}

auto Sweep3D::get_transform(Real t) const noexcept -> Transform3D { // t: unitless [0,1]
    RVec3 p = position0 * (1.0_r - t) + position1 * t;
    Transform3D xf(p, fast::nlerp(orientation0, orientation1, t));
    xf.position = p - xf.rotate(center_local);
    return xf;
}

auto HullShape::box(Real hx, Real hy, Real hz) -> HullShape { // meter half-extents
    const RVec3 pts[8] = {
        {-hx, -hy, -hz}, { hx, -hy, -hz}, { hx,  hy, -hz}, {-hx,  hy, -hz},
        {-hx, -hy,  hz}, { hx, -hy,  hz}, { hx,  hy,  hz}, {-hx,  hy,  hz}
    };
    return from_points(pts, 8);
}

auto HullShape::from_points(const RVec3* pts, std::size_t n) -> HullShape {
    HullShape h;
    std::vector<RVec3> p;
    p.reserve(n);
    RVec3 lo = n ? pts[0] : RVec3{}, hi = lo;
    for (std::size_t i = 0; i < n; ++i) { lo = vec3::min(lo, pts[i]); hi = vec3::max(hi, pts[i]); }
    const Real extent = std::max({ hi.x - lo.x, hi.y - lo.y, hi.z - lo.z });
    if (!(extent > 0.0_r)) return h;
    const Real tol = 1e-7_r * extent;

    for (std::size_t i = 0; i < n; ++i) {
        bool dup = false;
        for (const auto& q : p) { if (vec3::length_squared(q - pts[i]) <= tol * tol) { dup = true; break; } }
        if (!dup) p.push_back(pts[i]);
    }

    if (p.size() < 4 || p.size() > 0xFFFF) return h;
    const std::size_t m = p.size();
    struct Plane { RVec3 n; Real d; };
    std::vector<Plane> planes;

    for (std::size_t i = 0; i < m; ++i) {
        for (std::size_t j = i + 1; j < m; ++j) {
            for (std::size_t k = j + 1; k < m; ++k) {
                RVec3 nrm = vec3::cross(p[j] - p[i], p[k] - p[i]);
                Real len = vec3::length(nrm);
                if (len <= tol * extent) continue; 
                nrm = nrm * (1.0_r / len);
                Real d = vec3::dot(nrm, p[i]);
                bool above = false, below = false;

                for (std::size_t q = 0; q < m && !(above && below); ++q) {
                    Real s = vec3::dot(nrm, p[q]) - d;
                    if (s > tol) above = true;
                    else if (s < -tol) below = true;
                }

                if (above == below) continue; 
                if (above) { nrm = nrm * -1.0_r; d = -d; }
                bool dup = false;

                for (const auto& pl : planes) {
                    if (vec3::dot(pl.n, nrm) > 1.0_r - 1e-9_r && std::abs(pl.d - d) <= tol) { dup = true; break; }
                }

                if (!dup) planes.push_back({ nrm, d });
            }
        }
    }

    std::vector<int> remap(m, -1);

    for (const auto& pl : planes) {
        RVec3 u, w;
        vec3::orthonormal_basis(pl.n, u, w);
        struct P2 { Real x, y; std::size_t idx; };
        std::vector<P2> on;

        for (std::size_t q = 0; q < m; ++q) {
            if (std::abs(vec3::dot(pl.n, p[q]) - pl.d) <= tol) on.push_back({ vec3::dot(u, p[q]), vec3::dot(w, p[q]), q });
        }

        if (on.size() < 3) continue;
        std::sort(on.begin(), on.end(), [](const P2& a, const P2& b) { return a.x < b.x || (a.x == b.x && a.y < b.y); });
        auto turn = [](const P2& o, const P2& a, const P2& b) { return (a.x - o.x) * (b.y - o.y) - (a.y - o.y) * (b.x - o.x); };
        std::vector<P2> ring(2 * on.size());
        std::size_t k = 0;
        const Real area_tol = tol * extent;

        for (std::size_t q = 0; q < on.size(); ++q) {
            while (k >= 2 && turn(ring[k - 2], ring[k - 1], on[q]) <= area_tol) --k;
            ring[k++] = on[q];
        }

        for (std::size_t q = on.size() - 1, t = k + 1; q-- > 0; ) {
            while (k >= t && turn(ring[k - 2], ring[k - 1], on[q]) <= area_tol) --k;
            ring[k++] = on[q];
        }

        ring.resize(k > 0 ? k - 1 : 0);
        if (ring.size() < 3) continue;
        HullFace f;
        f.normal = pl.n;

        for (const auto& r : ring) {
            if (remap[r.idx] < 0) { remap[r.idx] = static_cast<int>(h.vertices.size()); h.vertices.push_back(p[r.idx]); }
            f.indices.push_back(static_cast<std::uint16_t>(remap[r.idx]));
        }

        h.faces.push_back(std::move(f));
    }

    h.finalize();
    return h;
}

void HullShape::finalize() {
    edges.clear();
    edge_directions.clear();
    volume = 0.0_r;
    centroid = {};
    if (vertices.empty() || faces.empty()) return;
    for (auto& f : faces) f.offset = vec3::dot(f.normal, vertices[f.indices[0]]);

    for (const auto& f : faces) {
        for (std::size_t i = 0; i < f.indices.size(); ++i) {
            std::uint16_t a = f.indices[i], b = f.indices[(i + 1) % f.indices.size()];
            std::array<std::uint16_t, 2> e = { std::min(a, b), std::max(a, b) };
            if (std::find(edges.begin(), edges.end(), e) == edges.end()) edges.push_back(e);
        }
    }

    for (const auto& e : edges) {
        RVec3 d = vec3::normalize(vertices[e[1]] - vertices[e[0]]);
        bool dup = false;
        for (const auto& q : edge_directions) { if (std::abs(vec3::dot(q, d)) > 1.0_r - 1e-9_r) { dup = true; break; } }
        if (!dup) edge_directions.push_back(d);
    }

    RVec3 o{};
    for (const auto& v : vertices) o += v;
    o = o * (1.0_r / static_cast<Real>(vertices.size()));
    RVec3 c{};

    for (const auto& f : faces) {
        const RVec3& a = vertices[f.indices[0]];

        for (std::size_t i = 1; i + 1 < f.indices.size(); ++i) {
            const RVec3& b = vertices[f.indices[i]];
            const RVec3& d = vertices[f.indices[i + 1]];
            Real vol = vec3::dot(a - o, vec3::cross(b - o, d - o)) / 6.0_r;
            volume += vol;
            c += (o + a + b + d) * (vol * 0.25_r);
        }
    }

    centroid = (volume > 0.0_r) ? c * (1.0_r / volume) : o;
}

auto HullShape::bounds() const noexcept -> AABB3D {
    if (vertices.empty()) return {};
    AABB3D b(vertices[0], vertices[0]);
    for (const auto& v : vertices) { b.min = vec3::min(b.min, v); b.max = vec3::max(b.max, v); }
    return b;
}

auto Shape3D::make_sphere(const RVec3& center, Real radius) -> Shape3D {
    Shape3D s;
    s.type = ShapeType3D::Sphere;
    s.sphere = { center, radius };
    s.rebuild_bounds();
    return s;
}

auto Shape3D::make_capsule(const RVec3& p1, const RVec3& p2, Real radius) -> Shape3D {
    Shape3D s;
    s.type = ShapeType3D::Capsule;
    s.capsule = { p1, p2, radius };
    s.rebuild_bounds();
    return s;
}

auto Shape3D::make_hull(HullShape h) -> Shape3D {
    assert(h.valid());
    Shape3D s;
    s.type = ShapeType3D::Hull;
    s.hull = std::make_shared<const HullShape>(std::move(h));
    s.rebuild_bounds();
    return s;
}

auto Shape3D::make_plane(const RVec3& normal, Real offset) -> Shape3D {
    Shape3D s;
    s.type = ShapeType3D::Plane;
    s.plane = { vec3::normalize(normal), offset };
    s.rebuild_bounds();
    return s;
}

auto Shape3D::make_cylinder(Real half_height, Real radius) -> Shape3D {
    Shape3D s;
    s.type = ShapeType3D::Cylinder;
    s.cylinder = { std::max(half_height, 1e-6_r), std::max(radius, 1e-6_r) };
    s.rebuild_bounds();
    return s;
}

auto Shape3D::make_cone(Real half_height, Real radius) -> Shape3D {
    Shape3D s;
    s.type = ShapeType3D::Cone;
    s.cone = { std::max(half_height, 1e-6_r), std::max(radius, 1e-6_r) };
    s.rebuild_bounds();
    return s;
}

auto Shape3D::make_rounded_box_core(Real core_hx, Real core_hy, Real core_hz, Real radius) -> Shape3D {
    const Real r = std::max(0.0_r, radius);
    return make_rounded_box(std::max(0.0_r, core_hx) + r, std::max(0.0_r, core_hy) + r, std::max(0.0_r, core_hz) + r, r);
}

auto Shape3D::make_rounded_box(Real hx, Real hy, Real hz, Real radius) -> Shape3D {
    Shape3D s;
    s.type = ShapeType3D::RoundedBox;
    radius = std::max(radius, 0.0_r);
    s.rounded_box.radius = std::min({ radius, hx, hy, hz });
    const Real r = s.rounded_box.radius;
    s.rounded_box.half_extents = { std::max(hx - r, 1e-9_r), std::max(hy - r, 1e-9_r), std::max(hz - r, 1e-9_r) };
    s.rebuild_bounds();
    return s;
}

auto Shape3D::make_heightfield(HeightfieldShape h) -> Shape3D {
    Shape3D s;
    s.type = ShapeType3D::Heightfield;
    h.finalize();
    s.heightfield = std::make_shared<const HeightfieldShape>(std::move(h));
    s.rebuild_bounds();
    return s;
}

auto Shape3D::make_heightfield(std::vector<Real> heights, int columns, int rows, Real cell_x, Real cell_z) -> Shape3D {
    HeightfieldShape h;
    h.heights = std::move(heights);
    h.columns = columns;
    h.rows = rows;
    h.cell_x = cell_x;
    h.cell_z = cell_z;
    return make_heightfield(std::move(h));
}

auto Shape3D::make_mesh(TriangleMeshShape m) -> Shape3D {
    Shape3D s;
    s.type = ShapeType3D::Mesh;
    m.build();
    s.mesh = std::make_shared<const TriangleMeshShape>(std::move(m));
    s.rebuild_bounds();
    return s;
}

auto Shape3D::make_mesh(std::vector<RVec3> vertices, std::vector<std::array<std::uint32_t, 3>> triangles, bool two_sided) -> Shape3D {
    TriangleMeshShape m;
    m.vertices = std::move(vertices);
    m.triangles = std::move(triangles);
    m.two_sided = two_sided;
    return make_mesh(std::move(m));
}

Real HeightfieldShape::height_at(Real x, Real z) const noexcept {
    if (!valid()) return 0.0_r;
    const Real fx = (x + 0.5_r * width()) / cell_x;
    const Real fz = (z + 0.5_r * depth()) / cell_z;
    const int c = std::clamp(static_cast<int>(std::floor(fx)), 0, columns - 2);
    const int r = std::clamp(static_cast<int>(std::floor(fz)), 0, rows - 2);
    const Real u = std::clamp(fx - c, 0.0_r, 1.0_r);
    const Real v = std::clamp(fz - r, 0.0_r, 1.0_r);
    const Real h00 = height_at_vertex(c, r), h10 = height_at_vertex(c + 1, r), h01 = height_at_vertex(c, r + 1), h11 = height_at_vertex(c + 1, r + 1);
    if (u + v <= 1.0_r) return h00 + (h10 - h00) * u + (h01 - h00) * v;
    return h11 + (h01 - h11) * (1.0_r - u) + (h10 - h11) * (1.0_r - v);
}

void HeightfieldShape::finalize() {
    if (heights.empty()) {
        min_height = max_height = 0.0_r;
        return;
    }
    auto mm = std::minmax_element(heights.begin(), heights.end());
    min_height = *mm.first;
    max_height = *mm.second;
}

AABB3D HeightfieldShape::bounds() const noexcept {
    return { { -0.5_r * width(), min_height, -0.5_r * depth() }, { 0.5_r * width(), max_height, 0.5_r * depth() } };
}

void TriangleMeshShape::build() {
    nodes.clear();
    order.clear();
    normals.clear();
    triangles.erase(std::remove_if(triangles.begin(), triangles.end(), [this](const std::array<std::uint32_t, 3>& t) {
        return t[0] >= vertices.size() || t[1] >= vertices.size() || t[2] >= vertices.size();
    }), triangles.end());
    if (triangles.empty()) return;
    const std::size_t n = triangles.size();
    std::vector<AABB3D> boxes(n);
    std::vector<RVec3> centers(n);
    normals.resize(n);
    for (std::size_t i = 0; i < n; ++i) {
        const auto& a = vertices[triangles[i][0]];
        const auto& b = vertices[triangles[i][1]];
        const auto& c = vertices[triangles[i][2]];
        boxes[i] = { vec3::min(a, vec3::min(b, c)), vec3::max(a, vec3::max(b, c)) };
        centers[i] = (a + b + c) * (1.0_r / 3.0_r);
        normals[i] = vec3::normalize(vec3::cross(b - a, c - a));
    }
    order.resize(n);
    for (std::size_t i = 0; i < n; ++i) order[i] = static_cast<std::uint32_t>(i);
    nodes.reserve(2 * n);
    struct Job { std::int32_t node; std::uint32_t first; std::uint32_t count; int depth; };
    std::vector<Job> jobs;
    nodes.push_back(Node{});
    jobs.push_back({ 0, 0, static_cast<std::uint32_t>(n), 0 });
    while (!jobs.empty()) {
        const Job job = jobs.back();
        jobs.pop_back();
        AABB3D box = boxes[order[job.first]];
        AABB3D cbox(centers[order[job.first]], centers[order[job.first]]);
        for (std::uint32_t i = 0; i < job.count; ++i) {
            const std::uint32_t t = order[job.first + i];
            box = AABB3D::merge(box, boxes[t]);
            cbox = AABB3D::merge(cbox, AABB3D(centers[t], centers[t]));
        }
        Node& nd = nodes[static_cast<std::size_t>(job.node)];
        nd.box = box;
        if (job.count <= 4 || job.depth >= 30) {
            nd.first = job.first;
            nd.count = job.count;
            continue;
        }
        const RVec3 ext = cbox.max - cbox.min;
        const int axis = ext.x >= ext.y && ext.x >= ext.z ? 0 : (ext.y >= ext.z ? 1 : 2);
        const Real mid = vec3::component(cbox.center(), axis);
        auto first = order.begin() + job.first;
        auto last = first + job.count;
        auto split = std::partition(first, last, [&](std::uint32_t t) { return vec3::component(centers[t], axis) < mid; });
        std::uint32_t left_count = static_cast<std::uint32_t>(split - first);
        if (left_count == 0 || left_count == job.count) {
            left_count = job.count / 2;
            std::nth_element(first, first + left_count, last, [&](std::uint32_t a, std::uint32_t b) { return vec3::component(centers[a], axis) < vec3::component(centers[b], axis); });
        }
        const std::int32_t l = static_cast<std::int32_t>(nodes.size());
        nodes.push_back(Node{});
        const std::int32_t r = static_cast<std::int32_t>(nodes.size());
        nodes.push_back(Node{});
        nodes[static_cast<std::size_t>(job.node)].left = l;
        nodes[static_cast<std::size_t>(job.node)].right = r;
        jobs.push_back({ l, job.first, left_count, job.depth + 1 });
        jobs.push_back({ r, job.first + left_count, job.count - left_count, job.depth + 1 });
    }
}

bool TriangleMeshShape::raycast(const RVec3& origin, const RVec3& dir, Real max_t, Real& t, RVec3& normal, std::uint32_t& triangle) const {
    if (nodes.empty()) return false;
    const RVec3 inv{ std::abs(dir.x) > 1e-30_r ? 1.0_r / dir.x : 1e30_r, std::abs(dir.y) > 1e-30_r ? 1.0_r / dir.y : 1e30_r, std::abs(dir.z) > 1e-30_r ? 1.0_r / dir.z : 1e30_r };
    auto slab = [&](const AABB3D& b, Real limit) {
        Real t0 = 0.0_r, t1 = limit;
        const Real o[3] = { origin.x, origin.y, origin.z }, iv[3] = { inv.x, inv.y, inv.z };
        const Real lo[3] = { b.min.x, b.min.y, b.min.z }, hi[3] = { b.max.x, b.max.y, b.max.z };
        for (int i = 0; i < 3; ++i) {
            Real a = (lo[i] - o[i]) * iv[i], c = (hi[i] - o[i]) * iv[i];
            if (a > c) std::swap(a, c);
            t0 = std::max(t0, a);
            t1 = std::min(t1, c);
            if (t0 > t1) return false;
        }
        return true;
    };
    bool hit = false;
    Real best = max_t;
    std::int32_t stack[64];
    int sp = 0;
    stack[sp++] = 0;
    while (sp > 0) {
        const Node& n = nodes[static_cast<std::size_t>(stack[--sp])];
        if (!slab(n.box, best)) continue;
        if (n.left >= 0) {
            if (sp < 62) {
                stack[sp++] = n.left;
                stack[sp++] = n.right;
            }
            continue;
        }
        for (std::uint32_t i = 0; i < n.count; ++i) {
            const std::uint32_t ti = order[n.first + i];
            const auto& tri = triangles[ti];
            const RVec3& a = vertices[tri[0]];
            const RVec3 e1 = vertices[tri[1]] - a, e2 = vertices[tri[2]] - a;
            const RVec3 p = vec3::cross(dir, e2);
            const Real det = vec3::dot(e1, p);
            if (std::abs(det) < 1e-14_r) continue;
            if (!two_sided && det < 0.0_r) continue;
            const Real id = 1.0_r / det;
            const RVec3 s = origin - a;
            const Real u = vec3::dot(s, p) * id;
            if (u < 0.0_r || u > 1.0_r) continue;
            const RVec3 q = vec3::cross(s, e1);
            const Real v = vec3::dot(dir, q) * id;
            if (v < 0.0_r || u + v > 1.0_r) continue;
            const Real tt = vec3::dot(e2, q) * id;
            if (tt < 0.0_r || tt >= best) continue;
            best = tt;
            hit = true;
            triangle = ti;
            normal = normals[ti];
            if (vec3::dot(normal, dir) > 0.0_r) normal = normal * -1.0_r;
        }
    }
    if (hit) t = best;
    return hit;
}

void Shape3D::rebuild_bounds() {
    const Real big = PLANE_EXTENT;

    switch (type) {
        case ShapeType3D::Sphere: {
            Real r = sphere.radius;
            bounding_radius = vec3::length(sphere.center) + r;
            local_aabb = { sphere.center - RVec3{r, r, r}, sphere.center + RVec3{r, r, r} };
            break;
        }
        case ShapeType3D::Capsule: {
            Real r = capsule.radius;
            bounding_radius = std::max(vec3::length(capsule.point1), vec3::length(capsule.point2)) + r;
            local_aabb = { vec3::min(capsule.point1, capsule.point2) - RVec3{r, r, r}, vec3::max(capsule.point1, capsule.point2) + RVec3{r, r, r} };
            break;
        }
        case ShapeType3D::Hull: {
            bounding_radius = 0.0_r;
            if (hull) { for (const auto& v : hull->vertices) bounding_radius = std::max(bounding_radius, vec3::length(v)); }
            local_aabb = hull ? hull->bounds() : AABB3D{};
            break;
        }
        case ShapeType3D::Plane:
            bounding_radius = big;
            local_aabb = { {-big, -big, -big}, {big, big, big} };
            break;
        case ShapeType3D::Cylinder:
            bounding_radius = std::sqrt(cylinder.half_height * cylinder.half_height + cylinder.radius * cylinder.radius);
            local_aabb = { { -cylinder.radius, -cylinder.half_height, -cylinder.radius }, { cylinder.radius, cylinder.half_height, cylinder.radius } };
            break;
        case ShapeType3D::Cone:
            bounding_radius = std::sqrt(cone.half_height * cone.half_height + cone.radius * cone.radius);
            local_aabb = { { -cone.radius, -cone.half_height, -cone.radius }, { cone.radius, cone.half_height, cone.radius } };
            break;
        case ShapeType3D::RoundedBox: {
            const RVec3 e = rounded_box.half_extents + RVec3{ rounded_box.radius, rounded_box.radius, rounded_box.radius };
            bounding_radius = vec3::length(e);
            local_aabb = { e * -1.0_r, e };
            break;
        }
        case ShapeType3D::Heightfield:
            local_aabb = heightfield ? heightfield->bounds() : AABB3D{};
            bounding_radius = vec3::length(vec3::max(local_aabb.max, local_aabb.min * -1.0_r));
            break;
        case ShapeType3D::Mesh:
            local_aabb = mesh ? mesh->bounds() : AABB3D{};
            bounding_radius = vec3::length(vec3::max(local_aabb.max, local_aabb.min * -1.0_r));
            break;
    }
}

auto Shape3D::compute_aabb(const Transform3D& xf) const noexcept -> AABB3D {
    switch (type) {
        case ShapeType3D::Sphere: {
            RVec3 c = xf.apply(sphere.center);
            Real r = sphere.radius;
            return { c - RVec3{r, r, r}, c + RVec3{r, r, r} };
        }
        case ShapeType3D::Capsule: {
            RVec3 p1 = xf.apply(capsule.point1);
            RVec3 p2 = xf.apply(capsule.point2);
            Real r = capsule.radius;
            return { vec3::min(p1, p2) - RVec3{r, r, r}, vec3::max(p1, p2) + RVec3{r, r, r} };
        }
        case ShapeType3D::Hull: {
            if (!hull || hull->vertices.empty()) return { xf.position, xf.position };
            RVec3 v = xf.apply(hull->vertices[0]);
            AABB3D box(v, v);

            for (std::size_t i = 1; i < hull->vertices.size(); ++i) {
                v = xf.apply(hull->vertices[i]);
                box.min = vec3::min(box.min, v);
                box.max = vec3::max(box.max, v);
            }

            return box;
        }
        case ShapeType3D::Plane: {
            const Real big = PLANE_EXTENT;
            RVec3 n = xf.rotate(plane.normal);
            Real d = plane.offset + vec3::dot(n, xf.position);
            Real lo[3] = { -big, -big, -big }, hi[3] = { big, big, big };
            const Real nn[3] = { n.x, n.y, n.z };

            for (int i = 0; i < 3; ++i) {
                if (nn[i] >  1.0_r - 1e-12_r) hi[i] =  d;
                if (nn[i] < -1.0_r + 1e-12_r) lo[i] = -d;
            }

            return { { lo[0], lo[1], lo[2] }, { hi[0], hi[1], hi[2] } };
        }
        case ShapeType3D::Cylinder: {
            const RVec3 axis = xf.rotate({ 0.0_r, 1.0_r, 0.0_r });
            const RVec3 c = xf.position;
            const Real h = cylinder.half_height, r = cylinder.radius;
            const RVec3 e{ h * std::abs(axis.x) + r * std::sqrt(std::max(0.0_r, 1.0_r - axis.x * axis.x)),
                                      h * std::abs(axis.y) + r * std::sqrt(std::max(0.0_r, 1.0_r - axis.y * axis.y)),
                                      h * std::abs(axis.z) + r * std::sqrt(std::max(0.0_r, 1.0_r - axis.z * axis.z)) };
            return { c - e, c + e };
        }
        case ShapeType3D::RoundedBox: {
            const RVec3& he = rounded_box.half_extents;
            const auto& d = xf.rotation.data;
            const RVec3 e{ std::abs(d[0]) * he.x + std::abs(d[1]) * he.y + std::abs(d[2]) * he.z + rounded_box.radius,
                                      std::abs(d[3]) * he.x + std::abs(d[4]) * he.y + std::abs(d[5]) * he.z + rounded_box.radius,
                                      std::abs(d[6]) * he.x + std::abs(d[7]) * he.y + std::abs(d[8]) * he.z + rounded_box.radius };
            return { xf.position - e, xf.position + e };
        }
        case ShapeType3D::Cone:
        case ShapeType3D::Heightfield:
        case ShapeType3D::Mesh: {
            const RVec3 c = (local_aabb.min + local_aabb.max) * 0.5_r;
            const RVec3 he = (local_aabb.max - local_aabb.min) * 0.5_r;
            const auto& d = xf.rotation.data;
            const RVec3 wc = xf.apply(c);
            const RVec3 e{ std::abs(d[0]) * he.x + std::abs(d[1]) * he.y + std::abs(d[2]) * he.z,
                                      std::abs(d[3]) * he.x + std::abs(d[4]) * he.y + std::abs(d[5]) * he.z,
                                      std::abs(d[6]) * he.x + std::abs(d[7]) * he.y + std::abs(d[8]) * he.z };
            return { wc - e, wc + e };
        }
    }
    return {};
}

auto MassData3D::combine(const MassData3D* parts, std::size_t n) -> MassData3D {
    MassData3D total;
    if (n == 0) return total;
    for (std::size_t i = 0; i < n; ++i) total.mass += parts[i].mass;
    if (total.mass <= 0.0_r) return total;
    for (std::size_t i = 0; i < n; ++i) total.center += parts[i].center * parts[i].mass;
    total.center = total.center / total.mass;

    for (std::size_t i = 0; i < n; ++i) {
        RVec3 d = parts[i].center - total.center;
        total.inertia = total.inertia + parts[i].inertia + (RMat3::identity() * vec3::length_squared(d) - vec3::outer(d, d)) * parts[i].mass;
    }

    return total;
}

} // namespace physics
} // namespace fizmo

namespace fizmo {
namespace physics {
namespace detail {

Real capsule_volume(const CapsuleShape3D& c) {
    Real r = c.radius;
    Real h = vec3::length(c.point2 - c.point1);
    return static_cast<Real>(constants::pi()) * r * r * (h + 4.0_r / 3.0_r * r);
}

MassData3D compute_sphere_mass(const SphereShape& s, Real density) {
    Real mass = density * sphere_volume(s);
    Real I = 0.4_r * mass * s.radius * s.radius;
    return { mass, RMat3::diagonal(I, I, I), s.center };
}

MassData3D compute_capsule_mass(const CapsuleShape3D& c, Real density) {
    const Real pi = static_cast<Real>(constants::pi());
    Real r = c.radius;
    RVec3 axis = c.point2 - c.point1;
    Real h = vec3::length(axis);
    RVec3 a = vec3::normalize(axis);
    Real m_cyl = density * pi * r * r * h;
    Real m_sph = density * 4.0_r / 3.0_r * pi * r * r * r; 
    Real I_axis = m_cyl * r * r * 0.5_r + m_sph * r * r * 0.4_r;
    Real I_perp = m_cyl * (h * h / 12.0_r + r * r * 0.25_r) + m_sph * (r * r * 0.4_r + h * h * 0.25_r + 3.0_r * h * r / 8.0_r);
    RMat3 aa = vec3::outer(a, a);
    RMat3 I = aa * I_axis + (RMat3::identity() - aa) * I_perp;
    return { m_cyl + m_sph, I, c.center() };
}

MassData3D compute_hull_mass(const HullShape& h, Real density) {
    MassData3D md;
    if (!h.valid()) return md;
    RVec3 o{};
    for (const auto& v : h.vertices) o += v;
    o = o * (1.0_r / static_cast<Real>(h.vertices.size()));
    
    const RMat3 canonical(1.0_r / 60.0_r,  1.0_r / 120.0_r, 1.0_r / 120.0_r,
                                   1.0_r / 120.0_r, 1.0_r / 60.0_r,  1.0_r / 120.0_r,
                                   1.0_r / 120.0_r, 1.0_r / 120.0_r, 1.0_r / 60.0_r
                                );

    RMat3 C{};

    for (const auto& f : h.faces) {
        const RVec3 a = h.vertices[f.indices[0]] - o;

        for (std::size_t i = 1; i + 1 < f.indices.size(); ++i) {
            const RVec3 b = h.vertices[f.indices[i]] - o;
            const RVec3 d = h.vertices[f.indices[i + 1]] - o;

            const RMat3 A(a.x, b.x, d.x,
                                   a.y, b.y, d.y,
                                   a.z, b.z, d.z
                                );

            const Real det = vec3::dot(a, vec3::cross(b, d));
            C = C + fast::mul(fast::mul(A, canonical), A.transpose()) * det;
        }
    }

    md.mass = density * h.volume;
    md.center = h.centroid;
    C = C * density;
    const RVec3 com = h.centroid - o;
    C = C - vec3::outer(com, com) * md.mass; 
    md.inertia = RMat3::identity() * fast::trace(C) - C;
    return md;
}

} // namespace detail
} // namespace physics
} // namespace fizmo

namespace fizmo {
namespace physics {

Real compute_shape_volume(const Shape3D& shape) {
    switch (shape.type) {
        case ShapeType3D::Sphere:  return detail::sphere_volume(shape.sphere);
        case ShapeType3D::Capsule: return detail::capsule_volume(shape.capsule);
        case ShapeType3D::Hull:    return shape.hull ? shape.hull->volume : 0.0_r;
        case ShapeType3D::Plane:   return 0.0_r; // infinite; treated as massless
        case ShapeType3D::Cylinder: return static_cast<Real>(constants::pi()) * shape.cylinder.radius * shape.cylinder.radius * 2.0_r * shape.cylinder.half_height;
        case ShapeType3D::Cone: return static_cast<Real>(constants::pi()) * shape.cone.radius * shape.cone.radius * 2.0_r * shape.cone.half_height / 3.0_r;
        case ShapeType3D::RoundedBox: {
            const RVec3 e = shape.rounded_box.half_extents * 2.0_r;
            const Real r = shape.rounded_box.radius;
            const Real pi = static_cast<Real>(constants::pi());
            return e.x * e.y * e.z + 2.0_r * r * (e.x * e.y + e.y * e.z + e.z * e.x) + pi * r * r * (e.x + e.y + e.z) + 4.0_r / 3.0_r * pi * r * r * r;
        }
        case ShapeType3D::Heightfield:
        case ShapeType3D::Mesh: return 0.0_r;
    }
    return 0.0_r;
}

MassData3D compute_mass(const Shape3D& shape, Real density) {
    switch (shape.type) {
        case ShapeType3D::Sphere:  return detail::compute_sphere_mass(shape.sphere, density);
        case ShapeType3D::Capsule: return detail::compute_capsule_mass(shape.capsule, density);
        case ShapeType3D::Hull:    return shape.hull ? detail::compute_hull_mass(*shape.hull, density) : MassData3D{};
        case ShapeType3D::Plane:   return {};
        case ShapeType3D::Cylinder: {
            const Real r = shape.cylinder.radius, h = 2.0_r * shape.cylinder.half_height;
            const Real m = density * compute_shape_volume(shape);
            const Real ia = 0.5_r * m * r * r;
            const Real ip = m * (3.0_r * r * r + h * h) / 12.0_r;
            return { m, RMat3::diagonal(ip, ia, ip), {} };
        }
        case ShapeType3D::Cone: {
            const Real r = shape.cone.radius, h = 2.0_r * shape.cone.half_height;
            const Real m = density * compute_shape_volume(shape);
            const Real ia = 0.3_r * m * r * r;
            const Real ip = m * (3.0_r * r * r / 20.0_r + 3.0_r * h * h / 80.0_r);
            return { m, RMat3::diagonal(ip, ia, ip), { 0.0_r, -0.5_r * shape.cone.half_height, 0.0_r } };
        }
        case ShapeType3D::RoundedBox: {
            const RVec3 e = (shape.rounded_box.half_extents + RVec3{ shape.rounded_box.radius, shape.rounded_box.radius, shape.rounded_box.radius }) * 2.0_r;
            const Real m = density * compute_shape_volume(shape);
            return { m, RMat3::diagonal(m * (e.y * e.y + e.z * e.z) / 12.0_r, m * (e.x * e.x + e.z * e.z) / 12.0_r, m * (e.x * e.x + e.y * e.y) / 12.0_r), {} };
        }
        case ShapeType3D::Heightfield:
        case ShapeType3D::Mesh: return {};
    }
    return {};
}

Real MassSpec3D::resolve_density(Real shape_volume_m3, Real gravity) const noexcept {
    switch (mode) {
        case MassMode::Density: return value;
        case MassMode::Mass:    return (shape_volume_m3 > 0.0_r) ? value / shape_volume_m3 : 0.0_r;
        case MassMode::Weight:  return (shape_volume_m3 > 0.0_r && gravity > 0.0_r) ? (value / gravity) / shape_volume_m3 : 0.0_r;
    }
    return 0.0_r;
}

void Collider3D::recompute_mass(Real gravity) {
    const Real volume  = compute_shape_volume(shape);
    const Real density = material.mass_spec.resolve_density(volume, gravity);
    mass_data = compute_mass(shape, density);
    mass_data.center  = local_offset.apply(mass_data.center);
    mass_data.inertia = fast::rotate_inertia(mass_data.inertia, local_offset.orientation);
}

void Collider3D::update_world_aabb(const Transform3D& body_xf) {
    Transform3D world_xf = body_xf.compose(local_offset);
    world_aabb = shape.compute_aabb(world_xf);
}

bool RigidBody3D::freeze(FrozenProperty prop) noexcept {
    FrozenProperty candidate = m_frozen | prop;
    if (!validate_frozen(candidate)) return false;
    m_frozen = candidate;
    return true;
}

bool RigidBody3D::can_collide_with(const RigidBody3D& other) const noexcept {
    if (!has(PhysicsFlags::CollisionResponse) && !other.has(PhysicsFlags::CollisionResponse)) return false;
    if (has(PhysicsFlags::Intangible) && other.has(PhysicsFlags::Intangible)) return false;
    if ((m_collision_layer & other.m_collision_mask) == 0) return false;
    if ((other.m_collision_layer & m_collision_mask) == 0) return false;
    return true;
}

auto RigidBody3D::add_collider(
    Shape3D shape,
    Material3D mat,
    Transform3D offset,
    CollisionFilter filt,
    bool sensor 
) -> Collider3D& {
    m_colliders.emplace_back(Collider3D{
        std::move(shape),
        std::move(offset),
        std::move(mat),
        std::move(filt),
        sensor,
        this,
        {},
        {}
    });

    Collider3D& c = m_colliders.back();
    c.update_world_aabb(m_transform);
    m_mass_dirty = true;
    return c;
}

void RigidBody3D::set_rotation(const RVec3& axis, Real angle, bool in_rad) noexcept {
    if (!in_rad) angle /= constants::reciprocal_pi_180();
    set_orientation(fast::from_axis_angle(axis.x, axis.y, axis.z, angle));
}

std::array<Real, 3> RigidBody3D::euler_angles(bool in_rad) const noexcept {
    std::array<Real, 3> e = m_transform.orientation.to_euler();
    if (!in_rad) for (auto& a : e) a *= constants::reciprocal_pi_180();
    return e;
}

void RigidBody3D::set_euler_angles(Real roll, Real pitch, Real yaw, bool in_rad) noexcept {
    if (!in_rad) {
        roll  *= constants::pi_180();
        pitch *= constants::pi_180();
        yaw   *= constants::pi_180();
    }

    set_orientation(fast::from_euler(roll, pitch, yaw));
}

void RigidBody3D::set_yaw_pitch_roll(Real yaw, Real pitch, Real roll, bool in_rad) noexcept {
    set_yaw_pitch_roll(default_axes3d(), yaw, pitch, roll, in_rad);
}

std::array<Real, 3> RigidBody3D::yaw_pitch_roll(bool in_rad) const noexcept {
    return yaw_pitch_roll(default_axes3d(), in_rad);
}

void RigidBody3D::set_density(Real d) noexcept {
    if (is_frozen(FrozenProperty::Density)) return;
    ensure_mass_uptodate();
    refresh_shape_volume();
    m_density = d;
    resolve_mass_constraint_from(FrozenProperty::Density);
}

void RigidBody3D::set_mass(Real kg) noexcept {
    if (is_frozen(FrozenProperty::Mass)) return;
    ensure_mass_uptodate();
    refresh_shape_volume();
    m_mass     = kg;
    m_inv_mass = (kg > 0.0_r) ? 1.0_r / kg : 0.0_r;
    resolve_mass_constraint_from(FrozenProperty::Mass);
}

auto RigidBody3D::inv_inertia_world() const noexcept -> const RMat3& {
    ensure_mass_uptodate();

    if (m_world_inertia_dirty) {
        const RMat3& R = m_transform.rotation;
        RMat3 W = fast::mul(fast::mul(R, m_inv_inertia), R.transpose());
        const bool lock[3] = { has(PhysicsFlags::LockRotationX), has(PhysicsFlags::LockRotationY), has(PhysicsFlags::LockRotationZ) };

        for (int r = 0; r < 3; ++r) {
            for (int c = 0; c < 3; ++c) {
                if (lock[r] || lock[c]) W.at(static_cast<std::size_t>(r), static_cast<std::size_t>(c)) = 0.0_r;
            }
        }

        m_inv_inertia_world = W;
        m_world_inertia_dirty = false;
    }

    return m_inv_inertia_world;
}

void RigidBody3D::apply_force_at(const RVec3& f, const RVec3& world_point) {
    if (!has(PhysicsFlags::LinearForces)) return;
    ensure_mass_uptodate();
    m_force += f;
    if (has(PhysicsFlags::Torque)) m_torque += vec3::cross(world_point - world_center(), f);
    wake();
}

void RigidBody3D::apply_linear_impulse(const RVec3& impulse) {
    if (!has(PhysicsFlags::LinearImpulses)) return;
    ensure_mass_uptodate();
    m_linear_velocity += impulse * m_inv_mass;
    wake();
}

void RigidBody3D::apply_impulse_at(const RVec3& impulse, const RVec3& world_point) {
    if (!has(PhysicsFlags::LinearImpulses)) return;
    ensure_mass_uptodate();
    m_linear_velocity += impulse * m_inv_mass;
    if (has(PhysicsFlags::AngularImpulses)) m_angular_velocity += apply_inv_inertia(vec3::cross(world_point - world_center(), impulse));
    wake();
}

void RigidBody3D::apply_angular_impulse(const RVec3& impulse) {
    if (!has(PhysicsFlags::AngularImpulses)) return;
    ensure_mass_uptodate();
    m_angular_velocity += apply_inv_inertia(impulse);
    wake();
}

void RigidBody3D::integrate_forces(Real dt, const RVec3& gravity) {
    ensure_mass_uptodate();
    if (has(PhysicsFlags::Gravity))        m_linear_velocity += gravity * m_gravity_scale * dt;
    if (has(PhysicsFlags::LinearForces))   m_linear_velocity += m_force * m_inv_mass * dt;
    if (has(PhysicsFlags::Torque))         m_angular_velocity += apply_inv_inertia(m_torque) * dt;
    if (has(PhysicsFlags::LinearDamping))  m_linear_velocity *= 1.0_r / (1.0_r + dt * m_linear_damping);
    if (has(PhysicsFlags::AngularDamping)) m_angular_velocity *= 1.0_r / (1.0_r + dt * m_angular_damping);

    if (has(PhysicsFlags::VelocityClamping)) {
        Real speed_sq = vec3::length_squared(m_linear_velocity);
        if (speed_sq > m_max_linear_speed * m_max_linear_speed) m_linear_velocity = m_linear_velocity * (m_max_linear_speed / std::sqrt(speed_sq));
    }

    if (has(PhysicsFlags::AngularClamping)) {
        Real speed_sq = vec3::length_squared(m_angular_velocity);
        if (speed_sq > m_max_angular_speed * m_max_angular_speed) m_angular_velocity = m_angular_velocity * (m_max_angular_speed / std::sqrt(speed_sq));
    }

    m_angular_velocity = mask_rotation(m_angular_velocity);
}

void RigidBody3D::integrate_gyroscopic(Real dt) {
    ensure_mass_uptodate();
    m_gyro_half = {};
    if (m_inv_mass <= 0.0_r || !has(PhysicsFlags::AngularVelocity)) return;
    if (has(PhysicsFlags::LockRotationX) || has(PhysicsFlags::LockRotationY) || has(PhysicsFlags::LockRotationZ)) return;
    if (fast::trace(m_inertia) <= 0.0_r || vec3::is_zero(m_angular_velocity)) return;
    const RMat3& I  = m_inertia;
    const RMat3& Ii = m_inv_inertia;
    auto euler = [&](const RVec3& w) { return vec3::mul(Ii, vec3::cross(vec3::mul(I, w), w)); };
    const RMat3& R = m_transform.rotation;
    const RVec3 wb = vec3::mul_transpose(R, m_angular_velocity);
    const RVec3 k1 = euler(wb);
    const RVec3 k2 = euler(wb + k1 * (dt * 0.5_r));
    const RVec3 k3 = euler(wb + k2 * (dt * 0.5_r));
    const RVec3 k4 = euler(wb + k3 * dt);
    const RVec3 dw = vec3::mul(R, (k1 + k2 * 2.0_r + k3 * 2.0_r + k4) * (dt / 6.0_r));
    m_angular_velocity += dw;
    m_gyro_half = dw * 0.5_r;
}

void RigidBody3D::integrate_velocities(Real dt) {
    ensure_mass_uptodate();
    RVec3 center = m_transform.apply(m_local_center);
    m_sweep.position0    = center;
    m_sweep.orientation0 = m_transform.orientation;
    if (has(PhysicsFlags::LinearVelocity)) center += mask_translation(m_linear_velocity * dt);

    if (has(PhysicsFlags::AngularVelocity) && !has(PhysicsFlags::LockRotation3D)) {
        RVec3 w = mask_rotation(m_angular_velocity - m_gyro_half);

        if (!vec3::is_zero(w)) {
            const RVec3 w_body = m_transform.rotate_inverse(m_angular_velocity);
            set_orientation(vec3::integrate_rotation(m_transform.orientation, w, dt));
            if (!vec3::is_zero(m_gyro_half)) m_angular_velocity = m_transform.rotate(w_body); 
        }
    }

    m_gyro_half = {};
    m_transform.position = center - m_transform.rotate(m_local_center);
    m_sweep.position1    = center;
    m_sweep.orientation1 = m_transform.orientation;
}

void RigidBody3D::apply_position_correction(const RVec3& d_center, const RVec3& d_rotation) {
    RVec3 dc = mask_translation(d_center);
    RVec3 dr = mask_rotation(d_rotation);
    if (vec3::is_zero(dc) && vec3::is_zero(dr)) return;
    ensure_mass_uptodate();
    RVec3 center = m_transform.apply(m_local_center) + dc;
    if (!vec3::is_zero(dr)) set_orientation(vec3::integrate_rotation(m_transform.orientation, dr, 1.0_r));
    m_transform.position = center - m_transform.rotate(m_local_center);
}

void RigidBody3D::put_to_sleep() {
    m_is_awake         = false;
    m_sleep_timer      = 0.0_r;
    m_linear_velocity = {};
    m_angular_velocity = {};
    m_force = {};
    m_torque = {};
    m_gyro_half = {};
}

Real RigidBody3D::advance_sleep_timer(Real dt) {
    if (!has(PhysicsFlags::AllowSleep)) { m_sleep_timer = 0.0_r; return 0.0_r; }

    if (vec3::length_squared(m_linear_velocity) > m_linear_sleep_tolerance * m_linear_sleep_tolerance ||
        vec3::length_squared(m_angular_velocity) > m_angular_sleep_tolerance * m_angular_sleep_tolerance) {
        m_sleep_timer = 0.0_r;
    } else {
        m_sleep_timer += dt;
    }

    return m_sleep_timer;
}

void RigidBody3D::update_sleep(Real dt) {
    if (!has(PhysicsFlags::AllowSleep)) return;
    if (advance_sleep_timer(dt) >= m_sleep_time) put_to_sleep();
}

auto RigidBody3D::compute_body_aabb() const -> AABB3D {
    if (m_colliders.empty()) return { m_transform.position, m_transform.position };
    AABB3D result = m_colliders[0].world_aabb;
    for (std::size_t i = 1; i < m_colliders.size(); ++i) result = AABB3D::merge(result, m_colliders[i].world_aabb);
    return result;
}

Real RigidBody3D::kinetic_energy() const noexcept {
    ensure_mass_uptodate();
    Real lin = 0.5_r * m_mass * vec3::length_squared(m_linear_velocity);
    RVec3 wb = m_transform.rotate_inverse(m_angular_velocity);
    Real rot = 0.5_r * vec3::dot(wb, vec3::mul(m_inertia, wb));
    return lin + rot;
}

auto RigidBody3D::mask_rotation(RVec3 w) const noexcept -> RVec3 {
    if (has(PhysicsFlags::LockRotationX)) w.x = 0.0_r;
    if (has(PhysicsFlags::LockRotationY)) w.y = 0.0_r;
    if (has(PhysicsFlags::LockRotationZ)) w.z = 0.0_r;
    return w;
}

auto RigidBody3D::mask_translation(RVec3 d) const noexcept -> RVec3 {
    if (has(PhysicsFlags::LockPositionX)) d.x = 0.0_r;
    if (has(PhysicsFlags::LockPositionY)) d.y = 0.0_r;
    if (has(PhysicsFlags::LockPositionZ)) d.z = 0.0_r;
    return d;
}

bool RigidBody3D::validate_frozen(FrozenProperty f) noexcept {
    int count = 0;
    if (has_frozen(f, FrozenProperty::Mass))    ++count;
    if (has_frozen(f, FrozenProperty::Density)) ++count;
    bool area_or_vol = has_frozen(f, FrozenProperty::Area) || has_frozen(f, FrozenProperty::Volume);
    if (area_or_vol) ++count;
    return count <= 2;
}

void RigidBody3D::set_inertia(const RMat3& I) const noexcept {
    if (fast::trace(I) > 0.0_r && !has(PhysicsFlags::LockRotation3D)) {
        m_inertia     = I;
        m_inv_inertia = fast::inverse(I);
    } else {
        m_inertia     = {};
        m_inv_inertia = {};
    }

    m_world_inertia_dirty = true;
}

void RigidBody3D::recompute_mass_data(Real gravity) const {
    m_mass = 0.0_r; m_inv_mass = 0.0_r;
    m_inertia = {}; m_inv_inertia = {};
    m_local_center = {};
    m_world_inertia_dirty = true;

    if (!has(PhysicsFlags::LinearForces) && !has(PhysicsFlags::LinearImpulses)) {
        m_sweep.center_local = {};
        return;
    }

    refresh_shape_volume();
    std::vector<MassData3D> parts;
    parts.reserve(m_colliders.size());

    for (auto& c : m_colliders) {
        if (c.is_sensor) continue;
        const Real volume  = compute_shape_volume(c.shape);
        const Real density = c.material.mass_spec.resolve_density(volume, gravity);
        MassData3D md = compute_mass(c.shape, density);
        md.center  = c.local_offset.apply(md.center);
        md.inertia = fast::rotate_inertia(md.inertia, c.local_offset.orientation);
        parts.push_back(md);
    }

    MassData3D total = MassData3D::combine(parts.data(), parts.size());

    if (total.mass > 0.0_r) {
        m_mass     = total.mass;
        m_inv_mass = 1.0_r / m_mass;
        m_local_center = total.center;
        set_inertia(total.inertia);
    } else {
        m_mass     = 1.0_r;
        m_inv_mass = 1.0_r;
        set_inertia({});
    }

    m_sweep.center_local = m_local_center;
    m_density = (m_shape_volume > 0.0_r) ? m_mass / m_shape_volume : 0.0_r;
}

void RigidBody3D::resolve_mass_constraint_from(FrozenProperty just_set) noexcept {
    const bool mass_frozen    = is_frozen(FrozenProperty::Mass);
    const bool density_frozen = is_frozen(FrozenProperty::Density);
    const bool volume_frozen  = is_frozen(FrozenProperty::Area) || is_frozen(FrozenProperty::Volume);

    if (just_set == FrozenProperty::Mass) {
        if (volume_frozen || !density_frozen) {
            m_density = (m_shape_volume > 0.0_r) ? m_mass / m_shape_volume : 0.0_r;
        } else {
            derive_volume_from_mass_density();
        }

        recompute_inertia_from_density();
    } else if (just_set == FrozenProperty::Density) {
        if (volume_frozen || !mass_frozen) {
            derive_mass_from_density_volume();
        } else {
            derive_volume_from_mass_density();
        }
    } else {
        if (density_frozen || !mass_frozen) {
            derive_mass_from_density_volume();
        } else {
            m_density = (m_shape_volume > 0.0_r) ? m_mass / m_shape_volume : 0.0_r;
            recompute_inertia_from_density();
        }
    }
}

void RigidBody3D::derive_mass_from_density_volume() noexcept {
    m_mass     = m_density * m_shape_volume;
    m_inv_mass = (m_mass > 0.0_r) ? 1.0_r / m_mass : 0.0_r;
    recompute_inertia_from_density();
}

void RigidBody3D::derive_volume_from_mass_density() noexcept {
    if (m_density <= 0.0_r || m_colliders.empty()) return;
    Real target  = m_mass / m_density;
    Real current = m_shape_volume;
    if (current <= 0.0_r) return;
    scale_collider_geometry(std::cbrt(target / current));
    refresh_shape_volume();
    recompute_inertia_from_density();
}

void RigidBody3D::scale_collider_geometry(Real s) {
    for (auto& c : m_colliders) {
        switch (c.shape.type) {
            case ShapeType3D::Sphere:
                c.shape.sphere.center = c.shape.sphere.center * s;
                c.shape.sphere.radius *= s;
                break;
            case ShapeType3D::Capsule:
                c.shape.capsule.point1 = c.shape.capsule.point1 * s;
                c.shape.capsule.point2 = c.shape.capsule.point2 * s;
                c.shape.capsule.radius *= s;
                break;
            case ShapeType3D::Hull:
                if (c.shape.hull) {
                    HullShape h = *c.shape.hull;
                    h.scale(s);
                    c.shape.hull = std::make_shared<const HullShape>(std::move(h));
                }
                break;
            case ShapeType3D::Plane:
                break;
            case ShapeType3D::Cylinder:
                c.shape.cylinder.half_height *= s;
                c.shape.cylinder.radius *= s;
                break;
            case ShapeType3D::Cone:
                c.shape.cone.half_height *= s;
                c.shape.cone.radius *= s;
                break;
            case ShapeType3D::RoundedBox:
                c.shape.rounded_box.half_extents = c.shape.rounded_box.half_extents * s;
                c.shape.rounded_box.radius *= s;
                break;
            case ShapeType3D::Heightfield:
                if (c.shape.heightfield) {
                    HeightfieldShape h = *c.shape.heightfield;
                    for (Real& v : h.heights) v *= s;
                    h.cell_x *= s;
                    h.cell_z *= s;
                    h.finalize();
                    c.shape.heightfield = std::make_shared<const HeightfieldShape>(std::move(h));
                }
                break;
            case ShapeType3D::Mesh:
                if (c.shape.mesh) {
                    TriangleMeshShape m = *c.shape.mesh;
                    for (auto& v : m.vertices) v = v * s;
                    m.build();
                    c.shape.mesh = std::make_shared<const TriangleMeshShape>(std::move(m));
                }
                break;
        }

        c.shape.rebuild_bounds();
    }
}

void RigidBody3D::refresh_shape_volume() const noexcept {
    m_shape_volume = 0.0_r;
    for (const auto& c : m_colliders) { if (!c.is_sensor) m_shape_volume += compute_shape_volume(c.shape); }
}

void RigidBody3D::recompute_inertia_from_density() noexcept {
    std::vector<MassData3D> parts;
    parts.reserve(m_colliders.size());

    for (auto& c : m_colliders) {
        if (c.is_sensor) continue;
        MassData3D md = compute_mass(c.shape, m_density);
        md.center  = c.local_offset.apply(md.center);
        md.inertia = fast::rotate_inertia(md.inertia, c.local_offset.orientation);
        parts.push_back(md);
    }

    MassData3D combined = MassData3D::combine(parts.data(), parts.size());
    m_local_center = combined.center;
    set_inertia(combined.inertia);
    m_sweep.center_local = m_local_center;
}

void* RigidBody3D::operator new(std::size_t size) {
    if (size != sizeof(RigidBody3D)) return ::operator new(size);
    return detail::ObjectPool<sizeof(RigidBody3D), alignof(RigidBody3D)>::instance().allocate();
}

void RigidBody3D::operator delete(void* p, std::size_t size) noexcept {
    if (size != sizeof(RigidBody3D)) {
        ::operator delete(p);
        return;
    }
    detail::ObjectPool<sizeof(RigidBody3D), alignof(RigidBody3D)>::instance().release(p);
}

std::size_t RigidBody3D::pooled_bodies() {
    return detail::ObjectPool<sizeof(RigidBody3D), alignof(RigidBody3D)>::instance().live();
}

} // namespace physics
} // namespace fizmo
