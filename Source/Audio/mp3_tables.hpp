#ifndef FIZMO_AUDIO_MP3_TABLES_HPP
#define FIZMO_AUDIO_MP3_TABLES_HPP

#include <cstdint>

namespace fizmo {
namespace audio {
namespace detail {
namespace mp3tables {

struct HuffmanCodeword {
    std::uint8_t length;
    std::uint32_t code;
};

struct HuffmanTableSpec {
    int xlen;
    int ylen;
    int linbits;
    int data_table;
    const HuffmanCodeword* codewords;
};

struct Layer2Class {
    std::uint16_t steps;
    std::uint8_t grouped;
    std::uint8_t bits;
};

struct Layer2Allocation {
    int sblimit;
    std::uint8_t nbal[32];
    std::uint16_t steps[32][16];
};

extern const double k_synthesis_window[512];
extern const double k_analysis_window[512];
extern const HuffmanTableSpec k_huffman_tables[34];
extern const Layer2Class k_layer2_classes[17];
extern const Layer2Allocation k_layer2_allocations[5];

} // namespace mp3tables
} // namespace detail
} // namespace audio
} // namespace fizmo

#endif // FIZMO_AUDIO_MP3_TABLES_HPP
