#ifndef FIZMO_AUDIO_OPUS_HPP
#define FIZMO_AUDIO_OPUS_HPP

#include "buffer.hpp"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace fizmo {
namespace audio {

struct OpusHead {
    std::uint8_t version = 0;
    std::uint8_t channels = 0;
    std::uint16_t pre_skip = 0;
    std::uint32_t input_sample_rate = 0;
    std::int16_t output_gain = 0;
    std::uint8_t mapping_family = 0;
    std::uint8_t streams = 0;
    std::uint8_t coupled_streams = 0;
    std::vector<std::uint8_t> mapping;
};

struct OpusInfo {
    std::uint32_t sample_rate = 48000;
    std::uint16_t channels = 0;
    std::uint64_t frames = 0;
    std::uint32_t input_sample_rate = 0;
    std::uint16_t pre_skip = 0;
    std::int16_t output_gain = 0;
    std::uint8_t mapping_family = 0;
    std::vector<std::pair<std::string, std::string>> tags;
    std::string vendor;
    std::uint32_t links = 0;
    bool chain_stopped = false;
};

bool parse_opus_head(const void* data, std::size_t size, OpusHead& head, std::string* error = nullptr);
int opus_packet_frames(const void* packet, std::size_t size) noexcept;
int opus_packet_frames(const void* packet, std::size_t size, std::uint32_t streams) noexcept;

class OpusDecoder {
public:
    struct Impl;

    static constexpr std::uint32_t sample_rate = 48000;
    static constexpr std::size_t max_packet_frames = 5760;

    OpusDecoder();
    ~OpusDecoder();
    OpusDecoder(OpusDecoder&&) noexcept;
    OpusDecoder& operator=(OpusDecoder&&) noexcept;
    OpusDecoder(const OpusDecoder&) = delete;
    OpusDecoder& operator=(const OpusDecoder&) = delete;

    bool open(std::uint32_t channels, std::string* error = nullptr);
    bool open_head(const std::uint8_t* opus_head, std::size_t size, std::string* error = nullptr);
    bool open_head(const OpusHead& head, std::string* error = nullptr);
    bool open_multistream(std::uint32_t channels, std::uint32_t streams, std::uint32_t coupled_streams, const std::uint8_t* mapping, std::string* error = nullptr);
    void close();
    bool is_open() const noexcept;

    std::size_t decode(const std::uint8_t* packet, std::size_t size, std::vector<float>& out_interleaved, std::string* error = nullptr);
    std::size_t decode_fec(const std::uint8_t* packet, std::size_t size, std::size_t frames, std::vector<float>& out_interleaved);
    std::size_t conceal(std::size_t frames, std::vector<float>& out);
    void reset();

    std::uint32_t channels() const noexcept;
    std::uint32_t streams() const noexcept;
    std::uint32_t coupled_streams() const noexcept;
    std::uint16_t pre_skip() const noexcept;
    std::int16_t output_gain() const noexcept;
    void set_output_gain(std::int16_t gain_q8) noexcept;
    const OpusHead& head() const noexcept;
    std::uint32_t final_range() const noexcept;
    bool set_output_rate(std::uint32_t rate, std::string* error = nullptr);
    std::uint32_t output_rate() const noexcept;

private:
    std::unique_ptr<Impl> m_impl;
};

bool is_ogg_opus(const void* data, std::size_t size) noexcept;
bool read_opus(const std::string& path, AudioBuffer& out, std::string* error = nullptr, OpusInfo* info = nullptr);
bool read_opus(const void* data, std::size_t size, AudioBuffer& out, std::string* error = nullptr, OpusInfo* info = nullptr);

class OpusReader {
public:
    struct Impl;

    static constexpr std::uint64_t seek_preroll = 3840;

    OpusReader();
    ~OpusReader();
    OpusReader(OpusReader&&) noexcept;
    OpusReader& operator=(OpusReader&&) noexcept;
    OpusReader(const OpusReader&) = delete;
    OpusReader& operator=(const OpusReader&) = delete;

    bool open(const std::string& path, std::string* error = nullptr);
    bool open(const void* data, std::size_t size, std::string* error = nullptr);
    void close();
    bool is_open() const noexcept;

    const OpusInfo& info() const noexcept { return m_info; }
    std::size_t read(float* interleaved, std::size_t frames);
    std::size_t read(AudioBuffer& out, std::size_t frames);
    bool seek(std::uint64_t frame);
    std::uint64_t position() const noexcept { return m_position; }

private:
    bool finish_open(std::string* error);

    std::unique_ptr<Impl> m_impl;
    OpusInfo m_info;
    std::uint64_t m_position = 0;
};

} // namespace audio
} // namespace fizmo

#endif // FIZMO_AUDIO_OPUS_HPP
