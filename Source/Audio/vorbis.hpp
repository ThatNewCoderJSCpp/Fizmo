#ifndef FIZMO_AUDIO_VORBIS_HPP
#define FIZMO_AUDIO_VORBIS_HPP

#include "buffer.hpp"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace fizmo {
namespace audio {

struct VorbisInfo {
    std::uint32_t sample_rate = 0;
    std::uint16_t channels = 0;
    std::uint64_t frames = 0;
    std::uint32_t bitrate_nominal = 0;
    std::vector<std::pair<std::string, std::string>> tags;
    std::string vendor;
    std::uint32_t links = 0;
    bool chain_stopped = false;
};

bool is_ogg_vorbis(const void* data, std::size_t size) noexcept;
bool read_vorbis(const std::string& path, AudioBuffer& out, std::string* error = nullptr, VorbisInfo* info = nullptr);
bool read_vorbis(const void* data, std::size_t size, AudioBuffer& out, std::string* error = nullptr, VorbisInfo* info = nullptr);

class VorbisReader {
public:
    struct Impl;

    VorbisReader();
    ~VorbisReader();
    VorbisReader(VorbisReader&&) noexcept;
    VorbisReader& operator=(VorbisReader&&) noexcept;
    VorbisReader(const VorbisReader&) = delete;
    VorbisReader& operator=(const VorbisReader&) = delete;

    bool open(const std::string& path, std::string* error = nullptr);
    bool open(const void* data, std::size_t size, std::string* error = nullptr);
    void close();
    bool is_open() const noexcept;

    const VorbisInfo& info() const noexcept { return m_info; }
    std::size_t read(float* interleaved, std::size_t frames);
    std::size_t read(AudioBuffer& out, std::size_t frames);
    bool seek(std::uint64_t frame);
    std::uint64_t position() const noexcept { return m_position; }

private:
    bool finish_open(std::string* error);

    std::unique_ptr<Impl> m_impl;
    VorbisInfo m_info;
    std::uint64_t m_position = 0;
};

class VorbisDecoder {
public:
    struct Impl;

    VorbisDecoder();
    ~VorbisDecoder();
    VorbisDecoder(VorbisDecoder&&) noexcept;
    VorbisDecoder& operator=(VorbisDecoder&&) noexcept;
    VorbisDecoder(const VorbisDecoder&) = delete;
    VorbisDecoder& operator=(const VorbisDecoder&) = delete;

    bool open(const std::uint8_t* xiph_headers, std::size_t size, std::string* error = nullptr);
    bool open(const std::uint8_t* identification, std::size_t identification_size, const std::uint8_t* setup, std::size_t setup_size, std::string* error = nullptr);
    void close();
    bool is_open() const noexcept;

    std::size_t decode(const std::uint8_t* packet, std::size_t size, std::vector<float>& out_interleaved, std::string* error = nullptr);
    std::size_t packet_samples(const std::uint8_t* packet, std::size_t size) const noexcept;
    void reset();

    std::uint32_t sample_rate() const noexcept;
    std::uint16_t channels() const noexcept;

private:
    std::unique_ptr<Impl> m_impl;
};

} // namespace audio
} // namespace fizmo

#endif // FIZMO_AUDIO_VORBIS_HPP
