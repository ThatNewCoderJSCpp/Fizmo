#ifndef FIZMO_AUDIO_FORMAT_HPP
#define FIZMO_AUDIO_FORMAT_HPP

#include <cstddef>
#include <cstdint>

namespace fizmo {
namespace audio {

enum class SampleFormat {
    u8,
    s16,
    s24,
    s32,
    f32,
    f64,
    alaw,
    ulaw
};

std::size_t bytes_per_sample(SampleFormat format) noexcept;
std::size_t bits_per_sample(SampleFormat format) noexcept;
bool is_float_format(SampleFormat format) noexcept;
const char* format_name(SampleFormat format) noexcept;

void to_float(const void* source, SampleFormat format, float* destination, std::size_t samples) noexcept;
void from_float(const float* source, void* destination, SampleFormat format, std::size_t samples, bool dither = false) noexcept;

float alaw_to_float(std::uint8_t value) noexcept;
float ulaw_to_float(std::uint8_t value) noexcept;
std::uint8_t float_to_alaw(float value) noexcept;
std::uint8_t float_to_ulaw(float value) noexcept;

void remix(const float* source, std::size_t source_channels, float* destination, std::size_t destination_channels, std::size_t frames) noexcept;

float db_to_gain(float db) noexcept;
float gain_to_db(float gain) noexcept;

} // namespace audio
} // namespace fizmo

#endif // FIZMO_AUDIO_FORMAT_HPP
