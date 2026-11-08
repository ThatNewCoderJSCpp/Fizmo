#ifndef FIZMO_PHYSICS_2D_HPP
#define FIZMO_PHYSICS_2D_HPP

#include "common.hpp"

namespace fizmo {
namespace physics {

inline constexpr std::size_t MAX_POLYGON_VERTICES = 8;

struct AABB {
    RVec2 min{}; // meters
    RVec2 max{}; // meters

    constexpr AABB() noexcept = default;
    constexpr AABB(const RVec2& lo, const RVec2& hi) noexcept : min(lo), max(hi) {}

    RVec2 center() const noexcept { return (min + max) * 0.5_r; }
    RVec2 extents() const noexcept { return (max - min) * 0.5_r; }

    constexpr Real perimeter() const noexcept { 
        Real w = max.x - min.x, h = max.y - min.y;
        return 2.0_r * (w + h);
    }

    constexpr Real area() const noexcept { return (max.x - min.x) * (max.y - min.y); }
    constexpr bool overlaps(const AABB& o) const noexcept { return min.x <= o.max.x && max.x >= o.min.x && min.y <= o.max.y && max.y >= o.min.y; }
    constexpr bool contains(const RVec2& p) const noexcept { return p.x >= min.x && p.x <= max.x && p.y >= min.y && p.y <= max.y; }
    constexpr bool contains(const AABB& o) const noexcept { return min.x <= o.min.x && min.y <= o.min.y && max.x >= o.max.x && max.y >= o.max.y; }

    // margin in meters
    AABB fatten(Real margin) const noexcept {
        return { {min.x - margin, min.y - margin}, {max.x + margin, max.y + margin} };
    }

    static AABB merge(const AABB& a, const AABB& b) noexcept {
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
    RVec2 position{}; // meters
    Real angle = 0.0_r;  // radians
    Real cos_a = 1.0_r;
    Real sin_a = 0.0_r;

    constexpr Transform2D() noexcept = default;

    Transform2D(const RVec2& pos, Real angle_rad) noexcept
        : position(pos), angle(angle_rad),
          cos_a(std::cos(angle_rad)), sin_a(std::sin(angle_rad)) {}

    static Transform2D identity() noexcept { return {}; }

    RVec2 apply(const RVec2& local) const noexcept {
        return { cos_a * local.x - sin_a * local.y + position.x, sin_a * local.x + cos_a * local.y + position.y };
    }

    RVec2 apply_inverse(const RVec2& world) const noexcept {
        Real dx = world.x - position.x;
        Real dy = world.y - position.y;
        return {  cos_a * dx + sin_a * dy, -sin_a * dx + cos_a * dy };
    }

    RVec2 rotate(const RVec2& dir) const noexcept {
        return { cos_a * dir.x - sin_a * dir.y, sin_a * dir.x + cos_a * dir.y };
    }

    RVec2 rotate_inverse(const RVec2& dir) const noexcept {
        return { cos_a * dir.x + sin_a * dir.y, -sin_a * dir.x + cos_a * dir.y };
    }

    Transform2D compose(const Transform2D& other) const noexcept {
        return Transform2D(apply(other.position), angle + other.angle);
    }

    void set_angle(Real rad) noexcept {
        angle = rad;
        cos_a = std::cos(rad);
        sin_a = std::sin(rad);
    }
};

struct Sweep2D {
    RVec2 center_local{}; // meters
    RVec2 position0{};    // meters
    RVec2 position1{};    // meters
    Real angle0 = 0.0_r;     // radians
    Real angle1 = 0.0_r;     // radians

    Transform2D get_transform(Real t) const noexcept;
};

struct CircleShape {
    RVec2 center{};   // meters, local body space
    Real radius = 0.0_r; // meters
};

struct PolygonShape {
    std::array<RVec2, MAX_POLYGON_VERTICES> vertices{}; // meters, local
    std::array<RVec2, MAX_POLYGON_VERTICES> normals{};  // unitless, unit vectors
    RVec2 centroid{}; // meters, local
    std::size_t count = 0;

    void set(const RVec2* pts, std::size_t n);

    static PolygonShape make_box(Real hx, Real hy);

