#ifndef FIZMO_PNG_CODEC_HPP
#define FIZMO_PNG_CODEC_HPP

#include "../Bitmap/image.hpp"
#include <array>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

namespace fizmo {
namespace images {

namespace png_detail {

std::uint32_t be32(const std::uint8_t* p) noexcept;

void put32(std::vector<std::uint8_t>& out, std::uint32_t v);

std::uint8_t paeth(int a, int b, int c) noexcept;

bool unfilter(std::uint8_t* data, std::size_t rows, std::size_t stride, std::size_t bpp, std::uint8_t* out);

} // namespace png_detail

bool is_png(const std::uint8_t* data, std::size_t size) noexcept;

bool decode_png(const std::uint8_t* data, std::size_t size, BitmapImage& out, std::string* error = nullptr);

bool encode_png(const BitmapImage& img, std::vector<std::uint8_t>& out, bool alpha = true, int level = 6);

} // namespace images
} // namespace fizmo

#endif // FIZMO_PNG_CODEC_HPP
