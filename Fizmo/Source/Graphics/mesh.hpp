#ifndef FIZMO_MESH_3D_HPP
#define FIZMO_MESH_3D_HPP

#include <cstddef>
#include <cstdint>
#include <vector>
#include "color.hpp"
#include "content_version.hpp"
#include "texture.hpp"
#include "../Vectors/vectors.hpp"

#ifdef OS_LINUX
#include "../x11_compat.hpp"
#endif

namespace fizmo {
namespace graphics {

struct Vertex3D {
    float x = 0.0f, y = 0.0f, z = 0.0f;
    float nx = 0.0f, ny = 0.0f, nz = 0.0f;
    float u = 0.0f, v = 0.0f;
    std::uint32_t rgba = 0xFFFFFFFFu;  

    constexpr Vertex3D() noexcept = default;

    Vertex3D(const vector3d& p, const Color& c) noexcept
        : x(static_cast<float>(p.x)), y(static_cast<float>(p.y)), z(static_cast<float>(p.z)), rgba(pack(c)) {}

    Vertex3D(const vector3d& p, const vector3d& n, const Color& c, float tu = 0.0f, float tv = 0.0f) noexcept
        : x(static_cast<float>(p.x)), y(static_cast<float>(p.y)), z(static_cast<float>(p.z)),
          nx(static_cast<float>(n.x)), ny(static_cast<float>(n.y)), nz(static_cast<float>(n.z)),
          u(tu), v(tv), rgba(pack(c)) {}

    Color color() const noexcept {
        return Color(static_cast<std::uint8_t>(rgba & 0xFF), static_cast<std::uint8_t>((rgba >> 8) & 0xFF),
                     static_cast<std::uint8_t>((rgba >> 16) & 0xFF), static_cast<std::uint8_t>(rgba >> 24));
    }

    void set_color(const Color& c) noexcept { rgba = pack(c); }
    vector3d position() const noexcept { return { double(x), double(y), double(z) }; }
    vector3d normal()   const noexcept { return { double(nx), double(ny), double(nz) }; }

    static constexpr std::uint32_t pack(const Color& c) noexcept {
        return static_cast<std::uint32_t>(c.red()) | (static_cast<std::uint32_t>(c.green()) << 8)
             | (static_cast<std::uint32_t>(c.blue()) << 16) | (static_cast<std::uint32_t>(c.alpha()) << 24);
    }
};

static_assert(sizeof(Vertex3D) == 36, "Vertex3D layout is shared with the GPU pipeline");

enum class Cull3D  : std::uint8_t { Back = 0, None, Front };   
enum class Blend3D : std::uint8_t { Opaque = 0, Alpha, Additive };

struct Material3D {
    const Texture* texture     = nullptr;  
    Cull3D         cull        = Cull3D::Back;
    Blend3D        blend       = Blend3D::Opaque;
    bool           depth_test  = true;
    bool           depth_write = true;     

    static Material3D opaque() noexcept { return {}; }
    static Material3D double_sided() noexcept { Material3D m; m.cull = Cull3D::None; return m; }
    static Material3D transparent() noexcept { Material3D m; m.blend = Blend3D::Alpha; m.depth_write = false; m.cull = Cull3D::None; return m; }
};

struct Light3D {
    vector3d direction{ -0.35, -1.0, -0.55 };  
    float    ambient = 1.0f;
    float    diffuse = 0.0f;

    static Light3D none() noexcept { return {}; }
    static Light3D sun(const vector3d& dir, float ambient_level = 0.45f, float diffuse_level = 0.55f) noexcept {
        Light3D l; l.direction = dir; l.ambient = ambient_level; l.diffuse = diffuse_level; return l;
    }
};

class Mesh3D {
public:
    Mesh3D() = default;

    std::size_t add_vertex(const Vertex3D& v) { m_vertices.push_back(v); m_version.touch(); return m_vertices.size() - 1; }

    void add_triangle(std::uint32_t a, std::uint32_t b, std::uint32_t c) {
        m_indices.push_back(a); m_indices.push_back(b); m_indices.push_back(c);
        m_version.touch();
    }

    void add_quad(const Vertex3D& a, const Vertex3D& b, const Vertex3D& c, const Vertex3D& d) {
        const std::uint32_t base = static_cast<std::uint32_t>(m_vertices.size());
        m_vertices.push_back(a); m_vertices.push_back(b); m_vertices.push_back(c); m_vertices.push_back(d);
        const std::uint32_t idx[6] = { base, base + 1, base + 2, base, base + 2, base + 3 };
        m_indices.insert(m_indices.end(), idx, idx + 6);
        m_version.touch();
    }

    void clear() noexcept { m_vertices.clear(); m_indices.clear(); m_version.touch(); }
    void reserve(std::size_t vertices, std::size_t indices) { m_vertices.reserve(vertices); m_indices.reserve(indices); }

    const std::vector<Vertex3D>&      vertices() const noexcept { return m_vertices; }
    const std::vector<std::uint32_t>& indices()  const noexcept { return m_indices; }

    std::vector<Vertex3D>&      edit_vertices() noexcept { m_version.touch(); return m_vertices; }
    std::vector<std::uint32_t>& edit_indices()  noexcept { m_version.touch(); return m_indices; }
    void touch() noexcept { m_version.touch(); }

    bool        empty()          const noexcept { return m_vertices.empty(); }
    bool        indexed()        const noexcept { return !m_indices.empty(); }
    std::size_t triangle_count() const noexcept { return (indexed() ? m_indices.size() : m_vertices.size()) / 3; }
    std::uint64_t version()      const noexcept { return m_version.get(); }

private:
    std::vector<Vertex3D>      m_vertices;
    std::vector<std::uint32_t> m_indices;
    ContentVersion             m_version;
};

} // namespace graphics
} // namespace fizmo

#endif // FIZMO_MESH_3D_HPP