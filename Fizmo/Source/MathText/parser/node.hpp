#ifndef FIZMO_MATHTEXT_NODE_HPP
#define FIZMO_MATHTEXT_NODE_HPP

#include <cstddef>
#include <cstdint>
#include "../core/span.hpp"

namespace fizmo {
namespace mathtext {

#define FIZMO_MATHTEXT_NODES(X) \
    X(Document) \
    X(Call) \
    X(Number) \
    X(String) \
    X(TextRun) \
    X(Symbol) \
    X(Negate) \
    X(Error)

enum class NodeKind : std::uint8_t {
#define FIZMO_MATHTEXT_X(name) name,
    FIZMO_MATHTEXT_NODES(FIZMO_MATHTEXT_X)
#undef FIZMO_MATHTEXT_X
};

inline constexpr const char* node_kind_name(NodeKind k) noexcept {
    switch (k) {
#define FIZMO_MATHTEXT_X(name) case NodeKind::name: return #name;
        FIZMO_MATHTEXT_NODES(FIZMO_MATHTEXT_X)
#undef FIZMO_MATHTEXT_X
    }
    return "Unknown";
}

using NodeId = std::uint32_t;

inline constexpr NodeId kNoNode = 0xFFFFFFFFu;

namespace node_flags {
inline constexpr std::uint8_t Command = 1u << 0;
inline constexpr std::uint8_t SingleQuoted = 1u << 1;
inline constexpr std::uint8_t KnownSymbol = 1u << 2;
inline constexpr std::uint8_t SyntheticName = 1u << 3;
inline constexpr std::uint8_t Negative = 1u << 4;
inline constexpr std::uint8_t Variable = 1u << 5;
inline constexpr std::uint8_t Protected = 1u << 6;
} // namespace node_flags

struct Node {
    NodeKind      kind = NodeKind::Error;
    std::uint8_t  flags = 0;
    char          marker = 0;
    Span          span;
    Span          name;
    Span          exponent;
    std::uint32_t first_child = 0;
    std::uint32_t child_count = 0;
    std::uint32_t value = 0;
    std::uint32_t text_offset = 0;
    std::uint32_t text_length = 0;

    bool has(std::uint8_t f) const noexcept { return (flags & f) != 0; }
    bool is(NodeKind k) const noexcept { return kind == k; }
};

class NodeRange {
public:
    NodeRange() noexcept = default;
    NodeRange(const NodeId* b, std::size_t n) noexcept : m_begin(b), m_size(n) {}
    const NodeId* begin() const noexcept { return m_begin; }
    const NodeId* end() const noexcept { return m_begin + m_size; }
    std::size_t size() const noexcept { return m_size; }
    bool empty() const noexcept { return m_size == 0; }
    NodeId operator[](std::size_t i) const noexcept { return m_begin[i]; }

private:
    const NodeId* m_begin = nullptr;
    std::size_t   m_size = 0;
};

} // namespace mathtext
} // namespace fizmo

#endif // FIZMO_MATHTEXT_NODE_HPP
