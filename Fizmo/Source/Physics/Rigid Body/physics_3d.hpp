#ifndef FIZMO_PHYSICS_3D_HPP
#define FIZMO_PHYSICS_3D_HPP

#include "common.hpp"

namespace fizmo {
namespace physics {

namespace vec3 {

inline double dot(const vector3d& a, const vector3d& b) noexcept { return a.x * b.x + a.y * b.y + a.z * b.z; }

inline vector3d cross(const vector3d& a, const vector3d& b) noexcept {
    return { a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x };
}

inline double length_squared(const vector3d& a) noexcept { return dot(a, a); }
inline double length(const vector3d& a) noexcept { return std::sqrt(dot(a, a)); }

inline vector3d normalize(const vector3d& a, const vector3d& fallback = vector3d{0.0, 1.0, 0.0}) noexcept {
    double l = length(a);
    return l > constants::middle_epsilon() ? a * (1.0 / l) : fallback;
}

inline vector3d negate(const vector3d& a) noexcept { return a * -1.0; }
inline bool is_zero(const vector3d& a) noexcept { return a.x == 0.0 && a.y == 0.0 && a.z == 0.0; }

inline vector3d min(const vector3d& a, const vector3d& b) noexcept { return { std::min(a.x, b.x), std::min(a.y, b.y), std::min(a.z, b.z) }; }
inline vector3d max(const vector3d& a, const vector3d& b) noexcept { return { std::max(a.x, b.x), std::max(a.y, b.y), std::max(a.z, b.z) }; }

inline double component(const vector3d& v, int axis) noexcept { return axis == 0 ? v.x : (axis == 1 ? v.y : v.z); }

 vector3d mul(const math::Matrix3d& m, const vector3d& v) noexcept;

 vector3d mul_transpose(const math::Matrix3d& m, const vector3d& v) noexcept;

inline math::Matrix3d outer(const vector3d& a, const vector3d& b) noexcept {
    return { a.x * b.x, a.x * b.y, a.x * b.z,
             a.y * b.x, a.y * b.y, a.y * b.z,
             a.z * b.x, a.z * b.y, a.z * b.z };
}

 void orthonormal_basis(const vector3d& n, vector3d& t1, vector3d& t2) noexcept;

 QuatD integrate_rotation(const QuatD& q, const vector3d& w, double dt) noexcept;

} // namespace vec3

struct AABB3D {
    vector3d min{}; // meters
    vector3d max{}; // meters

    constexpr AABB3D() noexcept = default;
    constexpr AABB3D(const vector3d& lo, const vector3d& hi) noexcept : min(lo), max(hi) {}

    vector3d center()  const noexcept { return (min + max) * 0.5; }
    vector3d extents() const noexcept { return (max - min) * 0.5; }

    double surface_area() const noexcept {
        double w = max.x - min.x, h = max.y - min.y, d = max.z - min.z;
        return 2.0 * (w * h + h * d + d * w);
    }

    double volume() const noexcept { return (max.x - min.x) * (max.y - min.y) * (max.z - min.z); }

    bool overlaps(const AABB3D& o) const noexcept;

    bool contains(const vector3d& p) const noexcept {
        return p.x >= min.x && p.x <= max.x && p.y >= min.y && p.y <= max.y && p.z >= min.z && p.z <= max.z;
    }

    bool contains(const AABB3D& o) const noexcept;

    // margin in meters
    AABB3D fatten(double margin) const noexcept;

    static AABB3D merge(const AABB3D& a, const AABB3D& b) noexcept { return { vec3::min(a.min, b.min), vec3::max(a.max, b.max) }; }
};

struct Transform3D {
    vector3d       position{};                              // meters
    QuatD          orientation{1.0, 0.0, 0.0, 0.0};         // unit quaternion
    math::Matrix3d rotation = math::Matrix3d::identity();   // cached from orientation

    Transform3D() noexcept = default;

    Transform3D(const vector3d& pos, const QuatD& q) noexcept
        : position(pos), orientation(q.normalized()), rotation(quat_to_mat3(orientation)) {}

    static Transform3D identity() noexcept { return {}; }

