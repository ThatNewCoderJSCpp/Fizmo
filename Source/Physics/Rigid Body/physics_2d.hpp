#ifndef FIZMO_PHYSICS_2D_HPP
#define FIZMO_PHYSICS_2D_HPP

#include "common.hpp"

namespace fizmo {
namespace physics {

inline constexpr std::size_t MAX_POLYGON_VERTICES = 8;

struct AABB {
    vector2d min{}; // meters
    vector2d max{}; // meters

    constexpr AABB() noexcept = default;
    constexpr AABB(const vector2d& lo, const vector2d& hi) noexcept : min(lo), max(hi) {}

    constexpr vector2d center() const noexcept { return (min + max) * 0.5; }
    constexpr vector2d extents() const noexcept { return (max - min) * 0.5; }

    constexpr double perimeter() const noexcept { 
        double w = max.x - min.x, h = max.y - min.y;
        return 2.0 * (w + h);
    }

    constexpr double area() const noexcept { return (max.x - min.x) * (max.y - min.y); }
    constexpr bool overlaps(const AABB& o) const noexcept { return min.x <= o.max.x && max.x >= o.min.x && min.y <= o.max.y && max.y >= o.min.y; }
    constexpr bool contains(const vector2d& p) const noexcept { return p.x >= min.x && p.x <= max.x && p.y >= min.y && p.y <= max.y; }
    constexpr bool contains(const AABB& o) const noexcept { return min.x <= o.min.x && min.y <= o.min.y && max.x >= o.max.x && max.y >= o.max.y; }

    // margin in meters
    constexpr AABB fatten(double margin) const noexcept {
        return { {min.x - margin, min.y - margin}, {max.x + margin, max.y + margin} };
    }

    static constexpr AABB merge(const AABB& a, const AABB& b) noexcept {
        return { 
            { 
                a.min.x < b.min.x ? a.min.x : b.min.x,
                a.min.y < b.min.y ? a.min.y : b.min.y 
            },
            { 
                a.max.x > b.max.x ? a.max.x : b.max.x,
                a.max.y > b.max.y ? a.max.y : b.max.y 
            } 
        };
    }
};

struct Transform2D {
    vector2d position{}; // meters
    double angle = 0.0;  // radians
    double cos_a = 1.0;
    double sin_a = 0.0;

    constexpr Transform2D() noexcept = default;

    Transform2D(const vector2d& pos, double angle_rad) noexcept
        : position(pos), angle(angle_rad),
          cos_a(std::cos(angle_rad)), sin_a(std::sin(angle_rad)) {}

    static constexpr Transform2D identity() noexcept { return {}; }

    constexpr vector2d apply(const vector2d& local) const noexcept {
        return { cos_a * local.x - sin_a * local.y + position.x, sin_a * local.x + cos_a * local.y + position.y };
    }

    constexpr vector2d apply_inverse(const vector2d& world) const noexcept {
        double dx = world.x - position.x;
        double dy = world.y - position.y;
        return {  cos_a * dx + sin_a * dy, -sin_a * dx + cos_a * dy };
    }

    constexpr vector2d rotate(const vector2d& dir) const noexcept {
        return { cos_a * dir.x - sin_a * dir.y, sin_a * dir.x + cos_a * dir.y };
    }

    constexpr vector2d rotate_inverse(const vector2d& dir) const noexcept {
        return { cos_a * dir.x + sin_a * dir.y, -sin_a * dir.x + cos_a * dir.y };
    }

    Transform2D compose(const Transform2D& other) const noexcept {
        return Transform2D(apply(other.position), angle + other.angle);
    }

    void set_angle(double rad) noexcept {
        angle = rad;
        cos_a = std::cos(rad);
        sin_a = std::sin(rad);
    }
};

struct Sweep2D {
    vector2d center_local{}; // meters
    vector2d position0{};    // meters
    vector2d position1{};    // meters
    double angle0 = 0.0;     // radians
    double angle1 = 0.0;     // radians

    Transform2D get_transform(double t) const noexcept;
};

struct CircleShape {
    vector2d center{};   // meters, local body space
    double radius = 0.0; // meters
};

struct PolygonShape {
    std::array<vector2d, MAX_POLYGON_VERTICES> vertices{}; // meters, local
    std::array<vector2d, MAX_POLYGON_VERTICES> normals{};  // unitless, unit vectors
    vector2d centroid{}; // meters, local
    std::size_t count = 0;

