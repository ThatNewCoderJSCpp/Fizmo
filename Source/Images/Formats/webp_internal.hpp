#ifndef FIZMO_IMAGES_WEBP_INTERNAL_HPP
#define FIZMO_IMAGES_WEBP_INTERNAL_HPP

#include "format_util.hpp"

namespace fizmo {
namespace images {
namespace webp_detail {

bool vp8l_decode(const std::uint8_t* data, std::size_t size, fmt::Rgba& out, std::string* error);
bool vp8l_decode_stream(const std::uint8_t* data, std::size_t size, unsigned width, unsigned height, std::vector<std::uint32_t>& argb, std::string* error);
bool vp8l_encode(const fmt::Rgba& img, std::vector<std::uint8_t>& out, int effort);
bool vp8l_encode_stream(const std::vector<std::uint32_t>& argb, unsigned width, unsigned height, std::vector<std::uint8_t>& out, int effort);

bool vp8_decode(const std::uint8_t* data, std::size_t size, fmt::Rgba& out, std::string* error);
bool vp8_encode(const fmt::Rgba& img, int quality, std::vector<std::uint8_t>& out);

} // namespace webp_detail
} // namespace images
} // namespace fizmo

#endif // FIZMO_IMAGES_WEBP_INTERNAL_HPP
