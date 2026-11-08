#include "texture.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>

namespace fizmo {
namespace lighting {

namespace {

const std::array<float, 256>& srgb_table() {
    static const std::array<float, 256> table = []() {
        std::array<float, 256> t{};
        for (int i = 0; i < 256; ++i) t[static_cast<std::size_t>(i)] = srgb_to_linear(static_cast<float>(i) / 255.0f);
        return t;
    }();
    return table;
}

std::uint64_t next_version() noexcept {
    static std::atomic<std::uint64_t> counter{ 0 };
    return ++counter;
}

} // namespace

UvTransform UvTransform::tiling(float u, float v) noexcept {
    UvTransform t;
    t.scale = { u, v };
    return t;
}

UvTransform UvTransform::make(Vec2f s, Vec2f o, float rotation_radians) noexcept {
    UvTransform t;
    t.scale = s;
    t.offset = o;
    t.rotation = rotation_radians;
    return t;
}

bool UvTransform::is_identity() const noexcept {
    return scale.x == 1.0f && scale.y == 1.0f && offset.x == 0.0f && offset.y == 0.0f && rotation == 0.0f;
}

void UvTransform::matrix(float out[6]) const noexcept {
    const float c = std::cos(rotation), s = std::sin(rotation);
    out[0] = c * scale.x;
    out[1] = -s * scale.y;
    out[2] = offset.x;
    out[3] = s * scale.x;
    out[4] = c * scale.y;
    out[5] = offset.y;
}

Vec2f UvTransform::apply(Vec2f uv) const noexcept {
    if (rotation == 0.0f) return { uv.x * scale.x + offset.x, uv.y * scale.y + offset.y };
    float m[6];
    matrix(m);
    return { m[0] * uv.x + m[1] * uv.y + m[2], m[3] * uv.x + m[4] * uv.y + m[5] };
}

Texture::Texture(unsigned width, unsigned height, std::vector<std::uint8_t> rgba8, ColorSpace space) : m_space(space) {
    assign_rgba8(width, height, std::move(rgba8));
}

Texture Texture::from_rgba8(unsigned width, unsigned height, std::vector<std::uint8_t> rgba8, ColorSpace space) {
    return Texture(width, height, std::move(rgba8), space);
}

Texture Texture::from_float(unsigned width, unsigned height, std::vector<float> rgba, ColorSpace space) {
    Texture t;
    t.m_space = space;
    t.assign_float(width, height, std::move(rgba));
    return t;
}

Texture Texture::solid(const Rgba& c, ColorSpace space) {
    return from_float(1, 1, { c.r, c.g, c.b, c.a }, space);
}

Texture Texture::checker(unsigned size, unsigned cells, const Rgba& a, const Rgba& b, ColorSpace space) {
    size = std::max(size, 1u);
    cells = std::max(cells, 1u);
    std::vector<float> px(static_cast<std::size_t>(size) * size * 4);
    for (unsigned y = 0; y < size; ++y) {
        for (unsigned x = 0; x < size; ++x) {
            const unsigned cx = x * cells / size, cy = y * cells / size;
            const Rgba& c = ((cx + cy) & 1u) ? b : a;
            float* d = &px[(static_cast<std::size_t>(y) * size + x) * 4];
            d[0] = c.r;
            d[1] = c.g;
            d[2] = c.b;
            d[3] = c.a;
        }
    }
    return from_float(size, size, std::move(px), space);
}

void Texture::assign_rgba8(unsigned width, unsigned height, std::vector<std::uint8_t> rgba8) {
    const std::size_t need = static_cast<std::size_t>(width) * height * 4;
    m_floats.clear();
    if (rgba8.size() < need || need == 0) {
        m_width = m_height = 0;
        m_bytes.clear();
    } else {
        m_width = width;
        m_height = height;
        rgba8.resize(need);
        m_bytes = std::move(rgba8);
    }
    refresh();
}

void Texture::assign_float(unsigned width, unsigned height, std::vector<float> rgba) {
    const std::size_t need = static_cast<std::size_t>(width) * height * 4;
    m_bytes.clear();
    if (rgba.size() < need || need == 0) {
        m_width = m_height = 0;
        m_floats.clear();
    } else {
        m_width = width;
        m_height = height;
        rgba.resize(need);
        m_floats = std::move(rgba);
    }
    refresh();
}

void Texture::refresh() {
    m_version = next_version();
    m_alpha = false;
    m_average = Rgba(0.0f, 0.0f, 0.0f, 1.0f);
    if (!valid()) return;
    double sum[4] = { 0.0, 0.0, 0.0, 0.0 };
    for (unsigned y = 0; y < m_height; ++y) {
        for (unsigned x = 0; x < m_width; ++x) {
            const Rgba t = texel(static_cast<int>(x), static_cast<int>(y));
            sum[0] += t.r;
            sum[1] += t.g;
            sum[2] += t.b;
            sum[3] += t.a;
            if (t.a < 1.0f) m_alpha = true;
        }
    }
    const double inv = 1.0 / (static_cast<double>(m_width) * m_height);
    m_average = Rgba(static_cast<float>(sum[0] * inv), static_cast<float>(sum[1] * inv), static_cast<float>(sum[2] * inv), static_cast<float>(sum[3] * inv));
}

Rgba Texture::stored(int x, int y) const noexcept {
    if (!valid()) return {};
    x = std::clamp(x, 0, static_cast<int>(m_width) - 1);
    y = std::clamp(y, 0, static_cast<int>(m_height) - 1);
    const std::size_t i = (static_cast<std::size_t>(y) * m_width + static_cast<std::size_t>(x)) * 4;
    if (!m_floats.empty()) return { m_floats[i], m_floats[i + 1], m_floats[i + 2], m_floats[i + 3] };
    constexpr float k = 1.0f / 255.0f;
    return { m_bytes[i] * k, m_bytes[i + 1] * k, m_bytes[i + 2] * k, m_bytes[i + 3] * k };
}

Rgba Texture::texel(int x, int y) const noexcept {
    if (!valid()) return {};
    if (m_space == ColorSpace::linear) return stored(x, y);
    x = std::clamp(x, 0, static_cast<int>(m_width) - 1);
    y = std::clamp(y, 0, static_cast<int>(m_height) - 1);
    const std::size_t i = (static_cast<std::size_t>(y) * m_width + static_cast<std::size_t>(x)) * 4;
    if (!m_floats.empty()) return { srgb_to_linear(m_floats[i]), srgb_to_linear(m_floats[i + 1]), srgb_to_linear(m_floats[i + 2]), m_floats[i + 3] };
    const auto& lut = srgb_table();
    return { lut[m_bytes[i]], lut[m_bytes[i + 1]], lut[m_bytes[i + 2]], m_bytes[i + 3] * (1.0f / 255.0f) };
}

int Texture::wrap_coord(int c, int size, TextureWrap wrap) noexcept {
    if (size <= 0) return 0;
    switch (wrap) {
    case TextureWrap::repeat: {
        c %= size;
        return c < 0 ? c + size : c;
    }
    case TextureWrap::mirror: {
        const int period = size * 2;
        int m = c % period;
        if (m < 0) m += period;
        return m < size ? m : period - 1 - m;
    }
    case TextureWrap::clamp: break;
    }
    return std::clamp(c, 0, size - 1);
}

Rgba Texture::texel(int x, int y, TextureWrap wrap) const noexcept {
    return texel(wrap_coord(x, static_cast<int>(m_width), wrap), wrap_coord(y, static_cast<int>(m_height), wrap));
}

Rgba Texture::sample(Vec2f uv, TextureFilter filter, TextureWrap wrap) const noexcept {
    if (!valid()) return {};
    if (!std::isfinite(uv.x) || !std::isfinite(uv.y)) uv = { 0.0f, 0.0f };
    const float px = uv.x * static_cast<float>(m_width);
    const float py = uv.y * static_cast<float>(m_height);
    if (filter == TextureFilter::nearest) return texel(static_cast<int>(std::floor(px)), static_cast<int>(std::floor(py)), wrap);
    const float fx = px - 0.5f, fy = py - 0.5f;
    const float x0f = std::floor(fx), y0f = std::floor(fy);
    const float tx = fx - x0f, ty = fy - y0f;
    const int x0 = static_cast<int>(x0f), y0 = static_cast<int>(y0f);
    const Rgba a = texel(x0, y0, wrap), b = texel(x0 + 1, y0, wrap);
    const Rgba c = texel(x0, y0 + 1, wrap), d = texel(x0 + 1, y0 + 1, wrap);
    const Rgba top = a * (1.0f - tx) + b * tx;
    const Rgba bottom = c * (1.0f - tx) + d * tx;
    return top * (1.0f - ty) + bottom * ty;
}

Rgba Texture::average() const noexcept {
    return m_average;
}

TextureMap TextureMap::of(std::shared_ptr<const Texture> tex, int channel_index) {
    TextureMap m(std::move(tex));
    m.channel = channel_index;
    return m;
}

Rgba TextureMap::sample(Vec2f uv) const noexcept {
    if (!valid()) return {};
    return texture->sample(transform.apply(uv), filter, wrap);
}

Rgba TextureMap::sample(Vec2f uv, const UvTransform& outer) const noexcept {
    if (!valid()) return {};
    return texture->sample(transform.apply(outer.apply(uv)), filter, wrap);
}

Rgba TextureMap::sample_triplanar(const Vec3f& p, const Vec3f& n) const noexcept {
    if (!valid()) return {};
    const Vec3f w = triplanar_weights(n, triplanar_sharpness);
    const Vec3f q = p * triplanar_scale;
    Rgba out(0.0f, 0.0f, 0.0f, 0.0f);
    if (w.x > 0.0f) out = out + texture->sample(transform.apply({ q.z, -q.y }), filter, wrap) * w.x;
    if (w.y > 0.0f) out = out + texture->sample(transform.apply({ q.x, -q.z }), filter, wrap) * w.y;
    if (w.z > 0.0f) out = out + texture->sample(transform.apply({ q.x, -q.y }), filter, wrap) * w.z;
    return out;
}

float TextureMap::value(Vec2f uv, const UvTransform& outer) const noexcept {
    return sample(uv, outer)[std::clamp(channel, 0, 3)];
}

Vec3f decode_normal(const Rgba& t, float strength) noexcept {
    Vec3f n{ (t.r * 2.0f - 1.0f) * strength, (t.g * 2.0f - 1.0f) * strength, t.b * 2.0f - 1.0f };
    if (n.z < 1e-3f) n.z = 1e-3f;
    return normalize(n);
}

Vec3f perturb_normal(const Vec3f& n, const Vec3f& tangent, const Vec3f& bitangent, const Vec3f& ts) noexcept {
    const Vec3f out = normalize(tangent * ts.x + bitangent * ts.y + n * ts.z);
    return length_squared(out) > 0.0f ? out : n;
}

Vec3f triplanar_weights(const Vec3f& n, float sharpness) noexcept {
    const float k = std::max(sharpness, 1.0f);
    Vec3f w{ std::pow(std::fabs(n.x), k), std::pow(std::fabs(n.y), k), std::pow(std::fabs(n.z), k) };
    const float s = w.x + w.y + w.z;
    if (s <= 0.0f) return { 0.0f, 0.0f, 1.0f };
    return w / s;
}

} // namespace lighting
} // namespace fizmo
