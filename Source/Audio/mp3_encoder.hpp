#ifndef FIZMO_AUDIO_MP3_ENCODER_HPP
#define FIZMO_AUDIO_MP3_ENCODER_HPP

#include "buffer.hpp"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace fizmo {
namespace audio {

enum class Mp3Mode {
    automatic,
    stereo,
    joint_stereo,
    mono
};

struct Mp3EncodeOptions {
    std::uint32_t bitrate_kbps = 192;
    bool vbr = false;
    int vbr_quality = 2;
    int quality = 2;
    Mp3Mode mode = Mp3Mode::automatic;
    bool write_xing = true;
    std::vector<std::pair<std::string, std::string>> tags;
};

bool write_mp3(const std::string& path, const AudioBuffer& buffer, const Mp3EncodeOptions& options = {}, std::string* error = nullptr);
std::vector<std::uint8_t> encode_mp3(const AudioBuffer& buffer, const Mp3EncodeOptions& options = {});
std::uint32_t mp3_encoder_sample_rate(std::uint32_t input_rate) noexcept;
std::uint32_t mp3_encoder_bitrate(std::uint32_t sample_rate, std::uint32_t requested_kbps) noexcept;

class Mp3Writer {
public:
    struct Impl;

    Mp3Writer();
    ~Mp3Writer();
    Mp3Writer(Mp3Writer&&) noexcept;
    Mp3Writer& operator=(Mp3Writer&&) noexcept;
    Mp3Writer(const Mp3Writer&) = delete;
    Mp3Writer& operator=(const Mp3Writer&) = delete;

    bool open(const std::string& path, std::size_t channels, std::uint32_t sample_rate, const Mp3EncodeOptions& options = {}, std::string* error = nullptr);
    bool write(const float* interleaved, std::size_t frames);
    bool write(const AudioBuffer& buffer);
    bool close(std::string* error = nullptr);
    bool is_open() const noexcept;
    std::uint64_t frames_written() const noexcept;
    std::uint32_t output_sample_rate() const noexcept;
    std::size_t output_channels() const noexcept;
    std::uint32_t encoder_delay() const noexcept;

private:
    std::unique_ptr<Impl> m_impl;
};

} // namespace audio
} // namespace fizmo

#endif // FIZMO_AUDIO_MP3_ENCODER_HPP
