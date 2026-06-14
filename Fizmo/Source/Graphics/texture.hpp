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
    ) noexcept : m_image(std::make_shared<fizmo::images::BitmapImage>(img)), m_filter(filter), m_wrap(wrap) {}

    explicit Texture(
        fizmo::images::BitmapImage&& img,
        SampleFilter filter = SampleFilter::Nearest,
        WrapMode wrap = WrapMode::Clamp
    ) noexcept : m_image(std::make_shared<fizmo::images::BitmapImage>(std::move(img))), m_filter(filter), m_wrap(wrap) {}

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

    Color sample(int x, int y) const noexcept {
        if (!m_image) return {};
        int w = static_cast<int>(m_image->width());
        int h = static_cast<int>(m_image->height());
        x = wrap_coord(x, w);
        y = wrap_coord(y, h);
        return m_image->get_pixel(static_cast<unsigned int>(x), static_cast<unsigned int>(y));
    }

    Color sample(double u, double v) const noexcept {
        if (!m_image) return {};
        if (m_filter == SampleFilter::Bilinear) return sample_bilinear(u, v);
        return sample_nearest(u, v);
    }

    Color sample_uv(double u, double v) const noexcept {
        if (!m_image) return {};
        double px = u * (m_image->width()  - 1);
        double py = v * (m_image->height() - 1);
        return sample(px, py);
    }

private:
    int wrap_coord(int c, int size) const noexcept {
        if (size <= 0) return 0;

        switch (m_wrap) {
            case WrapMode::Repeat:
                c = c % size;
                if (c < 0) c += size;
                return c;
            case WrapMode::MirrorRepeat: {
                if (c < 0) c = -c;
                int period = c / size;
                c = c % size;
                if (period & 1) c = size - 1 - c;
                return c;
            }
            case WrapMode::Clamp:
            default:
                return std::max(0, std::min(c, size - 1));
        }
    }

    Color sample_nearest(double px, double py) const noexcept {
        int x = static_cast<int>(std::round(px));
        int y = static_cast<int>(std::round(py));
        return sample(x, y);
    }

    Color sample_bilinear(double px, double py) const noexcept {
        int w = static_cast<int>(m_image->width());
        int h = static_cast<int>(m_image->height());
        int x0 = static_cast<int>(std::floor(px));
        int y0 = static_cast<int>(std::floor(py));
        int x1 = x0 + 1;
        int y1 = y0 + 1;
        double fx = px - x0;
        double fy = py - y0;
        auto c00 = sample(x0, y0);
        auto c10 = sample(x1, y0);
        auto c01 = sample(x0, y1);
        auto c11 = sample(x1, y1);
        auto lerp_ch = [](double a, double b, double t) -> std::uint8_t { return static_cast<std::uint8_t>(std::round(a + (b - a) * t)); };
        double r0 = c00.red()   + (c10.red()   - static_cast<double>(c00.red()))   * fx;
        double r1 = c01.red()   + (c11.red()   - static_cast<double>(c01.red()))   * fx;
        double g0 = c00.green() + (c10.green() - static_cast<double>(c00.green())) * fx;
        double g1 = c01.green() + (c11.green() - static_cast<double>(c01.green())) * fx;
        double b0 = c00.blue()  + (c10.blue()  - static_cast<double>(c00.blue()))  * fx;
        double b1 = c01.blue()  + (c11.blue()  - static_cast<double>(c01.blue()))  * fx;
        double a0 = c00.alpha() + (c10.alpha() - static_cast<double>(c00.alpha())) * fx;
        double a1 = c01.alpha() + (c11.alpha() - static_cast<double>(c01.alpha())) * fx;

        return Color(
            lerp_ch(r0, r1, fy),
            lerp_ch(g0, g1, fy),
            lerp_ch(b0, b1, fy),
            lerp_ch(a0, a1, fy)
        );
    }
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