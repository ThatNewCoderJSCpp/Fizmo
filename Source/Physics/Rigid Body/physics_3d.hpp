#ifndef FIZMO_PHYSICS_3D_HPP
#define FIZMO_PHYSICS_3D_HPP

#include "common.hpp"

namespace fizmo {
namespace physics {

namespace vec3 {

inline Real dot(const RVec3& a, const RVec3& b) noexcept { return a.x * b.x + a.y * b.y + a.z * b.z; }

inline RVec3 cross(const RVec3& a, const RVec3& b) noexcept {
    return { a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x };
}

inline Real length_squared(const RVec3& a) noexcept { return dot(a, a); }
inline Real length(const RVec3& a) noexcept { return std::sqrt(dot(a, a)); }

inline RVec3 normalize(const RVec3& a, const RVec3& fallback = RVec3{0.0_r, 1.0_r, 0.0_r}) noexcept {
    Real l = length(a);
    return l > constants::middle_epsilon() ? a * (1.0_r / l) : fallback;
}

inline RVec3 negate(const RVec3& a) noexcept { return a * -1.0_r; }
inline bool is_zero(const RVec3& a) noexcept { return a.x == 0.0_r && a.y == 0.0_r && a.z == 0.0_r; }

inline RVec3 min(const RVec3& a, const RVec3& b) noexcept { return { std::min(a.x, b.x), std::min(a.y, b.y), std::min(a.z, b.z) }; }
inline RVec3 max(const RVec3& a, const RVec3& b) noexcept { return { std::max(a.x, b.x), std::max(a.y, b.y), std::max(a.z, b.z) }; }

inline Real component(const RVec3& v, int axis) noexcept { return axis == 0 ? v.x : (axis == 1 ? v.y : v.z); }

RVec3 mul(const RMat3& m, const RVec3& v) noexcept;

RVec3 mul_transpose(const RMat3& m, const RVec3& v) noexcept;

inline RMat3 outer(const RVec3& a, const RVec3& b) noexcept {
    return { a.x * b.x, a.x * b.y, a.x * b.z,
             a.y * b.x, a.y * b.y, a.y * b.z,
             a.z * b.x, a.z * b.y, a.z * b.z };
}

void orthonormal_basis(const RVec3& n, RVec3& t1, RVec3& t2) noexcept;

RQuat integrate_rotation(const RQuat& q, const RVec3& w, Real dt) noexcept;

} // namespace vec3

struct AABB3D {
    RVec3 min{}; // meters
    RVec3 max{}; // meters

    constexpr AABB3D() noexcept = default;
    constexpr AABB3D(const RVec3& lo, const RVec3& hi) noexcept : min(lo), max(hi) {}

    RVec3 center()  const noexcept { return (min + max) * 0.5_r; }
    RVec3 extents() const noexcept { return (max - min) * 0.5_r; }

    Real surface_area() const noexcept {
        Real w = max.x - min.x, h = max.y - min.y, d = max.z - min.z;
        return 2.0_r * (w * h + h * d + d * w);
    }

    Real volume() const noexcept { return (max.x - min.x) * (max.y - min.y) * (max.z - min.z); }

    bool overlaps(const AABB3D& o) const noexcept;

    bool contains(const RVec3& p) const noexcept {
        return p.x >= min.x && p.x <= max.x && p.y >= min.y && p.y <= max.y && p.z >= min.z && p.z <= max.z;
    }

    bool contains(const AABB3D& o) const noexcept;

    // margin in meters
    AABB3D fatten(Real margin) const noexcept;

    static AABB3D merge(const AABB3D& a, const AABB3D& b) noexcept { return { vec3::min(a.min, b.min), vec3::max(a.max, b.max) }; }
};

struct Transform3D {
    RVec3       position{};                              // meters
    RQuat          orientation{1.0_r, 0.0_r, 0.0_r, 0.0_r};         // unit quaternion
    RMat3 rotation = RMat3::identity();   // cached from orientation

    Transform3D() noexcept = default;

    Transform3D(const RVec3& pos, const RQuat& q) noexcept
        : position(pos), orientation(fast::normalize(q)), rotation(fast::rotation_matrix(orientation)) {}

    static Transform3D identity() noexcept { return {}; }

    RVec3 apply(const RVec3& local) const noexcept { return vec3::mul(rotation, local) + position; }
    RVec3 apply_inverse(const RVec3& world) const noexcept { return vec3::mul_transpose(rotation, world - position); }
    RVec3 rotate(const RVec3& dir) const noexcept { return vec3::mul(rotation, dir); }
    RVec3 rotate_inverse(const RVec3& dir) const noexcept { return vec3::mul_transpose(rotation, dir); }

    Transform3D compose(const Transform3D& other) const noexcept {
        return Transform3D(apply(other.position), fast::mul(orientation, other.orientation));
    }

    void set_orientation(const RQuat& q) noexcept {
        orientation = fast::normalize(q);
        rotation = fast::rotation_matrix(orientation);
    }
};

struct Sweep3D {
    RVec3 center_local{}; // meters
    RVec3 position0{};    // meters (center of mass)
    RVec3 position1{};    // meters (center of mass)
    RQuat    orientation0{1.0_r, 0.0_r, 0.0_r, 0.0_r};
    RQuat    orientation1{1.0_r, 0.0_r, 0.0_r, 0.0_r};

    Transform3D get_transform(Real t) const noexcept;
};

struct SphereShape {
    RVec3 center{};   // meters, local body space
    Real radius = 0.0_r; // meters
};

struct CapsuleShape3D {
    RVec3 point1{};   // meters, local
    RVec3 point2{};   // meters, local
    Real radius = 0.0_r; // meters
    RVec3 center() const noexcept { return (point1 + point2) * 0.5_r; }
    Real half_length() const noexcept { return vec3::length(point2 - point1) * 0.5_r; } // m
};

struct PlaneShape {
    RVec3 normal{0.0_r, 1.0_r, 0.0_r}; // unit, points out of the solid
    Real   offset = 0.0_r;          // meters
};

struct HullFace {
    RVec3 normal{};                  
    Real   offset = 0.0_r;              
    std::vector<std::uint16_t> indices; // counter-clockwise when viewed from outside
};

struct HullShape {
    std::vector<RVec3> vertices;                    // meters, local
    std::vector<HullFace> faces;
    std::vector<std::array<std::uint16_t, 2>> edges;   // unique edges
    std::vector<RVec3> edge_directions;             // unique (up to sign) unit edge directions
    RVec3 centroid{};                               // volume centroid (m, local)
    Real   volume = 0.0_r;                             // m3

