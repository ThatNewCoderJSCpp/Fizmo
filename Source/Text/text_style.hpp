#ifndef FIZMO_TEXT_STYLE_HPP
#define FIZMO_TEXT_STYLE_HPP

#include "util.hpp"
#include "../Graphics/optional_color.hpp"

namespace fizmo {
namespace text {

class TextStyle {
public:
    constexpr TextStyle() noexcept = default;
    constexpr TextStyle(double size_px, const graphics::Color& color) noexcept : m_size(size_px), m_fg_color(color) {}

public:
    Font font()                const noexcept { return m_font;                }
    FontCategory font_family() const noexcept { return font_category(m_font); }
    bool has_font()            const noexcept { return m_font_set;            }

    void clear_font() noexcept { 
        m_font_set = false; 
        m_font =  Font::None; 
    }
    
    void set_font(Font f)            noexcept { m_font = f;          m_font_set = true; }
    void set_font(SerifFont f)       noexcept { m_font = to_font(f); m_font_set = true; }
    void set_font(SansSerifFont f)   noexcept { m_font = to_font(f); m_font_set = true; }
    void set_font(MonospaceFont f)   noexcept { m_font = to_font(f); m_font_set = true; }
    void set_font(HandwritingFont f) noexcept { m_font = to_font(f); m_font_set = true; }
    void set_font(DisplayFont f)     noexcept { m_font = to_font(f); m_font_set = true; }

    bool is_serif() const noexcept {
        return m_font_set && fizmo::text::is_serif(m_font);
    }
    bool is_sans_serif() const noexcept {
        return m_font_set && fizmo::text::is_sans_serif(m_font);
    }
    bool is_monospace() const noexcept {
        return m_font_set && fizmo::text::is_monospace(m_font);
    }
    bool is_display() const noexcept {
        return m_font_set && fizmo::text::is_display(m_font);
    }
    bool is_handwriting() const noexcept {
        return m_font_set && fizmo::text::is_handwriting(m_font);
    }

    SerifFont serif_font() const noexcept {
        return is_serif() ? to_serif(m_font) : SerifFont::InvalidSerif;
    }
    SansSerifFont sans_serif_font() const noexcept {
        return is_sans_serif() ? to_sans_serif(m_font) : SansSerifFont::InvalidSansSerif;
    }
    MonospaceFont monospace_font() const noexcept {
        return is_monospace() ? to_monospace(m_font) : MonospaceFont::InvalidMonospace;
    }
    DisplayFont display_font() const noexcept {
        return is_display() ? to_display(m_font) : DisplayFont::InvalidDisplay;
    }
    HandwritingFont handwriting_font() const noexcept {
        return is_handwriting() ? to_handwriting(m_font) : HandwritingFont::InvalidCursive;
    }

public:
    double size()                    const noexcept { return m_size; }
    void   set_size(double px)             noexcept { m_size = px; }
    bool   has_size()                const noexcept { return detail::is_set(m_size, detail::kUnsetDouble); }
    void   clear_size()                    noexcept { m_size = detail::kUnsetDouble; }

public:
    FontWeight weight()                    const noexcept { return m_weight; }
    void       set_weight(FontWeight w)          noexcept { m_weight = w; m_weight_set = true; }
    bool       has_weight()                const noexcept { return m_weight_set; }
    void       clear_weight()                    noexcept { m_weight_set = false; }

    void set_bold(bool b = true) noexcept {
        set_weight(b ? FontWeight::Bold : FontWeight::Normal);
    }
    bool is_bold() const noexcept {
        return m_weight_set && fizmo::text::is_bold(m_weight);
    }

public:
    FontSlant slant()                    const noexcept { return m_slant; }
    void      set_slant(FontSlant s)           noexcept { m_slant = s; m_slant_set = true; }
    bool      has_slant()                const noexcept { return m_slant_set; }
    void      clear_slant()                    noexcept { m_slant_set = false; }

    void set_italic(bool i = true) noexcept {
        set_slant(i ? FontSlant::Italic : FontSlant::Normal);
    }
    bool is_italic() const noexcept {
        return m_slant_set && m_slant != FontSlant::Normal;
    }

public:
    const graphics::OptionalColor& fg_color()          const noexcept { return m_fg_color; }
    void set_fg_color(const graphics::Color& c)              noexcept { m_fg_color.set(c); }
    bool has_fg_color()                                const noexcept { return m_fg_color.is_set(); }
    void clear_fg_color()                                    noexcept { m_fg_color.clear(); }

