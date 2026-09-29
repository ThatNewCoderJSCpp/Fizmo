#ifndef FIZMO_RASTER_3D_HPP
#define FIZMO_RASTER_3D_HPP

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <vector>
#include "../Graphics/mesh.hpp"

namespace fizmo {
namespace windows {
namespace detail {

using Mat4f = std::array<float, 16>;   

inline Mat4f mat4_mul(const Mat4f& a, const Mat4f& b) noexcept {
    Mat4f r{};
    for (int i = 0; i < 4; ++i)
        for (int j = 0; j < 4; ++j)
            r[i * 4 + j] = a[i * 4 + 0] * b[0 * 4 + j] + a[i * 4 + 1] * b[1 * 4 + j] + a[i * 4 + 2] * b[2 * 4 + j] + a[i * 4 + 3] * b[3 * 4 + j];
    return r;
}

inline Mat4f mat4_identity() noexcept { return { 1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1 }; }

struct Scene3D {
    Mat4f        view_proj = mat4_identity();  
    float        camera[3] = { 0.0f, 0.0f, 0.0f };
    double       origin[3] = { 0.0, 0.0, 0.0 };
    int          x = 0, y = 0;                
    unsigned int width = 0, height = 0;
};

class SoftwareRasterizer3D {
public:
    void begin(const Scene3D& scene) {
        m_scene = scene;
        prepare_lighting();
        m_w = static_cast<int>(scene.width);
        m_h = static_cast<int>(scene.height);
        const std::size_t n = static_cast<std::size_t>(std::max(m_w, 0)) * static_cast<std::size_t>(std::max(m_h, 0));
        m_color.assign(n * 4, 0.0f);
        m_depth.assign(n, 1.0f);
        m_touched = false;
    }

    void set_light(const graphics::Light3D& l) noexcept {
        const double len = l.direction.magnitude();
        m_light[0] = len > 0 ? static_cast<float>(l.direction.x / len) : 0.0f;
        m_light[1] = len > 0 ? static_cast<float>(l.direction.y / len) : -1.0f;
        m_light[2] = len > 0 ? static_cast<float>(l.direction.z / len) : 0.0f;
        m_ambient = l.ambient;
        m_diffuse = l.diffuse;
    }

    void set_scene_lighting(const graphics::SceneLighting3D* lighting) {
        m_lighting = lighting;
        prepare_lighting();
    }

    void draw(
        const graphics::Vertex3D* v, std::size_t vcount, 
        const std::uint32_t* indices, std::size_t icount,
        const float* model, const graphics::Material3D& mat,
        const std::uint32_t* vertex_lights = nullptr
    ) {
        draw_vertices(v, vcount, indices, icount, model, mat, vertex_lights, false);
    }

    void lines(const vector3d* pts, std::size_t count, const graphics::Color& color, float width, bool depth_test) {
        if (!pts || m_w <= 0 || m_h <= 0) return;
        const float a = color.alpha() / 255.0f;
        const float rgba[4] = { color.red() / 255.0f * a, color.green() / 255.0f * a, color.blue() / 255.0f * a, a };
        const float half = std::max(width, 1.0f) * 0.5f;

        for (std::size_t i = 0; i + 1 < count; i += 2) {
            float ca[4], cb[4];
            to_clip(m_scene.view_proj, pts[i], ca);
            to_clip(m_scene.view_proj, pts[i + 1], cb);
            if (!clip_segment(ca, cb)) continue;
            Screen sa = to_screen(ca), sb = to_screen(cb);
            float dx = sb.x - sa.x, dy = sb.y - sa.y;
            const float len = std::sqrt(dx * dx + dy * dy);
            if (len < 1e-6f) { dx = 1.0f; dy = 0.0f; } else { dx /= len; dy /= len; }
            const float nx = -dy * half, ny = dx * half, ex = dx * half, ey = dy * half;

            const Screen q[4] = {
                { sa.x - ex + nx, sa.y - ey + ny, sa.z, 1 }, { sa.x - ex - nx, sa.y - ey - ny, sa.z, 1 },
                { sb.x + ex - nx, sb.y + ey - ny, sb.z, 1 }, { sb.x + ex + nx, sb.y + ey + ny, sb.z, 1 },
            };

            flat_triangle(q[0], q[1], q[2], rgba, depth_test);
            flat_triangle(q[0], q[2], q[3], rgba, depth_test);
        }
    }