    vector3d apply(const vector3d& local) const noexcept { return vec3::mul(rotation, local) + position; }
    vector3d apply_inverse(const vector3d& world) const noexcept { return vec3::mul_transpose(rotation, world - position); }
    vector3d rotate(const vector3d& dir) const noexcept { return vec3::mul(rotation, dir); }
    vector3d rotate_inverse(const vector3d& dir) const noexcept { return vec3::mul_transpose(rotation, dir); }

    Transform3D compose(const Transform3D& other) const noexcept {
        return Transform3D(apply(other.position), orientation * other.orientation);
    }

    void set_orientation(const QuatD& q) noexcept {
        orientation = q.normalized();
        rotation = quat_to_mat3(orientation);
    }
};

struct Sweep3D {
    vector3d center_local{}; // meters
    vector3d position0{};    // meters (center of mass)
    vector3d position1{};    // meters (center of mass)
    QuatD    orientation0{1.0, 0.0, 0.0, 0.0};
    QuatD    orientation1{1.0, 0.0, 0.0, 0.0};

    Transform3D get_transform(double t) const noexcept;
};

struct SphereShape {
    vector3d center{};   // meters, local body space
    double radius = 0.0; // meters
};

struct CapsuleShape3D {
    vector3d point1{};   // meters, local
    vector3d point2{};   // meters, local
    double radius = 0.0; // meters
    vector3d center() const noexcept { return (point1 + point2) * 0.5; }
    double half_length() const noexcept { return vec3::length(point2 - point1) * 0.5; } // m
};

struct PlaneShape {
    vector3d normal{0.0, 1.0, 0.0}; // unit, points out of the solid
    double   offset = 0.0;          // meters
};

struct HullFace {
    vector3d normal{};                  
    double   offset = 0.0;              
    std::vector<std::uint16_t> indices; // counter-clockwise when viewed from outside
};

struct HullShape {
    std::vector<vector3d> vertices;                    // meters, local
    std::vector<HullFace> faces;
    std::vector<std::array<std::uint16_t, 2>> edges;   // unique edges
    std::vector<vector3d> edge_directions;             // unique (up to sign) unit edge directions
    vector3d centroid{};                               // volume centroid (m, local)
    double   volume = 0.0;                             // m3

    bool valid() const noexcept { return faces.size() >= 4 && volume > 0.0; }

    static HullShape box(double hx, double hy, double hz);

    static HullShape from_points(const vector3d* pts, std::size_t n);

    static HullShape from_points(const std::vector<vector3d>& pts) { return from_points(pts.data(), pts.size()); }

    void finalize();

    void scale(double s) {
        for (auto& v : vertices) v = v * s;
        finalize();
    }

    AABB3D bounds() const noexcept;
};

enum class ShapeType3D : std::uint8_t {
    Sphere = 0,
    Capsule,
    Hull,
    Plane
};

struct Shape3D {
    ShapeType3D type = ShapeType3D::Sphere;

    SphereShape                      sphere{};
    CapsuleShape3D                   capsule{};
    std::shared_ptr<const HullShape> hull;     
    PlaneShape                       plane{};

    double bounding_radius = 0.0; // meters (infinite for planes)
    AABB3D local_aabb{};

    static Shape3D make_sphere(const vector3d& center, double radius);

    static Shape3D make_capsule(const vector3d& p1, const vector3d& p2, double radius);

    static Shape3D make_hull(HullShape h);

    static Shape3D make_box(double hx, double hy, double hz) { return make_hull(HullShape::box(hx, hy, hz)); } // meter half-extents
    static Shape3D make_convex(const vector3d* pts, std::size_t n) { return make_hull(HullShape::from_points(pts, n)); }
    static Shape3D make_convex(const std::vector<vector3d>& pts) { return make_hull(HullShape::from_points(pts)); }

    static Shape3D make_plane(const vector3d& normal, double offset);

    void rebuild_bounds();

    AABB3D compute_aabb(const Transform3D& xf) const noexcept;

    static constexpr double PLANE_EXTENT = 1e15; // meters, stands in for infinity in plane AABBs
};

struct MassData3D {
    double         mass = 0.0; // kg
    math::Matrix3d inertia{};  // kg*m2, about center, local axes
    vector3d       center{};   // meters, local body space

    static MassData3D combine(const MassData3D* parts, std::size_t n);
};

namespace detail {

inline double sphere_volume(const SphereShape& s) { return 4.0 / 3.0 * static_cast<double>(constants::pi()) * s.radius * s.radius * s.radius; }