    void set(const vector2d* pts, std::size_t n);

    static PolygonShape make_box(double hx, double hy);

    static PolygonShape make_box(double hx, double hy, const vector2d& center, double angle_rad);
};

struct CapsuleShape {
    vector2d point1{}; // meters, local
    vector2d point2{}; // meters, local
    double radius = 0.0; // meters
    constexpr vector2d center() const noexcept { return (point1 + point2) * 0.5; }
    constexpr double half_length() const noexcept { return (point2 - point1).magnitude() * 0.5; } // m
};

struct EdgeShape {
    vector2d v1{}, v2{};         // meters, local
    vector2d ghost1{}, ghost2{}; // meters, local
    bool has_ghost1 = false;
    bool has_ghost2 = false;

    constexpr vector2d normal() const noexcept {
        vector2d e = v2 - v1;
        return vector2d(e.y, -e.x).unit_vector();
    }
};

enum class ShapeType : std::uint8_t {
    Circle = 0,
    Polygon,
    Capsule,
    Edge
};

struct Shape2D {
    ShapeType type = ShapeType::Circle;

    union {
        CircleShape  circle;
        PolygonShape polygon;
        CapsuleShape capsule;
        EdgeShape    edge;
    };

    double bounding_radius = 0.0; // meters
    AABB   local_aabb{};

    Shape2D() noexcept : circle{} {}
    ~Shape2D() = default;
    Shape2D(const Shape2D& o) noexcept { copy_from(o); }
    Shape2D& operator=(const Shape2D& o) noexcept { if (this != &o) copy_from(o); return *this; }

    static Shape2D make_circle(const vector2d& center, double radius);

    static Shape2D make_polygon(const vector2d* pts, std::size_t n);

    static Shape2D make_box(double hx, double hy);

    static Shape2D make_capsule(const vector2d& p1, const vector2d& p2, double radius);

    static Shape2D make_edge(const vector2d& v1, const vector2d& v2);

    AABB compute_aabb(const Transform2D& xf) const noexcept;

private:
    void compute_polygon_bounds();

    void copy_from(const Shape2D& o) noexcept;
};

struct MassData {
    double mass    = 0.0; // kg
    double inertia = 0.0; // kg*m2, about centroid
    vector2d center{};    // meters, local body space

    static MassData combine(const MassData* parts, std::size_t n);
};

namespace detail {

inline double circle_area(const CircleShape& c) { return constants::pi() * c.radius * c.radius; }

double polygon_area(const PolygonShape& p);

double capsule_area(const CapsuleShape& c);

inline double edge_area(const EdgeShape&) { return 0.0; }

} // namespace detail

double compute_shape_area(const Shape2D& shape);

namespace detail {

MassData compute_circle_mass(const CircleShape& c, double density);

MassData compute_polygon_mass(const PolygonShape& p, double density);

MassData compute_capsule_mass(const CapsuleShape& c, double density);

inline MassData compute_edge_mass(const EdgeShape&, double) { return { 0.0, 0.0, {} }; }

template <class FromU, class ToU>
inline typename std::enable_if<is_fizmo_unit_v<FromU> && is_fizmo_unit_v<ToU>, vector2d>::type
convert_velocity_vec(const vector2d& v, FromU from, ToU to) {
    return { convert_velocity_scalar(v.x, from, to), convert_velocity_scalar(v.y, from, to) };
}

template <class FromU, class ToU>
inline typename std::enable_if<is_fizmo_unit_v<FromU> && is_fizmo_unit_v<ToU>, vector2d>::type
convert_force_vec(const vector2d& v, FromU from, ToU to) {
    return { convert_force_scalar(v.x, from, to), convert_force_scalar(v.y, from, to) };
}

template <class FromU, class ToU>
inline typename std::enable_if<is_fizmo_unit_v<FromU> && is_fizmo_unit_v<ToU>, vector2d>::type
convert_position_vec(const vector2d& v, FromU from, ToU to) {
    return { convert_distance_scalar(v.x, from, to), convert_distance_scalar(v.y, from, to) };
}

} // namespace detail

MassData compute_mass(const Shape2D& shape, double density);

struct MassSpec {
public:
    static constexpr double STANDARD_GRAVITY = constants::gravity(); // m/s2

public:
    MassMode mode  = MassMode::Density;
    double   value = 1.0;

public:
    constexpr MassSpec() noexcept = default;
    constexpr MassSpec(MassMode m, double v) noexcept : mode(m), value(v) {}

public:
    static constexpr MassSpec from_density(double kg_per_m2) noexcept { return { MassMode::Density, kg_per_m2 }; }
    static constexpr MassSpec from_mass(double kg) noexcept { return { MassMode::Mass, kg }; }
    static constexpr MassSpec from_weight(double newtons) noexcept { return { MassMode::Weight, newtons }; }

