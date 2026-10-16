#include "fizmo_library.hpp"
#include "format_util.hpp"

#include <cmath>

namespace fizmo {
namespace images {

namespace {

struct TextScanner {
    const std::uint8_t* p;
    std::size_t         n;
    std::size_t         i;

    void skip_space() {
        while (i < n) {
            if (p[i] == '#') {
                while (i < n && p[i] != '\n' && p[i] != '\r') ++i;
            } else if (p[i] == ' ' || p[i] == '\t' || p[i] == '\n' || p[i] == '\r' || p[i] == '\v' || p[i] == '\f') {
                ++i;
            } else {
                break;
            }
        }
    }

    bool number(std::uint64_t& v) {
        skip_space();
        if (i >= n || p[i] < '0' || p[i] > '9') return false;
        v = 0;
        while (i < n && p[i] >= '0' && p[i] <= '9') {
            v = v * 10 + (p[i] - '0');
            if (v > 0xFFFFFFFFull) return false;
            ++i;
        }
        return true;
    }

    bool bit(unsigned& v) {
        skip_space();
        if (i >= n || (p[i] != '0' && p[i] != '1')) return false;
        v = p[i++] - '0';
        return true;
    }

    std::string token() {
        skip_space();
        std::string s;
        while (i < n && p[i] > ' ') s.push_back(static_cast<char>(p[i++]));
        return s;
    }

