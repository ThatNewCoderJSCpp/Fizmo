#ifndef FIZMO_LIGHTING_RENDERER_HPP
#define FIZMO_LIGHTING_RENDERER_HPP

#include "bake.hpp"
#include "probes.hpp"
#include "scene.hpp"
#include "settings.hpp"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace fizmo {
namespace lighting {

struct Camera {
    Vec3 position{ 0.0, 1.0, 5.0 };
    Vec3 forward{ 0.0, 0.0, -1.0 };
    Vec3 up{ 0.0, 1.0, 0.0 };
    float fov_degrees = 60.0f;
    float aperture = 0.0f;
    float focus_distance = 0.0f;

    static Camera look_at(const Vec3& eye, const Vec3& target, const Vec3& up = Vec3{ 0.0, 1.0, 0.0 }, float fov_degrees = 60.0f);
};

class Image {
public:
    Image() = default;
    Image(unsigned width, unsigned height) : m_width(width), m_height(height), m_pixels(static_cast<std::size_t>(width) * height) {}

    unsigned width() const noexcept { return m_width; }
    unsigned height() const noexcept { return m_height; }
    bool empty() const noexcept { return m_pixels.empty(); }
    Rgb& at(unsigned x, unsigned y) { return m_pixels[static_cast<std::size_t>(y) * m_width + x]; }
    const Rgb& at(unsigned x, unsigned y) const { return m_pixels[static_cast<std::size_t>(y) * m_width + x]; }
    std::vector<Rgb>& pixels() noexcept { return m_pixels; }
    const std::vector<Rgb>& pixels() const noexcept { return m_pixels; }
    Rgb average() const noexcept;

    std::vector<std::uint8_t> to_rgba8(const LightingSettings& settings) const;
    bool write_ppm(const std::string& path, const LightingSettings& settings) const;
    bool write_pfm(const std::string& path) const;

private:
    unsigned m_width = 0;
    unsigned m_height = 0;
    std::vector<Rgb> m_pixels;
};

Rgb tone_map(const Rgb& hdr, const LightingSettings& settings) noexcept;

struct RenderStats {
    std::size_t samples_per_pixel = 0;
    std::size_t passes = 0;
    std::size_t photons = 0;
    std::size_t probes = 0;
    double last_pass_ms = 0.0;
};

class Renderer {
public:
    Renderer();
    explicit Renderer(const LightingSettings& settings);
    ~Renderer();
    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;

    void set_settings(const LightingSettings& settings);
    const LightingSettings& settings() const noexcept { return m_settings; }
    void set_baked(std::shared_ptr<const BakedLighting> baked);
    const BakedLighting* baked() const noexcept { return m_baked.get(); }

    const Image& render(Scene& scene, const Camera& camera, unsigned width, unsigned height);
    const Image& render_passes(Scene& scene, const Camera& camera, unsigned width, unsigned height, int passes);
    void reset();

    const Image& image() const noexcept { return m_output; }
    const Image& accumulated() const noexcept { return m_accum_view; }
    std::vector<std::uint8_t> rgba8() const { return m_output.to_rgba8(m_settings); }
    const RenderStats& stats() const noexcept { return m_stats; }
    const ProbeVolume& probes() const noexcept { return m_probes; }

    Rgb irradiance(Scene& scene, const Vec3& point, const Vec3& normal, int samples = 16);
    float light_level(Scene& scene, const Vec3& point, const Vec3& normal, int samples = 16);
    bool lit_by(Scene& scene, const Vec3& point, std::size_t light_index);

private:
    struct State;
    void prepare(Scene& scene, const Camera& camera, unsigned width, unsigned height);
    void pass(Scene& scene, const Camera& camera);
    void finish();

    LightingSettings m_settings;
    std::shared_ptr<const BakedLighting> m_baked;
    std::unique_ptr<State> m_state;
    ProbeVolume m_probes;
    Image m_output;
    Image m_accum_view;
    RenderStats m_stats;
};

} // namespace lighting
} // namespace fizmo

#endif // FIZMO_LIGHTING_RENDERER_HPP
