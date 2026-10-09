#ifndef FIZMO_MATHTEXT_SPAN_HPP
#define FIZMO_MATHTEXT_SPAN_HPP

#include <cstddef>
#include <cstdint>
#include <string_view>
#include <vector>
#include "utf8.hpp"

namespace fizmo {
namespace mathtext {

struct Span {
    std::uint32_t begin = 0;
    std::uint32_t length = 0;

    constexpr Span() noexcept = default;
    constexpr Span(std::uint32_t b, std::uint32_t n) noexcept : begin(b), length(n) {}

    static constexpr Span between(std::size_t b, std::size_t e) noexcept { return Span(static_cast<std::uint32_t>(b), static_cast<std::uint32_t>(e > b ? e - b : 0)); }

    constexpr std::uint32_t end() const noexcept { return begin + length; }
    constexpr bool empty() const noexcept { return length == 0; }
    constexpr bool contains(std::uint32_t offset) const noexcept { return offset >= begin && offset < end(); }
    constexpr std::string_view in(std::string_view source) const noexcept { return source.substr(begin, length); }
    constexpr bool operator==(const Span& o) const noexcept { return begin == o.begin && length == o.length; }
    constexpr bool operator!=(const Span& o) const noexcept { return !(*this == o); }
};

struct LineColumn {
    std::uint32_t line = 1;
    std::uint32_t column = 1;
};

class LineIndex {
public:
    LineIndex() = default;

    explicit LineIndex(std::string_view source) { rebuild(source); }

    void rebuild(std::string_view source) {
        m_starts.clear();
        m_starts.push_back(0);
        for (std::size_t i = 0; i < source.size(); ++i) if (source[i] == '\n') m_starts.push_back(static_cast<std::uint32_t>(i + 1));
    }

    std::size_t line_count() const noexcept { return m_starts.size(); }

    std::uint32_t line_of(std::uint32_t offset) const noexcept {
        std::size_t lo = 0, hi = m_starts.size();
        while (hi - lo > 1) {
            const std::size_t mid = lo + (hi - lo) / 2;
            if (m_starts[mid] <= offset) lo = mid;
            else hi = mid;
        }
        return static_cast<std::uint32_t>(lo);
    }

    Span line_span(std::string_view source, std::uint32_t line_zero_based) const noexcept {
        if (line_zero_based >= m_starts.size()) return Span(static_cast<std::uint32_t>(source.size()), 0);
        const std::uint32_t b = m_starts[line_zero_based];
        std::uint32_t e = line_zero_based + 1 < m_starts.size() ? m_starts[line_zero_based + 1] - 1 : static_cast<std::uint32_t>(source.size());
        if (e > b && source[e - 1] == '\r') --e;
        return Span::between(b, e);
    }

    LineColumn locate(std::string_view source, std::uint32_t offset) const noexcept {
        if (offset > source.size()) offset = static_cast<std::uint32_t>(source.size());
        const std::uint32_t line = line_of(offset);
        LineColumn lc;
        lc.line = line + 1;
        lc.column = static_cast<std::uint32_t>(utf8::count_codepoints(source.substr(m_starts[line], offset - m_starts[line])) + 1);
        return lc;
    }

private:
    std::vector<std::uint32_t> m_starts{ 0 };
};

} // namespace mathtext
} // namespace fizmo

#endif // FIZMO_MATHTEXT_SPAN_HPP
