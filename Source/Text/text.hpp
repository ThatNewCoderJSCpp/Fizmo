#ifndef FIZMO_TEXT_TEXT_HPP
#define FIZMO_TEXT_TEXT_HPP

#include "emoji.hpp"
#include "fancy_text.hpp"
#include "fancy_text_fwd.hpp"
#include "rich_text.hpp"
#include "text_style.hpp"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace fizmo {

class String;

namespace images {
class BitmapImage;
} // namespace images

namespace text {

class TextRasterizer;

enum class TextSegmentKind : std::uint8_t { Text, Math, Image, LineBreak, QuoteBar };

struct TextSegment {
    TextSegmentKind kind = TextSegmentKind::Text;
    std::string     content;
    TextStyle       style;
    std::string     family;
    EmojiImage      image;
    float           image_width = 0.0f;
    float           image_height = 0.0f;
    std::int32_t    link = -1;
    bool            display_math = false;

    bool operator==(const TextSegment& o) const noexcept;
    bool operator!=(const TextSegment& o) const noexcept { return !(*this == o); }
};

struct TextLink {
    std::string target;
    std::string label;
};

struct TextBox {
    float width = 0.0f;
    float height = 0.0f;
    float ascent = 0.0f;
    float descent = 0.0f;
    std::size_t lines = 0;
};

struct MarkupOptions {
    bool bold = true;
    bool italic = true;
    bool underline = true;
    bool strikethrough = true;
    bool code = true;
    bool code_blocks = true;
    bool spoilers = true;
    bool links = true;
    bool math = true;
    bool emoji = true;
    bool headings = true;
    bool quotes = true;
    bool lists = true;
    bool color_tags = true;
    bool jumbo_emoji = true;
    graphics::Color code_background = graphics::Color(47, 49, 54, 255);
    graphics::Color code_color = graphics::Color(220, 221, 222, 255);
    graphics::Color spoiler_color = graphics::Color(32, 34, 37, 255);
    graphics::Color link_color = graphics::Color(0, 168, 252, 255);
    graphics::Color quote_bar_color = graphics::Color(79, 84, 92, 255);
    std::string     code_family = "monospace";
};

class Text {
public:
    Text() = default;
    Text(const char* utf8);
    Text(const char* utf8, const TextStyle& style);
    Text(std::string utf8);
    Text(std::string utf8, const TextStyle& style);
    Text(std::string_view utf8);
    Text(const std::wstring& wide);
    Text(const String& str);
    Text(const String& str, const TextStyle& style);
    Text(const RichText& rich);

    static Text plain(std::string utf8, const TextStyle& style = TextStyle());
    static Text markup(std::string_view source, const TextStyle& base = TextStyle(), const MarkupOptions& options = MarkupOptions());
    static Text math(std::string_view source, const TextStyle& style = TextStyle(), bool display = false);
    static Text emoji(std::string_view name, const TextStyle& style = TextStyle());
    static Text image(EmojiImage image, float width = 0.0f, float height = 0.0f);

    Text& add(std::string utf8, const TextStyle& style = TextStyle(), std::string family = std::string());
    Text& add(const Text& other);
    Text& add_markup(std::string_view source, const TextStyle& style = TextStyle(), const MarkupOptions& options = MarkupOptions());
    Text& add_math(std::string source, const TextStyle& style = TextStyle(), bool display = false);
    Text& add_emoji(std::string_view name, const TextStyle& style = TextStyle());
    Text& add_image(EmojiImage image, float width = 0.0f, float height = 0.0f, std::string alt = std::string());
    Text& add_link(std::string label, std::string target, const TextStyle& style = TextStyle());
    std::int32_t register_link(std::string target, std::string label = std::string());
    Text& newline();

    Text& operator+=(const Text& other) { return add(other); }
    Text& operator+=(std::string utf8) { return add(std::move(utf8)); }
    Text& operator+=(const char* utf8) { return add(std::string(utf8 ? utf8 : "")); }
    friend Text operator+(Text a, const Text& b) { return a.add(b); }

