#ifndef FIZMO_MESH_3D_HPP
#define FIZMO_MESH_3D_HPP

#include <cstddef>
#include <cstdint>
#include <memory>
#include <utility>
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

enum class CellFace : std::uint8_t { NegX = 0, PosX, NegY, PosY, NegZ, PosZ, None };

enum CompactVertexFlags : std::uint8_t {
    CompactLit = 1u << 0
};

inline int cell_variation(std::int32_t x, std::int32_t y, std::int32_t z, unsigned int amount) noexcept {
    if (amount == 0) return 0;
    std::uint32_t h = static_cast<std::uint32_t>(x) * 374761393u
                    + static_cast<std::uint32_t>(y) * 668265263u
                    + static_cast<std::uint32_t>(z) * 2147483647u;
    h = (h ^ (h >> 13)) * 1274126177u;
    h ^= h >> 16;
    return static_cast<int>(h % (2u * amount + 1u)) - static_cast<int>(amount);
}

inline Color cell_tinted(const Color& base, int delta, std::uint8_t shade) noexcept {
    auto ch = [delta, shade](std::uint8_t c) {
        int v = static_cast<int>(c) + delta;
        v = v < 0 ? 0 : (v > 255 ? 255 : v);
        return static_cast<std::uint8_t>((v * static_cast<int>(shade) + 127) / 255);
    };
    return Color(ch(base.red()), ch(base.green()), ch(base.blue()), base.alpha());
}

struct CompactVertex3D {
    float         x = 0.0f, y = 0.0f, z = 0.0f;
    std::uint32_t rgba      = 0xFFFFFFFFu;
    std::uint8_t  variation = 0;
    CellFace      face      = CellFace::None;
    std::uint8_t  flags     = 0;
    std::uint8_t  shade     = 255;

    constexpr CompactVertex3D() noexcept = default;

    CompactVertex3D(const vector3d& p, const Color& c, CellFace f = CellFace::None, std::uint8_t shade_level = 255,
                    std::uint8_t variation_amount = 0, std::uint8_t flag_bits = 0) noexcept
        : x(static_cast<float>(p.x)), y(static_cast<float>(p.y)), z(static_cast<float>(p.z)),
          rgba(Vertex3D::pack(c)), variation(variation_amount), face(f), flags(flag_bits), shade(shade_level) {}

    vector3d position() const noexcept { return { double(x), double(y), double(z) }; }
    Color    color()    const noexcept { Vertex3D v; v.rgba = rgba; return v.color(); }

    vector3d normal() const noexcept {
        switch (face) {
            case CellFace::NegX: return { -1.0, 0.0, 0.0 };
            case CellFace::PosX: return {  1.0, 0.0, 0.0 };
            case CellFace::NegY: return { 0.0, -1.0, 0.0 };
            case CellFace::PosY: return { 0.0,  1.0, 0.0 };
            case CellFace::NegZ: return { 0.0, 0.0, -1.0 };
            case CellFace::PosZ: return { 0.0, 0.0,  1.0 };
            default:             return { 0.0, 0.0, 0.0 };
        }
    }
};

static_assert(sizeof(CompactVertex3D) == 20, "CompactVertex3D layout is shared with the GPU pipeline");

class QuadMesh3D {
public:
    static constexpr std::size_t VERTICES_PER_QUAD = 4;
    static constexpr std::size_t INDICES_PER_QUAD  = 6;

    QuadMesh3D() = default;

    void add_quad(const CompactVertex3D& a, const CompactVertex3D& b, const CompactVertex3D& c, const CompactVertex3D& d) {
        m_vertices.push_back(a); m_vertices.push_back(b); m_vertices.push_back(c); m_vertices.push_back(d);
        m_version.touch();
    }

    void clear() noexcept { m_vertices.clear(); m_version.touch(); }
    void reserve(std::size_t quads) { m_vertices.reserve(quads * VERTICES_PER_QUAD); }
    void shrink_to_fit() { m_vertices.shrink_to_fit(); }

    const std::vector<CompactVertex3D>& vertices() const noexcept { return m_vertices; }
    std::vector<CompactVertex3D>& edit_vertices() noexcept { m_version.touch(); return m_vertices; }
    void touch() noexcept { m_version.touch(); }

