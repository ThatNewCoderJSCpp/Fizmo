#ifndef FIZMO_AUDIO_CODEC_HPP
#define FIZMO_AUDIO_CODEC_HPP

#include "buffer.hpp"
#include "format.hpp"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace fizmo {
namespace audio {

enum class AudioFileType {
    unknown,
    wav,
    flac,
    mp3,
    ogg_vorbis,
    ogg_opus,
    aac,
    mp4,
    matroska,
    ogg
};

const char* audio_type_name(AudioFileType type) noexcept;
AudioFileType detect_audio_type(const void* data, std::size_t size) noexcept;
AudioFileType audio_type_from_extension(const std::string& path) noexcept;
AudioFileType detect_audio_file(const std::string& path) noexcept;
std::vector<std::string> readable_audio_extensions();
std::vector<std::string> writable_audio_extensions();

struct AudioFileInfo {
    AudioFileType type = AudioFileType::unknown;
    std::uint32_t sample_rate = 0;
    std::uint16_t channels = 0;
    std::uint64_t frames = 0;
    double duration = 0.0;
    std::uint32_t bitrate_kbps = 0;
    std::vector<std::pair<std::string, std::string>> tags;

    std::string tag(const std::string& key) const;
};

bool load_audio(const std::string& path, AudioBuffer& out, std::string* error = nullptr, AudioFileInfo* info = nullptr);
bool load_audio(const void* data, std::size_t size, AudioBuffer& out, std::string* error = nullptr, AudioFileInfo* info = nullptr);
bool read_audio_info(const std::string& path, AudioFileInfo& info, std::string* error = nullptr);

struct SaveOptions {
    AudioFileType type = AudioFileType::unknown;
    SampleFormat wav_format = SampleFormat::s16;
    bool dither = true;
    int flac_compression = 5;
    unsigned flac_bits = 16;
    std::uint32_t mp3_bitrate_kbps = 192;
    bool mp3_vbr = false;
    int mp3_vbr_quality = 2;
    int mp3_quality = 2;
    std::vector<std::pair<std::string, std::string>> tags;
};

bool save_audio(const std::string& path, const AudioBuffer& buffer, const SaveOptions& options = SaveOptions(), std::string* error = nullptr);
std::vector<std::uint8_t> encode_audio(const AudioBuffer& buffer, AudioFileType type, const SaveOptions& options = SaveOptions(), std::string* error = nullptr);

class AudioSource {
public:
    virtual ~AudioSource() = default;
    virtual std::size_t read(float* interleaved, std::size_t frames) = 0;
    virtual bool seek(std::uint64_t frame) = 0;
    virtual std::uint64_t position() const noexcept = 0;
};

class AudioReader {
public:
    AudioReader();
    ~AudioReader();
    AudioReader(AudioReader&& other) noexcept;
    AudioReader& operator=(AudioReader&& other) noexcept;
    AudioReader(const AudioReader&) = delete;
    AudioReader& operator=(const AudioReader&) = delete;

    bool open(const std::string& path, std::string* error = nullptr);
    bool open(const void* data, std::size_t size, std::string* error = nullptr);
    bool open_track(const std::string& path, int track, std::string* error = nullptr);
    bool open_track(const void* data, std::size_t size, int track, std::string* error = nullptr);
    bool open_source(std::unique_ptr<AudioSource> source, const AudioFileInfo& info, std::string* error = nullptr);
    void close();
    bool is_open() const noexcept;

    const AudioFileInfo& info() const noexcept { return m_info; }
    std::size_t read(float* interleaved, std::size_t frames);
    std::size_t read(AudioBuffer& out, std::size_t frames);
    bool seek(std::uint64_t frame);
    bool seek_seconds(double seconds);
    std::uint64_t position() const noexcept;

    struct Impl;

private:
    std::unique_ptr<Impl> m_impl;
    AudioFileInfo m_info;
};

} // namespace audio
} // namespace fizmo

#endif // FIZMO_AUDIO_CODEC_HPP
