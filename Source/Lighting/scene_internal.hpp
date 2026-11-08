#ifndef FIZMO_LIGHTING_SCENE_INTERNAL_HPP
#define FIZMO_LIGHTING_SCENE_INTERNAL_HPP

#include "scene.hpp"
#include "sh.hpp"

#include <array>
#include <cstdint>
#include <vector>

namespace fizmo {
namespace lighting {
namespace detail {

struct Ray {
    Vec3f o;
    Vec3f d;
    float tmax = kInfinity;
};

struct Tri {
    Vec3f v0;
    Vec3f e1;
    Vec3f e2;
    Vec3f n0;
    Vec3f n1;
    Vec3f n2;
    Vec2f uv0;
    Vec2f uv1;
    Vec2f uv2;
    std::uint32_t instance = 0;
    std::uint32_t material = 0;
    std::uint32_t local = 0;
    std::uint32_t flags = 0;
};

enum TriFlag : std::uint32_t {
    tri_casts_shadows = 1u << 0,
    tri_visible = 1u << 1,
    tri_static = 1u << 2,
    tri_smooth = 1u << 3,
    tri_lightmap = 1u << 4
};

struct Hit {
    float t = kInfinity;
    std::uint32_t tri = 0;
    float u = 0.0f;
    float v = 0.0f;
};

struct BvhNode {
    Vec3f lo;
    Vec3f hi;
    std::uint32_t first = 0;
    std::uint32_t count = 0;
};

class Bvh {
public:
    void build(std::vector<Tri>& tris);
    void refit(const std::vector<Tri>& tris);
    bool intersect(const std::vector<Tri>& tris, const Ray& ray, Hit& hit, std::uint32_t require = 0) const;
    bool any(const std::vector<Tri>& tris, const Ray& ray, std::uint32_t require = 0) const;
    bool empty() const noexcept { return m_nodes.empty(); }
    Vec3f lo() const noexcept { return m_nodes.empty() ? Vec3f{} : m_nodes[0].lo; }
    Vec3f hi() const noexcept { return m_nodes.empty() ? Vec3f{} : m_nodes[0].hi; }
    const std::vector<BvhNode>& nodes() const noexcept { return m_nodes; }

private:
    std::vector<BvhNode> m_nodes;
};

enum class LightKind : std::uint8_t {
    point,
    directional,
    sphere,
    disk,
    rect,
    tube,
    triangle
};

struct PreparedLight {
    LightKind kind = LightKind::point;
    Vec3f p;
    Vec3f n{ 0.0f, -1.0f, 0.0f };
    Vec3f t{ 1.0f, 0.0f, 0.0f };
    Vec3f b{ 0.0f, 0.0f, 1.0f };
    Vec3f e1;
    Vec3f e2;
    float radius = 0.0f;
    float half_length = 0.0f;
    float half_width = 0.0f;
    float half_height = 0.0f;
    float area = 0.0f;
    float cyl_area = 0.0f;
    Rgb emit;
    bool two_sided = false;
    bool cone = false;
    float cos_inner = 1.0f;
    float cos_outer = -1.0f;
    float tan_outer = 1.0f;
    float cos_sun = 1.0f;
    float sun_solid_angle = 0.0f;
    float range = 0.0f;
    Falloff falloff = Falloff::physical;
    float exponent = 2.0f;
    float diffuse = 1.0f;
    float specular = 1.0f;
    float indirect = 1.0f;
    float volumetric = 1.0f;
    std::uint32_t channels = 1u;
    bool shadows = true;
    float shadow_strength = 1.0f;
    float bias = 0.0f;
    int shadow_samples = 0;
    bool visible = true;
    LightMobility mobility = LightMobility::dynamic;
    const IesProfile* ies = nullptr;
    const LightCookie* cookie = nullptr;
    float cookie_size = 1.0f;
    float power = 0.0f;
    int source = -1;
    std::uint32_t tri = 0;
};

struct LightSample {
    Vec3f wi;
    float distance = 0.0f;
    Rgb radiance;
    float pdf = 0.0f;
    bool delta = false;
    Vec3f point;
    Vec3f normal;
};

struct EnvSampler {
    unsigned w = 0;
    unsigned h = 0;
    std::vector<float> marginal;
    std::vector<float> conditional;
    float total = 0.0f;
    bool ready() const noexcept { return total > 0.0f && w > 0 && h > 0; }
};

struct Compiled {
    std::vector<Tri> tris;
    Bvh bvh;
    std::vector<PreparedLight> lights;
    std::vector<std::size_t> emissive_begin;
    std::vector<float> light_cdf;
    float total_power = 0.0f;
    std::size_t analytic_count = 0;
    std::vector<float> emissive_cdf;
    float emissive_power = 0.0f;
    Vec3f lo;
    Vec3f hi;
    Vec3f center;
    float radius = 1.0f;
    Environment env;
    Frame env_frame;
    int sun = -1;
    Vec3f sun_dir{ 0.0f, -1.0f, 0.0f };
    Rgb sun_color;
    Sh9 env_sh;
    EnvSampler env_sampler;
    std::vector<std::uint32_t> instance_channels;
    std::vector<std::uint8_t> instance_static;
    std::vector<std::vector<std::uint32_t>> instance_tris;
    float built_area = 0.0f;
    std::uint32_t refits = 0;
};

Compiled compile(const Scene& scene);
bool refit_moved(Compiled& c, const Scene& scene, const std::vector<std::uint32_t>& moved);
void compile_lights(Compiled& c, const Scene& scene);

bool intersect(const Compiled& c, const Ray& ray, Hit& hit, std::uint32_t require = tri_visible);
bool occluded(const Compiled& c, const Ray& ray);

Rgb sky_radiance(const Compiled& c, const Vec3f& dir);
bool sample_environment(const Compiled& c, float u1, float u2, Vec3f& dir, Rgb& radiance, float& pdf);
float environment_pdf(const Compiled& c, const Vec3f& dir);

bool sample_light(const PreparedLight& l, const Vec3f& p, float u1, float u2, bool area, bool use_ies, bool use_cookie, LightSample& s);
Rgb light_factor(const PreparedLight& l, const Vec3f& p, bool use_ies, bool use_cookie);
bool intersect_light(const PreparedLight& l, const Ray& ray, float& t, Vec3f& point, Vec3f& normal);
float light_pdf(const PreparedLight& l, const Vec3f& p, const Vec3f& wi, float t, const Vec3f& point, const Vec3f& normal);
Rgb light_hit_radiance(const PreparedLight& l, const Vec3f& point, const Vec3f& normal, const Vec3f& viewer, bool use_ies, bool use_cookie);
bool light_reaches(const PreparedLight& l, const Vec3f& p) noexcept;
float light_importance(const PreparedLight& l, const Vec3f& p, const Vec3f& n) noexcept;

} // namespace detail
} // namespace lighting
} // namespace fizmo

#endif // FIZMO_LIGHTING_SCENE_INTERNAL_HPP
