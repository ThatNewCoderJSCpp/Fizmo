#include "fizmo_library.hpp"
#include "png_codec.hpp"

#include <zlib.h>

namespace fizmo {
namespace images {
namespace png_detail {

std::uint32_t be32(const std::uint8_t* p) noexcept {
    return (static_cast<std::uint32_t>(p[0]) << 24) | (static_cast<std::uint32_t>(p[1]) << 16) | (static_cast<std::uint32_t>(p[2]) << 8) | p[3];
}

void put32(std::vector<std::uint8_t>& out, std::uint32_t v) {
    out.push_back(static_cast<std::uint8_t>(v >> 24));
    out.push_back(static_cast<std::uint8_t>(v >> 16));
    out.push_back(static_cast<std::uint8_t>(v >> 8));
    out.push_back(static_cast<std::uint8_t>(v));
}

std::uint8_t paeth(int a, int b, int c) noexcept {
    const int p = a + b - c;
    const int pa = std::abs(p - a), pb = std::abs(p - b), pc = std::abs(p - c);
    if (pa <= pb && pa <= pc) return static_cast<std::uint8_t>(a);
    if (pb <= pc) return static_cast<std::uint8_t>(b);
    return static_cast<std::uint8_t>(c);
}

bool unfilter(std::uint8_t* data, std::size_t rows, std::size_t stride, std::size_t bpp, std::uint8_t* out) {
    std::vector<std::uint8_t> zero(stride, 0);
    const std::uint8_t* prev = zero.data();

    for (std::size_t r = 0; r < rows; ++r) {
        const std::uint8_t filter = data[r * (stride + 1)];
        const std::uint8_t* in = data + r * (stride + 1) + 1;
        std::uint8_t* cur = out + r * stride;

        for (std::size_t i = 0; i < stride; ++i) {
            const int a = i >= bpp ? cur[i - bpp] : 0;
            const int b = prev[i];
            const int c = i >= bpp ? prev[i - bpp] : 0;
            int v = in[i];
            switch (filter) {
                case 0: break;
                case 1: v += a; break;
                case 2: v += b; break;
                case 3: v += (a + b) >> 1; break;
                case 4: v += paeth(a, b, c); break;
                default: return false;
            }
            cur[i] = static_cast<std::uint8_t>(v);
        }

        prev = cur;
    }

    return true;
}

} // namespace png_detail
} // namespace images
} // namespace fizmo