    bool valid() const noexcept { return faces.size() >= 4 && volume > 0.0_r; }

    static HullShape box(Real hx, Real hy, Real hz);

    static HullShape from_points(const RVec3* pts, std::size_t n);

    static HullShape from_points(const std::vector<RVec3>& pts) { return from_points(pts.data(), pts.size()); }

    void finalize();

    void scale(Real s) {
        for (auto& v : vertices) v = v * s;
        finalize();
    }

    AABB3D bounds() const noexcept;
};

struct CylinderShape {
    Real half_height = 0.5_r;
    Real radius = 0.5_r;
};

struct ConeShape {
    Real half_height = 0.5_r;
    Real radius = 0.5_r;
};

struct RoundedBoxShape {
    RVec3 half_extents{0.5_r, 0.5_r, 0.5_r};
    Real radius = 0.05_r;
};

struct HeightfieldShape {
    std::vector<Real> heights;
    int columns = 0;
    int rows = 0;
    Real cell_x = 1.0_r;
    Real cell_z = 1.0_r;
    Real min_height = 0.0_r;
    Real max_height = 0.0_r;

    bool valid() const noexcept { return columns >= 2 && rows >= 2 && heights.size() == static_cast<std::size_t>(columns) * static_cast<std::size_t>(rows); }
    Real width() const noexcept { return cell_x * (columns - 1); }
    Real depth() const noexcept { return cell_z * (rows - 1); }
    Real height_at_vertex(int c, int r) const noexcept { return heights[static_cast<std::size_t>(r) * static_cast<std::size_t>(columns) + static_cast<std::size_t>(c)]; }
    RVec3 vertex(int c, int r) const noexcept { return { c * cell_x - 0.5_r * width(), height_at_vertex(c, r), r * cell_z - 0.5_r * depth() }; }
    Real height_at(Real x, Real z) const noexcept;
    void finalize();
    AABB3D bounds() const noexcept;

    template <class Fn>
    void for_each_triangle(const AABB3D& region, Fn&& fn) const {
        if (!valid()) return;
        const Real x0 = -0.5_r * width();
        const Real z0 = -0.5_r * depth();
        int c0 = static_cast<int>(std::floor((region.min.x - x0) / cell_x));
        int c1 = static_cast<int>(std::floor((region.max.x - x0) / cell_x));
        int r0 = static_cast<int>(std::floor((region.min.z - z0) / cell_z));
        int r1 = static_cast<int>(std::floor((region.max.z - z0) / cell_z));
        c0 = std::max(c0, 0);
        r0 = std::max(r0, 0);
        c1 = std::min(c1, columns - 2);
        r1 = std::min(r1, rows - 2);
        if (region.max.y < min_height || region.min.y > max_height) return;
        for (int r = r0; r <= r1; ++r) {
            for (int c = c0; c <= c1; ++c) {
                const RVec3 a = vertex(c, r), b = vertex(c + 1, r), d = vertex(c, r + 1), e = vertex(c + 1, r + 1);
                const std::uint32_t base = static_cast<std::uint32_t>((r * (columns - 1) + c) * 2);
                if (!fn(a, d, b, base)) return;
                if (!fn(b, d, e, base + 1)) return;
            }
        }
    }
};

struct TriangleMeshShape {
    struct Node {
        AABB3D box{};
        std::int32_t left = -1;
        std::int32_t right = -1;
        std::uint32_t first = 0;
        std::uint32_t count = 0;
    };

    std::vector<RVec3> vertices;
    std::vector<std::array<std::uint32_t, 3>> triangles;
    std::vector<RVec3> normals;
    std::vector<Node> nodes;
    std::vector<std::uint32_t> order;
    bool two_sided = true;

    bool valid() const noexcept { return !triangles.empty() && !nodes.empty(); }
    void build();
    AABB3D bounds() const noexcept { return nodes.empty() ? AABB3D{} : nodes[0].box; }

    template <class Fn>
    void for_each_triangle(const AABB3D& region, Fn&& fn) const {
        if (nodes.empty()) return;
        std::int32_t stack[64];
        int sp = 0;
        stack[sp++] = 0;
        while (sp > 0) {
            const Node& n = nodes[static_cast<std::size_t>(stack[--sp])];
            if (!n.box.overlaps(region)) continue;
            if (n.left < 0) {
                for (std::uint32_t i = 0; i < n.count; ++i) {
                    const std::uint32_t t = order[n.first + i];
                    const auto& tri = triangles[t];
                    if (!fn(vertices[tri[0]], vertices[tri[1]], vertices[tri[2]], t)) return;
                }
            } else if (sp < 62) {
                stack[sp++] = n.left;
                stack[sp++] = n.right;
            }
        }
    }

    bool raycast(const RVec3& origin, const RVec3& dir, Real max_t, Real& t, RVec3& normal, std::uint32_t& triangle) const;
};

struct SphereSection3D {
    Real radius = 0.5_r;
    Real polar_min = 0.0_r;
    Real polar_max = 180.0_r;
    Real azimuth_min = 0.0_r;
    Real azimuth_max = 360.0_r;
    Real inner_radius = 0.0_r;
    Real height_min = -std::numeric_limits<Real>::infinity();
    Real height_max = std::numeric_limits<Real>::infinity();

    static SphereSection3D sphere(Real r);
    static SphereSection3D hemisphere(Real r);
    static SphereSection3D dome(Real r, Real polar_degrees);
    static SphereSection3D cap(Real r, Real height);
    static SphereSection3D wedge(Real r, Real azimuth_degrees);
    static SphereSection3D shell(Real r, Real thickness);
    static SphereSection3D bowl(Real r, Real thickness);

