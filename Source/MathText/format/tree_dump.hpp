#ifndef FIZMO_MATHTEXT_TREE_DUMP_HPP
#define FIZMO_MATHTEXT_TREE_DUMP_HPP

#include <string>
#include "number_format.hpp"

namespace fizmo {
namespace mathtext {

inline void dump_quoted(std::string& out, std::string_view s) {
    out.push_back('"');
    for (char c : s) {
        if (c == '\n') out += "\\n";
        else if (c == '\t') out += "\\t";
        else if (c == '"' || c == '\\') { out.push_back('\\'); out.push_back(c); }
        else out.push_back(c);
    }
    out.push_back('"');
}

inline std::string dump_tree(const Document& doc, bool with_locations = true) {
    std::string out;
    doc.walk([&](NodeId id, std::size_t depth) {
        const Node& n = doc.node(id);
        out.append(depth * 2, ' ');
        out += node_kind_name(n.kind);
        switch (n.kind) {
            case NodeKind::Call:
                out.push_back(' ');
                if (n.has(node_flags::Command)) out += doc.command_text(id);
                else out += doc.name(id);
                out += " (" + std::to_string(n.child_count) + (n.child_count == 1 ? " arg)" : " args)");
                break;
            case NodeKind::Number: {
                const NumberParts p = number_parts(doc, id);
                out.push_back(' ');
                out += doc.source_text(id);
                out += " [mantissa ";
                if (p.negative) out.push_back('-');
                out += p.mantissa;
                if (p.has_exponent) {
                    out += ", exponent ";
                    if (p.exponent_negative && p.exponent_digits != "0") out.push_back('-');
                    out += p.exponent_digits;
                }
                out.push_back(']');
                break;
            }
            case NodeKind::String:
                out.push_back(' ');
                out += doc.source_text(id);
                break;
            case NodeKind::TextRun:
                out.push_back(' ');
                dump_quoted(out, doc.decoded_text(id));
                break;
            case NodeKind::Symbol:
                out.push_back(' ');
                out += doc.command_text(id);
                if (const SymbolInfo* s = doc.symbol(id)) {
                    out += " -> ";
                    out += s->utf8;
                    out += " (";
                    out += symbol_class_name(s->cls);
                    out.push_back(')');
                } else {
                    out += " (unknown)";
                }
                break;
            case NodeKind::Error:
                out.push_back(' ');
                dump_quoted(out, doc.source_text(id));
                if (const Diagnostic* d = doc.error_of(id)) {
                    out += " <";
                    out += diagnostic_name(d->code);
                    out.push_back('>');
                }
                break;
            case NodeKind::Negate:
            case NodeKind::Document:
                break;
        }
        if (with_locations && n.kind != NodeKind::Document) {
            const LineColumn lc = doc.locate(n.span.begin);
            out += " @" + std::to_string(lc.line) + ":" + std::to_string(lc.column);
        }
        out.push_back('\n');
    });
    return out;
}

inline std::string format_diagnostic(const Document& doc, const Diagnostic& d, std::string_view source_name = "input") {
    const LineColumn lc = doc.locate(d.span.begin);
    std::string out;
    out.append(source_name.data(), source_name.size());
    out += ":" + std::to_string(lc.line) + ":" + std::to_string(lc.column) + ": ";
    out += severity_name(d.severity);
    out += ": ";
    out += d.message;
    out.push_back('\n');
    const std::string_view line = doc.line_text(lc.line);
    out += "    ";
    out += line;
    out += "\n    ";
    std::size_t col = 1;
    for (std::size_t i = 0; i < line.size() && col < lc.column; i = utf8::advance(line, i), ++col) out.push_back(line[i] == '\t' ? '\t' : ' ');
    out.push_back('^');
    const std::size_t line_end_offset = static_cast<std::size_t>(line.data() - doc.source().data()) + line.size();
    const std::size_t span_end = d.span.end() < line_end_offset ? d.span.end() : line_end_offset;
    if (span_end > d.span.begin) {
        const std::size_t width = utf8::count_codepoints(doc.text(Span::between(d.span.begin, span_end)));
        if (width > 1) out.append(width - 1, '~');
    }
    out.push_back('\n');
    return out;
}

inline std::string format_diagnostics(const Document& doc, std::string_view source_name = "input") {
    std::string out;
    for (const Diagnostic& d : doc.diagnostics()) out += format_diagnostic(doc, d, source_name);
    return out;
}

} // namespace mathtext
} // namespace fizmo

#endif // FIZMO_MATHTEXT_TREE_DUMP_HPP