 double capsule_volume(const CapsuleShape3D& c);

 MassData3D compute_sphere_mass(const SphereShape& s, double density);

 MassData3D compute_capsule_mass(const CapsuleShape3D& c, double density);

 MassData3D compute_hull_mass(const HullShape& h, double density);

template <class FromU, class ToU>
inline typename std::enable_if<is_fizmo_unit_v<FromU> && is_fizmo_unit_v<ToU>, vector3d>::type
convert_velocity_vec(const vector3d& v, FromU from, ToU to) {
    return { convert_velocity_scalar(v.x, from, to), convert_velocity_scalar(v.y, from, to), convert_velocity_scalar(v.z, from, to) };
}

template <class FromU, class ToU>
inline typename std::enable_if<is_fizmo_unit_v<FromU> && is_fizmo_unit_v<ToU>, vector3d>::type
convert_angular_velocity_vec(const vector3d& v, FromU from, ToU to) {
    return { convert_angular_velocity_scalar(v.x, from, to), convert_angular_velocity_scalar(v.y, from, to), convert_angular_velocity_scalar(v.z, from, to) };
}

template <class FromU, class ToU>
inline typename std::enable_if<is_fizmo_unit_v<FromU> && is_fizmo_unit_v<ToU>, vector3d>::type
convert_force_vec(const vector3d& v, FromU from, ToU to) {
    return { convert_force_scalar(v.x, from, to), convert_force_scalar(v.y, from, to), convert_force_scalar(v.z, from, to) };
}

template <class FromU, class ToU>
inline typename std::enable_if<is_fizmo_unit_v<FromU> && is_fizmo_unit_v<ToU>, vector3d>::type
convert_torque_vec(const vector3d& v, FromU from, ToU to) {
    return { convert_torque_scalar(v.x, from, to), convert_torque_scalar(v.y, from, to), convert_torque_scalar(v.z, from, to) };
}

template <class FromU, class ToU>
inline typename std::enable_if<is_fizmo_unit_v<FromU> && is_fizmo_unit_v<ToU>, vector3d>::type
convert_position_vec(const vector3d& v, FromU from, ToU to) {
    return { convert_distance_scalar(v.x, from, to), convert_distance_scalar(v.y, from, to), convert_distance_scalar(v.z, from, to) };
}

} // namespace detail

 double compute_shape_volume(const Shape3D& shape);

 MassData3D compute_mass(const Shape3D& shape, double density);

struct MassSpec3D {
public:
    static constexpr double STANDARD_GRAVITY = constants::gravity(); // m/s2

public:
    MassMode mode  = MassMode::Density;
    double   value = 999.9749; // kg/m3 by default (water)

public:
    constexpr MassSpec3D() noexcept = default;
    constexpr MassSpec3D(MassMode m, double v) noexcept : mode(m), value(v) {}

public:
    static constexpr MassSpec3D from_density(double kg_per_m3) noexcept { return { MassMode::Density, kg_per_m3 }; }
    static constexpr MassSpec3D from_mass(double kg) noexcept { return { MassMode::Mass, kg }; }
    static constexpr MassSpec3D from_weight(double newtons) noexcept { return { MassMode::Weight, newtons }; }

    template <class U, typename std::enable_if<is_fizmo_unit_v<U> && U::dimension() == units::DIMENSION_DENSITY_3D, int>::type = 0>
    static MassSpec3D from_density(double val, U unit) { return { MassMode::Density, detail::convert_weight_scalar(val, unit, (units::mass::kilogram / units::volume::cubic_meter)) }; }

    template <class U, typename std::enable_if<is_fizmo_unit_v<U> && U::dimension() == units::DIMENSION_MASS, int>::type = 0>
    static MassSpec3D from_mass(double val, U unit)    { return { MassMode::Mass   , detail::convert_weight_scalar(val, unit, units::mass::kilogram) }; }

    template <class U, typename std::enable_if<is_fizmo_unit_v<U> && U::dimension() == units::DIMENSION_FORCE, int>::type = 0>
    static MassSpec3D from_weight(double val, U unit)  { return { MassMode::Weight , detail::convert_force_scalar (val, unit, units::force::newton ) }; }

public:
    double resolve_density(double shape_volume_m3, double gravity = STANDARD_GRAVITY) const noexcept;