    template <class U, typename std::enable_if<is_fizmo_unit_v<U> && U::dimension() == units::DIMENSION_DENSITY_2D, int>::type = 0>
    static MassSpec from_density(double val, U unit) { return { MassMode::Density, detail::convert_weight_scalar(val, unit, (units::mass::kilogram / units::area::square_meter)) }; }

    template <class U, typename std::enable_if<is_fizmo_unit_v<U> && U::dimension() == units::DIMENSION_MASS, int>::type = 0>
    static MassSpec from_mass(double val, U unit)    { return { MassMode::Mass   , detail::convert_weight_scalar(val, unit, units::mass::kilogram) }; }

    template <class U, typename std::enable_if<is_fizmo_unit_v<U> && U::dimension() == units::DIMENSION_FORCE, int>::type = 0>
    static MassSpec from_weight(double val, U unit)  { return { MassMode::Weight , detail::convert_force_scalar (val, unit, units::force::newton ) }; }

public:
    double resolve_density(double shape_area_m2, double gravity = STANDARD_GRAVITY) const noexcept;

    double to_mass(double shape_area_m2, double gravity = STANDARD_GRAVITY) const noexcept {
        return resolve_density(shape_area_m2, gravity) * shape_area_m2;
    }
};

struct Material {
    MassSpec mass_spec{};                  
    double   friction    = 0.3;           // unitless [0, 1]
    double   restitution = 0.0;           // unitless [0, 1]
    double   restitution_threshold = 1.0; // m/s

    constexpr Material() noexcept = default;
    explicit constexpr Material(double density) noexcept : mass_spec(MassSpec::from_density(density)) {}
    constexpr Material(const MassSpec& spec, double fric = 0.3, double rest = 0.0, double rest_thresh = 1.0) noexcept : mass_spec(spec), friction(fric), restitution(rest), restitution_threshold(rest_thresh) {}

    static constexpr Material with_density(double kg_per_m2) noexcept { return Material(MassSpec::from_density(kg_per_m2)); }
    static constexpr Material with_mass(double kg) noexcept { return Material(MassSpec::from_mass(kg)); }
    static constexpr Material with_weight(double newtons) noexcept { return Material(MassSpec::from_weight(newtons)); }

    template <class U, typename std::enable_if<is_fizmo_unit_v<U> && U::dimension() == units::DIMENSION_DENSITY_2D, int>::type = 0>
    static Material with_density(double val, U unit) { return Material(MassSpec::from_density(val, unit)); }

    template <class U, typename std::enable_if<is_fizmo_unit_v<U> && U::dimension() == units::DIMENSION_MASS, int>::type = 0>
    static Material with_mass(double val, U unit) { return Material(MassSpec::from_mass(val, unit)); }

    template <class U, typename std::enable_if<is_fizmo_unit_v<U> && U::dimension() == units::DIMENSION_FORCE, int>::type = 0>
    static Material with_weight(double val, U unit) { return Material(MassSpec::from_weight(val, unit)); }
};

class RigidBody2D;

struct Collider2D {
    Shape2D         shape;
    Transform2D     local_offset;
    Material        material;
    CollisionFilter filter;
    bool            is_sensor = false;
    RigidBody2D*    body = nullptr;

    AABB            world_aabb{};
    MassData        mass_data{};

    void recompute_mass(double gravity = MassSpec::STANDARD_GRAVITY);

    void update_world_aabb(const Transform2D& body_xf);
};

class RigidBody2D {
public:
    RigidBody2D() = default;
    explicit RigidBody2D(PhysicsFlags preset) : m_flags(preset) {}

public:
    PhysicsFlags flags() const noexcept { return m_flags; }

