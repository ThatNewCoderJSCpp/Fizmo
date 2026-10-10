#ifndef FIZMO_MATHTEXT_MATH_FILES_HPP
#define FIZMO_MATHTEXT_MATH_FILES_HPP

#include <cstddef>
#include <cstdint>
#include <fstream>
#include <iterator>
#include <sstream>
#include <string>
#include <string_view>
#include "../math/math_parser.hpp"
#include "../format/printer.hpp"
#include "../format/math_printer.hpp"
#include "../format/tree_dump.hpp"

namespace fizmo {
namespace mathtext {

enum class SourceSyntax : std::uint8_t { Auto, Math, Brace };

inline std::string lowercase_extension(std::string_view path) {
    const std::size_t slash = path.find_last_of("/\\");
    const std::size_t dot = path.find_last_of('.');
    if (dot == std::string_view::npos || (slash != std::string_view::npos && dot < slash)) return std::string();
    std::string ext(path.substr(dot));
    for (char& c : ext) if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
    return ext;
}

inline std::string replace_extension(std::string_view path, std::string_view ext) {
    const std::size_t slash = path.find_last_of("/\\");
    const std::size_t dot = path.find_last_of('.');
    std::string out(path);
    if (dot != std::string_view::npos && (slash == std::string_view::npos || dot > slash)) out.resize(dot);
    out.append(ext.data(), ext.size());
    return out;
}

inline bool is_brace_extension(std::string_view ext) noexcept { return ext == ".mtb" || ext == ".brace" || ext == ".mtbrace"; }
inline bool is_math_extension(std::string_view ext) noexcept { return ext == ".math" || ext == ".mt" || ext == ".txt" || ext == ".mathtext"; }

inline SourceSyntax syntax_for_path(std::string_view path) {
    return is_brace_extension(lowercase_extension(path)) ? SourceSyntax::Brace : SourceSyntax::Math;
}

inline SourceSyntax resolve_syntax(SourceSyntax syntax, std::string_view path) { return syntax == SourceSyntax::Auto ? syntax_for_path(path) : syntax; }

inline bool read_text_file(const std::string& path, std::string& out, std::string* error = nullptr) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        if (error) *error = "cannot open '" + path + "' for reading";
        return false;
    }
    out.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
    if (out.size() >= 3 && static_cast<unsigned char>(out[0]) == 0xEF && static_cast<unsigned char>(out[1]) == 0xBB && static_cast<unsigned char>(out[2]) == 0xBF) out.erase(0, 3);
    return true;
}

inline bool write_text_file(const std::string& path, std::string_view text, std::string* error = nullptr) {
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out) {
        if (error) *error = "cannot open '" + path + "' for writing";
        return false;
    }
    out.write(text.data(), static_cast<std::streamsize>(text.size()));
    if (!out) {
        if (error) *error = "failed while writing '" + path + "'";
        return false;
    }
    return true;
}

inline Document parse_source(std::string_view text, SourceSyntax syntax, const ParseOptions& options = ParseOptions()) {
    return syntax == SourceSyntax::Brace ? parse(text, options) : parse_math(text, options);
}

struct FileResult {
    bool        io_ok = false;
    std::size_t errors = 0;
    std::size_t warnings = 0;
    std::string message;
    Document    document;

    bool ok() const noexcept { return io_ok && errors == 0; }
};

inline void summarize(FileResult& r, std::string_view source_name) {
    r.errors = 0;
    r.warnings = 0;
    for (const Diagnostic& d : r.document.diagnostics()) {
        if (d.severity == Severity::Error) ++r.errors;
        else if (d.severity == Severity::Warning) ++r.warnings;
    }
    r.message += format_diagnostics(r.document, source_name);
}

inline FileResult load_math_file(const std::string& path, SourceSyntax syntax = SourceSyntax::Auto, const ParseOptions& options = ParseOptions()) {
    FileResult r;
    std::string text;
    if (!read_text_file(path, text, &r.message)) return r;
    r.io_ok = true;
    r.document = parse_source(text, resolve_syntax(syntax, path), options);
    summarize(r, path);
    return r;
}

inline PrintOptions default_brace_file_options() {
    PrintOptions o;
    o.layout = Layout::Auto;
    return o;
}

inline std::string brace_file_text(const Document& doc, const PrintOptions& options = default_brace_file_options()) {
    std::string out = to_string(doc, options);
    if (!out.empty()) out.push_back('\n');
    return out;
}

inline std::string math_file_text(const Document& doc, MathPrintOptions options = MathPrintOptions()) {
    options.item_separator = ";\n";
    std::string out = to_math(doc, options);
    if (!out.empty()) out.push_back('\n');
    return out;
}

inline FileResult convert_to_brace_file(const std::string& in_path, const std::string& out_path, const PrintOptions& options = default_brace_file_options(), SourceSyntax syntax = SourceSyntax::Auto) {
    FileResult r = load_math_file(in_path, syntax);
    if (!r.io_ok) return r;
    if (!write_text_file(out_path, brace_file_text(r.document, options), &r.message)) r.io_ok = false;
    return r;
}

inline FileResult convert_to_math_file(const std::string& in_path, const std::string& out_path, const MathPrintOptions& options = MathPrintOptions(), SourceSyntax syntax = SourceSyntax::Auto) {
    FileResult r = load_math_file(in_path, syntax);
    if (!r.io_ok) return r;
    if (!write_text_file(out_path, math_file_text(r.document, options), &r.message)) r.io_ok = false;
    return r;
}

} // namespace mathtext
} // namespace fizmo

#endif // FIZMO_MATHTEXT_MATH_FILES_HPP