    Text& style(const TextStyle& s) { m_base = s; return *this; }
    Text& color(const graphics::Color& c) { m_base.set_fg_color(c); return *this; }
    Text& background(const graphics::Color& c) { m_base.set_bg_color(c); return *this; }
    Text& size(double px) { m_base.set_size(px); return *this; }
    Text& font(Font f) { m_base.set_font(f); return *this; }
    Text& family(std::string name) { m_family = std::move(name); return *this; }
    Text& bold(bool b = true) { m_base.set_bold(b); return *this; }
    Text& italic(bool i = true) { m_base.set_italic(i); return *this; }
    Text& underline(bool u = true) { m_base.set_underline(u); return *this; }
    Text& strikethrough(bool s = true) { m_base.set_strikethrough(s); return *this; }
    Text& opacity(float o) { m_base.set_opacity(o); return *this; }
    Text& shadow(double dx, double dy, double blur, const graphics::Color& c = graphics::Color(0, 0, 0, 128)) { m_base.set_shadow(dx, dy, blur, c); return *this; }
    Text& outline(const graphics::Color& c, double width) { m_base.set_outline_color(c); m_base.set_outline_width(width); return *this; }

    Text& at(float x, float y) noexcept { m_x = x; m_y = y; m_positioned = true; return *this; }
    Text& position(float x, float y) noexcept { return at(x, y); }
    Text& anchor(FancyAnchor a) noexcept { m_anchor = a; return *this; }
    Text& max_width(float w) noexcept { m_max_width = w < 0.0f ? 0.0f : w; return *this; }
    Text& align(TextAlign a) noexcept { m_align = a; return *this; }
    Text& line_spacing(float multiplier) noexcept { m_line_spacing = multiplier > 0.0f ? multiplier : 1.0f; return *this; }
    Text& emoji_scale(float em) noexcept { m_emoji_em = em > 0.0f ? em : 1.375f; return *this; }
    Text& jumbo_emoji(bool on = true) noexcept { m_jumbo = on; return *this; }

    const TextStyle&                 base() const noexcept { return m_base; }
    TextStyle&                       base() noexcept { return m_base; }
    const std::string&               base_family() const noexcept { return m_family; }
    const std::vector<TextSegment>&  segments() const noexcept { return m_segments; }
    std::vector<TextSegment>&        segments() noexcept { return m_segments; }
    const std::vector<TextLink>&     links() const noexcept { return m_links; }
    float                            x() const noexcept { return m_x; }
    float                            y() const noexcept { return m_y; }
    bool                             positioned() const noexcept { return m_positioned; }
    FancyAnchor                      anchor() const noexcept { return m_anchor; }
    float                            max_width() const noexcept { return m_max_width; }
    TextAlign                        align() const noexcept { return m_align; }
    float                            line_spacing() const noexcept { return m_line_spacing; }

    bool        empty() const noexcept;
    bool        has_math() const noexcept;
    bool        has_images() const noexcept;
    bool        animated() const noexcept;
    bool        is_simple() const noexcept;
    std::string plain_text() const;
    RichText    to_rich_text() const;
    std::string fingerprint() const;

    FancyText layout(TextRasterizer& raster) const;
    FancyText layout() const;
    TextBox   measure(TextRasterizer& raster) const;
    TextBox   measure() const;

    images::BitmapImage render(float padding = 2.0f, const graphics::Color& background = graphics::Color(255, 255, 255, 0)) const;
    images::BitmapImage render(TextRasterizer& raster, float padding, const graphics::Color& background) const;

    std::uint32_t link_at(const FancyText& layout, float x, float y) const noexcept;

    bool operator==(const Text& o) const noexcept;
    bool operator!=(const Text& o) const noexcept { return !(*this == o); }

private:
    void push_text(std::string utf8, const TextStyle& style, std::string family, std::int32_t link);

    TextStyle                m_base;
    std::string              m_family;
    std::vector<TextSegment> m_segments;
    std::vector<TextLink>    m_links;
    float                    m_x = 0.0f;
    float                    m_y = 0.0f;
    bool                     m_positioned = false;
    FancyAnchor              m_anchor = FancyAnchor::TopLeft;
    float                    m_max_width = 0.0f;
    TextAlign                m_align = TextAlign::Left;
    float                    m_line_spacing = 1.0f;
    float                    m_emoji_em = 1.375f;
    bool                     m_jumbo = false;
};

namespace literals {

inline Text operator""_text(const char* s, std::size_t n) { return Text(std::string(s, n)); }

inline Text operator""_markup(const char* s, std::size_t n) { return Text::markup(std::string_view(s, n)); }

inline Text operator""_math(const char* s, std::size_t n) { return Text::math(std::string_view(s, n)); }

} // namespace literals

void draw_animated_frame(images::BitmapImage& img, const FancyImage& item, float x, float y, double time_ms);

} // namespace text
} // namespace fizmo

#endif // FIZMO_TEXT_TEXT_HPP