    double to_mass(double shape_volume_m3, double gravity = STANDARD_GRAVITY) const noexcept {
        return resolve_density(shape_volume_m3, gravity) * shape_volume_m3;
    }
};

struct Material3D {
    MassSpec3D mass_spec{};
    double     friction    = 0.3;           // unitless [0, 1]
    double     restitution = 0.0;           // unitless [0, 1]
    double     restitution_threshold = 1.0; // m/s

    constexpr Material3D() noexcept = default;
    explicit constexpr Material3D(double density) noexcept : mass_spec(MassSpec3D::from_density(density)) {}
    constexpr Material3D(const MassSpec3D& spec, double fric = 0.3, double rest = 0.0, double rest_thresh = 1.0) noexcept : mass_spec(spec), friction(fric), restitution(rest), restitution_threshold(rest_thresh) {}

    static constexpr Material3D with_density(double kg_per_m3) noexcept { return Material3D(MassSpec3D::from_density(kg_per_m3)); }
    static constexpr Material3D with_mass(double kg) noexcept { return Material3D(MassSpec3D::from_mass(kg)); }
    static constexpr Material3D with_weight(double newtons) noexcept { return Material3D(MassSpec3D::from_weight(newtons)); }

    template <class U, typename std::enable_if<is_fizmo_unit_v<U> && U::dimension() == units::DIMENSION_DENSITY_3D, int>::type = 0>
    static Material3D with_density(double val, U unit) { return Material3D(MassSpec3D::from_density(val, unit)); }

    template <class U, typename std::enable_if<is_fizmo_unit_v<U> && U::dimension() == units::DIMENSION_MASS, int>::type = 0>
    static Material3D with_mass(double val, U unit) { return Material3D(MassSpec3D::from_mass(val, unit)); }

    template <class U, typename std::enable_if<is_fizmo_unit_v<U> && U::dimension() == units::DIMENSION_FORCE, int>::type = 0>
    static Material3D with_weight(double val, U unit) { return Material3D(MassSpec3D::from_weight(val, unit)); }
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

    void recompute_mass(double gravity = MassSpec3D::STANDARD_GRAVITY);

    void update_world_aabb(const Transform3D& body_xf);
};

class RigidBody3D {
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

