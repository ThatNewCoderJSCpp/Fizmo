#ifndef FIZMO_MATHTEXT_SYMBOL_TYPES_HPP
#define FIZMO_MATHTEXT_SYMBOL_TYPES_HPP

#include <cstddef>
#include <cstdint>
#include <initializer_list>

namespace fizmo {
namespace mathtext {

#define FIZMO_MATHTEXT_SYMBOL_CLASSES(X) \
    X(GreekLower) \
    X(GreekUpper) \
    X(Letterlike) \
    X(BinaryOperator) \
    X(Relation) \
    X(NegatedRelation) \
    X(Arrow) \
    X(LargeOperator) \
    X(Delimiter) \
    X(Ordinary) \
    X(Space) \
    X(Punctuation)

enum class SymbolClass : std::uint8_t {
#define FIZMO_MATHTEXT_X(name) name,
    FIZMO_MATHTEXT_SYMBOL_CLASSES(FIZMO_MATHTEXT_X)
#undef FIZMO_MATHTEXT_X
};

inline constexpr const char* symbol_class_name(SymbolClass c) noexcept {
    switch (c) {
#define FIZMO_MATHTEXT_X(name) case SymbolClass::name: return #name;
        FIZMO_MATHTEXT_SYMBOL_CLASSES(FIZMO_MATHTEXT_X)
#undef FIZMO_MATHTEXT_X
    }
    return "Ordinary";
}

inline constexpr std::size_t kMaxSymbolSpellings = 12;

class SymbolNames {
public:
    constexpr SymbolNames(const char* name) noexcept : m_items{ name }, m_count(1) {}

    constexpr SymbolNames(std::initializer_list<const char*> names) noexcept : m_items{}, m_count(0) {
        for (const char* n : names) {
            if (m_count == kMaxSymbolSpellings) break;
            m_items[m_count++] = n;
        }
    }

    constexpr std::size_t size() const noexcept { return m_count; }
    constexpr const char* operator[](std::size_t i) const noexcept { return m_items[i]; }
    constexpr const char* const* begin() const noexcept { return m_items; }
    constexpr const char* const* end() const noexcept { return m_items + m_count; }
    constexpr const char* primary() const noexcept { return m_items[0]; }

private:
    const char* m_items[kMaxSymbolSpellings];
    std::size_t m_count;
};

struct SymbolInfo {
    SymbolNames   names;
    const char*   utf8;
    std::uint32_t codepoint;
    SymbolClass   cls;

    constexpr const char* name() const noexcept { return names.primary(); }
};

struct SymbolGroup {
    const SymbolInfo* items;
    std::size_t       count;
    SymbolClass       cls;
};

inline constexpr std::uint32_t kNoSymbol = 0xFFFFFFFFu;

inline constexpr bool is_relation_class(SymbolClass c) noexcept { return c == SymbolClass::Relation || c == SymbolClass::NegatedRelation || c == SymbolClass::Arrow; }

} // namespace mathtext
} // namespace fizmo

#endif // FIZMO_MATHTEXT_SYMBOL_TYPES_HPP
