#ifndef FIZMO_PARTICLES_DRAW_HPP
#define FIZMO_PARTICLES_DRAW_HPP

#include "particle_system.hpp"
#include "../mesh.hpp"
#include "../draw_types_2d.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace fizmo {
namespace graphics {
class Renderer;
} // namespace graphics

namespace particles {

enum class DrawPath : std::uint8_t { Instanced = 0, Triangles, Sprites };

struct ParticleSprite {
    float         x = 0.0f, y = 0.0f, z = 0.0f;
    float         size = 0.0f;
    float         axis_x = 0.0f, axis_y = 0.0f, axis_z = 0.0f;
    float         rotation = 0.0f;
    float         u0 = 0.0f, v0 = 0.0f, u1 = 1.0f, v1 = 1.0f;
    std::uint32_t rgba = 0xFFFFFFFFu;
    float         depth = 0.0f;
};

struct StagedParticle {
    float         x = 0.0f, y = 0.0f, z = 0.0f;
    float         size = 0.0f;
    std::uint32_t rgba = 0;
    std::uint32_t index = 0;
};

struct DrawBatch3D {
    const Emitter*           emitter = nullptr;
    DrawPath                 path = DrawPath::Triangles;
    ParticleBlend            blend = ParticleBlend::Alpha;
    RenderMode               mode = RenderMode::Billboard;
    bool                     lit = false;
    bool                     soft = false;
    float                    soft_distance = 0.0f;
    int                      texture_id = 0;
    const std::string*       texture_name = nullptr;
    const graphics::Texture* texture = nullptr;
    std::size_t              first = 0;
    std::size_t              count = 0;
    std::size_t              particles = 0;
    bool                     sorted = false;
};

struct DrawOptions {
    bool  allow_instancing = true;
    bool  sprites = false;
    bool  extrapolate = true;
    float min_pixel_size = -1.0f;
    float max_extrapolation = 0.1f;
};

struct DrawList3D {
    linalg::vector3d                  origin{};
    std::vector<graphics::Instance3D> instances;
    std::vector<graphics::Vertex3D>   vertices;
    std::vector<ParticleSprite>       sprites;
    std::vector<DrawBatch3D>          batches;
    graphics::Mesh3D                  billboard;
    std::size_t                       particles = 0;
    std::size_t                       culled = 0;

    std::vector<StagedParticle>       scratch_stage;
    std::vector<std::uint32_t>        scratch_index_b;
    std::vector<std::uint32_t>        scratch_order;
    std::vector<float>                scratch_depth;
    std::vector<std::uint16_t>        scratch_key;
    std::vector<std::uint16_t>        scratch_key_b;
    float                             basis[6] = { 0, 0, 0, 0, 0, 0 };

    void clear() noexcept {
        instances.clear();
        vertices.clear();
        sprites.clear();
        batches.clear();
        particles = 0;
        culled = 0;
    }
    void reserve(std::size_t particle_count);
};

struct DrawBatch2D {
    const Emitter*           emitter = nullptr;
    ParticleBlend            blend = ParticleBlend::Alpha;
    graphics::BlendMode      mode = graphics::BlendMode::Normal;
    int                      texture_id = 0;
    const std::string*       texture_name = nullptr;
    const graphics::Texture* texture = nullptr;
    std::size_t              first = 0;
    std::size_t              count = 0;
    std::size_t              particles = 0;
};

struct DrawList2D {
    std::vector<graphics::Vertex2D> vertices;
    std::vector<DrawBatch2D>        batches;
    std::size_t                     particles = 0;

    void clear() noexcept { vertices.clear(); batches.clear(); particles = 0; }
    void reserve(std::size_t particle_count) { vertices.reserve(particle_count * 6); }
};

void build_draw_list(const ParticleSystem& system, const CameraView& view, DrawList3D& out, const DrawOptions& options = DrawOptions{});
void append_draw_list(const Emitter& emitter, const CameraView& view, DrawList3D& out, const DrawOptions& options = DrawOptions{});

void build_draw_list_2d(const ParticleSystem& system, const CameraView& view, DrawList2D& out, const DrawOptions& options = DrawOptions{});
void append_draw_list_2d(const Emitter& emitter, const CameraView& view, DrawList2D& out, const DrawOptions& options = DrawOptions{});

graphics::Material3D particle_material(const DrawBatch3D& batch);
graphics::BlendMode particle_blend_2d(ParticleBlend blend) noexcept;
const graphics::Texture& default_particle_texture();

std::size_t draw(graphics::Renderer& renderer, const DrawList3D& list);
std::size_t draw(graphics::Renderer& renderer, const DrawList2D& list);

std::size_t draw_particles(graphics::Renderer& renderer, const ParticleSystem& system, const graphics::Camera3D& camera, DrawList3D& scratch);
std::size_t draw_particles_2d(graphics::Renderer& renderer, const ParticleSystem& system, DrawList2D& scratch);

CameraView view_of(const graphics::Renderer& renderer) noexcept;

} // namespace particles
} // namespace fizmo

#endif // FIZMO_PARTICLES_DRAW_HPP