    const graphics::OptionalColor& bg_color()          const noexcept { return m_bg_color; }
    void set_bg_color(const graphics::Color& c)              noexcept { m_bg_color.set(c); }
    bool has_bg_color()                                const noexcept { return m_bg_color.is_set(); }
    void clear_bg_color()                                    noexcept { m_bg_color.clear(); }

public:
    TextDecoration decoration()                    const noexcept { return m_decoration; }
    void set_decoration(TextDecoration d)                noexcept { m_decoration = d; m_decoration_set = true; }
    void add_decoration(TextDecoration d)                noexcept { m_decoration |= d; m_decoration_set = true; }
    void remove_decoration(TextDecoration d)             noexcept { m_decoration &= ~d; m_decoration_set = true; }
    bool has_decoration_flags()                    const noexcept { return m_decoration_set; }
    void clear_decoration()                              noexcept { m_decoration_set = false; m_decoration = TextDecoration::None; }

public:
    void set_underline(bool u = true) noexcept;
    bool is_underlined() const noexcept {
        return has_decoration(m_decoration, TextDecoration::Underline);
    }

public:
    void set_strikethrough(bool s = true) noexcept;
    bool is_strikethrough() const noexcept {
        return has_decoration(m_decoration, TextDecoration::Strikethrough);
    }

public:
    void set_overline(bool o = true) noexcept;
    bool is_overlined() const noexcept {
        return has_decoration(m_decoration, TextDecoration::Overline);
    }

public:
    DecorationStyle decoration_style()                    const noexcept { return m_deco_style; }
    void set_decoration_style(DecorationStyle s)                noexcept { m_deco_style = s; m_deco_style_set = true; }
    bool has_decoration_style()                           const noexcept { return m_deco_style_set; }
    void clear_decoration_style()                               noexcept { m_deco_style_set = false; }

public:
    const graphics::OptionalColor& decoration_color()    const noexcept { return m_deco_color; }
    void set_decoration_color(const graphics::Color& c)        noexcept { m_deco_color.set(c); }
    bool has_decoration_color()                          const noexcept { return m_deco_color.is_set(); }
    void clear_decoration_color()                              noexcept { m_deco_color.clear(); }

public:
    double letter_spacing()                    const noexcept { return m_letter_spacing; }
    void   set_letter_spacing(double px)             noexcept { m_letter_spacing = px; }
    bool   has_letter_spacing()                const noexcept { return detail::is_set(m_letter_spacing, detail::kUnsetSpacing); }
    void   clear_letter_spacing()                    noexcept { m_letter_spacing = detail::kUnsetSpacing; }

public:
    double word_spacing()                    const noexcept { return m_word_spacing; }
    void   set_word_spacing(double px)             noexcept { m_word_spacing = px; }
    bool   has_word_spacing()                const noexcept { return detail::is_set(m_word_spacing, detail::kUnsetSpacing); }
    void   clear_word_spacing()                    noexcept { m_word_spacing = detail::kUnsetSpacing; }

public:
    double line_height()                    const noexcept { return m_line_height; }
    void   set_line_height(double mult)           noexcept { m_line_height = mult; }
    bool   has_line_height()                const noexcept { return detail::is_set(m_line_height, detail::kUnsetDouble); }
    void   clear_line_height()                    noexcept { m_line_height = detail::kUnsetDouble; }

public:
    VerticalAlign vertical_align()                    const noexcept { return m_vert_align; }
    void set_vertical_align(VerticalAlign v)                noexcept { m_vert_align = v; m_vert_align_set = true; }
    bool has_vertical_align()                         const noexcept { return m_vert_align_set; }
    void clear_vertical_align()                             noexcept { m_vert_align_set = false; }

public:
    void set_superscript(bool s = true) noexcept {
        set_vertical_align(s ? VerticalAlign::Superscript : VerticalAlign::Baseline);
    }
    void set_subscript(bool s = true) noexcept {
        set_vertical_align(s ? VerticalAlign::Subscript : VerticalAlign::Baseline);
    }

public:
    TextTransform transform()                    const noexcept { return m_transform; }
    void set_transform(TextTransform t)                noexcept { m_transform = t; m_transform_set = true; }
    bool has_transform()                         const noexcept { return m_transform_set; }
    void clear_transform()                             noexcept { m_transform_set = false; }

public:
    float opacity()                    const noexcept { return m_opacity; }
    void  set_opacity(float o)               noexcept { m_opacity = o; m_opacity_set = true; }
    bool  has_opacity()                const noexcept { return m_opacity_set; }
    void  clear_opacity()                    noexcept { m_opacity_set = false; }

public:
    const graphics::OptionalColor& outline_color()     const noexcept { return m_outline_color; }
    void set_outline_color(const graphics::Color& c)         noexcept { m_outline_color.set(c); }
    bool has_outline_color()                           const noexcept { return m_outline_color.is_set(); }
    void clear_outline_color()                               noexcept { m_outline_color.clear(); }

