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

 MathImageFormat image_format_for_extension(std::string_view ext) noexcept;

inline bool is_image_extension(std::string_view ext) noexcept { return image_format_for_extension(ext) != MathImageFormat::Auto; }

inline MathImageFormat image_format_for_path(std::string_view path) { return image_format_for_extension(mathtext::lowercase_extension(path)); }

 MathImageFormat resolve_image_format(MathImageFormat format, std::string_view path);

struct MathImageOptions {
    MathRenderOptions render;
    float             padding = 8.0f;
    graphics::Color   background = graphics::Color(255, 255, 255, 0);
    graphics::Color   matte = graphics::Color(255, 255, 255, 255);
    MathImageFormat   format = MathImageFormat::Auto;
    int               jpeg_quality = 92;
};

 images::BitmapImage flatten_image(const images::BitmapImage& src, const graphics::Color& matte);

 images::BitmapImage render_math_image(const mathtext::Document& doc, const MathImageOptions& options = MathImageOptions());

inline images::BitmapImage render_math_image(std::string_view source, const MathImageOptions& options = MathImageOptions()) {
    return render_math_image(parse_math_source(source, options.render), options);
}

 bool save_math_image(const images::BitmapImage& image, const std::string& path, const MathImageOptions& options = MathImageOptions(), std::string* error = nullptr);

inline bool save_math_image(const mathtext::Document& doc, const std::string& path, const MathImageOptions& options = MathImageOptions(), std::string* error = nullptr) {
    return save_math_image(render_math_image(doc, options), path, options, error);
}

 mathtext::FileResult math_text_to_image_file(std::string_view source, const std::string& out_path, const MathImageOptions& options = MathImageOptions(), mathtext::SourceSyntax syntax = mathtext::SourceSyntax::Auto);

struct MathFileOutputs {
    std::string brace_path;
    std::string math_path;
    std::string image_path;
};

 mathtext::FileResult process_math_file(const std::string& in_path, const MathFileOutputs& outputs, const MathImageOptions& image_options = MathImageOptions(), mathtext::SourceSyntax syntax = mathtext::SourceSyntax::Auto, const mathtext::PrintOptions& brace_options = mathtext::default_brace_file_options(), const mathtext::MathPrintOptions& math_options = mathtext::MathPrintOptions());

 mathtext::FileResult math_file_to_image(const std::string& in_path, const std::string& out_path, const MathImageOptions& options = MathImageOptions(), mathtext::SourceSyntax syntax = mathtext::SourceSyntax::Auto);

} // namespace text
} // namespace fizmo

#endif // FIZMO_TEXT_MATH_FILES_HPP