    bool touched() const noexcept { return m_touched; }

    void draw_quads(
        const graphics::CompactVertex3D* v, std::size_t quads, const float* model,
        const std::int32_t* cell_origin, const graphics::Material3D& mat
    ) {
        if (!v || quads == 0 || m_w <= 0 || m_h <= 0) return;
        m_quad_scratch.clear();

        for (std::size_t q = 0; q < quads; ++q) {
            const graphics::CompactVertex3D* c = v + q * graphics::QuadMesh3D::VERTICES_PER_QUAD;
            expand_quad(c, cell_origin, model, mat.lit);
        }

        draw_vertices(m_quad_scratch.data(), m_quad_scratch.size(), nullptr, 0, model, mat, nullptr, true);
    }

    void light_at(const float world[3], const float normal[3], bool has_normal, std::uint32_t baked, float out[3]) const noexcept {
        const graphics::BakedLight b = graphics::BakedLight::unpack(baked);
        const float sky = graphics::detail::light_curve(b.sky / 255.0f, m_soft.falloff);
        const float block[3] = {
            graphics::detail::light_curve(b.red / 255.0f, m_soft.falloff),
            graphics::detail::light_curve(b.green / 255.0f, m_soft.falloff),
            graphics::detail::light_curve(b.blue / 255.0f, m_soft.falloff)
        };
        float ndl_sun = has_normal ? std::max(0.0f, -(normal[0] * m_soft.sun_dir[0] + normal[1] * m_soft.sun_dir[1] + normal[2] * m_soft.sun_dir[2])) : 1.0f;
        const float exposure = m_soft.sun_on ? std::pow(b.sky / 255.0f, m_soft.exposure) : 0.0f;
        ndl_sun *= exposure;

        for (int c = 0; c < 3; ++c) {
            const float s = clamp01(sky * m_soft.sky[c]);
            const float k = clamp01(block[c] * m_soft.block[c]);
            out[c] = 1.0f - (1.0f - s) * (1.0f - k) + m_soft.ambient + m_soft.sun[c] * ndl_sun;
        }

        for (const SoftPointLight& p : m_soft.points) {
            const float d[3] = { world[0] - p.pos[0], world[1] - p.pos[1], world[2] - p.pos[2] };
            const float dist2 = d[0] * d[0] + d[1] * d[1] + d[2] * d[2];
            if (dist2 >= p.radius * p.radius) continue;
            const float dist = std::sqrt(dist2);
            const float fall = 1.0f - dist / p.radius;
            const float inv = 1.0f / std::max(dist, 1e-4f);
            const float ndl = has_normal ? std::max(0.0f, -(normal[0] * d[0] + normal[1] * d[1] + normal[2] * d[2]) * inv) : 1.0f;
            for (int c = 0; c < 3; ++c) out[c] += p.rgb[c] * fall * fall * ndl;
        }

        for (int c = 0; c < 3; ++c) out[c] = std::min(std::max(out[c], m_soft.min_light), m_soft.max_light);
    }

    static void cell_breaks(double from, double to, std::vector<double>& out) {
        out.clear();
        out.push_back(0.0);
        const double span = to - from;

        if (std::fabs(span) > 1e-9) {
            const double lo = std::min(from, to), hi = std::max(from, to);
            for (double k = std::floor(lo) + 1.0; k < hi - 1e-9; k += 1.0) out.push_back((k - from) / span);
        }

        out.push_back(1.0);
        std::sort(out.begin(), out.end());
    }

private:
    struct SoftPointLight {
        float pos[3];
        float radius;
        float rgb[3];
    };

    struct SoftLighting {
        bool  enabled  = false;
        bool  sun_on   = false;
        float sun_dir[3] = { 0.0f, 0.0f, -1.0f };
        float sun[3]   = { 0.0f, 0.0f, 0.0f };
        float sky[3]   = { 1.0f, 1.0f, 1.0f };
        float block[3] = { 1.0f, 1.0f, 1.0f };
        float exposure = 1.0f;
        float ambient  = 0.0f;
        float min_light = 0.0f;
        float max_light = 1.0f;
        float falloff  = 1.0f;
        std::vector<SoftPointLight> points;
    };

    static float clamp01(float v) noexcept { return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v); }