    std::string line_rest() {
        while (i < n && (p[i] == ' ' || p[i] == '\t')) ++i;
        std::string s;
        while (i < n && p[i] != '\n' && p[i] != '\r') s.push_back(static_cast<char>(p[i++]));
        while (!s.empty() && (s.back() == ' ' || s.back() == '\t')) s.pop_back();
        return s;
    }
};

} // namespace

bool is_pnm(const std::uint8_t* data, std::size_t size) noexcept {
    if (size < 3 || data[0] != 'P') return false;
    const char c = static_cast<char>(data[1]);
    if (c == 'F' || c == 'f') return data[2] == '\n' || data[2] == '\r' || data[2] == ' ';
    return c >= '1' && c <= '7' && (data[2] == '\n' || data[2] == '\r' || data[2] == ' ' || data[2] == '\t' || data[2] == '#');
}

bool decode_pnm(const std::uint8_t* data, std::size_t size, BitmapImage& out, std::string* error) {
    if (!is_pnm(data, size)) return fmt::fail(error, "not a PNM file");
    TextScanner s{ data, size, 2 };
    const char kind = static_cast<char>(data[1]);
    std::uint64_t w = 0, h = 0, maxval = 1, depth = 0;

    if (kind == '7') {
        while (true) {
            s.skip_space();
            const std::string key = s.token();
            if (key.empty()) return fmt::fail(error, "bad PAM header");
            if (key == "ENDHDR") break;
            if (key == "WIDTH") { if (!s.number(w)) return fmt::fail(error, "bad PAM header"); }
            else if (key == "HEIGHT") { if (!s.number(h)) return fmt::fail(error, "bad PAM header"); }
            else if (key == "DEPTH") { if (!s.number(depth)) return fmt::fail(error, "bad PAM header"); }
            else if (key == "MAXVAL") { if (!s.number(maxval)) return fmt::fail(error, "bad PAM header"); }
            else s.line_rest();
        }
        if (s.i < size && data[s.i] == '\r') ++s.i;
        if (s.i < size && data[s.i] == '\n') ++s.i;
        if (depth < 1 || depth > 4) return fmt::fail(error, "unsupported PAM depth");
    } else if (kind == 'F' || kind == 'f') {
        const std::string ws = s.token(), hs = s.token(), sc = s.token();
        if (ws.empty() || hs.empty() || sc.empty()) return fmt::fail(error, "bad PFM header");
        w = std::strtoull(ws.c_str(), nullptr, 10);
        h = std::strtoull(hs.c_str(), nullptr, 10);
        const double scale = std::strtod(sc.c_str(), nullptr);
        if (s.i < size) ++s.i;
        if (!fmt::sane_dimensions(w, h)) return fmt::fail(error, "bad PFM dimensions");
        const unsigned ch = kind == 'F' ? 3 : 1;
        const std::size_t need = static_cast<std::size_t>(w * h) * ch * 4;
        if (size - s.i < need) return fmt::fail(error, "truncated PFM");
        const bool little = scale < 0;
        fmt::Rgba img(static_cast<unsigned>(w), static_cast<unsigned>(h));

        for (std::uint64_t y = 0; y < h; ++y)
            for (std::uint64_t x = 0; x < w; ++x) {
                std::uint8_t* d = img.at(static_cast<unsigned>(x), static_cast<unsigned>(h - 1 - y));
                for (unsigned c = 0; c < 3; ++c) {
                    const std::uint8_t* q = data + s.i + ((y * w + x) * ch + (ch == 3 ? c : 0)) * 4;
                    const std::uint32_t bits = little ? (q[0] | (q[1] << 8) | (static_cast<std::uint32_t>(q[2]) << 16) | (static_cast<std::uint32_t>(q[3]) << 24))
                                                      : ((static_cast<std::uint32_t>(q[0]) << 24) | (static_cast<std::uint32_t>(q[1]) << 16) | (q[2] << 8) | q[3]);
                    float f;
                    std::memcpy(&f, &bits, 4);
                    double v = std::isfinite(f) ? static_cast<double>(f) : (f > 0 ? 1.0 : 0.0);
                    v = v < 0 ? 0 : v > 1 ? 1 : v;
                    d[c] = static_cast<std::uint8_t>(std::lround(v * 255.0));
                }
                d[3] = 255;
            }

        out = img.to_bitmap();
        return true;
    } else {
        if (!s.number(w) || !s.number(h)) return fmt::fail(error, "bad PNM header");
        if (kind != '1' && kind != '4' && !s.number(maxval)) return fmt::fail(error, "bad PNM header");
        depth = (kind == '3' || kind == '6') ? 3 : 1;
        if (kind >= '4' && kind <= '6') {
            if (s.i >= size) return fmt::fail(error, "truncated PNM");
            ++s.i;
        }
    }

    if (!fmt::sane_dimensions(w, h)) return fmt::fail(error, "bad PNM dimensions");
    if (maxval == 0 || maxval > 65535) return fmt::fail(error, "bad PNM maxval");
    const unsigned W = static_cast<unsigned>(w), H = static_cast<unsigned>(h);
    fmt::Rgba img(W, H);
    const bool bitmap = kind == '1' || kind == '4';
    const unsigned D = static_cast<unsigned>(depth);

    auto store = [&](unsigned x, unsigned y, const std::uint64_t* v) {
        std::uint8_t* d = img.at(x, y);
        if (bitmap) {
            d[0] = d[1] = d[2] = v[0] ? 0 : 255;
            d[3] = 255;
            return;
        }
        auto sc = [&](std::uint64_t q) { return fmt::scale_to8(q, maxval); };
        if (D == 1) { d[0] = d[1] = d[2] = sc(v[0]); d[3] = 255; }
        else if (D == 2) { d[0] = d[1] = d[2] = sc(v[0]); d[3] = sc(v[1]); }
        else { d[0] = sc(v[0]); d[1] = sc(v[1]); d[2] = sc(v[2]); d[3] = D == 4 ? sc(v[3]) : 255; }
    };

    std::uint64_t v[4] = { 0, 0, 0, 0 };

    if (kind == '1') {
        for (unsigned y = 0; y < H; ++y)
            for (unsigned x = 0; x < W; ++x) {
                unsigned b;
                if (!s.bit(b)) return fmt::fail(error, "truncated PBM");
                v[0] = b;
                store(x, y, v);
            }
    } else if (kind == '2' || kind == '3') {
        for (unsigned y = 0; y < H; ++y)
            for (unsigned x = 0; x < W; ++x) {
                for (unsigned c = 0; c < D; ++c) if (!s.number(v[c])) return fmt::fail(error, "truncated PNM");
                store(x, y, v);
            }
    } else if (kind == '4') {
        const std::size_t stride = (W + 7) / 8;
        if (size - s.i < stride * H) return fmt::fail(error, "truncated PBM");
        for (unsigned y = 0; y < H; ++y)
            for (unsigned x = 0; x < W; ++x) {
                v[0] = (data[s.i + y * stride + x / 8] >> (7 - x % 8)) & 1u;
                store(x, y, v);
            }
    } else {
        const unsigned bps = maxval > 255 ? 2 : 1;
        const std::size_t need = static_cast<std::size_t>(W) * H * D * bps;
        if (size - s.i < need) return fmt::fail(error, "truncated PNM");
        const std::uint8_t* p = data + s.i;
        for (unsigned y = 0; y < H; ++y)
            for (unsigned x = 0; x < W; ++x) {
                for (unsigned c = 0; c < D; ++c) {
                    v[c] = bps == 2 ? ((p[0] << 8) | p[1]) : p[0];
                    p += bps;
                }
                store(x, y, v);
            }
    }

    out = img.to_bitmap();
    return true;
}

bool encode_pnm(const BitmapImage& img, std::vector<std::uint8_t>& out, bool alpha) {
    out.clear();
    if (!img.is_valid_image()) return false;
    const unsigned w = img.width(), h = img.height();
    std::string header;

    if (alpha) header = "P7\nWIDTH " + std::to_string(w) + "\nHEIGHT " + std::to_string(h) + "\nDEPTH 4\nMAXVAL 255\nTUPLTYPE RGB_ALPHA\nENDHDR\n";
    else header = "P6\n" + std::to_string(w) + " " + std::to_string(h) + "\n255\n";

    out.assign(header.begin(), header.end());
    out.reserve(out.size() + static_cast<std::size_t>(w) * h * (alpha ? 4 : 3));

    for (const colors::Color& c : img.pixels()) {
        out.push_back(c.red());
        out.push_back(c.green());
        out.push_back(c.blue());
        if (alpha) out.push_back(c.alpha());
    }

    return true;
}

} // namespace images
} // namespace fizmo
