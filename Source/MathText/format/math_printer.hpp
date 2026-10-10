#ifndef FIZMO_MATHTEXT_MATH_PRINTER_HPP
#define FIZMO_MATHTEXT_MATH_PRINTER_HPP

#include <cstddef>
#include <string>
#include <string_view>
#include "printer.hpp"
#include "../math/math_precedence.hpp"

namespace fizmo {
namespace mathtext {

struct MathPrintOptions {
    NumberStyle      numbers = NumberStyle::AsWritten;
    std::string_view times_ten = "*10^";
    bool             unicode_symbols = false;
    bool             spaces_around_operators = true;
    std::string_view item_separator = "; ";
};

class MathPrinter {
public:
    MathPrinter(const Document& doc, Sink& sink, const MathPrintOptions& options = MathPrintOptions()) noexcept : m_doc(doc), m_sink(sink), m_opt(options) {}

    void print() {
        bool first = true;
        for (NodeId id : m_doc.top_level()) {
            if (!first) m_sink.write(m_opt.item_separator, TextStyle::Punctuation);
            first = false;
            node(id);
        }
    }

    void print(NodeId id) {
        if (m_doc.kind(id) == NodeKind::Document) print();
        else node(id);
    }

    static bool is_identifier(std::string_view s) noexcept {
        if (s.empty() || !chars::is_letter(s[0])) return false;
        for (char c : s) if (!chars::is_letter(c) && !chars::is_digit(c)) return false;
        return true;
    }

    int precedence_of(NodeId id) const noexcept { return math_precedence(m_doc, id); }

private:
    std::string_view relation_text(std::string_view nm) const noexcept {
        if (nm == names::Equals) return "=";
        if (nm == names::Less) return "<";
        if (nm == names::Greater) return ">";
        if (nm == names::LessEqual) return m_opt.unicode_symbols ? "\xE2\x89\xA4" : "<=";
        if (nm == names::GreaterEqual) return m_opt.unicode_symbols ? "\xE2\x89\xA5" : ">=";
        return m_opt.unicode_symbols ? "\xE2\x89\xA0" : "!=";
    }

    void op(std::string_view text) {
        if (m_opt.spaces_around_operators) m_sink.write(" ", TextStyle::Plain);
        m_sink.write(text, TextStyle::Punctuation);
        if (m_opt.spaces_around_operators) m_sink.write(" ", TextStyle::Plain);
    }

    void tight(std::string_view text) { m_sink.write(text, TextStyle::Punctuation); }

    bool negative_like(NodeId id) const noexcept { return is_negative_like(m_doc, id); }

    void child(NodeId id, int required) {
        if (precedence_of(id) < required) {
            m_sink.write("(", TextStyle::Punctuation);
            node(id);
            m_sink.write(")", TextStyle::Punctuation);
            return;
        }
        node(id);
    }

    bool script_atom(NodeId id) const noexcept {
        switch (m_doc.kind(id)) {
            case NodeKind::Number: return !negative_like(id);
            case NodeKind::String:
            case NodeKind::Symbol: return true;
            case NodeKind::Call: return precedence_of(id) == precedence::Atom && !is_named(m_doc, id, names::Parens);
            default: return false;
        }
    }

    void braced(NodeId id) {
        m_sink.write("{", TextStyle::Punctuation);
        node(id);
        m_sink.write("}", TextStyle::Punctuation);
    }

    void script(NodeId id) {
        if (script_atom(id)) node(id);
        else braced(id);
    }

    void operand(NodeId id, int required) {
        if (is_named(m_doc, id, names::Parens) && m_doc.node(id).child_count == 1) braced(id);
        else child(id, required);
    }

    void symbol(NodeId id) {
        const SymbolInfo* info = m_doc.symbol(id);
        if (info && (info->codepoint == 0x2B || info->codepoint == 0x2212) && info->cls == SymbolClass::Ordinary) {
            m_sink.write("{", TextStyle::Punctuation);
            m_sink.write(info->codepoint == 0x2B ? "+" : "-", TextStyle::Symbol);
            m_sink.write("}", TextStyle::Punctuation);
            return;
        }
        if (info && m_opt.unicode_symbols && info->codepoint >= 0x80 && info->cls != SymbolClass::Space) {
            m_sink.write(info->utf8, TextStyle::Symbol);
            return;
        }
        m_sink.write(m_doc.command_text(id), info ? TextStyle::Symbol : TextStyle::Error);
    }

    void string(NodeId id) {
        const NodeRange runs = m_doc.children(id);
        if (runs.size() == 1 && m_doc.kind(runs[0]) == NodeKind::TextRun && is_identifier(m_doc.decoded_text(runs[0]))) {
            m_sink.write(m_doc.decoded_text(runs[0]), TextStyle::Name);
            return;
        }
        const char quote = m_doc.node(id).has(node_flags::SingleQuoted) ? '\'' : '"';
        const char q[1] = { quote };
        m_sink.write(std::string_view(q, 1), TextStyle::String);
        for (NodeId c : runs) {
            if (m_doc.kind(c) == NodeKind::Symbol) {
                m_sink.write(m_doc.command_text(c), m_doc.symbol(c) ? TextStyle::Symbol : TextStyle::Error);
                continue;
            }
            const std::string_view text = m_doc.decoded_text(c);
            std::size_t run = 0;
            for (std::size_t i = 0; i < text.size(); ++i) {
                if (text[i] != '\\' && text[i] != quote) continue;
                if (i > run) m_sink.write(text.substr(run, i - run), TextStyle::String);
                const char esc[2] = { '\\', text[i] };
                m_sink.write(std::string_view(esc, 2), TextStyle::String);
                run = i + 1;
            }
            if (run < text.size()) m_sink.write(text.substr(run), TextStyle::String);
        }
        m_sink.write(std::string_view(q, 1), TextStyle::String);
    }

