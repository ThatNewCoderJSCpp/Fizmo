#ifndef FIZMO_AUDIO_MP3_HPP
#define FIZMO_AUDIO_MP3_HPP

#include "buffer.hpp"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace fizmo {
namespace audio {

struct Mp3Info {
    std::uint32_t sample_rate = 0;
    std::uint16_t channels = 0;
    std::uint64_t frames = 0;
    std::uint32_t bitrate_kbps = 0;
    bool vbr = false;
    int mpeg_version = 0;
    int layer = 0;
    std::uint32_t encoder_delay = 0;
    std::uint32_t encoder_padding = 0;
    std::vector<std::pair<std::string, std::string>> tags;
};

bool is_mp3(const void* data, std::size_t size) noexcept;
bool read_mp3(const std::string& path, AudioBuffer& out, std::string* error = nullptr, Mp3Info* info = nullptr);
bool read_mp3(const void* data, std::size_t size, AudioBuffer& out, std::string* error = nullptr, Mp3Info* info = nullptr);

class Mp3Reader {
public:
    struct State;

    Mp3Reader();
    ~Mp3Reader();
    Mp3Reader(Mp3Reader&&) noexcept;
    Mp3Reader& operator=(Mp3Reader&&) noexcept;
    Mp3Reader(const Mp3Reader&) = delete;
    Mp3Reader& operator=(const Mp3Reader&) = delete;

    bool open(const std::string& path, std::string* error = nullptr);
    bool open(const void* data, std::size_t size, std::string* error = nullptr);
    void close();
    bool is_open() const noexcept;

    const Mp3Info& info() const noexcept { return m_info; }
    std::size_t read(float* interleaved, std::size_t frames);
    std::size_t read(AudioBuffer& out, std::size_t frames);
    bool seek(std::uint64_t frame);
    std::uint64_t position() const noexcept { return m_position; }

private:
    bool finish_open(std::string* error);

    std::unique_ptr<State> m_state;
    Mp3Info m_info;
    std::uint64_t m_position = 0;
};

class Mp3Decoder {
public:
    struct Impl;

    Mp3Decoder();
    ~Mp3Decoder();
    Mp3Decoder(Mp3Decoder&&) noexcept;
    Mp3Decoder& operator=(Mp3Decoder&&) noexcept;
    Mp3Decoder(const Mp3Decoder&) = delete;
    Mp3Decoder& operator=(const Mp3Decoder&) = delete;

    bool open(std::uint16_t channels = 0);
    void close();
    bool is_open() const noexcept;

    std::size_t decode(const std::uint8_t* packet, std::size_t size, std::vector<float>& out_interleaved, std::string* error = nullptr);
    void reset();

    std::uint32_t sample_rate() const noexcept;
    std::uint16_t channels() const noexcept;

private:
    std::unique_ptr<Impl> m_impl;
};

bool mp3_frame_info(const std::uint8_t* data, std::size_t size, std::uint32_t& sample_rate, std::uint16_t& channels, std::uint32_t& samples) noexcept;

} // namespace audio
} // namespace fizmo

#endif // FIZMO_AUDIO_MP3_HPP
