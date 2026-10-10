#ifndef FIZMO_TEXTURE_HPP
#define FIZMO_TEXTURE_HPP

#include "../Graphics/color.hpp"
#include "../Images/Bitmap/image.hpp"
#include <memory>
#include <algorithm>
#include <cmath>

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