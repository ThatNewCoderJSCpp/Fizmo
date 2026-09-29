#ifndef FIZMO_MESH_3D_HPP
#define FIZMO_MESH_3D_HPP

#include <cstddef>
#include <algorithm>
#include <array>
#include <cstdint>
#include <memory>
#include <mutex>
#include <utility>
#include <vector>
#include "color.hpp"
#include "content_version.hpp"
#include "lighting_3d.hpp"
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
    std::uint32_t light     = LIGHT_FULL_SKY;

    constexpr CompactVertex3D() noexcept = default;

    CompactVertex3D(const vector3d& p, const Color& c, CellFace f = CellFace::None, std::uint8_t shade_level = 255,
                    std::uint8_t variation_amount = 0, std::uint8_t flag_bits = 0, BakedLight baked = BakedLight::full_sky()) noexcept
        : x(static_cast<float>(p.x)), y(static_cast<float>(p.y)), z(static_cast<float>(p.z)),
          rgba(Vertex3D::pack(c)), variation(variation_amount), face(f), flags(flag_bits), shade(shade_level), light(baked.packed()) {}

    BakedLight baked_light() const noexcept { return BakedLight::unpack(light); }

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

static_assert(sizeof(CompactVertex3D) == 24, "CompactVertex3D layout is shared with the GPU pipeline");

constexpr std::size_t CELL_FACE_GROUPS = 7;

using FaceMask = std::uint8_t;
constexpr FaceMask ALL_FACE_GROUPS = static_cast<FaceMask>((1u << CELL_FACE_GROUPS) - 1u);

constexpr FaceMask face_bit(CellFace f) noexcept { return static_cast<FaceMask>(1u << static_cast<unsigned>(f)); }

inline FaceMask facing_faces(const vector3d& eye, const vector3d& lo, const vector3d& hi) noexcept {
    FaceMask m = face_bit(CellFace::None);
    if (eye.x < hi.x) m |= face_bit(CellFace::NegX);
    if (eye.x > lo.x) m |= face_bit(CellFace::PosX);
    if (eye.y < hi.y) m |= face_bit(CellFace::NegY);
    if (eye.y > lo.y) m |= face_bit(CellFace::PosY);
    if (eye.z < hi.z) m |= face_bit(CellFace::NegZ);
    if (eye.z > lo.z) m |= face_bit(CellFace::PosZ);
    return m;
}

struct QuadRange {
    std::uint32_t first = 0;
    std::uint32_t count = 0;
};

using FaceGroups = std::array<QuadRange, CELL_FACE_GROUPS>;

class QuadMesh3D {
public:
    static constexpr std::size_t VERTICES_PER_QUAD = 4;
    static constexpr std::size_t INDICES_PER_QUAD  = 6;

    QuadMesh3D() = default;

    void add_quad(const CompactVertex3D& a, const CompactVertex3D& b, const CompactVertex3D& c, const CompactVertex3D& d) {
        m_vertices.push_back(a); m_vertices.push_back(b); m_vertices.push_back(c); m_vertices.push_back(d);
        m_grouped = false;
        m_version.touch();
    }

    void finalize() {
        const std::size_t quads = quad_count();
        std::array<std::uint32_t, CELL_FACE_GROUPS> counts{};
        bool sorted = true;
        std::size_t last = 0;

        for (std::size_t q = 0; q < quads; ++q) {
            const std::size_t g = group_of(q);
            ++counts[g];
            if (g < last) sorted = false;
            last = g;
        }

        std::uint32_t first = 0;
        for (std::size_t g = 0; g < CELL_FACE_GROUPS; ++g) { m_groups[g] = { first, counts[g] }; first += counts[g]; }

        if (!sorted) {
            std::vector<CompactVertex3D> out(m_vertices.size());
            std::array<std::uint32_t, CELL_FACE_GROUPS> cursor{};
            for (std::size_t g = 0; g < CELL_FACE_GROUPS; ++g) cursor[g] = m_groups[g].first;

            for (std::size_t q = 0; q < quads; ++q) {
                const std::size_t dst = cursor[group_of(q)]++;
                std::copy_n(&m_vertices[q * VERTICES_PER_QUAD], VERTICES_PER_QUAD, &out[dst * VERTICES_PER_QUAD]);
            }

            m_vertices.swap(out);
        }

        m_vertices.shrink_to_fit();
        m_grouped = true;
        m_version.touch();
    }

