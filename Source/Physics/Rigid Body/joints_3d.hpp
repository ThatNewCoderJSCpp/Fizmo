#ifndef FIZMO_PHYSICS_JOINTS_3D_HPP
#define FIZMO_PHYSICS_JOINTS_3D_HPP

#include "solver_3d.hpp"

namespace fizmo {
namespace physics {

enum class JointType3D : std::uint8_t {
    Ball = 0,
    Hinge,
    Slider,
    Fixed,
    Distance,
    Spring,
    ConeTwist
};

const char* to_string(JointType3D t) noexcept;
std::optional<JointType3D> parse_joint_type(std::string_view text) noexcept;

struct JointLimits3D {
    bool enabled = false;
    Real lower = 0.0_r;
    Real upper = 0.0_r;
};

struct JointMotor3D {
    bool enabled = false;
    Real target_speed = 0.0_r;
    Real max_force = 0.0_r;
};

struct JointSpring3D {
    Real frequency = 0.0_r;
    Real damping_ratio = 0.7_r;
    Real target = 0.0_r;
};

struct JointDef3D {
    JointType3D type = JointType3D::Ball;
    RigidBody3D* body_a = nullptr;
    RigidBody3D* body_b = nullptr;
    RVec3 local_anchor_a{};
    RVec3 local_anchor_b{};
    RVec3 local_axis_a{ 0.0_r, 1.0_r, 0.0_r };
    RVec3 local_axis_b{ 0.0_r, 1.0_r, 0.0_r };
    RVec3 local_reference_a{ 1.0_r, 0.0_r, 0.0_r };
    RVec3 local_reference_b{ 1.0_r, 0.0_r, 0.0_r };
    RQuat reference_rotation{ 1.0_r, 0.0_r, 0.0_r, 0.0_r };
    Real rest_length = 1.0_r;
    Real min_length = 0.0_r;
    Real max_length = 0.0_r;
    JointLimits3D limits{};
    JointMotor3D motor{};
    JointSpring3D spring{};
    Real swing_limit = 0.0_r;
    Real break_force = 0.0_r;
    Real break_torque = 0.0_r;
    bool collide_connected = false;
    bool enabled = true;
};

class Joint3D {
public:
    explicit Joint3D(const JointDef3D& def) : m_def(def) {}

    JointType3D type() const noexcept { return m_def.type; }
    const JointDef3D& def() const noexcept { return m_def; }
    JointDef3D& def_mut() noexcept { return m_def; }
    RigidBody3D* body_a() const noexcept { return m_def.body_a; }
    RigidBody3D* body_b() const noexcept { return m_def.body_b; }
    bool enabled() const noexcept { return m_def.enabled && !m_broken; }
    void set_enabled(bool on) noexcept { m_def.enabled = on; }
    bool broken() const noexcept { return m_broken; }
    void repair() noexcept { m_broken = false; }
    bool collide_connected() const noexcept { return m_def.collide_connected; }

    void set_motor(bool on, Real target_speed, Real max_force) noexcept {
        m_def.motor.enabled = on;
        m_def.motor.target_speed = target_speed;
        m_def.motor.max_force = max_force;
    }
    void set_limits(bool on, Real lower, Real upper) noexcept {
        m_def.limits.enabled = on;
        m_def.limits.lower = std::min(lower, upper);
        m_def.limits.upper = std::max(lower, upper);
    }
    void set_spring(Real frequency, Real damping_ratio, Real target = 0.0_r) noexcept {
        m_def.spring.frequency = std::max(0.0_r, frequency);
        m_def.spring.damping_ratio = std::max(0.0_r, damping_ratio);
        m_def.spring.target = target;
    }

    RVec3 world_anchor_a() const noexcept;
    RVec3 world_anchor_b() const noexcept;
    RVec3 world_axis_a() const noexcept;
    Real angle() const noexcept;
    Real translation() const noexcept;
    Real length() const noexcept;
    RVec3 reaction_force() const noexcept { return m_reaction_force; }
    RVec3 reaction_torque() const noexcept { return m_reaction_torque; }
    std::uint64_t id() const noexcept { return m_id; }

    struct Row {
        RVec3 lin_a{};
        RVec3 ang_a{};
        RVec3 lin_b{};
        RVec3 ang_b{};
        Real mass = 0.0_r;
        Real bias = 0.0_r;
        Real gamma = 0.0_r;
        Real lower = -std::numeric_limits<Real>::infinity();
        Real upper = std::numeric_limits<Real>::infinity();
        Real impulse = 0.0_r;
        Real target = 0.0_r;
        Real position_error = 0.0_r;
        std::uint8_t kind = 0;
        std::uint8_t slot = 0;
        bool soft_bias = true;
    };

    void prepare(SolverBody3D* a, SolverBody3D* b, Real dt, Real baumgarte, const SoftParams& soft, bool use_soft);
    void warm_start(SolverBody3D* a, SolverBody3D* b) noexcept;
    void solve(SolverBody3D* a, SolverBody3D* b, bool use_bias) noexcept;
    void refresh_bias(SolverBody3D* a, SolverBody3D* b, Real dt, Real baumgarte, const SoftParams& soft, bool use_soft);
    void finish(Real dt);
    void shift_world(const RVec3& offset) noexcept;

    std::size_t solver_a = 0;
    std::size_t solver_b = 0;
    bool has_a = false;
    bool has_b = false;
    std::size_t island = 0;

private:
    friend class Simulation3D;

    void build_rows(SolverBody3D* a, SolverBody3D* b, Real dt, Real baumgarte, const SoftParams& soft, bool use_soft, bool keep_impulses);
    void add_linear(const RVec3& rA, const RVec3& rB, const RVec3& axis, Real C, std::uint8_t slot);
    void add_angular(const RVec3& axis, Real C, std::uint8_t slot);

    JointDef3D m_def;
    std::vector<Row> m_rows;
    std::array<Real, 24> m_saved{};
    bool m_broken = false;
    RVec3 m_reaction_force{};
    RVec3 m_reaction_torque{};
    std::uint64_t m_id = 0;
};

} // namespace physics
} // namespace fizmo

#endif // FIZMO_PHYSICS_JOINTS_3D_HPP