    static PolygonShape make_box(Real hx, Real hy, const RVec2& center, Real angle_rad);
};

struct CapsuleShape {
    RVec2 point1{}; // meters, local
    RVec2 point2{}; // meters, local
    Real radius = 0.0_r; // meters
    RVec2 center() const noexcept { return (point1 + point2) * 0.5_r; }
    Real half_length() const noexcept { return fast::length(point2 - point1) * 0.5_r; } // m
};

struct EdgeShape {
    RVec2 v1{}, v2{};         // meters, local
    RVec2 ghost1{}, ghost2{}; // meters, local
    bool has_ghost1 = false;
    bool has_ghost2 = false;

    RVec2 normal() const noexcept {
        RVec2 e = v2 - v1;
        return fast::normalize(RVec2(e.y, -e.x));
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

    Real bounding_radius = 0.0_r; // meters
    AABB   local_aabb{};

    Shape2D() noexcept : circle{} {}
    ~Shape2D() = default;
    Shape2D(const Shape2D& o) noexcept { copy_from(o); }
    Shape2D& operator=(const Shape2D& o) noexcept { if (this != &o) copy_from(o); return *this; }

    static Shape2D make_circle(const RVec2& center, Real radius);

    static Shape2D make_polygon(const RVec2* pts, std::size_t n);

    static Shape2D make_box(Real hx, Real hy);

    static Shape2D make_capsule(const RVec2& p1, const RVec2& p2, Real radius);

    static Shape2D make_edge(const RVec2& v1, const RVec2& v2);

    AABB compute_aabb(const Transform2D& xf) const noexcept;

private:
    void compute_polygon_bounds();

    void copy_from(const Shape2D& o) noexcept;
};

struct MassData {
    Real mass    = 0.0_r; // kg
    Real inertia = 0.0_r; // kg*m2, about centroid
    RVec2 center{};    // meters, local body space

    static MassData combine(const MassData* parts, std::size_t n);
};

namespace detail {

inline Real circle_area(const CircleShape& c) { return constants::pi() * c.radius * c.radius; }

Real polygon_area(const PolygonShape& p);

Real capsule_area(const CapsuleShape& c);

inline Real edge_area(const EdgeShape&) { return 0.0_r; }

} // namespace detail

Real compute_shape_area(const Shape2D& shape);

namespace detail {

MassData compute_circle_mass(const CircleShape& c, Real density);

MassData compute_polygon_mass(const PolygonShape& p, Real density);

MassData compute_capsule_mass(const CapsuleShape& c, Real density);

inline MassData compute_edge_mass(const EdgeShape&, Real) { return { 0.0_r, 0.0_r, {} }; }

template <class FromU, class ToU>
inline typename std::enable_if<is_fizmo_unit_v<FromU> && is_fizmo_unit_v<ToU>, RVec2>::type
convert_velocity_vec(const RVec2& v, FromU from, ToU to) {
    return { convert_velocity_scalar(v.x, from, to), convert_velocity_scalar(v.y, from, to) };
}

template <class FromU, class ToU>
inline typename std::enable_if<is_fizmo_unit_v<FromU> && is_fizmo_unit_v<ToU>, RVec2>::type
convert_force_vec(const RVec2& v, FromU from, ToU to) {
    return { convert_force_scalar(v.x, from, to), convert_force_scalar(v.y, from, to) };
}

template <class FromU, class ToU>
inline typename std::enable_if<is_fizmo_unit_v<FromU> && is_fizmo_unit_v<ToU>, RVec2>::type
convert_position_vec(const RVec2& v, FromU from, ToU to) {
    return { convert_distance_scalar(v.x, from, to), convert_distance_scalar(v.y, from, to) };
}

} // namespace detail

MassData compute_mass(const Shape2D& shape, Real density);

struct MassSpec {
public:
    static constexpr Real STANDARD_GRAVITY = constants::gravity(); // m/s2

public:
    MassMode mode  = MassMode::Density;
    Real   value = 1.0_r;

public:
    constexpr MassSpec() noexcept = default;
    constexpr MassSpec(MassMode m, Real v) noexcept : mode(m), value(v) {}

public:
    static constexpr MassSpec from_density(Real kg_per_m2) noexcept { return { MassMode::Density, kg_per_m2 }; }
    static constexpr MassSpec from_mass(Real kg) noexcept { return { MassMode::Mass, kg }; }
    static constexpr MassSpec from_weight(Real newtons) noexcept { return { MassMode::Weight, newtons }; }