    bool              grouped() const noexcept { return m_grouped; }
    const FaceGroups& groups()  const noexcept { return m_groups; }
    QuadRange         group(CellFace f) const noexcept { return m_groups[static_cast<std::size_t>(f)]; }

    void clear() noexcept { m_vertices.clear(); m_groups = {}; m_grouped = false; m_version.touch(); }
    void reserve(std::size_t quads) { m_vertices.reserve(quads * VERTICES_PER_QUAD); }
    void shrink_to_fit() { m_vertices.shrink_to_fit(); }

    const std::vector<CompactVertex3D>& vertices() const noexcept { return m_vertices; }
    std::vector<CompactVertex3D>& edit_vertices() noexcept { m_grouped = false; m_version.touch(); return m_vertices; }
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
    std::size_t group_of(std::size_t quad) const noexcept {
        const auto f = static_cast<std::size_t>(m_vertices[quad * VERTICES_PER_QUAD].face);
        return f < CELL_FACE_GROUPS ? f : CELL_FACE_GROUPS - 1;
    }

    std::vector<CompactVertex3D> m_vertices;
    FaceGroups                   m_groups{};
    bool                         m_grouped = false;
    ContentVersion               m_version;
};

struct Instance3D {
    float         x = 0.0f, y = 0.0f, z = 0.0f;
    float         scale = 1.0f;
    std::uint32_t rgba  = 0xFFFFFFFFu;
    std::uint32_t light = LIGHT_FULL_SKY;

    constexpr Instance3D() noexcept = default;
    Instance3D(float px, float py, float pz, float s, const Color& c, BakedLight baked = BakedLight::full_sky()) noexcept
        : x(px), y(py), z(pz), scale(s), rgba(Vertex3D::pack(c)), light(baked.packed()) {}
};

static_assert(sizeof(Instance3D) == 24, "Instance3D layout is shared with the GPU pipeline");

enum class Cull3D  : std::uint8_t { Back = 0, None, Front };   
enum class Blend3D : std::uint8_t { Opaque = 0, Alpha, Additive };
enum class Shadow3D : std::uint8_t { Cast = 0, None, CastOnly };

struct Material3D {
    const Texture* texture     = nullptr;  
    Cull3D         cull        = Cull3D::Back;
    Blend3D        blend       = Blend3D::Opaque;
    bool           depth_test  = true;
    bool           depth_write = true;     
    bool           lit         = true;
    Shadow3D       shadow      = Shadow3D::Cast;
    std::uint32_t  light       = LIGHT_FULL_SKY;

    static Material3D opaque() noexcept { return {}; }
    static Material3D double_sided() noexcept { Material3D m; m.cull = Cull3D::None; return m; }
    static Material3D transparent() noexcept { Material3D m; m.blend = Blend3D::Alpha; m.depth_write = false; m.cull = Cull3D::None; m.shadow = Shadow3D::None; return m; }
    static Material3D unlit() noexcept { Material3D m; m.lit = false; m.shadow = Shadow3D::None; return m; }
    static Material3D shadow_caster() noexcept { Material3D m; m.shadow = Shadow3D::CastOnly; m.cull = Cull3D::None; return m; }

    Material3D& with_light(BakedLight baked) noexcept { light = baked.packed(); return *this; }
    Material3D& with_shadow(Shadow3D mode) noexcept { shadow = mode; return *this; }
    Material3D& with_lit(bool on) noexcept { lit = on; return *this; }

    bool visible()      const noexcept { return shadow != Shadow3D::CastOnly; }
    bool casts_shadow() const noexcept { return shadow != Shadow3D::None && blend == Blend3D::Opaque; }
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


struct Bounds3D {
    float lo[3] = { 0.0f, 0.0f, 0.0f };
    float hi[3] = { 0.0f, 0.0f, 0.0f };
    bool  valid = false;