    void prepare_lighting() {
        m_soft.enabled = m_lighting && m_lighting->enabled;
        m_soft.points.clear();
        if (!m_soft.enabled) return;
        const graphics::SceneLighting3D& l = *m_lighting;
        const double len = l.sun_direction.magnitude();
        const vector3d d = len > 0.0 ? l.sun_direction / len : vector3d{ 0.0, 0.0, -1.0 };
        m_soft.sun_dir[0] = static_cast<float>(d.x); m_soft.sun_dir[1] = static_cast<float>(d.y); m_soft.sun_dir[2] = static_cast<float>(d.z);
        m_soft.sun_on = l.sun_intensity > 0.0f;

        for (int c = 0; c < 3; ++c) {
            m_soft.sun[c]   = graphics::detail::light_channel(l.sun_color, c) * l.sun_intensity;
            m_soft.sky[c]   = graphics::detail::light_channel(l.sky_color, c) * l.sky_intensity;
            m_soft.block[c] = graphics::detail::light_channel(l.block_color, c) * l.block_intensity;
        }

        m_soft.exposure  = l.sun_exposure;
        m_soft.ambient   = l.ambient;
        m_soft.min_light = l.min_light;
        m_soft.max_light = l.max_light;
        m_soft.falloff   = l.falloff;
        const std::size_t n = std::min(l.point_lights.size(), graphics::SceneLighting3D::MAX_POINT_LIGHTS);

        for (std::size_t i = 0; i < n; ++i) {
            const graphics::PointLight3D& p = l.point_lights[i];
            if (p.radius <= 0.0f || p.intensity <= 0.0f) continue;
            SoftPointLight s;
            s.pos[0] = static_cast<float>(p.position.x - m_scene.origin[0]);
            s.pos[1] = static_cast<float>(p.position.y - m_scene.origin[1]);
            s.pos[2] = static_cast<float>(p.position.z - m_scene.origin[2]);
            s.radius = p.radius;
            for (int c = 0; c < 3; ++c) s.rgb[c] = graphics::detail::light_channel(p.color, c) * p.intensity;
            m_soft.points.push_back(s);
        }
    }

    static void transform_point(const float* model, const vector3d& p, float out[3]) noexcept {
        const float x = static_cast<float>(p.x), y = static_cast<float>(p.y), z = static_cast<float>(p.z);
        if (!model) { out[0] = x; out[1] = y; out[2] = z; return; }
        for (int r = 0; r < 3; ++r) out[r] = model[r * 4 + 0] * x + model[r * 4 + 1] * y + model[r * 4 + 2] * z + model[r * 4 + 3];
    }

    static void transform_normal(const float* model, const vector3d& n, float out[3]) noexcept {
        const float x = static_cast<float>(n.x), y = static_cast<float>(n.y), z = static_cast<float>(n.z);
        if (!model) { out[0] = x; out[1] = y; out[2] = z; }
        else for (int r = 0; r < 3; ++r) out[r] = model[r * 4 + 0] * x + model[r * 4 + 1] * y + model[r * 4 + 2] * z;
        const float len = std::sqrt(out[0] * out[0] + out[1] * out[1] + out[2] * out[2]);
        if (len > 1e-6f) { out[0] /= len; out[1] /= len; out[2] /= len; }
    }

    void corner_factor(const graphics::CompactVertex3D& v, const float* model, bool material_lit, float out[3]) const noexcept {
        const float s = v.shade / 255.0f;
        out[0] = out[1] = out[2] = s;
        if (!material_lit) return;
        float n[3];
        transform_normal(model, v.normal(), n);
        const bool has_normal = v.face != graphics::CellFace::None;

        if (!m_soft.enabled) {
            if (!has_normal || (v.flags & graphics::CompactLit) == 0) return;
            const float k = m_ambient + m_diffuse * std::max(0.0f, -(n[0] * m_light[0] + n[1] * m_light[1] + n[2] * m_light[2]));
            for (int c = 0; c < 3; ++c) out[c] *= k;
            return;
        }

        float w[3], l[3];
        transform_point(model, v.position(), w);
        light_at(w, n, has_normal, v.light, l);
        for (int c = 0; c < 3; ++c) out[c] *= l[c];
    }

