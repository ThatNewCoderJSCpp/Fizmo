#ifndef FIZMO_UNICODE_TABLES_HPP
#define FIZMO_UNICODE_TABLES_HPP

#include <cstddef>
#include <cstdint>

namespace fizmo {
namespace unicode {
namespace tables {

enum : std::uint16_t {
    flag_extended_pictographic = 1u << 0,
    flag_white_space           = 1u << 1,
    flag_alphabetic            = 1u << 2,
    flag_lowercase             = 1u << 3,
    flag_uppercase             = 1u << 4,
    flag_cased                 = 1u << 5,
    flag_case_ignorable        = 1u << 6,
    flag_default_ignorable     = 1u << 7,
    flag_emoji_presentation    = 1u << 8,
    flag_special_case          = 1u << 9,
    flag_canonical_decomp      = 1u << 10,
    flag_compat_decomp         = 1u << 11,
    flag_emoji                 = 1u << 12,
    flag_nfc_qc_not_yes        = 1u << 13,
    flag_nfkc_qc_not_yes       = 1u << 14,
    flag_composes_second       = 1u << 15
};

struct Record {
    std::int32_t  upper_delta;
    std::int32_t  lower_delta;
    std::int32_t  title_delta;
    std::int32_t  fold_delta;
    std::uint16_t flags;
    std::uint8_t  general_category;
    std::uint8_t  grapheme_break;
    std::uint8_t  word_break;
    std::uint8_t  combining_class;
    std::uint8_t  indic_conjunct_break;
    std::uint8_t  width;
};

struct DecompositionEntry {
    char32_t      code_point;
    std::uint16_t canonical_offset;
    std::uint8_t  canonical_length;
    std::uint16_t compat_offset;
    std::uint8_t  compat_length;
};

struct CompositionEntry {
    char32_t first;
    char32_t second;
    char32_t composite;
};

struct SpecialCaseEntry {
    char32_t code_point;
    char32_t lower[3];
    char32_t upper[3];
    char32_t title[3];
    char32_t fold[3];
};

} // namespace tables
} // namespace unicode
} // namespace fizmo

#endif // FIZMO_UNICODE_TABLES_HPP
