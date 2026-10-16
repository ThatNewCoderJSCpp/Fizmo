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
    const bool cmyk = cinfo.jpeg_color_space == JCS_CMYK || cinfo.jpeg_color_space == JCS_YCCK;
    const bool inverted = cmyk && cinfo.saw_Adobe_marker;
    cinfo.out_color_space = cmyk ? JCS_CMYK : JCS_RGB;
    jpeg_start_decompress(&cinfo);
    const unsigned int w = cinfo.output_width, h = cinfo.output_height;
    const int comps = cinfo.output_components;
    if (w == 0 || h == 0 || comps != (cmyk ? 4 : 3)) {
        jpeg_destroy_decompress(&cinfo);
        if (error) *error = "unsupported JPEG layout";
        return false;
    }
    result = BitmapImage(w, h);
    row.resize(static_cast<std::size_t>(w) * static_cast<std::size_t>(comps));

    while (cinfo.output_scanline < h) {
        const unsigned int y = cinfo.output_scanline;
        JSAMPROW ptr = row.data();
        jpeg_read_scanlines(&cinfo, &ptr, 1);
        if (!cmyk) {
            for (unsigned int x = 0; x < w; ++x) result.set_pixel(x, y, colors::Color(row[x * 3], row[x * 3 + 1], row[x * 3 + 2]));
            continue;
        }
        for (unsigned int x = 0; x < w; ++x) {
            unsigned c = row[x * 4], m = row[x * 4 + 1], ye = row[x * 4 + 2], k = row[x * 4 + 3];
            if (!inverted) { c = 255 - c; m = 255 - m; ye = 255 - ye; k = 255 - k; }
            result.set_pixel(x, y, colors::Color(static_cast<std::uint8_t>((c * k + 127) / 255), static_cast<std::uint8_t>((m * k + 127) / 255), static_cast<std::uint8_t>((ye * k + 127) / 255)));
        }
    }

    jpeg_finish_decompress(&cinfo);
    jpeg_destroy_decompress(&cinfo);
    out = std::move(result);
    return true;
}

bool encode_jpeg(const BitmapImage& img, std::vector<std::uint8_t>& out, int quality, std::string* error) {
    out.clear();
    if (!img.is_valid_image()) {
        if (error) *error = "empty image";
        return false;
    }

    jpeg_compress_struct cinfo;
    jpeg_detail::ErrorManager err;
    cinfo.err = jpeg_std_error(&err.pub);
    err.pub.error_exit = jpeg_detail::fizmo_jpeg_error_exit;
    err.pub.emit_message = jpeg_detail::fizmo_jpeg_silent;
    err.message[0] = '\0';
    unsigned char* buffer = nullptr;
    unsigned long size = 0;
    std::vector<std::uint8_t> row;

    if (setjmp(err.jump)) {
        jpeg_destroy_compress(&cinfo);
        if (buffer) std::free(buffer);
        if (error) *error = std::string("JPEG encode failed: ") + err.message;
        return false;
    }

    jpeg_create_compress(&cinfo);
    jpeg_mem_dest(&cinfo, &buffer, &size);
    cinfo.image_width = img.width();
    cinfo.image_height = img.height();
    cinfo.input_components = 3;
    cinfo.in_color_space = JCS_RGB;
    jpeg_set_defaults(&cinfo);
    jpeg_set_quality(&cinfo, std::max(1, std::min(100, quality)), TRUE);
    cinfo.optimize_coding = TRUE;
    jpeg_start_compress(&cinfo, TRUE);
    row.resize(static_cast<std::size_t>(img.width()) * 3);
    const auto& px = img.pixels();

    while (cinfo.next_scanline < cinfo.image_height) {
        const std::size_t base = static_cast<std::size_t>(cinfo.next_scanline) * img.width();
        for (unsigned int x = 0; x < img.width(); ++x) {
            const colors::Color& c = px[base + x];
            row[x * 3] = c.red();
            row[x * 3 + 1] = c.green();
            row[x * 3 + 2] = c.blue();
        }
        JSAMPROW ptr = row.data();
        jpeg_write_scanlines(&cinfo, &ptr, 1);
    }

    jpeg_finish_compress(&cinfo);
    out.assign(buffer, buffer + size);
    jpeg_destroy_compress(&cinfo);
    std::free(buffer);
    return true;
}

} // namespace images
} // namespace fizmo
