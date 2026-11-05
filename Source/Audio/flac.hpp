#ifndef FIZMO_AUDIO_FLAC_HPP
#define FIZMO_AUDIO_FLAC_HPP

#include "buffer.hpp"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace fizmo {
namespace audio {

struct FlacInfo {
    std::uint32_t sample_rate = 0;
    std::uint16_t channels = 0;
    std::uint16_t bits_per_sample = 0;
    std::uint64_t frames = 0;
    std::vector<std::pair<std::string, std::string>> tags;
    std::uint8_t md5[16] = {};
    std::uint16_t min_block_size = 0;
    std::uint16_t max_block_size = 0;
    std::uint32_t min_frame_size = 0;
    std::uint32_t max_frame_size = 0;
    bool has_seek_table = false;
};

struct FlacEncodeOptions {
    int compression = 5;
    unsigned bits_per_sample = 16;
    bool verify = false;
    std::vector<std::pair<std::string, std::string>> tags;
    unsigned block_size = 0;
    bool dither = true;
};

bool is_flac(const void* data, std::size_t size) noexcept;
bool read_flac(const std::string& path, AudioBuffer& out, std::string* error = nullptr, FlacInfo* info = nullptr);
bool read_flac(const void* data, std::size_t size, AudioBuffer& out, std::string* error = nullptr, FlacInfo* info = nullptr);
bool write_flac(const std::string& path, const AudioBuffer& buffer, const FlacEncodeOptions& options = {}, std::string* error = nullptr);
std::vector<std::uint8_t> encode_flac(const AudioBuffer& buffer, const FlacEncodeOptions& options = {});
std::vector<std::uint8_t> encode_flac(const AudioBuffer& buffer, const FlacEncodeOptions& options, std::string* error);

class FlacReader {
public:
    struct Impl;

    FlacReader();
    ~FlacReader();
    FlacReader(FlacReader&&) noexcept;
    FlacReader& operator=(FlacReader&&) noexcept;
    FlacReader(const FlacReader&) = delete;
    FlacReader& operator=(const FlacReader&) = delete;

    bool open(const std::string& path, std::string* error = nullptr);
    bool open(const void* data, std::size_t size, std::string* error = nullptr);
    void close();
    bool is_open() const noexcept;

    const FlacInfo& info() const noexcept;
    std::size_t read(float* interleaved, std::size_t frames);
    std::size_t read(AudioBuffer& out, std::size_t frames);
    std::size_t read_pcm(std::int32_t* interleaved, std::size_t frames);
    bool seek(std::uint64_t frame);
    std::uint64_t position() const noexcept;
    std::size_t error_count() const noexcept;
    const std::string& last_error() const noexcept;

private:
    std::unique_ptr<Impl> m_impl;
};

class FlacWriter {
public:
    struct Impl;

    FlacWriter();
    ~FlacWriter();
    FlacWriter(FlacWriter&&) noexcept;
    FlacWriter& operator=(FlacWriter&&) noexcept;
    FlacWriter(const FlacWriter&) = delete;
    FlacWriter& operator=(const FlacWriter&) = delete;

    bool open(const std::string& path, std::size_t channels, std::uint32_t sample_rate, const FlacEncodeOptions& options = {}, std::string* error = nullptr, std::uint64_t expected_frames = 0);
    bool write(const float* interleaved, std::size_t frames);
    bool write(const AudioBuffer& buffer);
    bool write_pcm(const std::int32_t* interleaved, std::size_t frames);
    bool close(std::string* error = nullptr);
    bool is_open() const noexcept;
    std::uint64_t frames_written() const noexcept;

private:
    std::unique_ptr<Impl> m_impl;
};

class FlacDecoder {
public:
    struct Impl;

    FlacDecoder();
    ~FlacDecoder();
    FlacDecoder(FlacDecoder&&) noexcept;
    FlacDecoder& operator=(FlacDecoder&&) noexcept;
    FlacDecoder(const FlacDecoder&) = delete;
    FlacDecoder& operator=(const FlacDecoder&) = delete;

    bool open(const std::uint8_t* config, std::size_t size, std::string* error = nullptr);
    void close();
    bool is_open() const noexcept;

    std::size_t decode(const std::uint8_t* frame, std::size_t size, std::vector<float>& out_interleaved, std::string* error = nullptr);
    void reset();

    std::uint32_t sample_rate() const noexcept;
    std::uint16_t channels() const noexcept;
    std::uint16_t bits_per_sample() const noexcept;
    std::uint64_t total_frames() const noexcept;
    std::uint32_t fixed_block_size() const noexcept;

private:
    std::unique_ptr<Impl> m_impl;
};

} // namespace audio
} // namespace fizmo

#endif // FIZMO_AUDIO_FLAC_HPP
