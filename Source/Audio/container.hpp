#ifndef FIZMO_AUDIO_CONTAINER_HPP
#define FIZMO_AUDIO_CONTAINER_HPP

#include "buffer.hpp"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace fizmo {
namespace audio {

struct ContainerAudioInfo {
    std::uint32_t sample_rate = 0;
    std::uint16_t channels = 0;
    std::uint64_t frames = 0;
    double duration = 0.0;
    std::uint32_t bitrate_kbps = 0;
    std::string codec;
    std::string container;
    int track = -1;
    std::uint32_t preroll = 0;
    double start_offset = 0.0;
    std::vector<std::pair<std::string, std::string>> tags;
};

bool container_audio_codec_supported(const std::string& codec) noexcept;

class ContainerAudioReader {
public:
    struct Impl;

    ContainerAudioReader();
    ~ContainerAudioReader();
    ContainerAudioReader(ContainerAudioReader&&) noexcept;
    ContainerAudioReader& operator=(ContainerAudioReader&&) noexcept;
    ContainerAudioReader(const ContainerAudioReader&) = delete;
    ContainerAudioReader& operator=(const ContainerAudioReader&) = delete;

    bool open(const std::string& path, std::string* error = nullptr, int track = -1);
    bool open(const void* data, std::size_t size, std::string* error = nullptr, int track = -1);
    bool open_shared(std::shared_ptr<const std::vector<std::uint8_t>> data, std::string* error = nullptr, int track = -1);
    void close();
    bool is_open() const noexcept;

    const ContainerAudioInfo& info() const noexcept { return m_info; }
    std::size_t read(float* interleaved, std::size_t frames);
    std::size_t read(AudioBuffer& out, std::size_t frames);
    bool seek(std::uint64_t frame);
    bool seek_seconds(double seconds);
    std::uint64_t position() const noexcept;
    std::uint64_t decode_errors() const noexcept;
    const std::string& last_error() const noexcept;

private:
    std::unique_ptr<Impl> m_impl;
    ContainerAudioInfo m_info;
};

} // namespace audio
} // namespace fizmo

#endif // FIZMO_AUDIO_CONTAINER_HPP