    bool          empty()        const noexcept { return m_vertices.size() < VERTICES_PER_QUAD; }
    std::size_t   quad_count()   const noexcept { return m_vertices.size() / VERTICES_PER_QUAD; }
    std::size_t   memory_bytes() const noexcept { return m_vertices.capacity() * sizeof(CompactVertex3D); }
    std::uint64_t version()      const noexcept { return m_version.get(); }

    template <typename Fn>
    static void for_each_quad_index(std::size_t quads, Fn&& fn) {
        for (std::size_t q = 0; q < quads; ++q) {
            const std::uint32_t base = static_cast<std::uint32_t>(q * VERTICES_PER_QUAD);
            fn(base); fn(base + 1); fn(base + 2); fn(base); fn(base + 2); fn(base + 3);
        }
    }

private:
    std::vector<CompactVertex3D> m_vertices;
    ContentVersion               m_version;
};

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
    std::size_t memory_bytes()   const noexcept { return m_vertices.capacity() * sizeof(Vertex3D) + m_indices.capacity() * sizeof(std::uint32_t); }
    std::size_t triangle_count() const noexcept { return (indexed() ? m_indices.size() : m_vertices.size()) / 3; }
    std::uint64_t version()      const noexcept { return m_version.get(); }

private:
    std::vector<Vertex3D>      m_vertices;
    std::vector<std::uint32_t> m_indices;
    ContentVersion             m_version;
};


namespace detail {

struct MeshSlot3D {
    bool          quads         = false;
    bool          keep_cpu      = false;
    bool          resident      = false;
    bool          lost          = false;
    std::uint64_t owner_epoch   = 0;
    std::uint64_t gpu_bytes     = 0;
    std::size_t   element_count = 0;
    Mesh3D        mesh;
    QuadMesh3D    quad_mesh;

    bool has_cpu_data() const noexcept { return quads ? !quad_mesh.empty() : !mesh.empty(); }
    std::size_t cpu_bytes() const noexcept { return quads ? quad_mesh.memory_bytes() : mesh.memory_bytes(); }

    void drop_cpu_data() noexcept {
        if (keep_cpu) return;
        mesh = Mesh3D();
        quad_mesh = QuadMesh3D();
    }
};

} // namespace detail

class MeshHandle3D {
public:
    MeshHandle3D() = default;

    static MeshHandle3D from(Mesh3D mesh, bool keep_cpu_copy = false) {
        MeshHandle3D h;
        h.m_slot = std::make_shared<detail::MeshSlot3D>();
        h.m_slot->quads = false;
        h.m_slot->keep_cpu = keep_cpu_copy;
        h.m_slot->element_count = mesh.indexed() ? mesh.indices().size() : mesh.vertices().size();
        h.m_slot->mesh = std::move(mesh);
        return h;
    }

    static MeshHandle3D from(QuadMesh3D quads, bool keep_cpu_copy = false) {
        MeshHandle3D h;
        h.m_slot = std::make_shared<detail::MeshSlot3D>();
        h.m_slot->quads = true;
        h.m_slot->keep_cpu = keep_cpu_copy;
        h.m_slot->element_count = quads.quad_count();
        h.m_slot->quad_mesh = std::move(quads);
        return h;
    }

    bool valid()    const noexcept { return m_slot && m_slot->element_count > 0 && !m_slot->lost; }
    bool empty()    const noexcept { return !m_slot || m_slot->element_count == 0; }
    bool lost()     const noexcept { return m_slot && m_slot->lost; }
    bool resident() const noexcept { return m_slot && m_slot->resident; }
    bool is_quads() const noexcept { return m_slot && m_slot->quads; }

    std::size_t   quad_count()     const noexcept { return m_slot && m_slot->quads ? m_slot->element_count : 0; }
    std::size_t   element_count()  const noexcept { return m_slot ? m_slot->element_count : 0; }
    std::size_t   cpu_bytes()      const noexcept { return m_slot ? m_slot->cpu_bytes() : 0; }
    std::uint64_t gpu_bytes()      const noexcept { return m_slot ? m_slot->gpu_bytes : 0; }

    void reset() noexcept { m_slot.reset(); }

    const std::shared_ptr<detail::MeshSlot3D>& slot() const noexcept { return m_slot; }

private:
    std::shared_ptr<detail::MeshSlot3D> m_slot;
};

} // namespace graphics
} // namespace fizmo

#endif // FIZMO_MESH_3D_HPP