#ifndef FIZMO_LIGHTING_BAKE_HPP
#define FIZMO_LIGHTING_BAKE_HPP

#include "probes.hpp"
#include "scene.hpp"
#include "settings.hpp"

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace fizmo {
namespace lighting {

struct Lightmap {
    unsigned width = 0;
    unsigned height = 0;
    std::vector<Rgb> texels;

    bool empty() const noexcept { return width == 0 || height == 0 || texels.empty(); }
    Rgb sample(float u, float v) const noexcept;
};

struct BakeOptions {
    int samples = 128;
    int bounces = 4;
    bool lightmaps = true;
    bool vertices = true;
    bool probes = true;
    float probe_spacing = 1.0f;
    int probe_rays = 256;
    int max_lightmap_size = 512;
    int dilation = 2;
    bool include_sky = true;
    unsigned threads = 0;
    std::uint64_t seed = 7;
};

struct BakedLighting {
    std::uint64_t geometry_version = 0;
    std::uint64_t lighting_version = 0;
    std::vector<std::vector<Rgb>> vertex;
    std::vector<Lightmap> lightmaps;
    ProbeVolume volume;

    bool empty() const noexcept { return vertex.empty() && lightmaps.empty() && volume.empty(); }
    bool has(std::size_t instance) const noexcept;
    bool save(const std::string& path) const;
    static std::optional<BakedLighting> load(const std::string& path);
};

BakedLighting bake(Scene& scene, const LightingSettings& settings, const BakeOptions& options = BakeOptions(), const std::function<void(float)>& progress = {});

} // namespace lighting
} // namespace fizmo

#endif // FIZMO_LIGHTING_BAKE_HPP
