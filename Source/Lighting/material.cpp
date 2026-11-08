#include "material.hpp"

namespace fizmo {
namespace lighting {

Material Material::diffuse(const Rgb& color) {
    Material m;
    m.name = "diffuse";
    m.base_color = color;
    m.roughness = 1.0f;
    m.specular = 0.0f;
    return m;
}

Material Material::plastic(const Rgb& color, float rough) {
    Material m;
    m.name = "plastic";
    m.base_color = color;
    m.roughness = rough;
    return m;
}

Material Material::metal(const Rgb& color, float rough) {
    Material m;
    m.name = "metal";
    m.base_color = color;
    m.metallic = 1.0f;
    m.roughness = rough;
    return m;
}

Material Material::mirror() {
    Material m = metal(Rgb(0.97f), 0.0f);
    m.name = "mirror";
    return m;
}

Material Material::glass(float index, float abbe_number) {
    Material m;
    m.name = "glass";
    m.base_color = Rgb(1.0f);
    m.roughness = 0.0f;
    m.transmission = 1.0f;
    m.ior = index;
    m.abbe = abbe_number;
    return m;
}

Material Material::frosted_glass(float rough) {
    Material m = glass();
    m.name = "frosted glass";
    m.roughness = rough;
    return m;
}

Material Material::flint_glass() {
    Material m = glass(1.72f, 29.0f);
    m.name = "flint glass";
    return m;
}

Material Material::crystal() {
    Material m = glass(1.65f, 33.0f);
    m.name = "crystal";
    return m;
}

Material Material::diamond() {
    Material m = glass(2.417f, 55.3f);
    m.name = "diamond";
    return m;
}

Material Material::water() {
    Material m = glass(1.333f, 55.7f);
    m.name = "water";
    m.absorption_color = Rgb(0.55f, 0.85f, 0.92f);
    m.absorption_distance = 4.0f;
    return m;
}

Material Material::ice() {
    Material m = glass(1.309f, 56.0f);
    m.name = "ice";
    m.roughness = 0.08f;
    m.absorption_color = Rgb(0.85f, 0.95f, 1.0f);
    m.absorption_distance = 2.0f;
    return m;
}

Material Material::window_pane() {
    Material m = glass(1.52f, 0.0f);
    m.name = "window";
    m.thin_walled = true;
    return m;
}

Material Material::emissive(const Rgb& color, float strength) {
    Material m = diffuse(Rgb(0.0f));
    m.name = "emissive";
    m.emission = color;
    m.emission_strength = strength;
    return m;
}

float Material::ior_at(float nm) const noexcept {
    if (abbe <= 0.0f) return ior;
    const float lf = spectral::kHydrogenF * 1e-3f, lc = spectral::kHydrogenC * 1e-3f, ld = spectral::kSodiumD * 1e-3f;
    const float b = (ior - 1.0f) / (abbe * (1.0f / (lf * lf) - 1.0f / (lc * lc)));
    const float a = ior - b / (ld * ld);
    const float l = nm * 1e-3f;
    return a + b / (l * l);
}

} // namespace lighting
} // namespace fizmo
