#ifndef FIZMO_LINE_BREAK_HPP
#define FIZMO_LINE_BREAK_HPP

#include <cstdint>
#include <vector>

namespace fizmo {
namespace graphics {
namespace detail {
namespace gfx {

enum class LineBreakClass : std::uint8_t {
    AL, BA, BB, B2, CL, CM, CP, EX, GL, HY, ID, IS, NS, NU, OP, PO, PR, QU, SP, SY, WJ, ZW, ZWJ, RI, EB, EM, HL, BK
};

LineBreakClass line_break_class(char32_t c) noexcept;

void find_line_breaks(const char32_t* text, std::size_t count, std::vector<std::uint8_t>& allowed);

bool needs_hyphen(char32_t before, char32_t after) noexcept;

} // namespace gfx
} // namespace detail
} // namespace graphics
} // namespace fizmo

#endif // FIZMO_LINE_BREAK_HPP
