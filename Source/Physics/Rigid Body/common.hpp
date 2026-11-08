#ifndef FIZMO_PHYSICS_COMMON_HPP
#define FIZMO_PHYSICS_COMMON_HPP

#include <cstddef>
#include <mutex>
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
#include <optional>
#include <string_view>

#include "../../Vectors/vectors.hpp"
#include "../../Converters/quantity.hpp"
#include "../../Matrices/square_matrices.hpp"
#include "../../Quaternions/quaternion.hpp"
#include "../../Vectors/fast_math.hpp"
#include "../../Vectors/axes.hpp"

#ifdef OS_LINUX
#endif

namespace fizmo {
namespace physics {

#ifdef FIZMO_PHYSICS_DOUBLE
using Real = double;
#else
using Real = float;
#endif

using RVec2 = linalg::Vector2D<Real>;
using RVec3 = linalg::Vector3D<Real>;
using RMat3 = linalg::Matrix3x3<Real>;
using RQuat = linalg::Quaternion<Real>;

constexpr Real operator""_r(long double v) noexcept { return static_cast<Real>(v); }

inline RVec2 to_real(const linalg::vector2d& v) noexcept { return RVec2(static_cast<Real>(v.x), static_cast<Real>(v.y)); }
inline RVec3 to_real(const linalg::vector3d& v) noexcept { return RVec3(static_cast<Real>(v.x), static_cast<Real>(v.y), static_cast<Real>(v.z)); }
inline RQuat to_real(const linalg::QuatD& q) noexcept { return RQuat(static_cast<Real>(q.w()), static_cast<Real>(q.x()), static_cast<Real>(q.y()), static_cast<Real>(q.z())); }
inline linalg::vector2d to_double(const RVec2& v) noexcept { return linalg::vector2d(v.x, v.y); }
inline linalg::vector3d to_double(const RVec3& v) noexcept { return linalg::vector3d(v.x, v.y, v.z); }
inline linalg::QuatD to_double(const RQuat& q) noexcept { return linalg::QuatD(q.w(), q.x(), q.y(), q.z()); }


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

    LockRotation2D      = LockRotationZ,
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
inline typename std::enable_if<is_fizmo_unit_v<FromU> && is_fizmo_unit_v<ToU>, Real>::type
convert_velocity_scalar(Real val, FromU from, ToU to) {
    return static_cast<Real>(units::convert(static_cast<long double>(val), from, to));
}

template <class FromU, class ToU>
inline typename std::enable_if<is_fizmo_unit_v<FromU> && is_fizmo_unit_v<ToU>, Real>::type
convert_angular_velocity_scalar(Real val, FromU from, ToU to) {
    return static_cast<Real>(units::convert(static_cast<long double>(val), from, to));
}

template <class FromU, class ToU>
inline typename std::enable_if<is_fizmo_unit_v<FromU> && is_fizmo_unit_v<ToU>, Real>::type
convert_force_scalar(Real val, FromU from, ToU to) {
    return static_cast<Real>(units::convert(static_cast<long double>(val), from, to));
}

template <class FromU, class ToU>
inline typename std::enable_if<is_fizmo_unit_v<FromU> && is_fizmo_unit_v<ToU>, Real>::type
convert_torque_scalar(Real val, FromU from, ToU to) {
    return static_cast<Real>(units::convert(static_cast<long double>(val), from, to));
}

template <class FromU, class ToU>
inline typename std::enable_if<is_fizmo_unit_v<FromU> && is_fizmo_unit_v<ToU>, Real>::type
convert_weight_scalar(Real val, FromU from, ToU to) {
    return static_cast<Real>(units::convert(static_cast<long double>(val), from, to));
}

template <class FromU, class ToU>
inline typename std::enable_if<is_fizmo_unit_v<FromU> && is_fizmo_unit_v<ToU>, Real>::type
convert_distance_scalar(Real val, FromU from, ToU to) {
    return static_cast<Real>(units::convert(static_cast<long double>(val), from, to));
}

template <class FromU, class ToU>
inline typename std::enable_if<is_fizmo_unit_v<FromU> && is_fizmo_unit_v<ToU>, Real>::type
convert_impulse_scalar(Real val, FromU from, ToU to) {
    return static_cast<Real>(units::convert(static_cast<long double>(val), from, to));
}

} // namespace detail

namespace detail {

template <class Body>
struct ContactKey {
    const Body* body_a = nullptr;
    const Body* body_b = nullptr;
    std::size_t collider_a = 0;
    std::size_t collider_b = 0;
    std::uint32_t feature = 0;

