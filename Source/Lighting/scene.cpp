#include "scene_internal.hpp"

#include <cctype>

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
    m_compiled.reset();
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
    m_compiled.reset();
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
    if (!m_compiled || m_dirty) {
        auto c = std::make_shared<detail::Compiled>(detail::compile(*this));
        m_compiled = std::move(c);
        m_dirty = false;
        m_lights_dirty = false;
        clear_moved();
        ++m_geometry_version;
        ++m_lighting_version;
        return;
    }
    if (!m_moved.empty()) {
        std::shared_ptr<detail::Compiled> c = m_compiled.use_count() == 1 ? m_compiled : std::make_shared<detail::Compiled>(*m_compiled);
        const bool ok = detail::refit_moved(*c, *this, m_moved);
        clear_moved();
        if (!ok) c = std::make_shared<detail::Compiled>(detail::compile(*this));
        else ++m_refit_count;
        m_compiled = std::move(c);
        m_lights_dirty = false;
        ++m_geometry_version;
        ++m_lighting_version;
        return;
    }
    if (m_lights_dirty) {
        auto c = std::make_shared<detail::Compiled>(*m_compiled);
        detail::compile_lights(*c, *this);
        m_compiled = std::move(c);
        m_lights_dirty = false;
        ++m_lighting_version;
    }
}

bool Scene::committed() const noexcept {
    return m_compiled && !m_dirty && !m_lights_dirty;
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
    const detail::Tri& tri = m_compiled->tris[h.tri];
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
    return c.bvh.intersect(c.tris, ray, hit, require);
}

bool occluded(const Compiled& c, const Ray& ray) {
    return c.bvh.any(c.tris, ray, tri_casts_shadows);
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

} // namespace

Compiled compile(const Scene& scene) {
    Compiled c;
    const auto& instances = scene.instances();
    const auto& meshes = scene.meshes();
    c.instance_channels.resize(instances.size(), 1u);
    c.instance_static.resize(instances.size(), 0);
    for (std::size_t ii = 0; ii < instances.size(); ++ii) {
        const Instance& inst = instances[ii];
        c.instance_channels[ii] = inst.channels;
        c.instance_static[ii] = inst.is_static ? 1 : 0;
        if (inst.mesh >= meshes.size()) continue;
        const Mesh& m = meshes[inst.mesh];
        const bool smooth = m.has_normals();
        const bool lm = m.has_lightmap_uvs();
        const bool tex = m.has_uvs();
        const bool tan = tex && m.has_tangents();
        std::uint32_t flags = 0;
        if (inst.casts_shadows) flags |= tri_casts_shadows;
        if (inst.visible) flags |= tri_visible;
        if (inst.is_static) flags |= tri_static;
        if (smooth) flags |= tri_smooth;
        if (lm) flags |= tri_lightmap;
        if (tex) flags |= tri_texcoords;
        for (std::size_t t = 0; t + 2 < m.indices.size(); t += 3) {
            const std::uint32_t i0 = m.indices[t], i1 = m.indices[t + 1], i2 = m.indices[t + 2];
            if (i0 >= m.positions.size() || i1 >= m.positions.size() || i2 >= m.positions.size()) continue;
            Tri tri;
            const Vec3f a = inst.transform.point(m.positions[i0]);
            const Vec3f b = inst.transform.point(m.positions[i1]);
            const Vec3f cc = inst.transform.point(m.positions[i2]);
            tri.v0 = a;
            tri.e1 = b - a;
            tri.e2 = cc - a;
            const Vec3f gn = normalize(cross(tri.e1, tri.e2));
            if (smooth) {
                tri.n0 = inst.transform.normal(m.normals[i0]);
                tri.n1 = inst.transform.normal(m.normals[i1]);
                tri.n2 = inst.transform.normal(m.normals[i2]);
            } else {
                tri.n0 = tri.n1 = tri.n2 = gn;
            }
            if (lm) {
                tri.uv0 = m.lightmap_uvs[i0];
                tri.uv1 = m.lightmap_uvs[i1];
                tri.uv2 = m.lightmap_uvs[i2];
            }
            if (tex) {
                tri.tex0 = m.uvs[i0];
                tri.tex1 = m.uvs[i1];
                tri.tex2 = m.uvs[i2];
            }
            if (tan) {
                tri.tangents = static_cast<std::uint32_t>(c.tangents.size());
                c.tangents.push_back(tangent_frame(inst.transform, m, i0, i1, i2));
            }
            tri.instance = static_cast<std::uint32_t>(ii);
            tri.material = static_cast<std::uint32_t>(inst.material);
            tri.local = static_cast<std::uint32_t>(t / 3);
            tri.flags = flags;
            c.tris.push_back(tri);
        }
    }
    c.bvh.build(c.tris);
    c.lo = c.bvh.lo();
    c.hi = c.bvh.hi();
    if (c.tris.empty()) {
        c.lo = { -1.0f, -1.0f, -1.0f };
        c.hi = { 1.0f, 1.0f, 1.0f };
    }
    c.center = (c.lo + c.hi) * 0.5f;
    c.radius = std::max(length(c.hi - c.lo) * 0.5f, 1e-3f);
    const Vec3f size = c.hi - c.lo;
    c.built_area = 2.0f * (size.x * size.y + size.y * size.z + size.z * size.x);
    compile_lights(c, scene);
    return c;
}

bool refit_moved(Compiled& c, const Scene& scene, const std::vector<std::uint32_t>& moved) {
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
        const bool smooth = m.has_normals();
        for (std::uint32_t ti : c.instance_tris[ii]) {
            Tri& tri = c.tris[ti];
            const std::size_t t = static_cast<std::size_t>(tri.local) * 3;
            if (t + 2 >= m.indices.size()) return false;
            const std::uint32_t i0 = m.indices[t], i1 = m.indices[t + 1], i2 = m.indices[t + 2];
            if (i0 >= m.positions.size() || i1 >= m.positions.size() || i2 >= m.positions.size()) return false;
            const Vec3f a = inst.transform.point(m.positions[i0]);
            const Vec3f b = inst.transform.point(m.positions[i1]);
            const Vec3f cc = inst.transform.point(m.positions[i2]);
            tri.v0 = a;
            tri.e1 = b - a;
            tri.e2 = cc - a;
            if (smooth) {
                tri.n0 = inst.transform.normal(m.normals[i0]);
                tri.n1 = inst.transform.normal(m.normals[i1]);
                tri.n2 = inst.transform.normal(m.normals[i2]);
            } else {
                tri.n0 = tri.n1 = tri.n2 = normalize(cross(tri.e1, tri.e2));
            }
            if (tri.tangents < c.tangents.size() && m.has_tangents()) c.tangents[tri.tangents] = tangent_frame(inst.transform, m, i0, i1, i2);
        }
    }
    c.bvh.refit(c.tris);
    ++c.refits;
    c.lo = c.bvh.lo();
    c.hi = c.bvh.hi();
    c.center = (c.lo + c.hi) * 0.5f;
    c.radius = std::max(length(c.hi - c.lo) * 0.5f, 1e-3f);
    const Vec3f size = c.hi - c.lo;
    const float area = 2.0f * (size.x * size.y + size.y * size.z + size.z * size.x);
    if (c.built_area > 0.0f && area > 2.0f * c.built_area) return false;
    compile_lights(c, scene);
    return true;
}

} // namespace detail

} // namespace lighting
} // namespace fizmo
