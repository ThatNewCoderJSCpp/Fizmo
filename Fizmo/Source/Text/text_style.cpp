#define ALL_FIZMO
#include <fizmo/includes.hpp>
#include "text_style.hpp"

namespace fizmo {
namespace text {

auto TextStyle::set_underline(bool u) noexcept -> void {
        if (u) add_decoration(TextDecoration::Underline);
        else   remove_decoration(TextDecoration::Underline);
    }

auto TextStyle::set_strikethrough(bool s) noexcept -> void {
        if (s) add_decoration(TextDecoration::Strikethrough);
        else   remove_decoration(TextDecoration::Strikethrough);
    }

auto TextStyle::set_overline(bool o) noexcept -> void {
        if (o) add_decoration(TextDecoration::Overline);
        else   remove_decoration(TextDecoration::Overline);
    }

auto TextStyle::merge_over(const TextStyle& over) const noexcept -> TextStyle {
        TextStyle r = *this;

        if (over.m_font_set)                 { r.m_font           = over.m_font;          r.m_font_set     = true; }
        if (over.has_size())                   r.m_size           = over.m_size;
        if (over.m_weight_set)               { r.m_weight         = over.m_weight;        r.m_weight_set     = true; }
        if (over.m_slant_set)                { r.m_slant          = over.m_slant;         r.m_slant_set      = true; }
        if (over.m_fg_color.is_set())          r.m_fg_color       = over.m_fg_color;
        if (over.m_bg_color.is_set())          r.m_bg_color       = over.m_bg_color;
        if (over.m_decoration_set)           { r.m_decoration     = over.m_decoration;    r.m_decoration_set = true; }
        if (over.m_deco_style_set)           { r.m_deco_style     = over.m_deco_style;    r.m_deco_style_set = true; }
        if (over.m_deco_color.is_set())        r.m_deco_color     = over.m_deco_color;
        if (over.has_letter_spacing())         r.m_letter_spacing = over.m_letter_spacing;
        if (over.has_word_spacing())           r.m_word_spacing   = over.m_word_spacing;
        if (over.has_line_height())            r.m_line_height    = over.m_line_height;
        if (over.m_vert_align_set)           { r.m_vert_align     = over.m_vert_align;    r.m_vert_align_set = true; }
        if (over.m_transform_set)            { r.m_transform      = over.m_transform;     r.m_transform_set  = true; }
        if (over.m_opacity_set)              { r.m_opacity        = over.m_opacity;       r.m_opacity_set    = true; }
        if (over.m_outline_color.is_set())     r.m_outline_color  = over.m_outline_color;
        if (over.has_outline_width())          r.m_outline_width  = over.m_outline_width;
        if (over.m_shadow_set)               { r.m_shadow         = over.m_shadow;        r.m_shadow_set     = true; }
        if (over.m_direction_set)            { r.m_direction      = over.m_direction;     r.m_direction_set  = true; }
        if (over.m_align_set)                { r.m_align          = over.m_align;         r.m_align_set      = true; }
        if (over.m_overflow_set)             { r.m_overflow       = over.m_overflow;      r.m_overflow_set   = true; }
        if (over.has_indent())                 r.m_indent         = over.m_indent;
        if (over.has_paragraph_spacing())      r.m_para_spacing   = over.m_para_spacing;
        if (over.has_max_lines())              r.m_max_lines      = over.m_max_lines;

        return r;
    }

auto TextStyle::is_empty() const noexcept -> bool {
        return !m_font_set
            && !has_size()
            && !m_weight_set
            && !m_slant_set
            && !m_fg_color.is_set()
            && !m_bg_color.is_set()
            && !m_decoration_set
            && !m_deco_style_set
            && !m_deco_color.is_set()
            && !has_letter_spacing()
            && !has_word_spacing()
            && !has_line_height()
            && !m_vert_align_set
            && !m_transform_set
            && !m_opacity_set
            && !m_outline_color.is_set()
            && !has_outline_width()
            && !m_shadow_set
            && !m_direction_set
            && !m_align_set
            && !m_overflow_set
            && !has_indent()
            && !has_paragraph_spacing()
            && !has_max_lines();
    }

auto TextStyle::operator==(const TextStyle& o) const noexcept -> bool {
        return m_font           == o.m_font
            && m_font_set       == o.m_font_set
            && m_size           == o.m_size
            && m_weight         == o.m_weight
            && m_weight_set     == o.m_weight_set
            && m_slant          == o.m_slant
            && m_slant_set      == o.m_slant_set
            && m_fg_color       == o.m_fg_color
            && m_bg_color       == o.m_bg_color
            && m_decoration     == o.m_decoration
            && m_decoration_set == o.m_decoration_set
            && m_deco_style     == o.m_deco_style
            && m_deco_style_set == o.m_deco_style_set
            && m_deco_color     == o.m_deco_color
            && m_letter_spacing == o.m_letter_spacing
            && m_word_spacing   == o.m_word_spacing
            && m_line_height    == o.m_line_height
            && m_vert_align     == o.m_vert_align
            && m_vert_align_set == o.m_vert_align_set
            && m_transform      == o.m_transform
            && m_transform_set  == o.m_transform_set
            && m_opacity        == o.m_opacity
            && m_opacity_set    == o.m_opacity_set
            && m_outline_color  == o.m_outline_color
            && m_outline_width  == o.m_outline_width
            && m_shadow         == o.m_shadow
            && m_shadow_set     == o.m_shadow_set
            && m_direction      == o.m_direction
            && m_direction_set  == o.m_direction_set
            && m_align          == o.m_align
            && m_align_set      == o.m_align_set
            && m_overflow       == o.m_overflow
            && m_overflow_set   == o.m_overflow_set
            && m_indent         == o.m_indent
            && m_para_spacing   == o.m_para_spacing
            && m_max_lines      == o.m_max_lines;
    }

} // namespace text
} // namespace fizmo
