#ifndef FIZMO_TEXT_MATH_FILES_HPP
#define FIZMO_TEXT_MATH_FILES_HPP

#include "fancy_math.hpp"
#include <cstdint>
#include <exception>
#include <string>
#include <string_view>

namespace fizmo {
namespace text {

enum class MathImageFormat : std::uint8_t { Auto, Png, Jpeg, Bmp };

inline MathImageFormat image_format_for_extension(std::string_view ext) noexcept {
    if (ext == ".png") return MathImageFormat::Png;
    if (ext == ".jpg" || ext == ".jpeg") return MathImageFormat::Jpeg;
    if (ext == ".bmp") return MathImageFormat::Bmp;
    return MathImageFormat::Auto;
}

inline bool is_image_extension(std::string_view ext) noexcept { return image_format_for_extension(ext) != MathImageFormat::Auto; }

inline MathImageFormat image_format_for_path(std::string_view path) { return image_format_for_extension(mathtext::lowercase_extension(path)); }

inline MathImageFormat resolve_image_format(MathImageFormat format, std::string_view path) {
    if (format != MathImageFormat::Auto) return format;
    const MathImageFormat f = image_format_for_path(path);
    return f == MathImageFormat::Auto ? MathImageFormat::Png : f;
}

struct MathImageOptions {
    MathRenderOptions render;
    float             padding = 8.0f;
    graphics::Color   background = graphics::Color(255, 255, 255, 0);
    graphics::Color   matte = graphics::Color(255, 255, 255, 255);
    MathImageFormat   format = MathImageFormat::Auto;
    int               jpeg_quality = 92;
};

inline images::BitmapImage flatten_image(const images::BitmapImage& src, const graphics::Color& matte) {
    images::BitmapImage out(src.width(), src.height());
    for (unsigned y = 0; y < src.height(); ++y) {
        for (unsigned x = 0; x < src.width(); ++x) {
            const graphics::Color c = src.get_pixel(x, y);
            const unsigned a = c.alpha();
            const auto mix = [a](unsigned fg, unsigned bg) { return static_cast<std::uint8_t>((fg * a + bg * (255u - a) + 127u) / 255u); };
            out.set_pixel(x, y, graphics::Color(mix(c.red(), matte.red()), mix(c.green(), matte.green()), mix(c.blue(), matte.blue()), 255));
        }
    }
    return out;
}

inline images::BitmapImage render_math_image(const mathtext::Document& doc, const MathImageOptions& options = MathImageOptions()) {
    return fancy_text_image(layout_math(doc, shared_text_rasterizer(), options.render), options.padding, options.background);
}

inline images::BitmapImage render_math_image(std::string_view source, const MathImageOptions& options = MathImageOptions()) {
    return render_math_image(parse_math_source(source, options.render), options);
}

inline bool save_math_image(const images::BitmapImage& image, const std::string& path, const MathImageOptions& options = MathImageOptions(), std::string* error = nullptr) {
    bool ok = false;
    try {
        switch (resolve_image_format(options.format, path)) {
            case MathImageFormat::Jpeg: ok = flatten_image(image, options.matte).save_to_jpeg(path, options.jpeg_quality); break;
            case MathImageFormat::Bmp: ok = flatten_image(image, options.matte).save_to_file(path); break;
            default: {
                images::BitmapImage copy = image;
                ok = copy.save_to_png(path);
                break;
            }
        }
    } catch (const std::exception& e) {
        if (error) *error = "cannot write image '" + path + "': " + e.what();
        return false;
    }
    if (!ok && error) *error = "cannot write image '" + path + "'";
    return ok;
}

inline bool save_math_image(const mathtext::Document& doc, const std::string& path, const MathImageOptions& options = MathImageOptions(), std::string* error = nullptr) {
    return save_math_image(render_math_image(doc, options), path, options, error);
}

inline mathtext::FileResult math_text_to_image_file(std::string_view source, const std::string& out_path, const MathImageOptions& options = MathImageOptions(), mathtext::SourceSyntax syntax = mathtext::SourceSyntax::Auto) {
    mathtext::FileResult r;
    r.io_ok = true;
    r.document = mathtext::parse_source(source, syntax == mathtext::SourceSyntax::Auto ? (options.render.brace_syntax ? mathtext::SourceSyntax::Brace : mathtext::SourceSyntax::Math) : syntax, options.render.parse);
    mathtext::summarize(r, "input");
    std::string error;
    if (!save_math_image(r.document, out_path, options, &error)) {
        r.io_ok = false;
        r.message += error;
    }
    return r;
}

struct MathFileOutputs {
    std::string brace_path;
    std::string math_path;
    std::string image_path;
};

inline mathtext::FileResult process_math_file(const std::string& in_path, const MathFileOutputs& outputs, const MathImageOptions& image_options = MathImageOptions(), mathtext::SourceSyntax syntax = mathtext::SourceSyntax::Auto, const mathtext::PrintOptions& brace_options = mathtext::default_brace_file_options(), const mathtext::MathPrintOptions& math_options = mathtext::MathPrintOptions()) {
    mathtext::FileResult r = mathtext::load_math_file(in_path, syntax, image_options.render.parse);
    if (!r.io_ok) return r;
    std::string error;
    if (!outputs.brace_path.empty() && !mathtext::write_text_file(outputs.brace_path, mathtext::brace_file_text(r.document, brace_options), &error)) {
        r.io_ok = false;
        r.message += error + "\n";
        error.clear();
    }
    if (!outputs.math_path.empty() && !mathtext::write_text_file(outputs.math_path, mathtext::math_file_text(r.document, math_options), &error)) {
        r.io_ok = false;
        r.message += error + "\n";
        error.clear();
    }
    if (!outputs.image_path.empty() && !save_math_image(r.document, outputs.image_path, image_options, &error)) {
        r.io_ok = false;
        r.message += error + "\n";
    }
    return r;
}

inline mathtext::FileResult math_file_to_image(const std::string& in_path, const std::string& out_path, const MathImageOptions& options = MathImageOptions(), mathtext::SourceSyntax syntax = mathtext::SourceSyntax::Auto) {
    MathFileOutputs outputs;
    outputs.image_path = out_path;
    return process_math_file(in_path, outputs, options, syntax);
}

} // namespace text
} // namespace fizmo

#endif // FIZMO_TEXT_MATH_FILES_HPP