    double outline_width()                   const noexcept { return m_outline_width; }
    void   set_outline_width(double px)            noexcept { m_outline_width = px; }
    bool   has_outline_width()               const noexcept { return detail::is_set(m_outline_width, detail::kUnsetDouble); }
    void   clear_outline_width()                   noexcept { m_outline_width = detail::kUnsetDouble; }

public:
    struct TextShadow {
        double offset_x = 0.0;
        double offset_y = 0.0;
        double blur     = 0.0;
        graphics::Color color{0, 0, 0, 128};

        constexpr bool operator==(const TextShadow& o) const noexcept {
            return offset_x == o.offset_x && offset_y == o.offset_y
                && blur == o.blur && color == o.color;
        }
        constexpr bool operator!=(const TextShadow& o) const noexcept { return !(*this == o); }
    };

    const TextShadow& shadow()                         const noexcept { return m_shadow; }
    void set_shadow(const TextShadow& s)                     noexcept { m_shadow = s; m_shadow_set = true; }

    void set_shadow(double ox, double oy, double blur, const graphics::Color& c = graphics::Color(0,0,0,128)) noexcept {
        m_shadow = {ox, oy, blur, c};
        m_shadow_set = true;
    }

    bool has_shadow()                                  const noexcept { return m_shadow_set; }
    void clear_shadow()                                      noexcept { m_shadow_set = false; }

public:
    WritingDirection direction()                       const noexcept { return m_direction; }
    void set_direction(WritingDirection d)                   noexcept { m_direction = d; m_direction_set = true; }
    bool has_direction()                               const noexcept { return m_direction_set; }
    void clear_direction()                                   noexcept { m_direction_set = false; }

public:
    TextAlign   text_align()                  const noexcept { return m_align; }
    void        set_text_align(TextAlign a)         noexcept { m_align = a; m_align_set = true; }
    bool        has_text_align()              const noexcept { return m_align_set; }
    void        clear_text_align()                  noexcept { m_align_set = false; }

public:
    TextOverflow text_overflow()              const noexcept { return m_overflow; }
    void set_text_overflow(TextOverflow o)          noexcept { m_overflow = o; m_overflow_set = true; }
    bool has_text_overflow()                  const noexcept { return m_overflow_set; }
    void clear_text_overflow()                      noexcept { m_overflow_set = false; }

public:
    double indent()                    const noexcept { return m_indent; }
    void   set_indent(double px)             noexcept { m_indent = px; }
    bool   has_indent()                const noexcept { return detail::is_set(m_indent, detail::kUnsetSpacing); }
    void   clear_indent()                    noexcept { m_indent = detail::kUnsetSpacing; }

public:
    double paragraph_spacing()               const noexcept { return m_para_spacing; }
    void   set_paragraph_spacing(double px)        noexcept { m_para_spacing = px; }
    bool   has_paragraph_spacing()           const noexcept { return detail::is_set(m_para_spacing, detail::kUnsetSpacing); }
    void   clear_paragraph_spacing()               noexcept { m_para_spacing = detail::kUnsetSpacing; }

public:
    unsigned int max_lines()                 const noexcept { return m_max_lines; }
    void set_max_lines(unsigned int n)             noexcept { m_max_lines = n; }
    bool has_max_lines()                     const noexcept { return m_max_lines != 0; }
    void clear_max_lines()                         noexcept { m_max_lines = 0; }

public:
    TextStyle merge_over(const TextStyle& over) const noexcept;