    bool is_full_sphere() const noexcept;
    bool is_convex() const noexcept;
    Real exact_volume() const noexcept;
};

enum class ShapeType3D : std::uint8_t {
    Sphere = 0,
    Capsule,
    Hull,
    Plane,
    Cylinder,
    Cone,
    RoundedBox,
    Heightfield,
    Mesh
};

struct Shape3D {
    ShapeType3D type = ShapeType3D::Sphere;

    SphereShape                      sphere{};
    CapsuleShape3D                   capsule{};
    std::shared_ptr<const HullShape> hull;     
    PlaneShape                       plane{};
    CylinderShape                    cylinder{};
    ConeShape                        cone{};
    RoundedBoxShape                  rounded_box{};
    std::shared_ptr<const HeightfieldShape>  heightfield;
    std::shared_ptr<const TriangleMeshShape> mesh;

    Real bounding_radius = 0.0_r; // meters (infinite for planes)
    AABB3D local_aabb{};

    static Shape3D make_sphere(const RVec3& center, Real radius);

    static Shape3D make_capsule(const RVec3& p1, const RVec3& p2, Real radius);

    static Shape3D make_hull(HullShape h);

    static Shape3D make_box(Real hx, Real hy, Real hz) { return make_hull(HullShape::box(hx, hy, hz)); } // meter half-extents
    static Shape3D make_convex(const RVec3* pts, std::size_t n) { return make_hull(HullShape::from_points(pts, n)); }
    static Shape3D make_convex(const std::vector<RVec3>& pts) { return make_hull(HullShape::from_points(pts)); }

    static Shape3D make_plane(const RVec3& normal, Real offset);
    static Shape3D make_cylinder(Real half_height, Real radius);
    static Shape3D make_cone(Real half_height, Real radius);
    static Shape3D make_rounded_box(Real hx, Real hy, Real hz, Real radius);
    static Shape3D make_rounded_box_core(Real core_hx, Real core_hy, Real core_hz, Real radius);
    static Shape3D make_heightfield(HeightfieldShape h);
    static Shape3D make_heightfield(std::vector<Real> heights, int columns, int rows, Real cell_x, Real cell_z);
    static Shape3D make_mesh(TriangleMeshShape m);
    static Shape3D make_mesh(std::vector<RVec3> vertices, std::vector<std::array<std::uint32_t, 3>> triangles, bool two_sided = true);
    static Shape3D make_sphere_section(const SphereSection3D& section, int detail = 24);
    static std::vector<Shape3D> make_sphere_section_parts(const SphereSection3D& section, int detail = 24);
    static Shape3D make_hemisphere(Real radius, int detail = 24) { return make_sphere_section(SphereSection3D::hemisphere(radius), detail); }

    bool is_convex() const noexcept { return type != ShapeType3D::Plane && type != ShapeType3D::Heightfield && type != ShapeType3D::Mesh; }
    bool is_static_only() const noexcept { return type == ShapeType3D::Plane || type == ShapeType3D::Heightfield || type == ShapeType3D::Mesh; }

    void rebuild_bounds();

    AABB3D compute_aabb(const Transform3D& xf) const noexcept;

    static constexpr Real PLANE_EXTENT = 1e15_r; // meters, stands in for infinity in plane AABBs
};

std::vector<std::array<RVec3, 3>> sphere_section_triangles(const SphereSection3D& section, int detail = 24);

std::vector<RVec3> sphere_section_points(const SphereSection3D& section, int detail = 24);

struct MassData3D {
    Real         mass = 0.0_r; // kg
    RMat3 inertia{};  // kg*m2, about center, local axes
    RVec3       center{};   // meters, local body space

    static MassData3D combine(const MassData3D* parts, std::size_t n);
};

namespace detail {

inline Real sphere_volume(const SphereShape& s) { return 4.0_r / 3.0_r * static_cast<Real>(constants::pi()) * s.radius * s.radius * s.radius; }

Real capsule_volume(const CapsuleShape3D& c);

MassData3D compute_sphere_mass(const SphereShape& s, Real density);

MassData3D compute_capsule_mass(const CapsuleShape3D& c, Real density);

MassData3D compute_hull_mass(const HullShape& h, Real density);

template <class FromU, class ToU>
inline typename std::enable_if<is_fizmo_unit_v<FromU> && is_fizmo_unit_v<ToU>, RVec3>::type
convert_velocity_vec(const RVec3& v, FromU from, ToU to) {
    return { convert_velocity_scalar(v.x, from, to), convert_velocity_scalar(v.y, from, to), convert_velocity_scalar(v.z, from, to) };
}

template <class FromU, class ToU>
inline typename std::enable_if<is_fizmo_unit_v<FromU> && is_fizmo_unit_v<ToU>, RVec3>::type
convert_angular_velocity_vec(const RVec3& v, FromU from, ToU to) {
    return { convert_angular_velocity_scalar(v.x, from, to), convert_angular_velocity_scalar(v.y, from, to), convert_angular_velocity_scalar(v.z, from, to) };
}

template <class FromU, class ToU>
inline typename std::enable_if<is_fizmo_unit_v<FromU> && is_fizmo_unit_v<ToU>, RVec3>::type
convert_force_vec(const RVec3& v, FromU from, ToU to) {
    return { convert_force_scalar(v.x, from, to), convert_force_scalar(v.y, from, to), convert_force_scalar(v.z, from, to) };
}

template <class FromU, class ToU>
inline typename std::enable_if<is_fizmo_unit_v<FromU> && is_fizmo_unit_v<ToU>, RVec3>::type
convert_torque_vec(const RVec3& v, FromU from, ToU to) {
    return { convert_torque_scalar(v.x, from, to), convert_torque_scalar(v.y, from, to), convert_torque_scalar(v.z, from, to) };
}

template <class FromU, class ToU>
inline typename std::enable_if<is_fizmo_unit_v<FromU> && is_fizmo_unit_v<ToU>, RVec3>::type
convert_position_vec(const RVec3& v, FromU from, ToU to) {
    return { convert_distance_scalar(v.x, from, to), convert_distance_scalar(v.y, from, to), convert_distance_scalar(v.z, from, to) };
}

} // namespace detail

Real compute_shape_volume(const Shape3D& shape);

MassData3D compute_mass(const Shape3D& shape, Real density);

struct MassSpec3D {
public:
    static constexpr Real STANDARD_GRAVITY = constants::gravity(); // m/s2

public:
    MassMode mode  = MassMode::Density;
    Real   value = 999.9749_r; // kg/m3 by default (water)

public:
    constexpr MassSpec3D() noexcept = default;
    constexpr MassSpec3D(MassMode m, Real v) noexcept : mode(m), value(v) {}

public:
    static constexpr MassSpec3D from_density(Real kg_per_m3) noexcept { return { MassMode::Density, kg_per_m3 }; }
    static constexpr MassSpec3D from_mass(Real kg) noexcept { return { MassMode::Mass, kg }; }
    static constexpr MassSpec3D from_weight(Real newtons) noexcept { return { MassMode::Weight, newtons }; }

