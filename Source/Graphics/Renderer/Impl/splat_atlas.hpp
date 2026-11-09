#ifndef FIZMO_SPLAT_ATLAS_HPP
#define FIZMO_SPLAT_ATLAS_HPP

#include "../../mesh.hpp"
#include "../../texture.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <memory>
#include <vector>

namespace fizmo {
namespace graphics {
namespace detail {

struct SplatAtlasLayout {
    int count  = 0;
    int cols   = 0;
    int rows   = 0;
    int tile   = 0;
    int gutter = 0;
    int width  = 0;
    int height = 0;

    int cell() const noexcept { return tile + 2 * gutter; }
    int max_lod() const noexcept {
        int lod = 0;
        for (int g = gutter; g > 1 && lod < 16; g >>= 1) ++lod;
        return lod;
    }
};

struct SplatAtlas {
    SplatAtlasLayout layout;
    Texture          albedo;
    Texture          normals;
    bool             has_normals = false;
};

inline SplatAtlasLayout splat_atlas_layout(int count, int tile = 512, int gutter = 64) noexcept {
    SplatAtlasLayout l;
    l.count  = (std::max)(1, (std::min)(count, Material3D::MAX_SPLAT_LAYERS));
    l.cols   = (std::min)(l.count, 4);
    l.rows   = (l.count + 3) / 4;
    l.tile   = (std::max)(tile, 4);
    l.gutter = (std::max)(0, (std::min)(gutter, l.tile / 2));
    l.width  = l.cols * l.cell();
    l.height = l.rows * l.cell();
    return l;
}

inline void splat_resample(const images::BitmapImage& img, int tile, std::vector<float>& out) {
    const int sw = static_cast<int>(img.width()), sh = static_cast<int>(img.height());
    out.assign(static_cast<std::size_t>(tile) * static_cast<std::size_t>(tile) * 4, 0.0f);
    if (sw <= 0 || sh <= 0) return;
    const std::vector<colors::Color>& px = img.pixels();
    const int kx = (std::max)(1, (sw + tile - 1) / tile), ky = (std::max)(1, (sh + tile - 1) / tile);
    const double fx = static_cast<double>(sw) / tile, fy = static_cast<double>(sh) / tile;
    const float inv = 1.0f / (255.0f * static_cast<float>(kx * ky));
    auto wrap = [](int c, int n) { c %= n; return c < 0 ? c + n : c; };

    for (int y = 0; y < tile; ++y) {
        for (int x = 0; x < tile; ++x) {
            float acc[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
            for (int j = 0; j < ky; ++j) {
                for (int i = 0; i < kx; ++i) {
                    const double sx = (x + (i + 0.5) / kx) * fx - 0.5, sy = (y + (j + 0.5) / ky) * fy - 0.5;
                    const int x0 = static_cast<int>(std::floor(sx)), y0 = static_cast<int>(std::floor(sy));
                    const float ax = static_cast<float>(sx - x0), ay = static_cast<float>(sy - y0);
                    const int xa = wrap(x0, sw), xb = wrap(x0 + 1, sw), ya = wrap(y0, sh), yb = wrap(y0 + 1, sh);
                    const colors::Color& c00 = px[static_cast<std::size_t>(ya) * sw + xa];
                    const colors::Color& c10 = px[static_cast<std::size_t>(ya) * sw + xb];
                    const colors::Color& c01 = px[static_cast<std::size_t>(yb) * sw + xa];
                    const colors::Color& c11 = px[static_cast<std::size_t>(yb) * sw + xb];
                    const float w00 = (1.0f - ax) * (1.0f - ay), w10 = ax * (1.0f - ay), w01 = (1.0f - ax) * ay, w11 = ax * ay;
                    acc[0] += c00.red() * w00 + c10.red() * w10 + c01.red() * w01 + c11.red() * w11;
                    acc[1] += c00.green() * w00 + c10.green() * w10 + c01.green() * w01 + c11.green() * w11;
                    acc[2] += c00.blue() * w00 + c10.blue() * w10 + c01.blue() * w01 + c11.blue() * w11;
                    acc[3] += c00.alpha() * w00 + c10.alpha() * w10 + c01.alpha() * w01 + c11.alpha() * w11;
                }
            }
            float* o = &out[(static_cast<std::size_t>(y) * tile + x) * 4];
            for (int k = 0; k < 4; ++k) o[k] = acc[k] * inv;
        }
    }
}

inline void splat_heights(std::vector<float>& tile) {
    const std::size_t n = tile.size() / 4;
    if (n == 0) return;
    int hist[256] = {};
    for (std::size_t i = 0; i < n; ++i) {
        const float* p = &tile[i * 4];
        const float l = 0.299f * p[0] + 0.587f * p[1] + 0.114f * p[2];
        tile[i * 4 + 3] = l;
        ++hist[(std::min)(255, (std::max)(0, static_cast<int>(l * 255.0f)))];
    }
    const std::size_t cut = n / 50;
    std::size_t seen = 0;
    int lo = 0, hi = 255;
    for (int b = 0; b < 256; ++b) { seen += static_cast<std::size_t>(hist[b]); if (seen > cut) { lo = b; break; } }
    seen = 0;
    for (int b = 255; b >= 0; --b) { seen += static_cast<std::size_t>(hist[b]); if (seen > cut) { hi = b; break; } }
    const float a = lo / 255.0f, range = (std::max)(1.0f / 255.0f, (hi - lo) / 255.0f);
    for (std::size_t i = 0; i < n; ++i) tile[i * 4 + 3] = (std::min)(1.0f, (std::max)(0.0f, (tile[i * 4 + 3] - a) / range));
}

inline std::uint8_t splat_byte(float v) noexcept {
    return static_cast<std::uint8_t>(std::lround((std::min)(1.0f, (std::max)(0.0f, v)) * 255.0f));
}

inline void splat_place(std::vector<colors::Color>& dst, const SplatAtlasLayout& l, int layer, const std::vector<float>* tile, const colors::Color& fill) {
    const int cell = l.cell(), ox = (layer % l.cols) * cell, oy = (layer / l.cols) * cell, t = l.tile;
    for (int y = 0; y < cell; ++y) {
        const int ty = ((y - l.gutter) % t + t) % t;
        colors::Color* row = &dst[static_cast<std::size_t>(oy + y) * l.width + ox];
        for (int x = 0; x < cell; ++x) {
            if (!tile) { row[x] = fill; continue; }
            const int tx = ((x - l.gutter) % t + t) % t;
            const float* p = &(*tile)[(static_cast<std::size_t>(ty) * t + tx) * 4];
            row[x] = colors::Color(splat_byte(p[0]), splat_byte(p[1]), splat_byte(p[2]), splat_byte(p[3]));
        }
    }
}

inline int splat_tile_size(const Texture* const* layers, int count, int cap = 512) noexcept {
    int best = 64;
    for (int i = 0; i < count; ++i) {
        if (!layers[i] || !layers[i]->valid()) continue;
        const int s = static_cast<int>((std::max)(layers[i]->width(), layers[i]->height()));
        while (best < s && best < cap) best *= 2;
    }
    return (std::min)(best, cap);
}

inline void build_splat_atlas(const Texture* const* layers, const Texture* const* normals, int count, SplatAtlas& out, int tile_cap = 512) {
    const int tile = splat_tile_size(layers, count, tile_cap);
    out.layout = splat_atlas_layout(count, tile, tile / 8);
    const SplatAtlasLayout& l = out.layout;
    std::vector<colors::Color> albedo(static_cast<std::size_t>(l.width) * l.height, colors::Color(255, 255, 255, 128));
    std::vector<float> scratch;

    for (int i = 0; i < l.count; ++i) {
        const Texture* t = i < count ? layers[i] : nullptr;
        if (t && t->valid()) {
            splat_resample(*t->image(), l.tile, scratch);
            splat_heights(scratch);
            splat_place(albedo, l, i, &scratch, colors::Color());
        } else {
            splat_place(albedo, l, i, nullptr, colors::Color(255, 255, 255, 128));
        }
    }

    out.has_normals = false;
    for (int i = 0; i < count && i < l.count; ++i) if (normals && normals[i] && normals[i]->valid()) out.has_normals = true;
    out.albedo = Texture(images::BitmapImage(static_cast<unsigned int>(l.width), static_cast<unsigned int>(l.height), std::move(albedo)), SampleFilter::Bilinear, WrapMode::Clamp);
    out.albedo.set_mipmaps(true);
    out.albedo.set_color_space(ColorSpace::Linear);

    if (!out.has_normals) {
        out.normals = Texture();
        return;
    }

    const colors::Color flat(128, 128, 255, 255);
    std::vector<colors::Color> nm(static_cast<std::size_t>(l.width) * l.height, flat);

    for (int i = 0; i < l.count; ++i) {
        const Texture* t = i < count ? normals[i] : nullptr;
        if (t && t->valid()) {
            splat_resample(*t->image(), l.tile, scratch);
            splat_place(nm, l, i, &scratch, flat);
        } else {
            splat_place(nm, l, i, nullptr, flat);
        }
    }

    out.normals = Texture(images::BitmapImage(static_cast<unsigned int>(l.width), static_cast<unsigned int>(l.height), std::move(nm)), SampleFilter::Bilinear, WrapMode::Clamp);
    out.normals.set_mipmaps(true);
    out.normals.set_color_space(ColorSpace::Linear);
}

inline bool splat_weights_at(const Material3D& m, float u, float v, float out[Material3D::MAX_SPLAT_LAYERS]) noexcept {
    for (int i = 0; i < Material3D::MAX_SPLAT_LAYERS; ++i) out[i] = 0.0f;
    const int count = m.splat_layers();
    if (count <= 0) return false;
    const Texture* maps[2] = { m.splat_map, count > 4 ? m.splat_map_b : nullptr };
    float total = 0.0f;
    for (int k = 0; k < 2; ++k) {
        if (!maps[k] || !maps[k]->valid()) continue;
        const double w = maps[k]->width(), h = maps[k]->height();
        const double cu = (std::min)(1.0, (std::max)(0.0, static_cast<double>(u))), cv = (std::min)(1.0, (std::max)(0.0, static_cast<double>(v)));
        const colors::Color c = maps[k]->sample(cu * w, cv * h);
        const std::uint8_t ch[4] = { c.red(), c.green(), c.blue(), c.alpha() };
        for (int j = 0; j < 4 && k * 4 + j < count; ++j) { out[k * 4 + j] = ch[j] / 255.0f; total += out[k * 4 + j]; }
    }
    if (total <= 1e-5f) { out[0] = 1.0f; return true; }
    for (int i = 0; i < count; ++i) out[i] /= total;
    return true;
}

} // namespace detail
} // namespace graphics
} // namespace fizmo

#endif // FIZMO_SPLAT_ATLAS_HPP
