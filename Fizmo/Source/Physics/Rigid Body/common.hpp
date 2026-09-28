#ifndef FIZMO_PHYSICS_COMMON_HPP
#define FIZMO_PHYSICS_COMMON_HPP

#include <cstddef>
#include <cstdint>
#include <functional>
#include <iterator>
#include <memory>
#include <type_traits>
#include <vector>
#include <cassert>
#include <utility>
#include <cmath>
#include <algorithm>
#include <unordered_map>
#include <limits>

#include "../../Vectors/vectors.hpp"
#include "../../Converters/quantity.hpp"
#include "../../Matrices/square_matrices.hpp"
#include "../../Quaternions/quaternion.hpp"

#ifdef OS_LINUX
    #include "../../x11_compat.hpp"
#endif

namespace fizmo {
namespace physics {

enum class MassMode : std::uint8_t {
    Density = 0, // mass from density
    Mass,        // directly from mass (KG)
    Weight       // N (gravitational weight, computed from g)
};

enum class FrozenProperty : std::uint8_t {
    None    = 0,
    Mass    = 1 << 0,
    Density = 1 << 1,
    Area    = 1 << 2, // 2D only
    Volume  = 1 << 3  // 3D only
};

constexpr FrozenProperty operator|(FrozenProperty a, FrozenProperty b) noexcept {
    return static_cast<FrozenProperty>(static_cast<std::uint8_t>(a) | static_cast<std::uint8_t>(b));
}
constexpr FrozenProperty operator&(FrozenProperty a, FrozenProperty b) noexcept {
    return static_cast<FrozenProperty>(static_cast<std::uint8_t>(a) & static_cast<std::uint8_t>(b));
}
constexpr FrozenProperty operator~(FrozenProperty a) noexcept {
    return static_cast<FrozenProperty>(~static_cast<std::uint8_t>(a));
}
constexpr FrozenProperty& operator|=(FrozenProperty& a, FrozenProperty b) noexcept {
    return a = a | b;
}
constexpr FrozenProperty& operator&=(FrozenProperty& a, FrozenProperty b) noexcept {
    return a = a & b;
}
constexpr bool has_frozen(FrozenProperty set, FrozenProperty flag) noexcept {
    return (static_cast<std::uint8_t>(set) & static_cast<std::uint8_t>(flag)) != 0;
}

struct CollisionFilter {
    std::uint16_t category_bits = 0x0001;
    std::uint16_t mask_bits     = 0xFFFF;
    std::int16_t  group_index   = 0;

    constexpr bool should_collide(const CollisionFilter& other) const noexcept {
        if (group_index != 0 && group_index == other.group_index) { return group_index > 0; }
        return (category_bits & other.mask_bits) != 0 && (other.category_bits & mask_bits) != 0;
    }
};

enum class PhysicsFlags : std::uint64_t {
    None = 0,

    Gravity             = 1ULL << 0,
    LinearForces        = 1ULL << 1,   // accumulates external linear forces
    Torque              = 1ULL << 2,   // accumulates external torque
    Wind                = 1ULL << 3,   // affected by wind / fluid zones
    Buoyancy            = 1ULL << 4,   // affected by buoyancy volumes
    Magnetism           = 1ULL << 5,   // affected by magnetic fields
    Springs             = 1ULL << 6,   // participates in spring / soft constraints
    UserForceField      = 1ULL << 7,   // affected by user-defined force fields

    LinearVelocity      = 1ULL << 8,   // integrates linear velocity
    AngularVelocity     = 1ULL << 9,   // integrates angular velocity
    LinearDamping       = 1ULL << 10,  // apply linear damping each step
    AngularDamping      = 1ULL << 11,  // apply angular damping each step
    VelocityClamping    = 1ULL << 12,  // clamp to max linear speed
    AngularClamping     = 1ULL << 13,  // clamp to max angular speed

    LinearImpulses      = 1ULL << 14,  // accepts linear impulses
    AngularImpulses     = 1ULL << 15,  // accepts angular impulses
    ContactImpulses     = 1ULL << 16,  // receives impulses from contact solver
    JointImpulses       = 1ULL << 17,  // receives impulses from joints

    LockRotationX       = 1ULL << 18,  // freeze rotation about X
    LockRotationY       = 1ULL << 19,  // freeze rotation about Y
    LockRotationZ       = 1ULL << 20,  // freeze rotation about Z