    const Transform2D& transform() const noexcept { return m_transform; }
          Transform2D& transform()       noexcept { return m_transform; }

    const Sweep2D& sweep() const noexcept { return m_sweep; }
          Sweep2D& sweep()       noexcept { return m_sweep; }

    const std::vector<Collider2D>& colliders() const noexcept { return m_colliders; }

    // Caller should call mark_mass_dirty() after mutating
    std::vector<Collider2D>& colliders_mut() noexcept { return m_colliders; }

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
    void set_flag(PhysicsFlags f)                     noexcept { m_flags |= f;  }
    void clear_flag(PhysicsFlags f)                   noexcept { m_flags &= ~f; }
    void toggle_flag(PhysicsFlags f)                  noexcept { m_flags ^= f;  }
    void set_flag_to(PhysicsFlags f, bool on)         noexcept { on ? set_flag(f) : clear_flag(f); }

    bool is_frozen(FrozenProperty prop) const noexcept { return has_frozen(m_frozen, prop); }
    void unfreeze(FrozenProperty prop)        noexcept { m_frozen &= ~prop; }
    void unfreeze_all()                       noexcept { m_frozen = FrozenProperty::None; }

public:
    // Call after externally mutating colliders, materials, offsets, or flags
    void mark_mass_dirty() const noexcept { m_mass_dirty = true; }

    bool can_collide_with(const RigidBody2D& other) const noexcept;

    bool is_mass_dirty() const noexcept { return m_mass_dirty; }