    void expand_quad(const graphics::CompactVertex3D* c, const std::int32_t* cell_origin, const float* model, bool material_lit) {
        const vector3d a = c[0].position(), b = c[1].position(), d = c[3].position();
        const vector3d n = c[0].normal();
        const bool tint = c[0].variation > 0 && c[0].face != graphics::CellFace::None;
        const graphics::Color base = c[0].color();
        float corner[4][3];
        for (int k = 0; k < 4; ++k) corner_factor(c[k], model, material_lit, corner[k]);

        auto factor = [&](double s, double t, int ch) {
            const double top = corner[0][ch] + (corner[1][ch] - corner[0][ch]) * s;
            const double bottom = corner[3][ch] + (corner[2][ch] - corner[3][ch]) * s;
            return top + (bottom - top) * t;
        };

        auto lit_color = [&](const graphics::Color& col, double s, double t) {
            auto ch = [&](std::uint8_t v, int k) {
                const double r = v * factor(s, t, k);
                return static_cast<std::uint8_t>(r < 0.0 ? 0.0 : (r > 255.0 ? 255.0 : r + 0.5));
            };
            return graphics::Color(ch(col.red(), 0), ch(col.green(), 1), ch(col.blue(), 2), col.alpha());
        };

        auto emit = [&](double s0, double s1, double t0, double t1) {
            const vector3d p00 = a + (b - a) * s0 + (d - a) * t0;
            const vector3d p10 = a + (b - a) * s1 + (d - a) * t0;
            const vector3d p11 = a + (b - a) * s1 + (d - a) * t1;
            const vector3d p01 = a + (b - a) * s0 + (d - a) * t1;
            int delta = 0;

            if (tint) {
                const vector3d mid = (p00 + p11) * 0.5 - n * 0.5;
                const std::int32_t o[3] = { cell_origin ? cell_origin[0] : 0, cell_origin ? cell_origin[1] : 0, cell_origin ? cell_origin[2] : 0 };
                delta = graphics::cell_variation(
                    static_cast<std::int32_t>(std::floor(mid.x)) + o[0],
                    static_cast<std::int32_t>(std::floor(mid.y)) + o[1],
                    static_cast<std::int32_t>(std::floor(mid.z)) + o[2], c[0].variation);
            }

            const graphics::Color col = graphics::cell_tinted(base, delta, 255);
            const vector3d vn{ 0.0, 0.0, 0.0 };
            const graphics::Vertex3D w0(p00, vn, lit_color(col, s0, t0)), w1(p10, vn, lit_color(col, s1, t0));
            const graphics::Vertex3D w2(p11, vn, lit_color(col, s1, t1)), w3(p01, vn, lit_color(col, s0, t1));
            m_quad_scratch.push_back(w0); m_quad_scratch.push_back(w1); m_quad_scratch.push_back(w2);
            m_quad_scratch.push_back(w0); m_quad_scratch.push_back(w2); m_quad_scratch.push_back(w3);
        };

        if (!tint) { emit(0.0, 1.0, 0.0, 1.0); return; }
        const int su = axis_of(b - a), sv = axis_of(d - a);
        cell_breaks(component(a, su), component(b, su), m_breaks_s);
        cell_breaks(component(a, sv), component(d, sv), m_breaks_t);

        for (std::size_t j = 0; j + 1 < m_breaks_t.size(); ++j)
            for (std::size_t i = 0; i + 1 < m_breaks_s.size(); ++i)
                emit(m_breaks_s[i], m_breaks_s[i + 1], m_breaks_t[j], m_breaks_t[j + 1]);
    }

    static int axis_of(const vector3d& e) noexcept {
        const double ax = std::fabs(e.x), ay = std::fabs(e.y), az = std::fabs(e.z);
        return ax >= ay && ax >= az ? 0 : (ay >= az ? 1 : 2);
    }

    static double component(const vector3d& p, int axis) noexcept { return axis == 0 ? p.x : (axis == 1 ? p.y : p.z); }

public:

    void resolve(std::vector<graphics::Color>& out) const {
        const std::size_t n = m_depth.size();
        out.resize(n);

        for (std::size_t i = 0; i < n; ++i) {
            const float* c = &m_color[i * 4];
            const float a = c[3];
            if (a <= 0.0f) { out[i] = graphics::Color(0, 0, 0, 0); continue; }
            const float inv = 1.0f / a;
            out[i] = graphics::Color(to8(c[0] * inv), to8(c[1] * inv), to8(c[2] * inv), to8(a));
        }
    }

    const Scene3D& scene() const noexcept { return m_scene; }

private:
    struct ClipVertex { float p[4]; float c[4]; float u, v; };   
    struct Screen { float x, y, z, iw; };

    static std::uint8_t to8(float f) noexcept { return static_cast<std::uint8_t>(std::lround(std::min(std::max(f, 0.0f), 1.0f) * 255.0f)); }