    LockRotation2D      = LockRotationX | LockRotationY,
    LockRotation3D      = LockRotationX | LockRotationY | LockRotationZ,

    CollisionResponse   = 1ULL << 21,  // participates in collision resolution
    FrictionResponse    = 1ULL << 22,  // friction applied during contacts
    RestitutionResponse = 1ULL << 23,  // restitution (bounce) applied
    ContactEvents       = 1ULL << 24,  // fires begin/end contact callbacks
    SensorEvents        = 1ULL << 25,  // fires sensor overlap callbacks
    PreSolveEvents      = 1ULL << 26,  // fires pre-solve callback (can disable contact)
    PostSolveEvents     = 1ULL << 27,  // fires post-solve callback

    BroadphaseActive    = 1ULL << 28,  // inserted into broadphase tree
    Bullet              = 1ULL << 29,  // continuous collision detection
    OneWayPlatform      = 1ULL << 30,  // only collides from one side (e.g. top)

    AllowSleep          = 1ULL << 31,  // may auto-sleep when idle
    StartAwake          = 1ULL << 32,  // created in awake state

    LockPositionX       = 1ULL << 33,  // freeze X translation
    LockPositionY       = 1ULL << 34,  // freeze Y translation
    LockPositionZ       = 1ULL << 35,  // freeze Z translation

    LockPosition2D      = LockPositionX | LockPositionY,
    LockPosition3D      = LockPositionX | LockPositionY | LockPositionZ,

    Visible             = 1ULL << 36,  // should be rendered
    DebugDraw           = 1ULL << 37,  // draw debug overlay (AABB, normals, etc.)
    ShowVelocity        = 1ULL << 38,  // draw velocity arrow
    ShowContacts        = 1ULL << 39,  // draw contact points

    Enabled             = 1ULL << 40,  // body is active in the world
    AutoDisable         = 1ULL << 41,  // auto-disable after N seconds of sleep
    DestroyOnSleep      = 1ULL << 42,  // remove body when it falls asleep
    DestroyOffScreen    = 1ULL << 43,  // remove body when outside world bounds

    IslandMember        = 1ULL << 44,  // participates in island solving
    WarmStarting        = 1ULL << 45,  // use warm-starting in iterative solver
    PositionCorrection  = 1ULL << 46,  // apply Baumgarte / split-impulse correction
    SpeculativeContacts = 1ULL << 47,  // generate speculative contact points

    Draggable           = 1ULL << 48,  // can be picked up by mouse / touch joint
    Intangible          = 1ULL << 49,  // generates contacts but no response

    ImpactSounds        = 1ULL << 50,  // trigger sound on contact
    TrailEffect         = 1ULL << 51,
    ParticleOnImpact    = 1ULL << 52,

    UserFlag1           = 1ULL << 53,
    UserFlag2           = 1ULL << 54,
    UserFlag3           = 1ULL << 55,
    UserFlag4           = 1ULL << 56,
    UserFlag5           = 1ULL << 58,
    UserFlag6           = 1ULL << 59,
    UserFlag7           = 1ULL << 60,
    UserFlag8           = 1ULL << 61,
    UserFlag9           = 1ULL << 62,
    UserFlag10          = 1ULL << 63,

    _AllCollision = CollisionResponse | FrictionResponse | RestitutionResponse | ContactEvents | BroadphaseActive,
    _AllDamping   = LinearDamping | AngularDamping,
    _AllForces    = Gravity | LinearForces | Torque,
    _AllVelocity  = LinearVelocity | AngularVelocity,
    _AllImpulses  = LinearImpulses | AngularImpulses | ContactImpulses | JointImpulses,

    DefaultDynamic   = _AllForces | _AllVelocity | _AllDamping | _AllImpulses | _AllCollision | AllowSleep | StartAwake | Enabled | IslandMember | WarmStarting | PositionCorrection | Visible,
    DefaultStatic    = _AllCollision | Enabled | Visible | IslandMember,
    DefaultKinematic = _AllVelocity | _AllCollision | Enabled | Visible | IslandMember | StartAwake,

