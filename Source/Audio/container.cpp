#include "container.hpp"

#include "aac.hpp"
#include "flac.hpp"
#include "format.hpp"
#include "mp3.hpp"
#include "opus.hpp"
#include "vorbis.hpp"

#include "../Video/demux.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <new>
#include <utility>

namespace fizmo {
namespace audio {

namespace {

enum class Codec {
    none,
    aac,
    opus,
    vorbis,
    mp3,
    flac,
    pcm,
    alaw,
    mulaw
};

struct PcmFormat {
    int bits = 16;
    bool is_float = false;
    bool little = true;
    bool is_signed = true;
};

bool parse_pcm(const std::string& codec, PcmFormat& f) noexcept {
    if (codec.size() < 6 || codec.compare(0, 4, "pcm_") != 0) return false;
    if (codec == "pcm_u8") {
        f.bits = 8;
        f.is_signed = false;
        return true;
    }
    if (codec == "pcm_s8") {
        f.bits = 8;
        return true;
    }
    const char kind = codec[4];
    if (kind != 's' && kind != 'f') return false;
    f.is_float = kind == 'f';
    std::size_t i = 5;
    int bits = 0;
    while (i < codec.size() && codec[i] >= '0' && codec[i] <= '9') {
        bits = bits * 10 + (codec[i] - '0');
        ++i;
    }
    const std::string end = codec.substr(i);
    if (end != "le" && end != "be") return false;
    f.little = end == "le";
    f.bits = bits;
    if (f.is_float) return bits == 32 || bits == 64;
    return bits == 16 || bits == 24 || bits == 32;
}

Codec codec_kind(const std::string& codec, PcmFormat* pcm = nullptr) noexcept {
    if (codec == "aac") return Codec::aac;
    if (codec == "opus") return Codec::opus;
    if (codec == "vorbis") return Codec::vorbis;
    if (codec == "mp3" || codec == "mp2" || codec == "mp1") return Codec::mp3;
    if (codec == "flac") return Codec::flac;
    if (codec == "pcm_alaw") return Codec::alaw;
    if (codec == "pcm_mulaw") return Codec::mulaw;
    PcmFormat f;
    if (parse_pcm(codec, f)) {
        if (pcm) *pcm = f;
        return Codec::pcm;
    }
    return Codec::none;
}

float alaw_sample(std::uint8_t v) noexcept {
    v ^= 0x55;
    int t = (v & 0x0F) << 4;
    const int seg = (v & 0x70) >> 4;
    if (seg == 0) {
        t += 8;
    } else if (seg == 1) {
        t += 0x108;
    } else {
        t += 0x108;
        t <<= seg - 1;
    }
    return static_cast<float>((v & 0x80) ? t : -t) / 32768.0f;
}

float mulaw_sample(std::uint8_t v) noexcept {
    v = static_cast<std::uint8_t>(~v);
    int t = ((v & 0x0F) << 3) + 0x84;
    t <<= (v & 0x70) >> 4;
    return static_cast<float>((v & 0x80) ? (0x84 - t) : (t - 0x84)) / 32768.0f;
}

std::int64_t round_frames(double seconds, std::uint32_t rate) noexcept {
    const double v = seconds * static_cast<double>(rate);
    if (!(v == v)) return 0;
    if (v > 9.0e15) return static_cast<std::int64_t>(9.0e15);
    if (v < -9.0e15) return -static_cast<std::int64_t>(9.0e15);
    return static_cast<std::int64_t>(std::llround(v));
}

} // namespace

bool container_audio_codec_supported(const std::string& codec) noexcept {
    return codec_kind(codec) != Codec::none;
}

struct ContainerAudioReader::Impl {
    video::Demuxer demux;
    int track = -1;
    Codec codec = Codec::none;
    PcmFormat pcm;
    std::uint16_t source_channels = 0;
    AacDecoder aac;
    OpusDecoder opus;
    VorbisDecoder vorbis;
    Mp3Decoder mp3;
    FlacDecoder flac;
    std::vector<std::uint8_t> config;
    std::uint32_t rate = 0;
    std::size_t channels = 0;
    std::int64_t total = -1;
    std::int64_t shift = 0;
    std::int64_t base = 0;
    std::int64_t grid = 0;
    std::int64_t margin = 0;
    double end_pts = 0.0;
    bool has_end_pts = false;
    std::uint32_t preroll = 0;
    std::vector<float> chunk;
    std::vector<float> decoded;
    std::int64_t chunk_start = 0;
    std::size_t chunk_frames = 0;
    std::int64_t next_expected = 0;
    bool anchored = false;
    std::int64_t position = 0;
    bool ended = false;
    std::uint64_t errors = 0;
    std::string last_error;
    video::Packet packet;

