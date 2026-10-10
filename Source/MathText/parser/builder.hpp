#ifndef FIZMO_MATHTEXT_BUILDER_HPP
#define FIZMO_MATHTEXT_BUILDER_HPP

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>
#include "document.hpp"
#include "../lexer/chars.hpp"
#include "../lexer/number_scan.hpp"
#include "../lexer/command_scan.hpp"

namespace fizmo {
namespace mathtext {

inline Span command_name(Span token) noexcept { return token.length >= 2 ? Span(token.begin + 1, token.length - 2) : Span(token.begin, 0); }

class Builder {
public:
    struct Mark {
        std::size_t nodes, children, strings, diagnostics;
    };

    Builder(Document& doc, std::string_view source, std::size_t max_quoted = 40) : m_doc(doc), m_max_quoted(max_quoted) {
        doc.m_source.assign(source.data(), source.size());
        doc.m_lines.rebuild(doc.m_source);
        m_src = doc.m_source;
    }

    std::string_view source() const noexcept { return m_src; }
    Document& document() noexcept { return m_doc; }
    const Node& node(NodeId id) const noexcept { return m_doc.m_nodes[id]; }
    Node& node(NodeId id) noexcept { return m_doc.m_nodes[id]; }

    Mark mark() const noexcept { return Mark{ m_doc.m_nodes.size(), m_doc.m_children.size(), m_doc.m_strings.size(), m_doc.m_diagnostics.size() }; }

    void rollback(const Mark& m) {
        m_doc.m_nodes.resize(m.nodes);
        m_doc.m_children.resize(m.children);
        m_doc.m_strings.resize(m.strings);
        m_doc.m_diagnostics.resize(m.diagnostics);
    }

    NodeId add(const Node& n) {
        m_doc.m_nodes.push_back(n);
        return static_cast<NodeId>(m_doc.m_nodes.size() - 1);
    }

    void attach(NodeId parent, const NodeId* kids, std::size_t count) {
        Node& p = m_doc.m_nodes[parent];
        p.first_child = static_cast<std::uint32_t>(m_doc.m_children.size());
        p.child_count = static_cast<std::uint32_t>(count);
        m_doc.m_children.insert(m_doc.m_children.end(), kids, kids + count);
    }

    void attach(NodeId parent, const std::vector<NodeId>& kids) { attach(parent, kids.data(), kids.size()); }

    void finish_root(const std::vector<NodeId>& items) {
        m_doc.m_nodes[m_doc.root()].span = Span::between(0, m_src.size());
        attach(m_doc.root(), items);
    }

    std::uint32_t intern(std::string_view text) {
        const std::uint32_t at = static_cast<std::uint32_t>(m_doc.m_strings.size());
        m_doc.m_strings.append(text.data(), text.size());
        return at;
    }

    std::uint32_t diagnose(DiagnosticCode code, Span where, Span quoted = Span()) {
        Diagnostic d;
        d.code = code;
        d.severity = diagnostic_severity(code);
        d.span = where;
        d.message = diagnostic_text(code);
        if (!quoted.empty()) {
            std::string_view q = quoted.in(m_src);
            const bool cut = q.size() > m_max_quoted;
            if (cut) {
                std::size_t k = m_max_quoted;
                while (k > 0 && utf8::is_continuation(static_cast<unsigned char>(q[k]))) --k;
                q = q.substr(0, k);
            }
            d.message += ": ";
            d.message.append(q.data(), q.size());
            if (cut) d.message += "...";
        }
        m_doc.m_diagnostics.push_back(std::move(d));
        return static_cast<std::uint32_t>(m_doc.m_diagnostics.size() - 1);
    }

    Span trimmed(std::size_t b, std::size_t e) const noexcept {
        while (e > b && chars::is_space(m_src[e - 1])) --e;
        while (b < e && chars::is_space(m_src[b])) ++b;
        return Span::between(b, e);
    }

    NodeId error(std::size_t b, std::size_t e, DiagnosticCode code, Span where) {
        const Span raw = trimmed(b, e);
        Node n;
        n.kind = NodeKind::Error;
        n.span = raw;
        n.value = diagnose(code, where, raw);
        return add(n);
    }

    NodeId error(Span raw, DiagnosticCode code) { return error(raw.begin, raw.end(), code, raw); }

    NodeId number(Span token) {
        const NumberShape shape = analyze_number(token.in(m_src));
        Node n;
        n.kind = NodeKind::Number;
        n.span = token;
        n.name = Span::between(token.begin, token.begin + shape.mantissa_end);
        n.marker = shape.marker;
        if (shape.marker) n.exponent = Span::between(token.begin + shape.exponent_begin, token.end());
        return add(n);
    }