    WorldEnvironment  = DefaultStatic,
    KinematicPlatform = DefaultKinematic | OneWayPlatform | ContactEvents,
    TriggerZone       = BroadphaseActive | SensorEvents | Enabled,
    BulletProjectile  = DefaultDynamic | Bullet | DestroyOffScreen,
    ZeroGravity       = DefaultDynamic & ~(Gravity | AllowSleep),
    Decoration        = Enabled | Visible | _AllVelocity,
    GhostBody         = DefaultDynamic | Intangible | ContactEvents,
    InteractiveProp   = DefaultDynamic | Draggable | ImpactSounds
};

constexpr PhysicsFlags operator|(PhysicsFlags a, PhysicsFlags b) noexcept {
    return static_cast<PhysicsFlags>(static_cast<std::uint64_t>(a) | static_cast<std::uint64_t>(b));
}
constexpr PhysicsFlags operator&(PhysicsFlags a, PhysicsFlags b) noexcept {
    return static_cast<PhysicsFlags>(static_cast<std::uint64_t>(a) & static_cast<std::uint64_t>(b));
}
constexpr PhysicsFlags operator^(PhysicsFlags a, PhysicsFlags b) noexcept {
    return static_cast<PhysicsFlags>(static_cast<std::uint64_t>(a) ^ static_cast<std::uint64_t>(b));
}
constexpr PhysicsFlags operator~(PhysicsFlags a) noexcept {
    return static_cast<PhysicsFlags>(~static_cast<std::uint64_t>(a));
}
constexpr PhysicsFlags& operator|=(PhysicsFlags& a, PhysicsFlags b) noexcept {
    return a = a | b;
}
constexpr PhysicsFlags& operator&=(PhysicsFlags& a, PhysicsFlags b) noexcept {
    return a = a & b;
}
constexpr PhysicsFlags& operator^=(PhysicsFlags& a, PhysicsFlags b) noexcept {
    return a = a ^ b;
}
constexpr bool has_flag(PhysicsFlags set, PhysicsFlags flag) noexcept {
    return (static_cast<std::uint64_t>(set) & static_cast<std::uint64_t>(flag)) == static_cast<std::uint64_t>(flag);
}
constexpr bool has_any(PhysicsFlags set, PhysicsFlags mask) noexcept {
    return (static_cast<std::uint64_t>(set) & static_cast<std::uint64_t>(mask)) != 0;
}

namespace detail {

template <class FromU, class ToU>
inline typename std::enable_if<is_fizmo_unit_v<FromU> && is_fizmo_unit_v<ToU>, double>::type
convert_velocity_scalar(double val, FromU from, ToU to) {
    return static_cast<double>(units::convert(static_cast<long double>(val), from, to));
}

template <class FromU, class ToU>
inline typename std::enable_if<is_fizmo_unit_v<FromU> && is_fizmo_unit_v<ToU>, double>::type
convert_angular_velocity_scalar(double val, FromU from, ToU to) {
    return static_cast<double>(units::convert(static_cast<long double>(val), from, to));
}

template <class FromU, class ToU>
inline typename std::enable_if<is_fizmo_unit_v<FromU> && is_fizmo_unit_v<ToU>, double>::type
convert_force_scalar(double val, FromU from, ToU to) {
    return static_cast<double>(units::convert(static_cast<long double>(val), from, to));
}

template <class FromU, class ToU>
inline typename std::enable_if<is_fizmo_unit_v<FromU> && is_fizmo_unit_v<ToU>, double>::type
convert_torque_scalar(double val, FromU from, ToU to) {
    return static_cast<double>(units::convert(static_cast<long double>(val), from, to));
}

template <class FromU, class ToU>
inline typename std::enable_if<is_fizmo_unit_v<FromU> && is_fizmo_unit_v<ToU>, double>::type
convert_weight_scalar(double val, FromU from, ToU to) {
    return static_cast<double>(units::convert(static_cast<long double>(val), from, to));
}

template <class FromU, class ToU>
inline typename std::enable_if<is_fizmo_unit_v<FromU> && is_fizmo_unit_v<ToU>, double>::type
convert_distance_scalar(double val, FromU from, ToU to) {
    return static_cast<double>(units::convert(static_cast<long double>(val), from, to));
}

template <class FromU, class ToU>
inline typename std::enable_if<is_fizmo_unit_v<FromU> && is_fizmo_unit_v<ToU>, double>::type
convert_impulse_scalar(double val, FromU from, ToU to) {
    return static_cast<double>(units::convert(static_cast<long double>(val), from, to));
}

} // namespace detail

namespace detail {

template <class Body>
struct ContactKey {
    const Body* body_a = nullptr;
    const Body* body_b = nullptr;
    std::size_t collider_a = 0;
    std::size_t collider_b = 0;

