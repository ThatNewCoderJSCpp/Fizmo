#include "fizmo_library.hpp"
#include "jpeg_codec.hpp"

#include <csetjmp>
#include <jpeglib.h>

namespace fizmo {
namespace images {

namespace jpeg_detail {

struct ErrorManager {
    jpeg_error_mgr pub;
    std::jmp_buf   jump;
    char           message[JMSG_LENGTH_MAX];
};

extern "C" inline void fizmo_jpeg_error_exit(j_common_ptr cinfo) {
    ErrorManager* err = reinterpret_cast<ErrorManager*>(cinfo->err);
    (*cinfo->err->format_message)(cinfo, err->message);
    std::longjmp(err->jump, 1);
}

extern "C" inline void fizmo_jpeg_silent(j_common_ptr, int) {}

} // namespace jpeg_detail

} // namespace images
} // namespace fizmo

namespace fizmo {
namespace images {

bool decode_jpeg(const std::uint8_t* data, std::size_t size, BitmapImage& out, std::string* error) {
    if (!is_jpeg(data, size)) { if (error) *error = "not a JPEG file"; return false; }
    jpeg_decompress_struct cinfo;
    jpeg_detail::ErrorManager err;
    cinfo.err = jpeg_std_error(&err.pub);
    err.pub.error_exit = jpeg_detail::fizmo_jpeg_error_exit;
    err.pub.emit_message = jpeg_detail::fizmo_jpeg_silent;
    err.message[0] = '\0';
    std::vector<std::uint8_t> row;
    BitmapImage result;

    if (setjmp(err.jump)) {
        jpeg_destroy_decompress(&cinfo);
        if (error) *error = std::string("JPEG decode failed: ") + err.message;
        return false;
    }

    jpeg_create_decompress(&cinfo);
    jpeg_mem_src(&cinfo, const_cast<unsigned char*>(data), static_cast<unsigned long>(size));
    jpeg_read_header(&cinfo, TRUE);
    cinfo.out_color_space = JCS_RGB;
    jpeg_start_decompress(&cinfo);
    const unsigned int w = cinfo.output_width, h = cinfo.output_height;
    if (w == 0 || h == 0 || cinfo.output_components != 3) {
        jpeg_destroy_decompress(&cinfo);
        if (error) *error = "unsupported JPEG layout";
        return false;
    }
    result = BitmapImage(w, h);
    row.resize(static_cast<std::size_t>(w) * 3);

    while (cinfo.output_scanline < h) {
        const unsigned int y = cinfo.output_scanline;
        JSAMPROW ptr = row.data();
        jpeg_read_scanlines(&cinfo, &ptr, 1);
        for (unsigned int x = 0; x < w; ++x) result.set_pixel(x, y, graphics::Color(row[x * 3], row[x * 3 + 1], row[x * 3 + 2]));
    }

    jpeg_finish_decompress(&cinfo);
    jpeg_destroy_decompress(&cinfo);
    out = std::move(result);
    return true;
}

bool decode_bmp(const std::uint8_t* data, std::size_t size, BitmapImage& out, std::string* error) {
    auto fail = [&](const char* why) { if (error) *error = why; return false; };
    auto u16 = [&](std::size_t o) -> std::uint32_t { return o + 2 <= size ? static_cast<std::uint32_t>(data[o] | (data[o + 1] << 8)) : 0; };
    auto u32 = [&](std::size_t o) -> std::uint32_t { return o + 4 <= size ? static_cast<std::uint32_t>(data[o] | (data[o + 1] << 8) | (data[o + 2] << 16) | (static_cast<std::uint32_t>(data[o + 3]) << 24)) : 0; };
    if (size < 54 || data[0] != 'B' || data[1] != 'M') return fail("not a BMP file");
    const std::uint32_t offset = u32(10);
    const std::int32_t width = static_cast<std::int32_t>(u32(18));
    const std::int32_t height_raw = static_cast<std::int32_t>(u32(22));
    const std::uint32_t bits = u16(28), compression = u32(30);
    if (width <= 0 || height_raw == 0 || (bits != 24 && bits != 32)) return fail("unsupported BMP (24/32-bit only)");
    if (compression != 0 && !(compression == 3 && bits == 32)) return fail("compressed BMP not supported");
    const bool top_down = height_raw < 0;
    const std::uint32_t w = static_cast<std::uint32_t>(width), h = static_cast<std::uint32_t>(top_down ? -height_raw : height_raw);
    const std::size_t stride = ((static_cast<std::size_t>(w) * bits / 8) + 3) & ~std::size_t(3);
    if (offset + stride * h > size) return fail("truncated BMP");
    BitmapImage img(w, h);

    for (std::uint32_t y = 0; y < h; ++y) {
        const std::uint8_t* row = data + offset + stride * (top_down ? y : h - 1 - y);
        for (std::uint32_t x = 0; x < w; ++x) {
            const std::uint8_t* p = row + x * (bits / 8);
            img.set_pixel(x, y, graphics::Color(p[2], p[1], p[0], bits == 32 && compression == 3 ? p[3] : 255));
        }
    }

    out = std::move(img);
    return true;
}

} // namespace images
} // namespace fizmo
