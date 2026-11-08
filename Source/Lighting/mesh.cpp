#include "mesh.hpp"

namespace fizmo {
namespace lighting {

namespace {

void add_quad(Mesh& m, const Vec3f& a, const Vec3f& b, const Vec3f& c, const Vec3f& d, const Vec3f& n, Vec2f uv0, Vec2f uv1) {
    const std::uint32_t base = static_cast<std::uint32_t>(m.positions.size());
    m.positions.insert(m.positions.end(), { a, b, c, d });
    m.normals.insert(m.normals.end(), { n, n, n, n });
    m.lightmap_uvs.insert(m.lightmap_uvs.end(), { Vec2f{ uv0.x, uv0.y }, Vec2f{ uv1.x, uv0.y }, Vec2f{ uv1.x, uv1.y }, Vec2f{ uv0.x, uv1.y } });
    m.indices.insert(m.indices.end(), { base, base + 1, base + 2, base, base + 2, base + 3 });
}

} // namespace

void Mesh::compute_normals() {
    normals.assign(positions.size(), Vec3f{});
    for (std::size_t i = 0; i + 2 < indices.size(); i += 3) {
        const Vec3f& a = positions[indices[i]];
        const Vec3f& b = positions[indices[i + 1]];
        const Vec3f& c = positions[indices[i + 2]];
        const Vec3f n = cross(b - a, c - a);
        normals[indices[i]] += n;
        normals[indices[i + 1]] += n;
        normals[indices[i + 2]] += n;
    }
    for (auto& n : normals) n = normalize(n);
}

void Mesh::append(const Mesh& other, const Transform& xf) {
    const std::uint32_t base = static_cast<std::uint32_t>(positions.size());
    const bool keep_normals = has_normals() || positions.empty();
    for (const auto& p : other.positions) positions.push_back(xf.point(p));
    if (keep_normals && other.has_normals()) {
        for (const auto& n : other.normals) normals.push_back(xf.normal(n));
    } else {
        normals.clear();
    }
    if (other.has_lightmap_uvs() && (lightmap_uvs.size() == base)) {
        lightmap_uvs.insert(lightmap_uvs.end(), other.lightmap_uvs.begin(), other.lightmap_uvs.end());
    } else {
        lightmap_uvs.clear();
    }
    for (auto i : other.indices) indices.push_back(base + i);
}

Mesh Mesh::box(const Vec3f& h) {
    Mesh m;
    const float s = 1.0f / 3.0f, t = 0.5f, pad = 0.01f;
    auto cell = [&](int i, int j, Vec2f& a, Vec2f& b) {
        a = { s * static_cast<float>(i) + pad, t * static_cast<float>(j) + pad };
        b = { s * static_cast<float>(i + 1) - pad, t * static_cast<float>(j + 1) - pad };
    };
    Vec2f a, b;
    cell(0, 0, a, b);
    add_quad(m, { h.x, -h.y, h.z }, { h.x, -h.y, -h.z }, { h.x, h.y, -h.z }, { h.x, h.y, h.z }, { 1, 0, 0 }, a, b);
    cell(1, 0, a, b);
    add_quad(m, { -h.x, -h.y, -h.z }, { -h.x, -h.y, h.z }, { -h.x, h.y, h.z }, { -h.x, h.y, -h.z }, { -1, 0, 0 }, a, b);
    cell(2, 0, a, b);
    add_quad(m, { -h.x, h.y, h.z }, { h.x, h.y, h.z }, { h.x, h.y, -h.z }, { -h.x, h.y, -h.z }, { 0, 1, 0 }, a, b);
    cell(0, 1, a, b);
    add_quad(m, { -h.x, -h.y, -h.z }, { h.x, -h.y, -h.z }, { h.x, -h.y, h.z }, { -h.x, -h.y, h.z }, { 0, -1, 0 }, a, b);
    cell(1, 1, a, b);
    add_quad(m, { -h.x, -h.y, h.z }, { h.x, -h.y, h.z }, { h.x, h.y, h.z }, { -h.x, h.y, h.z }, { 0, 0, 1 }, a, b);
    cell(2, 1, a, b);
    add_quad(m, { h.x, -h.y, -h.z }, { -h.x, -h.y, -h.z }, { -h.x, h.y, -h.z }, { h.x, h.y, -h.z }, { 0, 0, -1 }, a, b);
    return m;
}

Mesh Mesh::sphere(float radius, int segments, int rings) {
    Mesh m;
    segments = std::max(segments, 3);
    rings = std::max(rings, 2);
    for (int r = 0; r <= rings; ++r) {
        const float v = static_cast<float>(r) / static_cast<float>(rings);
        const float theta = v * kPi;
        for (int s = 0; s <= segments; ++s) {
            const float u = static_cast<float>(s) / static_cast<float>(segments);
            const float phi = u * kTwoPi;
            const Vec3f n{ std::sin(theta) * std::cos(phi), std::cos(theta), std::sin(theta) * std::sin(phi) };
            m.positions.push_back(n * radius);
            m.normals.push_back(n);
            m.lightmap_uvs.push_back({ u, v });
        }
    }
    const std::uint32_t row = static_cast<std::uint32_t>(segments + 1);
    for (int r = 0; r < rings; ++r) {
        for (int s = 0; s < segments; ++s) {
            const std::uint32_t a = static_cast<std::uint32_t>(r) * row + static_cast<std::uint32_t>(s);
            const std::uint32_t b = a + row;
            if (r != 0) m.indices.insert(m.indices.end(), { a, a + 1, b });
            if (r != rings - 1) m.indices.insert(m.indices.end(), { a + 1, b + 1, b });
        }
    }
    return m;
}

Mesh Mesh::plane(float hw, float hd) {
    Mesh m;
    add_quad(m, { -hw, 0.0f, hd }, { hw, 0.0f, hd }, { hw, 0.0f, -hd }, { -hw, 0.0f, -hd }, { 0, 1, 0 }, { 0.0f, 0.0f }, { 1.0f, 1.0f });
    return m;
}

Mesh Mesh::quad(const Vec3f& a, const Vec3f& b, const Vec3f& c, const Vec3f& d) {
    Mesh m;
    add_quad(m, a, b, c, d, normalize(cross(b - a, c - a)), { 0.0f, 0.0f }, { 1.0f, 1.0f });
    return m;
}

Mesh Mesh::cylinder(float radius, float hh, int segments, bool capped) {
    Mesh m;
    segments = std::max(segments, 3);
    for (int s = 0; s <= segments; ++s) {
        const float u = static_cast<float>(s) / static_cast<float>(segments);
        const float phi = u * kTwoPi;
        const Vec3f n{ std::cos(phi), 0.0f, std::sin(phi) };
        m.positions.push_back({ n.x * radius, -hh, n.z * radius });
        m.positions.push_back({ n.x * radius, hh, n.z * radius });
        m.normals.push_back(n);
        m.normals.push_back(n);
        m.lightmap_uvs.push_back({ u, 0.0f });
        m.lightmap_uvs.push_back({ u, 0.5f });
    }
    for (int s = 0; s < segments; ++s) {
        const std::uint32_t a = static_cast<std::uint32_t>(s) * 2;
        m.indices.insert(m.indices.end(), { a, a + 1, a + 3, a, a + 3, a + 2 });
    }
    if (capped) {
        for (int side = 0; side < 2; ++side) {
            const float y = side == 0 ? -hh : hh;
            const Vec3f n{ 0.0f, side == 0 ? -1.0f : 1.0f, 0.0f };
            const std::uint32_t center = static_cast<std::uint32_t>(m.positions.size());
            m.positions.push_back({ 0.0f, y, 0.0f });
            m.normals.push_back(n);
            m.lightmap_uvs.push_back({ 0.25f + 0.5f * static_cast<float>(side), 0.75f });
            for (int s = 0; s <= segments; ++s) {
                const float phi = static_cast<float>(s) / static_cast<float>(segments) * kTwoPi;
                m.positions.push_back({ std::cos(phi) * radius, y, std::sin(phi) * radius });
                m.normals.push_back(n);
                m.lightmap_uvs.push_back({ 0.25f + 0.5f * static_cast<float>(side) + 0.2f * std::cos(phi), 0.75f + 0.2f * std::sin(phi) });
            }
            for (int s = 0; s < segments; ++s) {
                const std::uint32_t a = center + 1 + static_cast<std::uint32_t>(s);
                if (side == 0) m.indices.insert(m.indices.end(), { center, a, a + 1 });
                else m.indices.insert(m.indices.end(), { center, a + 1, a });
            }
        }
    }
    return m;
}

Mesh Mesh::capsule(float radius, float hh, int segments, int rings) {
    Mesh m;
    segments = std::max(segments, 3);
    rings = std::max(rings, 2);
    const int total = rings * 2 + 1;
    for (int r = 0; r <= total; ++r) {
        float theta, y;
        if (r <= rings) {
            theta = static_cast<float>(r) / static_cast<float>(rings) * (kPi * 0.5f);
            y = hh;
        } else {
            theta = kPi * 0.5f + static_cast<float>(r - rings - 1) / static_cast<float>(rings) * (kPi * 0.5f);
            y = -hh;
        }
        for (int s = 0; s <= segments; ++s) {
            const float u = static_cast<float>(s) / static_cast<float>(segments);
            const float phi = u * kTwoPi;
            const Vec3f n{ std::sin(theta) * std::cos(phi), std::cos(theta), std::sin(theta) * std::sin(phi) };
            m.positions.push_back({ n.x * radius, n.y * radius + y, n.z * radius });
            m.normals.push_back(n);
            m.lightmap_uvs.push_back({ u, static_cast<float>(r) / static_cast<float>(total) });
        }
    }
    const std::uint32_t row = static_cast<std::uint32_t>(segments + 1);
    for (int r = 0; r < total; ++r) {
        for (int s = 0; s < segments; ++s) {
            const std::uint32_t a = static_cast<std::uint32_t>(r) * row + static_cast<std::uint32_t>(s);
            const std::uint32_t b = a + row;
            m.indices.insert(m.indices.end(), { a, a + 1, b, a + 1, b + 1, b });
        }
    }
    return m;
}

Mesh Mesh::prism(float radius, float hh) {
    Mesh m;
    Vec3f c[3];
    for (int i = 0; i < 3; ++i) {
        const float a = kTwoPi * static_cast<float>(i) / 3.0f + kPi * 0.5f;
        c[i] = { std::cos(a) * radius, 0.0f, std::sin(a) * radius };
    }
    for (int i = 0; i < 3; ++i) {
        const Vec3f p0 = c[i], p1 = c[(i + 1) % 3];
        const Vec3f n = normalize(cross(Vec3f{ 0.0f, 1.0f, 0.0f }, p1 - p0));
        const Vec3f out = dot(n, (p0 + p1) * 0.5f) < 0.0f ? -n : n;
        const Vec3f lo0{ p0.x, -hh, p0.z }, lo1{ p1.x, -hh, p1.z }, hi0{ p0.x, hh, p0.z }, hi1{ p1.x, hh, p1.z };
        if (dot(cross(lo1 - lo0, hi1 - lo0), out) > 0.0f) add_quad(m, lo0, lo1, hi1, hi0, out, { static_cast<float>(i) / 3.0f, 0.0f }, { static_cast<float>(i + 1) / 3.0f - 0.01f, 0.5f });
        else add_quad(m, lo1, lo0, hi0, hi1, out, { static_cast<float>(i) / 3.0f, 0.0f }, { static_cast<float>(i + 1) / 3.0f - 0.01f, 0.5f });
    }
    for (int side = 0; side < 2; ++side) {
        const float y = side == 0 ? -hh : hh;
        const Vec3f n{ 0.0f, side == 0 ? -1.0f : 1.0f, 0.0f };
        const std::uint32_t base = static_cast<std::uint32_t>(m.positions.size());
        for (int i = 0; i < 3; ++i) {
            m.positions.push_back({ c[i].x, y, c[i].z });
            m.normals.push_back(n);
            m.lightmap_uvs.push_back({ 0.25f + 0.5f * static_cast<float>(side) + 0.2f * c[i].x / radius, 0.75f + 0.2f * c[i].z / radius });
        }
        const Vec3f tri_n = cross(m.positions[base + 1] - m.positions[base], m.positions[base + 2] - m.positions[base]);
        if (dot(tri_n, n) > 0.0f) m.indices.insert(m.indices.end(), { base, base + 1, base + 2 });
        else m.indices.insert(m.indices.end(), { base, base + 2, base + 1 });
    }
    return m;
}

} // namespace lighting
} // namespace fizmo
