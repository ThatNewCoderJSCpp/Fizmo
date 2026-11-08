#ifndef FIZMO_PHYSICS_FLUID_COMMON_HPP
#define FIZMO_PHYSICS_FLUID_COMMON_HPP

#include "../Rigid Body/common.hpp"

#include <cstdint>

namespace fizmo {
namespace physics {

enum class FluidLevel : std::uint8_t {
    blocky = 0,
    still,
    flowing,
    waves,
    realistic
};

enum class FluidQuality : std::uint8_t {
    low = 0,
    medium,
    high,
    ultra
};

enum class FluidPreset : std::uint8_t {
    water = 0,
    sea_water,
    oil,
    honey,
    lava,
    mud,
    mercury,
    custom
};

const char* to_string(FluidLevel level) noexcept;
const char* to_string(FluidQuality quality) noexcept;
const char* to_string(FluidPreset preset) noexcept;

struct FluidColor {
    float r = 0.2f;
    float g = 0.45f;
    float b = 0.8f;
    float a = 0.6f;
};

struct FluidMaterial {
    FluidPreset preset = FluidPreset::water;
    Real density = 1000.0_r;
    Real viscosity = 1.0e-3_r;
    Real surface_tension = 0.072_r;
    FluidColor color{};
    Real linear_drag = 1.0_r;
    Real quadratic_drag = 0.8_r;
    Real angular_drag = 0.6_r;
    Real friction = 0.1_r;
    Real restitution = 0.0_r;
    Real stiffness = 1.0_r;
    Real temperature = 293.15_r;
    bool emissive = false;

    Real kinematic_viscosity() const noexcept { return viscosity / std::max(density, 1e-6_r); }
    Real viscosity_factor() const noexcept;
    Real fluidity() const noexcept { return 1.0_r / viscosity_factor(); }
    Real opacity() const noexcept { return static_cast<Real>(color.a); }

    static FluidMaterial make(FluidPreset preset);
    static FluidMaterial water() { return make(FluidPreset::water); }
    static FluidMaterial sea_water() { return make(FluidPreset::sea_water); }
    static FluidMaterial oil() { return make(FluidPreset::oil); }
    static FluidMaterial honey() { return make(FluidPreset::honey); }
    static FluidMaterial lava() { return make(FluidPreset::lava); }
    static FluidMaterial mud() { return make(FluidPreset::mud); }
    static FluidMaterial mercury() { return make(FluidPreset::mercury); }
    static FluidMaterial custom(Real density, Real viscosity, Real surface_tension = 0.05_r);
};

struct FluidQualitySettings {
    FluidQuality quality = FluidQuality::medium;
    Real resolution_scale = 1.0_r;
    int max_substeps = 4;
    int solver_iterations = 3;
    int buoyancy_samples = 4;
    std::size_t max_particles = 30000;
    std::size_t particle_work_budget = 600000;
    int max_ticks_per_frame = 4;
    Real cfl = 0.5_r;

    static FluidQualitySettings preset(FluidQuality q);
};

struct FluidStillSettings {
    Real bob_amplitude = 0.0_r;
    Real bob_period = 4.0_r;
    Real bob_wavelength = 8.0_r;
};

struct FluidFlowingSettings {
    Real tick_rate = 30.0_r;
    Real max_mass = 1.0_r;
    Real compression = 0.03_r;
    Real horizontal_rate = 0.2_r;
    Real vertical_rate = 0.5_r;
    Real min_flow = 1e-5_r;
    bool keep_sources_full = true;
    Real momentum = 0.9_r;
};

struct FluidCouplingSettings {
    bool buoyancy = true;
    bool drag = true;
    bool push_fluid = true;
    bool two_way = true;
    Real buoyancy_scale = 1.0_r;
    Real drag_scale = 1.0_r;
    Real push_scale = 1.0_r;
    Real linear_damping = 0.8_r;
    Real angular_damping = 0.8_r;
    std::uint64_t layers = ~0ULL;
};

} // namespace physics
} // namespace fizmo

#endif // FIZMO_PHYSICS_FLUID_COMMON_HPP
