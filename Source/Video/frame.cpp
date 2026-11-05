#include "frame.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace fizmo {
namespace video {

int bit_depth(PixelFormat format) noexcept {
    switch (format) {
    case PixelFormat::gray10:
    case PixelFormat::yuv420p10:
    case PixelFormat::yuv422p10:
    case PixelFormat::yuv444p10:
    case PixelFormat::yuv440p10:
        return 10;
    case PixelFormat::gray12:
    case PixelFormat::yuv420p12:
    case PixelFormat::yuv422p12:
    case PixelFormat::yuv444p12:
    case PixelFormat::yuv440p12:
        return 12;
    case PixelFormat::gray14:
    case PixelFormat::yuv420p14:
    case PixelFormat::yuv422p14:
    case PixelFormat::yuv444p14:
        return 14;
    case PixelFormat::none:
        return 0;
    default:
        return 8;
    }
}

int plane_count(PixelFormat format) noexcept {
    switch (format) {
    case PixelFormat::none:
        return 0;
    case PixelFormat::gray:
    case PixelFormat::gray10:
    case PixelFormat::gray12:
    case PixelFormat::gray14:
        return 1;
    default:
        return 3;
    }
}

int bytes_per_sample(PixelFormat format) noexcept {
    return bit_depth(format) > 8 ? 2 : 1;
}

int chroma_shift_x(PixelFormat format) noexcept {
    switch (format) {
    case PixelFormat::yuv420p:
    case PixelFormat::yuv420p10:
    case PixelFormat::yuv420p12:
    case PixelFormat::yuv420p14:
    case PixelFormat::yuv422p:
    case PixelFormat::yuv422p10:
    case PixelFormat::yuv422p12:
    case PixelFormat::yuv422p14:
        return 1;
    default:
        return 0;
    }
}

int chroma_shift_y(PixelFormat format) noexcept {
    switch (format) {
    case PixelFormat::yuv420p:
    case PixelFormat::yuv420p10:
    case PixelFormat::yuv420p12:
    case PixelFormat::yuv420p14:
    case PixelFormat::yuv440p:
    case PixelFormat::yuv440p10:
    case PixelFormat::yuv440p12:
        return 1;
    default:
        return 0;
    }
}

const char* format_name(PixelFormat format) noexcept {
    switch (format) {
    case PixelFormat::gray: return "gray";
    case PixelFormat::yuv420p: return "yuv420p";
    case PixelFormat::yuv422p: return "yuv422p";
    case PixelFormat::yuv444p: return "yuv444p";
    case PixelFormat::gray10: return "gray10";
    case PixelFormat::yuv420p10: return "yuv420p10";
    case PixelFormat::yuv422p10: return "yuv422p10";
    case PixelFormat::yuv444p10: return "yuv444p10";
    case PixelFormat::gray12: return "gray12";
    case PixelFormat::yuv420p12: return "yuv420p12";
    case PixelFormat::yuv422p12: return "yuv422p12";
    case PixelFormat::yuv444p12: return "yuv444p12";
    case PixelFormat::gray14: return "gray14";
    case PixelFormat::yuv420p14: return "yuv420p14";
    case PixelFormat::yuv422p14: return "yuv422p14";
    case PixelFormat::yuv444p14: return "yuv444p14";
    case PixelFormat::yuv440p: return "yuv440p";
    case PixelFormat::yuv440p10: return "yuv440p10";
    case PixelFormat::yuv440p12: return "yuv440p12";
    default: return "none";
    }
}

int chroma_format_of(PixelFormat format) noexcept {
    if (format == PixelFormat::none) return -1;
    if (plane_count(format) == 1) return 0;
    if (chroma_shift_y(format) && !chroma_shift_x(format)) return -1;
    if (chroma_shift_y(format)) return 1;
    if (chroma_shift_x(format)) return 2;
    return 3;
}

int max_sample_value(PixelFormat format) noexcept {
    int d = bit_depth(format);
    return d > 0 ? (1 << d) - 1 : 0;
}

PixelFormat pixel_format_for(int chroma_format_idc, int bit_depth) noexcept {
    static const PixelFormat table[4][4] = {
        {PixelFormat::gray, PixelFormat::gray10, PixelFormat::gray12, PixelFormat::gray14},
        {PixelFormat::yuv420p, PixelFormat::yuv420p10, PixelFormat::yuv420p12, PixelFormat::yuv420p14},
        {PixelFormat::yuv422p, PixelFormat::yuv422p10, PixelFormat::yuv422p12, PixelFormat::yuv422p14},
        {PixelFormat::yuv444p, PixelFormat::yuv444p10, PixelFormat::yuv444p12, PixelFormat::yuv444p14},
    };
    if (chroma_format_idc < 0 || chroma_format_idc > 3 || bit_depth < 8 || bit_depth > 14) return PixelFormat::none;
    int d = bit_depth <= 8 ? 0 : (bit_depth <= 10 ? 1 : (bit_depth <= 12 ? 2 : 3));
    return table[chroma_format_idc][d];
}

const char* picture_structure_name(PictureStructure structure) noexcept {
    switch (structure) {
    case PictureStructure::frame: return "frame";
    case PictureStructure::mbaff_frame: return "mbaff_frame";
    case PictureStructure::field_pair: return "field_pair";
    case PictureStructure::top_field: return "top_field";
    case PictureStructure::bottom_field: return "bottom_field";
    default: return "unknown";
    }
}

std::uint32_t VideoFrame::plane_width(int i) const noexcept {
    if (i == 0) return width;
    int sx = chroma_shift_x(format);
    return (width + (1u << sx) - 1) >> sx;
}

std::uint32_t VideoFrame::plane_height(int i) const noexcept {
    if (i == 0) return height;
    int sy = chroma_shift_y(format);
    return (height + (1u << sy) - 1) >> sy;
}

double VideoFrame::display_aspect() const noexcept {
    if (height == 0) return 1.0;
    double sar = (sar_num && sar_den) ? static_cast<double>(sar_num) / sar_den : 1.0;
    return static_cast<double>(width) * sar / height;
}

bool VideoFrame::allocate(std::uint32_t w, std::uint32_t h, PixelFormat f) {
    if (w == 0 || h == 0 || f == PixelFormat::none || w > 65536 || h > 65536) return false;
    width = w;
    height = h;
    format = f;
    std::size_t bps = static_cast<std::size_t>(bytes_per_sample(f));
    std::size_t total = 0;
    int planes = plane_count(f);
    for (int i = 0; i < 3; ++i) {
        if (i >= planes) {
            offset[i] = 0;
            stride[i] = 0;
            continue;
        }
        std::size_t row = static_cast<std::size_t>(plane_width(i)) * bps;
        stride[i] = (row + 31) & ~static_cast<std::size_t>(31);
        offset[i] = total;
        total += stride[i] * plane_height(i);
    }
    data.assign(total, 0);
    return true;
}

void VideoFrame::clear() {
    data.clear();
    data.shrink_to_fit();
    width = height = 0;
    format = PixelFormat::none;
    for (int i = 0; i < 3; ++i) {
        offset[i] = 0;
        stride[i] = 0;
    }
    interlaced = false;
    top_field_first = true;
    structure = PictureStructure::frame;
    repeat_fields = 0;
}

namespace {

struct Converter {
    int depth = 8;
    int y_off = 16;
    int c_off = 128;
    std::int32_t cy = 0;
    std::int32_t crv = 0;
    std::int32_t cgu = 0;
    std::int32_t cgv = 0;
    std::int32_t cbu = 0;
    bool rgb = false;
};

Converter make_converter(const VideoFrame& f) {
    Converter c;
    c.depth = bit_depth(f.format);
    int shift = c.depth - 8;
    ColorMatrix m = f.color.matrix;
    if (m == ColorMatrix::unspecified) m = (f.height > 576 || f.width > 1024) ? ColorMatrix::bt709 : ColorMatrix::bt601;
    double kr = 0.299;
    double kb = 0.114;
    if (m == ColorMatrix::bt709) {
        kr = 0.2126;
        kb = 0.0722;
    } else if (m == ColorMatrix::bt2020) {
        kr = 0.2627;
        kb = 0.0593;
    }
    double kg = 1.0 - kr - kb;
    double max_value = static_cast<double>((1 << c.depth) - 1);
    double y_range;
    double c_range;
    if (f.color.range == ColorRange::full) {
        c.y_off = 0;
        y_range = max_value;
        c_range = max_value;
    } else {
        c.y_off = 16 << shift;
        y_range = static_cast<double>(219 << shift);
        c_range = static_cast<double>(224 << shift);
    }
    c.c_off = 128 << shift;
    const double scale = 65536.0 * 255.0;
    c.cy = static_cast<std::int32_t>(std::lround(scale / y_range));
    if (m == ColorMatrix::rgb) {
        c.rgb = true;
        c.crv = c.cy;
        return c;
    }
    c.crv = static_cast<std::int32_t>(std::lround(scale * 2.0 * (1.0 - kr) / c_range));
    c.cbu = static_cast<std::int32_t>(std::lround(scale * 2.0 * (1.0 - kb) / c_range));
    c.cgu = static_cast<std::int32_t>(std::lround(scale * 2.0 * kb * (1.0 - kb) / kg / c_range));
    c.cgv = static_cast<std::int32_t>(std::lround(scale * 2.0 * kr * (1.0 - kr) / kg / c_range));
    return c;
}

inline std::uint8_t clamp_u8(std::int32_t v) {
    return static_cast<std::uint8_t>(v < 0 ? 0 : (v > 255 ? 255 : v));
}

template <typename T>
void load_row(const VideoFrame& f, int plane, std::uint32_t y, std::vector<std::int32_t>& out) {
    const T* row = reinterpret_cast<const T*>(f.plane(plane) + f.stride[plane] * y);
    std::uint32_t w = f.plane_width(plane);
    out.resize(w);
    for (std::uint32_t x = 0; x < w; ++x) out[x] = row[x];
}

template <typename T>
void convert_frame(const VideoFrame& f, std::uint8_t* rgba, std::size_t stride) {
    Converter cv = make_converter(f);
    const int planes = plane_count(f.format);
    const int sx = chroma_shift_x(f.format);
    const int sy = chroma_shift_y(f.format);
    const std::uint32_t cw = planes > 1 ? f.plane_width(1) : 0;
    const std::uint32_t ch = planes > 1 ? f.plane_height(1) : 0;
    const int loc = f.color.chroma_location;
    const bool h_cosited = (loc == 0 || loc == 2 || loc == 4);
    const bool v_cosited = (loc == 2 || loc == 3);
    std::vector<std::int32_t> ua;
    std::vector<std::int32_t> ub;
    std::vector<std::int32_t> va;
    std::vector<std::int32_t> vb;
    std::vector<std::int32_t> uv_col_u(cw);
    std::vector<std::int32_t> uv_col_v(cw);
    std::vector<std::int32_t> up_u(f.width);
    std::vector<std::int32_t> up_v(f.width);
    for (std::uint32_t y = 0; y < f.height; ++y) {
        const T* yrow = reinterpret_cast<const T*>(f.plane(0) + f.stride[0] * y);
        std::uint8_t* out = rgba + stride * y;
        if (planes == 1) {
            for (std::uint32_t x = 0; x < f.width; ++x) {
                std::uint8_t v = clamp_u8(((static_cast<std::int32_t>(yrow[x]) - cv.y_off) * cv.cy + 32768) >> 16);
                out[4 * x] = v;
                out[4 * x + 1] = v;
                out[4 * x + 2] = v;
                out[4 * x + 3] = 255;
            }
            continue;
        }
        std::uint32_t c0;
        std::uint32_t c1;
        int w0;
        int w1;
        if (sy == 0) {
            c0 = c1 = y;
            w0 = 16;
            w1 = 0;
        } else if (v_cosited) {
            c0 = y >> 1;
            c1 = std::min(c0 + (y & 1), ch - 1);
            w0 = (y & 1) ? 8 : 16;
            w1 = 16 - w0;
        } else {
            c0 = y >> 1;
            if (y & 1) {
                c1 = std::min(c0 + 1, ch - 1);
            } else {
                c1 = c0 > 0 ? c0 - 1 : 0;
            }
            w0 = 12;
            w1 = 4;
        }
        if (c0 >= ch) c0 = ch - 1;
        load_row<T>(f, 1, c0, ua);
        load_row<T>(f, 1, c1, ub);
        load_row<T>(f, 2, c0, va);
        load_row<T>(f, 2, c1, vb);
        for (std::uint32_t x = 0; x < cw; ++x) {
            uv_col_u[x] = ua[x] * w0 + ub[x] * w1;
            uv_col_v[x] = va[x] * w0 + vb[x] * w1;
        }
        if (sx == 0) {
            for (std::uint32_t x = 0; x < f.width; ++x) {
                up_u[x] = (uv_col_u[x] * 16 + 128) >> 8;
                up_v[x] = (uv_col_v[x] * 16 + 128) >> 8;
            }
        } else {
            for (std::uint32_t x = 0; x < f.width; ++x) {
                std::uint32_t i = x >> 1;
                if (i >= cw) i = cw - 1;
                std::uint32_t j;
                int a;
                int b;
                if (h_cosited) {
                    j = std::min(i + (x & 1), cw - 1);
                    a = (x & 1) ? 8 : 16;
                    b = 16 - a;
                } else {
                    if (x & 1) {
                        j = std::min(i + 1, cw - 1);
                    } else {
                        j = i > 0 ? i - 1 : 0;
                    }
                    a = 12;
                    b = 4;
                }
                up_u[x] = (uv_col_u[i] * a + uv_col_u[j] * b + 128) >> 8;
                up_v[x] = (uv_col_v[i] * a + uv_col_v[j] * b + 128) >> 8;
            }
        }
        if (cv.rgb) {
            for (std::uint32_t x = 0; x < f.width; ++x) {
                std::int32_t g = static_cast<std::int32_t>(yrow[x]);
                out[4 * x] = clamp_u8(((up_v[x] - cv.y_off) * cv.cy + 32768) >> 16);
                out[4 * x + 1] = clamp_u8(((g - cv.y_off) * cv.cy + 32768) >> 16);
                out[4 * x + 2] = clamp_u8(((up_u[x] - cv.y_off) * cv.cy + 32768) >> 16);
                out[4 * x + 3] = 255;
            }
            continue;
        }
        for (std::uint32_t x = 0; x < f.width; ++x) {
            std::int32_t l = (static_cast<std::int32_t>(yrow[x]) - cv.y_off) * cv.cy + 32768;
            std::int32_t u = up_u[x] - cv.c_off;
            std::int32_t v = up_v[x] - cv.c_off;
            out[4 * x] = clamp_u8((l + cv.crv * v) >> 16);
            out[4 * x + 1] = clamp_u8((l - cv.cgu * u - cv.cgv * v) >> 16);
            out[4 * x + 2] = clamp_u8((l + cv.cbu * u) >> 16);
            out[4 * x + 3] = 255;
        }
    }
}

} // namespace

void to_rgba(const VideoFrame& frame, std::uint8_t* rgba, std::size_t stride) {
    if (!rgba || frame.empty() || frame.width == 0 || frame.height == 0) return;
    if (bytes_per_sample(frame.format) == 2) {
        convert_frame<std::uint16_t>(frame, rgba, stride);
    } else {
        convert_frame<std::uint8_t>(frame, rgba, stride);
    }
}

std::vector<std::uint8_t> to_rgba(const VideoFrame& frame) {
    std::vector<std::uint8_t> out;
    if (frame.empty() || frame.width == 0 || frame.height == 0) return out;
    out.resize(static_cast<std::size_t>(frame.width) * frame.height * 4);
    to_rgba(frame, out.data(), static_cast<std::size_t>(frame.width) * 4);
    return out;
}

} // namespace video
} // namespace fizmo
