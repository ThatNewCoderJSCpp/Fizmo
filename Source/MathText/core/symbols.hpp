#ifndef FIZMO_MATHTEXT_SYMBOLS_HPP
#define FIZMO_MATHTEXT_SYMBOLS_HPP

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <string_view>
#include <vector>
#include "spelling.hpp"
#include "symbols/symbol_types.hpp"
#include "symbols/greek.hpp"
#include "symbols/letterlike.hpp"
#include "symbols/operators.hpp"
#include "symbols/relations.hpp"
#include "symbols/arrows.hpp"
#include "symbols/large_operators.hpp"
#include "symbols/delimiters.hpp"
#include "symbols/ordinary.hpp"
#include "symbols/spacing.hpp"

namespace fizmo {
namespace mathtext {

template <std::size_t N>
inline constexpr SymbolGroup make_symbol_group(const SymbolInfo (&items)[N], SymbolClass cls) noexcept { return SymbolGroup{ items, N, cls }; }

inline constexpr SymbolGroup kSymbolGroups[] = {
    make_symbol_group(kGreekLowerSymbols, SymbolClass::GreekLower),
    make_symbol_group(kGreekUpperSymbols, SymbolClass::GreekUpper),
    make_symbol_group(kLetterlikeSymbols, SymbolClass::Letterlike),
    make_symbol_group(kBinaryOperatorSymbols, SymbolClass::BinaryOperator),
    make_symbol_group(kRelationSymbols, SymbolClass::Relation),
    make_symbol_group(kNegatedRelationSymbols, SymbolClass::NegatedRelation),
    make_symbol_group(kArrowSymbols, SymbolClass::Arrow),
    make_symbol_group(kLargeOperatorSymbols, SymbolClass::LargeOperator),
    make_symbol_group(kDelimiterSymbols, SymbolClass::Delimiter),
    make_symbol_group(kOrdinarySymbols, SymbolClass::Ordinary),
    make_symbol_group(kSpaceSymbols, SymbolClass::Space),
    make_symbol_group(kPunctuationSymbols, SymbolClass::Punctuation),
};

inline constexpr std::size_t symbol_group_count() noexcept { return sizeof(kSymbolGroups) / sizeof(kSymbolGroups[0]); }

inline constexpr std::size_t symbol_count() noexcept {
    std::size_t n = 0;
    for (const SymbolGroup& g : kSymbolGroups) n += g.count;
    return n;
}

inline constexpr const SymbolInfo& symbol_info(std::uint32_t index) noexcept {
    std::size_t i = index;
    for (const SymbolGroup& g : kSymbolGroups) {
        if (i < g.count) return g.items[i];
        i -= g.count;
    }
    return kSymbolGroups[0].items[0];
}

inline constexpr const SymbolGroup* symbol_group(SymbolClass cls) noexcept {
    for (const SymbolGroup& g : kSymbolGroups) if (g.cls == cls) return &g;
    return nullptr;
}

inline constexpr std::uint32_t first_symbol_index(SymbolClass cls) noexcept {
    std::uint32_t at = 0;
    for (const SymbolGroup& g : kSymbolGroups) {
        if (g.cls == cls) return at;
        at += static_cast<std::uint32_t>(g.count);
    }
    return kNoSymbol;
}

namespace detail {

class SymbolIndex {
public:
    struct Spelling {
        const char*   text;
        std::uint32_t index;
    };

    static const SymbolIndex& get() {
        static const SymbolIndex index;
        return index;
    }

    std::uint32_t find(std::string_view name) const noexcept {
        std::size_t lo = 0, hi = m_by_name.size();
        while (lo < hi) {
            const std::size_t mid = lo + (hi - lo) / 2;
            const int c = spelling_compare(m_by_name[mid].text, name);
            if (c == 0) return m_by_name[mid].index;
            if (c < 0) lo = mid + 1;
            else hi = mid;
        }
        return kNoSymbol;
    }

    std::uint32_t find_codepoint(std::uint32_t codepoint) const noexcept {
        std::size_t lo = 0, hi = m_by_codepoint.size();
        while (lo < hi) {
            const std::size_t mid = lo + (hi - lo) / 2;
            if (symbol_info(m_by_codepoint[mid]).codepoint < codepoint) lo = mid + 1;
            else hi = mid;
        }
        return lo < m_by_codepoint.size() && symbol_info(m_by_codepoint[lo]).codepoint == codepoint ? m_by_codepoint[lo] : kNoSymbol;
    }

    const std::vector<Spelling>& spellings() const noexcept { return m_by_name; }

private:
    SymbolIndex() {
        const std::uint32_t n = static_cast<std::uint32_t>(symbol_count());
        for (std::uint32_t i = 0; i < n; ++i) {
            for (const char* spelling : symbol_info(i).names) m_by_name.push_back(Spelling{ spelling, i });
            const char c = symbol_info(i).name()[0];
            if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')) m_by_codepoint.push_back(i);
        }
        std::sort(m_by_name.begin(), m_by_name.end(), [](const Spelling& a, const Spelling& b) { return spelling_compare(a.text, b.text) < 0; });
        std::stable_sort(m_by_codepoint.begin(), m_by_codepoint.end(), [](std::uint32_t a, std::uint32_t b) { return symbol_info(a).codepoint < symbol_info(b).codepoint; });
    }

    std::vector<Spelling>      m_by_name;
    std::vector<std::uint32_t> m_by_codepoint;
};

} // namespace detail

inline std::uint32_t find_symbol(std::string_view name) noexcept { return detail::SymbolIndex::get().find(name); }

inline std::uint32_t find_symbol_by_codepoint(std::uint32_t codepoint) noexcept { return detail::SymbolIndex::get().find_codepoint(codepoint); }

inline const char* canonical_symbol_name(std::string_view spelling) noexcept {
    const std::uint32_t index = find_symbol(spelling);
    return index == kNoSymbol ? nullptr : symbol_info(index).name();
}

inline bool same_symbol(std::string_view a, std::string_view b) noexcept {
    const std::uint32_t ia = find_symbol(a);
    return ia != kNoSymbol && ia == find_symbol(b);
}

} // namespace mathtext
} // namespace fizmo

#endif // FIZMO_MATHTEXT_SYMBOLS_HPP