    template <class U, typename std::enable_if<is_fizmo_unit_v<U> && U::dimension() == units::DIMENSION_DENSITY_3D, int>::type = 0>
    static MassSpec3D from_density(Real val, U unit) { return { MassMode::Density, detail::convert_weight_scalar(val, unit, (units::mass::kilogram / units::volume::cubic_meter)) }; }

    template <class U, typename std::enable_if<is_fizmo_unit_v<U> && U::dimension() == units::DIMENSION_MASS, int>::type = 0>
    static MassSpec3D from_mass(Real val, U unit)    { return { MassMode::Mass   , detail::convert_weight_scalar(val, unit, units::mass::kilogram) }; }

    template <class U, typename std::enable_if<is_fizmo_unit_v<U> && U::dimension() == units::DIMENSION_FORCE, int>::type = 0>
    static MassSpec3D from_weight(Real val, U unit)  { return { MassMode::Weight , detail::convert_force_scalar (val, unit, units::force::newton ) }; }

public:
    Real resolve_density(Real shape_volume_m3, Real gravity = STANDARD_GRAVITY) const noexcept;

    Real to_mass(Real shape_volume_m3, Real gravity = STANDARD_GRAVITY) const noexcept {
        return resolve_density(shape_volume_m3, gravity) * shape_volume_m3;
    }
};

struct Material3D {
    MassSpec3D mass_spec{};
    Real     friction    = 0.3_r;           // unitless [0, 1]
    Real     restitution = 0.0_r;           // unitless [0, 1]
    Real     restitution_threshold = 1.0_r; // m/s
    CombineMode friction_combine = CombineMode::Default;
    CombineMode restitution_combine = CombineMode::Default;
    Real     rolling_friction  = 0.0_r;
    Real     spinning_friction = 0.0_r;
    Real     anisotropic_x = 1.0_r;
    Real     anisotropic_y = 1.0_r;
    Real     anisotropic_z = 1.0_r;

    constexpr Material3D() noexcept = default;
    explicit constexpr Material3D(Real density) noexcept : mass_spec(MassSpec3D::from_density(density)) {}
    constexpr Material3D(const MassSpec3D& spec, Real fric = 0.3_r, Real rest = 0.0_r, Real rest_thresh = 1.0_r) noexcept : mass_spec(spec), friction(fric), restitution(rest), restitution_threshold(rest_thresh) {}

    static constexpr Material3D with_density(Real kg_per_m3) noexcept { return Material3D(MassSpec3D::from_density(kg_per_m3)); }
    static constexpr Material3D with_mass(Real kg) noexcept { return Material3D(MassSpec3D::from_mass(kg)); }
    static constexpr Material3D with_weight(Real newtons) noexcept { return Material3D(MassSpec3D::from_weight(newtons)); }

    template <class U, typename std::enable_if<is_fizmo_unit_v<U> && U::dimension() == units::DIMENSION_DENSITY_3D, int>::type = 0>
    static Material3D with_density(Real val, U unit) { return Material3D(MassSpec3D::from_density(val, unit)); }

    template <class U, typename std::enable_if<is_fizmo_unit_v<U> && U::dimension() == units::DIMENSION_MASS, int>::type = 0>
    static Material3D with_mass(Real val, U unit) { return Material3D(MassSpec3D::from_mass(val, unit)); }

    template <class U, typename std::enable_if<is_fizmo_unit_v<U> && U::dimension() == units::DIMENSION_FORCE, int>::type = 0>
    static Material3D with_weight(Real val, U unit) { return Material3D(MassSpec3D::from_weight(val, unit)); }
};

class RigidBody3D;

struct Collider3D {
    Shape3D         shape;
    Transform3D     local_offset;
    Material3D      material;
    CollisionFilter filter;
    bool            is_sensor = false;
    RigidBody3D*    body = nullptr;

    AABB3D          world_aabb{};
    MassData3D      mass_data{};

    void recompute_mass(Real gravity = MassSpec3D::STANDARD_GRAVITY);

    void update_world_aabb(const Transform3D& body_xf);
};

class Simulation3D;

class RigidBody3D {
    friend class Simulation3D;

public:
    RigidBody3D() = default;
    explicit RigidBody3D(PhysicsFlags preset) : m_flags(preset) {}

public:
    PhysicsFlags flags() const noexcept { return m_flags; }

    const Transform3D& transform() const noexcept { return m_transform; }
          Transform3D& transform()       noexcept { m_world_inertia_dirty = true; return m_transform; }

    const Sweep3D& sweep() const noexcept { return m_sweep; }
          Sweep3D& sweep()       noexcept { return m_sweep; }

    const std::vector<Collider3D>& colliders() const noexcept { return m_colliders; }

    // Caller should call mark_mass_dirty() after mutating
    std::vector<Collider3D>& colliders_mut() noexcept { return m_colliders; }

    std::uint64_t collision_layer() const noexcept { return m_collision_layer; }
    std::uint64_t collision_mask()  const noexcept { return m_collision_mask; }

    void set_collision_layer(std::uint64_t layer) noexcept { m_collision_layer = layer; }
    void set_collision_mask(std::uint64_t mask)   noexcept { m_collision_mask = mask; }

    void set_collision_filter(std::uint64_t layer, std::uint64_t mask) noexcept {
        m_collision_layer = layer;
        m_collision_mask  = mask;
    }

