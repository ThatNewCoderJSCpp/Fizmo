#include "fizmo_library.hpp"
#include "fluid_common.hpp"

#include <cmath>

namespace fizmo {
namespace physics {

const char* to_string(FluidLevel level) noexcept {
    switch (level) {
    case FluidLevel::blocky: return "blocky";
    case FluidLevel::still: return "still";
    case FluidLevel::flowing: return "flowing";
    case FluidLevel::waves: return "waves";
    case FluidLevel::realistic: return "realistic";
    }
    return "unknown";
}

const char* to_string(FluidQuality quality) noexcept {
    switch (quality) {
    case FluidQuality::low: return "low";
    case FluidQuality::medium: return "medium";
    case FluidQuality::high: return "high";
    case FluidQuality::ultra: return "ultra";
    }
    return "unknown";
}

const char* to_string(FluidPreset preset) noexcept {
    switch (preset) {
    case FluidPreset::water: return "water";
    case FluidPreset::sea_water: return "sea_water";
    case FluidPreset::oil: return "oil";
    case FluidPreset::honey: return "honey";
    case FluidPreset::lava: return "lava";
    case FluidPreset::mud: return "mud";
    case FluidPreset::mercury: return "mercury";
    case FluidPreset::custom: return "custom";
    }
    return "unknown";
}

Real FluidMaterial::viscosity_factor() const noexcept {
    const Real ratio = std::max(viscosity, 1e-9_r) / 1.0e-3_r;
    return std::max(1.0_r, 1.0_r + std::log10(ratio));
}

FluidMaterial FluidMaterial::make(FluidPreset preset) {
    FluidMaterial m;
    m.preset = preset;
    switch (preset) {
    case FluidPreset::water:
    case FluidPreset::custom:
        break;
    case FluidPreset::sea_water:
        m.density = 1025.0_r;
        m.viscosity = 1.08e-3_r;
        m.surface_tension = 0.073_r;
        m.color = FluidColor{ 0.1f, 0.35f, 0.55f, 0.75f };
        break;
    case FluidPreset::oil:
        m.density = 900.0_r;
        m.viscosity = 0.08_r;
        m.surface_tension = 0.032_r;
        m.color = FluidColor{ 0.55f, 0.45f, 0.1f, 0.85f };
        m.friction = 0.02_r;
        m.quadratic_drag = 0.9_r;
        break;
    case FluidPreset::honey:
        m.density = 1420.0_r;
        m.viscosity = 10.0_r;
        m.surface_tension = 0.06_r;
        m.color = FluidColor{ 0.85f, 0.55f, 0.05f, 0.9f };
        m.friction = 0.6_r;
        m.angular_drag = 2.0_r;
        break;
    case FluidPreset::lava:
        m.density = 2600.0_r;
        m.viscosity = 100.0_r;
        m.surface_tension = 0.4_r;
        m.color = FluidColor{ 1.0f, 0.3f, 0.05f, 1.0f };
        m.temperature = 1400.0_r;
        m.emissive = true;
        m.friction = 0.5_r;
        m.angular_drag = 3.0_r;
        break;
    case FluidPreset::mud:
        m.density = 1700.0_r;
        m.viscosity = 2.0_r;
        m.surface_tension = 0.05_r;
        m.color = FluidColor{ 0.35f, 0.25f, 0.15f, 1.0f };
        m.friction = 0.8_r;
        m.quadratic_drag = 1.2_r;
        m.angular_drag = 2.0_r;
        break;
    case FluidPreset::mercury:
        m.density = 13534.0_r;
        m.viscosity = 1.53e-3_r;
        m.surface_tension = 0.485_r;
        m.color = FluidColor{ 0.75f, 0.76f, 0.78f, 1.0f };
        m.restitution = 0.2_r;
        m.friction = 0.05_r;
        break;
    }
    return m;
}

FluidMaterial FluidMaterial::custom(Real density, Real viscosity, Real surface_tension) {
    FluidMaterial m;
    m.preset = FluidPreset::custom;
    m.density = density;
    m.viscosity = viscosity;
    m.surface_tension = surface_tension;
    return m;
}

FluidQualitySettings FluidQualitySettings::preset(FluidQuality q) {
    FluidQualitySettings s;
    s.quality = q;
    switch (q) {
    case FluidQuality::low:
        s.resolution_scale = 2.0_r;
        s.max_substeps = 2;
        s.solver_iterations = 2;
        s.buoyancy_samples = 3;
        s.max_particles = 8000;
        s.particle_work_budget = 120000;
        s.max_ticks_per_frame = 2;
        s.cfl = 0.7_r;
        break;
    case FluidQuality::medium:
        break;
    case FluidQuality::high:
        s.resolution_scale = 1.0_r;
        s.max_substeps = 6;
        s.solver_iterations = 4;
        s.buoyancy_samples = 5;
        s.max_particles = 80000;
        s.particle_work_budget = 2000000;
        s.max_ticks_per_frame = 6;
        s.cfl = 0.4_r;
        break;
    case FluidQuality::ultra:
        s.resolution_scale = 0.5_r;
        s.max_substeps = 10;
        s.solver_iterations = 6;
        s.buoyancy_samples = 6;
        s.max_particles = 250000;
        s.particle_work_budget = 8000000;
        s.max_ticks_per_frame = 10;
        s.cfl = 0.3_r;
        break;
    }
    return s;
}

} // namespace physics
} // namespace fizmo
