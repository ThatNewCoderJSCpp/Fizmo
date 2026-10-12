#include "fizmo_library.hpp"
#include "format_util.hpp"
#include "../Normal/jpeg_codec.hpp"
#include "../Normal/png_codec.hpp"

#include <cctype>
#include <fstream>
#include <iterator>
#include <stdexcept>

namespace fizmo {
namespace images {

namespace {

bool read_file(const std::string& path, std::vector<std::uint8_t>& out) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return false;
    out.assign(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>());
    return true;
}

bool write_file(const std::string& path, const std::vector<std::uint8_t>& data) {
    std::ofstream f(path, std::ios::binary | std::ios::trunc);
    if (!f) return false;
    f.write(reinterpret_cast<const char*>(data.data()), static_cast<std::streamsize>(data.size()));
    return static_cast<bool>(f);
}

bool is_gzip_svg(const std::uint8_t* data, std::size_t size) {
    if (size < 2 || data[0] != 0x1F || data[1] != 0x8B) return false;
    std::vector<std::uint8_t> raw;
    fmt::zlib_inflate(data, std::min<std::size_t>(size, 1u << 16), raw, 0);
    return is_svg(raw.data(), raw.size());
}

} // namespace

ImageFormat detect_format(const std::uint8_t* data, std::size_t size) noexcept {
    if (!data || size < 2) return ImageFormat::Unknown;
    if (is_png(data, size)) return ImageFormat::Png;
    if (is_jpeg(data, size)) return ImageFormat::Jpeg;
    if (is_gif(data, size)) return ImageFormat::Gif;
    if (is_webp(data, size)) return ImageFormat::WebP;
    if (size >= 2 && data[0] == 'B' && data[1] == 'M') return ImageFormat::Bmp;
    if (is_qoi(data, size)) return ImageFormat::Qoi;
    if (is_tiff(data, size)) return ImageFormat::Tiff;
    if (is_ico(data, size)) return data[2] == 2 ? ImageFormat::Cur : ImageFormat::Ico;
    if (is_pnm(data, size)) return ImageFormat::Pnm;
    if (is_svg(data, size)) return ImageFormat::Svg;
    try {
        if (is_gzip_svg(data, size)) return ImageFormat::Svg;
    } catch (...) {
    }
    if (is_tga(data, size)) return ImageFormat::Tga;
    return ImageFormat::Unknown;
}

ImageFormat format_from_extension(std::string_view path) noexcept {
    const std::size_t dot = path.rfind('.');
    std::string ext(dot == std::string_view::npos ? path : path.substr(dot + 1));
    for (char& c : ext) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    if (ext == "png" || ext == "apng") return ImageFormat::Png;
    if (ext == "jpg" || ext == "jpeg" || ext == "jpe" || ext == "jfif") return ImageFormat::Jpeg;
    if (ext == "bmp" || ext == "dib") return ImageFormat::Bmp;
    if (ext == "gif") return ImageFormat::Gif;
    if (ext == "webp") return ImageFormat::WebP;
    if (ext == "tga" || ext == "tpic" || ext == "icb" || ext == "vda" || ext == "vst") return ImageFormat::Tga;
    if (ext == "qoi") return ImageFormat::Qoi;
    if (ext == "pbm" || ext == "pgm" || ext == "ppm" || ext == "pnm" || ext == "pam" || ext == "pfm") return ImageFormat::Pnm;
    if (ext == "ico") return ImageFormat::Ico;
    if (ext == "cur") return ImageFormat::Cur;
    if (ext == "tif" || ext == "tiff") return ImageFormat::Tiff;
    if (ext == "svg" || ext == "svgz") return ImageFormat::Svg;
    return ImageFormat::Unknown;
}

const char* format_name(ImageFormat f) noexcept {
    switch (f) {
        case ImageFormat::Png: return "PNG";
        case ImageFormat::Jpeg: return "JPEG";
        case ImageFormat::Bmp: return "BMP";
        case ImageFormat::Gif: return "GIF";
        case ImageFormat::WebP: return "WebP";
        case ImageFormat::Tga: return "TGA";
        case ImageFormat::Qoi: return "QOI";
        case ImageFormat::Pnm: return "PNM";
        case ImageFormat::Ico: return "ICO";
        case ImageFormat::Cur: return "CUR";
        case ImageFormat::Tiff: return "TIFF";
        case ImageFormat::Svg: return "SVG";
        default: return "Unknown";
    }
}

const char* format_extension(ImageFormat f) noexcept {
    switch (f) {
        case ImageFormat::Png: return "png";
        case ImageFormat::Jpeg: return "jpg";
        case ImageFormat::Bmp: return "bmp";
        case ImageFormat::Gif: return "gif";
        case ImageFormat::WebP: return "webp";
        case ImageFormat::Tga: return "tga";
        case ImageFormat::Qoi: return "qoi";
        case ImageFormat::Pnm: return "ppm";
        case ImageFormat::Ico: return "ico";
        case ImageFormat::Cur: return "cur";
        case ImageFormat::Tiff: return "tiff";
        case ImageFormat::Svg: return "svg";
        default: return "";
    }
}

const char* format_mime_type(ImageFormat f) noexcept {
    switch (f) {
        case ImageFormat::Png: return "image/png";
        case ImageFormat::Jpeg: return "image/jpeg";
        case ImageFormat::Bmp: return "image/bmp";
        case ImageFormat::Gif: return "image/gif";
        case ImageFormat::WebP: return "image/webp";
        case ImageFormat::Tga: return "image/x-tga";
        case ImageFormat::Qoi: return "image/qoi";
        case ImageFormat::Pnm: return "image/x-portable-anymap";
        case ImageFormat::Ico: return "image/vnd.microsoft.icon";
        case ImageFormat::Cur: return "image/x-win-bitmap";
        case ImageFormat::Tiff: return "image/tiff";
        case ImageFormat::Svg: return "image/svg+xml";
        default: return "application/octet-stream";
    }
}

bool can_decode(ImageFormat f) noexcept { return f != ImageFormat::Unknown; }

bool can_encode(ImageFormat f) noexcept { return f != ImageFormat::Unknown && f != ImageFormat::Svg; }

bool can_animate(ImageFormat f) noexcept { return f == ImageFormat::Gif || f == ImageFormat::WebP || f == ImageFormat::Png; }

bool decode_image(const std::uint8_t* data, std::size_t size, BitmapImage& out, std::string* error) {
    switch (detect_format(data, size)) {
        case ImageFormat::Png: return decode_png(data, size, out, error);
        case ImageFormat::Jpeg: return decode_jpeg(data, size, out, error);
        case ImageFormat::Bmp: return decode_bmp(data, size, out, error);
        case ImageFormat::Gif: {
            AnimatedImage a;
            if (!decode_gif(data, size, a, error)) return false;
            out = a.frame(0).image;
            return true;
        }
        case ImageFormat::WebP: return decode_webp(data, size, out, error);
        case ImageFormat::Tga: return decode_tga(data, size, out, error);
        case ImageFormat::Qoi: return decode_qoi(data, size, out, error);
        case ImageFormat::Pnm: return decode_pnm(data, size, out, error);
        case ImageFormat::Ico:
        case ImageFormat::Cur: return decode_ico(data, size, out, error);
        case ImageFormat::Tiff: return decode_tiff(data, size, out, error);
        case ImageFormat::Svg: return decode_svg(data, size, out, SvgOptions(), error);
        default: return fmt::fail(error, "unknown image format");
    }
}

bool decode_animation(const std::uint8_t* data, std::size_t size, AnimatedImage& out, std::string* error) {
    switch (detect_format(data, size)) {
        case ImageFormat::Gif: return decode_gif(data, size, out, error);
        case ImageFormat::WebP: return decode_webp(data, size, out, error);
        case ImageFormat::Png: return decode_apng(data, size, out, error);
        default: {
            BitmapImage img;
            if (!decode_image(data, size, img, error)) return false;
            out = AnimatedImage(std::move(img));
            return true;
        }
    }
}

bool encode_image(const BitmapImage& img, ImageFormat format, std::vector<std::uint8_t>& out, const EncodeOptions& options, std::string* error) {
    out.clear();
    if (!img.is_valid_image()) return fmt::fail(error, "empty image");
    bool ok = false;

    switch (format) {
        case ImageFormat::Png: ok = encode_png(img, out, options.alpha, std::max(0, std::min(9, options.compression))); break;
        case ImageFormat::Jpeg: return encode_jpeg(img, out, options.quality, error);
        case ImageFormat::Bmp: ok = encode_bmp(img, out, options.alpha); break;
        case ImageFormat::Gif: return encode_gif(AnimatedImage(img), out, options.dither, error);
        case ImageFormat::WebP: return encode_webp(img, out, options, error);
        case ImageFormat::Tga: ok = encode_tga(img, out, options.alpha, options.compression > 0); break;
        case ImageFormat::Qoi: ok = encode_qoi(img, out, options.alpha); break;
        case ImageFormat::Pnm: ok = encode_pnm(img, out, options.alpha); break;
        case ImageFormat::Ico:
        case ImageFormat::Cur: {
            if (img.width() <= 256 && img.height() <= 256) return encode_ico({ img }, out, error);
            const double s = 256.0 / std::max(img.width(), img.height());
            const BitmapImage small = img.as_resized(std::max(1u, static_cast<unsigned>(std::lround(img.width() * s))), std::max(1u, static_cast<unsigned>(std::lround(img.height() * s))));
            return encode_ico({ small }, out, error);
        }
        case ImageFormat::Tiff: ok = encode_tiff(img, out, options.alpha, options.compression > 0); break;
        case ImageFormat::Svg: return fmt::fail(error, "SVG encoding is not supported");
        default: return fmt::fail(error, "unknown image format");
    }

    return ok || fmt::fail(error, "image encode failed");
}

bool encode_animation(const AnimatedImage& anim, ImageFormat format, std::vector<std::uint8_t>& out, const EncodeOptions& options, std::string* error) {
    if (anim.empty()) return fmt::fail(error, "empty animation");
    switch (format) {
        case ImageFormat::Gif: return encode_gif(anim, out, options.dither, error);
        case ImageFormat::WebP: return encode_webp(anim, out, options, error);
        case ImageFormat::Png: return anim.frame_count() > 1 ? encode_apng(anim, out, error) : encode_image(anim.frame(0).image, format, out, options, error);
        default: return encode_image(anim.frame(0).image, format, out, options, error);
    }
}

bool load_image(const std::string& path, BitmapImage& out, std::string* error) {
    std::vector<std::uint8_t> data;
    if (!read_file(path, data)) return fmt::fail(error, "cannot read file");
    return decode_image(data.data(), data.size(), out, error);
}

bool load_animation(const std::string& path, AnimatedImage& out, std::string* error) {
    std::vector<std::uint8_t> data;
    if (!read_file(path, data)) return fmt::fail(error, "cannot read file");
    return decode_animation(data.data(), data.size(), out, error);
}

bool save_image(const std::string& path, const BitmapImage& img, ImageFormat format, const EncodeOptions& options, std::string* error) {
    if (format == ImageFormat::Unknown) format = format_from_extension(path);
    if (format == ImageFormat::Unknown) format = ImageFormat::Png;
    std::vector<std::uint8_t> data;
    if (!encode_image(img, format, data, options, error)) return false;
    return write_file(path, data) || fmt::fail(error, "cannot write file");
}

bool save_animation(const std::string& path, const AnimatedImage& anim, ImageFormat format, const EncodeOptions& options, std::string* error) {
    if (format == ImageFormat::Unknown) format = format_from_extension(path);
    if (format == ImageFormat::Unknown) format = ImageFormat::Gif;
    std::vector<std::uint8_t> data;
    if (!encode_animation(anim, format, data, options, error)) return false;
    return write_file(path, data) || fmt::fail(error, "cannot write file");
}

BitmapImage load_image(const std::string& path) {
    BitmapImage img;
    std::string err;
    if (!load_image(path, img, &err)) throw std::runtime_error("load_image(" + path + "): " + err);
    return img;
}

AnimatedImage load_animation(const std::string& path) {
    AnimatedImage a;
    std::string err;
    if (!load_animation(path, a, &err)) throw std::runtime_error("load_animation(" + path + "): " + err);
    return a;
}

} // namespace images
} // namespace fizmo