    bool open_decoder(const video::TrackInfo& t, std::string* error) {
        switch (codec) {
            case Codec::aac:
                if (t.codec_config.empty()) return aac.open_adts();
                return aac.open(t.codec_config.data(), t.codec_config.size(), error);
            case Codec::opus:
                if (t.codec_config.empty()) return opus.open(t.channels ? t.channels : 2, error);
                return opus.open_head(t.codec_config.data(), t.codec_config.size(), error);
            case Codec::vorbis:
                return vorbis.open(t.codec_config.data(), t.codec_config.size(), error);
            case Codec::mp3:
                return mp3.open();
            case Codec::flac:
                return flac.open(t.codec_config.data(), t.codec_config.size(), error);
            case Codec::pcm:
            case Codec::alaw:
            case Codec::mulaw:
                if (t.channels == 0 || t.sample_rate == 0) {
                    if (error) *error = "the PCM track has no format";
                    return false;
                }
                source_channels = t.channels;
                return true;
            default:
                break;
        }
        if (error) *error = "unsupported audio codec " + t.codec;
        return false;
    }

    void reset_decoder() {
        switch (codec) {
            case Codec::aac: aac.reset(); break;
            case Codec::opus: opus.reset(); break;
            case Codec::vorbis: vorbis.reset(); break;
            case Codec::mp3: mp3.reset(); break;
            case Codec::flac: flac.reset(); break;
            default: break;
        }
    }

    std::size_t decode_pcm(const std::uint8_t* data, std::size_t size) {
        const std::size_t ch = source_channels;
        std::size_t bytes = 1;
        if (codec == Codec::pcm) bytes = static_cast<std::size_t>(pcm.bits / 8);
        const std::size_t n = size / (bytes * ch);
        decoded.resize(n * ch);
        for (std::size_t i = 0; i < n * ch; ++i) {
            const std::uint8_t* p = data + i * bytes;
            float v = 0.0f;
            if (codec == Codec::alaw) {
                v = alaw_sample(p[0]);
            } else if (codec == Codec::mulaw) {
                v = mulaw_sample(p[0]);
            } else if (pcm.bits == 8) {
                v = pcm.is_signed ? static_cast<float>(static_cast<std::int8_t>(p[0])) / 128.0f : (static_cast<float>(p[0]) - 128.0f) / 128.0f;
            } else {
                std::uint64_t raw = 0;
                for (std::size_t k = 0; k < bytes; ++k) {
                    const std::size_t at = pcm.little ? bytes - 1 - k : k;
                    raw = (raw << 8) | p[at];
                }
                if (pcm.is_float) {
                    if (bytes == 4) {
                        const std::uint32_t r32 = static_cast<std::uint32_t>(raw);
                        float f;
                        std::memcpy(&f, &r32, 4);
                        v = f;
                    } else {
                        double d;
                        std::memcpy(&d, &raw, 8);
                        v = static_cast<float>(d);
                    }
                } else {
                    const unsigned bits = static_cast<unsigned>(pcm.bits);
                    std::int64_t s = static_cast<std::int64_t>(raw);
                    if (raw & (std::uint64_t(1) << (bits - 1))) s -= static_cast<std::int64_t>(std::uint64_t(1) << bits);
                    v = static_cast<float>(static_cast<double>(s) / static_cast<double>(std::uint64_t(1) << (bits - 1)));
                }
            }
            decoded[i] = v;
        }
        return n;
    }

    std::size_t decode_packet(const video::Packet& p, std::vector<float>& out) {
        std::string err;
        std::size_t n = 0;
        try {
            switch (codec) {
                case Codec::aac:
                    decoded.clear();
                    n = aac.decode(p.data.data(), p.data.size(), decoded, &err);
                    break;
                case Codec::opus: n = opus.decode(p.data.data(), p.data.size(), decoded, &err); break;
                case Codec::vorbis: n = vorbis.decode(p.data.data(), p.data.size(), decoded, &err); break;
                case Codec::mp3: n = mp3.decode(p.data.data(), p.data.size(), decoded, &err); break;
                case Codec::flac: n = flac.decode(p.data.data(), p.data.size(), decoded, &err); break;
                case Codec::pcm:
                case Codec::alaw:
                case Codec::mulaw:
                    n = decode_pcm(p.data.data(), p.data.size());
                    break;
                default: break;
            }
        } catch (const std::bad_alloc&) {
            err = "out of memory";
            n = 0;
        }
        if (n == 0) {
            if (!err.empty()) {
                ++errors;
                last_error = err;
            }
            return 0;
        }
        const std::size_t have = decoded.size() / n;
        if (have == 0) return 0;
        if (channels == 0) channels = have;
        if (have == channels) {
            out.swap(decoded);
            out.resize(n * channels);
        } else {
            out.resize(n * channels);
            remix(decoded.data(), have, out.data(), channels, n);
        }
        return n;
    }

