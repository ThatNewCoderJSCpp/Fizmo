#ifndef FIZMO_LIGHTING_MATERIAL_HPP
#define FIZMO_LIGHTING_MATERIAL_HPP

#include "color.hpp"

#include <string>

namespace fizmo {
namespace lighting {

struct Material {
    std::string name;
    Rgb base_color{ 0.8f, 0.8f, 0.8f };
    float metallic = 0.0f;
    float roughness = 0.5f;
    float specular = 0.5f;

    Rgb emission{ 0.0f, 0.0f, 0.0f };
    float emission_strength = 1.0f;

    float transmission = 0.0f;
    float ior = 1.5f;
    float abbe = 0.0f;
    bool thin_walled = false;
    Rgb absorption_color{ 1.0f, 1.0f, 1.0f };
    float absorption_distance = 0.0f;

    float opacity = 1.0f;
    bool double_sided = true;

    static Material diffuse(const Rgb& color);
    static Material plastic(const Rgb& color, float roughness = 0.35f);
    static Material metal(const Rgb& color, float roughness = 0.2f);
    static Material mirror();
    static Material glass(float ior = 1.52f, float abbe = 64.0f);
    static Material frosted_glass(float roughness = 0.3f);
    static Material flint_glass();
    static Material crystal();
    static Material diamond();
    static Material water();
    static Material ice();
    static Material window_pane();
    static Material emissive(const Rgb& color, float strength = 1.0f);

    Rgb emitted() const noexcept { return emission * emission_strength; }
    bool emits() const noexcept { return !emitted().is_black(); }
    bool is_transmissive() const noexcept { return transmission > 0.0f && metallic < 1.0f; }
    bool is_dispersive() const noexcept { return abbe > 0.0f && is_transmissive(); }
    float ior_at(float nanometers) const noexcept;
};

} // namespace lighting
} // namespace fizmo

#endif // FIZMO_LIGHTING_MATERIAL_HPP
