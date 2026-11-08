#include "renderer.hpp"

#include <cstdio>
#include <fstream>

namespace fizmo {
namespace lighting {

namespace {

float aces(float x) noexcept {
    const float a = 2.51f, b = 0.03f, c = 2.43f, d = 0.59f, e = 0.14f;
    return clamp01((x * (a * x + b)) / (x * (c * x + d) + e));
}

float hable_partial(float x) noexcept {
    const float a = 0.15f, b = 0.50f, c = 0.10f, d = 0.20f, e = 0.02f, f = 0.30f;
    return ((x * (a * x + c * b) + d * e) / (x * (a * x + b) + d * f)) - e / f;
}

float agx_curve(float x) noexcept {
    const float x2 = x * x, x4 = x2 * x2;
    return 15.5f * x4 * x2 - 40.14f * x4 * x + 31.96f * x4 - 6.868f * x2 * x + 0.4298f * x2 + 0.1191f * x - 0.00232f;
}

Rgb agx(const Rgb& c) noexcept {
    const Rgb in{ 0.842479062253094f * c.r + 0.0423282422610123f * c.g + 0.0423756549057051f * c.b,
                  0.0784335999999992f * c.r + 0.878468636469772f * c.g + 0.0784336f * c.b,
                  0.0792237451477643f * c.r + 0.0791661274605434f * c.g + 0.879142973793104f * c.b };
    const float lo = -12.47393f, hi = 4.026069f;
    Rgb v;
    for (int i = 0; i < 3; ++i) {
        const float l = std::log2(std::max(in[i], 1e-10f));
        v[i] = agx_curve(clamp01((l - lo) / (hi - lo)));
    }
    const Rgb out{ 1.19687900512017f * v.r - 0.0528968517574562f * v.g - 0.0529716355144438f * v.b,
                   -0.0980208811401368f * v.r + 1.15190312990417f * v.g - 0.0980434501171241f * v.b,
                   -0.0990297440797205f * v.r - 0.0989611768448433f * v.g + 1.15107367264116f * v.b };
    Rgb lin;
    for (int i = 0; i < 3; ++i) lin[i] = std::pow(std::max(out[i], 0.0f), 2.2f);
    return lin;
}

} // namespace

Rgb tone_map(const Rgb& hdr_in, const LightingSettings& s) noexcept {
    Rgb hdr = hdr_in.is_finite() ? hdr_in : Rgb();
    hdr = Rgb(std::max(hdr.r, 0.0f), std::max(hdr.g, 0.0f), std::max(hdr.b, 0.0f)) * s.exposure;
    if (s.white_balance > 0.0f) {
        const Rgb w = Rgb::from_kelvin(s.white_balance);
        hdr = Rgb(hdr.r / std::max(w.r, 1e-4f), hdr.g / std::max(w.g, 1e-4f), hdr.b / std::max(w.b, 1e-4f));
    }
    if (s.saturation != 1.0f) {
        const float y = hdr.luminance();
        hdr = Rgb(y) + (hdr - Rgb(y)) * s.saturation;
        hdr = Rgb(std::max(hdr.r, 0.0f), std::max(hdr.g, 0.0f), std::max(hdr.b, 0.0f));
    }
    Rgb out;
    switch (s.tone_mapper) {
    case ToneMapper::none: out = hdr.clamped(0.0f, 1.0f); break;
    case ToneMapper::reinhard: out = Rgb(hdr.r / (1.0f + hdr.r), hdr.g / (1.0f + hdr.g), hdr.b / (1.0f + hdr.b)); break;
    case ToneMapper::aces: out = Rgb(aces(hdr.r * 0.6f), aces(hdr.g * 0.6f), aces(hdr.b * 0.6f)); break;
    case ToneMapper::filmic: {
        const float white = 1.0f / hable_partial(11.2f);
        out = Rgb(hable_partial(hdr.r * 2.0f) * white, hable_partial(hdr.g * 2.0f) * white, hable_partial(hdr.b * 2.0f) * white);
        break;
    }
    case ToneMapper::agx: out = agx(hdr); break;
    }
    if (s.contrast != 1.0f) {
        for (int i = 0; i < 3; ++i) out[i] = clamp01((out[i] - 0.18f) * s.contrast + 0.18f);
    }
    return out.clamped(0.0f, 1.0f);
}

Rgb Image::average() const noexcept {
    Rgb sum;
    for (const auto& p : m_pixels) sum += p;
    return m_pixels.empty() ? Rgb() : sum / static_cast<float>(m_pixels.size());
}

std::vector<std::uint8_t> Image::to_rgba8(const LightingSettings& s) const {
    std::vector<std::uint8_t> out(m_pixels.size() * 4);
    for (std::size_t i = 0; i < m_pixels.size(); ++i) {
        const Rgb c = tone_map(m_pixels[i], s);
        if (s.srgb_output) {
            c.to_srgb8(out[i * 4], out[i * 4 + 1], out[i * 4 + 2]);
        } else {
            const float g = 1.0f / std::max(s.gamma, 0.1f);
            out[i * 4] = static_cast<std::uint8_t>(std::lround(std::pow(c.r, g) * 255.0f));
            out[i * 4 + 1] = static_cast<std::uint8_t>(std::lround(std::pow(c.g, g) * 255.0f));
            out[i * 4 + 2] = static_cast<std::uint8_t>(std::lround(std::pow(c.b, g) * 255.0f));
        }
        out[i * 4 + 3] = 255;
    }
    return out;
}

bool Image::write_ppm(const std::string& path, const LightingSettings& s) const {
    std::ofstream f(path, std::ios::binary);
    if (!f) return false;
    f << "P6\n" << m_width << " " << m_height << "\n255\n";
    const auto rgba = to_rgba8(s);
    for (std::size_t i = 0; i < m_pixels.size(); ++i) f.write(reinterpret_cast<const char*>(&rgba[i * 4]), 3);
    return static_cast<bool>(f);
}

bool Image::write_pfm(const std::string& path) const {
    std::ofstream f(path, std::ios::binary);
    if (!f) return false;
    f << "PF\n" << m_width << " " << m_height << "\n-1.0\n";
    for (unsigned y = m_height; y-- > 0;) {
        for (unsigned x = 0; x < m_width; ++x) {
            const Rgb& c = at(x, y);
            const float v[3] = { c.r, c.g, c.b };
            f.write(reinterpret_cast<const char*>(v), sizeof(v));
        }
    }
    return static_cast<bool>(f);
}

} // namespace lighting
} // namespace fizmo
