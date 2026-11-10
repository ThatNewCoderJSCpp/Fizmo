#ifndef FIZMO_GPU_PATH_TRACER_HPP
#define FIZMO_GPU_PATH_TRACER_HPP

#include "render_target.hpp"
#include "../Lighting/renderer.hpp"
#include "../Lighting/scene.hpp"
#include "../Lighting/settings.hpp"

#include <cstdint>
#include <memory>
#include <string>

namespace fizmo {
namespace graphics {

class Renderer;

struct GpuTraceStats {
    std::uint32_t samples = 0;
    std::uint32_t frames = 0;
    std::uint32_t photons_per_frame = 0;
    std::size_t triangles = 0;
    std::size_t lights = 0;
    bool caustics = false;
    bool spectral = false;
};

class GpuPathTracer {
public:
    GpuPathTracer();
    ~GpuPathTracer();
    GpuPathTracer(const GpuPathTracer&) = delete;
    GpuPathTracer& operator=(const GpuPathTracer&) = delete;

    bool attach(Renderer& renderer, unsigned width, unsigned height);
    void detach();
    bool attached() const noexcept;
    bool supported() const noexcept;
    const std::string& error() const noexcept;

    void set_settings(const lighting::LightingSettings& settings);
    const lighting::LightingSettings& settings() const noexcept;
    void set_scene(lighting::Scene& scene);
    void scene_changed();
    void set_camera(const lighting::Camera& camera);
    const lighting::Camera& camera() const noexcept;
    void resize(unsigned width, unsigned height);
    void reset();
    void clear_history();
    void set_temporal(bool on, std::uint32_t history_cap = 24);
    bool temporal() const noexcept;

    void set_photons_per_frame(std::uint32_t photons);
    void set_samples_per_frame(int samples);
    void set_max_samples(std::uint32_t samples);
    void set_paused(bool paused);
    bool paused() const noexcept;

    unsigned width() const noexcept;
    unsigned height() const noexcept;
    std::uint32_t samples() const noexcept;
    bool converged() const noexcept;
    GpuTraceStats stats() const noexcept;

    const RenderTarget& target() const noexcept;
    void draw(Renderer& renderer, int x, int y, unsigned width, unsigned height, float opacity = 1.0f);
    void draw(Renderer& renderer, int x, int y);
    bool read_hdr(lighting::Image& out);

    static bool wants_gpu(const lighting::LightingSettings& settings) noexcept;

private:
    struct State;
    std::unique_ptr<State> m_state;
};

} // namespace graphics
} // namespace fizmo

#endif // FIZMO_GPU_PATH_TRACER_HPP
