#ifndef FIZMO_JPEG_CODEC_HPP
#define FIZMO_JPEG_CODEC_HPP

#include "../Bitmap/image.hpp"
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

namespace fizmo {
namespace images {

inline bool is_jpeg(const std::uint8_t* data, std::size_t size) noexcept {
    return size >= 3 && data[0] == 0xFF && data[1] == 0xD8 && data[2] == 0xFF;
}

bool decode_jpeg(const std::uint8_t* data, std::size_t size, BitmapImage& out, std::string* error = nullptr);

bool decode_bmp(const std::uint8_t* data, std::size_t size, BitmapImage& out, std::string* error = nullptr);

} // namespace images
} // namespace fizmo

#endif // FIZMO_JPEG_CODEC_HPP