    bool next_chunk() {
        while (true) {
            if (!demux.read(packet)) return false;
            if (static_cast<int>(packet.track) != track || packet.data.empty()) continue;
            if (has_end_pts && packet.pts >= end_pts - 1e-9) return false;
            std::size_t n = decode_packet(packet, chunk);
            if (n > 0 && packet.discard_padding > 0.0) {
                const std::int64_t cut = round_frames(packet.discard_padding, rate);
                n = cut >= static_cast<std::int64_t>(n) ? 0 : n - static_cast<std::size_t>(cut);
            }
            if (n == 0) continue;
            std::int64_t start = next_expected;
            if (!anchored) {
                start = round_frames(packet.pts, rate) - shift;
                if (grid > 0) {
                    const double steps = std::floor(static_cast<double>(start - base) / static_cast<double>(grid) + 0.5);
                    start = base + static_cast<std::int64_t>(steps) * grid;
                }
                anchored = true;
            }
            next_expected = start + static_cast<std::int64_t>(n);
            chunk_start = start;
            chunk_frames = n;
            if (next_expected <= position) continue;
            return true;
        }
    }

    std::size_t read(float* out, std::size_t frames) {
        std::size_t done = 0;
        const std::size_t ch = channels;
        while (done < frames) {
            if (total >= 0 && position >= total) break;
            std::size_t want = frames - done;
            if (total >= 0) want = static_cast<std::size_t>(std::min<std::int64_t>(static_cast<std::int64_t>(want), total - position));
            const std::int64_t chunk_end = chunk_start + static_cast<std::int64_t>(chunk_frames);
            if (chunk_frames && position >= chunk_start && position < chunk_end) {
                const std::size_t off = static_cast<std::size_t>(position - chunk_start);
                const std::size_t n = std::min(want, static_cast<std::size_t>(chunk_end - position));
                std::memcpy(out + done * ch, chunk.data() + off * ch, n * ch * sizeof(float));
                done += n;
                position += static_cast<std::int64_t>(n);
                continue;
            }
            if (chunk_frames && position < chunk_start) {
                const std::size_t n = std::min(want, static_cast<std::size_t>(chunk_start - position));
                std::fill(out + done * ch, out + (done + n) * ch, 0.0f);
                done += n;
                position += static_cast<std::int64_t>(n);
                continue;
            }
            if (ended) break;
            if (!next_chunk()) {
                ended = true;
                chunk_frames = 0;
                break;
            }
        }
        return done;
    }

