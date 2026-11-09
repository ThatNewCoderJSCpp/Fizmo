#include "scene_internal.hpp"

#include <algorithm>
#include <atomic>
#include <cctype>
#include <cstring>
#include <unordered_map>

namespace fizmo {
namespace lighting {

const char* to_string(SkyKind v) noexcept {
    switch (v) {
    case SkyKind::none: return "none";
    case SkyKind::color: return "color";
    case SkyKind::gradient: return "gradient";
    case SkyKind::physical: return "physical";
    case SkyKind::image: return "image";
    }
    return "gradient";
}

std::optional<SkyKind> parse_sky_kind(const std::string& text) noexcept {
    std::string s;
    for (char c : text) s.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
    if (s == "none" || s == "black" || s == "off") return SkyKind::none;
    if (s == "color" || s == "uniform" || s == "flat") return SkyKind::color;
    if (s == "gradient") return SkyKind::gradient;
    if (s == "physical" || s == "atmosphere" || s == "sky") return SkyKind::physical;
    if (s == "image" || s == "hdri" || s == "equirect") return SkyKind::image;
    return std::nullopt;
}

Environment Environment::none() {
    Environment e;
    e.kind = SkyKind::none;
    return e;
}

Environment Environment::uniform(const Rgb& c, float strength) {
    Environment e;
    e.kind = SkyKind::color;
    e.color = c;
    e.intensity = strength;
    return e;
}

Environment Environment::gradient(const Rgb& z, const Rgb& h, const Rgb& g, float strength) {
    Environment e;
    e.kind = SkyKind::gradient;
    e.zenith = z;
    e.horizon = h;
    e.ground = g;
    e.intensity = strength;
    return e;
}

Environment Environment::physical_sky(float t, float strength) {
    Environment e;
    e.kind = SkyKind::physical;
    e.turbidity = t;
    e.intensity = strength;
    return e;
}

Environment Environment::equirect(unsigned width, unsigned height, std::vector<Rgb> texels, float strength) {
    Environment e;
    e.kind = SkyKind::image;
    e.image_width = width;
    e.image_height = height;
    e.image = std::move(texels);
    e.intensity = strength;
    return e;
}

Scene::Scene() = default;
Scene::~Scene() = default;
Scene::Scene(Scene&&) noexcept = default;
Scene& Scene::operator=(Scene&&) noexcept = default;

Scene::Scene(const Scene& o)
    : m_meshes(o.m_meshes), m_materials(o.m_materials), m_instances(o.m_instances), m_lights(o.m_lights), m_environment(o.m_environment) {}

Scene& Scene::operator=(const Scene& o) {
    if (this == &o) return *this;
    m_meshes = o.m_meshes;
    m_materials = o.m_materials;
    m_instances = o.m_instances;
    m_lights = o.m_lights;
    m_environment = o.m_environment;
    if (m_compiled) m_previous = m_compiled;
    m_compiled.reset();
    m_meshes_stale = true;
    m_reshaped.clear();
    m_dirty = m_lights_dirty = true;
    return *this;
}

std::size_t Scene::add_mesh(Mesh mesh) {
    m_dirty = true;
    m_meshes.push_back(std::move(mesh));
    return m_meshes.size() - 1;
}

std::size_t Scene::add_material(Material material) {
    m_dirty = true;
    m_materials.push_back(std::move(material));
    return m_materials.size() - 1;
}

std::size_t Scene::add_instance(Instance instance) {
    m_dirty = true;
    m_instances.push_back(std::move(instance));
    return m_instances.size() - 1;
}

std::size_t Scene::add_instance(std::size_t mesh, std::size_t material, const Transform& transform) {
    Instance i;
    i.mesh = mesh;
    i.material = material;
    i.transform = transform;
    return add_instance(std::move(i));
}

std::size_t Scene::add_light(Light light) {
    m_lights_dirty = true;
    m_lights.push_back(std::move(light));
    return m_lights.size() - 1;
}

std::size_t Scene::add_object(const Mesh& mesh, const Material& material, const Transform& transform) {
    const std::size_t me = add_mesh(mesh);
    const std::size_t ma = add_material(material);
    return add_instance(me, ma, transform);
}

void Scene::clear() {
    m_meshes.clear();
    m_materials.clear();
    m_instances.clear();
    m_lights.clear();
    if (m_compiled) m_previous = m_compiled;
    m_compiled.reset();
    m_mesh_versions.clear();
    m_meshes_stale = false;
    m_reshaped.clear();
    for (std::uint32_t i : m_moved) {
        if (i < m_moved_mark.size()) m_moved_mark[i] = 0;
    }
    m_moved.clear();
    m_dirty = m_lights_dirty = true;
}

Light* Scene::find_light(const std::string& name) {
    for (auto& l : m_lights) {
        if (l.name == name) {
            m_lights_dirty = true;
            return &l;
        }
    }
    return nullptr;
}

void Scene::stamp_meshes() {
    if (m_mesh_versions.size() > m_meshes.size()) m_mesh_versions.resize(m_meshes.size());
    if (m_meshes_stale) {
        for (auto& v : m_mesh_versions) v = ++m_mesh_counter;
        m_meshes_stale = false;
    }
    while (m_mesh_versions.size() < m_meshes.size()) m_mesh_versions.push_back(++m_mesh_counter);
}

bool Scene::replace_mesh(std::size_t index, Mesh mesh) {
    if (index >= m_meshes.size()) return false;
    stamp_meshes();
    m_meshes[index] = std::move(mesh);
    m_mesh_versions[index] = ++m_mesh_counter;
    if (m_dirty) return true;
    const auto id = static_cast<std::uint32_t>(index);
    if (std::find(m_reshaped.begin(), m_reshaped.end(), id) == m_reshaped.end()) m_reshaped.push_back(id);
    return true;
}

bool Scene::replace_material(std::size_t index, Material material) {
    if (index >= m_materials.size()) return false;
    m_materials[index] = std::move(material);
    m_lights_dirty = true;
    return true;
}

std::size_t Scene::instanced_count() const {
    return m_compiled ? m_compiled->tlas.size() : 0;
}

std::size_t Scene::blas_count() const {
    if (!m_compiled) return 0;
    std::size_t n = 0;
    for (const auto& b : m_compiled->mesh_blas) n += b ? 1 : 0;
    return n;
}

std::size_t Scene::traced_triangle_count() const {
    if (!m_compiled) return 0;
    std::size_t n = m_compiled->tris.size();
    for (const auto& ti : m_compiled->tlas) {
        if (ti.mesh < m_compiled->mesh_blas.size() && m_compiled->mesh_blas[ti.mesh]) n += m_compiled->mesh_blas[ti.mesh]->tris.size();
    }
    return n;
}

bool Scene::move_instance(std::size_t index, const Transform& transform) {
    if (index >= m_instances.size()) return false;
    if (m_instances[index].transform.m == transform.m) return true;
    m_instances[index].transform = transform;
    if (m_dirty) return true;
    if (m_moved_mark.size() < m_instances.size()) m_moved_mark.resize(m_instances.size(), 0);
    if (!m_moved_mark[index]) {
        m_moved_mark[index] = 1;
        m_moved.push_back(static_cast<std::uint32_t>(index));
    }
    return true;
}

void Scene::commit() {
    auto clear_moved = [this]() {
        for (std::uint32_t i : m_moved) {
            if (i < m_moved_mark.size()) m_moved_mark[i] = 0;
        }
        m_moved.clear();
    };
    stamp_meshes();
    if (!m_compiled || m_dirty) {
        const detail::Compiled* previous = m_compiled ? m_compiled.get() : m_previous.get();
        auto c = std::make_shared<detail::Compiled>(detail::compile(*this, previous));
        m_blas_builds += c->blas_built;
        ++m_full_builds;
        m_compiled = std::move(c);
        m_previous.reset();
        m_dirty = false;
        m_lights_dirty = false;
        m_reshaped.clear();
        clear_moved();
        ++m_geometry_version;
        ++m_lighting_version;
        return;
    }
    if (!m_moved.empty() || !m_reshaped.empty()) {
        std::shared_ptr<detail::Compiled> c = m_compiled.use_count() == 1 ? m_compiled : std::make_shared<detail::Compiled>(*m_compiled);
        const std::uint64_t built = c->blas_built;
        const std::uint64_t lights = c->light_epoch;
        const bool ok = detail::update_compiled(*c, *this, m_moved, m_reshaped);
        clear_moved();
        m_reshaped.clear();
        bool relit = false;
        if (!ok) {
            c = std::make_shared<detail::Compiled>(detail::compile(*this, m_compiled.get()));
            m_blas_builds += c->blas_built;
            ++m_full_builds;
            relit = true;
        } else {
            m_blas_builds += c->blas_built - built;
            ++m_refit_count;
            relit = c->light_epoch != lights;
        }
        m_compiled = std::move(c);
        ++m_geometry_version;
        if (relit) {
            m_lights_dirty = false;
            ++m_lighting_version;
            return;
        }
    }
    if (m_lights_dirty) {
        auto c = m_compiled.use_count() == 1 ? m_compiled : std::make_shared<detail::Compiled>(*m_compiled);
        detail::compile_lights(*c, *this);
        c->light_epoch = detail::next_epoch();
        m_compiled = std::move(c);
        m_lights_dirty = false;
        ++m_lighting_version;
    }
}

bool Scene::committed() const noexcept {
    return m_compiled && !m_dirty && !m_lights_dirty && m_moved.empty() && m_reshaped.empty();
}

const detail::Compiled& Scene::compiled() const {
    static const detail::Compiled empty;
    return m_compiled ? *m_compiled : empty;
}

RayHit Scene::intersect(const Vec3& origin, const Vec3& direction, float max_distance) const {
    RayHit out;
    if (!m_compiled) return out;
    detail::Ray r{ Vec3f(origin), normalize(Vec3f(direction)), max_distance };
    detail::Hit h;
    if (!detail::intersect(*m_compiled, r, h)) return out;
    const detail::Tri& tri = detail::tri_of(*m_compiled, h);
    out.hit = true;
    out.distance = h.t;
    out.position = (r.o + r.d * h.t).to_double();
    out.normal = normalize(cross(tri.e1, tri.e2)).to_double();
    out.instance = tri.instance;
    out.triangle = tri.local;
    return out;
}

bool Scene::occluded(const Vec3& from, const Vec3& to) const {
    if (!m_compiled) return false;
    const Vec3f a(from), b(to);
    const Vec3f d = b - a;
    const float len = length(d);
    if (len <= 1e-6f) return false;
    detail::Ray r{ a, d / len, len * 0.999f };
    return detail::occluded(*m_compiled, r);
}

Vec3 Scene::bounds_min() const { return m_compiled ? m_compiled->lo.to_double() : Vec3{}; }
Vec3 Scene::bounds_max() const { return m_compiled ? m_compiled->hi.to_double() : Vec3{}; }
std::size_t Scene::triangle_count() const { return m_compiled ? m_compiled->tris.size() : 0; }

namespace detail {

namespace {

Rgb env_image(const Environment& e, const Vec3f& local) {
    if (e.image_width == 0 || e.image_height == 0 || e.image.empty()) return Rgb();
    float u = 0.5f + std::atan2(local.x, -local.z) * kInvTwoPi + e.rotation_degrees / 360.0f;
    u -= std::floor(u);
    const float v = std::acos(std::clamp(local.y, -1.0f, 1.0f)) * kInvPi;
    const unsigned x = std::min(e.image_width - 1, static_cast<unsigned>(u * static_cast<float>(e.image_width)));
    const unsigned y = std::min(e.image_height - 1, static_cast<unsigned>(v * static_cast<float>(e.image_height)));
    return e.image[static_cast<std::size_t>(y) * e.image_width + x];
}

float air_mass(float mu) {
    const float zenith = std::acos(std::clamp(mu, 0.0f, 1.0f)) / kDegToRad;
    return 1.0f / (std::max(mu, 0.0f) + 0.50572f * std::pow(std::max(96.07995f - zenith, 0.01f), -1.6364f));
}

Rgb physical_sky(const Compiled& c, const Vec3f& local) {
    const Environment& e = c.env;
    const Vec3f s = normalize(c.env_frame.to_local(-c.sun_dir));
    const Vec3f sl{ s.x, s.z, s.y };
    const float mu_v = local.y, mu_s = sl.y;
    const Rgb beta_r{ 5.802e-6f, 13.558e-6f, 33.1e-6f };
    const float beta_m = 21e-6f * std::max(e.turbidity, 1.0f) / 3.0f;
    const float hr = 8000.0f, hm = 1200.0f;
    const float cos_t = dot(local, sl);
    const float pr = 3.0f / (16.0f * kPi) * (1.0f + cos_t * cos_t);
    const float g = 0.76f;
    const float pm = (1.0f - g * g) / (4.0f * kPi * std::pow(std::max(1.0f + g * g - 2.0f * g * cos_t, 1e-4f), 1.5f));
    const float ms = air_mass(mu_s);
    Rgb sun_t;
    for (int i = 0; i < 3; ++i) sun_t[i] = std::exp(-(beta_r[i] * hr + beta_m * hm) * ms);
    const float sun_up = smoothstep(-0.08f, 0.02f, mu_s);
    const float mv = air_mass(std::max(mu_v, 0.0f));
    Rgb out;
    for (int i = 0; i < 3; ++i) {
        const float er = beta_r[i] * hr, em = beta_m * hm;
        const float frac = 1.0f - std::exp(-(er + em) * mv);
        const float wr = er / (er + em);
        out[i] = sun_t[i] * (wr * pr + (1.0f - wr) * pm) * frac;
    }
    out *= 24.0f * sun_up;
    out += Rgb(0.004f, 0.006f, 0.012f);
    if (mu_v < 0.0f) {
        const float k = smoothstep(0.0f, -0.15f, mu_v);
        const Rgb ground = e.ground * (0.3f * std::max(mu_s, 0.0f) + 0.02f);
        out = out * (1.0f - k) + ground * k;
    }
    return out;
}

Rgb gradient_sky(const Compiled& c, const Vec3f& local) {
    const Environment& e = c.env;
    const float h = local.y;
    Rgb out;
    if (h >= 0.0f) {
        const float t = std::pow(h, 0.5f);
        out = e.horizon * (1.0f - t) + e.zenith * t;
    } else {
        const float t = std::pow(std::min(-h * 4.0f, 1.0f), 0.5f);
        out = e.horizon * (1.0f - t) + e.ground * t;
    }
    if (c.sun >= 0 && e.sun_glow > 0.0f) {
        const Vec3f s = normalize(c.env_frame.to_local(-c.sun_dir));
        const Vec3f sl{ s.x, s.z, s.y };
        const float cs = std::max(dot(local, sl), 0.0f);
        out += c.sun_color * (e.sun_glow * 0.02f * std::pow(cs, 24.0f));
    }
    return out;
}

void build_env_sampler(Compiled& c) {
    EnvSampler& s = c.env_sampler;
    s = EnvSampler();
    const Environment& e = c.env;
    if (e.kind != SkyKind::image || e.image_width == 0 || e.image_height == 0 || e.image.size() < static_cast<std::size_t>(e.image_width) * e.image_height) return;
    s.w = e.image_width;
    s.h = e.image_height;
    s.conditional.assign(static_cast<std::size_t>(s.w + 1) * s.h, 0.0f);
    s.marginal.assign(s.h + 1, 0.0f);
    for (unsigned y = 0; y < s.h; ++y) {
        const float sin_t = std::sin((static_cast<float>(y) + 0.5f) / static_cast<float>(s.h) * kPi);
        float* row = &s.conditional[static_cast<std::size_t>(y) * (s.w + 1)];
        row[0] = 0.0f;
        for (unsigned x = 0; x < s.w; ++x) row[x + 1] = row[x] + std::max(e.image[static_cast<std::size_t>(y) * s.w + x].luminance(), 0.0f) * sin_t + 1e-8f;
        s.marginal[y + 1] = s.marginal[y] + row[s.w];
    }
    s.total = s.marginal[s.h];
}

} // namespace

Rgb sky_radiance(const Compiled& c, const Vec3f& dir) {
    const Environment& e = c.env;
    const Vec3f l3 = c.env_frame.to_local(dir);
    const Vec3f local{ l3.x, l3.z, l3.y };
    Rgb out;
    switch (e.kind) {
    case SkyKind::none: return Rgb();
    case SkyKind::color: out = e.color; break;
    case SkyKind::gradient: out = gradient_sky(c, local); break;
    case SkyKind::physical: out = physical_sky(c, local); break;
    case SkyKind::image: out = env_image(e, local); break;
    }
    return out * e.intensity;
}

bool sample_environment(const Compiled& c, float u1, float u2, Vec3f& dir, Rgb& radiance, float& pdf) {
    const EnvSampler& s = c.env_sampler;
    if (!s.ready()) return false;
    const float target_y = u1 * s.total;
    unsigned y = static_cast<unsigned>(std::upper_bound(s.marginal.begin(), s.marginal.end(), target_y) - s.marginal.begin());
    y = std::clamp(y, 1u, s.h) - 1u;
    const float* row = &s.conditional[static_cast<std::size_t>(y) * (s.w + 1)];
    const float target_x = u2 * row[s.w];
    unsigned x = static_cast<unsigned>(std::upper_bound(row, row + s.w + 1, target_x) - row);
    x = std::clamp(x, 1u, s.w) - 1u;
    const float u = (static_cast<float>(x) + 0.5f) / static_cast<float>(s.w) - c.env.rotation_degrees / 360.0f;
    const float v = (static_cast<float>(y) + 0.5f) / static_cast<float>(s.h);
    const float theta = v * kPi, phi = (u - 0.5f) * kTwoPi;
    const float st = std::sin(theta);
    const Vec3f local{ st * std::sin(phi), std::cos(theta), -st * std::cos(phi) };
    dir = normalize(c.env_frame.to_world(Vec3f{ local.x, local.z, local.y }));
    const float texel_p = (row[x + 1] - row[x]) / s.total;
    const float texel_solid = (kTwoPi / static_cast<float>(s.w)) * (kPi / static_cast<float>(s.h)) * std::max(st, 1e-6f);
    pdf = texel_p / texel_solid;
    radiance = sky_radiance(c, dir);
    return pdf > 0.0f;
}

float environment_pdf(const Compiled& c, const Vec3f& dir) {
    const EnvSampler& s = c.env_sampler;
    if (!s.ready()) return 0.0f;
    const Vec3f l3 = c.env_frame.to_local(dir);
    const Vec3f local{ l3.x, l3.z, l3.y };
    float u = 0.5f + std::atan2(local.x, -local.z) * kInvTwoPi + c.env.rotation_degrees / 360.0f;
    u -= std::floor(u);
    const float v = std::acos(std::clamp(local.y, -1.0f, 1.0f)) * kInvPi;
    const unsigned x = std::min(s.w - 1, static_cast<unsigned>(u * static_cast<float>(s.w)));
    const unsigned y = std::min(s.h - 1, static_cast<unsigned>(v * static_cast<float>(s.h)));
    const float* row = &s.conditional[static_cast<std::size_t>(y) * (s.w + 1)];
    const float st = std::sin((static_cast<float>(y) + 0.5f) / static_cast<float>(s.h) * kPi);
    const float texel_solid = (kTwoPi / static_cast<float>(s.w)) * (kPi / static_cast<float>(s.h)) * std::max(st, 1e-6f);
    return (row[x + 1] - row[x]) / s.total / texel_solid;
}

bool intersect(const Compiled& c, const Ray& ray, Hit& hit, std::uint32_t require) {
    hit.inst = kNoInstance;
    bool found = c.bvh.intersect(c.tris, ray, hit, require);
    if (!c.tlas.empty() && intersect_instances(c, ray, hit, require)) {
        resolve_instance_hit(c, hit);
        found = true;
    }
    return found;
}

bool occluded(const Compiled& c, const Ray& ray) {
    if (c.bvh.any(c.tris, ray, tri_casts_shadows)) return true;
    return !c.tlas.empty() && any_instance(c, ray, tri_casts_shadows);
}

std::uint64_t next_epoch() noexcept {
    static std::atomic<std::uint64_t> counter{ 0 };
    return ++counter;
}

Transform inverse_transform(const Transform& t) noexcept {
    const auto& x = t.m;
    const float a = x[0], b = x[1], c = x[2], d = x[4], e = x[5], f = x[6], g = x[8], h = x[9], i = x[10];
    const float det = a * (e * i - f * h) - b * (d * i - f * g) + c * (d * h - e * g);
    Transform r;
    if (std::fabs(det) < 1e-30f) {
        r.m = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
        return r;
    }
    const float k = 1.0f / det;
    r.m[0] = (e * i - f * h) * k;
    r.m[1] = (c * h - b * i) * k;
    r.m[2] = (b * f - c * e) * k;
    r.m[4] = (f * g - d * i) * k;
    r.m[5] = (a * i - c * g) * k;
    r.m[6] = (c * d - a * f) * k;
    r.m[8] = (d * h - e * g) * k;
    r.m[9] = (b * g - a * h) * k;
    r.m[10] = (a * e - b * d) * k;
    const float tx = x[3], ty = x[7], tz = x[11];
    r.m[3] = -(r.m[0] * tx + r.m[1] * ty + r.m[2] * tz);
    r.m[7] = -(r.m[4] * tx + r.m[5] * ty + r.m[6] * tz);
    r.m[11] = -(r.m[8] * tx + r.m[9] * ty + r.m[10] * tz);
    return r;
}

void resolve_instance_hit(const Compiled& c, Hit& hit) {
    if (hit.inst >= c.tlas.size()) return;
    const TlasInstance& ti = c.tlas[hit.inst];
    if (ti.mesh >= c.mesh_blas.size() || !c.mesh_blas[ti.mesh]) return;
    const Blas& b = *c.mesh_blas[ti.mesh];
    if (hit.blas_tri >= b.tris.size()) return;
    const Tri& lt = b.tris[hit.blas_tri];
    Tri w = lt;
    const Transform& x = ti.to_world;
    w.v0 = x.point(lt.v0);
    w.e1 = x.vector(lt.e1);
    w.e2 = x.vector(lt.e2);
    w.n0 = x.normal(lt.n0);
    w.n1 = x.normal(lt.n1);
    w.n2 = x.normal(lt.n2);
    w.instance = ti.instance;
    w.material = ti.material;
    w.flags = (lt.flags & ~(tri_casts_shadows | tri_visible | tri_static)) | ti.flags;
    if (lt.tangents < b.tangents.size()) {
        const TriTangents& src = b.tangents[lt.tangents];
        for (int k = 0; k < 3; ++k) {
            hit.world_tangents.t[k] = normalize(x.vector(src.t[k]));
            hit.world_tangents.sign[k] = src.sign[k];
        }
        w.tangents = kHitTangents;
    } else {
        w.tangents = 0xFFFFFFFFu;
    }
    w.colors = lt.colors < b.colors.size() ? kHitTangents : 0xFFFFFFFFu;
    hit.world = w;
}

const Tri& tri_of(const Compiled& c, const Hit& hit) noexcept {
    if (hit.inst != kNoInstance) return hit.world;
    return c.tris[hit.tri];
}

const TriTangents* tangents_of(const Compiled& c, const Tri& tri, const Hit& hit) noexcept {
    if (tri.tangents == kHitTangents) return &hit.world_tangents;
    return tri.tangents < c.tangents.size() ? &c.tangents[tri.tangents] : nullptr;
}

const TriColors* colors_of(const Compiled& c, const Tri& tri, const Hit& hit) noexcept {
    if (tri.colors == kHitTangents) {
        if (hit.inst >= c.tlas.size()) return nullptr;
        const TlasInstance& ti = c.tlas[hit.inst];
        if (ti.mesh >= c.mesh_blas.size() || !c.mesh_blas[ti.mesh]) return nullptr;
        const Blas& b = *c.mesh_blas[ti.mesh];
        if (hit.blas_tri >= b.tris.size()) return nullptr;
        const std::uint32_t k = b.tris[hit.blas_tri].colors;
        return k < b.colors.size() ? &b.colors[k] : nullptr;
    }
    return tri.colors < c.colors.size() ? &c.colors[tri.colors] : nullptr;
}

void compile_lights(Compiled& c, const Scene& scene) {
    c.lights.clear();
    c.light_cdf.clear();
    c.total_power = 0.0f;
    c.env = scene.environment();
    c.env_frame = Frame(normalize(Vec3f(c.env.up)));
    c.sun = -1;
    float best_sun = -1.0f;
    const auto& src = scene.lights();
    for (std::size_t i = 0; i < src.size(); ++i) {
        const Light& l = src[i];
        if (!l.enabled || l.intensity <= 0.0f) continue;
        PreparedLight p;
        p.source = static_cast<int>(i);
        p.p = Vec3f(l.position);
        p.n = normalize(Vec3f(l.direction));
        if (length_squared(p.n) == 0.0f) p.n = { 0.0f, -1.0f, 0.0f };
        Vec3f up = normalize(Vec3f(l.up));
        if (length_squared(up) == 0.0f || std::fabs(dot(up, p.n)) > 0.999f) up = Frame(p.n).b;
        p.t = normalize(cross(up, p.n));
        p.b = cross(p.n, p.t);
        p.emit = l.tint() * l.emitted_scale();
        p.two_sided = l.two_sided;
        p.range = std::max(l.range, 0.0f);
        p.falloff = l.falloff;
        p.exponent = l.falloff_exponent;
        p.diffuse = l.diffuse;
        p.specular = l.specular;
        p.indirect = l.indirect;
        p.volumetric = l.volumetric;
        p.channels = l.channels;
        p.shadows = l.casts_shadows;
        p.shadow_strength = std::clamp(l.shadow_strength, 0.0f, 1.0f);
        p.bias = l.shadow_bias;
        p.shadow_samples = l.shadow_samples;
        p.visible = l.visible;
        p.mobility = l.mobility;
        p.ies = l.ies.get();
        p.cookie = l.cookie.get();
        p.cookie_size = std::max(l.cookie_size, 1e-4f);
        if (l.has_cone()) {
            p.cone = true;
            const float outer = std::clamp(l.outer_cone, 0.01f, 179.9f);
            const float inner = std::clamp(l.inner_cone, 0.0f, outer);
            p.cos_outer = std::cos(outer * kDegToRad);
            p.cos_inner = std::cos(inner * kDegToRad);
            if (p.cos_inner - p.cos_outer < 1e-4f) p.cos_inner = p.cos_outer + 1e-4f;
            p.tan_outer = std::tan(std::min(outer, 89.0f) * kDegToRad);
        }
        switch (l.shape) {
        case LightShape::point:
        case LightShape::spot:
            if (l.radius > 0.0f) {
                p.kind = LightKind::sphere;
                p.radius = l.radius;
                p.emit = l.tint() * (l.emitted_scale() / (kPi * l.radius * l.radius));
                p.area = 4.0f * kPi * l.radius * l.radius;
            } else {
                p.kind = LightKind::point;
            }
            break;
        case LightShape::directional: {
            p.kind = LightKind::directional;
            const float half = std::clamp(l.angular_diameter * 0.5f, 0.0f, 45.0f) * kDegToRad;
            p.cos_sun = std::cos(half);
            p.sun_solid_angle = kTwoPi * (1.0f - p.cos_sun);
            const float lum = p.emit.luminance();
            if (lum > best_sun) {
                best_sun = lum;
                c.sun = static_cast<int>(c.lights.size());
                c.sun_dir = p.n;
                c.sun_color = p.emit;
            }
            break;
        }
        case LightShape::sphere:
            if (l.radius <= 0.0f) {
                p.kind = LightKind::point;
                break;
            }
            p.kind = LightKind::sphere;
            p.radius = l.radius;
            p.area = 4.0f * kPi * l.radius * l.radius;
            break;
        case LightShape::disk:
            if (l.radius <= 0.0f) {
                p.kind = LightKind::point;
                break;
            }
            p.kind = LightKind::disk;
            p.radius = l.radius;
            p.area = kPi * l.radius * l.radius;
            break;
        case LightShape::rectangle:
            if (l.width <= 0.0f || l.height <= 0.0f) {
                p.kind = LightKind::point;
                break;
            }
            p.kind = LightKind::rect;
            p.half_width = l.width * 0.5f;
            p.half_height = l.height * 0.5f;
            p.area = l.width * l.height;
            break;
        case LightShape::tube: {
            p.kind = LightKind::tube;
            p.radius = std::max(l.radius, std::max(0.005f, l.length * 0.005f));
            p.half_length = std::max(l.length, 0.0f) * 0.5f;
            p.cyl_area = kTwoPi * p.radius * 2.0f * p.half_length;
            p.area = p.cyl_area + 4.0f * kPi * p.radius * p.radius;
            Light copy = l;
            copy.radius = p.radius;
            p.emit = l.tint() * copy.emitted_scale();
            break;
        }
        }
        if (p.kind == LightKind::point && (l.shape == LightShape::sphere || l.shape == LightShape::disk || l.shape == LightShape::rectangle)) {
            Light copy = l;
            copy.shape = LightShape::point;
            copy.radius = 0.0f;
            p.emit = l.tint() * copy.emitted_scale();
        }
        p.power = std::max(l.luminous_power(), 1e-6f);
        if (p.kind == LightKind::directional) p.power = std::max(p.emit.luminance() * kPi * c.radius * c.radius, 1e-6f);
        c.lights.push_back(p);
    }
    c.emissive_begin.clear();
    c.emissive_begin.push_back(c.lights.size());
    for (std::size_t i = 0; i < c.tris.size(); ++i) {
        const Tri& tri = c.tris[i];
        if (tri.material >= scene.materials().size()) continue;
        const Material& m = scene.materials()[tri.material];
        if (!m.emits()) continue;
        const Rgb e = m.average_emitted();
        if (e.is_black()) continue;
        PreparedLight p;
        p.kind = LightKind::triangle;
        p.p = tri.v0;
        p.e1 = tri.e1;
        p.e2 = tri.e2;
        const Vec3f cr = cross(tri.e1, tri.e2);
        p.area = 0.5f * length(cr);
        if (p.area <= 1e-10f) continue;
        p.n = normalize(cr);
        const Vec3f shade = normalize(tri.n0 + tri.n1 + tri.n2);
        if (dot(shade, p.n) < 0.0f) p.n = -p.n;
        p.emit = e;
        p.two_sided = m.double_sided;
        p.tri = static_cast<std::uint32_t>(i);
        p.channels = tri.instance < c.instance_channels.size() ? c.instance_channels[tri.instance] : 1u;
        p.shadows = true;
        p.mobility = (tri.flags & tri_static) ? LightMobility::baked : LightMobility::dynamic;
        p.power = std::max(e.luminance() * kPi * p.area * (p.two_sided ? 2.0f : 1.0f), 1e-8f);
        c.lights.push_back(p);
    }
    c.analytic_count = c.emissive_begin.front();
    c.emissive_cdf.assign(c.lights.size() - c.analytic_count + 1, 0.0f);
    for (std::size_t i = c.analytic_count; i < c.lights.size(); ++i) c.emissive_cdf[i - c.analytic_count + 1] = c.emissive_cdf[i - c.analytic_count] + c.lights[i].power;
    c.emissive_power = c.emissive_cdf.back();
    c.light_cdf.resize(c.lights.size() + 1, 0.0f);
    for (std::size_t i = 0; i < c.lights.size(); ++i) c.light_cdf[i + 1] = c.light_cdf[i] + c.lights[i].power;
    c.total_power = c.light_cdf.back();
    build_env_sampler(c);
    c.env_sh = Sh9();
    if (c.env.kind != SkyKind::none && c.env.lights_scene) {
        const int n = 1024;
        for (int i = 0; i < n; ++i) {
            const float u1 = (static_cast<float>(i) + 0.5f) / static_cast<float>(n);
            const float u2 = std::fmod(static_cast<float>(i) * 0.6180339887f, 1.0f);
            const Vec3f d = sample_uniform_sphere(u1, u2);
            c.env_sh.add(d, sky_radiance(c, d), 4.0f * kPi / static_cast<float>(n));
        }
    }
}

namespace {

TriTangents tangent_frame(const Transform& xf, const Mesh& m, std::uint32_t i0, std::uint32_t i1, std::uint32_t i2) {
    TriTangents t;
    const std::uint32_t idx[3] = { i0, i1, i2 };
    for (int k = 0; k < 3; ++k) {
        const Vec4f& v = m.tangents[idx[k]];
        t.t[k] = normalize(xf.vector(Vec3f{ v.x, v.y, v.z }));
        t.sign[k] = v.w < 0.0f ? -1.0f : 1.0f;
    }
    return t;
}

TriColors color_frame(const Mesh& m, std::uint32_t i0, std::uint32_t i1, std::uint32_t i2) {
    TriColors t;
    t.c[0] = m.colors[i0];
    t.c[1] = m.colors[i1];
    t.c[2] = m.colors[i2];
    return t;
}

std::uint64_t mix64(std::uint64_t h, std::uint64_t v) noexcept {
    h ^= v + 0x9E3779B97F4A7C15ULL + (h << 6) + (h >> 2);
    h *= 0xBF58476D1CE4E5B9ULL;
    return h ^ (h >> 31);
}

template <typename T>
std::uint64_t hash_bytes(std::uint64_t h, const std::vector<T>& v) noexcept {
    h = mix64(h, v.size());
    const auto* p = reinterpret_cast<const unsigned char*>(v.data());
    const std::size_t n = v.size() * sizeof(T);
    std::size_t i = 0;
    std::uint64_t a = 0, b = 0;
    for (; i + 16 <= n; i += 16) {
        std::uint64_t w0, w1;
        std::memcpy(&w0, p + i, 8);
        std::memcpy(&w1, p + i + 8, 8);
        a = (a ^ w0) * 0x9E3779B97F4A7C15ULL;
        b = (b ^ w1) * 0xC2B2AE3D27D4EB4FULL;
        a ^= a >> 29;
        b ^= b >> 32;
    }
    for (; i < n; ++i) a = (a ^ p[i]) * 0x100000001B3ULL;
    return mix64(mix64(h, a), b);
}

std::uint32_t mesh_flags(const Mesh& m) noexcept {
    std::uint32_t flags = 0;
    if (m.has_normals()) flags |= tri_smooth;
    if (m.has_lightmap_uvs()) flags |= tri_lightmap;
    if (m.has_uvs()) flags |= tri_texcoords;
    return flags;
}

bool fill_tri(const Mesh& m, const Transform& xf, std::size_t t, Tri& tri) {
    const std::uint32_t i0 = m.indices[t], i1 = m.indices[t + 1], i2 = m.indices[t + 2];
    if (i0 >= m.positions.size() || i1 >= m.positions.size() || i2 >= m.positions.size()) return false;
    const Vec3f a = xf.point(m.positions[i0]);
    const Vec3f b = xf.point(m.positions[i1]);
    const Vec3f cc = xf.point(m.positions[i2]);
    tri.v0 = a;
    tri.e1 = b - a;
    tri.e2 = cc - a;
    if (m.has_normals()) {
        tri.n0 = xf.normal(m.normals[i0]);
        tri.n1 = xf.normal(m.normals[i1]);
        tri.n2 = xf.normal(m.normals[i2]);
    } else {
        tri.n0 = tri.n1 = tri.n2 = normalize(cross(tri.e1, tri.e2));
    }
    if (m.has_lightmap_uvs()) {
        tri.uv0 = m.lightmap_uvs[i0];
        tri.uv1 = m.lightmap_uvs[i1];
        tri.uv2 = m.lightmap_uvs[i2];
    }
    if (m.has_uvs()) {
        tri.tex0 = m.uvs[i0];
        tri.tex1 = m.uvs[i1];
        tri.tex2 = m.uvs[i2];
    }
    return true;
}

void box_of(const Blas& b, const Transform& xf, Vec3f& lo, Vec3f& hi) {
    lo = { kInfinity, kInfinity, kInfinity };
    hi = { -kInfinity, -kInfinity, -kInfinity };
    for (int k = 0; k < 8; ++k) {
        const Vec3f p{ (k & 1) ? b.hi.x : b.lo.x, (k & 2) ? b.hi.y : b.lo.y, (k & 4) ? b.hi.z : b.lo.z };
        const Vec3f w = xf.point(p);
        lo = vmin(lo, w);
        hi = vmax(hi, w);
    }
}

std::uint32_t instance_flags(const Instance& inst) noexcept {
    std::uint32_t flags = 0;
    if (inst.casts_shadows) flags |= tri_casts_shadows;
    if (inst.visible) flags |= tri_visible;
    if (inst.is_static) flags |= tri_static;
    return flags;
}

void set_bounds(Compiled& c) {
    const bool flat = !c.tris.empty() && !c.bvh.empty();
    const bool inst = !c.tlas.empty() && !c.tlas_bvh.empty();
    if (flat) {
        c.flat_lo = c.bvh.lo();
        c.flat_hi = c.bvh.hi();
    }
    if (flat && inst) {
        c.lo = vmin(c.flat_lo, c.tlas_bvh.lo());
        c.hi = vmax(c.flat_hi, c.tlas_bvh.hi());
    } else if (flat) {
        c.lo = c.flat_lo;
        c.hi = c.flat_hi;
    } else if (inst) {
        c.lo = c.tlas_bvh.lo();
        c.hi = c.tlas_bvh.hi();
    } else {
        c.lo = { -1.0f, -1.0f, -1.0f };
        c.hi = { 1.0f, 1.0f, 1.0f };
    }
    c.center = (c.lo + c.hi) * 0.5f;
    c.radius = std::max(length(c.hi - c.lo) * 0.5f, 1e-3f);
}

float box_area(const Vec3f& lo, const Vec3f& hi) noexcept {
    const Vec3f size = hi - lo;
    return 2.0f * (size.x * size.y + size.y * size.z + size.z * size.x);
}

bool refit_flat(Compiled& c, const Scene& scene, const std::vector<std::uint32_t>& moved) {
    const auto& instances = scene.instances();
    const auto& meshes = scene.meshes();
    if (c.tris.empty() || c.bvh.empty() || c.refits >= 240) return false;
    if (c.instance_tris.size() != instances.size()) {
        c.instance_tris.assign(instances.size(), {});
        for (std::uint32_t i = 0; i < c.tris.size(); ++i) {
            const std::uint32_t inst = c.tris[i].instance;
            if (inst >= instances.size()) return false;
            c.instance_tris[inst].push_back(i);
        }
    }
    for (std::uint32_t ii : moved) {
        if (ii >= instances.size()) return false;
        const Instance& inst = instances[ii];
        if (inst.mesh >= meshes.size()) continue;
        const Mesh& m = meshes[inst.mesh];
        for (std::uint32_t ti : c.instance_tris[ii]) {
            Tri& tri = c.tris[ti];
            const std::size_t t = static_cast<std::size_t>(tri.local) * 3;
            if (t + 2 >= m.indices.size()) return false;
            if (!fill_tri(m, inst.transform, t, tri)) return false;
            if (tri.tangents < c.tangents.size() && m.has_tangents()) c.tangents[tri.tangents] = tangent_frame(inst.transform, m, m.indices[t], m.indices[t + 1], m.indices[t + 2]);
        }
    }
    c.bvh.refit(c.tris);
    ++c.refits;
    if (c.built_area > 0.0f && box_area(c.bvh.lo(), c.bvh.hi()) > 2.0f * c.built_area) return false;
    return true;
}

std::shared_ptr<const Blas> find_blas(const Compiled* previous, const Mesh& m, std::size_t index, std::uint64_t version, std::unordered_map<std::uint64_t, std::shared_ptr<const Blas>>& by_hash, bool& indexed) {
    if (!previous) return nullptr;
    if (index < previous->mesh_blas.size() && index < previous->blas_versions.size() && previous->mesh_blas[index] && previous->blas_versions[index] == version) return previous->mesh_blas[index];
    if (!indexed) {
        indexed = true;
        for (const auto& b : previous->mesh_blas)
            if (b) by_hash.emplace(b->hash, b);
    }
    if (by_hash.empty()) return nullptr;
    const auto it = by_hash.find(mesh_hash(m));
    if (it == by_hash.end()) return nullptr;
    if (it->second->index_count != m.indices.size() || it->second->vertex_count != m.positions.size()) return nullptr;
    return it->second;
}

} // namespace

std::uint64_t mesh_hash(const Mesh& m) noexcept {
    std::uint64_t h = 0x51ED270B27u;
    h = hash_bytes(h, m.positions);
    h = hash_bytes(h, m.indices);
    h = hash_bytes(h, m.normals);
    h = hash_bytes(h, m.uvs);
    h = hash_bytes(h, m.tangents);
    h = hash_bytes(h, m.colors);
    h = hash_bytes(h, m.lightmap_uvs);
    return h;
}

std::shared_ptr<const Blas> build_blas(const Mesh& m, std::uint64_t version) {
    auto b = std::make_shared<Blas>();
    const std::uint32_t flags = mesh_flags(m) | tri_casts_shadows | tri_visible;
    const bool tan = m.has_uvs() && m.has_tangents();
    const bool col = m.has_colors();
    const Transform id;
    b->tris.reserve(m.indices.size() / 3);
    for (std::size_t t = 0; t + 2 < m.indices.size(); t += 3) {
        Tri tri;
        if (!fill_tri(m, id, t, tri)) continue;
        if (tan) {
            tri.tangents = static_cast<std::uint32_t>(b->tangents.size());
            b->tangents.push_back(tangent_frame(id, m, m.indices[t], m.indices[t + 1], m.indices[t + 2]));
        }
        if (col) {
            tri.colors = static_cast<std::uint32_t>(b->colors.size());
            b->colors.push_back(color_frame(m, m.indices[t], m.indices[t + 1], m.indices[t + 2]));
        }
        tri.local = static_cast<std::uint32_t>(t / 3);
        tri.flags = flags;
        b->tris.push_back(tri);
    }
    b->bvh.build(b->tris);
    b->lo = b->bvh.lo();
    b->hi = b->bvh.hi();
    b->index_count = m.indices.size();
    b->vertex_count = m.positions.size();
    b->version = version;
    b->hash = mesh_hash(m);
    b->index_hash = hash_bytes(0x1D3u, m.indices);
    return b;
}

std::shared_ptr<const Blas> refit_blas(const Blas& old, const Mesh& m, std::uint64_t version) {
    if (old.index_count != m.indices.size() || old.vertex_count != m.positions.size() || old.refits >= 120) return nullptr;
    if (old.index_hash != hash_bytes(0x1D3u, m.indices)) return nullptr;
    auto b = std::make_shared<Blas>(old);
    const Transform id;
    const bool tan = m.has_uvs() && m.has_tangents();
    const bool col = m.has_colors();
    for (Tri& tri : b->tris) {
        const std::size_t t = static_cast<std::size_t>(tri.local) * 3;
        if (t + 2 >= m.indices.size()) return nullptr;
        if (!fill_tri(m, id, t, tri)) return nullptr;
        if (tan && tri.tangents < b->tangents.size()) b->tangents[tri.tangents] = tangent_frame(id, m, m.indices[t], m.indices[t + 1], m.indices[t + 2]);
        if (col && tri.colors < b->colors.size()) b->colors[tri.colors] = color_frame(m, m.indices[t], m.indices[t + 1], m.indices[t + 2]);
    }
    b->bvh.refit(b->tris);
    b->lo = b->bvh.lo();
    b->hi = b->bvh.hi();
    b->version = version;
    b->hash = mesh_hash(m);
    ++b->refits;
    return b;
}

void build_tlas(Compiled& c, const Scene& scene) {
    const auto& instances = scene.instances();
    c.tlas.clear();
    c.instance_slot.assign(instances.size(), kNoInstance);
    std::vector<TlasInstance> raw;
    for (std::size_t ii = 0; ii < instances.size(); ++ii) {
        const Instance& inst = instances[ii];
        if (!inst.instanced || inst.mesh >= c.mesh_blas.size() || !c.mesh_blas[inst.mesh] || c.mesh_blas[inst.mesh]->tris.empty()) continue;
        TlasInstance ti;
        ti.to_world = inst.transform;
        ti.to_object = inverse_transform(inst.transform);
        box_of(*c.mesh_blas[inst.mesh], inst.transform, ti.lo, ti.hi);
        ti.instance = static_cast<std::uint32_t>(ii);
        ti.mesh = static_cast<std::uint32_t>(inst.mesh);
        ti.material = static_cast<std::uint32_t>(inst.material);
        ti.flags = instance_flags(inst);
        ti.channels = inst.channels;
        raw.push_back(ti);
    }
    std::vector<Vec3f> lo(raw.size()), hi(raw.size());
    for (std::size_t i = 0; i < raw.size(); ++i) {
        lo[i] = raw[i].lo;
        hi[i] = raw[i].hi;
    }
    std::vector<std::uint32_t> order;
    c.tlas_bvh.build_boxes(lo, hi, order);
    c.tlas.reserve(raw.size());
    for (std::uint32_t k : order) c.tlas.push_back(raw[k]);
    for (std::size_t k = 0; k < c.tlas.size(); ++k) c.instance_slot[c.tlas[k].instance] = static_cast<std::uint32_t>(k);
    c.tlas_refits = 0;
    c.tlas_built_area = c.tlas_bvh.empty() ? 0.0f : box_area(c.tlas_bvh.lo(), c.tlas_bvh.hi());
    c.tlas_epoch = next_epoch();
}

Compiled compile(const Scene& scene) {
    return compile(scene, nullptr);
}

Compiled compile(const Scene& scene, const Compiled* previous) {
    Compiled c;
    const auto& instances = scene.instances();
    const auto& meshes = scene.meshes();
    c.instance_channels.resize(instances.size(), 1u);
    c.instance_static.resize(instances.size(), 0);
    std::vector<std::uint8_t> used(meshes.size(), 0);
    for (std::size_t ii = 0; ii < instances.size(); ++ii) {
        const Instance& inst = instances[ii];
        c.instance_channels[ii] = inst.channels;
        c.instance_static[ii] = inst.is_static ? 1 : 0;
        if (inst.mesh >= meshes.size()) continue;
        if (inst.instanced) {
            used[inst.mesh] = 1;
            continue;
        }
        const Mesh& m = meshes[inst.mesh];
        const bool tan = m.has_uvs() && m.has_tangents();
        const bool col = m.has_colors();
        std::uint32_t flags = mesh_flags(m);
        if (inst.casts_shadows) flags |= tri_casts_shadows;
        if (inst.visible) flags |= tri_visible;
        if (inst.is_static) flags |= tri_static;
        for (std::size_t t = 0; t + 2 < m.indices.size(); t += 3) {
            Tri tri;
            if (!fill_tri(m, inst.transform, t, tri)) continue;
            if (tan) {
                tri.tangents = static_cast<std::uint32_t>(c.tangents.size());
                c.tangents.push_back(tangent_frame(inst.transform, m, m.indices[t], m.indices[t + 1], m.indices[t + 2]));
            }
            if (col) {
                tri.colors = static_cast<std::uint32_t>(c.colors.size());
                c.colors.push_back(color_frame(m, m.indices[t], m.indices[t + 1], m.indices[t + 2]));
            }
            tri.instance = static_cast<std::uint32_t>(ii);
            tri.material = static_cast<std::uint32_t>(inst.material);
            tri.local = static_cast<std::uint32_t>(t / 3);
            tri.flags = flags;
            c.tris.push_back(tri);
        }
    }
    c.bvh.build(c.tris);
    if (!c.tris.empty()) c.built_area = box_area(c.bvh.lo(), c.bvh.hi());
    c.mesh_blas.assign(meshes.size(), nullptr);
    c.blas_versions.assign(meshes.size(), 0);
    std::unordered_map<std::uint64_t, std::shared_ptr<const Blas>> by_hash;
    bool indexed = false;
    for (std::size_t mi = 0; mi < meshes.size(); ++mi) {
        if (!used[mi]) continue;
        const std::uint64_t version = scene.mesh_version(mi);
        std::shared_ptr<const Blas> b = find_blas(previous, meshes[mi], mi, version, by_hash, indexed);
        if (!b) {
            b = build_blas(meshes[mi], version);
            ++c.blas_built;
        }
        c.mesh_blas[mi] = std::move(b);
        c.blas_versions[mi] = version;
    }
    build_tlas(c, scene);
    set_bounds(c);
    c.flat_epoch = next_epoch();
    c.blas_epoch = next_epoch();
    compile_lights(c, scene);
    c.light_epoch = next_epoch();
    return c;
}

bool refit_moved(Compiled& c, const Scene& scene, const std::vector<std::uint32_t>& moved) {
    if (!refit_flat(c, scene, moved)) return false;
    set_bounds(c);
    c.flat_epoch = next_epoch();
    compile_lights(c, scene);
    c.light_epoch = next_epoch();
    return true;
}

bool update_compiled(Compiled& c, const Scene& scene, const std::vector<std::uint32_t>& moved, const std::vector<std::uint32_t>& reshaped) {
    const auto& instances = scene.instances();
    const auto& meshes = scene.meshes();
    if (c.mesh_blas.size() != meshes.size() || c.instance_slot.size() != instances.size()) return false;
    bool tlas_dirty = false;
    bool tlas_rebuild = false;
    for (std::uint32_t mi : reshaped) {
        if (mi >= meshes.size()) return false;
        bool flat_user = false, inst_user = false;
        for (const Instance& inst : instances) {
            if (inst.mesh != mi) continue;
            if (inst.instanced) inst_user = true;
            else flat_user = true;
        }
        if (flat_user) return false;
        if (!inst_user) continue;
        const Mesh& m = meshes[mi];
        const std::uint64_t version = scene.mesh_version(mi);
        const bool had = c.mesh_blas[mi] && !c.mesh_blas[mi]->tris.empty();
        std::shared_ptr<const Blas> b = c.mesh_blas[mi] ? refit_blas(*c.mesh_blas[mi], m, version) : nullptr;
        if (b) {
            ++c.blas_refit;
        } else {
            b = build_blas(m, version);
            ++c.blas_built;
        }
        const bool has = !b->tris.empty();
        c.mesh_blas[mi] = std::move(b);
        c.blas_versions[mi] = version;
        c.blas_epoch = next_epoch();
        if (had != has) {
            tlas_rebuild = true;
            continue;
        }
        for (TlasInstance& ti : c.tlas) {
            if (ti.mesh != mi) continue;
            box_of(*c.mesh_blas[mi], ti.to_world, ti.lo, ti.hi);
            tlas_dirty = true;
        }
    }
    std::vector<std::uint32_t> flat_moved;
    for (std::uint32_t ii : moved) {
        if (ii >= instances.size()) return false;
        const Instance& inst = instances[ii];
        const std::uint32_t slot = c.instance_slot[ii];
        if (slot != kNoInstance && slot < c.tlas.size()) {
            TlasInstance& ti = c.tlas[slot];
            ti.to_world = inst.transform;
            ti.to_object = inverse_transform(inst.transform);
            if (ti.mesh < c.mesh_blas.size() && c.mesh_blas[ti.mesh]) box_of(*c.mesh_blas[ti.mesh], inst.transform, ti.lo, ti.hi);
            tlas_dirty = true;
        } else if (!inst.instanced) {
            flat_moved.push_back(ii);
        }
    }
    const bool relight = !flat_moved.empty();
    if (relight) {
        if (!refit_flat(c, scene, flat_moved)) return false;
        c.flat_epoch = next_epoch();
    }
    if (tlas_rebuild || (tlas_dirty && c.tlas_refits >= 240)) {
        build_tlas(c, scene);
    } else if (tlas_dirty) {
        std::vector<Vec3f> lo(c.tlas.size()), hi(c.tlas.size());
        for (std::size_t k = 0; k < c.tlas.size(); ++k) {
            lo[k] = c.tlas[k].lo;
            hi[k] = c.tlas[k].hi;
        }
        c.tlas_bvh.refit_boxes(lo, hi);
        ++c.tlas_refits;
        if (c.tlas_built_area > 0.0f && box_area(c.tlas_bvh.lo(), c.tlas_bvh.hi()) > 2.0f * c.tlas_built_area) build_tlas(c, scene);
        else c.tlas_epoch = next_epoch();
    }
    set_bounds(c);
    if (relight) {
        compile_lights(c, scene);
        c.light_epoch = next_epoch();
    }
    return true;
}

} // namespace detail

} // namespace lighting
} // namespace fizmo