    bool operator==(const ContactKey& o) const noexcept {
        return body_a == o.body_a && body_b == o.body_b && collider_a == o.collider_a && collider_b == o.collider_b && feature == o.feature;
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
        mix(k.feature);
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

enum class BodyQuality : std::uint8_t {
    Default = 0,
    Arcade,
    Normal,
    High,
    Ultra
};

enum class ContinuousMode : std::uint8_t {
    Default = 0,
    On,
    Off
};

inline const char* to_string(BodyQuality q) noexcept {
    switch (q) {
    case BodyQuality::Default: return "default";
    case BodyQuality::Arcade: return "arcade";
    case BodyQuality::Normal: return "normal";
    case BodyQuality::High: return "high";
    case BodyQuality::Ultra: return "ultra";
    }
    return "default";
}

inline std::optional<BodyQuality> parse_body_quality(std::string_view text) noexcept {
    if (text == "default") return BodyQuality::Default;
    if (text == "arcade" || text == "low") return BodyQuality::Arcade;
    if (text == "normal" || text == "medium") return BodyQuality::Normal;
    if (text == "high") return BodyQuality::High;
    if (text == "ultra" || text == "max") return BodyQuality::Ultra;
    return std::nullopt;
}

inline int quality_iterations(BodyQuality q, int fallback) noexcept {
    switch (q) {
    case BodyQuality::Default: return fallback;
    case BodyQuality::Arcade: return std::max(2, fallback / 2);
    case BodyQuality::Normal: return fallback;
    case BodyQuality::High: return fallback * 2;
    case BodyQuality::Ultra: return fallback * 4;
    }
    return fallback;
}

inline constexpr bool layers_can_collide(std::uint64_t layer_a, std::uint64_t mask_a, std::uint64_t layer_b, std::uint64_t mask_b) noexcept {
    return (layer_a & mask_b) != 0 && (layer_b & mask_a) != 0;
}

enum class CombineMode : std::uint8_t {
    Default = 0,
    Average,
    GeometricMean,
    HarmonicMean,
    QuadraticMean,
    Minimum,
    Multiply,
    Maximum,
    Sum,
    Custom
};

inline constexpr int combine_priority(CombineMode m) noexcept {
    switch (m) {
    case CombineMode::Average: return 1;
    case CombineMode::GeometricMean: return 2;
    case CombineMode::HarmonicMean: return 3;
    case CombineMode::QuadraticMean: return 4;
    case CombineMode::Minimum: return 5;
    case CombineMode::Multiply: return 6;
    case CombineMode::Maximum: return 7;
    case CombineMode::Sum: return 8;
    case CombineMode::Custom: return 9;
    case CombineMode::Default: break;
    }
    return 0;
}

inline constexpr CombineMode resolve_combine(CombineMode a, CombineMode b, CombineMode fallback) noexcept {
    if (fallback == CombineMode::Default) fallback = CombineMode::Average;
    if (a == CombineMode::Default) a = fallback;
    if (b == CombineMode::Default) b = fallback;
    return combine_priority(a) >= combine_priority(b) ? a : b;
}

inline Real combine_values(CombineMode m, Real a, Real b) noexcept {
    switch (m) {
    case CombineMode::GeometricMean: return a * b > 0.0_r ? std::sqrt(a * b) : 0.0_r;
    case CombineMode::HarmonicMean: return a + b > 0.0_r ? 2.0_r * a * b / (a + b) : 0.0_r;
    case CombineMode::QuadraticMean: return std::sqrt(0.5_r * (a * a + b * b));
    case CombineMode::Minimum: return std::min(a, b);
    case CombineMode::Multiply: return a * b;
    case CombineMode::Maximum: return std::max(a, b);
    case CombineMode::Sum: return a + b;
    case CombineMode::Average:
    case CombineMode::Default:
    case CombineMode::Custom: break;
    }
    return 0.5_r * (a + b);
}

inline const char* to_string(CombineMode m) noexcept {
    switch (m) {
    case CombineMode::Default: return "default";
    case CombineMode::Average: return "average";
    case CombineMode::GeometricMean: return "geometric-mean";
    case CombineMode::HarmonicMean: return "harmonic-mean";
    case CombineMode::QuadraticMean: return "quadratic-mean";
    case CombineMode::Minimum: return "minimum";
    case CombineMode::Multiply: return "multiply";
    case CombineMode::Maximum: return "maximum";
    case CombineMode::Sum: return "sum";
    case CombineMode::Custom: return "custom";
    }
    return "default";
}

inline std::optional<CombineMode> parse_combine_mode(std::string_view text) noexcept {
    if (text == "default") return CombineMode::Default;
    if (text == "average" || text == "mean") return CombineMode::Average;
    if (text == "geometric-mean" || text == "geometric" || text == "sqrt") return CombineMode::GeometricMean;
    if (text == "harmonic-mean" || text == "harmonic") return CombineMode::HarmonicMean;
    if (text == "quadratic-mean" || text == "quadratic" || text == "rms") return CombineMode::QuadraticMean;
    if (text == "minimum" || text == "min") return CombineMode::Minimum;
    if (text == "multiply" || text == "product") return CombineMode::Multiply;
    if (text == "maximum" || text == "max") return CombineMode::Maximum;
    if (text == "sum" || text == "add") return CombineMode::Sum;
    if (text == "custom") return CombineMode::Custom;
    return std::nullopt;
}

struct BodyHandle {
    std::uint32_t index = std::numeric_limits<std::uint32_t>::max();
    std::uint32_t generation = 0;

    bool valid() const noexcept { return index != std::numeric_limits<std::uint32_t>::max(); }
    bool operator==(const BodyHandle& o) const noexcept { return index == o.index && generation == o.generation; }
    bool operator!=(const BodyHandle& o) const noexcept { return !(*this == o); }
    std::uint64_t packed() const noexcept { return (static_cast<std::uint64_t>(generation) << 32) | index; }
    static BodyHandle unpack(std::uint64_t v) noexcept { return { static_cast<std::uint32_t>(v), static_cast<std::uint32_t>(v >> 32) }; }
};

namespace detail {

template <std::size_t Size, std::size_t Align>
class ObjectPool {
public:
    static constexpr std::size_t CHUNK = 64;

    static ObjectPool& instance() {
        static ObjectPool* pool = new ObjectPool();
        return *pool;
    }

    void* allocate() {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (!m_free) grow();
        Node* n = m_free;
        m_free = n->next;
        ++m_live;
        return n->bytes;
    }

    void release(void* p) noexcept {
        if (!p) return;
        std::lock_guard<std::mutex> lock(m_mutex);
        Node* n = reinterpret_cast<Node*>(p);
        n->next = m_free;
        m_free = n;
        --m_live;
    }

    std::size_t live() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_live;
    }

    std::size_t capacity() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_chunks.size() * CHUNK;
    }

private:
    union Node {
        Node* next;
        alignas(Align) unsigned char bytes[Size];
    };

    void grow() {
        m_chunks.emplace_back(new Node[CHUNK]);
        Node* chunk = m_chunks.back().get();
        for (std::size_t i = CHUNK; i-- > 0;) {
            chunk[i].next = m_free;
            m_free = &chunk[i];
        }
    }

    mutable std::mutex m_mutex;
    std::vector<std::unique_ptr<Node[]>> m_chunks;
    Node* m_free = nullptr;
    std::size_t m_live = 0;
};

template <class T>
class HandleTable {
public:
    BodyHandle add(T* item) {
        std::uint32_t index;
        if (!m_free.empty()) {
            index = m_free.back();
            m_free.pop_back();
            m_items[index] = item;
        } else {
            index = static_cast<std::uint32_t>(m_items.size());
            m_items.push_back(item);
            m_generations.push_back(1);
        }
        return { index, m_generations[index] };
    }

    void remove(BodyHandle h) {
        if (!alive(h)) return;
        m_items[h.index] = nullptr;
        ++m_generations[h.index];
        m_free.push_back(h.index);
    }

    bool alive(BodyHandle h) const noexcept {
        return h.index < m_items.size() && m_generations[h.index] == h.generation && m_items[h.index] != nullptr;
    }

    T* get(BodyHandle h) const noexcept { return alive(h) ? m_items[h.index] : nullptr; }

    void clear() {
        for (std::size_t i = 0; i < m_items.size(); ++i) {
            if (m_items[i]) {
                m_items[i] = nullptr;
                ++m_generations[i];
                m_free.push_back(static_cast<std::uint32_t>(i));
            }
        }
    }

private:
    std::vector<T*> m_items;
    std::vector<std::uint32_t> m_generations;
    std::vector<std::uint32_t> m_free;
};

} // namespace detail

} // namespace physics
} // namespace fizmo

#endif // FIZMO_PHYSICS_COMMON_HPP