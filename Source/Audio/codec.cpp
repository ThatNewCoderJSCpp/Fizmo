#include "codec.hpp"
#include "aac.hpp"
#include "container.hpp"
#include "file_io.hpp"
#include "flac.hpp"
#include "mp3.hpp"
#include "mp3_encoder.hpp"
#include "opus.hpp"
#include "vorbis.hpp"
#include "wav.hpp"

#include "../Video/demux.hpp"

#include <algorithm>
#include <cctype>
#include <utility>

namespace fizmo {
namespace audio {

namespace {

std::string lower(std::string s) {
    for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

std::string extension_of(const std::string& path) {
    const std::size_t slash = path.find_last_of("/\\");
    const std::size_t dot = path.find_last_of('.');
    if (dot == std::string::npos || (slash != std::string::npos && dot < slash)) return std::string();
    return lower(path.substr(dot + 1));
}

template <typename Info>
void fill_common(AudioFileInfo& out, const Info& in, AudioFileType type) {
    out.type = type;
    out.sample_rate = in.sample_rate;
    out.channels = static_cast<std::uint16_t>(in.channels);
    out.frames = in.frames;
    out.duration = in.sample_rate ? static_cast<double>(in.frames) / static_cast<double>(in.sample_rate) : 0.0;
    out.tags = in.tags;
}

void fill(AudioFileInfo& out, const WavInfo& in) {
    fill_common(out, in, AudioFileType::wav);
    out.bitrate_kbps = static_cast<std::uint32_t>(static_cast<std::uint64_t>(in.sample_rate) * in.channels * bits_per_sample(in.format) / 1000);
}

void fill(AudioFileInfo& out, const FlacInfo& in) {
    fill_common(out, in, AudioFileType::flac);
}

void fill(AudioFileInfo& out, const Mp3Info& in) {
    fill_common(out, in, AudioFileType::mp3);
    out.bitrate_kbps = in.bitrate_kbps;
}

void fill(AudioFileInfo& out, const VorbisInfo& in) {
    fill_common(out, in, AudioFileType::ogg_vorbis);
    out.bitrate_kbps = in.bitrate_nominal / 1000;
}

void fill(AudioFileInfo& out, const OpusInfo& in) {
    fill_common(out, in, AudioFileType::ogg_opus);
}

void fill(AudioFileInfo& out, const AacInfo& in) {
    out.type = AudioFileType::aac;
    out.sample_rate = in.sample_rate;
    out.channels = in.channels;
    out.frames = in.frames;
    out.duration = in.sample_rate ? static_cast<double>(in.frames) / static_cast<double>(in.sample_rate) : 0.0;
    out.bitrate_kbps = in.bitrate_kbps;
    out.tags.clear();
}

AudioFileType container_type(const std::string& name) noexcept {
    if (name == "matroska" || name == "webm") return AudioFileType::matroska;
    if (name == "ogg") return AudioFileType::ogg;
    return AudioFileType::mp4;
}

void fill(AudioFileInfo& out, const ContainerAudioInfo& in) {
    out.type = container_type(in.container);
    out.sample_rate = in.sample_rate;
    out.channels = in.channels;
    out.frames = in.frames;
    out.duration = in.duration;
    out.bitrate_kbps = in.bitrate_kbps;
    out.tags = in.tags;
}

std::uint64_t id3_length(const std::uint8_t* p, std::size_t n) noexcept {
    if (n < 10 || p[0] != 'I' || p[1] != 'D' || p[2] != '3') return 0;
    if (p[3] == 0xFF || p[4] == 0xFF) return 0;
    if ((p[6] | p[7] | p[8] | p[9]) & 0x80) return 0;
    const std::uint64_t size = (static_cast<std::uint64_t>(p[6]) << 21) | (static_cast<std::uint64_t>(p[7]) << 14) | (static_cast<std::uint64_t>(p[8]) << 7) | p[9];
    return 10 + size + ((p[5] & 0x10) ? 10 : 0);
}

AudioFileType detect_bare(const void* data, std::size_t size) noexcept {
    if (is_wav(data, size)) return AudioFileType::wav;
    if (is_flac(data, size)) return AudioFileType::flac;
    if (is_ogg_vorbis(data, size)) return AudioFileType::ogg_vorbis;
    if (is_ogg_opus(data, size)) return AudioFileType::ogg_opus;
    if (video::is_ogg_container(data, size)) return AudioFileType::ogg;
    if (video::is_matroska(data, size)) return AudioFileType::matroska;
    if (video::is_mp4(data, size)) return AudioFileType::mp4;
    return AudioFileType::unknown;
}

AudioFileType detect_after_id3(const std::uint8_t* p, std::size_t size) noexcept {
    if (is_flac(p, size)) return AudioFileType::flac;
    if (is_aac_adts(p, size)) return AudioFileType::aac;
    if (is_mp3(p, size)) return AudioFileType::mp3;
    return AudioFileType::unknown;
}

} // namespace

struct AudioReader::Impl {
    virtual ~Impl() = default;
    virtual std::size_t read(float* interleaved, std::size_t frames) = 0;
    virtual bool seek(std::uint64_t frame) = 0;
    virtual std::uint64_t position() const noexcept = 0;
};

namespace {

template <typename Reader>
struct ReaderImpl : AudioReader::Impl {
    Reader reader;
    std::size_t read(float* interleaved, std::size_t frames) override { return reader.read(interleaved, frames); }
    bool seek(std::uint64_t frame) override { return reader.seek(frame); }
    std::uint64_t position() const noexcept override { return reader.position(); }
};

template <typename Reader, typename Source>
std::unique_ptr<AudioReader::Impl> open_with(const Source& open, AudioFileInfo& info, std::string* error) {
    auto impl = std::make_unique<ReaderImpl<Reader>>();
    if (!open(impl->reader, error)) return nullptr;
    fill(info, impl->reader.info());
    return impl;
}

template <typename Opener>
std::unique_ptr<AudioReader::Impl> open_typed(AudioFileType type, const Opener& open, AudioFileInfo& info, std::string* error) {
    switch (type) {
        case AudioFileType::wav: return open_with<WavReader>(open, info, error);
        case AudioFileType::flac: return open_with<FlacReader>(open, info, error);
        case AudioFileType::mp3: return open_with<Mp3Reader>(open, info, error);
        case AudioFileType::ogg_vorbis: return open_with<VorbisReader>(open, info, error);
        case AudioFileType::ogg_opus: return open_with<OpusReader>(open, info, error);
        case AudioFileType::aac: return open_with<AacReader>(open, info, error);
        case AudioFileType::mp4:
        case AudioFileType::matroska:
        case AudioFileType::ogg:
            return open_with<ContainerAudioReader>(open, info, error);
        default: break;
    }
    if (error) *error = "not an audio format fizmo can read";
    return nullptr;
}

} // namespace

const char* audio_type_name(AudioFileType type) noexcept {
    switch (type) {
        case AudioFileType::wav: return "WAV";
        case AudioFileType::flac: return "FLAC";
        case AudioFileType::mp3: return "MP3";
        case AudioFileType::ogg_vorbis: return "Ogg Vorbis";
        case AudioFileType::ogg_opus: return "Ogg Opus";
        case AudioFileType::aac: return "AAC";
        case AudioFileType::mp4: return "MP4";
        case AudioFileType::matroska: return "Matroska";
        case AudioFileType::ogg: return "Ogg";
        default: return "unknown";
    }
}

AudioFileType detect_audio_type(const void* data, std::size_t size) noexcept {
    if (!data) return AudioFileType::unknown;
    const AudioFileType bare = detect_bare(data, size);
    if (bare != AudioFileType::unknown) return bare;
    const std::uint8_t* p = static_cast<const std::uint8_t*>(data);
    std::size_t begin = 0;
    while (begin < size) {
        const std::uint64_t len = id3_length(p + begin, size - begin);
        if (len == 0) break;
        if (len >= size - begin) return is_mp3(data, size) ? AudioFileType::mp3 : AudioFileType::unknown;
        begin += static_cast<std::size_t>(len);
    }
    return detect_after_id3(p + begin, size - begin);
}

AudioFileType audio_type_from_extension(const std::string& path) noexcept {
    const std::string e = extension_of(path);
    if (e == "wav" || e == "wave" || e == "bwf" || e == "rf64") return AudioFileType::wav;
    if (e == "flac" || e == "fla") return AudioFileType::flac;
    if (e == "mp3" || e == "mp2" || e == "mp1" || e == "mpga") return AudioFileType::mp3;
    if (e == "ogg" || e == "oga" || e == "ogv") return AudioFileType::ogg_vorbis;
    if (e == "opus") return AudioFileType::ogg_opus;
    if (e == "aac" || e == "adts") return AudioFileType::aac;
    if (e == "m4a" || e == "m4b" || e == "m4r" || e == "mp4" || e == "m4v" || e == "mov" || e == "3gp" || e == "3g2") return AudioFileType::mp4;
    if (e == "mka" || e == "mkv" || e == "webm" || e == "weba") return AudioFileType::matroska;
    return AudioFileType::unknown;
}

AudioFileType detect_audio_file(const std::string& path) noexcept {
    try {
        detail::File f;
        if (f.open(path, "rb")) {
            std::vector<std::uint8_t> head(65536);
            head.resize(f.read(head.data(), head.size()));
            AudioFileType t = detect_bare(head.data(), head.size());
            if (t != AudioFileType::unknown) return t;
            std::uint64_t begin = 0;
            for (int guard = 0; guard < 8; ++guard) {
                std::uint8_t tag[10];
                if (!f.seek(static_cast<std::int64_t>(begin)) || f.read(tag, 10) != 10) break;
                const std::uint64_t len = id3_length(tag, 10);
                if (len == 0) break;
                begin += len;
            }
            if (begin > 0 && f.seek(static_cast<std::int64_t>(begin))) {
                head.assign(65536, 0);
                head.resize(f.read(head.data(), head.size()));
            }
            t = detect_after_id3(head.data(), head.size());
            if (t != AudioFileType::unknown) return t;
            if (begin > 0) {
                const AudioFileType e = audio_type_from_extension(path);
                return e == AudioFileType::aac ? e : AudioFileType::mp3;
            }
        }
    } catch (...) {
    }
    return audio_type_from_extension(path);
}

std::vector<std::string> readable_audio_extensions() {
    return { "wav", "wave", "flac", "mp3", "mp2", "mp1", "ogg", "oga", "opus", "aac", "adts", "m4a", "m4b", "m4r", "mp4", "m4v", "mov", "3gp", "3g2", "mka", "mkv", "webm", "weba", "ogv" };
}

std::vector<std::string> writable_audio_extensions() {
    return { "wav", "flac", "mp3" };
}

std::string AudioFileInfo::tag(const std::string& key) const {
    const std::string k = lower(key);
    for (const auto& t : tags) {
        if (lower(t.first) == k) return t.second;
    }
    if (k == "track") return tag("tracknumber");
    return std::string();
}

bool load_audio(const std::string& path, AudioBuffer& out, std::string* error, AudioFileInfo* info) {
    AudioReader r;
    if (!r.open(path, error)) return false;
    out = AudioBuffer(r.info().channels, r.info().sample_rate);
    if (r.info().frames) out.reserve(static_cast<std::size_t>(std::min<std::uint64_t>(r.info().frames, 1ull << 31)));
    const std::size_t chunk = 65536;
    while (r.read(out, chunk) == chunk) {
    }
    if (info) {
        *info = r.info();
        info->frames = out.frames();
        info->duration = out.duration();
    }
    return true;
}

bool load_audio(const void* data, std::size_t size, AudioBuffer& out, std::string* error, AudioFileInfo* info) {
    AudioReader r;
    if (!r.open(data, size, error)) return false;
    out = AudioBuffer(r.info().channels, r.info().sample_rate);
    const std::size_t chunk = 65536;
    while (r.read(out, chunk) == chunk) {
    }
    if (info) {
        *info = r.info();
        info->frames = out.frames();
        info->duration = out.duration();
    }
    return true;
}

bool read_audio_info(const std::string& path, AudioFileInfo& info, std::string* error) {
    AudioReader r;
    if (!r.open(path, error)) return false;
    info = r.info();
    return true;
}

namespace {

AudioFileType save_type(const std::string& path, const SaveOptions& options) {
    return options.type != AudioFileType::unknown ? options.type : audio_type_from_extension(path);
}

FlacEncodeOptions flac_options(const SaveOptions& o) {
    FlacEncodeOptions f;
    f.compression = o.flac_compression;
    f.bits_per_sample = o.flac_bits;
    f.dither = o.dither;
    f.tags = o.tags;
    return f;
}

Mp3EncodeOptions mp3_options(const SaveOptions& o) {
    Mp3EncodeOptions m;
    m.bitrate_kbps = o.mp3_bitrate_kbps;
    m.vbr = o.mp3_vbr;
    m.vbr_quality = o.mp3_vbr_quality;
    m.quality = o.mp3_quality;
    m.tags = o.tags;
    return m;
}

} // namespace

bool save_audio(const std::string& path, const AudioBuffer& buffer, const SaveOptions& options, std::string* error) {
    switch (save_type(path, options)) {
        case AudioFileType::wav: return write_wav(path, buffer, options.wav_format, error, options.dither);
        case AudioFileType::flac: return write_flac(path, buffer, flac_options(options), error);
        case AudioFileType::mp3: return write_mp3(path, buffer, mp3_options(options), error);
        case AudioFileType::ogg_vorbis:
            if (error) *error = "fizmo can read Ogg Vorbis but can't write it yet; save as WAV, FLAC or MP3";
            return false;
        default:
            if (error) *error = "can't tell which format to save '" + path + "' as; use .wav, .flac or .mp3";
            return false;
    }
}

std::vector<std::uint8_t> encode_audio(const AudioBuffer& buffer, AudioFileType type, const SaveOptions& options, std::string* error) {
    switch (type) {
        case AudioFileType::wav: return encode_wav(buffer, options.wav_format, options.dither);
        case AudioFileType::flac: return encode_flac(buffer, flac_options(options), error);
        case AudioFileType::mp3: return encode_mp3(buffer, mp3_options(options));
        default:
            if (error) *error = "fizmo can't encode that format";
            return {};
    }
}

AudioReader::AudioReader() = default;
AudioReader::~AudioReader() = default;
AudioReader::AudioReader(AudioReader&& other) noexcept = default;
AudioReader& AudioReader::operator=(AudioReader&& other) noexcept = default;

bool AudioReader::open(const std::string& path, std::string* error) {
    close();
    const AudioFileType type = detect_audio_file(path);
    auto opener = [&](auto& reader, std::string* err) { return reader.open(path, err); };
    m_impl = open_typed(type, opener, m_info, error);
    if (!m_impl) m_info = AudioFileInfo();
    return m_impl != nullptr;
}

namespace {

struct SourceImpl : AudioReader::Impl {
    std::unique_ptr<AudioSource> source;
    std::size_t read(float* interleaved, std::size_t frames) override { return source->read(interleaved, frames); }
    bool seek(std::uint64_t frame) override { return source->seek(frame); }
    std::uint64_t position() const noexcept override { return source->position(); }
};

} // namespace

bool AudioReader::open_track(const std::string& path, int track, std::string* error) {
    close();
    auto impl = std::make_unique<ReaderImpl<ContainerAudioReader>>();
    if (!impl->reader.open(path, error, track)) return false;
    fill(m_info, impl->reader.info());
    m_impl = std::move(impl);
    return true;
}

bool AudioReader::open_track(const void* data, std::size_t size, int track, std::string* error) {
    close();
    auto impl = std::make_unique<ReaderImpl<ContainerAudioReader>>();
    if (!impl->reader.open(data, size, error, track)) return false;
    fill(m_info, impl->reader.info());
    m_impl = std::move(impl);
    return true;
}

bool AudioReader::open_source(std::unique_ptr<AudioSource> source, const AudioFileInfo& info, std::string* error) {
    close();
    if (!source) {
        if (error) *error = "no audio source";
        return false;
    }
    if (info.sample_rate == 0 || info.channels == 0) {
        if (error) *error = "the audio source has no sample rate or channel count";
        return false;
    }
    auto impl = std::make_unique<SourceImpl>();
    impl->source = std::move(source);
    m_info = info;
    m_impl = std::move(impl);
    return true;
}

bool AudioReader::open(const void* data, std::size_t size, std::string* error) {
    close();
    const AudioFileType type = detect_audio_type(data, size);
    auto opener = [&](auto& reader, std::string* err) { return reader.open(data, size, err); };
    m_impl = open_typed(type, opener, m_info, error);
    if (!m_impl) m_info = AudioFileInfo();
    return m_impl != nullptr;
}

void AudioReader::close() {
    m_impl.reset();
    m_info = AudioFileInfo();
}

bool AudioReader::is_open() const noexcept {
    return m_impl != nullptr;
}

std::size_t AudioReader::read(float* interleaved, std::size_t frames) {
    return m_impl ? m_impl->read(interleaved, frames) : 0;
}

std::size_t AudioReader::read(AudioBuffer& out, std::size_t frames) {
    if (!m_impl) return 0;
    if (out.channels() != m_info.channels || out.sample_rate() != m_info.sample_rate) out = AudioBuffer(m_info.channels, m_info.sample_rate);
    const std::size_t start = out.frames();
    out.resize(start + frames);
    const std::size_t got = m_impl->read(out.frame(start), frames);
    out.resize(start + got);
    return got;
}

bool AudioReader::seek(std::uint64_t frame) {
    return m_impl && m_impl->seek(frame);
}

bool AudioReader::seek_seconds(double seconds) {
    if (seconds < 0.0) seconds = 0.0;
    return seek(static_cast<std::uint64_t>(seconds * static_cast<double>(m_info.sample_rate) + 0.5));
}

std::uint64_t AudioReader::position() const noexcept {
    return m_impl ? m_impl->position() : 0;
}

} // namespace audio
} // namespace fizmo