    static void* operator new(std::size_t size);
    static void operator delete(void* p, std::size_t size) noexcept;
    static std::size_t pooled_bodies();
    BodyHandle handle() const noexcept { return m_handle; }

    std::uint64_t id() const noexcept { return m_id; }
    void set_id(std::uint64_t id) noexcept { m_id = id; }

    BodyQuality quality() const noexcept { return m_quality; }
    void set_quality(BodyQuality q) noexcept { m_quality = q; }
    int velocity_iterations() const noexcept { return m_velocity_iterations; }
    void set_velocity_iterations(int n) noexcept { m_velocity_iterations = std::max(0, n); }
    int position_iterations() const noexcept { return m_position_iterations; }
    void set_position_iterations(int n) noexcept { m_position_iterations = std::max(0, n); }
    ContinuousMode continuous_mode() const noexcept { return m_continuous; }
    void set_continuous_mode(ContinuousMode m) noexcept { m_continuous = m; }
    Real linear_sleep_tolerance() const noexcept { return m_linear_sleep_tolerance; }
    Real angular_sleep_tolerance() const noexcept { return m_angular_sleep_tolerance; }
    Real sleep_time() const noexcept { return m_sleep_time; }
    void set_sleep_tolerances(Real linear, Real angular) noexcept {
        m_linear_sleep_tolerance = std::max(0.0_r, linear);
        m_angular_sleep_tolerance = std::max(0.0_r, angular);
    }
    void set_sleep_time(Real seconds) noexcept { m_sleep_time = std::max(0.0_r, seconds); }
    bool collides_with_layers(std::uint64_t layers) const noexcept { return (m_collision_mask & layers) != 0; }

    FrozenProperty frozen_properties() const noexcept { return m_frozen; }

    // Return false if the freeze would over-constrain
    bool freeze(FrozenProperty prop) noexcept;

public:
    bool has(PhysicsFlags f)                    const noexcept { return has_flag(m_flags, f); }
    void set_flag(PhysicsFlags f)                     noexcept { m_flags |= f;  m_world_inertia_dirty = true; }
    void clear_flag(PhysicsFlags f)                   noexcept { m_flags &= ~f; m_world_inertia_dirty = true; }
    void toggle_flag(PhysicsFlags f)                  noexcept { m_flags ^= f;  m_world_inertia_dirty = true; }
    void set_flag_to(PhysicsFlags f, bool on)         noexcept { on ? set_flag(f) : clear_flag(f); }

    bool is_frozen(FrozenProperty prop) const noexcept { return has_frozen(m_frozen, prop); }
    void unfreeze(FrozenProperty prop)        noexcept { m_frozen &= ~prop; }
    void unfreeze_all()                       noexcept { m_frozen = FrozenProperty::None; }

public:
    // Call after externally mutating colliders, materials, offsets, or flags
    void mark_mass_dirty() const noexcept { m_mass_dirty = true; }

    bool can_collide_with(const RigidBody3D& other) const noexcept;

    bool is_mass_dirty() const noexcept { return m_mass_dirty; }

    void ensure_mass_uptodate(Real gravity = MassSpec3D::STANDARD_GRAVITY) const {
        if (!m_mass_dirty) return;
        recompute_mass_data(gravity);
        m_mass_dirty = false;
    }

public:
    void apply_preset(PhysicsFlags preset) noexcept {
        m_flags = preset;
        recompute_mass_data();
        m_mass_dirty = false;
    }

    Collider3D& add_collider(
        Shape3D shape,
        Material3D mat = {},
        Transform3D offset = {},
        CollisionFilter filt = {},
        bool sensor = false
    );

public:
    RVec3 position() const noexcept { return m_transform.position; }

    template <class U, typename std::enable_if<is_fizmo_unit_v<U> && U::dimension() == units::DIMENSION_LENGTH, int>::type = 0>
    RVec3 position(U distance_unit) const noexcept {
        return detail::convert_position_vec(m_transform.position, units::distance::meter, distance_unit);
    }

    void set_position(const RVec3& pos) noexcept { m_transform.position = pos; }

    template <class U, typename std::enable_if<is_fizmo_unit_v<U> && U::dimension() == units::DIMENSION_LENGTH, int>::type = 0>
    void set_position(const RVec3& pos, U distance_unit) noexcept {
        m_transform.position = detail::convert_position_vec(pos, distance_unit, units::distance::meter);
    }

public:
    const RQuat& orientation() const noexcept { return m_transform.orientation; }

    void set_orientation(const RQuat& q) noexcept {
        m_transform.set_orientation(q);
        m_world_inertia_dirty = true;
    }

    void set_rotation(const RVec3& axis, Real angle, bool in_rad = true) noexcept;

    // {roll (about X), pitch (about Y), yaw (about Z)}
    std::array<Real, 3> euler_angles(bool in_rad = true) const noexcept;

    void set_euler_angles(Real roll, Real pitch, Real yaw, bool in_rad = true) noexcept;

    std::array<Real, 3> euler_xyz(bool in_rad = true) const noexcept { return euler_angles(in_rad); }

    void set_euler_xyz(Real about_x, Real about_y, Real about_z, bool in_rad = true) noexcept { set_euler_angles(about_x, about_y, about_z, in_rad); }

    void set_yaw_pitch_roll(Real yaw, Real pitch, Real roll, bool in_rad = true) noexcept;

    std::array<Real, 3> yaw_pitch_roll(bool in_rad = true) const noexcept;

    void set_yaw_pitch_roll(const Axes3D& axes, Real yaw, Real pitch, Real roll, bool in_rad = true) noexcept { set_orientation(to_real(axes.from_yaw_pitch_roll(yaw, pitch, roll, !in_rad))); }

    std::array<Real, 3> yaw_pitch_roll(const Axes3D& axes, bool in_rad = true) const noexcept {
        const auto a = axes.to_yaw_pitch_roll(to_double(m_transform.orientation), !in_rad);
        return { static_cast<Real>(a[0]), static_cast<Real>(a[1]), static_cast<Real>(a[2]) };
    }

public:
    RVec3 linear_velocity() const noexcept { return m_linear_velocity; }