    bool operator==(const ContactKey& o) const noexcept {
        return body_a == o.body_a && body_b == o.body_b && collider_a == o.collider_a && collider_b == o.collider_b;
    }
};

template <class Body>
struct ContactKeyHash {
    std::size_t operator()(const ContactKey<Body>& k) const noexcept {
        std::size_t h = std::hash<const void*>()(k.body_a);
        auto mix = [&h](std::size_t v) { h ^= v + static_cast<std::size_t>(0x9e3779b9u) + (h << 6) + (h >> 2); };
        mix(std::hash<const void*>()(k.body_b));
        mix(k.collider_a);
        mix(k.collider_b);
        return h;
    }
};

class UnionFind {
public:
    void reset(std::size_t n) {
        m_parent.resize(n);
        for (std::size_t i = 0; i < n; ++i) m_parent[i] = i;
    }

    std::size_t find(std::size_t i) noexcept {
        while (m_parent[i] != i) { m_parent[i] = m_parent[m_parent[i]]; i = m_parent[i]; }
        return i;
    }

    void unite(std::size_t a, std::size_t b) noexcept {
        a = find(a); b = find(b);
        if (a != b) m_parent[a < b ? b : a] = (a < b ? a : b);
    }

    std::size_t size() const noexcept { return m_parent.size(); }

private:
    std::vector<std::size_t> m_parent;
};

} // namespace detail

template <class Body>
class BodyRange {
    using Mutable  = typename std::remove_const<Body>::type;
    using Storage  = std::vector<std::unique_ptr<Mutable>>;
    using StorageP = typename std::conditional<std::is_const<Body>::value, const Storage*, Storage*>::type;
    using BaseIter = typename std::conditional<std::is_const<Body>::value, typename Storage::const_iterator, typename Storage::iterator>::type;

public:
    class iterator {
    public:
        using iterator_category = std::random_access_iterator_tag;
        using value_type        = Mutable;
        using difference_type   = std::ptrdiff_t;
        using pointer           = Body*;
        using reference         = Body&;

        iterator() = default;
        explicit iterator(BaseIter it) : m_it(it) {}

        reference operator*()  const { return **m_it; }
        pointer   operator->() const { return m_it->get(); }
        reference operator[](difference_type n) const { return *m_it[n]; }

        iterator& operator++() { ++m_it; return *this; }
        iterator  operator++(int) { iterator t = *this; ++m_it; return t; }
        iterator& operator--() { --m_it; return *this; }
        iterator  operator--(int) { iterator t = *this; --m_it; return t; }
        iterator& operator+=(difference_type n) { m_it += n; return *this; }
        iterator& operator-=(difference_type n) { m_it -= n; return *this; }
        iterator  operator+(difference_type n) const { return iterator(m_it + n); }
        iterator  operator-(difference_type n) const { return iterator(m_it - n); }
        difference_type operator-(const iterator& o) const { return m_it - o.m_it; }

        bool operator==(const iterator& o) const { return m_it == o.m_it; }
        bool operator!=(const iterator& o) const { return m_it != o.m_it; }
        bool operator< (const iterator& o) const { return m_it <  o.m_it; }

    private:
        BaseIter m_it{};
    };

    explicit BodyRange(StorageP storage) noexcept : m_storage(storage) {}

    iterator begin() const { return iterator(m_storage->begin()); }
    iterator end()   const { return iterator(m_storage->end()); }

    std::size_t size()  const noexcept { return m_storage->size(); }
    bool        empty() const noexcept { return m_storage->empty(); }

    Body& operator[](std::size_t i) const { return *(*m_storage)[i]; }
    Body& front() const { return *m_storage->front(); }
    Body& back()  const { return *m_storage->back(); }

private:
    StorageP m_storage;
};

} // namespace physics
} // namespace fizmo

#endif // FIZMO_PHYSICS_COMMON_HPP