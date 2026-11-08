#ifndef FIZMO_TEXTURE_HPP
#define FIZMO_TEXTURE_HPP

#include "color.hpp"
#include "../Images/Bitmap/image.hpp"
#include <memory>
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace fizmo {
namespace graphics {

enum class SampleFilter {
    Nearest = 0,
    Bilinear
};

enum class WrapMode {
    Clamp = 0,
    Repeat,
    MirrorRepeat
};

enum class ColorSpace : std::uint8_t {
    SRGB = 0,
    Linear
};

struct UvTransform {
    float scale_u  = 1.0f;
    float scale_v  = 1.0f;
    float offset_u = 0.0f;
    float offset_v = 0.0f;
    float rotation = 0.0f;

    static UvTransform tiling(float u, float v) noexcept { UvTransform t; t.scale_u = u; t.scale_v = v; return t; }
    static UvTransform make(float su, float sv, float ou, float ov, float rotation_radians = 0.0f) noexcept {
        UvTransform t; t.scale_u = su; t.scale_v = sv; t.offset_u = ou; t.offset_v = ov; t.rotation = rotation_radians; return t;
    }

    bool is_identity() const noexcept { return scale_u == 1.0f && scale_v == 1.0f && offset_u == 0.0f && offset_v == 0.0f && rotation == 0.0f; }

    void matrix(float out[6]) const noexcept {
        const float c = std::cos(rotation), s = std::sin(rotation);
        out[0] = c * scale_u; out[1] = -s * scale_v; out[2] = offset_u;
        out[3] = s * scale_u; out[4] = c * scale_v;  out[5] = offset_v;
    }

    void apply(float u, float v, float& out_u, float& out_v) const noexcept {
        float m[6];
        matrix(m);
        out_u = m[0] * u + m[1] * v + m[2];
        out_v = m[3] * u + m[4] * v + m[5];
    }

    static void compose(const UvTransform& first, const UvTransform& second, float out[6]) noexcept {
        float a[6], b[6];
        first.matrix(a);
        second.matrix(b);
        out[0] = b[0] * a[0] + b[1] * a[3];
        out[1] = b[0] * a[1] + b[1] * a[4];
        out[2] = b[0] * a[2] + b[1] * a[5] + b[2];
        out[3] = b[3] * a[0] + b[4] * a[3];
        out[4] = b[3] * a[1] + b[4] * a[4];
        out[5] = b[3] * a[2] + b[4] * a[5] + b[5];
    }
};

struct TextureRect {
    int x = 0;
    int y = 0;
    unsigned int w = 0;
    unsigned int h = 0;

    constexpr TextureRect() noexcept = default;
    constexpr TextureRect(int x, int y, unsigned int w, unsigned int h) noexcept : x(x), y(y), w(w), h(h) {}
    constexpr bool is_empty() const noexcept { return w == 0 || h == 0; }
};

class Texture {
private:
    std::shared_ptr<const fizmo::images::BitmapImage> m_image;
    SampleFilter m_filter  = SampleFilter::Nearest;
    WrapMode     m_wrap    = WrapMode::Clamp;
    ColorSpace   m_space   = ColorSpace::SRGB;
    bool         m_mipmaps = false;

public:
    Texture() noexcept = default;

    explicit Texture(
        const fizmo::images::BitmapImage& img,
        SampleFilter filter = SampleFilter::Nearest,
        WrapMode wrap = WrapMode::Clamp
    ) noexcept;

    explicit Texture(
        fizmo::images::BitmapImage&& img,
        SampleFilter filter = SampleFilter::Nearest,
        WrapMode wrap = WrapMode::Clamp
    ) noexcept;

    explicit Texture(
        std::shared_ptr<const fizmo::images::BitmapImage> img,
        SampleFilter filter = SampleFilter::Nearest,
        WrapMode wrap = WrapMode::Clamp
    ) noexcept : m_image(std::move(img)), m_filter(filter), m_wrap(wrap) {}

    bool valid() const noexcept { return m_image && m_image->is_valid_image(); }
    unsigned int width()  const noexcept { return m_image ? m_image->width()  : 0; }
    unsigned int height() const noexcept { return m_image ? m_image->height() : 0; }
    const fizmo::images::BitmapImage* image() const noexcept { return m_image.get(); }

    SampleFilter filter()  const noexcept { return m_filter; }
    WrapMode     wrap()    const noexcept { return m_wrap; }

    void set_filter(SampleFilter f)  noexcept { m_filter = f; }
    void set_wrap(WrapMode w)        noexcept { m_wrap = w; }

    ColorSpace color_space() const noexcept { return m_space; }
    bool       mipmaps()     const noexcept { return m_mipmaps; }
    bool       trilinear()   const noexcept { return m_mipmaps && m_filter == SampleFilter::Bilinear; }
    void set_color_space(ColorSpace space) noexcept { m_space = space; }
    void set_mipmaps(bool on)              noexcept { m_mipmaps = on; }

    const std::shared_ptr<const fizmo::images::BitmapImage>& shared_image() const noexcept { return m_image; }
    void set_image(std::shared_ptr<const fizmo::images::BitmapImage> img) noexcept { m_image = std::move(img); }

    Color sample_wrapped(double u, double v) const noexcept;

    TextureRect full_rect() const noexcept { return TextureRect(0, 0, width(), height()); }

    Color sample(int x, int y) const noexcept;

    Color sample(double u, double v) const noexcept;

    Color sample_uv(double u, double v) const noexcept;

private:
    int wrap_coord(int c, int size) const noexcept;

    Color sample_nearest(double px, double py) const noexcept {
        return sample(static_cast<int>(std::floor(px)), static_cast<int>(std::floor(py)));
    }

    Color sample_bilinear(double px, double py) const noexcept;
};

inline Texture make_texture(const fizmo::images::BitmapImage& img, SampleFilter filter = SampleFilter::Nearest, WrapMode wrap = WrapMode::Clamp) {
    return Texture(img, filter, wrap);
}

inline Texture make_texture(fizmo::images::BitmapImage&& img, SampleFilter filter = SampleFilter::Nearest, WrapMode wrap = WrapMode::Clamp) {
    return Texture(std::move(img), filter, wrap);
}

inline Texture make_texture(std::shared_ptr<const fizmo::images::BitmapImage> img, SampleFilter filter = SampleFilter::Nearest, WrapMode wrap = WrapMode::Clamp) {
    return Texture(std::move(img), filter, wrap);
}

} // namespace graphics
} // namespace fizmo

#endif // FIZMO_TEXTURE_HPP