    template <class U, typename std::enable_if<is_fizmo_unit_v<U> && U::dimension() == units::DIMENSION_VELOCITY, int>::type = 0>
    RVec3 linear_velocity(U unit) const noexcept {
        return detail::convert_velocity_vec(m_linear_velocity, units::velocity::meters_per_second, unit);
    }

    void set_linear_velocity(const RVec3& v) noexcept {
        m_linear_velocity = v;
        if (!vec3::is_zero(v)) wake();
    }

    template <class U, typename std::enable_if<is_fizmo_unit_v<U> && U::dimension() == units::DIMENSION_VELOCITY, int>::type = 0>
    void set_linear_velocity(const RVec3& v, U unit) noexcept {
        set_linear_velocity(detail::convert_velocity_vec(v, unit, units::velocity::meters_per_second));
    }

public:
    // World-space angular velocity (rad/s); its direction is the spin axis.
    RVec3 angular_velocity() const noexcept { return m_angular_velocity; }

    template <class U, typename std::enable_if<is_fizmo_unit_v<U> && U::dimension() == units::DIMENSION_ANGULAR_VELOCITY, int>::type = 0>
    RVec3 angular_velocity(U unit) const noexcept {
        static constexpr auto rad_per_s = units::angle::radian / units::time::second;
        return detail::convert_angular_velocity_vec(m_angular_velocity, rad_per_s, unit);
    }

    void set_angular_velocity(const RVec3& w) noexcept {
        m_angular_velocity = w;
        if (!vec3::is_zero(w)) wake();
    }

    template <class U, typename std::enable_if<is_fizmo_unit_v<U> && U::dimension() == units::DIMENSION_ANGULAR_VELOCITY, int>::type = 0>
    void set_angular_velocity(const RVec3& w, U unit) noexcept {
        static constexpr auto rad_per_s = units::angle::radian / units::time::second;
        set_angular_velocity(detail::convert_angular_velocity_vec(w, unit, rad_per_s));
    }

public:
    RVec3 force() const noexcept { return m_force; }

    template <class U, typename std::enable_if<is_fizmo_unit_v<U> && U::dimension() == units::DIMENSION_FORCE, int>::type = 0>
    RVec3 force(U force_unit) const noexcept {
        return detail::convert_force_vec(m_force, units::force::newton, force_unit);
    }

    void set_force(const RVec3& f) noexcept { m_force = f; }

    template <class U, typename std::enable_if<is_fizmo_unit_v<U> && U::dimension() == units::DIMENSION_FORCE, int>::type = 0>
    void set_force(const RVec3& f, U unit) noexcept {
        m_force = detail::convert_force_vec(f, unit, units::force::newton);
    }

public:
    RVec3 torque() const noexcept { return m_torque; }

    template <class U, typename std::enable_if<is_fizmo_unit_v<U> && U::dimension() == units::DIMENSION_TORQUE, int>::type = 0>
    RVec3 torque(U unit) const noexcept {
        return detail::convert_torque_vec(m_torque, units::torque::newton_meter, unit);
    }

    void set_torque(const RVec3& t) noexcept { m_torque = t; }

    template <class U, typename std::enable_if<is_fizmo_unit_v<U> && U::dimension() == units::DIMENSION_TORQUE, int>::type = 0>
    void set_torque(const RVec3& t, U torque_unit) noexcept {
        m_torque = detail::convert_torque_vec(t, torque_unit, units::torque::newton_meter);
    }

public:
    Real density() const noexcept {
        ensure_mass_uptodate();
        return m_density;
    }

    template <class U, typename std::enable_if<is_fizmo_unit_v<U> && U::dimension() == units::DIMENSION_DENSITY_3D, int>::type = 0>
    Real density(U density_unit) const noexcept {
        ensure_mass_uptodate();
        static constexpr auto si_density = units::mass::kilogram / units::volume::cubic_meter;
        return static_cast<Real>(units::convert(static_cast<long double>(m_density), si_density, density_unit));
    }

    void set_density(Real d) noexcept;

    template <class U, typename std::enable_if<is_fizmo_unit_v<U> && U::dimension() == units::DIMENSION_DENSITY_3D, int>::type = 0>
    void set_density(Real d, U density_unit) noexcept {
        static constexpr auto si_density = units::mass::kilogram / units::volume::cubic_meter;
        set_density(static_cast<Real>(units::convert(static_cast<long double>(d), density_unit, si_density)));
    }

public:
    Real mass(Real gravity = MassSpec3D::STANDARD_GRAVITY) const noexcept {
        ensure_mass_uptodate(gravity);
        return m_mass;
    }

    template <class U, typename std::enable_if<is_fizmo_unit_v<U> && U::dimension() == units::DIMENSION_MASS, int>::type = 0>
    Real mass(U mass_unit, Real gravity = MassSpec3D::STANDARD_GRAVITY) const noexcept {
        ensure_mass_uptodate(gravity);
        return detail::convert_weight_scalar(m_mass, units::mass::kilogram, mass_unit);
    }

    Real inv_mass(Real gravity = MassSpec3D::STANDARD_GRAVITY) const noexcept {
        ensure_mass_uptodate(gravity);
        return m_inv_mass;
    }

    void set_mass(Real kg) noexcept;

    template <class U, typename std::enable_if<is_fizmo_unit_v<U> && U::dimension() == units::DIMENSION_MASS, int>::type = 0>
    void set_mass(Real val, U mass_unit) noexcept {
        set_mass(detail::convert_weight_scalar(val, mass_unit, units::mass::kilogram));
    }

    void set_mass_from_weight(Real newtons, Real gravity = MassSpec3D::STANDARD_GRAVITY) noexcept {
        set_mass((gravity > 0.0_r) ? newtons / gravity : 0.0_r);
    }

public:
    const RMat3& inertia(Real gravity = MassSpec3D::STANDARD_GRAVITY) const noexcept {
        ensure_mass_uptodate(gravity);
        return m_inertia;
    }

    const RMat3& inv_inertia_local(Real gravity = MassSpec3D::STANDARD_GRAVITY) const noexcept {
        ensure_mass_uptodate(gravity);
        return m_inv_inertia;
    }

    const RMat3& inv_inertia_world() const noexcept;

