#ifndef FIZMO_AUDIO_AAC_HPP
#define FIZMO_AUDIO_AAC_HPP

#include "buffer.hpp"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace fizmo {
namespace audio {

struct AacConfig {
    int object_type = 0;
    std::uint32_t sample_rate = 0;
    std::uint16_t channels = 0;
    bool sbr = false;
    bool ps = false;
    std::uint32_t output_rate = 0;
    std::uint16_t output_channels = 0;
    std::uint32_t frame_length = 0;
    int channel_config = 0;
    int sample_rate_index = 0;
    std::uint32_t extension_rate = 0;
    std::uint32_t core_frame_length = 0;
    std::uint32_t channel_mask = 0;
    bool implicit_signalling = false;
};

bool parse_audio_specific_config(const std::uint8_t* data, std::size_t size, AacConfig& config, std::string* error = nullptr);

class AacDecoder {
public:
    AacDecoder();
    ~AacDecoder();
    AacDecoder(AacDecoder&&) noexcept;
    AacDecoder& operator=(AacDecoder&&) noexcept;
    AacDecoder(const AacDecoder&) = delete;
    AacDecoder& operator=(const AacDecoder&) = delete;

    bool open(const std::uint8_t* asc, std::size_t size, std::string* error = nullptr);
    bool open_adts();
    std::size_t decode(const std::uint8_t* au, std::size_t size, std::vector<float>& out_interleaved, std::string* error = nullptr);
    void reset();
    const AacConfig& config() const noexcept { return m_config; }
    std::uint32_t delay() const noexcept;
    bool is_open() const noexcept;

    struct Impl;

private:
    void refresh();

    std::unique_ptr<Impl> m_impl;
    AacConfig m_config;
};

struct AacInfo {
    std::uint32_t sample_rate = 0;
    std::uint16_t channels = 0;
    std::uint64_t frames = 0;
    std::uint32_t bitrate_kbps = 0;
    int object_type = 0;
    bool sbr = false;
    bool ps = false;
    std::uint32_t core_sample_rate = 0;
    std::uint32_t channel_mask = 0;
    std::uint64_t adts_frames = 0;
    std::uint32_t decoder_delay = 0;
    std::uint32_t skipped_samples = 0;
};

bool is_aac_adts(const void* data, std::size_t size) noexcept;
bool read_aac(const std::string& path, AudioBuffer& out, std::string* error = nullptr, AacInfo* info = nullptr);
bool read_aac(const void* data, std::size_t size, AudioBuffer& out, std::string* error = nullptr, AacInfo* info = nullptr);

class AacReader {
public:
    struct State;

    AacReader();
    ~AacReader();
    AacReader(AacReader&&) noexcept;
    AacReader& operator=(AacReader&&) noexcept;
    AacReader(const AacReader&) = delete;
    AacReader& operator=(const AacReader&) = delete;

    bool open(const std::string& path, std::string* error = nullptr);
    bool open(const void* data, std::size_t size, std::string* error = nullptr);
    void close();
    bool is_open() const noexcept;

    const AacInfo& info() const noexcept { return m_info; }
    std::size_t read(float* interleaved, std::size_t frames);
    std::size_t read(AudioBuffer& out, std::size_t frames);
    bool seek(std::uint64_t frame);
    std::uint64_t position() const noexcept { return m_position; }

private:
    bool finish_open(std::string* error);

    std::unique_ptr<State> m_state;
    AacInfo m_info;
    std::uint64_t m_position = 0;
};

} // namespace audio
} // namespace fizmo

#endif // FIZMO_AUDIO_AAC_HPP
