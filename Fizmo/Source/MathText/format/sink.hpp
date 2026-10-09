#ifndef FIZMO_MATHTEXT_SINK_HPP
#define FIZMO_MATHTEXT_SINK_HPP

#include <cstdint>
#include <string>
#include <string_view>

namespace fizmo {
namespace mathtext {

enum class TextStyle : std::uint8_t { Plain, Name, Punctuation, Number, String, Symbol, Error };

inline constexpr const char* text_style_name(TextStyle s) noexcept {
    switch (s) {
        case TextStyle::Plain: return "plain";
        case TextStyle::Name: return "name";
        case TextStyle::Punctuation: return "punct";
        case TextStyle::Number: return "number";
        case TextStyle::String: return "string";
        case TextStyle::Symbol: return "symbol";
        case TextStyle::Error: return "error";
    }
    return "plain";
}

class Sink {
public:
    virtual ~Sink() = default;
    virtual void write(std::string_view text, TextStyle style) = 0;
};

class StringSink : public Sink {
public:
    void write(std::string_view text, TextStyle) override { m_out.append(text.data(), text.size()); }
    const std::string& str() const noexcept { return m_out; }
    std::string take() noexcept { return std::move(m_out); }

private:
    std::string m_out;
};

class AnsiSink : public Sink {
public:
    explicit AnsiSink(bool highlight_syntax = false) noexcept : m_syntax(highlight_syntax) {}

    void write(std::string_view text, TextStyle style) override {
        const char* code = color(style);
        if (!code || text.empty()) {
            m_out.append(text.data(), text.size());
            return;
        }
        m_out += code;
        m_out.append(text.data(), text.size());
        m_out += "\x1b[0m";
    }

    const std::string& str() const noexcept { return m_out; }
    std::string take() noexcept { return std::move(m_out); }

private:
    const char* color(TextStyle s) const noexcept {
        if (s == TextStyle::Error) return "\x1b[31m";
        if (!m_syntax) return nullptr;
        switch (s) {
            case TextStyle::Name: return "\x1b[36m";
            case TextStyle::Number: return "\x1b[33m";
            case TextStyle::String: return "\x1b[32m";
            case TextStyle::Symbol: return "\x1b[35m";
            default: return nullptr;
        }
    }

    std::string m_out;
    bool        m_syntax;
};

struct HtmlSinkOptions {
    std::string_view class_prefix = "mt-";
    bool             classes_for_all = false;
    bool             inline_error_color = true;
    std::string_view error_color = "#d32f2f";
};

class HtmlSink : public Sink {
public:
    explicit HtmlSink(const HtmlSinkOptions& options = HtmlSinkOptions()) : m_opt(options) {}

    void write(std::string_view text, TextStyle style) override {
        if (text.empty()) return;
        const bool tagged = style == TextStyle::Error || (m_opt.classes_for_all && style != TextStyle::Plain);
        if (tagged) {
            m_out += "<span class=\"";
            m_out.append(m_opt.class_prefix.data(), m_opt.class_prefix.size());
            m_out += text_style_name(style);
            m_out += "\"";
            if (style == TextStyle::Error && m_opt.inline_error_color) {
                m_out += " style=\"color:";
                m_out.append(m_opt.error_color.data(), m_opt.error_color.size());
                m_out += "\"";
            }
            m_out += ">";
        }
        escape(text);
        if (tagged) m_out += "</span>";
    }

    const std::string& str() const noexcept { return m_out; }
    std::string take() noexcept { return std::move(m_out); }

private:
    void escape(std::string_view text) {
        for (char c : text) {
            switch (c) {
                case '&': m_out += "&amp;"; break;
                case '<': m_out += "&lt;"; break;
                case '>': m_out += "&gt;"; break;
                case '"': m_out += "&quot;"; break;
                case '\'': m_out += "&#39;"; break;
                default: m_out.push_back(c); break;
            }
        }
    }

    HtmlSinkOptions m_opt;
    std::string     m_out;
};

} // namespace mathtext
} // namespace fizmo

#endif // FIZMO_MATHTEXT_SINK_HPP
