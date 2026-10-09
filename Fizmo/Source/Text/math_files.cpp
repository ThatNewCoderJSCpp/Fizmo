#define ALL_FIZMO
#include <fizmo/includes.hpp>
#include "math_files.hpp"

namespace fizmo {
namespace text {

MathImageFormat image_format_for_extension(std::string_view ext) noexcept {
    if (ext == ".png") return MathImageFormat::Png;
    if (ext == ".jpg" || ext == ".jpeg") return MathImageFormat::Jpeg;
    if (ext == ".bmp") return MathImageFormat::Bmp;
    return MathImageFormat::Auto;
}

MathImageFormat resolve_image_format(MathImageFormat format, std::string_view path) {
    if (format != MathImageFormat::Auto) return format;
    const MathImageFormat f = image_format_for_path(path);
    return f == MathImageFormat::Auto ? MathImageFormat::Png : f;
}

images::BitmapImage flatten_image(const images::BitmapImage& src, const graphics::Color& matte) {
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

images::BitmapImage render_math_image(const mathtext::Document& doc, const MathImageOptions& options) {
    return fancy_text_image(layout_math(doc, shared_text_rasterizer(), options.render), options.padding, options.background);
}

bool save_math_image(const images::BitmapImage& image, const std::string& path, const MathImageOptions& options, std::string* error) {
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

mathtext::FileResult math_text_to_image_file(std::string_view source, const std::string& out_path, const MathImageOptions& options, mathtext::SourceSyntax syntax) {
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

mathtext::FileResult process_math_file(const std::string& in_path, const MathFileOutputs& outputs, const MathImageOptions& image_options, mathtext::SourceSyntax syntax, const mathtext::PrintOptions& brace_options, const mathtext::MathPrintOptions& math_options) {
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

mathtext::FileResult math_file_to_image(const std::string& in_path, const std::string& out_path, const MathImageOptions& options, mathtext::SourceSyntax syntax) {
    MathFileOutputs outputs;
    outputs.image_path = out_path;
    return process_math_file(in_path, outputs, options, syntax);
}

} // namespace text
} // namespace fizmo
