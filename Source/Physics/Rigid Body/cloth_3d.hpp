#ifndef FIZMO_PHYSICS_CLOTH_3D_HPP
#define FIZMO_PHYSICS_CLOTH_3D_HPP

#include "physics_3d.hpp"

#include <array>
#include <cstdint>
#include <vector>

namespace fizmo {
namespace physics {

class Simulation3D;

struct ClothDef3D {
    int columns = 16;
    int rows = 16;
    Real spacing = 0.1_r;
    Real spacing_along = 0.0_r;
    RVec3 origin{};
    RVec3 across{ 1.0_r, 0.0_r, 0.0_r };
    RVec3 along{ 0.0_r, 0.0_r, 1.0_r };
    Real mass = 1.0_r;
    Real stretch_compliance = 0.0_r;
    Real shear_compliance = 1e-3_r;
    Real bend_compliance = 1e-3_r;
    Real damping = 0.01_r;
    Real thickness = 0.02_r;
    Real friction = 0.4_r;
    Real drag = 0.0_r;
    Real lift = 0.0_r;
    RVec3 wind{};
    Real gravity_scale = 1.0_r;
    int substeps = 10;
    bool collide = true;
    bool self_bending = true;
    std::uint64_t layer = 1;
    std::uint64_t mask = ~0ULL;
    Real coupling = 1.0_r;
    bool scale_with_resolution = false;
    Real reference_spacing = 0.1_r;

    Real row_spacing() const noexcept { return spacing_along > 0.0_r ? spacing_along : spacing; }
    Real width() const noexcept { return spacing * static_cast<Real>(columns > 1 ? columns - 1 : 0); }
    Real height() const noexcept { return row_spacing() * static_cast<Real>(rows > 1 ? rows - 1 : 0); }
    std::size_t triangle_count() const noexcept { return (columns > 1 && rows > 1) ? static_cast<std::size_t>(2 * (columns - 1) * (rows - 1)) : 0; }
    std::size_t particle_count() const noexcept { return static_cast<std::size_t>(columns > 0 ? columns : 0) * static_cast<std::size_t>(rows > 0 ? rows : 0); }
    void set_resolution(int new_columns, int new_rows);

    static ClothDef3D sheet(const RVec3& origin, const RVec3& across, const RVec3& along, Real width, Real height, int columns, int rows);
    static ClothDef3D sheet_with_triangles(const RVec3& origin, const RVec3& across, const RVec3& along, Real width, Real height, std::size_t triangles);
    static ClothDef3D rope(const RVec3& start, const RVec3& direction, int segments, Real length);
};

class Cloth3D {
public:
    explicit Cloth3D(const ClothDef3D& def);

    const ClothDef3D& def() const noexcept { return m_def; }
    ClothDef3D& edit_def() noexcept { return m_def; }

    std::size_t particle_count() const noexcept { return m_x.size(); }
    int columns() const noexcept { return m_def.columns; }
    int rows() const noexcept { return m_def.rows; }
    std::size_t triangle_count() const noexcept { return m_triangles.size(); }
    std::size_t link_count() const noexcept { return m_links.size(); }
    std::size_t index(int column, int row) const noexcept { return static_cast<std::size_t>(row) * static_cast<std::size_t>(m_def.columns) + static_cast<std::size_t>(column); }

    const std::vector<RVec3>& positions() const noexcept { return m_x; }
    const std::vector<RVec3>& velocities() const noexcept { return m_v; }
    RVec3 position(std::size_t i) const { return m_x.at(i); }
    RVec3 velocity(std::size_t i) const { return m_v.at(i); }
    void set_position(std::size_t i, const RVec3& p);
    void set_velocity(std::size_t i, const RVec3& v);
    Real particle_mass() const noexcept { return m_particle_mass; }

    void pin(std::size_t i);
    void pin(std::size_t i, const RVec3& world_point);
    void unpin(std::size_t i);
    bool pinned(std::size_t i) const;
    void attach(std::size_t i, RigidBody3D& body);
    void detach(std::size_t i);
    bool attached(std::size_t i) const;
    void set_wind(const RVec3& wind) noexcept { m_def.wind = wind; }

    const std::vector<std::array<std::uint32_t, 3>>& triangles() const noexcept { return m_triangles; }
    std::vector<RVec3> normals() const;
    AABB3D bounds() const;
    Real max_stretch() const;
    std::size_t contacts() const noexcept { return m_contacts; }
    void shift(const RVec3& offset);

    void step(Simulation3D& sim, Real dt);

private:
    struct Link {
        std::uint32_t a = 0;
        std::uint32_t b = 0;
        Real rest = 0.0_r;
        Real compliance = 0.0_r;
        std::uint8_t kind = 0;
    };

    struct Attachment {
        std::uint32_t particle = 0;
        BodyHandle body{};
        RVec3 local{};
    };

    void add_link(std::size_t a, std::size_t b, Real compliance, std::uint8_t kind);
    void solve_links(Real h);
    void apply_attachments(Simulation3D& sim);
    void collide(Simulation3D& sim, const std::vector<RigidBody3D*>& bodies, Real h);
    void aerodynamics(Real h);
    Real base_inv_mass(std::size_t i) const;

    ClothDef3D m_def;
    Real m_particle_mass = 0.0_r;
    std::vector<RVec3> m_x;
    std::vector<RVec3> m_prev;
    std::vector<RVec3> m_v;
    std::vector<Real> m_w;
    std::vector<std::uint8_t> m_pinned;
    std::vector<RVec3> m_pin_at;
    std::vector<Link> m_links;
    std::vector<Attachment> m_attachments;
    std::vector<std::array<std::uint32_t, 3>> m_triangles;
    std::size_t m_contacts = 0;
};

} // namespace physics
} // namespace fizmo

#endif // FIZMO_PHYSICS_CLOTH_3D_HPP
