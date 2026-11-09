#ifndef FIZMO_LIGHTING_MESH_HPP
#define FIZMO_LIGHTING_MESH_HPP

#include "math.hpp"

#include <cstdint>
#include <vector>

namespace fizmo {
namespace lighting {

struct Vec2f {
    float x = 0.0f;
    float y = 0.0f;
};

struct Vec4f {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    float w = 1.0f;
};

struct Mesh {
    std::vector<Vec3f> positions;
    std::vector<Vec3f> normals;
    std::vector<Vec2f> lightmap_uvs;
    std::vector<std::uint32_t> indices;
    std::vector<Vec2f> uvs;
    std::vector<Vec4f> tangents;
    std::vector<Vec4f> colors;

    std::size_t triangle_count() const noexcept { return indices.size() / 3; }
    bool has_normals() const noexcept { return normals.size() == positions.size(); }
    bool has_lightmap_uvs() const noexcept { return lightmap_uvs.size() == positions.size(); }
    bool has_uvs() const noexcept { return !positions.empty() && uvs.size() == positions.size(); }
    bool has_tangents() const noexcept { return !positions.empty() && tangents.size() == positions.size(); }
    bool has_colors() const noexcept { return !positions.empty() && colors.size() == positions.size(); }
    void compute_tangents();
    void compute_normals();
    void append(const Mesh& other, const Transform& xf);

    static Mesh box(const Vec3f& half_extents);
    static Mesh sphere(float radius, int segments = 24, int rings = 16);
    static Mesh plane(float half_width, float half_depth);
    static Mesh quad(const Vec3f& a, const Vec3f& b, const Vec3f& c, const Vec3f& d);
    static Mesh cylinder(float radius, float half_height, int segments = 24, bool capped = true);
    static Mesh capsule(float radius, float half_height, int segments = 24, int rings = 8);
    static Mesh prism(float radius, float half_height);
};

} // namespace lighting
} // namespace fizmo

#endif // FIZMO_LIGHTING_MESH_HPP