    void ensure_mass_uptodate(double gravity = MassSpec3D::STANDARD_GRAVITY) const {
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
    vector3d position() const noexcept { return m_transform.position; }

    template <class U, typename std::enable_if<is_fizmo_unit_v<U> && U::dimension() == units::DIMENSION_LENGTH, int>::type = 0>
    vector3d position(U distance_unit) const noexcept {
        return detail::convert_position_vec(m_transform.position, units::distance::meter, distance_unit);
    }

    void set_position(const vector3d& pos) noexcept { m_transform.position = pos; }

    template <class U, typename std::enable_if<is_fizmo_unit_v<U> && U::dimension() == units::DIMENSION_LENGTH, int>::type = 0>
    void set_position(const vector3d& pos, U distance_unit) noexcept {
        m_transform.position = detail::convert_position_vec(pos, distance_unit, units::distance::meter);
    }

public:
    const QuatD& orientation() const noexcept { return m_transform.orientation; }

    void set_orientation(const QuatD& q) noexcept {
        m_transform.set_orientation(q);
        m_world_inertia_dirty = true;
    }

    void set_rotation(const vector3d& axis, double angle, bool in_rad = true) noexcept;

    // {roll (about X), pitch (about Y), yaw (about Z)}
    std::array<double, 3> euler_angles(bool in_rad = true) const noexcept;

    void set_euler_angles(double roll, double pitch, double yaw, bool in_rad = true) noexcept;

public:
    vector3d linear_velocity() const noexcept { return m_linear_velocity; }

    template <class U, typename std::enable_if<is_fizmo_unit_v<U> && U::dimension() == units::DIMENSION_VELOCITY, int>::type = 0>
    vector3d linear_velocity(U unit) const noexcept {
        return detail::convert_velocity_vec(m_linear_velocity, units::velocity::meters_per_second, unit);
    }

    void set_linear_velocity(const vector3d& v) noexcept {
        m_linear_velocity = v;
        if (!vec3::is_zero(v)) wake();
    }

    template <class U, typename std::enable_if<is_fizmo_unit_v<U> && U::dimension() == units::DIMENSION_VELOCITY, int>::type = 0>
    void set_linear_velocity(const vector3d& v, U unit) noexcept {
        set_linear_velocity(detail::convert_velocity_vec(v, unit, units::velocity::meters_per_second));
    }

public:
    // World-space angular velocity (rad/s); its direction is the spin axis.
    vector3d angular_velocity() const noexcept { return m_angular_velocity; }

    template <class U, typename std::enable_if<is_fizmo_unit_v<U> && U::dimension() == units::DIMENSION_ANGULAR_VELOCITY, int>::type = 0>
    vector3d angular_velocity(U unit) const noexcept {
        static constexpr auto rad_per_s = units::angle::radian / units::time::second;
        return detail::convert_angular_velocity_vec(m_angular_velocity, rad_per_s, unit);
    }

    void set_angular_velocity(const vector3d& w) noexcept {
        m_angular_velocity = w;
        if (!vec3::is_zero(w)) wake();
    }

    template <class U, typename std::enable_if<is_fizmo_unit_v<U> && U::dimension() == units::DIMENSION_ANGULAR_VELOCITY, int>::type = 0>
    void set_angular_velocity(const vector3d& w, U unit) noexcept {
        static constexpr auto rad_per_s = units::angle::radian / units::time::second;
        set_angular_velocity(detail::convert_angular_velocity_vec(w, unit, rad_per_s));
    }

public:
    vector3d force() const noexcept { return m_force; }

    template <class U, typename std::enable_if<is_fizmo_unit_v<U> && U::dimension() == units::DIMENSION_FORCE, int>::type = 0>
    vector3d force(U force_unit) const noexcept {
        return detail::convert_force_vec(m_force, units::force::newton, force_unit);
    }

    void set_force(const vector3d& f) noexcept { m_force = f; }

    template <class U, typename std::enable_if<is_fizmo_unit_v<U> && U::dimension() == units::DIMENSION_FORCE, int>::type = 0>
    void set_force(const vector3d& f, U unit) noexcept {
        m_force = detail::convert_force_vec(f, unit, units::force::newton);
    }

public:
    vector3d torque() const noexcept { return m_torque; }

    template <class U, typename std::enable_if<is_fizmo_unit_v<U> && U::dimension() == units::DIMENSION_TORQUE, int>::type = 0>
    vector3d torque(U unit) const noexcept {
        return detail::convert_torque_vec(m_torque, units::torque::newton_meter, unit);
    }

    void set_torque(const vector3d& t) noexcept { m_torque = t; }

    template <class U, typename std::enable_if<is_fizmo_unit_v<U> && U::dimension() == units::DIMENSION_TORQUE, int>::type = 0>
    void set_torque(const vector3d& t, U torque_unit) noexcept {
        m_torque = detail::convert_torque_vec(t, torque_unit, units::torque::newton_meter);
    }

public:
    double density() const noexcept {
        ensure_mass_uptodate();
        return m_density;
    }

    template <class U, typename std::enable_if<is_fizmo_unit_v<U> && U::dimension() == units::DIMENSION_DENSITY_3D, int>::type = 0>
    double density(U density_unit) const noexcept {
        ensure_mass_uptodate();
        static constexpr auto si_density = units::mass::kilogram / units::volume::cubic_meter;
        return static_cast<double>(units::convert(static_cast<long double>(m_density), si_density, density_unit));
    }

    void set_density(double d) noexcept;

    template <class U, typename std::enable_if<is_fizmo_unit_v<U> && U::dimension() == units::DIMENSION_DENSITY_3D, int>::type = 0>
    void set_density(double d, U density_unit) noexcept {
        static constexpr auto si_density = units::mass::kilogram / units::volume::cubic_meter;
        set_density(static_cast<double>(units::convert(static_cast<long double>(d), density_unit, si_density)));
    }

public:
    double mass(double gravity = MassSpec3D::STANDARD_GRAVITY) const noexcept {
        ensure_mass_uptodate(gravity);
        return m_mass;
    }

    template <class U, typename std::enable_if<is_fizmo_unit_v<U> && U::dimension() == units::DIMENSION_MASS, int>::type = 0>
    double mass(U mass_unit, double gravity = MassSpec3D::STANDARD_GRAVITY) const noexcept {
        ensure_mass_uptodate(gravity);
        return detail::convert_weight_scalar(m_mass, units::mass::kilogram, mass_unit);
    }

    double inv_mass(double gravity = MassSpec3D::STANDARD_GRAVITY) const noexcept {
        ensure_mass_uptodate(gravity);
        return m_inv_mass;
    }

    void set_mass(double kg) noexcept;

    template <class U, typename std::enable_if<is_fizmo_unit_v<U> && U::dimension() == units::DIMENSION_MASS, int>::type = 0>
    void set_mass(double val, U mass_unit) noexcept {
        set_mass(detail::convert_weight_scalar(val, mass_unit, units::mass::kilogram));
    }

    void set_mass_from_weight(double newtons, double gravity = MassSpec3D::STANDARD_GRAVITY) noexcept {
        set_mass((gravity > 0.0) ? newtons / gravity : 0.0);
    }

public:
    const math::Matrix3d& inertia(double gravity = MassSpec3D::STANDARD_GRAVITY) const noexcept {
        ensure_mass_uptodate(gravity);
        return m_inertia;
    }

    const math::Matrix3d& inv_inertia_local(double gravity = MassSpec3D::STANDARD_GRAVITY) const noexcept {
        ensure_mass_uptodate(gravity);
        return m_inv_inertia;
    }

    const math::Matrix3d& inv_inertia_world() const noexcept;

    vector3d apply_inv_inertia(const vector3d& v) const noexcept { return vec3::mul(inv_inertia_world(), v); }

public:
    double shape_volume() const noexcept {
        ensure_mass_uptodate();
        return m_shape_volume;
    }

public:
    vector3d local_center() const noexcept {
        ensure_mass_uptodate();
        return m_local_center;
    }

    template <class U, typename std::enable_if<is_fizmo_unit_v<U> && U::dimension() == units::DIMENSION_LENGTH, int>::type = 0>
    vector3d local_center(U length_unit) const noexcept {
        ensure_mass_uptodate();
        return detail::convert_position_vec(m_local_center, units::distance::meter, length_unit);
    }

public:
    double linear_damping()  const noexcept { return m_linear_damping; }
    double angular_damping() const noexcept { return m_angular_damping; }

    void set_linear_damping(double d)  noexcept { m_linear_damping  = d; }
    void set_angular_damping(double d) noexcept { m_angular_damping = d; }

public:
    double max_linear_speed() const noexcept { return m_max_linear_speed; }

    template <class U, typename std::enable_if<is_fizmo_unit_v<U> && U::dimension() == units::DIMENSION_VELOCITY, int>::type = 0>
    double max_linear_speed(U velocity_unit) const noexcept {
        return detail::convert_velocity_scalar(m_max_linear_speed, units::velocity::meters_per_second, velocity_unit);
    }

    void set_max_linear_speed(double s) noexcept { m_max_linear_speed = s; }

    template <class U, typename std::enable_if<is_fizmo_unit_v<U> && U::dimension() == units::DIMENSION_VELOCITY, int>::type = 0>
    void set_max_linear_speed(double s, U velocity_unit) noexcept {
        m_max_linear_speed = detail::convert_velocity_scalar(s, velocity_unit, units::velocity::meters_per_second);
    }

public:
    double max_angular_speed() const noexcept { return m_max_angular_speed; }

    template <class U, typename std::enable_if<is_fizmo_unit_v<U> && U::dimension() == units::DIMENSION_ANGULAR_VELOCITY, int>::type = 0>
    double max_angular_speed(U angular_velocity_unit) const noexcept {
        static constexpr auto rad_per_s = units::angle::radian / units::time::second;
        return detail::convert_angular_velocity_scalar(m_max_angular_speed, rad_per_s, angular_velocity_unit);
    }

    void set_max_angular_speed(double s) noexcept { m_max_angular_speed = s; }

    template <class U, typename std::enable_if<is_fizmo_unit_v<U> && U::dimension() == units::DIMENSION_ANGULAR_VELOCITY, int>::type = 0>
    void set_max_angular_speed(double s, U angular_velocity_unit) noexcept {
        static constexpr auto rad_per_s = units::angle::radian / units::time::second;
        m_max_angular_speed = detail::convert_angular_velocity_scalar(s, angular_velocity_unit, rad_per_s);
    }

public:
    bool   is_awake()    const noexcept { return m_is_awake; }
    double sleep_timer() const noexcept { return m_sleep_timer; }

public:
    double gravity_scale() const noexcept { return m_gravity_scale; }
    void   set_gravity_scale(double s) noexcept { m_gravity_scale = s; }

public:
    void apply_force(const vector3d& f) {
        if (!has(PhysicsFlags::LinearForces)) return;
        m_force += f;
        wake();
    }

    template <class U, typename std::enable_if<is_fizmo_unit_v<U> && U::dimension() == units::DIMENSION_FORCE, int>::type = 0>
    void apply_force(const vector3d& f, U unit) {
        apply_force(detail::convert_force_vec(f, unit, units::force::newton));
    }

    void apply_force_at(const vector3d& f, const vector3d& world_point);

    template <class FU, class PU, typename std::enable_if<is_fizmo_unit_v<FU> && is_fizmo_unit_v<PU> && FU::dimension() == units::DIMENSION_FORCE && PU::dimension() == units::DIMENSION_LENGTH, int>::type = 0>
    void apply_force_at(const vector3d& f, FU f_unit, const vector3d& world_point, PU p_unit) {
        apply_force_at(
            detail::convert_force_vec(f, f_unit, units::force::newton),
            detail::convert_position_vec(world_point, p_unit, units::distance::meter)
        );
    }

public:
    void apply_torque(const vector3d& t) {
        if (!has(PhysicsFlags::Torque)) return;
        m_torque += t;
        wake();
    }

    template <class U, typename std::enable_if<is_fizmo_unit_v<U> && U::dimension() == units::DIMENSION_TORQUE, int>::type = 0>
    void apply_torque(const vector3d& t, U torque_unit) {
        apply_torque(detail::convert_torque_vec(t, torque_unit, units::torque::newton_meter));
    }

public:
    void apply_linear_impulse(const vector3d& impulse);

    template <class U, typename std::enable_if<is_fizmo_unit_v<U> && U::dimension() == units::DIMENSION_IMPULSE, int>::type = 0>
    void apply_linear_impulse(const vector3d& impulse, U impulse_unit) {
        double f = detail::convert_impulse_scalar(1.0, impulse_unit, units::impulse::newton_second);
        apply_linear_impulse(impulse * f);
    }

    void apply_impulse_at(const vector3d& impulse, const vector3d& world_point);

    void apply_angular_impulse(const vector3d& impulse);

public:
    void integrate_forces(double dt, const vector3d& gravity);

    template <class U, typename std::enable_if<is_fizmo_unit_v<U> && U::dimension() == units::DIMENSION_ACCELERATION, int>::type = 0>
    void integrate_forces(double dt, const vector3d& gravity, U grav_accel_unit) {
        vector3d g = {
            static_cast<double>(units::convert(gravity.x, grav_accel_unit, units::acceleration::meters_per_second_squared)),
            static_cast<double>(units::convert(gravity.y, grav_accel_unit, units::acceleration::meters_per_second_squared)),
            static_cast<double>(units::convert(gravity.z, grav_accel_unit, units::acceleration::meters_per_second_squared))
        };
        integrate_forces(dt, g);
    }

    void integrate_gyroscopic(double dt);

    void integrate_velocities(double dt);

    void apply_position_correction(const vector3d& d_center, const vector3d& d_rotation);

    void clear_forces() {
        m_force;
        m_torque;
    }

    void wake() { if (!m_is_awake) { m_is_awake = true; m_sleep_timer = 0.0; }}

    void put_to_sleep();

public:
    static constexpr double SLEEP_TIME_THRESHOLD    = 0.5;
    static constexpr double LINEAR_SLEEP_TOLERANCE  = 0.01;
    static constexpr double ANGULAR_SLEEP_TOLERANCE = 0.035;

    double advance_sleep_timer(double dt);

    void update_sleep(double dt);

    void synchronize_colliders() { for (auto& c : m_colliders) c.update_world_aabb(m_transform); }

    AABB3D compute_body_aabb() const;

    vector3d world_center() const noexcept {
        ensure_mass_uptodate();
        return m_transform.apply(m_local_center);
    }

    template <class U, typename std::enable_if<is_fizmo_unit_v<U> && U::dimension() == units::DIMENSION_LENGTH, int>::type = 0>
    vector3d world_center(U distance_unit) const noexcept {
        return detail::convert_position_vec(world_center(), units::distance::meter, distance_unit);
    }

    vector3d get_world_point(const vector3d& local)  const noexcept { return m_transform.apply(local); }
    vector3d get_local_point(const vector3d& world)  const noexcept { return m_transform.apply_inverse(world); }
    vector3d get_world_vector(const vector3d& local) const noexcept { return m_transform.rotate(local); }
    vector3d get_local_vector(const vector3d& world) const noexcept { return m_transform.rotate_inverse(world); }

    vector3d get_velocity_at(const vector3d& world_point) const noexcept {
        return m_linear_velocity + vec3::cross(m_angular_velocity, world_point - world_center());
    }

    template <class PU, class VU, typename std::enable_if<is_fizmo_unit_v<PU> && is_fizmo_unit_v<VU> && PU::dimension() == units::DIMENSION_LENGTH && VU::dimension() == units::DIMENSION_VELOCITY, int>::type = 0>
    vector3d get_velocity_at(const vector3d& world_point, PU position_distance_unit, VU velocity_unit) const noexcept {
        vector3d wp_si = detail::convert_position_vec(world_point, position_distance_unit, units::distance::meter);
        return detail::convert_velocity_vec(get_velocity_at(wp_si), units::velocity::meters_per_second, velocity_unit);
    }

    double kinetic_energy() const noexcept;

    template <class U, typename std::enable_if<is_fizmo_unit_v<U> && U::dimension() == units::DIMENSION_ENERGY, int>::type = 0>
    double kinetic_energy(U energy_unit) const noexcept {
        return static_cast<double>(units::convert(static_cast<long double>(kinetic_energy()), units::energy::joule, energy_unit));
    }

private:
    vector3d mask_rotation(vector3d w) const noexcept;

    vector3d mask_translation(vector3d d) const noexcept;

    static bool validate_frozen(FrozenProperty f) noexcept;

    void set_inertia(const math::Matrix3d& I) const noexcept;

    void recompute_mass_data(double gravity = MassSpec3D::STANDARD_GRAVITY) const;

    void resolve_mass_constraint_from(FrozenProperty just_set) noexcept;

    void derive_mass_from_density_volume() noexcept;

    void derive_volume_from_mass_density() noexcept;

    void scale_collider_geometry(double s);

    void refresh_shape_volume() const noexcept;

    void recompute_inertia_from_density() noexcept;

private:
    PhysicsFlags m_flags = PhysicsFlags::DefaultDynamic;

    std::uint64_t m_collision_layer = 1ULL;
    std::uint64_t m_collision_mask  = std::numeric_limits<std::uint64_t>::max();

    Transform3D     m_transform;
    mutable Sweep3D m_sweep;

    vector3d m_linear_velocity{};   // m/s
    vector3d m_angular_velocity{};  // rad/s, world space
    vector3d m_force{};             // N
    vector3d m_torque{};            // N*m
    vector3d m_gyro_half{};         // half of this step's gyroscopic change in w (see integrate_gyroscopic)

    mutable double  m_density      = 1000.0; // kg/m3
    FrozenProperty  m_frozen       = FrozenProperty::None;
    mutable double  m_shape_volume = 0.0;     // m3

    mutable bool           m_mass_dirty  = true;
    mutable double         m_mass        = 0.0;  // kg
    mutable double         m_inv_mass    = 0.0;  // 1/kg
    mutable math::Matrix3d m_inertia{};          // kg*m2, body axes about the center of mass
    mutable math::Matrix3d m_inv_inertia{};      // 1/(kg*m2), body axes
    mutable math::Matrix3d m_inv_inertia_world{};
    mutable bool           m_world_inertia_dirty = true;
    mutable vector3d       m_local_center{};     // m

    double m_linear_damping    = 0.0;   // 1/s
    double m_angular_damping   = 0.0;   // 1/s
    double m_max_linear_speed  = 1e6;   // m/s
    double m_max_angular_speed = 1e6;   // rad/s

    double m_sleep_timer = 0.0;         // s
    bool   m_is_awake    = true;

    double m_gravity_scale = 1.0;

    std::vector<Collider3D> m_colliders;
};

} // namespace physics
} // namespace fizmo

#endif // FIZMO_PHYSICS_3D_HPP