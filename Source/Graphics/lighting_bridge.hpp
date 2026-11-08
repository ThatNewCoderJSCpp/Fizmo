#ifndef FIZMO_LIGHTING_BRIDGE_HPP
#define FIZMO_LIGHTING_BRIDGE_HPP

#include "lighting_3d.hpp"
#include "../Lighting/scene.hpp"
#include "../Lighting/settings.hpp"

namespace fizmo {
namespace graphics {

PointLight3D to_point_light(const lighting::Light& light, float scale = 1.0f);
SceneLighting3D to_scene_lighting(const lighting::Scene& scene, const lighting::LightingSettings& settings);
SceneLighting3D to_scene_lighting(const std::vector<lighting::Light>& lights, const lighting::Environment& environment, const lighting::LightingSettings& settings);

} // namespace graphics
} // namespace fizmo

#endif // FIZMO_LIGHTING_BRIDGE_HPP
