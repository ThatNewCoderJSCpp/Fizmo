#ifndef FIZMO_MATHTEXT_PRINTER_HPP
#define FIZMO_MATHTEXT_PRINTER_HPP

#include <cstddef>
#include <string>
#include <string_view>
#include "number_format.hpp"
#include "sink.hpp"

namespace fizmo {
namespace mathtext {

enum class Layout : std::uint8_t { Auto, Compact, Expanded };

struct PrintOptions {
    Layout           layout = Layout::Auto;
    std::size_t      indent = 4;
    std::size_t      max_width = 80;
    NumberStyle      numbers = NumberStyle::AsWritten;
    std::string_view times_ten = "*10^";
    bool             unicode_symbols = false;
};

class Printer {
public:
    Printer(const Document& doc, Sink& sink, const PrintOptions& options = PrintOptions()) noexcept : m_doc(doc), m_sink(sink), m_opt(options) {}

    void print() {
        bool first = true;
        for (NodeId id : m_doc.top_level()) {
            if (!first) m_sink.write("\n", TextStyle::Plain);
            first = false;
            node(id, 0, m_opt.layout == Layout::Compact);
        }
    }

    void print(NodeId id) {
        if (m_doc.kind(id) == NodeKind::Document) print();
        else node(id, 0, m_opt.layout == Layout::Compact);
    }

private:
    void spaces(std::size_t level) {
        static constexpr char pad[] = "                                                                ";
        std::size_t n = level * m_opt.indent;
        while (n > 0) {
            const std::size_t k = n < sizeof(pad) - 1 ? n : sizeof(pad) - 1;
            m_sink.write(std::string_view(pad, k), TextStyle::Plain);
            n -= k;
        }
    }

    void symbol(NodeId id, TextStyle known_style) {
        const SymbolInfo* info = m_doc.symbol(id);
        if (info && m_opt.unicode_symbols) {
            m_sink.write(info->utf8, known_style);
            return;
        }
        m_sink.write(m_doc.command_text(id), info ? known_style : TextStyle::Error);
    }

    void escaped(std::string_view text, char quote) {
        std::size_t run = 0;
        for (std::size_t i = 0; i < text.size(); ++i) {
            const char c = text[i];
            if (c != '\\' && c != quote) continue;
            if (i > run) m_sink.write(text.substr(run, i - run), TextStyle::String);
            const char esc[2] = { '\\', c };
            m_sink.write(std::string_view(esc, 2), TextStyle::String);
            run = i + 1;
        }
        if (run < text.size()) m_sink.write(text.substr(run), TextStyle::String);
    }

    void string(NodeId id) {
        const char quote = m_doc.node(id).has(node_flags::SingleQuoted) ? '\'' : '"';
        const char q[1] = { quote };
        m_sink.write(std::string_view(q, 1), TextStyle::String);
        for (NodeId c : m_doc.children(id)) {
            if (m_doc.kind(c) == NodeKind::Symbol) symbol(c, TextStyle::Symbol);
            else escaped(m_doc.decoded_text(c), quote);
        }
        m_sink.write(std::string_view(q, 1), TextStyle::String);
    }

    bool fits_compact(NodeId id, std::size_t level) const {
        for (NodeId c : m_doc.children(id)) {
            if (m_doc.kind(c) == NodeKind::Call) return false;
            if (m_doc.kind(c) == NodeKind::Error && m_doc.source_text(c).find('\n') != std::string_view::npos) return false;
        }
        StringSink probe;
        PrintOptions o = m_opt;
        o.layout = Layout::Compact;
        Printer(m_doc, probe, o).node(id, 0, true);
        return level * m_opt.indent + utf8::count_codepoints(probe.str()) <= m_opt.max_width;
    }

    void call(NodeId id, std::size_t level, bool compact) {
        const Node& n = m_doc.node(id);
        if (n.has(node_flags::Command)) m_sink.write(m_doc.command_text(id), TextStyle::Name);
        else m_sink.write(m_doc.name(id), TextStyle::Name);
        const NodeRange args = m_doc.children(id);
        if (!compact && m_opt.layout == Layout::Auto) compact = fits_compact(id, level);
        if (compact || args.empty()) {
            m_sink.write(args.empty() && !compact ? " {" : "{", TextStyle::Punctuation);
            for (std::size_t i = 0; i < args.size(); ++i) {
                if (i) m_sink.write(", ", TextStyle::Punctuation);
                node(args[i], level, true);
            }
            m_sink.write("}", TextStyle::Punctuation);
            return;
        }
        m_sink.write(" {", TextStyle::Punctuation);
        m_sink.write("\n", TextStyle::Plain);
        for (std::size_t i = 0; i < args.size(); ++i) {
            spaces(level + 1);
            node(args[i], level + 1, false);
            if (i + 1 < args.size()) m_sink.write(",", TextStyle::Punctuation);
            m_sink.write("\n", TextStyle::Plain);
        }
        spaces(level);
        m_sink.write("}", TextStyle::Punctuation);
    }

    void node(NodeId id, std::size_t level, bool compact) {
        switch (m_doc.kind(id)) {
            case NodeKind::Number: m_sink.write(format_number(m_doc, id, m_opt.numbers, m_opt.times_ten), TextStyle::Number); break;
            case NodeKind::String: string(id); break;
            case NodeKind::Symbol: symbol(id, TextStyle::Symbol); break;
            case NodeKind::Error: m_sink.write(m_doc.source_text(id), TextStyle::Error); break;
            case NodeKind::Call: call(id, level, compact); break;
            case NodeKind::Negate:
                m_sink.write(m_opt.unicode_symbols ? "\xE2\x88\x92" : "-", TextStyle::Punctuation);
                node(m_doc.children(id)[0], level, compact);
                break;
            case NodeKind::TextRun: m_sink.write(m_doc.decoded_text(id), TextStyle::String); break;
            case NodeKind::Document: print(); break;
        }
    }

    const Document& m_doc;
    Sink&           m_sink;
    PrintOptions    m_opt;
};

inline void print(const Document& doc, Sink& sink, const PrintOptions& options = PrintOptions()) { Printer(doc, sink, options).print(); }

inline std::string to_string(const Document& doc, const PrintOptions& options = PrintOptions()) {
    StringSink s;
    print(doc, s, options);
    return s.take();
}

inline std::string to_ansi(const Document& doc, const PrintOptions& options = PrintOptions(), bool highlight_syntax = false) {
    AnsiSink s(highlight_syntax);
    print(doc, s, options);
    return s.take();
}

inline std::string to_html(const Document& doc, const PrintOptions& options = PrintOptions(), const HtmlSinkOptions& html = HtmlSinkOptions()) {
    HtmlSink s(html);
    print(doc, s, options);
    return s.take();
}

} // namespace mathtext
} // namespace fizmo

#endif // FIZMO_MATHTEXT_PRINTER_HPP
