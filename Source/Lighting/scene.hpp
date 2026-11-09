#ifndef FIZMO_LIGHTING_SCENE_HPP
#define FIZMO_LIGHTING_SCENE_HPP

#include "light.hpp"
#include "material.hpp"
#include "mesh.hpp"

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace fizmo {
namespace lighting {

struct Instance {
    std::string name;
    std::uint64_t id = 0;
    std::size_t mesh = 0;
    std::size_t material = 0;
    Transform transform;
    bool visible = true;
    bool casts_shadows = true;
    bool is_static = false;
    std::uint32_t channels = 1u;
    float lightmap_density = 8.0f;
    bool instanced = false;
};

enum class SkyKind : std::uint8_t {
    none,
    color,
    gradient,
    physical,
    image
};

const char* to_string(SkyKind v) noexcept;
std::optional<SkyKind> parse_sky_kind(const std::string& text) noexcept;

struct Environment {
    SkyKind kind = SkyKind::gradient;
    Rgb color{ 0.5f, 0.6f, 0.8f };
    Rgb zenith{ 0.25f, 0.45f, 0.85f };
    Rgb horizon{ 0.75f, 0.82f, 0.92f };
    Rgb ground{ 0.22f, 0.2f, 0.18f };
    float intensity = 1.0f;
    Vec3 up{ 0.0, 1.0, 0.0 };
    float sun_glow = 1.0f;
    float turbidity = 3.0f;
    unsigned image_width = 0;
    unsigned image_height = 0;
    std::vector<Rgb> image;
    float rotation_degrees = 0.0f;
    bool visible = true;
    bool lights_scene = true;

    static Environment none();
    static Environment uniform(const Rgb& c, float intensity = 1.0f);
    static Environment gradient(const Rgb& zenith, const Rgb& horizon, const Rgb& ground, float intensity = 1.0f);
    static Environment physical_sky(float turbidity = 3.0f, float intensity = 1.0f);
    static Environment equirect(unsigned width, unsigned height, std::vector<Rgb> texels, float intensity = 1.0f);
};

struct RayHit {
    bool hit = false;
    float distance = 0.0f;
    Vec3 position;
    Vec3 normal;
    std::size_t instance = 0;
    std::size_t triangle = 0;
    int light = -1;
};

namespace detail {
struct Compiled;
} // namespace detail

class Scene {
public:
    Scene();
    ~Scene();
    Scene(const Scene& other);
    Scene& operator=(const Scene& other);
    Scene(Scene&&) noexcept;
    Scene& operator=(Scene&&) noexcept;

    std::size_t add_mesh(Mesh mesh);
    std::size_t add_material(Material material);
    std::size_t add_instance(Instance instance);
    std::size_t add_instance(std::size_t mesh, std::size_t material, const Transform& transform = Transform());
    std::size_t add_light(Light light);
    std::size_t add_object(const Mesh& mesh, const Material& material, const Transform& transform = Transform());
    void clear();

    std::vector<Mesh>& meshes() { m_dirty = true; m_meshes_stale = true; return m_meshes; }
    const std::vector<Mesh>& meshes() const { return m_meshes; }
    std::vector<Material>& materials() { m_dirty = true; return m_materials; }
    const std::vector<Material>& materials() const { return m_materials; }
    std::vector<Instance>& instances() { m_dirty = true; return m_instances; }
    const std::vector<Instance>& instances() const { return m_instances; }
    std::vector<Light>& lights() { m_lights_dirty = true; return m_lights; }
    const std::vector<Light>& lights() const { return m_lights; }
    Light* light(std::size_t i) { m_lights_dirty = true; return i < m_lights.size() ? &m_lights[i] : nullptr; }
    Light* find_light(const std::string& name);
    Environment& environment() { m_lights_dirty = true; return m_environment; }
    const Environment& environment() const { return m_environment; }

    void mark_geometry_changed() { m_dirty = true; }
    bool replace_mesh(std::size_t index, Mesh mesh);
    bool replace_material(std::size_t index, Material material);
    std::size_t reshaped_meshes() const noexcept { return m_reshaped.size(); }
    std::uint64_t mesh_version(std::size_t index) const noexcept { return index < m_mesh_versions.size() ? m_mesh_versions[index] : 0; }
    std::size_t instanced_count() const;
    std::size_t blas_count() const;
    std::size_t traced_triangle_count() const;
    std::uint64_t full_builds() const noexcept { return m_full_builds; }
    std::uint64_t blas_builds() const noexcept { return m_blas_builds; }
    bool move_instance(std::size_t index, const Transform& transform);
    std::size_t moved_instances() const noexcept { return m_moved.size(); }
    void mark_lights_changed() { m_lights_dirty = true; }
    void commit();
    bool committed() const noexcept;
    std::uint64_t geometry_version() const noexcept { return m_geometry_version; }
    std::uint64_t lighting_version() const noexcept { return m_lighting_version; }

    RayHit intersect(const Vec3& origin, const Vec3& direction, float max_distance = kInfinity) const;
    bool occluded(const Vec3& from, const Vec3& to) const;
    Vec3 bounds_min() const;
    Vec3 bounds_max() const;
    std::size_t triangle_count() const;

    const detail::Compiled& compiled() const;

private:
    std::vector<Mesh> m_meshes;
    std::vector<Material> m_materials;
    std::vector<Instance> m_instances;
    std::vector<Light> m_lights;
    Environment m_environment;
    std::shared_ptr<detail::Compiled> m_compiled;
    bool m_dirty = true;
    bool m_lights_dirty = true;
    std::vector<std::uint32_t> m_moved;
    std::vector<std::uint8_t> m_moved_mark;
    std::uint64_t m_refit_count = 0;
    std::vector<std::uint64_t> m_mesh_versions;
    std::uint64_t m_mesh_counter = 0;
    bool m_meshes_stale = false;
    std::vector<std::uint32_t> m_reshaped;
    std::uint64_t m_full_builds = 0;
    std::uint64_t m_blas_builds = 0;
    std::shared_ptr<detail::Compiled> m_previous;
    void stamp_meshes();
    std::uint64_t m_geometry_version = 0;
    std::uint64_t m_lighting_version = 0;
};

} // namespace lighting
} // namespace fizmo

#endif // FIZMO_LIGHTING_SCENE_HPP
