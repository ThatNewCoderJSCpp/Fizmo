#define ALL_FIZMO
#include <fizmo/includes.hpp>
#include "rich_text.hpp"

namespace fizmo {
namespace text {

RichText::RichText(std::string text, const TextStyle& style) : m_base(style) {
        if (!text.empty()) m_spans.push_back({ std::move(text), TextStyle{}, std::string() });
    }

auto RichText::add(std::string text, const TextStyle& style, std::string family) -> RichText& {
        if (!text.empty()) m_spans.push_back({ std::move(text), style, std::move(family) });
        return *this;
    }

auto RichText::newline() -> RichText& {
        if (m_spans.empty()) m_spans.push_back({ "\n", TextStyle{}, std::string() });
        else m_spans.back().text += '\n';
        return *this;
    }

} // namespace text
} // namespace fizmo