    static Mat4f to_mat(const float* m) noexcept { Mat4f r; std::copy(m, m + 16, r.begin()); return r; }

    static void to_clip(const Mat4f& m, const vector3d& p, float out[4]) noexcept {
        const float x = static_cast<float>(p.x), y = static_cast<float>(p.y), z = static_cast<float>(p.z);
        for (int r = 0; r < 4; ++r) out[r] = m[r * 4 + 0] * x + m[r * 4 + 1] * y + m[r * 4 + 2] * z + m[r * 4 + 3];
    }

    void draw_vertices(
        const graphics::Vertex3D* v, std::size_t vcount,
        const std::uint32_t* indices, std::size_t icount,
        const float* model, const graphics::Material3D& mat,
        const std::uint32_t* vertex_lights, bool prelit
    ) {
        if (!v || m_w <= 0 || m_h <= 0) return;
        const Mat4f mvp = model ? mat4_mul(m_scene.view_proj, to_mat(model)) : m_scene.view_proj;
        const std::size_t count = indices ? icount : vcount;
        const bool lit = mat.lit && !prelit;

        for (std::size_t t = 0; t + 2 < count; t += 3) {
            ClipVertex cv[3];
            bool ok = true;

            for (int k = 0; k < 3; ++k) {
                const std::size_t i = indices ? indices[t + static_cast<std::size_t>(k)] : t + static_cast<std::size_t>(k);
                if (i >= vcount) { ok = false; break; }
                cv[k] = shade(v[i], mvp, model, lit, vertex_lights ? vertex_lights[i] : mat.light);
            }

            if (ok) triangle(cv, mat);
        }
    }

    ClipVertex shade(const graphics::Vertex3D& v, const Mat4f& mvp, const float* model, bool lit, std::uint32_t baked) const noexcept {
        ClipVertex o;
        for (int r = 0; r < 4; ++r) o.p[r] = mvp[r * 4 + 0] * v.x + mvp[r * 4 + 1] * v.y + mvp[r * 4 + 2] * v.z + mvp[r * 4 + 3];
        float nx = v.nx, ny = v.ny, nz = v.nz;

        if (model) {
            const float tx = model[0] * nx + model[1] * ny + model[2]  * nz;
            const float ty = model[4] * nx + model[5] * ny + model[6]  * nz;
            const float tz = model[8] * nx + model[9] * ny + model[10] * nz;
            nx = tx; ny = ty; nz = tz;
        }

        float s[3] = { 1.0f, 1.0f, 1.0f };
        const float len = std::sqrt(nx * nx + ny * ny + nz * nz);

        if (lit && m_soft.enabled) {
            const float n[3] = { len > 1e-6f ? nx / len : 0.0f, len > 1e-6f ? ny / len : 0.0f, len > 1e-6f ? nz / len : 0.0f };
            float w[3];
            transform_point(model, v.position(), w);
            light_at(w, n, len > 1e-6f, baked, s);
        } else if (lit && len > 1e-6f) {
            s[0] = s[1] = s[2] = m_ambient + m_diffuse * std::max(0.0f, -(nx * m_light[0] + ny * m_light[1] + nz * m_light[2]) / len);
        }

        const float a = ((v.rgba >> 24) & 0xFF) / 255.0f;
        o.c[0] = (v.rgba & 0xFF) / 255.0f * s[0] * a;
        o.c[1] = ((v.rgba >> 8) & 0xFF) / 255.0f * s[1] * a;
        o.c[2] = ((v.rgba >> 16) & 0xFF) / 255.0f * s[2] * a;
        o.c[3] = a;
        o.u = v.u; o.v = v.v;
        return o;
    }

    static ClipVertex lerp(const ClipVertex& a, const ClipVertex& b, float t) noexcept {
        ClipVertex o;
        for (int i = 0; i < 4; ++i) { o.p[i] = a.p[i] + (b.p[i] - a.p[i]) * t; o.c[i] = a.c[i] + (b.c[i] - a.c[i]) * t; }
        o.u = a.u + (b.u - a.u) * t; o.v = a.v + (b.v - a.v) * t;
        return o;
    }