    void clear_all() noexcept { *this = TextStyle{}; }

    bool is_empty() const noexcept;

    bool operator==(const TextStyle& o) const noexcept;

    bool operator!=(const TextStyle& o) const noexcept { return !(*this == o); }

public:
    friend std::ostream& operator<<(std::ostream& os, const TextStyle& s) {
        os << "TextStyle{";
        bool first = true;
        auto sep = [&]() { if (!first) os << ", "; first = false; };

        if (s.m_font_set)              { sep(); os << "font=\"" << s.m_font << "\""; }
        if (s.has_size())              { sep(); os << "size=" << s.m_size << "px"; }
        if (s.m_weight_set)            { sep(); os << "weight=" << s.m_weight; }
        if (s.m_slant_set)             { sep(); os << "slant=" << s.m_slant; }
        if (s.m_fg_color.is_set())     { sep(); os << "fg=RGBA(...)"; }
        if (s.m_bg_color.is_set())     { sep(); os << "bg=RGBA(...)"; }
        if (s.is_underlined())         { sep(); os << "underline"; }
        if (s.is_strikethrough())      { sep(); os << "strikethrough"; }
        if (s.is_overlined())          { sep(); os << "overline"; }
        if (s.has_letter_spacing())    { sep(); os << "tracking=" << s.m_letter_spacing << "px"; }
        if (s.has_line_height())       { sep(); os << "leading=" << s.m_line_height << "x"; }
        if (s.m_vert_align_set && s.m_vert_align == VerticalAlign::Superscript) { sep(); os << "super"; }
        if (s.m_vert_align_set && s.m_vert_align == VerticalAlign::Subscript)   { sep(); os << "sub"; }
        if (s.m_transform_set && s.m_transform != TextTransform::None)          { sep(); os << "transform"; }
        if (s.m_opacity_set)           { sep(); os << "opacity=" << s.m_opacity; }
        if (s.m_shadow_set)            { sep(); os << "shadow"; }
        if (s.m_align_set)             { sep(); os << "align=" << s.m_align; }

        os << "}";
        return os;
    }

private:
    Font           m_font           = Font::None;
    bool           m_font_set       = false;
    double         m_size           = detail::kUnsetDouble;
    FontWeight     m_weight         = FontWeight::Normal;
    bool           m_weight_set     = false;
    FontSlant      m_slant          = FontSlant::Normal;
    bool           m_slant_set      = false;
    graphics::OptionalColor  m_fg_color;
    graphics::OptionalColor  m_bg_color;
    TextDecoration m_decoration     = TextDecoration::None;
    bool           m_decoration_set = false;
    DecorationStyle m_deco_style    = DecorationStyle::Solid;
    bool           m_deco_style_set = false;
    graphics::OptionalColor  m_deco_color;
    double         m_letter_spacing = detail::kUnsetSpacing;
    double         m_word_spacing   = detail::kUnsetSpacing;
    double         m_line_height    = detail::kUnsetDouble;
    VerticalAlign  m_vert_align     = VerticalAlign::Baseline;
    bool           m_vert_align_set = false;
    TextTransform  m_transform      = TextTransform::None;
    bool           m_transform_set  = false;
    float          m_opacity        = 1.0f;
    bool           m_opacity_set    = false;
    graphics::OptionalColor  m_outline_color;
    double         m_outline_width  = detail::kUnsetDouble;
    TextShadow     m_shadow;
    bool           m_shadow_set     = false;
    WritingDirection m_direction    = WritingDirection::LeftToRight;
    bool           m_direction_set  = false;

    TextAlign      m_align          = TextAlign::Left;
    bool           m_align_set      = false;
    TextOverflow   m_overflow       = TextOverflow::Visible;
    bool           m_overflow_set   = false;
    double         m_indent         = detail::kUnsetSpacing;
    double         m_para_spacing   = detail::kUnsetSpacing;
    unsigned int   m_max_lines      = 0; // 0 = unlimited
};

} // namespace text
} // namespace fizmo

#endif // FIZMO_TEXT_STYLE_HPP