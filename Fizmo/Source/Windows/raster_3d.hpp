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

 Mat4f mat4_mul(const Mat4f& a, const Mat4f& b) noexcept;

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
    void begin(const Scene3D& scene);

    void set_light(const graphics::Light3D& l) noexcept;

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

    void lines(const vector3d* pts, std::size_t count, const graphics::Color& color, float width, bool depth_test);

    bool touched() const noexcept { return m_touched; }

    void draw_quads(
        const graphics::CompactVertex3D* v, std::size_t quads, const float* model,
        const std::int32_t* cell_origin, const graphics::Material3D& mat
    );

    void light_at(const float world[3], const float normal[3], bool has_normal, std::uint32_t baked, float out[3]) const noexcept;

    static void cell_breaks(double from, double to, std::vector<double>& out);

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

    void prepare_lighting();

    static void transform_point(const float* model, const vector3d& p, float out[3]) noexcept;

    static void transform_normal(const float* model, const vector3d& n, float out[3]) noexcept;

    void corner_factor(const graphics::CompactVertex3D& v, const float* model, bool material_lit, float out[3]) const noexcept;

    void expand_quad(const graphics::CompactVertex3D* c, const std::int32_t* cell_origin, const float* model, bool material_lit);

    static int axis_of(const vector3d& e) noexcept;

    static double component(const vector3d& p, int axis) noexcept { return axis == 0 ? p.x : (axis == 1 ? p.y : p.z); }

public:

    void resolve(std::vector<graphics::Color>& out) const;

    const Scene3D& scene() const noexcept { return m_scene; }

private:
    struct ClipVertex { float p[4]; float c[4]; float u, v; };   
    struct Screen { float x, y, z, iw; };

    static std::uint8_t to8(float f) noexcept { return static_cast<std::uint8_t>(std::lround(std::min(std::max(f, 0.0f), 1.0f) * 255.0f)); }

    static Mat4f to_mat(const float* m) noexcept { Mat4f r; std::copy(m, m + 16, r.begin()); return r; }

    static void to_clip(const Mat4f& m, const vector3d& p, float out[4]) noexcept;

    void draw_vertices(
        const graphics::Vertex3D* v, std::size_t vcount,
        const std::uint32_t* indices, std::size_t icount,
        const float* model, const graphics::Material3D& mat,
        const std::uint32_t* vertex_lights, bool prelit
    );

    ClipVertex shade(const graphics::Vertex3D& v, const Mat4f& mvp, const float* model, bool lit, std::uint32_t baked) const noexcept;

    static ClipVertex lerp(const ClipVertex& a, const ClipVertex& b, float t) noexcept;

    static bool clip_segment(float a[4], float b[4]) noexcept;

    Screen to_screen(const float p[4]) const noexcept;

    void triangle(const ClipVertex in[3], const graphics::Material3D& mat);

    void raster(const ClipVertex& A, const ClipVertex& B, const ClipVertex& C, const graphics::Material3D& mat);

    void flat_triangle(const Screen& a, const Screen& b, const Screen& c, const float rgba[4], bool depth_test);

    void blend(std::size_t idx, const float c[4], graphics::Blend3D mode) noexcept;

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