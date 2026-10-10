#ifndef ENDIANESS_UTIL_HPP
#define ENDIANESS_UTIL_HPP

#include <cstdint>
#include <array>
#include <type_traits>

namespace fizmo {

constexpr bool SYSTEM_LITTLE_ENDIAN = ((static_cast<unsigned>(0x01020304) & 0xFFu) == 0x04u);
constexpr bool SYSTEM_BIG_ENDIAN = ((static_cast<unsigned>(0x01020304) & 0xFFu) == 0x01u);
constexpr bool SYSTEM_MIDDLE_ENDIAN = ((static_cast<unsigned>(0x01020304) & 0xFFu) == 0x02u);
constexpr bool SYSTEM_BIG_ENDIAN_SWAPPED = ((static_cast<unsigned>(0x01020304) & 0xFFu) == 0x03u);

enum class endianess {
    LITTLE = 0,
    MIDDLE,
    BIG,
    BIG_WORD_SWAPPED,
};

template <typename T, typename = typename std::enable_if<std::is_integral<T>::value>::type>
constexpr std::array<std::uint8_t, sizeof(T)> 
convert_to_bytes(const T v, const endianess byte_order = endianess::LITTLE) noexcept {
    std::array<std::uint8_t, sizeof(T)> out = {};
    constexpr std::size_t num_bytes = sizeof(T);
    const typename std::make_unsigned<T>::type uv = static_cast<typename std::make_unsigned<T>::type>(v);

    switch (byte_order) {
        case endianess::LITTLE:
            for (std::size_t i = 0; i < num_bytes; ++i) {
                out[i] = static_cast<std::uint8_t>((uv >> (8 * i)) & 0xFF);
            }
            break;

        case endianess::BIG:
            for (std::size_t i = 0; i < num_bytes; ++i) {
                out[i] = static_cast<std::uint8_t>((uv >> (8 * (num_bytes - 1 - i))) & 0xFF);
            }
            break;

        case endianess::MIDDLE:
            for (std::size_t i = 0; i < num_bytes; ++i) {
                const std::size_t pair = i / 2;
                const std::size_t offset = i % 2;
                const std::size_t src_byte = pair * 2 + (1 - offset);
                out[i] = static_cast<std::uint8_t>((uv >> (8 * src_byte)) & 0xFF);
            }
            break;

        case endianess::BIG_WORD_SWAPPED:
            for (std::size_t i = 0; i < num_bytes; ++i) {
                const std::size_t pair = i / 2;
                const std::size_t offset = i % 2;
                const std::size_t swapped_pair = (num_bytes / 2) - 1 - pair;
                const std::size_t src_byte = swapped_pair * 2 + offset;
                out[i] = static_cast<std::uint8_t>((uv >> (8 * src_byte)) & 0xFF);
            }
            break;
    }

    return out;
}

} // namespace fizmo

#endif // ENDIANESS_UTIL_HPP