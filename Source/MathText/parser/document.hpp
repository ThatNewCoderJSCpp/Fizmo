#ifndef FIZMO_MATHTEXT_DOCUMENT_HPP
#define FIZMO_MATHTEXT_DOCUMENT_HPP

#include <string>
#include <string_view>
#include <vector>
#include "node.hpp"
#include "../core/diagnostic.hpp"
#include "../core/symbols.hpp"

namespace fizmo {
namespace mathtext {

class Builder;

class Document {
public:
    Document() { m_nodes.push_back(root_node()); }

    const std::string& source() const noexcept { return m_source; }
    NodeId root() const noexcept { return 0; }
    std::size_t node_count() const noexcept { return m_nodes.size(); }
    const Node& node(NodeId id) const noexcept { return m_nodes[id]; }
    NodeKind kind(NodeId id) const noexcept { return m_nodes[id].kind; }

    NodeRange children(NodeId id) const noexcept {
        const Node& n = m_nodes[id];
        return n.child_count ? NodeRange(m_children.data() + n.first_child, n.child_count) : NodeRange();
    }

    NodeRange top_level() const noexcept { return children(root()); }

    std::string_view text(Span s) const noexcept { return s.in(m_source); }
    std::string_view source_text(NodeId id) const noexcept { return text(m_nodes[id].span); }
    std::string_view name(NodeId id) const noexcept {
        const Node& n = m_nodes[id];
        if (n.has(node_flags::SyntheticName)) return std::string_view(m_strings).substr(n.text_offset, n.text_length);
        return text(n.name);
    }

    bool is_call(NodeId id, std::string_view call_name) const noexcept { return m_nodes[id].is(NodeKind::Call) && !m_nodes[id].has(node_flags::Command) && name(id) == call_name; }

    std::string_view decoded_text(NodeId id) const noexcept {
        const Node& n = m_nodes[id];
        return std::string_view(m_strings).substr(n.text_offset, n.text_length);
    }

    std::string command_text(NodeId id) const {
        const Node& n = m_nodes[id];
        if (n.is(NodeKind::Symbol) && !n.has(node_flags::SyntheticName)) return std::string(source_text(id));
        std::string out = "\\";
        out += name(id);
        out += "\\";
        return out;
    }

    bool is_known_symbol(NodeId id) const noexcept { return m_nodes[id].is(NodeKind::Symbol) && m_nodes[id].has(node_flags::KnownSymbol); }
    const SymbolInfo* symbol(NodeId id) const noexcept { return is_known_symbol(id) ? &symbol_info(m_nodes[id].value) : nullptr; }

    const Diagnostic* error_of(NodeId id) const noexcept {
        const Node& n = m_nodes[id];
        return n.is(NodeKind::Error) && n.value < m_diagnostics.size() ? &m_diagnostics[n.value] : nullptr;
    }

    const std::vector<Diagnostic>& diagnostics() const noexcept { return m_diagnostics; }

    std::size_t error_count() const noexcept {
        std::size_t c = 0;
        for (const Diagnostic& d : m_diagnostics) if (d.severity == Severity::Error) ++c;
        return c;
    }

    bool ok() const noexcept { return error_count() == 0; }

    LineColumn locate(std::uint32_t offset) const noexcept { return m_lines.locate(m_source, offset); }
    std::string_view line_text(std::uint32_t line_one_based) const noexcept { return text(m_lines.line_span(m_source, line_one_based - 1)); }

    template <typename Fn>
    void walk(Fn&& fn) const { walk_from(root(), 0, fn); }

    template <typename Fn>
    void walk_from(NodeId id, std::size_t depth, Fn& fn) const {
        fn(id, depth);
        for (NodeId c : children(id)) walk_from(c, depth + 1, fn);
    }

private:
    friend class Builder;

    static Node root_node() noexcept {
        Node n;
        n.kind = NodeKind::Document;
        return n;
    }

    std::string             m_source;
    std::vector<Node>       m_nodes;
    std::vector<NodeId>     m_children;
    std::string             m_strings;
    std::vector<Diagnostic> m_diagnostics;
    LineIndex               m_lines;
};

} // namespace mathtext
} // namespace fizmo

#endif // FIZMO_MATHTEXT_DOCUMENT_HPP