    RVec3 apply_inv_inertia(const RVec3& v) const noexcept { return vec3::mul(inv_inertia_world(), v); }

public:
    Real shape_volume() const noexcept {
        ensure_mass_uptodate();
        return m_shape_volume;
    }

public:
    RVec3 local_center() const noexcept {
        ensure_mass_uptodate();
        return m_local_center;
    }

    template <class U, typename std::enable_if<is_fizmo_unit_v<U> && U::dimension() == units::DIMENSION_LENGTH, int>::type = 0>
    RVec3 local_center(U length_unit) const noexcept {
        ensure_mass_uptodate();
        return detail::convert_position_vec(m_local_center, units::distance::meter, length_unit);
    }

public:
    Real linear_damping()  const noexcept { return m_linear_damping; }
    Real angular_damping() const noexcept { return m_angular_damping; }

    void set_linear_damping(Real d)  noexcept { m_linear_damping  = d; }
    void set_angular_damping(Real d) noexcept { m_angular_damping = d; }

public:
    Real max_linear_speed() const noexcept { return m_max_linear_speed; }

    template <class U, typename std::enable_if<is_fizmo_unit_v<U> && U::dimension() == units::DIMENSION_VELOCITY, int>::type = 0>
    Real max_linear_speed(U velocity_unit) const noexcept {
        return detail::convert_velocity_scalar(m_max_linear_speed, units::velocity::meters_per_second, velocity_unit);
    }

    void set_max_linear_speed(Real s) noexcept { m_max_linear_speed = s; }

    template <class U, typename std::enable_if<is_fizmo_unit_v<U> && U::dimension() == units::DIMENSION_VELOCITY, int>::type = 0>
    void set_max_linear_speed(Real s, U velocity_unit) noexcept {
        m_max_linear_speed = detail::convert_velocity_scalar(s, velocity_unit, units::velocity::meters_per_second);
    }

public:
    Real max_angular_speed() const noexcept { return m_max_angular_speed; }

    template <class U, typename std::enable_if<is_fizmo_unit_v<U> && U::dimension() == units::DIMENSION_ANGULAR_VELOCITY, int>::type = 0>
    Real max_angular_speed(U angular_velocity_unit) const noexcept {
        static constexpr auto rad_per_s = units::angle::radian / units::time::second;
        return detail::convert_angular_velocity_scalar(m_max_angular_speed, rad_per_s, angular_velocity_unit);
    }

    void set_max_angular_speed(Real s) noexcept { m_max_angular_speed = s; }

    template <class U, typename std::enable_if<is_fizmo_unit_v<U> && U::dimension() == units::DIMENSION_ANGULAR_VELOCITY, int>::type = 0>
    void set_max_angular_speed(Real s, U angular_velocity_unit) noexcept {
        static constexpr auto rad_per_s = units::angle::radian / units::time::second;
        m_max_angular_speed = detail::convert_angular_velocity_scalar(s, angular_velocity_unit, rad_per_s);
    }

public:
    bool   is_awake()    const noexcept { return m_is_awake; }
    Real sleep_timer() const noexcept { return m_sleep_timer; }

public:
    Real gravity_scale() const noexcept { return m_gravity_scale; }
    void   set_gravity_scale(Real s) noexcept { m_gravity_scale = s; }

public:
    void apply_force(const RVec3& f) {
        if (!has(PhysicsFlags::LinearForces)) return;
        m_force += f;
        wake();
    }

    template <class U, typename std::enable_if<is_fizmo_unit_v<U> && U::dimension() == units::DIMENSION_FORCE, int>::type = 0>
    void apply_force(const RVec3& f, U unit) {
        apply_force(detail::convert_force_vec(f, unit, units::force::newton));
    }

    void apply_force_at(const RVec3& f, const RVec3& world_point);

    template <class FU, class PU, typename std::enable_if<is_fizmo_unit_v<FU> && is_fizmo_unit_v<PU> && FU::dimension() == units::DIMENSION_FORCE && PU::dimension() == units::DIMENSION_LENGTH, int>::type = 0>
    void apply_force_at(const RVec3& f, FU f_unit, const RVec3& world_point, PU p_unit) {
        apply_force_at(
            detail::convert_force_vec(f, f_unit, units::force::newton),
            detail::convert_position_vec(world_point, p_unit, units::distance::meter)
        );
    }

public:
    void apply_torque(const RVec3& t) {
        if (!has(PhysicsFlags::Torque)) return;
        m_torque += t;
        wake();
    }

    template <class U, typename std::enable_if<is_fizmo_unit_v<U> && U::dimension() == units::DIMENSION_TORQUE, int>::type = 0>
    void apply_torque(const RVec3& t, U torque_unit) {
        apply_torque(detail::convert_torque_vec(t, torque_unit, units::torque::newton_meter));
    }

public:
    void apply_linear_impulse(const RVec3& impulse);

    template <class U, typename std::enable_if<is_fizmo_unit_v<U> && U::dimension() == units::DIMENSION_IMPULSE, int>::type = 0>
    void apply_linear_impulse(const RVec3& impulse, U impulse_unit) {
        Real f = detail::convert_impulse_scalar(1.0_r, impulse_unit, units::impulse::newton_second);
        apply_linear_impulse(impulse * f);
    }

    void apply_impulse_at(const RVec3& impulse, const RVec3& world_point);

    void apply_angular_impulse(const RVec3& impulse);

public:
    void integrate_forces(Real dt, const RVec3& gravity);

    template <class U, typename std::enable_if<is_fizmo_unit_v<U> && U::dimension() == units::DIMENSION_ACCELERATION, int>::type = 0>
    void integrate_forces(Real dt, const RVec3& gravity, U grav_accel_unit) {
        RVec3 g = {
            static_cast<Real>(units::convert(gravity.x, grav_accel_unit, units::acceleration::meters_per_second_squared)),
            static_cast<Real>(units::convert(gravity.y, grav_accel_unit, units::acceleration::meters_per_second_squared)),
            static_cast<Real>(units::convert(gravity.z, grav_accel_unit, units::acceleration::meters_per_second_squared))
        };
        integrate_forces(dt, g);
    }

    void integrate_gyroscopic(Real dt);

