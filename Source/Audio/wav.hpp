#ifndef FIZMO_AUDIO_WAV_HPP
#define FIZMO_AUDIO_WAV_HPP

#include "buffer.hpp"
#include "file_io.hpp"
#include "format.hpp"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace fizmo {
namespace audio {

struct WavInfo {
    std::uint32_t sample_rate = 0;
    std::uint16_t channels = 0;
    SampleFormat format = SampleFormat::s16;
    std::uint16_t valid_bits = 0;
    std::uint32_t channel_mask = 0;
    std::uint64_t frames = 0;
    bool rf64 = false;
    std::vector<std::pair<std::string, std::string>> tags;
};

bool is_wav(const void* data, std::size_t size) noexcept;
bool read_wav(const std::string& path, AudioBuffer& out, std::string* error = nullptr, WavInfo* info = nullptr);
bool read_wav(const void* data, std::size_t size, AudioBuffer& out, std::string* error = nullptr, WavInfo* info = nullptr);
bool write_wav(const std::string& path, const AudioBuffer& buffer, SampleFormat format = SampleFormat::s16, std::string* error = nullptr, bool dither = true);
std::vector<std::uint8_t> encode_wav(const AudioBuffer& buffer, SampleFormat format = SampleFormat::s16, bool dither = true);
std::uint32_t default_channel_mask(std::size_t channels) noexcept;

class WavReader {
public:
    struct Source;

    WavReader();
    ~WavReader();
    WavReader(WavReader&&) noexcept;
    WavReader& operator=(WavReader&&) noexcept;
    WavReader(const WavReader&) = delete;
    WavReader& operator=(const WavReader&) = delete;

    bool open(const std::string& path, std::string* error = nullptr);
    bool open(const void* data, std::size_t size, std::string* error = nullptr);
    void close();
    bool is_open() const noexcept;

    const WavInfo& info() const noexcept { return m_info; }
    std::size_t read(float* interleaved, std::size_t frames);
    std::size_t read(AudioBuffer& out, std::size_t frames);
    bool seek(std::uint64_t frame);
    std::uint64_t position() const noexcept { return m_position; }

private:
    std::unique_ptr<Source> m_source;
    WavInfo m_info;
    std::uint64_t m_data_offset = 0;
    std::uint64_t m_position = 0;
    std::size_t m_block = 0;
    std::vector<std::uint8_t> m_raw;
};

class WavWriter {
public:
    WavWriter() = default;
    ~WavWriter();
    WavWriter(const WavWriter&) = delete;
    WavWriter& operator=(const WavWriter&) = delete;

    bool open(const std::string& path, std::size_t channels, std::uint32_t sample_rate, SampleFormat format = SampleFormat::s16, std::string* error = nullptr, bool dither = true);
    bool write(const float* interleaved, std::size_t frames);
    bool write(const AudioBuffer& buffer);
    bool close(std::string* error = nullptr);
    bool is_open() const noexcept { return m_file.is_open(); }
    std::uint64_t frames_written() const noexcept { return m_frames; }

private:
    detail::File m_file;
    std::size_t m_channels = 0;
    std::uint32_t m_rate = 0;
    SampleFormat m_format = SampleFormat::s16;
    bool m_dither = true;
    std::uint64_t m_frames = 0;
    std::int64_t m_header_riff = 0;
    std::int64_t m_ds64 = 0;
    std::int64_t m_fact = -1;
    std::int64_t m_data_size_pos = 0;
    std::int64_t m_data_start = 0;
    std::vector<std::uint8_t> m_raw;
    bool m_failed = false;
};

} // namespace audio
} // namespace fizmo

#endif // FIZMO_AUDIO_WAV_HPP
