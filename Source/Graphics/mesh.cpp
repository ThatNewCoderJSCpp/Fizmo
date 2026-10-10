#include "fizmo_library.hpp"
#include "mesh.hpp"

namespace fizmo {
namespace graphics {

Vertex3D::Vertex3D(const vector3d& p, const Color& c) noexcept : x(static_cast<float>(p.x)), y(static_cast<float>(p.y)), z(static_cast<float>(p.z)), rgba(pack(c)) {}

Vertex3D::Vertex3D(const vector3d& p, const vector3d& n, const Color& c, float tu, float tv) noexcept : x(static_cast<float>(p.x)), y(static_cast<float>(p.y)), z(static_cast<float>(p.z)),
          nx(static_cast<float>(n.x)), ny(static_cast<float>(n.y)), nz(static_cast<float>(n.z)),
          u(tu), v(tv), rgba(pack(c)) {}

auto Vertex3D::color() const noexcept -> Color {
    return Color(static_cast<std::uint8_t>(rgba & 0xFF), static_cast<std::uint8_t>((rgba >> 8) & 0xFF),
                 static_cast<std::uint8_t>((rgba >> 16) & 0xFF), static_cast<std::uint8_t>(rgba >> 24));
}

std::uint8_t compact_swell(double scale) noexcept {
    const double s = scale < 0.0 ? 0.0 : (scale > 1.0 ? 1.0 : scale);
    return static_cast<std::uint8_t>(static_cast<std::uint8_t>(s * COMPACT_SWELL_MAX + 0.5) << COMPACT_SWELL_SHIFT);
}

int cell_variation(std::int32_t x, std::int32_t y, std::int32_t z, unsigned int amount) noexcept {
    if (amount == 0) return 0;
    std::uint32_t h = static_cast<std::uint32_t>(x) * 374761393u
                    + static_cast<std::uint32_t>(y) * 668265263u
                    + static_cast<std::uint32_t>(z) * 2147483647u;
    h = (h ^ (h >> 13)) * 1274126177u;
    h ^= h >> 16;
    return static_cast<int>(h % (2u * amount + 1u)) - static_cast<int>(amount);
}

Color cell_tinted(const Color& base, int delta, std::uint8_t shade) noexcept {
    auto ch = [delta, shade](std::uint8_t c) {
        int v = static_cast<int>(c) + delta;
        v = v < 0 ? 0 : (v > 255 ? 255 : v);
        return static_cast<std::uint8_t>((v * static_cast<int>(shade) + 127) / 255);
    };
    return Color(ch(base.red()), ch(base.green()), ch(base.blue()), base.alpha());
}

CompactVertex3D::CompactVertex3D(const vector3d& p, const Color& c, CellFace f, std::uint8_t shade_level,
                    std::uint8_t variation_amount, std::uint8_t flag_bits, BakedLight baked) noexcept : x(static_cast<float>(p.x)), y(static_cast<float>(p.y)), z(static_cast<float>(p.z)),
          rgba(Vertex3D::pack(c)), variation(variation_amount), face(f), flags(flag_bits), shade(shade_level), light(baked.packed()) {}

auto CompactVertex3D::normal() const noexcept -> vector3d {
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

FaceMask facing_faces(const vector3d& eye, const vector3d& lo, const vector3d& hi) noexcept {
    FaceMask m = face_bit(CellFace::None);
    if (eye.x < hi.x) m |= face_bit(CellFace::NegX);
    if (eye.x > lo.x) m |= face_bit(CellFace::PosX);
    if (eye.y < hi.y) m |= face_bit(CellFace::NegY);
    if (eye.y > lo.y) m |= face_bit(CellFace::PosY);
    if (eye.z < hi.z) m |= face_bit(CellFace::NegZ);
    if (eye.z > lo.z) m |= face_bit(CellFace::PosZ);
    return m;
}

void QuadMesh3D::add_quad(const CompactVertex3D& a, const CompactVertex3D& b, const CompactVertex3D& c, const CompactVertex3D& d) {
    m_vertices.push_back(a); m_vertices.push_back(b); m_vertices.push_back(c); m_vertices.push_back(d);
    m_grouped = false;
    m_version.touch();
}

void QuadMesh3D::finalize() {
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

std::size_t QuadMesh3D::group_of(std::size_t quad) const noexcept {
    const auto f = static_cast<std::size_t>(m_vertices[quad * VERTICES_PER_QUAD].face);
    return f < CELL_FACE_GROUPS ? f : CELL_FACE_GROUPS - 1;
}

auto Material3D::transparent() noexcept -> Material3D { Material3D m; m.blend = Blend3D::Alpha; m.depth_write = false; m.cull = Cull3D::None; m.shadow = Shadow3D::None; return m; }

auto Material3D::physical(float metallic_value, float roughness_value) noexcept -> Material3D {
    Material3D m;
    m.pbr = true;
    m.metallic = metallic_value < 0.0f ? 0.0f : (metallic_value > 1.0f ? 1.0f : metallic_value);
    m.roughness = roughness_value < 0.0f ? 0.0f : (roughness_value > 1.0f ? 1.0f : roughness_value);
    return m;
}

void Mesh3D::add_quad(const Vertex3D& a, const Vertex3D& b, const Vertex3D& c, const Vertex3D& d) {
    const std::uint32_t base = static_cast<std::uint32_t>(m_vertices.size());
    m_vertices.push_back(a); m_vertices.push_back(b); m_vertices.push_back(c); m_vertices.push_back(d);
    const std::uint32_t idx[6] = { base, base + 1, base + 2, base, base + 2, base + 3 };
    m_indices.insert(m_indices.end(), idx, idx + 6);
    m_version.touch();
}

std::size_t Mesh3D::memory_bytes() const noexcept { return m_vertices.capacity() * sizeof(Vertex3D) + m_indices.capacity() * sizeof(std::uint32_t); }

void Bounds3D::include(float x, float y, float z) noexcept {
    const float p[3] = { x, y, z };

    for (int a = 0; a < 3; ++a) {
        if (!valid || p[a] < lo[a]) lo[a] = p[a];
        if (!valid || p[a] > hi[a]) hi[a] = p[a];
    }

    valid = true;
}

auto MeshHandle3D::from(Mesh3D mesh, bool keep_cpu_copy) -> MeshHandle3D {
    MeshHandle3D h;
    h.m_slot = std::make_shared<detail::MeshSlot3D>();
    h.m_slot->quads = false;
    h.m_slot->keep_cpu = keep_cpu_copy;
    h.m_slot->element_count = mesh.indexed() ? mesh.indices().size() : mesh.vertices().size();
    h.m_slot->bounds = Bounds3D::of(mesh.vertices());
    h.m_slot->mesh = std::move(mesh);
    return h;
}

auto MeshHandle3D::from(QuadMesh3D quads, bool keep_cpu_copy) -> MeshHandle3D {
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

void QuadBatch3D::add(const MeshHandle3D& mesh, const vector3d& offset, FaceMask faces) {
    if (!mesh.valid() || !mesh.is_quads() || faces == 0) return;
    m_items.push_back({ mesh.slot().get(), offset, faces });
}

} // namespace graphics
} // namespace fizmo