    template <class U, typename std::enable_if<is_fizmo_unit_v<U> && U::dimension() == units::DIMENSION_DENSITY_2D, int>::type = 0>
    static MassSpec from_density(Real val, U unit) { return { MassMode::Density, detail::convert_weight_scalar(val, unit, (units::mass::kilogram / units::area::square_meter)) }; }

    template <class U, typename std::enable_if<is_fizmo_unit_v<U> && U::dimension() == units::DIMENSION_MASS, int>::type = 0>
    static MassSpec from_mass(Real val, U unit)    { return { MassMode::Mass   , detail::convert_weight_scalar(val, unit, units::mass::kilogram) }; }

    template <class U, typename std::enable_if<is_fizmo_unit_v<U> && U::dimension() == units::DIMENSION_FORCE, int>::type = 0>
    static MassSpec from_weight(Real val, U unit)  { return { MassMode::Weight , detail::convert_force_scalar (val, unit, units::force::newton ) }; }

public:
    Real resolve_density(Real shape_area_m2, Real gravity = STANDARD_GRAVITY) const noexcept;

    Real to_mass(Real shape_area_m2, Real gravity = STANDARD_GRAVITY) const noexcept {
        return resolve_density(shape_area_m2, gravity) * shape_area_m2;
    }
};

struct Material {
    MassSpec mass_spec{};                  
    Real   friction    = 0.3_r;           // unitless [0, 1]
    Real   restitution = 0.0_r;           // unitless [0, 1]
    Real   restitution_threshold = 1.0_r; // m/s
    CombineMode friction_combine = CombineMode::Default;
    CombineMode restitution_combine = CombineMode::Default;

    constexpr Material() noexcept = default;
    explicit constexpr Material(Real density) noexcept : mass_spec(MassSpec::from_density(density)) {}
    constexpr Material(const MassSpec& spec, Real fric = 0.3_r, Real rest = 0.0_r, Real rest_thresh = 1.0_r) noexcept : mass_spec(spec), friction(fric), restitution(rest), restitution_threshold(rest_thresh) {}

    static constexpr Material with_density(Real kg_per_m2) noexcept { return Material(MassSpec::from_density(kg_per_m2)); }
    static constexpr Material with_mass(Real kg) noexcept { return Material(MassSpec::from_mass(kg)); }
    static constexpr Material with_weight(Real newtons) noexcept { return Material(MassSpec::from_weight(newtons)); }

    template <class U, typename std::enable_if<is_fizmo_unit_v<U> && U::dimension() == units::DIMENSION_DENSITY_2D, int>::type = 0>
    static Material with_density(Real val, U unit) { return Material(MassSpec::from_density(val, unit)); }

    template <class U, typename std::enable_if<is_fizmo_unit_v<U> && U::dimension() == units::DIMENSION_MASS, int>::type = 0>
    static Material with_mass(Real val, U unit) { return Material(MassSpec::from_mass(val, unit)); }

    template <class U, typename std::enable_if<is_fizmo_unit_v<U> && U::dimension() == units::DIMENSION_FORCE, int>::type = 0>
    static Material with_weight(Real val, U unit) { return Material(MassSpec::from_weight(val, unit)); }
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

    void recompute_mass(Real gravity = MassSpec::STANDARD_GRAVITY);

    void update_world_aabb(const Transform2D& body_xf);
};

class RigidBody2D {
    friend class Simulation2D;

public:
    static void* operator new(std::size_t size);
    static void operator delete(void* p, std::size_t size) noexcept;
    static std::size_t pooled_bodies();
    BodyHandle handle() const noexcept { return m_handle; }

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
    void set_flag(PhysicsFlags f)                     noexcept { m_flags |= f;  touch_rotation_lock(f); }
    void clear_flag(PhysicsFlags f)                   noexcept { m_flags &= ~f; touch_rotation_lock(f); }
    void toggle_flag(PhysicsFlags f)                  noexcept { m_flags ^= f;  touch_rotation_lock(f); }
    void set_flag_to(PhysicsFlags f, bool on)         noexcept { on ? set_flag(f) : clear_flag(f); }