    static bool clip_segment(float a[4], float b[4]) noexcept {
        const bool ai = a[2] >= 0.0f, bi = b[2] >= 0.0f;
        if (!ai && !bi) return false;
        if (ai && bi) return true;
        const float t = a[2] / (a[2] - b[2]);
        float* out = ai ? b : a;   
        float tmp[4];
        for (int i = 0; i < 4; ++i) tmp[i] = a[i] + (b[i] - a[i]) * t;
        for (int i = 0; i < 4; ++i) out[i] = tmp[i];
        return true;
    }

    Screen to_screen(const float p[4]) const noexcept {
        const float iw = 1.0f / p[3];
        return { (p[0] * iw * 0.5f + 0.5f) * static_cast<float>(m_w), (0.5f - p[1] * iw * 0.5f) * static_cast<float>(m_h), p[2] * iw, iw };
    }

    void triangle(const ClipVertex in[3], const graphics::Material3D& mat) {
        ClipVertex poly[4];
        int n = 0;

        for (int i = 0; i < 3; ++i) {
            const ClipVertex& a = in[i];
            const ClipVertex& b = in[(i + 1) % 3];
            const bool ai = a.p[2] >= 0.0f && a.p[3] > 1e-6f, bi = b.p[2] >= 0.0f && b.p[3] > 1e-6f;
            if (ai) poly[n++] = a;

            if (ai != bi && n < 4) {
                const float t = a.p[2] / (a.p[2] - b.p[2]);
                if (t >= 0.0f && t <= 1.0f) poly[n++] = lerp(a, b, t);
            }
        }

        for (int i = 1; i + 1 < n; ++i) raster(poly[0], poly[i], poly[i + 1], mat);
    }

    void raster(const ClipVertex& A, const ClipVertex& B, const ClipVertex& C, const graphics::Material3D& mat) {
        const Screen s[3] = { to_screen(A.p), to_screen(B.p), to_screen(C.p) };
        const float area = (s[1].x - s[0].x) * (s[2].y - s[0].y) - (s[2].x - s[0].x) * (s[1].y - s[0].y);
        if (std::fabs(area) < 1e-9f) return;
        const bool front = area < 0.0f;   
        if ((mat.cull == graphics::Cull3D::Back && !front) || (mat.cull == graphics::Cull3D::Front && front)) return;
        const int x0 = std::max(0, static_cast<int>(std::floor(std::min({ s[0].x, s[1].x, s[2].x }))));
        const int x1 = std::min(m_w - 1, static_cast<int>(std::ceil(std::max({ s[0].x, s[1].x, s[2].x }))));
        const int y0 = std::max(0, static_cast<int>(std::floor(std::min({ s[0].y, s[1].y, s[2].y }))));
        const int y1 = std::min(m_h - 1, static_cast<int>(std::ceil(std::max({ s[0].y, s[1].y, s[2].y }))));
        if (x0 > x1 || y0 > y1) return;
        const ClipVertex* v[3] = { &A, &B, &C };
        const float inv_area = 1.0f / area;
        const graphics::Texture* tex = (mat.texture && mat.texture->valid()) ? mat.texture : nullptr;
        const double tw = tex ? tex->width() : 0.0, th = tex ? tex->height() : 0.0;
        const bool depth_write = mat.depth_write;

        for (int py = y0; py <= y1; ++py) {
            const float fy = py + 0.5f;

            for (int px = x0; px <= x1; ++px) {
                const float fx = px + 0.5f;
                const float b0 = ((s[1].x - fx) * (s[2].y - fy) - (s[2].x - fx) * (s[1].y - fy)) * inv_area;
                const float b1 = ((s[2].x - fx) * (s[0].y - fy) - (s[0].x - fx) * (s[2].y - fy)) * inv_area;
                const float b2 = 1.0f - b0 - b1;
                if (b0 < 0.0f || b1 < 0.0f || b2 < 0.0f) continue;
                const float z = b0 * s[0].z + b1 * s[1].z + b2 * s[2].z;
                if (z < 0.0f || z > 1.0f) continue;
                const std::size_t idx = static_cast<std::size_t>(py) * static_cast<std::size_t>(m_w) + static_cast<std::size_t>(px);
                if (mat.depth_test && z >= m_depth[idx]) continue;
                const float w0 = b0 * s[0].iw, w1 = b1 * s[1].iw, w2 = b2 * s[2].iw;
                const float iw = 1.0f / (w0 + w1 + w2);
                float c[4];
                for (int k = 0; k < 4; ++k) c[k] = (w0 * v[0]->c[k] + w1 * v[1]->c[k] + w2 * v[2]->c[k]) * iw;

                if (tex) {
                    const float u = (w0 * v[0]->u + w1 * v[1]->u + w2 * v[2]->u) * iw;
                    const float vv = (w0 * v[0]->v + w1 * v[1]->v + w2 * v[2]->v) * iw;
                    const graphics::Color t = tex->sample(u * tw, vv * th);
                    const float ta = t.alpha() / 255.0f;
                    c[0] *= t.red() / 255.0f * ta; c[1] *= t.green() / 255.0f * ta; c[2] *= t.blue() / 255.0f * ta; c[3] *= ta;
                }

                if (mat.blend == graphics::Blend3D::Opaque && c[3] > 0.0f) { const float k = 1.0f / c[3]; c[0] *= k; c[1] *= k; c[2] *= k; c[3] = 1.0f; }
                blend(idx, c, mat.blend);
                if (depth_write) m_depth[idx] = z;
            }
        }

        m_touched = true;
    }