    void include(float x, float y, float z) noexcept {
        const float p[3] = { x, y, z };

        for (int a = 0; a < 3; ++a) {
            if (!valid || p[a] < lo[a]) lo[a] = p[a];
            if (!valid || p[a] > hi[a]) hi[a] = p[a];
        }

        valid = true;
    }

    template <typename V>
    static Bounds3D of(const std::vector<V>& vertices) noexcept {
        Bounds3D b;
        for (const V& v : vertices) b.include(v.x, v.y, v.z);
        return b;
    }
};

namespace detail {

class MeshReleaseQueue {
public:
    void push(std::uint64_t key) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_keys.push_back(key);
    }

    template <typename Fn>
    void drain(Fn&& fn) {
        std::vector<std::uint64_t> keys;

        {
            std::lock_guard<std::mutex> lock(m_mutex);
            if (m_keys.empty()) return;
            keys.swap(m_keys);
        }

        for (std::uint64_t k : keys) fn(k);
    }

private:
    std::mutex                 m_mutex;
    std::vector<std::uint64_t> m_keys;
};

struct MeshSlot3D : std::enable_shared_from_this<MeshSlot3D> {
    bool          quads         = false;
    bool          keep_cpu      = false;
    bool          resident      = false;
    bool          lost          = false;
    std::uint64_t owner_epoch   = 0;
    std::uint64_t gpu_bytes     = 0;
    std::uint64_t gpu_key       = 0;
    std::size_t   element_count = 0;
    FaceGroups    groups{};
    Bounds3D      bounds;
    Mesh3D        mesh;
    QuadMesh3D    quad_mesh;
    std::shared_ptr<MeshReleaseQueue> release;

    MeshSlot3D() = default;
    MeshSlot3D(const MeshSlot3D&) = delete;
    MeshSlot3D& operator=(const MeshSlot3D&) = delete;
    ~MeshSlot3D() { if (release && gpu_key) release->push(gpu_key); }

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
        h.m_slot->bounds = Bounds3D::of(mesh.vertices());
        h.m_slot->mesh = std::move(mesh);
        return h;
    }

    static MeshHandle3D from(QuadMesh3D quads, bool keep_cpu_copy = false) {
        MeshHandle3D h;
        h.m_slot = std::make_shared<detail::MeshSlot3D>();
        h.m_slot->quads = true;
        h.m_slot->keep_cpu = keep_cpu_copy;
        if (!quads.grouped()) quads.finalize();
        h.m_slot->element_count = quads.quad_count();
        h.m_slot->groups = quads.groups();
        h.m_slot->bounds = Bounds3D::of(quads.vertices());
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

    const FaceGroups& groups() const noexcept {
        static const FaceGroups none{};
        return m_slot ? m_slot->groups : none;
    }

    Bounds3D bounds() const noexcept { return m_slot ? m_slot->bounds : Bounds3D{}; }

    void reset() noexcept { m_slot.reset(); }

    const std::shared_ptr<detail::MeshSlot3D>& slot() const noexcept { return m_slot; }

private:
    std::shared_ptr<detail::MeshSlot3D> m_slot;
};

class QuadBatch3D {
public:
    struct Item {
        detail::MeshSlot3D* slot;
        vector3d            offset;
        FaceMask            faces;
    };

    void clear() noexcept { m_items.clear(); }
    void reserve(std::size_t n) { m_items.reserve(n); }

    void add(const MeshHandle3D& mesh, const vector3d& offset, FaceMask faces = ALL_FACE_GROUPS) {
        if (!mesh.valid() || !mesh.is_quads() || faces == 0) return;
        m_items.push_back({ mesh.slot().get(), offset, faces });
    }

    const std::vector<Item>& items() const noexcept { return m_items; }
    std::size_t size()  const noexcept { return m_items.size(); }
    bool        empty() const noexcept { return m_items.empty(); }

private:
    std::vector<Item> m_items;
};

} // namespace graphics
} // namespace fizmo

#endif // FIZMO_MESH_3D_HPP