namespace fizmo {
namespace images {

bool is_png(const std::uint8_t* data, std::size_t size) noexcept {
    static const std::uint8_t sig[8] = { 137, 80, 78, 71, 13, 10, 26, 10 };
    return size >= 8 && std::memcmp(data, sig, 8) == 0;
}

bool decode_png(const std::uint8_t* data, std::size_t size, BitmapImage& out, std::string* error) {
    auto fail = [&](const char* why) { if (error) *error = why; return false; };
    if (!is_png(data, size)) return fail("not a PNG file");
    std::size_t pos = 8;
    std::uint32_t width = 0, height = 0;
    int depth = 0, color = 0, interlace = 0;
    std::vector<std::uint8_t> idat;
    std::vector<std::array<std::uint8_t, 4>> palette;
    bool have_ihdr = false, have_trns = false;
    std::uint16_t trns_gray = 0, trns_r = 0, trns_g = 0, trns_b = 0;

    while (pos + 12 <= size) {
        const std::uint32_t len = png_detail::be32(data + pos);
        const char* type = reinterpret_cast<const char*>(data + pos + 4);
        if (len > size - pos - 12) return fail("truncated PNG chunk");
        const std::uint8_t* body = data + pos + 8;

        if (!std::memcmp(type, "IHDR", 4)) {
            if (len < 13) return fail("bad IHDR");
            width = png_detail::be32(body);
            height = png_detail::be32(body + 4);
            depth = body[8];
            color = body[9];
            interlace = body[12];
            have_ihdr = true;
        } else if (!std::memcmp(type, "PLTE", 4)) {
            palette.clear();
            for (std::uint32_t i = 0; i + 2 < len; i += 3) palette.push_back({ body[i], body[i + 1], body[i + 2], 255 });
        } else if (!std::memcmp(type, "tRNS", 4)) {
            have_trns = true;
            if (color == 3) { for (std::uint32_t i = 0; i < len && i < palette.size(); ++i) palette[i][3] = body[i]; }
            else if (color == 0 && len >= 2) trns_gray = static_cast<std::uint16_t>((body[0] << 8) | body[1]);
            else if (color == 2 && len >= 6) { trns_r = static_cast<std::uint16_t>((body[0] << 8) | body[1]); trns_g = static_cast<std::uint16_t>((body[2] << 8) | body[3]); trns_b = static_cast<std::uint16_t>((body[4] << 8) | body[5]); }
        } else if (!std::memcmp(type, "IDAT", 4)) {
            idat.insert(idat.end(), body, body + len);
        } else if (!std::memcmp(type, "IEND", 4)) {
            break;
        }

        pos += 12 + len;
    }

    if (!have_ihdr || width == 0 || height == 0) return fail("missing IHDR");
    if (width > 32768 || height > 32768) return fail("PNG too large");
    int channels = 0;
    switch (color) {
        case 0: channels = 1; break;
        case 2: channels = 3; break;
        case 3: channels = 1; break;
        case 4: channels = 2; break;
        case 6: channels = 4; break;
        default: return fail("unsupported PNG colour type");
    }
    if (depth != 1 && depth != 2 && depth != 4 && depth != 8 && depth != 16) return fail("unsupported PNG bit depth");
    if (color == 3 && palette.empty()) return fail("palette PNG without PLTE");
    const std::size_t bits_pp = static_cast<std::size_t>(channels) * depth;
    const std::size_t bpp = std::max<std::size_t>(1, bits_pp / 8);

    struct Pass { int x0, y0, dx, dy; };
    const Pass adam7[7] = { { 0, 0, 8, 8 }, { 4, 0, 8, 8 }, { 0, 4, 4, 8 }, { 2, 0, 4, 4 }, { 0, 2, 2, 4 }, { 1, 0, 2, 2 }, { 0, 1, 1, 2 } };
    const Pass whole[1] = { { 0, 0, 1, 1 } };
    const Pass* passes = interlace ? adam7 : whole;
    const int pass_count = interlace ? 7 : 1;
    std::size_t total = 0;

    for (int p = 0; p < pass_count; ++p) {
        const std::size_t pw = width > static_cast<std::uint32_t>(passes[p].x0) ? (width - passes[p].x0 + passes[p].dx - 1) / passes[p].dx : 0;
        const std::size_t ph = height > static_cast<std::uint32_t>(passes[p].y0) ? (height - passes[p].y0 + passes[p].dy - 1) / passes[p].dy : 0;
        if (pw && ph) total += ph * ((pw * bits_pp + 7) / 8 + 1);
    }

    std::vector<std::uint8_t> raw(total);
    z_stream zs;
    std::memset(&zs, 0, sizeof(zs));
    if (inflateInit(&zs) != Z_OK) return fail("zlib init failed");
    zs.next_in = idat.data();
    zs.avail_in = static_cast<uInt>(idat.size());
    zs.next_out = raw.data();
    zs.avail_out = static_cast<uInt>(raw.size());
    const int zr = inflate(&zs, Z_FINISH);
    const std::size_t produced = zs.total_out;
    inflateEnd(&zs);
    if ((zr != Z_STREAM_END && zr != Z_BUF_ERROR) || produced < total) return fail("corrupt PNG image data");

    out = BitmapImage(width, height);
    std::size_t offset = 0;
    std::vector<std::uint8_t> rows;

    for (int p = 0; p < pass_count; ++p) {
        const Pass& ps = passes[p];
        const std::size_t pw = width > static_cast<std::uint32_t>(ps.x0) ? (width - ps.x0 + ps.dx - 1) / ps.dx : 0;
        const std::size_t ph = height > static_cast<std::uint32_t>(ps.y0) ? (height - ps.y0 + ps.dy - 1) / ps.dy : 0;
        if (!pw || !ph) continue;
        const std::size_t stride = (pw * bits_pp + 7) / 8;
        rows.assign(stride * ph, 0);
        if (!png_detail::unfilter(raw.data() + offset, ph, stride, bpp, rows.data())) return fail("bad PNG filter");
        offset += ph * (stride + 1);

        for (std::size_t y = 0; y < ph; ++y) {
            const std::uint8_t* row = rows.data() + y * stride;
            for (std::size_t x = 0; x < pw; ++x) {
                auto sample = [&](int ch) -> std::uint16_t {
                    if (depth == 8) return row[x * channels + ch];
                    if (depth == 16) return static_cast<std::uint16_t>((row[(x * channels + ch) * 2] << 8) | row[(x * channels + ch) * 2 + 1]);
                    const std::size_t bit = (x * channels + ch) * depth;
                    return static_cast<std::uint16_t>((row[bit / 8] >> (8 - depth - bit % 8)) & ((1 << depth) - 1));
                };
                auto to8 = [&](std::uint16_t v) -> std::uint8_t {
                    if (depth == 16) return static_cast<std::uint8_t>(v >> 8);
                    if (depth == 8) return static_cast<std::uint8_t>(v);
                    return static_cast<std::uint8_t>(v * 255 / ((1 << depth) - 1));
                };
                std::uint8_t r = 0, g = 0, b = 0, a = 255;

                switch (color) {
                    case 0: {
                        const std::uint16_t v = sample(0);
                        r = g = b = to8(v);
                        if (have_trns && v == trns_gray) a = 0;
                        break;
                    }
                    case 2: {
                        const std::uint16_t vr = sample(0), vg = sample(1), vb = sample(2);
                        r = to8(vr); g = to8(vg); b = to8(vb);
                        if (have_trns && vr == trns_r && vg == trns_g && vb == trns_b) a = 0;
                        break;
                    }
                    case 3: {
                        const std::uint16_t i = sample(0);
                        if (i < palette.size()) { r = palette[i][0]; g = palette[i][1]; b = palette[i][2]; a = palette[i][3]; }
                        break;
                    }
                    case 4: r = g = b = to8(sample(0)); a = to8(sample(1)); break;
                    case 6: r = to8(sample(0)); g = to8(sample(1)); b = to8(sample(2)); a = to8(sample(3)); break;
                }

                out.set_pixel(static_cast<unsigned int>(ps.x0 + x * ps.dx), static_cast<unsigned int>(ps.y0 + y * ps.dy), graphics::Color(r, g, b, a));
            }
        }
    }

    return true;
}

bool encode_png(const BitmapImage& img, std::vector<std::uint8_t>& out, bool alpha, int level) {
    out.clear();
    if (!img.is_valid_image()) return false;
    const unsigned int w = img.width(), h = img.height();
    const std::size_t channels = alpha ? 4 : 3;
    const std::size_t stride = w * channels;
    std::vector<std::uint8_t> raw((stride + 1) * h);
    std::vector<std::uint8_t> line(stride), prev(stride, 0), trial(stride), best(stride);

    for (unsigned int y = 0; y < h; ++y) {
        for (unsigned int x = 0; x < w; ++x) {
            const graphics::Color c = img.get_pixel(x, y);
            std::uint8_t* p = &line[x * channels];
            p[0] = c.red(); p[1] = c.green(); p[2] = c.blue();
            if (alpha) p[3] = c.alpha();
        }

        std::uint64_t best_cost = ~0ull;
        std::uint8_t best_filter = 0;
        for (std::uint8_t f = 0; f < 5; ++f) {
            std::uint64_t cost = 0;
            for (std::size_t i = 0; i < stride; ++i) {
                const int a = i >= channels ? line[i - channels] : 0, b = prev[i], c = i >= channels ? prev[i - channels] : 0;
                int pred = 0;
                switch (f) { case 1: pred = a; break; case 2: pred = b; break; case 3: pred = (a + b) >> 1; break; case 4: pred = png_detail::paeth(a, b, c); break; default: break; }
                trial[i] = static_cast<std::uint8_t>(line[i] - pred);
                cost += static_cast<std::uint64_t>(std::abs(static_cast<int>(static_cast<std::int8_t>(trial[i]))));
            }
            if (cost < best_cost) { best_cost = cost; best_filter = f; best = trial; }
        }

        raw[y * (stride + 1)] = best_filter;
        std::memcpy(&raw[y * (stride + 1) + 1], best.data(), stride);
        prev = line;
    }

    uLongf bound = compressBound(static_cast<uLong>(raw.size()));
    std::vector<std::uint8_t> z(bound);
    if (compress2(z.data(), &bound, raw.data(), static_cast<uLong>(raw.size()), level) != Z_OK) return false;
    z.resize(bound);
    static const std::uint8_t sig[8] = { 137, 80, 78, 71, 13, 10, 26, 10 };
    out.assign(sig, sig + 8);

    auto chunk = [&](const char* type, const std::uint8_t* body, std::size_t len) {
        png_detail::put32(out, static_cast<std::uint32_t>(len));
        const std::size_t start = out.size();
        out.insert(out.end(), type, type + 4);
        if (len) out.insert(out.end(), body, body + len);
        png_detail::put32(out, static_cast<std::uint32_t>(crc32(0, out.data() + start, static_cast<uInt>(len + 4))));
    };

    std::uint8_t ihdr[13];
    ihdr[0] = static_cast<std::uint8_t>(w >> 24); ihdr[1] = static_cast<std::uint8_t>(w >> 16); ihdr[2] = static_cast<std::uint8_t>(w >> 8); ihdr[3] = static_cast<std::uint8_t>(w);
    ihdr[4] = static_cast<std::uint8_t>(h >> 24); ihdr[5] = static_cast<std::uint8_t>(h >> 16); ihdr[6] = static_cast<std::uint8_t>(h >> 8); ihdr[7] = static_cast<std::uint8_t>(h);
    ihdr[8] = 8; ihdr[9] = alpha ? 6 : 2; ihdr[10] = 0; ihdr[11] = 0; ihdr[12] = 0;
    chunk("IHDR", ihdr, 13);
    chunk("IDAT", z.data(), z.size());
    chunk("IEND", nullptr, 0);
    return true;
}

} // namespace images
} // namespace fizmo