    NodeId symbol(Span whole, Span name, bool report) {
        Node n;
        n.kind = NodeKind::Symbol;
        n.span = whole;
        n.name = name;
        const std::uint32_t index = find_symbol(name.in(m_src));
        if (index != kNoSymbol) {
            n.flags = node_flags::KnownSymbol;
            n.value = index;
        } else {
            n.value = kNoSymbol;
            if (report) diagnose(DiagnosticCode::UnknownCommand, whole, whole);
        }
        return add(n);
    }

    NodeId symbol_by_index(Span whole, std::uint32_t index) {
        Node n;
        n.kind = NodeKind::Symbol;
        n.span = whole;
        n.name = whole;
        n.flags = node_flags::KnownSymbol | node_flags::SyntheticName;
        n.value = index;
        const std::string_view nm = symbol_info(index).name();
        n.text_offset = intern(nm);
        n.text_length = static_cast<std::uint32_t>(nm.size());
        return add(n);
    }

    NodeId text_run(std::size_t raw_begin, std::size_t raw_end, std::string& buffer) {
        Node n;
        n.kind = NodeKind::TextRun;
        n.span = Span::between(raw_begin, raw_end);
        n.text_offset = intern(buffer);
        n.text_length = static_cast<std::uint32_t>(buffer.size());
        buffer.clear();
        return add(n);
    }

    NodeId string_literal(Span token) {
        const std::size_t b = token.begin + 1;
        const std::size_t e = token.end() - 1;
        std::vector<NodeId> runs;
        std::string buffer;
        std::size_t run_start = b;
        std::size_t i = b;
        while (i < e) {
            const char c = m_src[i];
            if (c != '\\') {
                buffer.push_back(c);
                ++i;
                continue;
            }
            if (is_string_escape(m_src, i, e)) {
                buffer.push_back(m_src[i + 1]);
                i += 2;
                continue;
            }
            const CommandScan cs = scan_command(m_src, i, e);
            if (cs.shape == CommandShape::Lone) {
                diagnose(DiagnosticCode::TrailingBackslash, Span::between(i, i + 1));
                buffer.push_back('\\');
                ++i;
                continue;
            }
            if (i > run_start) runs.push_back(text_run(run_start, i, buffer));
            if (cs.shape == CommandShape::Closed) {
                runs.push_back(symbol(Span::between(i, cs.end), Span::between(cs.name_begin, cs.name_end), true));
            } else {
                const NodeId bad = symbol(Span::between(i, cs.end), Span::between(cs.name_begin, cs.name_end), false);
                m_doc.m_nodes[bad].flags = static_cast<std::uint8_t>(m_doc.m_nodes[bad].flags & ~node_flags::KnownSymbol);
                m_doc.m_nodes[bad].value = kNoSymbol;
                diagnose(DiagnosticCode::UnclosedCommand, Span::between(i, cs.end), Span::between(i, cs.end));
                runs.push_back(bad);
            }
            i = cs.end;
            run_start = cs.end;
        }
        if (i > run_start || runs.empty()) runs.push_back(text_run(run_start, e, buffer));
        Node n;
        n.kind = NodeKind::String;
        n.flags = m_src[token.begin] == '\'' ? node_flags::SingleQuoted : 0;
        n.span = token;
        n.name = Span::between(b, e);
        const NodeId id = add(n);
        attach(id, runs);
        return id;
    }

    NodeId variable(Span token) {
        std::string buffer(token.in(m_src));
        const NodeId run = text_run(token.begin, token.end(), buffer);
        Node n;
        n.kind = NodeKind::String;
        n.flags = node_flags::Variable;
        n.span = token;
        n.name = token;
        const NodeId id = add(n);
        attach(id, &run, 1);
        return id;
    }

    NodeId call(Span whole, Span name, bool command, const std::vector<NodeId>& args) {
        Node n;
        n.kind = NodeKind::Call;
        n.flags = command ? node_flags::Command : 0;
        n.span = whole;
        n.name = name;
        const NodeId id = add(n);
        attach(id, args);
        return id;
    }

    NodeId synthetic_call(std::string_view name, Span whole, const NodeId* args, std::size_t count) {
        Node n;
        n.kind = NodeKind::Call;
        n.flags = node_flags::SyntheticName;
        n.span = whole;
        n.text_offset = intern(name);
        n.text_length = static_cast<std::uint32_t>(name.size());
        const NodeId id = add(n);
        attach(id, args, count);
        return id;
    }

    NodeId synthetic_call(std::string_view name, Span whole, const std::vector<NodeId>& args) { return synthetic_call(name, whole, args.data(), args.size()); }

    NodeId negate(Span whole, NodeId child) {
        Node n;
        n.kind = NodeKind::Negate;
        n.span = whole;
        const NodeId id = add(n);
        attach(id, &child, 1);
        return id;
    }

private:
    Document&        m_doc;
    std::string_view m_src;
    std::size_t      m_max_quoted;
};

} // namespace mathtext
} // namespace fizmo

#endif // FIZMO_MATHTEXT_BUILDER_HPP
