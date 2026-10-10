#ifndef FIZMO_MATHTEXT_CONVERT_HPP
#define FIZMO_MATHTEXT_CONVERT_HPP

#include <string>
#include <string_view>
#include "printer.hpp"
#include "math_printer.hpp"
#include "../math/math_parser.hpp"

namespace fizmo {
namespace mathtext {

inline std::string math_to_brace(std::string_view math, const PrintOptions& options = PrintOptions()) { return to_string(parse_math(math), options); }

inline std::string brace_to_math(std::string_view brace, const MathPrintOptions& options = MathPrintOptions()) { return to_math(parse(brace), options); }

} // namespace mathtext
} // namespace fizmo

#endif // FIZMO_MATHTEXT_CONVERT_HPP