    void integrate_velocities(Real dt);

    void apply_position_correction(const RVec3& d_center, const RVec3& d_rotation);

    void clear_forces() {
        m_force = {};
        m_torque = {};
    }

    void wake() { if (!m_is_awake) { m_is_awake = true; m_sleep_timer = 0.0_r; }}

    void put_to_sleep();

public:
    static constexpr Real SLEEP_TIME_THRESHOLD    = 0.5_r;
    static constexpr Real LINEAR_SLEEP_TOLERANCE  = 0.01_r;
    static constexpr Real ANGULAR_SLEEP_TOLERANCE = 0.035_r;

    Real advance_sleep_timer(Real dt);

    void update_sleep(Real dt);

    void synchronize_colliders() { for (auto& c : m_colliders) c.update_world_aabb(m_transform); }

    AABB3D compute_body_aabb() const;

    RVec3 world_center() const noexcept {
        ensure_mass_uptodate();
        return m_transform.apply(m_local_center);
    }

    template <class U, typename std::enable_if<is_fizmo_unit_v<U> && U::dimension() == units::DIMENSION_LENGTH, int>::type = 0>
    RVec3 world_center(U distance_unit) const noexcept {
        return detail::convert_position_vec(world_center(), units::distance::meter, distance_unit);
    }

    RVec3 get_world_point(const RVec3& local)  const noexcept { return m_transform.apply(local); }
    RVec3 get_local_point(const RVec3& world)  const noexcept { return m_transform.apply_inverse(world); }
    RVec3 get_world_vector(const RVec3& local) const noexcept { return m_transform.rotate(local); }
    RVec3 get_local_vector(const RVec3& world) const noexcept { return m_transform.rotate_inverse(world); }

    RVec3 get_velocity_at(const RVec3& world_point) const noexcept {
        return m_linear_velocity + vec3::cross(m_angular_velocity, world_point - world_center());
    }

    template <class PU, class VU, typename std::enable_if<is_fizmo_unit_v<PU> && is_fizmo_unit_v<VU> && PU::dimension() == units::DIMENSION_LENGTH && VU::dimension() == units::DIMENSION_VELOCITY, int>::type = 0>
    RVec3 get_velocity_at(const RVec3& world_point, PU position_distance_unit, VU velocity_unit) const noexcept {
        RVec3 wp_si = detail::convert_position_vec(world_point, position_distance_unit, units::distance::meter);
        return detail::convert_velocity_vec(get_velocity_at(wp_si), units::velocity::meters_per_second, velocity_unit);
    }

    Real kinetic_energy() const noexcept;

    template <class U, typename std::enable_if<is_fizmo_unit_v<U> && U::dimension() == units::DIMENSION_ENERGY, int>::type = 0>
    Real kinetic_energy(U energy_unit) const noexcept {
        return static_cast<Real>(units::convert(static_cast<long double>(kinetic_energy()), units::energy::joule, energy_unit));
    }

private:
    RVec3 mask_rotation(RVec3 w) const noexcept;

    RVec3 mask_translation(RVec3 d) const noexcept;

    static bool validate_frozen(FrozenProperty f) noexcept;

    void set_inertia(const RMat3& I) const noexcept;

    void recompute_mass_data(Real gravity = MassSpec3D::STANDARD_GRAVITY) const;

    void resolve_mass_constraint_from(FrozenProperty just_set) noexcept;

    void derive_mass_from_density_volume() noexcept;

    void derive_volume_from_mass_density() noexcept;

    void scale_collider_geometry(Real s);

    void refresh_shape_volume() const noexcept;

    void recompute_inertia_from_density() noexcept;

private:
    PhysicsFlags m_flags = PhysicsFlags::DefaultDynamic;

    std::uint64_t m_collision_layer = 1ULL;
    std::uint64_t m_collision_mask  = std::numeric_limits<std::uint64_t>::max();

    Transform3D     m_transform;
    mutable Sweep3D m_sweep;

    RVec3 m_linear_velocity{};   // m/s
    RVec3 m_angular_velocity{};  // rad/s, world space
    RVec3 m_force{};             // N
    RVec3 m_torque{};            // N*m
    RVec3 m_gyro_half{};         // half of this step's gyroscopic change in w (see integrate_gyroscopic)

    mutable Real  m_density      = 1000.0_r; // kg/m3
    FrozenProperty  m_frozen       = FrozenProperty::None;
    mutable Real  m_shape_volume = 0.0_r;     // m3

    mutable bool           m_mass_dirty  = true;
    mutable Real         m_mass        = 0.0_r;  // kg
    mutable Real         m_inv_mass    = 0.0_r;  // 1/kg
    mutable RMat3 m_inertia{};          // kg*m2, body axes about the center of mass
    mutable RMat3 m_inv_inertia{};      // 1/(kg*m2), body axes
    mutable RMat3 m_inv_inertia_world{};
    mutable bool           m_world_inertia_dirty = true;
    mutable RVec3       m_local_center{};     // m

    Real m_linear_damping    = 0.0_r;   // 1/s
    Real m_angular_damping   = 0.0_r;   // 1/s
    Real m_max_linear_speed  = 1e6_r;   // m/s
    Real m_max_angular_speed = 1e6_r;   // rad/s

    Real m_sleep_timer = 0.0_r;         // s
    bool   m_is_awake    = true;

    Real m_gravity_scale = 1.0_r;

    std::vector<Collider3D> m_colliders;
    std::uint64_t  m_id = 0;
    BodyQuality    m_quality = BodyQuality::Default;
    int            m_velocity_iterations = 0;
    int            m_position_iterations = 0;
    ContinuousMode m_continuous = ContinuousMode::Default;
    Real         m_linear_sleep_tolerance = LINEAR_SLEEP_TOLERANCE;
    Real         m_angular_sleep_tolerance = ANGULAR_SLEEP_TOLERANCE;
    Real         m_sleep_time = SLEEP_TIME_THRESHOLD;
    std::int32_t   m_proxy = -1;
    std::size_t    m_solver_index = 0;
    BodyHandle     m_handle{};
};

} // namespace physics
} // namespace fizmo

#endif // FIZMO_PHYSICS_3D_HPP