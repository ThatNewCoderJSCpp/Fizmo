#ifndef FIZMO_RICH_TEXT_HPP
#define FIZMO_RICH_TEXT_HPP

#include "text_style.hpp"
#include <cstddef>
#include <string>
#include <utility>
#include <vector>

namespace fizmo {
namespace text {

struct TextSpan {
    std::string text;
    TextStyle   style;
    std::string family;

    bool operator==(const TextSpan& o) const noexcept { return text == o.text && style == o.style && family == o.family; }
    bool operator!=(const TextSpan& o) const noexcept { return !(*this == o); }
};

class RichText {
private:
    TextStyle             m_base;
    std::vector<TextSpan> m_spans;

public:
    RichText() = default;
    explicit RichText(const TextStyle& base) : m_base(base) {}

    RichText(std::string text, const TextStyle& style);

    RichText& add(std::string text, const TextStyle& style = TextStyle{}) {
        if (!text.empty()) m_spans.push_back({ std::move(text), style, std::string() });
        return *this;
    }

    RichText& add(std::string text, const TextStyle& style, std::string family);

    RichText& add(TextSpan span) {
        if (!span.text.empty()) m_spans.push_back(std::move(span));
        return *this;
    }

    RichText& newline();

    const TextStyle&             base()  const noexcept { return m_base; }
    TextStyle&                   base()        noexcept { return m_base; }
    void                         set_base(const TextStyle& s) { m_base = s; }
    const std::vector<TextSpan>& spans() const noexcept { return m_spans; }
    std::vector<TextSpan>&       spans()       noexcept { return m_spans; }
    std::size_t                  size()  const noexcept { return m_spans.size(); }
    bool                         empty() const noexcept { return m_spans.empty(); }
    void                         clear()       noexcept { m_spans.clear(); }

    TextStyle resolved(std::size_t i) const noexcept { return m_base.merge_over(m_spans[i].style); }

    std::string plain_text() const {
        std::string s;
        for (const auto& sp : m_spans) s += sp.text;
        return s;
    }

    bool operator==(const RichText& o) const noexcept { return m_base == o.m_base && m_spans == o.m_spans; }
    bool operator!=(const RichText& o) const noexcept { return !(*this == o); }
};

} // namespace text
} // namespace fizmo

#endif // FIZMO_RICH_TEXT_HPP