    void arguments(NodeRange args, bool call_arguments = true) {
        m_sink.write("(", TextStyle::Punctuation);
        for (std::size_t i = 0; i < args.size(); ++i) {
            if (i) m_sink.write(", ", TextStyle::Punctuation);
            if (call_arguments && is_named(m_doc, args[i], names::Parens) && m_doc.node(args[i]).child_count == 1) braced(args[i]);
            else node(args[i]);
        }
        m_sink.write(")", TextStyle::Punctuation);
    }

    void call(NodeId id) {
        const Node& n = m_doc.node(id);
        const NodeRange a = m_doc.children(id);
        const std::string_view nm = n.has(node_flags::Command) ? m_doc.name(id) : canonical_call(m_doc.name(id));
        const int p = precedence_of(id);
        if (!n.has(node_flags::Command) && p != precedence::Atom) {
            if (nm == names::Add || nm == names::Multiply || nm == names::ImplicitMultiply) {
                for (std::size_t i = 0; i < a.size(); ++i) {
                    if (i) {
                        if (nm == names::Add) op("+");
                        else if (nm == names::Multiply) op(m_opt.unicode_symbols ? "\xC2\xB7" : "*");
                        else m_sink.write(" ", TextStyle::Plain);
                    }
                    if (i && nm == names::ImplicitMultiply && negative_like(a[i])) {
                        m_sink.write("(", TextStyle::Punctuation);
                        node(a[i]);
                        m_sink.write(")", TextStyle::Punctuation);
                    } else {
                        child(a[i], i ? p + 1 : p);
                    }
                }
                return;
            }
            if (nm == names::Subtract) { child(a[0], p); op(m_opt.unicode_symbols ? "\xE2\x88\x92" : "-"); child(a[1], p + 1); return; }
            if (nm == names::Fraction) { operand(a[0], p); tight("/"); operand(a[1], p + 1); return; }
            if (nm == names::Power) { child(a[0], p + 1); tight("^"); script(a[1]); return; }
            if (nm == names::Subscript) { child(a[0], p); tight("_"); script(a[1]); return; }
            if (nm == names::Factorial) { child(a[0], p); tight("!"); return; }
            if (nm == names::Negate) { tight(m_opt.unicode_symbols ? "\xE2\x88\x92" : "-"); child(a[0], p); return; }
            if (nm == names::Relation || nm == names::Operator) {
                child(a[1], p);
                if (m_opt.spaces_around_operators) m_sink.write(" ", TextStyle::Plain);
                symbol(a[0]);
                if (m_opt.spaces_around_operators) m_sink.write(" ", TextStyle::Plain);
                child(a[2], p + 1);
                return;
            }
            child(a[0], p);
            op(relation_text(nm));
            child(a[1], p + 1);
            return;
        }
        if (!n.has(node_flags::Command) && nm == names::Parens) {
            arguments(a, false);
            return;
        }
        if (n.has(node_flags::Command)) {
            const std::uint32_t index = find_symbol(nm);
            if (m_opt.unicode_symbols && index != kNoSymbol && chars::is_letter(nm[0])) {
                m_sink.write(symbol_info(index).utf8, TextStyle::Name);
                arguments(a);
                return;
            }
            m_sink.write(m_doc.command_text(id), TextStyle::Name);
            arguments(a);
            return;
        }
        m_sink.write(nm, TextStyle::Name);
        arguments(a);
    }

    void node(NodeId id) {
        switch (m_doc.kind(id)) {
            case NodeKind::Number: {
                const std::string text = format_number(m_doc, id, m_opt.numbers, m_opt.times_ten);
                if (m_opt.unicode_symbols && !text.empty() && text[0] == '-') {
                    m_sink.write("\xE2\x88\x92", TextStyle::Number);
                    m_sink.write(std::string_view(text).substr(1), TextStyle::Number);
                } else {
                    m_sink.write(text, TextStyle::Number);
                }
                break;
            }
            case NodeKind::String: string(id); break;
            case NodeKind::Symbol: symbol(id); break;
            case NodeKind::Error: m_sink.write(m_doc.source_text(id), TextStyle::Error); break;
            case NodeKind::Negate:
                tight(m_opt.unicode_symbols ? "\xE2\x88\x92" : "-");
                child(m_doc.children(id)[0], precedence::Unary);
                break;
            case NodeKind::Call: call(id); break;
            case NodeKind::TextRun: m_sink.write(m_doc.decoded_text(id), TextStyle::String); break;
            case NodeKind::Document: print(); break;
        }
    }

    const Document&  m_doc;
    Sink&            m_sink;
    MathPrintOptions m_opt;
};

inline void print_math(const Document& doc, Sink& sink, const MathPrintOptions& options = MathPrintOptions()) { MathPrinter(doc, sink, options).print(); }

inline std::string to_math(const Document& doc, const MathPrintOptions& options = MathPrintOptions()) {
    StringSink s;
    print_math(doc, s, options);
    return s.take();
}

inline std::string to_math_ansi(const Document& doc, const MathPrintOptions& options = MathPrintOptions(), bool highlight_syntax = false) {
    AnsiSink s(highlight_syntax);
    print_math(doc, s, options);
    return s.take();
}

inline std::string to_math_html(const Document& doc, const MathPrintOptions& options = MathPrintOptions(), const HtmlSinkOptions& html = HtmlSinkOptions()) {
    HtmlSink s(html);
    print_math(doc, s, options);
    return s.take();
}

} // namespace mathtext
} // namespace fizmo

#endif // FIZMO_MATHTEXT_MATH_PRINTER_HPP
