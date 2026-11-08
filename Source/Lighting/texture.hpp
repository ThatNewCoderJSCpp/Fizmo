#ifndef FIZMO_LIGHTING_TEXTURE_HPP
#define FIZMO_LIGHTING_TEXTURE_HPP

#include "color.hpp"
#include "mesh.hpp"

#include <cstdint>
#include <memory>
#include <vector>

namespace fizmo {
namespace lighting {

enum class ColorSpace : std::uint8_t {
    srgb,
    linear
};

enum class TextureFilter : std::uint8_t {
    nearest,
    bilinear
};

enum class TextureWrap : std::uint8_t {
    repeat,
    clamp,
    mirror
};

struct Rgba {
    float r = 0.0f;
    float g = 0.0f;
    float b = 0.0f;
    float a = 1.0f;

    constexpr Rgba() noexcept = default;
    constexpr Rgba(float red, float green, float blue, float alpha = 1.0f) noexcept : r(red), g(green), b(blue), a(alpha) {}
    explicit constexpr Rgba(const Rgb& c, float alpha = 1.0f) noexcept : r(c.r), g(c.g), b(c.b), a(alpha) {}

    constexpr Rgb rgb() const noexcept { return { r, g, b }; }
    constexpr float operator[](int i) const noexcept { return i == 0 ? r : (i == 1 ? g : (i == 2 ? b : a)); }
    constexpr Rgba operator+(const Rgba& o) const noexcept { return { r + o.r, g + o.g, b + o.b, a + o.a }; }
    constexpr Rgba operator*(float s) const noexcept { return { r * s, g * s, b * s, a * s }; }
};

struct UvTransform {
    Vec2f scale{ 1.0f, 1.0f };
    Vec2f offset{ 0.0f, 0.0f };
    float rotation = 0.0f;

    static UvTransform tiling(float u, float v) noexcept;
    static UvTransform make(Vec2f scale, Vec2f offset, float rotation_radians = 0.0f) noexcept;

    bool is_identity() const noexcept;
    Vec2f apply(Vec2f uv) const noexcept;
    void matrix(float out[6]) const noexcept;
};

class Texture {
public:
    Texture() = default;
    Texture(unsigned width, unsigned height, std::vector<std::uint8_t> rgba8, ColorSpace space = ColorSpace::srgb);

    static Texture from_rgba8(unsigned width, unsigned height, std::vector<std::uint8_t> rgba8, ColorSpace space = ColorSpace::srgb);
    static Texture from_float(unsigned width, unsigned height, std::vector<float> rgba, ColorSpace space = ColorSpace::linear);
    static Texture solid(const Rgba& stored, ColorSpace space = ColorSpace::linear);
    static Texture checker(unsigned size, unsigned cells, const Rgba& a, const Rgba& b, ColorSpace space = ColorSpace::linear);

    bool valid() const noexcept { return m_width > 0 && m_height > 0; }
    unsigned width() const noexcept { return m_width; }
    unsigned height() const noexcept { return m_height; }
    ColorSpace color_space() const noexcept { return m_space; }
    void set_color_space(ColorSpace space) noexcept { m_space = space; }
    bool is_float() const noexcept { return !m_floats.empty(); }
    bool has_alpha() const noexcept { return m_alpha; }
    std::uint64_t version() const noexcept { return m_version; }

    const std::vector<std::uint8_t>& bytes() const noexcept { return m_bytes; }
    const std::vector<float>& floats() const noexcept { return m_floats; }

    void assign_rgba8(unsigned width, unsigned height, std::vector<std::uint8_t> rgba8);
    void assign_float(unsigned width, unsigned height, std::vector<float> rgba);

    Rgba stored(int x, int y) const noexcept;
    Rgba texel(int x, int y) const noexcept;
    Rgba texel(int x, int y, TextureWrap wrap) const noexcept;
    Rgba sample(Vec2f uv, TextureFilter filter = TextureFilter::bilinear, TextureWrap wrap = TextureWrap::repeat) const noexcept;
    Rgba average() const noexcept;

    static int wrap_coord(int c, int size, TextureWrap wrap) noexcept;

private:
    void refresh();

    unsigned m_width = 0;
    unsigned m_height = 0;
    ColorSpace m_space = ColorSpace::srgb;
    std::vector<std::uint8_t> m_bytes;
    std::vector<float> m_floats;
    Rgba m_average;
    bool m_alpha = false;
    std::uint64_t m_version = 0;
};

struct TextureMap {
    std::shared_ptr<const Texture> texture;
    UvTransform transform;
    TextureFilter filter = TextureFilter::bilinear;
    TextureWrap wrap = TextureWrap::repeat;
    int channel = 0;
    float strength = 1.0f;
    bool triplanar = false;
    float triplanar_scale = 1.0f;
    float triplanar_sharpness = 4.0f;

    TextureMap() = default;
    TextureMap(std::shared_ptr<const Texture> tex) : texture(std::move(tex)) {}
    TextureMap(std::shared_ptr<const Texture> tex, const UvTransform& xf) : texture(std::move(tex)), transform(xf) {}

    static TextureMap of(std::shared_ptr<const Texture> tex, int channel_index = 0);

    bool valid() const noexcept { return texture && texture->valid(); }
    Rgba sample(Vec2f uv) const noexcept;
    Rgba sample(Vec2f uv, const UvTransform& outer) const noexcept;
    Rgba sample_triplanar(const Vec3f& p, const Vec3f& n) const noexcept;
    float value(Vec2f uv, const UvTransform& outer) const noexcept;
};

Vec3f decode_normal(const Rgba& stored, float strength) noexcept;
Vec3f perturb_normal(const Vec3f& n, const Vec3f& tangent, const Vec3f& bitangent, const Vec3f& tangent_space) noexcept;
Vec3f triplanar_weights(const Vec3f& n, float sharpness) noexcept;

} // namespace lighting
} // namespace fizmo

#endif // FIZMO_LIGHTING_TEXTURE_HPP
