#define ALL_FIZMO
#include <fizmo/includes.hpp>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <iterator>
#include <string>
#include <vector>

namespace {

using namespace fizmo;

const char* const kUsage =
    "usage: fizmo-mathtext [options] INPUT [OUTPUT...]\n"
    "       fizmo-mathtext [options] -e \"MATH\" OUTPUT...\n"
    "\n"
    "INPUT is a math text file (.math .mt .txt .mathtext), a brace file (.mtb .brace .mtbrace),\n"
    "or - for standard input. Each line of a math text file is its own equation; a line that\n"
    "starts or ends with an operator, or sits inside open brackets, continues the previous one.\n"
    "\n"
    "Each OUTPUT is chosen by its extension:\n"
    "  .png .jpg .jpeg .bmp        rendered image\n"
    "  .mtb .brace .mtbrace        brace form\n"
    "  .math .mt .txt .mathtext    normal math text\n"
    "  -                           brace form to standard output\n"
    "With no OUTPUT, INPUT.mtb and INPUT.png are written next to INPUT.\n"
    "\n"
    "options:\n"
    "  -e, --expr MATH          use MATH instead of an input file\n"
    "  --syntax math|brace      how to read the input (default: from the extension)\n"
    "  --size N                 font size in pixels (default 32)\n"
    "  --padding N              padding around the image in pixels (default 8)\n"
    "  --background COLOR       #rgb, #rrggbb, #rrggbbaa, white, black or transparent (default transparent)\n"
    "  --color COLOR            text color (default #141414)\n"
    "  --error-color COLOR      color for invalid input (default #d32f2f)\n"
    "  --center                 center multiple equations instead of left aligning them\n"
    "  --inline                 use inline (text) style instead of display style\n"
    "  --numbers STYLE          as-written, e, E or times-ten\n"
    "  --compact                write the brace form on one line per equation\n"
    "  --unicode                write symbols as Unicode in text outputs\n"
    "  --quality N              JPEG quality 1-100 (default 92)\n"
    "  -q, --quiet              do not print diagnostics\n"
    "  -h, --help               show this help\n"
    "\n"
    "exit status: 0 success, 1 file or usage error, 2 the math contains errors (outputs are still written)\n";

int hex_digit(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

bool parse_color(std::string s, graphics::Color& out) {
    for (char& c : s) if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
    if (s == "transparent" || s == "none") { out = graphics::Color(255, 255, 255, 0); return true; }
    if (s == "white") { out = graphics::Color(255, 255, 255, 255); return true; }
    if (s == "black") { out = graphics::Color(0, 0, 0, 255); return true; }
    if (!s.empty() && s[0] == '#') s.erase(0, 1);
    std::vector<int> v;
    for (char c : s) {
        const int d = hex_digit(c);
        if (d < 0) return false;
        v.push_back(d);
    }
    if (v.size() == 3 || v.size() == 4) {
        const auto ch = [&](std::size_t i) { return static_cast<std::uint8_t>(v[i] * 17); };
        out = graphics::Color(ch(0), ch(1), ch(2), v.size() == 4 ? ch(3) : 255);
        return true;
    }
    if (v.size() == 6 || v.size() == 8) {
        const auto ch = [&](std::size_t i) { return static_cast<std::uint8_t>(v[i] * 16 + v[i + 1]); };
        out = graphics::Color(ch(0), ch(2), ch(4), v.size() == 8 ? ch(6) : 255);
        return true;
    }
    return false;
}

bool parse_number(const std::string& s, double& out) {
    char* end = nullptr;
    out = std::strtod(s.c_str(), &end);
    return end && *end == '\0' && !s.empty();
}

enum class OutputKind { Image, Brace, Math, BraceStdout };

struct Output {
    OutputKind  kind;
    std::string path;
};

bool classify_output(const std::string& path, Output& out) {
    if (path == "-") { out = Output{ OutputKind::BraceStdout, path }; return true; }
    const std::string ext = mathtext::lowercase_extension(path);
    if (text::is_image_extension(ext)) { out = Output{ OutputKind::Image, path }; return true; }
    if (mathtext::is_brace_extension(ext)) { out = Output{ OutputKind::Brace, path }; return true; }
    if (mathtext::is_math_extension(ext)) { out = Output{ OutputKind::Math, path }; return true; }
    return false;
}

int fail(const std::string& message) {
    std::cerr << "fizmo-mathtext: " << message << "\n";
    return 1;
}

}

int main(int argc, char** argv) {
    std::vector<std::string> args(argv + 1, argv + argc);
    std::vector<std::string> positional;
    std::string expr;
    bool have_expr = false;
    bool quiet = false;
    mathtext::SourceSyntax syntax = mathtext::SourceSyntax::Auto;
    text::MathImageOptions image;
    image.render.layout.font_size = 32.0;
    mathtext::PrintOptions brace = mathtext::default_brace_file_options();
    mathtext::MathPrintOptions math;

    for (std::size_t i = 0; i < args.size(); ++i) {
        const std::string& a = args[i];
        const auto value = [&](std::string& out) {
            if (i + 1 >= args.size()) return false;
            out = args[++i];
            return true;
        };
        std::string v;
        double d = 0.0;
        if (a == "-h" || a == "--help") { std::cout << kUsage; return 0; }
        else if (a == "-q" || a == "--quiet") quiet = true;
        else if (a == "--center") image.render.layout.center_lines = true;
        else if (a == "--inline") image.render.layout.display = false;
        else if (a == "--compact") brace.layout = mathtext::Layout::Compact;
        else if (a == "--unicode") { brace.unicode_symbols = true; math.unicode_symbols = true; }
        else if (a == "-e" || a == "--expr") {
            if (!value(expr)) return fail("missing value for " + a);
            have_expr = true;
        }
        else if (a == "--syntax") {
            if (!value(v)) return fail("missing value for --syntax");
            if (v == "math") syntax = mathtext::SourceSyntax::Math;
            else if (v == "brace") syntax = mathtext::SourceSyntax::Brace;
            else return fail("unknown syntax '" + v + "' (use math or brace)");
        }
        else if (a == "--size") {
            if (!value(v) || !parse_number(v, d) || d < 4.0 || d > 1000.0) return fail("--size needs a number between 4 and 1000");
            image.render.layout.font_size = d;
        }
        else if (a == "--padding") {
            if (!value(v) || !parse_number(v, d) || d < 0.0 || d > 1000.0) return fail("--padding needs a number between 0 and 1000");
            image.padding = static_cast<float>(d);
        }
        else if (a == "--quality") {
            if (!value(v) || !parse_number(v, d) || d < 1.0 || d > 100.0) return fail("--quality needs a number between 1 and 100");
            image.jpeg_quality = static_cast<int>(d);
        }
        else if (a == "--background") {
            if (!value(v) || !parse_color(v, image.background)) return fail("--background needs a color");
            if (image.background.alpha() == 255) image.matte = image.background;
        }
        else if (a == "--color") {
            if (!value(v) || !parse_color(v, image.render.theme.color)) return fail("--color needs a color");
        }
        else if (a == "--error-color") {
            if (!value(v) || !parse_color(v, image.render.theme.error_color)) return fail("--error-color needs a color");
        }
        else if (a == "--numbers") {
            if (!value(v)) return fail("missing value for --numbers");
            mathtext::NumberStyle n;
            if (v == "as-written") n = mathtext::NumberStyle::AsWritten;
            else if (v == "e") n = mathtext::NumberStyle::LowerE;
            else if (v == "E") n = mathtext::NumberStyle::UpperE;
            else if (v == "times-ten") n = mathtext::NumberStyle::TimesTen;
            else return fail("unknown number style '" + v + "'");
            image.render.layout.numbers = n;
            brace.numbers = n;
            math.numbers = n;
        }
        else if (a.size() > 1 && a[0] == '-' && a != "-") return fail("unknown option '" + a + "' (see --help)");
        else positional.push_back(a);
    }

    std::string source_name;
    std::string text;
    std::size_t first_output = 0;
    if (have_expr) {
        source_name = "expression";
        text = expr;
        if (syntax == mathtext::SourceSyntax::Auto) syntax = mathtext::SourceSyntax::Math;
        if (positional.empty()) return fail("-e needs at least one OUTPUT");
    } else {
        if (positional.empty()) { std::cerr << kUsage; return 1; }
        source_name = positional[0];
        first_output = 1;
        if (source_name == "-") {
            text.assign(std::istreambuf_iterator<char>(std::cin), std::istreambuf_iterator<char>());
            source_name = "stdin";
            if (syntax == mathtext::SourceSyntax::Auto) syntax = mathtext::SourceSyntax::Math;
        } else {
            std::string error;
            if (!mathtext::read_text_file(source_name, text, &error)) return fail(error);
            syntax = mathtext::resolve_syntax(syntax, source_name);
        }
    }

    std::vector<Output> outputs;
    for (std::size_t i = first_output; i < positional.size(); ++i) {
        Output o{ OutputKind::Image, std::string() };
        if (!classify_output(positional[i], o)) return fail("cannot tell what to write for '" + positional[i] + "' (use an image, brace or math extension)");
        outputs.push_back(o);
    }
    if (outputs.empty()) {
        if (source_name == "stdin") return fail("reading standard input needs at least one OUTPUT");
        const std::string brace_path = mathtext::replace_extension(source_name, ".mtb");
        if (brace_path == source_name) return fail("input is already a .mtb file; name the outputs");
        outputs.push_back(Output{ OutputKind::Brace, brace_path });
        outputs.push_back(Output{ OutputKind::Image, mathtext::replace_extension(source_name, ".png") });
    }

    const mathtext::Document doc = mathtext::parse_source(text, syntax, image.render.parse);
    std::size_t errors = 0;
    for (const mathtext::Diagnostic& diag : doc.diagnostics()) if (diag.severity == mathtext::Severity::Error) ++errors;
    if (!quiet && !doc.diagnostics().empty()) std::cerr << mathtext::format_diagnostics(doc, source_name);

    bool io_ok = true;
    for (const Output& o : outputs) {
        std::string error;
        bool ok = true;
        switch (o.kind) {
            case OutputKind::Image: ok = text::save_math_image(doc, o.path, image, &error); break;
            case OutputKind::Brace: ok = mathtext::write_text_file(o.path, mathtext::brace_file_text(doc, brace), &error); break;
            case OutputKind::Math: ok = mathtext::write_text_file(o.path, mathtext::math_file_text(doc, math), &error); break;
            case OutputKind::BraceStdout: std::cout << mathtext::brace_file_text(doc, brace); break;
        }
        if (!ok) {
            io_ok = false;
            std::cerr << "fizmo-mathtext: " << error << "\n";
        } else if (!quiet && o.kind != OutputKind::BraceStdout) {
            std::cerr << "wrote " << o.path << "\n";
        }
    }
    if (!io_ok) return 1;
    return errors ? 2 : 0;
}