    bool rotation_locked() const noexcept {
        return has(PhysicsFlags::LockRotationZ) || has(PhysicsFlags::LockRotationX | PhysicsFlags::LockRotationY);
    }

    bool is_frozen(FrozenProperty prop) const noexcept { return has_frozen(m_frozen, prop); }
    void unfreeze(FrozenProperty prop)        noexcept { m_frozen &= ~prop; }
    void unfreeze_all()                       noexcept { m_frozen = FrozenProperty::None; }

public:
    // Call after externally mutating colliders, materials, offsets, or flags
    void mark_mass_dirty() const noexcept { m_mass_dirty = true; }

private:
    void touch_rotation_lock(PhysicsFlags f) noexcept {
        if (has_any(f, PhysicsFlags::LockRotationX | PhysicsFlags::LockRotationY | PhysicsFlags::LockRotationZ)) m_mass_dirty = true;
    }

public:

    bool can_collide_with(const RigidBody2D& other) const noexcept;

    bool is_mass_dirty() const noexcept { return m_mass_dirty; }

    void ensure_mass_uptodate(Real gravity = MassSpec::STANDARD_GRAVITY) const {
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
    RVec2 position() const noexcept { return m_transform.position; }

    template <class U, typename std::enable_if<is_fizmo_unit_v<U> && U::dimension() == units::DIMENSION_LENGTH, int>::type = 0>
    RVec2 position(U distance_unit) const noexcept {
        Real a = units::convert(m_transform.position.x, units::distance::meter, distance_unit);
        Real b = units::convert(m_transform.position.y, units::distance::meter, distance_unit);
        return RVec2(a, b);
    }

    void set_position(const RVec2& pos) noexcept { m_transform.position = pos; }

    template <class U, typename std::enable_if<is_fizmo_unit_v<U> && U::dimension() == units::DIMENSION_LENGTH, int>::type = 0>
    void set_position(const RVec2& pos, U distance_unit) noexcept {
        m_transform.position.x = units::convert(pos.x, distance_unit, units::distance::meter);
        m_transform.position.y = units::convert(pos.y, distance_unit, units::distance::meter);
    }

public:
    Real angle(bool in_rad = true) const noexcept { return in_rad ? m_transform.angle : (m_transform.angle * constants::reciprocal_pi_180()); }

    void set_angle(Real angle, bool in_rad = true) noexcept {
        if (!in_rad) { angle *= constants::pi_180(); }
        m_transform.set_angle(angle);
    }

public:
    RVec2 linear_velocity() const noexcept { return m_linear_velocity; }

    template <class U, typename std::enable_if<is_fizmo_unit_v<U> && U::dimension() == units::DIMENSION_VELOCITY, int>::type = 0>
    RVec2 linear_velocity(U unit) const noexcept {
        Real a = units::convert(m_linear_velocity.x, units::velocity::meters_per_second, unit);
        Real b = units::convert(m_linear_velocity.y, units::velocity::meters_per_second, unit);
        return RVec2(a, b);
    }

    void set_linear_velocity(const RVec2& v) noexcept {
        m_linear_velocity = v;
        if (v.x != 0.0_r || v.y != 0.0_r) wake();
    }

    template <class U, typename std::enable_if<is_fizmo_unit_v<U> && U::dimension() == units::DIMENSION_VELOCITY, int>::type = 0>
    void set_linear_velocity(const RVec2& v, U unit) noexcept {
        set_linear_velocity(detail::convert_velocity_vec(v, unit, units::velocity::meters_per_second));
    }

public:
    Real angular_velocity() const noexcept { return m_angular_velocity; }

    template <class U, typename std::enable_if<is_fizmo_unit_v<U> && U::dimension() == units::DIMENSION_ANGULAR_VELOCITY, int>::type = 0>
    Real angular_velocity(U unit) const noexcept {
        static constexpr auto rad_per_s = units::angle::radian / units::time::second;
        return detail::convert_angular_velocity_scalar(m_angular_velocity, rad_per_s, unit);
    }

    void set_angular_velocity(Real w) noexcept {
        m_angular_velocity = w;
        if (w != 0.0_r) wake();
    }

    template <class U, typename std::enable_if<is_fizmo_unit_v<U> && U::dimension() == units::DIMENSION_ANGULAR_VELOCITY, int>::type = 0>
    void set_angular_velocity(Real w, U unit) noexcept {
        static constexpr auto rad_per_s = units::angle::radian / units::time::second;
        set_angular_velocity(detail::convert_angular_velocity_scalar(w, unit, rad_per_s));
    }

public:
    RVec2 force() const noexcept { return m_force; }

    template <class U, typename std::enable_if<is_fizmo_unit_v<U> && U::dimension() == units::DIMENSION_FORCE, int>::type = 0>
    RVec2 force(U force_unit) const noexcept {
        return detail::convert_force_vec(m_force, units::force::newton, force_unit);
    }

    void set_force(const RVec2& f) noexcept { m_force = f; }

    template <class U, typename std::enable_if<is_fizmo_unit_v<U> && U::dimension() == units::DIMENSION_FORCE, int>::type = 0>
    void set_force(const RVec2& f, U unit) noexcept {
        m_force = detail::convert_force_vec(f, unit, units::force::newton);
    }

public:
    Real torque() const noexcept { return m_torque; }

    template <class U, typename std::enable_if<is_fizmo_unit_v<U> && U::dimension() == units::DIMENSION_TORQUE, int>::type = 0>
    Real torque(U unit) const noexcept {
        return detail::convert_torque_scalar(m_torque, units::torque::newton_meter, unit);
    }

    void set_torque(Real t) noexcept { m_torque = t; }

    template <class U, typename std::enable_if<is_fizmo_unit_v<U> && U::dimension() == units::DIMENSION_TORQUE, int>::type = 0>
    void set_torque(Real t, U torque_unit) noexcept {
        m_torque = detail::convert_torque_scalar(t, torque_unit, units::torque::newton_meter);
    }

public:
    Real density() const noexcept {
        ensure_mass_uptodate();
        return m_density;
    }

    template <class U, typename std::enable_if<is_fizmo_unit_v<U> && U::dimension() == units::DIMENSION_DENSITY_2D, int>::type = 0>
    Real density(U density_unit) const noexcept {
        ensure_mass_uptodate();
        static constexpr auto si_density = units::mass::kilogram / units::area::square_meter;
        return static_cast<Real>(units::convert(static_cast<long double>(m_density), si_density, density_unit));
    }

    void set_density(Real d) noexcept;

    template <class U, typename std::enable_if<is_fizmo_unit_v<U> && U::dimension() == units::DIMENSION_DENSITY_2D, int>::type = 0>
    void set_density(Real d, U density_unit) noexcept {
        static constexpr auto si_density = units::mass::kilogram / units::area::square_meter;
        d = units::convert(d, density_unit, si_density);
        set_density(d);
    }

public:
    Real mass(Real gravity = MassSpec::STANDARD_GRAVITY) const noexcept { 
        ensure_mass_uptodate(gravity);
        return m_mass; 
    }

    template <class U, typename std::enable_if<is_fizmo_unit_v<U> && U::dimension() == units::DIMENSION_MASS, int>::type = 0>
    Real mass(U mass_unit, Real gravity = MassSpec::STANDARD_GRAVITY) const noexcept {
        ensure_mass_uptodate(gravity);
        return detail::convert_weight_scalar(m_mass, units::mass::kilogram, mass_unit);
    }

    Real inv_mass(Real gravity = MassSpec::STANDARD_GRAVITY) const noexcept { 
        ensure_mass_uptodate(gravity);
        return m_inv_mass; 
    }

    template <class U, typename std::enable_if<is_fizmo_unit_v<U> && U::dimension() == units::DIMENSION_MASS, int>::type = 0>
    Real inv_mass(U unit, Real gravity = MassSpec::STANDARD_GRAVITY) const noexcept { 
        ensure_mass_uptodate(gravity);
        return units::convert(m_inv_mass, units::mass::kilogram, unit); 
    }

    void set_mass(Real kg) noexcept;

    template <class U, typename std::enable_if<is_fizmo_unit_v<U> && U::dimension() == units::DIMENSION_MASS, int>::type = 0>
    void set_mass(Real val, U mass_unit) noexcept {
        set_mass(detail::convert_weight_scalar(val, mass_unit, units::mass::kilogram));
    }

    void set_mass_from_weight(Real newtons, Real gravity = MassSpec::STANDARD_GRAVITY) noexcept {
        set_mass((gravity > 0.0_r) ? newtons / gravity : 0.0_r);
    }

public:
    Real inertia(Real gravity = MassSpec::STANDARD_GRAVITY) const noexcept { 
        ensure_mass_uptodate(gravity);
        return m_inertia; 
    }

    Real inv_inertia(Real gravity = MassSpec::STANDARD_GRAVITY) const noexcept { 
        ensure_mass_uptodate(gravity);
        return m_inv_inertia; 
    }

public:
    Real shape_area() const noexcept {
        ensure_mass_uptodate();
        return m_shape_area;
    }

public:
    RVec2 local_center() const noexcept {
        ensure_mass_uptodate();
        return m_local_center;
    }

    template <class U, typename std::enable_if<is_fizmo_unit_v<U> && U::dimension() == units::DIMENSION_LENGTH, int>::type = 0>
    RVec2 local_center(U length_unit) const noexcept {
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
    void apply_force(const RVec2& f) {
        if (!has(PhysicsFlags::LinearForces)) return;
        m_force += f;
        wake();
    }

    template <class U, typename std::enable_if<is_fizmo_unit_v<U> && U::dimension() == units::DIMENSION_FORCE, int>::type = 0>
    void apply_force(const RVec2& f, U unit) {
        apply_force(detail::convert_force_vec(f, unit, units::force::newton));
    }

    void apply_force_at(const RVec2& f, const RVec2& world_point);

    template <class FU, class PU, typename std::enable_if<is_fizmo_unit_v<FU> && is_fizmo_unit_v<PU> && FU::dimension() == units::DIMENSION_FORCE && PU::dimension() == units::DIMENSION_LENGTH, int>::type = 0>
    void apply_force_at(const RVec2& f, FU f_unit, const RVec2& world_point, PU p_unit) {
        apply_force_at(
            detail::convert_force_vec(f, f_unit, units::force::newton),
            detail::convert_position_vec(world_point, p_unit, units::distance::meter)
        );
    }

public:
    void apply_torque(Real t) {
        if (!has(PhysicsFlags::Torque)) return;
        m_torque += t;
        wake();
    }

    template <class U, typename std::enable_if<is_fizmo_unit_v<U> && U::dimension() == units::DIMENSION_TORQUE, int>::type = 0>
    void apply_torque(Real t, U torque_unit) {
        apply_torque(detail::convert_torque_scalar(t, torque_unit, units::torque::newton_meter));
    }

public:
    void apply_linear_impulse(const RVec2& impulse);

    template <class U, typename std::enable_if<is_fizmo_unit_v<U> && U::dimension() == units::DIMENSION_IMPULSE, int>::type = 0>
    void apply_linear_impulse(const RVec2& impulse, U impulse_unit) {
        Real f = detail::convert_impulse_scalar(1.0_r, impulse_unit, units::impulse::newton_second);
        apply_linear_impulse(impulse * f);
    }

    void apply_impulse_at(const RVec2& impulse, const RVec2& world_point);

    void apply_angular_impulse(Real impulse);

public:
    void integrate_forces(Real dt, const RVec2& gravity);

    template <class U, typename std::enable_if<is_fizmo_unit_v<U> && U::dimension() == units::DIMENSION_ACCELERATION, int>::type = 0>
    void integrate_forces(Real dt, const RVec2& gravity, U grav_accel_unit) {
        Real a = units::convert(gravity.x, grav_accel_unit, units::acceleration::meters_per_second_squared);
        Real b = units::convert(gravity.y, grav_accel_unit, units::acceleration::meters_per_second_squared);
        return integrate_forces(dt, RVec2(a, b));
    }

    void integrate_velocities(Real dt);

    void apply_position_correction(const RVec2& d_center, Real d_angle);

    void clear_forces() {
        m_force  = {};
        m_torque = 0.0_r;
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

    AABB compute_body_aabb() const;

    RVec2 world_center() const noexcept {
        ensure_mass_uptodate();
        return m_transform.apply(m_local_center);
    }

    template <class U, typename std::enable_if<is_fizmo_unit_v<U> && U::dimension() == units::DIMENSION_LENGTH, int>::type = 0>
    RVec2 world_center(U distance_unit) const noexcept {
        return detail::convert_position_vec(world_center(), units::distance::meter, distance_unit);
    }

    RVec2 get_world_point(const RVec2& local)  const noexcept { return m_transform.apply(local); }
    RVec2 get_local_point(const RVec2& world)  const noexcept { return m_transform.apply_inverse(world); }
    RVec2 get_world_vector(const RVec2& local) const noexcept { return m_transform.rotate(local); }
    RVec2 get_local_vector(const RVec2& world) const noexcept { return m_transform.rotate_inverse(world); }

    RVec2 get_velocity_at(const RVec2& world_point) const noexcept;

    template <class PU, class VU, typename std::enable_if<is_fizmo_unit_v<PU> && is_fizmo_unit_v<VU> && PU::dimension() == units::DIMENSION_LENGTH && VU::dimension() == units::DIMENSION_VELOCITY, int>::type = 0>
    RVec2 get_velocity_at(const RVec2& world_point, PU position_distance_unit, VU velocity_unit) const noexcept {
        RVec2 wp_si = detail::convert_position_vec(world_point, position_distance_unit, units::distance::meter);
        return detail::convert_velocity_vec(get_velocity_at(wp_si), units::velocity::meters_per_second, velocity_unit);
    }

    Real kinetic_energy() const noexcept;

    template <class U, typename std::enable_if<is_fizmo_unit_v<U> && U::dimension() == units::DIMENSION_ENERGY, int>::type = 0>
    Real kinetic_energy(U energy_unit) const noexcept {
        return static_cast<Real>(
            units::convert(
                static_cast<long double>(kinetic_energy()),
                units::energy::joule, energy_unit
            )
        );
    }

private:
    static bool validate_frozen(FrozenProperty f) noexcept;

    void recompute_mass_data(Real gravity = MassSpec::STANDARD_GRAVITY) const;

    void resolve_mass_constraint_from(FrozenProperty just_set) noexcept;

    void derive_mass_from_density_area() noexcept;

    void derive_area_from_mass_density() noexcept;

    void scale_collider_geometry(Real s) noexcept;

    static Shape2D rebuild_shape_bounds(const Shape2D& src) noexcept;

    void refresh_shape_area() const noexcept;

    void recompute_inertia_from_density() noexcept;

private:
    PhysicsFlags m_flags = PhysicsFlags::DefaultDynamic;

    std::uint64_t m_collision_layer = 1ULL;
    std::uint64_t m_collision_mask  = std::numeric_limits<std::uint64_t>::max();

    Transform2D     m_transform;
    mutable Sweep2D m_sweep;

    RVec2 m_linear_velocity{};       // m/s
    Real   m_angular_velocity = 0.0_r;  // rad/s
    RVec2 m_force{};                 // N
    Real   m_torque = 0.0_r;            // N*m

    mutable Real  m_density    = 1.0_r; // kg/m2
    FrozenProperty  m_frozen     = FrozenProperty::None;
    mutable Real  m_shape_area = 0.0_r;  

    mutable bool     m_mass_dirty  = true;
    mutable Real   m_mass        = 0.0_r;       // kg
    mutable Real   m_inv_mass    = 0.0_r;       // 1/kg
    mutable Real   m_inertia     = 0.0_r;       // kg*m2
    mutable Real   m_inv_inertia = 0.0_r;       // 1/(kg*m2)
    mutable RVec2 m_local_center{};          // m

    Real m_linear_damping   = 0.0_r;    // 1/s
    Real m_angular_damping  = 0.0_r;    // 1/s
    Real m_max_linear_speed  = 1e6_r;   // m/s
    Real m_max_angular_speed = 1e6_r;   // rad/s

    Real m_sleep_timer = 0.0_r;         // s
    bool   m_is_awake    = true;

    Real m_gravity_scale = 1.0_r;

    std::vector<Collider2D> m_colliders;
    BodyHandle m_handle{};
    std::size_t m_sim_index = 0;
};

} // namespace physics
} // namespace fizmo

#endif // FIZMO_PHYSICS_2D_HPP