    void flat_triangle(const Screen& a, const Screen& b, const Screen& c, const float rgba[4], bool depth_test) {
        const Screen s[3] = { a, b, c };
        const float area = (s[1].x - s[0].x) * (s[2].y - s[0].y) - (s[2].x - s[0].x) * (s[1].y - s[0].y);
        if (std::fabs(area) < 1e-9f) return;
        const float inv_area = 1.0f / area;
        const int x0 = std::max(0, static_cast<int>(std::floor(std::min({ a.x, b.x, c.x }))));
        const int x1 = std::min(m_w - 1, static_cast<int>(std::ceil(std::max({ a.x, b.x, c.x }))));
        const int y0 = std::max(0, static_cast<int>(std::floor(std::min({ a.y, b.y, c.y }))));
        const int y1 = std::min(m_h - 1, static_cast<int>(std::ceil(std::max({ a.y, b.y, c.y }))));

        for (int py = y0; py <= y1; ++py) {
            const float fy = py + 0.5f;

            for (int px = x0; px <= x1; ++px) {
                const float fx = px + 0.5f;
                const float b0 = ((s[1].x - fx) * (s[2].y - fy) - (s[2].x - fx) * (s[1].y - fy)) * inv_area;
                const float b1 = ((s[2].x - fx) * (s[0].y - fy) - (s[0].x - fx) * (s[2].y - fy)) * inv_area;
                const float b2 = 1.0f - b0 - b1;
                if (b0 < 0.0f || b1 < 0.0f || b2 < 0.0f) continue;
                const float z = b0 * a.z + b1 * b.z + b2 * c.z - 2e-5f;   // small bias so lines on surfaces win
                const std::size_t idx = static_cast<std::size_t>(py) * static_cast<std::size_t>(m_w) + static_cast<std::size_t>(px);
                if (depth_test && z >= m_depth[idx]) continue;
                float col[4] = { rgba[0], rgba[1], rgba[2], rgba[3] };
                blend(idx, col, graphics::Blend3D::Alpha);
            }
        }

        m_touched = true;
    }

    void blend(std::size_t idx, const float c[4], graphics::Blend3D mode) noexcept {
        float* d = &m_color[idx * 4];
        switch (mode) {
            case graphics::Blend3D::Opaque:
                d[0] = c[0]; d[1] = c[1]; d[2] = c[2]; d[3] = c[3];
                break;
            case graphics::Blend3D::Alpha: {
                const float k = 1.0f - c[3];
                for (int i = 0; i < 4; ++i) d[i] = c[i] + d[i] * k;
                break;
            }
            case graphics::Blend3D::Additive:
                for (int i = 0; i < 3; ++i) d[i] = std::min(1.0f, d[i] + c[i]);
                d[3] = std::min(1.0f, d[3] + c[3]);
                break;
        }
    }

    Scene3D            m_scene;
    int                m_w = 0, m_h = 0;
    std::vector<float> m_color;   
    std::vector<float> m_depth;
    std::vector<graphics::Vertex3D> m_quad_scratch;
    std::vector<double> m_breaks_s, m_breaks_t;
    float              m_light[3] = { 0.0f, -1.0f, 0.0f };
    float              m_ambient = 1.0f, m_diffuse = 0.0f;
    bool               m_touched = false;
    const graphics::SceneLighting3D* m_lighting = nullptr;
    SoftLighting       m_soft;
};

} // namespace detail
} // namespace windows
} // namespace fizmo

#endif // FIZMO_RASTER_3D_HPP