    void ensure_mass_uptodate(double gravity = MassSpec::STANDARD_GRAVITY) const {
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

    Collider2D& add_collider(
        Shape2D shape,
        Material mat = {},
        Transform2D offset = {},
        CollisionFilter filt = {},
        bool sensor = false
    );

public:
    vector2d position() const noexcept { return m_transform.position; }

    template <class U, typename std::enable_if<is_fizmo_unit_v<U> && U::dimension() == units::DIMENSION_LENGTH, int>::type = 0>
    vector2d position(U distance_unit) const noexcept {
        double a = units::convert(m_transform.position.x, units::distance::meter, distance_unit);
        double b = units::convert(m_transform.position.y, units::distance::meter, distance_unit);
        return vector2d(a, b);
    }

    void set_position(const vector2d& pos) noexcept { m_transform.position = pos; }

    template <class U, typename std::enable_if<is_fizmo_unit_v<U> && U::dimension() == units::DIMENSION_LENGTH, int>::type = 0>
    void set_position(const vector2d& pos, U distance_unit) noexcept {
        m_transform.position.x = units::convert(pos.x, distance_unit, units::distance::meter);
        m_transform.position.y = units::convert(pos.y, distance_unit, units::distance::meter);
    }

public:
    double angle(bool in_rad = true) const noexcept { return in_rad ? m_transform.angle : (m_transform.angle * constants::reciprocal_pi_180()); }

    void set_angle(double angle, bool in_rad = true) noexcept {
        if (!in_rad) { angle *= constants::pi_180(); }
        m_transform.set_angle(angle);
    }

public:
    vector2d linear_velocity() const noexcept { return m_linear_velocity; }

    template <class U, typename std::enable_if<is_fizmo_unit_v<U> && U::dimension() == units::DIMENSION_VELOCITY, int>::type = 0>
    vector2d linear_velocity(U unit) const noexcept {
        double a = units::convert(m_linear_velocity.x, units::velocity::meters_per_second, unit);
        double b = units::convert(m_linear_velocity.y, units::velocity::meters_per_second, unit);
        return vector2d(a, b);
    }

    void set_linear_velocity(const vector2d& v) noexcept {
        m_linear_velocity = v;
        if (v.x != 0.0 || v.y != 0.0) wake();
    }

    template <class U, typename std::enable_if<is_fizmo_unit_v<U> && U::dimension() == units::DIMENSION_VELOCITY, int>::type = 0>
    void set_linear_velocity(const vector2d& v, U unit) noexcept {
        set_linear_velocity(detail::convert_velocity_vec(v, unit, units::velocity::meters_per_second));
    }

public:
    double angular_velocity() const noexcept { return m_angular_velocity; }

    template <class U, typename std::enable_if<is_fizmo_unit_v<U> && U::dimension() == units::DIMENSION_ANGULAR_VELOCITY, int>::type = 0>
    double angular_velocity(U unit) const noexcept {
        static constexpr auto rad_per_s = units::angle::radian / units::time::second;
        return detail::convert_angular_velocity_scalar(m_angular_velocity, rad_per_s, unit);
    }

    void set_angular_velocity(double w) noexcept {
        m_angular_velocity = w;
        if (w != 0.0) wake();
    }

    template <class U, typename std::enable_if<is_fizmo_unit_v<U> && U::dimension() == units::DIMENSION_ANGULAR_VELOCITY, int>::type = 0>
    void set_angular_velocity(double w, U unit) noexcept {
        static constexpr auto rad_per_s = units::angle::radian / units::time::second;
        set_angular_velocity(detail::convert_angular_velocity_scalar(w, unit, rad_per_s));
    }

public:
    vector2d force() const noexcept { return m_force; }

    template <class U, typename std::enable_if<is_fizmo_unit_v<U> && U::dimension() == units::DIMENSION_FORCE, int>::type = 0>
    vector2d force(U force_unit) const noexcept {
        return detail::convert_force_vec(m_force, units::force::newton, force_unit);
    }

    void set_force(const vector2d& f) noexcept { m_force = f; }

    template <class U, typename std::enable_if<is_fizmo_unit_v<U> && U::dimension() == units::DIMENSION_FORCE, int>::type = 0>
    void set_force(const vector2d& f, U unit) noexcept {
        m_force = detail::convert_force_vec(f, unit, units::force::newton);
    }

public:
    double torque() const noexcept { return m_torque; }

    template <class U, typename std::enable_if<is_fizmo_unit_v<U> && U::dimension() == units::DIMENSION_TORQUE, int>::type = 0>
    double torque(U unit) const noexcept {
        return detail::convert_torque_scalar(m_torque, units::torque::newton_meter, unit);
    }

    void set_torque(double t) noexcept { m_torque = t; }

    template <class U, typename std::enable_if<is_fizmo_unit_v<U> && U::dimension() == units::DIMENSION_TORQUE, int>::type = 0>
    void set_torque(double t, U torque_unit) noexcept {
        m_torque = detail::convert_torque_scalar(t, torque_unit, units::torque::newton_meter);
    }

public:
    double density() const noexcept {
        ensure_mass_uptodate();
        return m_density;
    }

    template <class U, typename std::enable_if<is_fizmo_unit_v<U> && U::dimension() == units::DIMENSION_DENSITY_2D, int>::type = 0>
    double density(U density_unit) const noexcept {
        ensure_mass_uptodate();
        static constexpr auto si_density = units::mass::kilogram / units::area::square_meter;
        return static_cast<double>(units::convert(static_cast<long double>(m_density), si_density, density_unit));
    }

    void set_density(double d) noexcept;

    template <class U, typename std::enable_if<is_fizmo_unit_v<U> && U::dimension() == units::DIMENSION_DENSITY_2D, int>::type = 0>
    void set_density(double d, U density_unit) noexcept {
        static constexpr auto si_density = units::mass::kilogram / units::area::square_meter;
        d = units::convert(d, density_unit, si_density);
        set_density(d);
    }

public:
    double mass(double gravity = MassSpec::STANDARD_GRAVITY) const noexcept { 
        ensure_mass_uptodate(gravity);
        return m_mass; 
    }

    template <class U, typename std::enable_if<is_fizmo_unit_v<U> && U::dimension() == units::DIMENSION_MASS, int>::type = 0>
    double mass(U mass_unit, double gravity = MassSpec::STANDARD_GRAVITY) const noexcept {
        ensure_mass_uptodate(gravity);
        return detail::convert_weight_scalar(m_mass, units::mass::kilogram, mass_unit);
    }

    double inv_mass(double gravity = MassSpec::STANDARD_GRAVITY) const noexcept { 
        ensure_mass_uptodate(gravity);
        return m_inv_mass; 
    }

    template <class U, typename std::enable_if<is_fizmo_unit_v<U> && U::dimension() == units::DIMENSION_MASS, int>::type = 0>
    double inv_mass(U unit, double gravity = MassSpec::STANDARD_GRAVITY) const noexcept { 
        ensure_mass_uptodate(gravity);
        return units::convert(m_inv_mass, units::mass::kilogram, unit); 
    }

    void set_mass(double kg) noexcept;

    template <class U, typename std::enable_if<is_fizmo_unit_v<U> && U::dimension() == units::DIMENSION_MASS, int>::type = 0>
    void set_mass(double val, U mass_unit) noexcept {
        set_mass(detail::convert_weight_scalar(val, mass_unit, units::mass::kilogram));
    }

    void set_mass_from_weight(double newtons, double gravity = MassSpec::STANDARD_GRAVITY) noexcept {
        set_mass((gravity > 0.0) ? newtons / gravity : 0.0);
    }

public:
    double inertia(double gravity = MassSpec::STANDARD_GRAVITY) const noexcept { 
        ensure_mass_uptodate(gravity);
        return m_inertia; 
    }

    double inv_inertia(double gravity = MassSpec::STANDARD_GRAVITY) const noexcept { 
        ensure_mass_uptodate(gravity);
        return m_inv_inertia; 
    }

public:
    double shape_area() const noexcept {
        ensure_mass_uptodate();
        return m_shape_area;
    }

public:
    vector2d local_center() const noexcept {
        ensure_mass_uptodate();
        return m_local_center;
    }

    template <class U, typename std::enable_if<is_fizmo_unit_v<U> && U::dimension() == units::DIMENSION_LENGTH, int>::type = 0>
    vector2d local_center(U length_unit) const noexcept {
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
    void apply_force(const vector2d& f) {
        if (!has(PhysicsFlags::LinearForces)) return;
        m_force += f;
        wake();
    }

    template <class U, typename std::enable_if<is_fizmo_unit_v<U> && U::dimension() == units::DIMENSION_FORCE, int>::type = 0>
    void apply_force(const vector2d& f, U unit) {
        apply_force(detail::convert_force_vec(f, unit, units::force::newton));
    }

    void apply_force_at(const vector2d& f, const vector2d& world_point);

    template <class FU, class PU, typename std::enable_if<is_fizmo_unit_v<FU> && is_fizmo_unit_v<PU> && FU::dimension() == units::DIMENSION_FORCE && PU::dimension() == units::DIMENSION_LENGTH, int>::type = 0>
    void apply_force_at(const vector2d& f, FU f_unit, const vector2d& world_point, PU p_unit) {
        apply_force_at(
            detail::convert_force_vec(f, f_unit, units::force::newton),
            detail::convert_position_vec(world_point, p_unit, units::distance::meter)
        );
    }

public:
    void apply_torque(double t) {
        if (!has(PhysicsFlags::Torque)) return;
        m_torque += t;
        wake();
    }

    template <class U, typename std::enable_if<is_fizmo_unit_v<U> && U::dimension() == units::DIMENSION_TORQUE, int>::type = 0>
    void apply_torque(double t, U torque_unit) {
        apply_torque(detail::convert_torque_scalar(t, torque_unit, units::torque::newton_meter));
    }

public:
    void apply_linear_impulse(const vector2d& impulse);

    template <class U, typename std::enable_if<is_fizmo_unit_v<U> && U::dimension() == units::DIMENSION_IMPULSE, int>::type = 0>
    void apply_linear_impulse(const vector2d& impulse, U impulse_unit) {
        double f = detail::convert_impulse_scalar(1.0, impulse_unit, units::impulse::newton_second);
        apply_linear_impulse(impulse * f);
    }

    void apply_impulse_at(const vector2d& impulse, const vector2d& world_point);

    void apply_angular_impulse(double impulse);

public:
    void integrate_forces(double dt, const vector2d& gravity);

    template <class U, typename std::enable_if<is_fizmo_unit_v<U> && U::dimension() == units::DIMENSION_ACCELERATION, int>::type = 0>
    void integrate_forces(double dt, const vector2d& gravity, U grav_accel_unit) {
        double a = units::convert(gravity.x, grav_accel_unit, units::acceleration::meters_per_second_squared);
        double b = units::convert(gravity.y, grav_accel_unit, units::acceleration::meters_per_second_squared);
        return integrate_forces(dt, vector2d(a, b));
    }

    void integrate_velocities(double dt);

    void apply_position_correction(const vector2d& d_center, double d_angle);

    void clear_forces() {
        m_force  = {};
        m_torque = 0.0;
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

    AABB compute_body_aabb() const;

    vector2d world_center() const noexcept {
        ensure_mass_uptodate();
        return m_transform.apply(m_local_center);
    }

    template <class U, typename std::enable_if<is_fizmo_unit_v<U> && U::dimension() == units::DIMENSION_LENGTH, int>::type = 0>
    vector2d world_center(U distance_unit) const noexcept {
        return detail::convert_position_vec(world_center(), units::distance::meter, distance_unit);
    }

    vector2d get_world_point(const vector2d& local)  const noexcept { return m_transform.apply(local); }
    vector2d get_local_point(const vector2d& world)  const noexcept { return m_transform.apply_inverse(world); }
    vector2d get_world_vector(const vector2d& local) const noexcept { return m_transform.rotate(local); }
    vector2d get_local_vector(const vector2d& world) const noexcept { return m_transform.rotate_inverse(world); }

    vector2d get_velocity_at(const vector2d& world_point) const noexcept;

    template <class PU, class VU, typename std::enable_if<is_fizmo_unit_v<PU> && is_fizmo_unit_v<VU> && PU::dimension() == units::DIMENSION_LENGTH && VU::dimension() == units::DIMENSION_VELOCITY, int>::type = 0>
    vector2d get_velocity_at(const vector2d& world_point, PU position_distance_unit, VU velocity_unit) const noexcept {
        vector2d wp_si = detail::convert_position_vec(world_point, position_distance_unit, units::distance::meter);
        return detail::convert_velocity_vec(get_velocity_at(wp_si), units::velocity::meters_per_second, velocity_unit);
    }

    double kinetic_energy() const noexcept;

    template <class U, typename std::enable_if<is_fizmo_unit_v<U> && U::dimension() == units::DIMENSION_ENERGY, int>::type = 0>
    double kinetic_energy(U energy_unit) const noexcept {
        return static_cast<double>(
            units::convert(
                static_cast<long double>(kinetic_energy()),
                units::energy::joule, energy_unit
            )
        );
    }

private:
    static bool validate_frozen(FrozenProperty f) noexcept;

    void recompute_mass_data(double gravity = MassSpec::STANDARD_GRAVITY) const;

    void resolve_mass_constraint_from(FrozenProperty just_set) noexcept;

    void derive_mass_from_density_area() noexcept;

    void derive_area_from_mass_density() noexcept;

    void scale_collider_geometry(double s) noexcept;

    static Shape2D rebuild_shape_bounds(const Shape2D& src) noexcept;

    void refresh_shape_area() const noexcept;

    void recompute_inertia_from_density() noexcept;

private:
    PhysicsFlags m_flags = PhysicsFlags::DefaultDynamic;

    std::uint64_t m_collision_layer = 1ULL;
    std::uint64_t m_collision_mask  = std::numeric_limits<std::uint64_t>::max();

    Transform2D     m_transform;
    mutable Sweep2D m_sweep;

    vector2d m_linear_velocity{};       // m/s
    double   m_angular_velocity = 0.0;  // rad/s
    vector2d m_force{};                 // N
    double   m_torque = 0.0;            // N*m

    mutable double  m_density    = 1.0; // kg/m2
    FrozenProperty  m_frozen     = FrozenProperty::None;
    mutable double  m_shape_area = 0.0;  

    mutable bool     m_mass_dirty  = true;
    mutable double   m_mass        = 0.0;       // kg
    mutable double   m_inv_mass    = 0.0;       // 1/kg
    mutable double   m_inertia     = 0.0;       // kg*m2
    mutable double   m_inv_inertia = 0.0;       // 1/(kg*m2)
    mutable vector2d m_local_center{};          // m

    double m_linear_damping   = 0.0;    // 1/s
    double m_angular_damping  = 0.0;    // 1/s
    double m_max_linear_speed  = 1e6;   // m/s
    double m_max_angular_speed = 1e6;   // rad/s

    double m_sleep_timer = 0.0;         // s
    bool   m_is_awake    = true;

    double m_gravity_scale = 1.0;

    std::vector<Collider2D> m_colliders;
};

} // namespace physics
} // namespace fizmo

#endif // FIZMO_PHYSICS_2D_HPP