    bool seek(std::int64_t frame) {
        if (frame < 0) frame = 0;
        if (total >= 0 && frame > total) frame = total;
        position = frame;
        chunk_frames = 0;
        anchored = false;
        ended = false;
        const std::int64_t pre = frame + shift - static_cast<std::int64_t>(preroll) - margin;
        bool ok;
        if (pre <= 0 || frame == 0) {
            ok = demux.rewind();
        } else {
            ok = demux.seek(static_cast<double>(pre) / static_cast<double>(rate), true);
        }
        reset_decoder();
        return ok;
    }
};

ContainerAudioReader::ContainerAudioReader() = default;
ContainerAudioReader::~ContainerAudioReader() = default;
ContainerAudioReader::ContainerAudioReader(ContainerAudioReader&&) noexcept = default;
ContainerAudioReader& ContainerAudioReader::operator=(ContainerAudioReader&&) noexcept = default;

namespace {

bool finish_open(std::unique_ptr<ContainerAudioReader::Impl>& impl, ContainerAudioInfo& info, int track, std::string* error) {
    ContainerAudioReader::Impl& m = *impl;
    const std::vector<video::TrackInfo>& tracks = m.demux.tracks();
    if (track >= 0) {
        if (static_cast<std::size_t>(track) >= tracks.size() || tracks[static_cast<std::size_t>(track)].type != video::MediaType::audio) {
            if (error) *error = "track " + std::to_string(track) + " is not an audio track";
            return false;
        }
    } else {
        const int best = m.demux.best_track(video::MediaType::audio);
        if (best >= 0 && container_audio_codec_supported(tracks[static_cast<std::size_t>(best)].codec)) {
            track = best;
        } else {
            for (std::size_t i = 0; i < tracks.size(); ++i) {
                if (tracks[i].type == video::MediaType::audio && container_audio_codec_supported(tracks[i].codec)) {
                    track = static_cast<int>(i);
                    break;
                }
            }
        }
        if (track < 0) {
            if (error) *error = best >= 0 ? "unsupported audio codec " + tracks[static_cast<std::size_t>(best)].codec : "the file has no audio track";
            return false;
        }
    }
    const video::TrackInfo& t = tracks[static_cast<std::size_t>(track)];
    m.track = track;
    m.codec = codec_kind(t.codec, &m.pcm);
    if (m.codec == Codec::none) {
        if (error) *error = "unsupported audio codec " + (t.codec.empty() ? std::string("(unknown)") : t.codec);
        return false;
    }
    for (std::size_t i = 0; i < tracks.size(); ++i) m.demux.select(static_cast<std::uint32_t>(i), static_cast<int>(i) == track);
    if (!m.open_decoder(t, error)) return false;
    std::size_t probe = 0;
    int tries = 0;
    double first_pts = 0.0;
    std::size_t probe_bytes = 0;
    while (probe == 0 && tries < 64 && m.demux.read(m.packet)) {
        if (static_cast<int>(m.packet.track) != track || m.packet.data.empty()) continue;
        ++tries;
        probe = m.decode_packet(m.packet, m.chunk);
        first_pts = m.packet.pts;
        probe_bytes = m.packet.data.size();
    }
    switch (m.codec) {
        case Codec::aac: m.rate = m.aac.config().output_rate ? m.aac.config().output_rate : t.sample_rate; break;
        case Codec::opus: m.rate = OpusDecoder::sample_rate; break;
        case Codec::vorbis: m.rate = m.vorbis.sample_rate(); break;
        case Codec::mp3: m.rate = m.mp3.sample_rate() ? m.mp3.sample_rate() : t.sample_rate; break;
        case Codec::flac: m.rate = m.flac.sample_rate(); break;
        default: m.rate = t.sample_rate; break;
    }
    if (probe == 0 || m.rate == 0 || m.channels == 0) {
        if (error) *error = m.last_error.empty() ? "no decodable audio in the " + t.codec + " track" : "can't decode the " + t.codec + " track: " + m.last_error;
        return false;
    }
    std::uint32_t frame_len = static_cast<std::uint32_t>(probe);
    switch (m.codec) {
        case Codec::aac: m.preroll = frame_len * (m.aac.config().sbr ? 2u : 1u); break;
        case Codec::opus: m.preroll = static_cast<std::uint32_t>(OpusReader::seek_preroll); break;
        case Codec::vorbis: m.preroll = 4096; break;
        case Codec::mp3: {
            const std::size_t reservoir = frame_len >= 1152 ? 511u : 255u;
            const std::size_t bytes = std::max<std::size_t>(1, probe_bytes);
            const std::size_t count = std::min<std::size_t>(64, (reservoir + bytes - 1) / bytes + 2);
            m.preroll = frame_len * static_cast<std::uint32_t>(count);
            break;
        }
        default: m.preroll = 0; break;
    }
    m.shift = std::max<std::int64_t>(0, round_frames(first_pts, m.rate));
    m.base = round_frames(first_pts, m.rate) - m.shift;
    const std::string format = m.demux.format_name();
    m.grid = 0;
    m.margin = 0;
    if (format == "matroska" || format == "webm") {
        m.margin = static_cast<std::int64_t>(m.rate / 100);
        switch (m.codec) {
            case Codec::aac:
            case Codec::mp3: m.grid = static_cast<std::int64_t>(probe); break;
            case Codec::opus: m.grid = 120; break;
            case Codec::flac: m.grid = static_cast<std::int64_t>(m.flac.fixed_block_size()); break;
            case Codec::pcm:
            case Codec::alaw:
            case Codec::mulaw: m.grid = probe >= 64 ? static_cast<std::int64_t>(probe) : 0; break;
            default: break;
        }
    }
    const double duration = t.duration > 0.0 ? t.duration : m.demux.duration();
    std::int64_t estimate = duration > 0.0 ? round_frames(duration, m.rate) - m.shift : -1;
    m.total = -1;
    m.has_end_pts = false;
    if (format == "ogg") {
        m.total = estimate;
    } else if (format != "matroska" && format != "webm" && t.duration > 0.0) {
        m.has_end_pts = true;
        m.end_pts = t.duration;
    }
    if (m.total < 0) m.total = -1;
    if (estimate < 0) estimate = 0;
    m.errors = 0;
    m.last_error.clear();
    m.seek(0);
    info = ContainerAudioInfo();
    info.sample_rate = m.rate;
    info.channels = static_cast<std::uint16_t>(m.channels);
    info.frames = static_cast<std::uint64_t>(m.total >= 0 ? m.total : estimate);
    info.duration = static_cast<double>(info.frames) / static_cast<double>(m.rate);
    info.start_offset = static_cast<double>(m.shift) / static_cast<double>(m.rate);
    info.codec = t.codec;
    info.container = m.demux.format_name();
    info.track = track;
    info.preroll = m.preroll;
    info.tags = m.demux.tags();
    if (t.bitrate) {
        info.bitrate_kbps = (t.bitrate + 500) / 1000;
    } else if (m.codec == Codec::pcm || m.codec == Codec::alaw || m.codec == Codec::mulaw) {
        const std::uint64_t bits = m.codec == Codec::pcm ? static_cast<std::uint64_t>(m.pcm.bits) : 8u;
        info.bitrate_kbps = static_cast<std::uint32_t>(static_cast<std::uint64_t>(t.sample_rate) * t.channels * bits / 1000u);
    }
    return true;
}

} // namespace

bool ContainerAudioReader::open(const std::string& path, std::string* error, int track) {
    close();
    std::unique_ptr<Impl> impl(new Impl());
    if (!impl->demux.open(path, error)) return false;
    if (!finish_open(impl, m_info, track, error)) {
        m_info = ContainerAudioInfo();
        return false;
    }
    m_impl = std::move(impl);
    return true;
}

bool ContainerAudioReader::open(const void* data, std::size_t size, std::string* error, int track) {
    close();
    std::unique_ptr<Impl> impl(new Impl());
    if (!impl->demux.open(data, size, error)) return false;
    if (!finish_open(impl, m_info, track, error)) {
        m_info = ContainerAudioInfo();
        return false;
    }
    m_impl = std::move(impl);
    return true;
}

bool ContainerAudioReader::open_shared(std::shared_ptr<const std::vector<std::uint8_t>> data, std::string* error, int track) {
    close();
    std::unique_ptr<Impl> impl(new Impl());
    if (!impl->demux.open_shared(std::move(data), error)) return false;
    if (!finish_open(impl, m_info, track, error)) {
        m_info = ContainerAudioInfo();
        return false;
    }
    m_impl = std::move(impl);
    return true;
}

void ContainerAudioReader::close() {
    m_impl.reset();
    m_info = ContainerAudioInfo();
}

bool ContainerAudioReader::is_open() const noexcept {
    return m_impl != nullptr;
}

std::size_t ContainerAudioReader::read(float* interleaved, std::size_t frames) {
    if (!m_impl || !interleaved || frames == 0) return 0;
    return m_impl->read(interleaved, frames);
}

std::size_t ContainerAudioReader::read(AudioBuffer& out, std::size_t frames) {
    if (!m_impl) return 0;
    if (out.channels() != m_info.channels || out.sample_rate() != m_info.sample_rate) out = AudioBuffer(m_info.channels, m_info.sample_rate);
    const std::size_t start = out.frames();
    out.resize(start + frames);
    const std::size_t got = read(out.frame(start), frames);
    out.resize(start + got);
    return got;
}

bool ContainerAudioReader::seek(std::uint64_t frame) {
    if (!m_impl) return false;
    const std::uint64_t limit = static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max() / 2);
    return m_impl->seek(static_cast<std::int64_t>(std::min(frame, limit)));
}

bool ContainerAudioReader::seek_seconds(double seconds) {
    if (!m_impl) return false;
    if (!(seconds > 0.0)) seconds = 0.0;
    return m_impl->seek(round_frames(seconds, m_impl->rate));
}

std::uint64_t ContainerAudioReader::position() const noexcept {
    return m_impl ? static_cast<std::uint64_t>(std::max<std::int64_t>(0, m_impl->position)) : 0;
}

std::uint64_t ContainerAudioReader::decode_errors() const noexcept {
    return m_impl ? m_impl->errors : 0;
}

const std::string& ContainerAudioReader::last_error() const noexcept {
    static const std::string none;
    return m_impl ? m_impl->last_error : none;
}

} // namespace audio
} // namespace fizmo
