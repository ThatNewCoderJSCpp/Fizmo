#include "player.hpp"

#include "../Audio/codec.hpp"
#include "../Audio/container.hpp"
#include "../Audio/mixer.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <cstring>
#include <deque>
#include <limits>
#include <mutex>
#include <thread>
#include <utility>

namespace fizmo {
namespace video {

namespace {

std::int64_t now_ns() noexcept {
    return static_cast<std::int64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count());
}

std::uint32_t scaled_width(const TrackInfo& t) noexcept {
    return t.display_width ? t.display_width : t.width;
}

std::uint32_t scaled_height(const TrackInfo& t) noexcept {
    return t.display_height ? t.display_height : t.height;
}

std::string plain_codec_name(audio::AudioFileType type) {
    switch (type) {
        case audio::AudioFileType::wav: return "pcm";
        case audio::AudioFileType::flac: return "flac";
        case audio::AudioFileType::mp3: return "mp3";
        case audio::AudioFileType::ogg_vorbis: return "vorbis";
        case audio::AudioFileType::ogg_opus: return "opus";
        case audio::AudioFileType::aac: return "aac";
        default: return "unknown";
    }
}

std::string plain_container_name(audio::AudioFileType type) {
    switch (type) {
        case audio::AudioFileType::wav: return "wav";
        case audio::AudioFileType::flac: return "flac";
        case audio::AudioFileType::mp3: return "mp3";
        case audio::AudioFileType::ogg_vorbis:
        case audio::AudioFileType::ogg_opus: return "ogg";
        case audio::AudioFileType::aac: return "aac";
        default: return "unknown";
    }
}

bool plain_reader_type(audio::AudioFileType type) noexcept {
    switch (type) {
        case audio::AudioFileType::wav:
        case audio::AudioFileType::flac:
        case audio::AudioFileType::mp3:
        case audio::AudioFileType::ogg_vorbis:
        case audio::AudioFileType::ogg_opus:
        case audio::AudioFileType::aac:
            return true;
        default:
            return false;
    }
}

void fill_from_demuxer(const Demuxer& d, MediaInfo& info) {
    info.valid = true;
    info.container = d.format_name();
    info.duration = d.duration();
    info.tracks = d.tracks();
    info.tags = d.tags();
    const int bv = d.best_track(MediaType::video);
    int va = -1;
    for (std::size_t i = 0; i < info.tracks.size(); ++i) {
        if (info.tracks[i].type == MediaType::video && video_codec_supported(info.tracks[i].codec) && va < 0) va = static_cast<int>(i);
    }
    info.video_track = bv >= 0 && video_codec_supported(info.tracks[static_cast<std::size_t>(bv)].codec) ? bv : (va >= 0 ? va : bv);
    const int ba = d.best_track(MediaType::audio);
    int aa = -1;
    for (std::size_t i = 0; i < info.tracks.size(); ++i) {
        if (info.tracks[i].type == MediaType::audio && audio::container_audio_codec_supported(info.tracks[i].codec) && aa < 0) aa = static_cast<int>(i);
    }
    info.audio_track = ba >= 0 && audio::container_audio_codec_supported(info.tracks[static_cast<std::size_t>(ba)].codec) ? ba : (aa >= 0 ? aa : ba);
    if (info.video_track >= 0) {
        const TrackInfo& v = info.tracks[static_cast<std::size_t>(info.video_track)];
        info.video_codec = v.codec;
        info.video_supported = video_codec_supported(v.codec);
        if (!info.video_supported) info.video_error = unsupported_video_codec_message(v.codec);
        info.width = v.width;
        info.height = v.height;
        info.display_width = scaled_width(v);
        info.display_height = scaled_height(v);
        info.display_aspect = info.display_height ? static_cast<double>(info.display_width) / static_cast<double>(info.display_height) : 0.0;
        info.rotation = v.rotation;
        info.frame_rate = v.frame_rate;
        if (v.duration > info.duration) info.duration = v.duration;
    }
    if (info.audio_track >= 0) {
        const TrackInfo& a = info.tracks[static_cast<std::size_t>(info.audio_track)];
        info.audio_codec = a.codec;
        info.audio_supported = audio::container_audio_codec_supported(a.codec);
        if (!info.audio_supported) info.audio_error = "unsupported audio codec " + (a.codec.empty() ? std::string("(unknown)") : a.codec);
        info.sample_rate = a.sample_rate;
        info.channels = a.channels;
        if (a.duration > info.duration) info.duration = a.duration;
    }
}

void fill_from_plain(const audio::AudioFileInfo& a, MediaInfo& info) {
    info.valid = true;
    info.container = plain_container_name(a.type);
    info.duration = a.duration;
    TrackInfo t;
    t.id = 1;
    t.type = MediaType::audio;
    t.codec = plain_codec_name(a.type);
    t.duration = a.duration;
    t.sample_rate = a.sample_rate;
    t.channels = a.channels;
    t.bitrate = a.bitrate_kbps * 1000u;
    t.is_default = true;
    t.frame_count = a.frames;
    info.tracks.assign(1, t);
    info.tags = a.tags;
    info.audio_track = 0;
    info.audio_supported = true;
    info.audio_codec = t.codec;
    info.sample_rate = a.sample_rate;
    info.channels = a.channels;
}

struct Source {
    std::string path;
    std::shared_ptr<const std::vector<std::uint8_t>> memory;

    bool open_demuxer(Demuxer& d, std::string* error) const {
        if (memory) return d.open_shared(memory, error);
        return d.open(path, error);
    }
};

bool probe_plain(const Source& src, audio::AudioFileInfo& out, audio::AudioFileType& type, std::string* error) {
    if (src.memory) {
        type = audio::detect_audio_type(src.memory->data(), src.memory->size());
    } else {
        type = audio::detect_audio_file(src.path);
    }
    if (!plain_reader_type(type)) {
        if (error) *error = "not a media file fizmo can play";
        return false;
    }
    audio::AudioReader r;
    const bool ok = src.memory ? r.open(src.memory->data(), src.memory->size(), error) : r.open(src.path, error);
    if (!ok) return false;
    out = r.info();
    return true;
}

MediaInfo probe_source(const Source& src) {
    MediaInfo info;
    Demuxer d;
    std::string err;
    if (src.open_demuxer(d, &err)) {
        fill_from_demuxer(d, info);
        if (info.video_track < 0 || !info.video_supported) {
            audio::AudioFileInfo a;
            audio::AudioFileType type = audio::AudioFileType::unknown;
            std::string ignored;
            if (info.container == "ogg" && probe_plain(src, a, type, &ignored)) {
                MediaInfo plain;
                fill_from_plain(a, plain);
                plain.container = "ogg";
                if (info.video_track >= 0) {
                    plain.tracks = info.tracks;
                    plain.video_track = info.video_track;
                    plain.video_codec = info.video_codec;
                    plain.video_error = info.video_error;
                    plain.audio_track = info.audio_track;
                }
                return plain;
            }
        }
        if (info.video_track < 0 && info.audio_track < 0) info.error = "the file has no audio or video track";
        return info;
    }
    audio::AudioFileInfo a;
    audio::AudioFileType type = audio::AudioFileType::unknown;
    std::string perr;
    if (probe_plain(src, a, type, &perr)) {
        fill_from_plain(a, info);
        return info;
    }
    info.valid = false;
    info.error = err.empty() ? perr : err;
    if (err == "unknown container format" && !perr.empty()) info.error = perr;
    return info;
}

bool extract_from(const Source& src, double seconds, VideoFrame& out, std::string* error) {
    Demuxer d;
    if (!src.open_demuxer(d, error)) return false;
    MediaInfo info;
    fill_from_demuxer(d, info);
    if (info.video_track < 0) {
        if (error) *error = "the file has no video track";
        return false;
    }
    if (!info.video_supported) {
        if (error) *error = info.video_error;
        return false;
    }
    const std::uint32_t vt = static_cast<std::uint32_t>(info.video_track);
    for (std::size_t i = 0; i < info.tracks.size(); ++i) d.select(static_cast<std::uint32_t>(i), i == vt);
    std::unique_ptr<VideoDecoder> dec = create_video_decoder(info.tracks[vt], error);
    if (!dec) return false;
    if (!(seconds == seconds) || seconds < 0.0) seconds = 0.0;
    if (seconds > 0.0) d.seek(seconds, true);
    VideoFrame best;
    VideoFrame f;
    bool have = false;
    bool done = false;
    Packet p;
    std::size_t guard = 0;
    auto take = [&](VideoFrame& frame) {
        if (frame.pts < -1e-6) return;
        if (frame.pts <= seconds + 1e-6 || !have) {
            best = std::move(frame);
            have = true;
            if (best.pts > seconds + 1e-6) done = true;
        } else {
            done = true;
        }
    };
    while (!done && d.read(p)) {
        if (p.track != vt) continue;
        if (++guard > 100000) break;
        dec->decode_packet(p);
        while (!done && dec->receive(f)) take(f);
    }
    if (!done) {
        dec->flush();
        while (!done && dec->receive(f)) take(f);
    }
    if (!have) {
        if (error) *error = "no video frame could be decoded";
        return false;
    }
    out = std::move(best);
    return true;
}

struct QFrame {
    VideoFrame frame;
    std::vector<std::uint8_t> rgba;
    double t = 0.0;
    std::uint64_t gen = 0;
};

struct QPacket {
    enum class Kind { data, flush, epoch, eos };
    Kind kind = Kind::data;
    Packet packet;
    std::uint64_t gen = 0;
    double target = 0.0;
    int epoch = 0;
    std::unique_ptr<VideoDecoder> decoder;
};

struct AudioBox {
    std::mutex mutex;
    std::unique_ptr<audio::ContainerAudioReader> container;
    std::unique_ptr<audio::AudioReader> plain;
    std::uint32_t rate = 0;
    std::uint16_t channels = 0;
    std::int64_t offset = 0;
    std::int64_t inner_pos = -1;
    bool inner_eof = false;

    std::size_t inner_read(float* out, std::size_t frames) {
        return container ? container->read(out, frames) : plain->read(out, frames);
    }

    void inner_seek(std::int64_t frame) {
        if (container) container->seek(static_cast<std::uint64_t>(frame));
        else plain->seek(static_cast<std::uint64_t>(frame));
        inner_pos = frame;
        inner_eof = false;
    }
};

class PlayerAudioSource : public audio::AudioSource {
public:
    PlayerAudioSource(std::shared_ptr<AudioBox> box, std::int64_t period, std::shared_ptr<std::atomic<bool>> loop, std::int64_t start)
        : m_box(std::move(box)), m_period(period), m_loop(std::move(loop)), m_pos(start) {
        if (m_period > 0 && m_pos > m_period) m_pos = m_period;
    }

    std::size_t read(float* out, std::size_t frames) override {
        std::lock_guard<std::mutex> lock(m_box->mutex);
        AudioBox& b = *m_box;
        const std::size_t ch = b.channels;
        std::size_t done = 0;
        while (done < frames) {
            std::int64_t local = m_pos - m_iter;
            if (m_period > 0 && local >= m_period) {
                if (!m_loop->load(std::memory_order_relaxed)) break;
                m_iter += m_period;
                continue;
            }
            std::int64_t limit = static_cast<std::int64_t>(frames - done);
            if (m_period > 0) limit = std::min(limit, m_period - local);
            if (local < b.offset) {
                const std::int64_t n = std::min(limit, b.offset - local);
                std::fill(out + done * ch, out + (done + static_cast<std::size_t>(n)) * ch, 0.0f);
                done += static_cast<std::size_t>(n);
                m_pos += n;
                continue;
            }
            const std::int64_t want = local - b.offset;
            if (b.inner_pos != want) b.inner_seek(want);
            if (b.inner_eof) {
                if (m_period <= 0) break;
                std::fill(out + done * ch, out + (done + static_cast<std::size_t>(limit)) * ch, 0.0f);
                done += static_cast<std::size_t>(limit);
                m_pos += limit;
                continue;
            }
            const std::size_t got = b.inner_read(out + done * ch, static_cast<std::size_t>(limit));
            if (got == 0) {
                b.inner_eof = true;
                continue;
            }
            done += got;
            m_pos += static_cast<std::int64_t>(got);
            b.inner_pos += static_cast<std::int64_t>(got);
        }
        return done;
    }

    bool seek(std::uint64_t frame) override {
        std::lock_guard<std::mutex> lock(m_box->mutex);
        m_pos = static_cast<std::int64_t>(std::min<std::uint64_t>(frame, static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max() / 4)));
        m_iter = 0;
        return true;
    }

    std::uint64_t position() const noexcept override {
        return static_cast<std::uint64_t>(std::max<std::int64_t>(0, m_pos));
    }

private:
    std::shared_ptr<AudioBox> m_box;
    std::int64_t m_period;
    std::shared_ptr<std::atomic<bool>> m_loop;
    std::int64_t m_pos;
    std::int64_t m_iter = 0;
};

} // namespace

const char* player_state_name(PlayerState state) noexcept {
    switch (state) {
        case PlayerState::closed: return "closed";
        case PlayerState::paused: return "paused";
        case PlayerState::playing: return "playing";
        case PlayerState::ended: return "ended";
        case PlayerState::error: return "error";
    }
    return "unknown";
}

const char* player_event_name(PlayerEventType type) noexcept {
    switch (type) {
        case PlayerEventType::loaded: return "loaded";
        case PlayerEventType::first_frame: return "first_frame";
        case PlayerEventType::play: return "play";
        case PlayerEventType::pause: return "pause";
        case PlayerEventType::playing: return "playing";
        case PlayerEventType::waiting: return "waiting";
        case PlayerEventType::seeking: return "seeking";
        case PlayerEventType::seeked: return "seeked";
        case PlayerEventType::ended: return "ended";
        case PlayerEventType::looped: return "looped";
        case PlayerEventType::rate_changed: return "rate_changed";
        case PlayerEventType::volume_changed: return "volume_changed";
        case PlayerEventType::track_changed: return "track_changed";
        case PlayerEventType::warning: return "warning";
        case PlayerEventType::error: return "error";
    }
    return "unknown";
}

MediaInfo probe_media(const std::string& path) {
    Source s;
    s.path = path;
    return probe_source(s);
}

MediaInfo probe_media(const void* data, std::size_t size) {
    Source s;
    s.memory = std::make_shared<const std::vector<std::uint8_t>>(static_cast<const std::uint8_t*>(data), static_cast<const std::uint8_t*>(data) + (data ? size : 0));
    return probe_source(s);
}

bool extract_frame(const std::string& path, double seconds, VideoFrame& out, std::string* error) {
    Source s;
    s.path = path;
    return extract_from(s, seconds, out, error);
}

bool extract_frame(const void* data, std::size_t size, double seconds, VideoFrame& out, std::string* error) {
    if (!data || size == 0) {
        if (error) *error = "no data";
        return false;
    }
    Source s;
    s.memory = std::make_shared<const std::vector<std::uint8_t>>(static_cast<const std::uint8_t*>(data), static_cast<const std::uint8_t*>(data) + size);
    return extract_from(s, seconds, out, error);
}

struct MediaPlayer::Impl {
    mutable std::mutex mu;
    std::condition_variable cv_worker;
    std::condition_variable cv_decode;

    PlayerOptions opts;
    audio::Mixer* external = nullptr;
    std::unique_ptr<audio::Mixer> own;
    bool own_failed = false;

    std::mutex ev_mu;
    std::deque<PlayerEvent> events;
    std::deque<PlayerEvent> dispatch;
    EventCallback callback;

    bool opened = false;
    Source source;
    MediaInfo info;
    bool plain_audio = false;
    int vtrack = -1;
    int atrack = -1;
    bool video_on = false;
    bool audio_on = false;
    std::string video_err;
    std::string audio_err;
    double period = 0.0;
    double frame_duration = 1.0 / 30.0;

    Demuxer vdemux;
    int epoch = 0;
    bool demux_eos = false;
    std::shared_ptr<AudioBox> box;
    int box_track = -2;

    std::deque<QPacket> packets;
    std::size_t packet_bytes = 0;
    std::deque<std::unique_ptr<QFrame>> frames;
    std::vector<std::unique_ptr<QFrame>> pool;
    std::uint64_t vgen = 0;
    std::uint64_t ready_gen = 0;
    std::uint64_t eos_gen = 0;
    std::size_t max_frames = 8;
    bool quit = false;
    bool restarting = false;
    bool rgba_on = false;
    std::unique_ptr<VideoDecoder> pending_decoder;

    std::unique_ptr<QFrame> presented;
    std::uint64_t serial = 0;
    std::uint64_t presented_gen = 0;

    bool user_playing = false;
    bool pinned = true;
    double pinned_time = 0.0;
    double base_time = 0.0;
    std::int64_t base_ns = 0;
    double rate = 1.0;
    bool preserve = true;
    float volume = 1.0f;
    bool muted = false;
    bool loop = false;
    std::shared_ptr<std::atomic<bool>> loop_flag = std::make_shared<std::atomic<bool>>(false);
    bool audio_master = false;
    audio::Mixer* voice_mixer = nullptr;
    audio::Mixer::VoiceId voice = 0;
    double voice_base = 0.0;
    double last_audio = 0.0;
    bool voice_stretch = false;
    PlayerState state = PlayerState::closed;
    bool ended_flag = false;
    bool seeking = false;
    std::uint64_t seek_gen = 0;
    double seek_target = 0.0;
    bool cmd_seek = false;
    bool cmd_audio = false;
    double cmd_target = 0.0;
    bool cmd_accurate = true;
    int cmd_vtrack = -2;
    bool first_frame_sent = false;
    bool initial_seek = true;
    long long last_cycle = 0;
    bool waiting = false;
    std::int64_t starve_since = 0;
    double watch_value = -1.0;
    std::int64_t watch_ns = 0;
    std::string error;

    std::uint64_t frames_decoded = 0;
    std::uint64_t frames_presented = 0;
    std::uint64_t frames_dropped = 0;
    std::uint64_t frames_skipped = 0;
    std::uint64_t packets_skipped = 0;
    std::uint64_t decode_errors = 0;
    double decode_seconds = 0.0;
    std::uint64_t decode_count = 0;
    double convert_seconds = 0.0;
    std::uint64_t convert_count = 0;
    std::uint64_t frames_deinterlaced = 0;
    bool deinterlacing = false;

    std::thread worker;
    std::thread decoder_thread;

    ~Impl() { shutdown(); }

    audio::Mixer* mixer_locked() const noexcept { return external ? external : (own && !own_failed ? own.get() : nullptr); }

    void post(PlayerEventType type, double position, const std::string& message = std::string()) {
        PlayerEvent e;
        e.type = type;
        e.position = position;
        e.message = message;
        std::lock_guard<std::mutex> lock(ev_mu);
        events.push_back(e);
        while (events.size() > 512) events.pop_front();
        if (callback) dispatch.push_back(e);
    }

    void dispatch_events() {
        std::deque<PlayerEvent> local;
        EventCallback cb;
        {
            std::lock_guard<std::mutex> lock(ev_mu);
            if (dispatch.empty() || !callback) {
                dispatch.clear();
                return;
            }
            local.swap(dispatch);
            cb = callback;
        }
        for (const PlayerEvent& e : local) cb(e);
    }

    double audio_clock_locked() {
        audio::Mixer* m = voice_mixer;
        if (!m || !voice) return last_audio + voice_base;
        double p = m->playback_position(voice);
        if (p < last_audio) {
            if (!m->is_playing(voice)) p = last_audio;
            else if (last_audio - p < 0.25) p = last_audio;
        }
        last_audio = p;
        return voice_base + p;
    }

    double clock_locked() {
        if (pinned) return pinned_time;
        if (audio_master) return audio_clock_locked();
        return base_time + static_cast<double>(now_ns() - base_ns) * 1e-9 * rate;
    }

    double position_from(double u) const noexcept {
        if (period > 0.0) {
            if (ended_flag) return period;
            if (u >= period) {
                const double k = std::floor(u / period);
                return u - k * period;
            }
        }
        return u < 0.0 ? 0.0 : u;
    }

    void recycle_locked(std::unique_ptr<QFrame> f) {
        if (!f) return;
        if (pool.size() < 16) pool.push_back(std::move(f));
    }

    std::unique_ptr<QFrame> take_pool_locked() {
        if (pool.empty()) return std::unique_ptr<QFrame>(new QFrame());
        std::unique_ptr<QFrame> f = std::move(pool.back());
        pool.pop_back();
        return f;
    }

    void advance_locked(double u) {
        bool dropped = false;
        while (frames.size() >= 2 && frames[1]->t <= u && frames[0]->gen == frames[1]->gen) {
            recycle_locked(std::move(frames.front()));
            frames.pop_front();
            ++frames_dropped;
            dropped = true;
        }
        if (dropped) cv_decode.notify_all();
    }

    bool present_locked(double u) {
        advance_locked(u);
        if (frames.empty() || frames[0]->gen != vgen) return false;
        const bool fresh = !presented || presented_gen != vgen;
        if (frames[0]->t > u && !fresh) return false;
        recycle_locked(std::move(presented));
        presented = std::move(frames.front());
        frames.pop_front();
        presented_gen = presented->gen;
        ++serial;
        ++frames_presented;
        cv_decode.notify_all();
        return true;
    }

    void drop_presented_locked() {
        recycle_locked(std::move(presented));
        ++serial;
    }

    void resume_locked() {
        if (audio_master && voice_mixer && voice) {
            voice_mixer->set_paused(voice, false);
        } else {
            base_time = pinned_time;
            base_ns = now_ns();
        }
        pinned = false;
        watch_value = -1.0;
        state = PlayerState::playing;
    }

    void pause_locked() {
        if (!pinned) pinned_time = clock_locked();
        pinned = true;
        if (voice_mixer && voice) voice_mixer->set_paused(voice, true);
    }

    void request_audio_restart_locked() {
        if (!opened) return;
        if (!pinned) {
            pinned_time = clock_locked();
            pinned = true;
        }
        if (voice_mixer && voice) voice_mixer->set_paused(voice, true);
        cmd_audio = true;
        restarting = true;
        cv_worker.notify_all();
    }

    void do_audio_restart() {
        double u;
        {
            std::lock_guard<std::mutex> lock(mu);
            cmd_audio = false;
            u = pinned_time;
        }
        start_voice(position_from(u), u);
        std::lock_guard<std::mutex> lock(mu);
        restarting = false;
        if (!seeking && !cmd_seek && user_playing && !ended_flag) resume_locked();
        cv_worker.notify_all();
    }

    void request_seek_locked(double target, bool accurate) {
        cmd_seek = true;
        cmd_target = target;
        cmd_accurate = accurate;
        seeking = true;
        pinned = true;
        pinned_time = target;
        ended_flag = false;
        if (voice_mixer && voice) voice_mixer->set_paused(voice, true);
        cv_worker.notify_all();
    }

    void shutdown() {
        {
            std::lock_guard<std::mutex> lock(mu);
            quit = true;
        }
        cv_worker.notify_all();
        cv_decode.notify_all();
        if (worker.joinable()) worker.join();
        if (decoder_thread.joinable()) decoder_thread.join();
        std::lock_guard<std::mutex> lock(mu);
        if (voice_mixer && voice) voice_mixer->stop(voice);
        voice = 0;
        voice_mixer = nullptr;
        box.reset();
        box_track = -2;
        packets.clear();
        packet_bytes = 0;
        frames.clear();
        presented.reset();
        vdemux.close();
        quit = false;
    }

    bool open_box(int track, std::string* err) {
        std::shared_ptr<AudioBox> b = std::make_shared<AudioBox>();
        if (plain_audio) {
            b->plain.reset(new audio::AudioReader());
            const bool ok = source.memory ? b->plain->open(source.memory->data(), source.memory->size(), err) : b->plain->open(source.path, err);
            if (!ok) return false;
            b->rate = b->plain->info().sample_rate;
            b->channels = b->plain->info().channels;
        } else {
            b->container.reset(new audio::ContainerAudioReader());
            const bool ok = source.memory ? b->container->open_shared(source.memory, err, track) : b->container->open(source.path, err, track);
            if (!ok) return false;
            b->rate = b->container->info().sample_rate;
            b->channels = b->container->info().channels;
            b->offset = static_cast<std::int64_t>(std::llround(b->container->info().start_offset * static_cast<double>(b->rate)));
        }
        b->inner_pos = 0;
        if (b->rate == 0 || b->channels == 0) {
            if (err) *err = "the audio track has no format";
            return false;
        }
        box = std::move(b);
        box_track = track;
        return true;
    }

    void start_voice(double target, double base) {
        audio::Mixer* m;
        int track;
        bool enabled;
        audio::StreamConfig output;
        {
            std::lock_guard<std::mutex> lock(mu);
            output = opts.output;
            enabled = audio_on && opts.enable_audio;
            track = atrack;
            if (enabled && !external && !own && !own_failed) {
                own.reset(new audio::Mixer(opts.output_sample_rate ? opts.output_sample_rate : 48000, opts.output_channels ? opts.output_channels : 2));
            }
            m = mixer_locked();
        }
        if (enabled && own && m == own.get() && !own->output_running()) {
            std::string err;
            if (!own->start(output, &err)) {
                std::lock_guard<std::mutex> lock(mu);
                own_failed = true;
                m = mixer_locked();
                post(PlayerEventType::warning, pinned_time, "no audio output (" + err + "); playing without sound");
            }
        }
        audio::Mixer* old_mixer;
        audio::Mixer::VoiceId old_voice;
        {
            std::lock_guard<std::mutex> lock(mu);
            old_mixer = voice_mixer;
            old_voice = voice;
            voice = 0;
            voice_mixer = nullptr;
            audio_master = false;
        }
        if (old_mixer && old_voice) old_mixer->stop(old_voice);
        if (!enabled || !m) return;
        std::string err;
        if (!box || box_track != track) {
            box.reset();
            if (!open_box(track, &err)) {
                std::lock_guard<std::mutex> lock(mu);
                audio_on = false;
                audio_err = err;
                post(PlayerEventType::warning, target, "can't play the audio: " + err);
                return;
            }
        }
        double p;
        float gain;
        double r;
        bool keep;
        double buffer;
        {
            std::lock_guard<std::mutex> lock(mu);
            p = period;
            gain = muted ? 0.0f : volume;
            r = rate;
            keep = preserve;
            buffer = opts.audio_buffer_seconds;
        }
        const double rate_hz = static_cast<double>(box->rate);
        const std::int64_t period_frames = p > 0.0 ? static_cast<std::int64_t>(std::llround(p * rate_hz)) : 0;
        const std::int64_t start = static_cast<std::int64_t>(std::llround(std::max(0.0, target) * rate_hz));
        std::unique_ptr<audio::AudioSource> src(new PlayerAudioSource(box, period_frames, loop_flag, start));
        audio::AudioFileInfo ai;
        ai.type = audio::AudioFileType::unknown;
        ai.sample_rate = box->rate;
        ai.channels = box->channels;
        std::unique_ptr<audio::AudioReader> reader(new audio::AudioReader());
        if (!reader->open_source(std::move(src), ai, &err)) return;
        audio::PlayOptions po;
        po.paused = true;
        po.gain = gain;
        po.speed = static_cast<float>(r);
        po.preserve_pitch = keep;
        po.stretchable = keep && std::fabs(r - 1.0) > 1e-9;
        po.stream_buffer_seconds = std::max(0.25, buffer);
        const audio::Mixer::VoiceId id = m->play_stream(std::move(reader), po, &err);
        std::lock_guard<std::mutex> lock(mu);
        if (!id) {
            post(PlayerEventType::warning, target, "can't start the audio: " + err);
            return;
        }
        voice = id;
        voice_mixer = m;
        voice_base = base;
        last_audio = 0.0;
        voice_stretch = po.stretchable;
        audio_master = true;
    }

    void do_seek(double target, bool accurate, int new_vtrack) {
        std::unique_ptr<VideoDecoder> newdec;
        bool video;
        std::uint64_t gen;
        {
            std::lock_guard<std::mutex> lock(mu);
            if (period > 0.0 && target > period) target = period;
            if (target < 0.0) target = 0.0;
            gen = ++vgen;
            seek_gen = gen;
            seeking = true;
            restarting = true;
            pinned = true;
            pinned_time = target;
            for (auto& f : frames) recycle_locked(std::move(f));
            frames.clear();
            for (QPacket& q : packets) {
                if (q.decoder) pending_decoder = std::move(q.decoder);
            }
            packets.clear();
            packet_bytes = 0;
            demux_eos = false;
            epoch = 0;
            last_cycle = 0;
            cv_decode.notify_all();
        }
        if (new_vtrack != -2) {
            std::string err;
            bool ok = new_vtrack >= 0;
            if (ok) {
                vdemux.close();
                ok = source.open_demuxer(vdemux, &err);
                if (ok) {
                    for (std::size_t i = 0; i < vdemux.tracks().size(); ++i) vdemux.select(static_cast<std::uint32_t>(i), static_cast<int>(i) == new_vtrack);
                    newdec = create_video_decoder(vdemux.tracks()[static_cast<std::size_t>(new_vtrack)], &err);
                    ok = newdec != nullptr;
                }
            }
            std::lock_guard<std::mutex> lock(mu);
            vtrack = ok ? new_vtrack : -1;
            video_on = ok;
            if (!ok && new_vtrack >= 0) {
                video_err = err;
                post(PlayerEventType::warning, target, err);
            }
            if (ok) {
                const TrackInfo& t = vdemux.tracks()[static_cast<std::size_t>(new_vtrack)];
                if (t.frame_rate > 0.0) frame_duration = 1.0 / t.frame_rate;
            }
        }
        {
            std::lock_guard<std::mutex> lock(mu);
            video = video_on;
            if (!newdec && new_vtrack == -2) newdec = std::move(pending_decoder);
            pending_decoder.reset();
        }
        Packet first;
        bool have_first = false;
        if (video) {
            vdemux.seek(target, true);
            if (!accurate && vdemux.read(first)) {
                have_first = true;
                if (first.keyframe && first.pts >= 0.0) target = first.pts;
            }
        }
        {
            std::lock_guard<std::mutex> lock(mu);
            if (gen == vgen && video) {
                QPacket q;
                q.kind = QPacket::Kind::flush;
                q.gen = gen;
                q.target = target;
                q.decoder = std::move(newdec);
                packets.push_back(std::move(q));
                if (have_first) {
                    QPacket d;
                    d.gen = gen;
                    packet_bytes += first.data.size();
                    d.packet = std::move(first);
                    packets.push_back(std::move(d));
                }
                cv_decode.notify_all();
            }
            pinned_time = target;
            seek_target = target;
        }
        start_voice(target, target);
        std::lock_guard<std::mutex> lock(mu);
        pinned_time = target;
        restarting = false;
        cmd_audio = false;
        cv_worker.notify_all();
    }

    bool packets_wanted_locked() const {
        return video_on && !demux_eos && packet_bytes < opts.max_queued_packet_bytes && packets.size() < 1024;
    }

    void read_packets() {
        for (int i = 0; i < 32; ++i) {
            Packet p;
            const bool got = vdemux.read(p);
            std::lock_guard<std::mutex> lock(mu);
            if (cmd_seek || quit) return;
            if (got) {
                if (static_cast<int>(p.track) != vtrack) continue;
                QPacket q;
                q.gen = vgen;
                packet_bytes += p.data.size();
                q.packet = std::move(p);
                packets.push_back(std::move(q));
                cv_decode.notify_all();
                if (!packets_wanted_locked()) return;
                continue;
            }
            if (loop && period > 0.0) {
                vdemux.rewind();
                ++epoch;
                QPacket q;
                q.kind = QPacket::Kind::epoch;
                q.gen = vgen;
                q.epoch = epoch;
                packets.push_back(std::move(q));
                cv_decode.notify_all();
                continue;
            }
            QPacket q;
            q.kind = QPacket::Kind::eos;
            q.gen = vgen;
            packets.push_back(std::move(q));
            demux_eos = true;
            cv_decode.notify_all();
            return;
        }
    }

    void checks_locked() {
        if (seeking && !cmd_seek && !restarting && seek_gen == vgen && (!video_on || ready_gen == seek_gen)) {
            seeking = false;
            if (user_playing) {
                pinned_time = seek_target;
                resume_locked();
            } else {
                state = PlayerState::paused;
            }
            if (!first_frame_sent) {
                first_frame_sent = true;
                post(PlayerEventType::first_frame, seek_target);
            }
            if (initial_seek) {
                initial_seek = false;
                if (user_playing) post(PlayerEventType::play, seek_target);
            } else {
                post(PlayerEventType::seeked, position_from(seek_target));
            }
            if (user_playing) post(PlayerEventType::playing, position_from(seek_target));
        }
        if (seeking || pinned || cmd_seek || restarting) return;
        const double u = clock_locked();
        advance_locked(u);
        if (audio_master && voice_mixer && voice) {
            const std::int64_t t = now_ns();
            if (watch_value != u) {
                watch_value = u;
                watch_ns = t;
            } else if (t - watch_ns > 1500000000 && voice_mixer->buffered_seconds(voice) > 0.1) {
                post(PlayerEventType::warning, position_from(u), "the audio output is not running; using the system clock");
                audio_master = false;
                base_time = u;
                base_ns = t;
            }
        }
        if (video_on) {
            const bool starving = frames.empty() && eos_gen != vgen;
            const std::int64_t t = now_ns();
            if (starving) {
                if (!starve_since) starve_since = t;
                if (!waiting && t - starve_since > 150000000) {
                    waiting = true;
                    post(PlayerEventType::waiting, position_from(u));
                }
            } else {
                starve_since = 0;
                if (waiting) {
                    waiting = false;
                    post(PlayerEventType::playing, position_from(u));
                }
            }
        }
        if (loop && period > 0.0) {
            const long long cycle = static_cast<long long>(std::floor(u / period));
            if (cycle > last_cycle) {
                last_cycle = cycle;
                post(PlayerEventType::looped, position_from(u));
            }
            return;
        }
        const bool video_done = !video_on || (eos_gen == vgen && (frames.empty() || (frames.size() == 1 && frames[0]->t <= u)));
        bool audio_done = true;
        if (audio_master && voice_mixer && voice) audio_done = !voice_mixer->is_playing(voice);
        bool time_done = true;
        if (period > 0.0) {
            const double iter_end = (std::floor(std::max(0.0, u - 1e-6) / period) + 1.0) * period;
            time_done = u >= iter_end - 0.002;
            if (audio_master) audio_done = audio_done || time_done;
        }
        if (video_done && audio_done && (audio_master || time_done)) {
            if (loop) {
                request_seek_locked(0.0, true);
                post(PlayerEventType::looped, 0.0);
                return;
            }
            pinned_time = u;
            pinned = true;
            if (voice_mixer && voice) voice_mixer->set_paused(voice, true);
            ended_flag = true;
            user_playing = false;
            state = PlayerState::ended;
            post(PlayerEventType::ended, position_from(u));
        }
    }

    void worker_loop() {
        while (true) {
            int vt = -2;
            double target = 0.0;
            bool accurate = true;
            bool do_cmd = false;
            bool do_audio = false;
            bool read = false;
            {
                std::unique_lock<std::mutex> lock(mu);
                const auto ready = [&]() { return quit || cmd_seek || cmd_audio || packets_wanted_locked(); };
                const bool busy = seeking || !pinned;
                cv_worker.wait_for(lock, std::chrono::milliseconds(busy ? 4 : 40), ready);
                if (quit) break;
                if (cmd_seek) {
                    do_cmd = true;
                    cmd_seek = false;
                    target = cmd_target;
                    accurate = cmd_accurate;
                    vt = cmd_vtrack;
                    cmd_vtrack = -2;
                } else if (cmd_audio) {
                    do_audio = true;
                } else {
                    checks_locked();
                    read = packets_wanted_locked();
                }
            }
            if (do_cmd) do_seek(target, accurate, vt);
            else if (do_audio) do_audio_restart();
            else if (read) read_packets();
            dispatch_events();
        }
        dispatch_events();
    }

    void emit_frame(std::unique_ptr<QFrame> f, std::uint64_t gen, bool convert) {
        bool want;
        {
            std::lock_guard<std::mutex> lock(mu);
            want = rgba_on;
        }
        if (convert && want && !f->frame.empty()) {
            const std::int64_t t0 = now_ns();
            const std::size_t bytes = static_cast<std::size_t>(f->frame.width) * f->frame.height * 4;
            f->rgba.resize(bytes);
            to_rgba(f->frame, f->rgba.data(), static_cast<std::size_t>(f->frame.width) * 4);
            const double dt = static_cast<double>(now_ns() - t0) * 1e-9;
            std::lock_guard<std::mutex> lock(mu);
            convert_seconds += dt;
            ++convert_count;
        } else {
            f->rgba.clear();
        }
        f->gen = gen;
        std::unique_lock<std::mutex> lock(mu);
        cv_decode.wait(lock, [&]() { return quit || gen != vgen || frames.size() < max_frames; });
        if (quit || gen != vgen) {
            recycle_locked(std::move(f));
            return;
        }
        frames.push_back(std::move(f));
        if (ready_gen != gen) {
            ready_gen = gen;
            cv_worker.notify_all();
        }
    }

    void decode_loop() {
        std::unique_ptr<VideoDecoder> dec;
        std::uint64_t gen = 0;
        double target = 0.0;
        bool filtering = false;
        int cur_epoch = 0;
        double per = 0.0;
        bool skipping = false;
        double last_t = -1e300;
        std::unique_ptr<QFrame> candidate;
        VideoFrame frame;
        Deinterlacer deint;
        DeinterlaceSetting deint_setting = DeinterlaceSetting::off;
        bool deint_used = false;

        auto deliver = [&](VideoFrame& vf) {
            std::unique_ptr<QFrame> q;
            double now_u;
            bool playing;
            double fd;
            {
                std::lock_guard<std::mutex> lock(mu);
                fd = frame_duration;
                ++frames_decoded;
                if (gen != vgen) return;
                if (vf.pts < -1e-6) {
                    ++frames_skipped;
                    return;
                }
                q = take_pool_locked();
                now_u = clock_locked();
                playing = !pinned && !seeking;
            }
            q->t = vf.pts + static_cast<double>(cur_epoch) * per;
            q->frame = std::move(vf);
            if (filtering) {
                if (q->t <= target + 1e-6) {
                    if (candidate) {
                        std::lock_guard<std::mutex> lock(mu);
                        ++frames_skipped;
                        recycle_locked(std::move(candidate));
                    }
                    candidate = std::move(q);
                    return;
                }
                filtering = false;
                if (candidate) {
                    last_t = candidate->t;
                    emit_frame(std::move(candidate), gen, true);
                }
            }
            last_t = q->t;
            const bool late = playing && q->t + 2.0 * fd < now_u;
            emit_frame(std::move(q), gen, !late);
        };

        auto drain_deint = [&]() {
            VideoFrame out;
            while (deint.pull(out)) deliver(out);
            std::lock_guard<std::mutex> lock(mu);
            frames_deinterlaced = deint.frames_deinterlaced();
            deinterlacing = deint.last_was_deinterlaced() && deint_setting != DeinterlaceSetting::off;
        };

        auto handle = [&](VideoFrame& vf) {
            DeinterlaceOptions want;
            {
                std::lock_guard<std::mutex> lock(mu);
                deint_setting = opts.deinterlace;
                want.mode = opts.deinterlace_mode;
                want.field_rate = opts.deinterlace_field_rate;
            }
            want.only_interlaced = deint_setting != DeinterlaceSetting::on;
            if (deint_setting == DeinterlaceSetting::off) {
                if (deint_used) {
                    deint.flush();
                    drain_deint();
                    deint_used = false;
                }
                deliver(vf);
                return;
            }
            const DeinterlaceOptions& have = deint.options();
            if (have.mode != want.mode || have.field_rate != want.field_rate || have.only_interlaced != want.only_interlaced) {
                deint.flush();
                drain_deint();
                deint.set_options(want);
            }
            deint_used = true;
            deint.push(std::move(vf));
            drain_deint();
        };

        while (true) {
            QPacket qp;
            {
                std::unique_lock<std::mutex> lock(mu);
                cv_decode.wait(lock, [&]() { return quit || !packets.empty(); });
                if (quit) break;
                qp = std::move(packets.front());
                packets.pop_front();
                if (qp.kind == QPacket::Kind::data) packet_bytes -= std::min(packet_bytes, qp.packet.data.size());
                per = period;
                cv_worker.notify_all();
                if (qp.gen != vgen) continue;
            }
            switch (qp.kind) {
                case QPacket::Kind::flush:
                    gen = qp.gen;
                    target = qp.target;
                    filtering = true;
                    cur_epoch = 0;
                    skipping = false;
                    last_t = -1e300;
                    if (candidate) {
                        std::lock_guard<std::mutex> lock(mu);
                        recycle_locked(std::move(candidate));
                    }
                    deint.reset();
                    if (qp.decoder) dec = std::move(qp.decoder);
                    if (dec) dec->reset();
                    continue;
                case QPacket::Kind::epoch:
                    if (dec) {
                        dec->flush();
                        while (dec->receive(frame)) handle(frame);
                        dec->reset();
                    }
                    deint.flush();
                    drain_deint();
                    cur_epoch = qp.epoch;
                    continue;
                case QPacket::Kind::eos:
                    if (dec) {
                        dec->flush();
                        while (dec->receive(frame)) handle(frame);
                    }
                    deint.flush();
                    drain_deint();
                    if (candidate) {
                        filtering = false;
                        emit_frame(std::move(candidate), gen, true);
                    }
                    {
                        std::lock_guard<std::mutex> lock(mu);
                        if (gen == vgen) {
                            eos_gen = gen;
                            if (ready_gen != gen) ready_gen = gen;
                            cv_worker.notify_all();
                        }
                    }
                    continue;
                case QPacket::Kind::data:
                    break;
            }
            if (!dec) continue;
            if (filtering && cur_epoch == 0) {
                double fd;
                {
                    std::lock_guard<std::mutex> lock(mu);
                    fd = frame_duration;
                }
                const double spacing = std::max(fd, qp.packet.duration);
                if (qp.packet.pts + 2.0 * spacing < target && qp.packet.pts < target - 0.1 && dec->is_disposable(qp.packet.data.data(), qp.packet.data.size())) {
                    std::lock_guard<std::mutex> lock(mu);
                    ++packets_skipped;
                    continue;
                }
            }
            if (!filtering) {
                double now_u;
                bool playing;
                {
                    std::lock_guard<std::mutex> lock(mu);
                    now_u = clock_locked();
                    playing = !pinned && !seeking;
                }
                const double pkt_t = qp.packet.pts + static_cast<double>(cur_epoch) * per;
                if (playing && !skipping && !qp.packet.keyframe && last_t > -1e299 && last_t < now_u - 1.0) skipping = true;
                if (skipping) {
                    if (!qp.packet.keyframe || pkt_t < now_u - 0.25) {
                        std::lock_guard<std::mutex> lock(mu);
                        ++packets_skipped;
                        continue;
                    }
                    skipping = false;
                    dec->reset();
                }
            }
            const std::int64_t t0 = now_ns();
            std::string err;
            const bool ok = dec->decode_packet(qp.packet, &err);
            const double dt = static_cast<double>(now_ns() - t0) * 1e-9;
            {
                std::lock_guard<std::mutex> lock(mu);
                decode_seconds += dt;
                ++decode_count;
                if (!ok) ++decode_errors;
            }
            while (dec->receive(frame)) handle(frame);
        }
        if (candidate) {
            std::lock_guard<std::mutex> lock(mu);
            recycle_locked(std::move(candidate));
        }
    }

    bool open_source(Source src, std::string* error) {
        shutdown();
        {
            std::lock_guard<std::mutex> lock(mu);
            opened = false;
            state = PlayerState::closed;
        }
        MediaInfo mi = probe_source(src);
        if (!mi.valid || (mi.video_track < 0 && mi.audio_track < 0)) {
            const std::string msg = mi.error.empty() ? "can't open the media" : mi.error;
            if (error) *error = msg;
            std::lock_guard<std::mutex> lock(mu);
            this->error = msg;
            state = PlayerState::error;
            post(PlayerEventType::error, 0.0, msg);
            return false;
        }
        const bool plain = mi.tracks.size() == 1 && mi.container != "mp4" && mi.container != "mov" && mi.container != "m4a" && mi.container != "3gp" && mi.container != "matroska" && mi.container != "webm" && mi.container != "ivf" && mi.container != "hevc" && mi.container != "obu" && mi.container != "av1 annexb" && mi.tracks[0].type != MediaType::video && (mi.container != "ogg" || mi.video_track < 0 || !mi.video_supported);
        std::string verr;
        std::unique_ptr<VideoDecoder> dec;
        bool video = false;
        if (opts.enable_video && mi.video_track >= 0 && mi.video_supported && !plain) {
            vdemux.close();
            if (src.open_demuxer(vdemux, &verr)) {
                for (std::size_t i = 0; i < vdemux.tracks().size(); ++i) vdemux.select(static_cast<std::uint32_t>(i), static_cast<int>(i) == mi.video_track);
                dec = create_video_decoder(vdemux.tracks()[static_cast<std::size_t>(mi.video_track)], &verr);
                video = dec != nullptr;
            }
        }
        std::lock_guard<std::mutex> lock(mu);
        source = std::move(src);
        info = mi;
        plain_audio = plain;
        vtrack = video ? mi.video_track : -1;
        atrack = mi.audio_supported ? mi.audio_track : -1;
        video_on = video;
        audio_on = atrack >= 0;
        video_err = mi.video_error.empty() ? verr : mi.video_error;
        audio_err = mi.audio_error;
        period = mi.duration > 0.0 ? mi.duration : 0.0;
        frame_duration = mi.frame_rate > 0.0 ? 1.0 / mi.frame_rate : 1.0 / 30.0;
        rate = std::min(4.0, std::max(0.25, opts.rate == opts.rate ? opts.rate : 1.0));
        preserve = opts.preserve_pitch;
        volume = std::max(0.0f, opts.volume);
        muted = opts.muted;
        loop = opts.loop;
        loop_flag->store(loop);
        std::size_t frame_bytes = static_cast<std::size_t>(std::max<std::uint32_t>(16, mi.width)) * std::max<std::uint32_t>(16, mi.height) * 3 / 2;
        if (opts.rgba) frame_bytes += static_cast<std::size_t>(std::max<std::uint32_t>(16, mi.width)) * std::max<std::uint32_t>(16, mi.height) * 4;
        max_frames = opts.max_queued_frames ? opts.max_queued_frames : std::min<std::size_t>(12, std::max<std::size_t>(3, (std::size_t(64) << 20) / frame_bytes));
        user_playing = opts.autoplay;
        pinned = true;
        pinned_time = 0.0;
        ended_flag = false;
        seeking = true;
        initial_seek = true;
        first_frame_sent = false;
        waiting = false;
        starve_since = 0;
        this->error.clear();
        vgen = 0;
        ready_gen = 0;
        eos_gen = 0;
        serial = 0;
        presented_gen = 0;
        frames_decoded = frames_presented = frames_dropped = frames_skipped = packets_skipped = decode_errors = 0;
        decode_seconds = convert_seconds = 0.0;
        decode_count = convert_count = 0;
        frames_deinterlaced = 0;
        deinterlacing = false;
        state = PlayerState::paused;
        opened = true;
        cmd_seek = true;
        cmd_target = 0.0;
        cmd_accurate = true;
        cmd_vtrack = -2;
        quit = false;
        pending_decoder = std::move(dec);
        rgba_on = opts.rgba;
        if (!video && !video_err.empty() && mi.video_track >= 0) post(PlayerEventType::warning, 0.0, video_err);
        if (!audio_err.empty()) post(PlayerEventType::warning, 0.0, audio_err);
        post(PlayerEventType::loaded, 0.0);
        worker = std::thread([this]() { worker_loop(); });
        decoder_thread = std::thread([this]() { decode_loop(); });
        return true;
    }
};

MediaPlayer::MediaPlayer() : m_impl(new Impl()) {}

MediaPlayer::MediaPlayer(const PlayerOptions& options) : m_impl(new Impl()) {
    m_impl->opts = options;
}

MediaPlayer::MediaPlayer(audio::Mixer& mixer, const PlayerOptions& options) : m_impl(new Impl()) {
    m_impl->opts = options;
    m_impl->external = &mixer;
}

MediaPlayer::~MediaPlayer() {
    close();
}

void MediaPlayer::set_options(const PlayerOptions& options) {
    std::lock_guard<std::mutex> lock(m_impl->mu);
    m_impl->opts = options;
}

PlayerOptions MediaPlayer::options() const {
    std::lock_guard<std::mutex> lock(m_impl->mu);
    return m_impl->opts;
}

void MediaPlayer::attach(audio::Mixer& mixer) {
    Impl& m = *m_impl;
    std::lock_guard<std::mutex> lock(m.mu);
    if (m.external == &mixer) return;
    m.external = &mixer;
    if (m.opened && m.audio_on) m.request_audio_restart_locked();
}

void MediaPlayer::detach() {
    Impl& m = *m_impl;
    std::lock_guard<std::mutex> lock(m.mu);
    if (!m.external) return;
    m.external = nullptr;
    if (m.opened && m.audio_on) {
        m.request_audio_restart_locked();
    } else if (m.voice_mixer && m.voice) {
        m.voice_mixer->stop(m.voice);
        m.voice = 0;
        m.voice_mixer = nullptr;
        m.audio_master = false;
    }
}

audio::Mixer* MediaPlayer::mixer() const noexcept {
    std::lock_guard<std::mutex> lock(m_impl->mu);
    return m_impl->mixer_locked();
}

bool MediaPlayer::open(const std::string& path, std::string* error) {
    Source s;
    s.path = path;
    return m_impl->open_source(std::move(s), error);
}

bool MediaPlayer::open(const void* data, std::size_t size, std::string* error) {
    if (!data || size == 0) {
        if (error) *error = "no data";
        return false;
    }
    Source s;
    s.memory = std::make_shared<const std::vector<std::uint8_t>>(static_cast<const std::uint8_t*>(data), static_cast<const std::uint8_t*>(data) + size);
    return m_impl->open_source(std::move(s), error);
}

bool MediaPlayer::open_shared(std::shared_ptr<const std::vector<std::uint8_t>> data, std::string* error) {
    if (!data || data->empty()) {
        if (error) *error = "no data";
        return false;
    }
    Source s;
    s.memory = std::move(data);
    return m_impl->open_source(std::move(s), error);
}

void MediaPlayer::close() {
    Impl& m = *m_impl;
    m.shutdown();
    std::lock_guard<std::mutex> lock(m.mu);
    if (m.opened) m.state = PlayerState::closed;
    m.opened = false;
    m.info = MediaInfo();
    m.source = Source();
    m.video_on = false;
    m.audio_on = false;
    m.audio_master = false;
    m.pinned = true;
    m.pinned_time = 0.0;
    m.seeking = false;
    m.cmd_seek = false;
    m.ended_flag = false;
    m.user_playing = false;
    m.pool.clear();
}

bool MediaPlayer::is_open() const noexcept {
    std::lock_guard<std::mutex> lock(m_impl->mu);
    return m_impl->opened;
}

const MediaInfo& MediaPlayer::info() const noexcept {
    return m_impl->info;
}

const std::vector<TrackInfo>& MediaPlayer::tracks() const noexcept {
    return m_impl->info.tracks;
}

double MediaPlayer::duration() const noexcept {
    std::lock_guard<std::mutex> lock(m_impl->mu);
    return m_impl->period;
}

bool MediaPlayer::has_video() const noexcept {
    std::lock_guard<std::mutex> lock(m_impl->mu);
    return m_impl->video_on;
}

bool MediaPlayer::has_audio() const noexcept {
    std::lock_guard<std::mutex> lock(m_impl->mu);
    return m_impl->audio_on;
}

std::uint32_t MediaPlayer::width() const noexcept {
    return m_impl->info.width;
}

std::uint32_t MediaPlayer::height() const noexcept {
    return m_impl->info.height;
}

std::uint32_t MediaPlayer::display_width() const noexcept {
    return m_impl->info.display_width;
}

std::uint32_t MediaPlayer::display_height() const noexcept {
    return m_impl->info.display_height;
}

double MediaPlayer::display_aspect() const noexcept {
    return m_impl->info.display_aspect;
}

int MediaPlayer::rotation() const noexcept {
    return m_impl->info.rotation;
}

double MediaPlayer::frame_rate() const noexcept {
    return m_impl->info.frame_rate;
}

int MediaPlayer::video_track() const noexcept {
    std::lock_guard<std::mutex> lock(m_impl->mu);
    return m_impl->video_on ? m_impl->vtrack : -1;
}

int MediaPlayer::audio_track() const noexcept {
    std::lock_guard<std::mutex> lock(m_impl->mu);
    return m_impl->audio_on ? m_impl->atrack : -1;
}

bool MediaPlayer::select_video_track(int index, std::string* error) {
    Impl& m = *m_impl;
    std::lock_guard<std::mutex> lock(m.mu);
    if (!m.opened) {
        if (error) *error = "nothing is open";
        return false;
    }
    if (index >= 0) {
        if (m.plain_audio || static_cast<std::size_t>(index) >= m.info.tracks.size() || m.info.tracks[static_cast<std::size_t>(index)].type != MediaType::video) {
            if (error) *error = "track " + std::to_string(index) + " is not a video track";
            return false;
        }
        const std::string& codec = m.info.tracks[static_cast<std::size_t>(index)].codec;
        if (!video_codec_supported(codec)) {
            if (error) *error = unsupported_video_codec_message(codec);
            return false;
        }
    } else {
        index = -1;
    }
    if (index == (m.video_on ? m.vtrack : -1)) return true;
    m.cmd_vtrack = index;
    m.request_seek_locked(m.position_from(m.clock_locked()), true);
    m.post(PlayerEventType::track_changed, m.pinned_time, "video");
    return true;
}

bool MediaPlayer::select_audio_track(int index, std::string* error) {
    Impl& m = *m_impl;
    std::lock_guard<std::mutex> lock(m.mu);
    if (!m.opened) {
        if (error) *error = "nothing is open";
        return false;
    }
    if (index >= 0) {
        if (static_cast<std::size_t>(index) >= m.info.tracks.size() || m.info.tracks[static_cast<std::size_t>(index)].type != MediaType::audio) {
            if (error) *error = "track " + std::to_string(index) + " is not an audio track";
            return false;
        }
        const std::string& codec = m.info.tracks[static_cast<std::size_t>(index)].codec;
        if (!m.plain_audio && !audio::container_audio_codec_supported(codec)) {
            if (error) *error = "unsupported audio codec " + codec;
            return false;
        }
    } else {
        index = -1;
    }
    if (index == (m.audio_on ? m.atrack : -1)) return true;
    m.atrack = index;
    m.audio_on = index >= 0;
    m.audio_err.clear();
    m.request_audio_restart_locked();
    m.post(PlayerEventType::track_changed, m.pinned_time, "audio");
    return true;
}

std::string MediaPlayer::video_error() const {
    std::lock_guard<std::mutex> lock(m_impl->mu);
    return m_impl->video_err;
}

std::string MediaPlayer::audio_error() const {
    std::lock_guard<std::mutex> lock(m_impl->mu);
    return m_impl->audio_err;
}

void MediaPlayer::play() {
    Impl& m = *m_impl;
    std::lock_guard<std::mutex> lock(m.mu);
    if (!m.opened) return;
    if (m.user_playing && !m.ended_flag) return;
    m.user_playing = true;
    if (m.ended_flag) {
        m.ended_flag = false;
        m.request_seek_locked(0.0, true);
        m.state = PlayerState::playing;
        m.post(PlayerEventType::play, 0.0);
        return;
    }
    if (m.initial_seek) {
        m.state = PlayerState::playing;
        return;
    }
    m.post(PlayerEventType::play, m.position_from(m.pinned_time));
    m.state = PlayerState::playing;
    if (!m.seeking && !m.cmd_seek && !m.restarting) {
        m.resume_locked();
        m.post(PlayerEventType::playing, m.position_from(m.pinned_time));
    }
    m.cv_worker.notify_all();
}

void MediaPlayer::pause() {
    Impl& m = *m_impl;
    std::lock_guard<std::mutex> lock(m.mu);
    if (!m.opened || !m.user_playing) return;
    m.user_playing = false;
    if (!m.seeking && !m.cmd_seek) m.pause_locked();
    if (m.state != PlayerState::ended) m.state = PlayerState::paused;
    m.post(PlayerEventType::pause, m.position_from(m.pinned_time));
}

void MediaPlayer::stop() {
    Impl& m = *m_impl;
    std::lock_guard<std::mutex> lock(m.mu);
    if (!m.opened) return;
    const bool was = m.user_playing;
    m.user_playing = false;
    if (!m.seeking && !m.cmd_seek) m.pause_locked();
    m.state = PlayerState::paused;
    if (was) m.post(PlayerEventType::pause, m.position_from(m.pinned_time));
    m.request_seek_locked(0.0, true);
    m.post(PlayerEventType::seeking, 0.0);
}

bool MediaPlayer::seek(double seconds, bool accurate) {
    Impl& m = *m_impl;
    std::lock_guard<std::mutex> lock(m.mu);
    if (!m.opened || !(seconds == seconds)) return false;
    if (seconds < 0.0) seconds = 0.0;
    if (m.period > 0.0 && seconds > m.period) seconds = m.period;
    if (m.state == PlayerState::ended) m.state = PlayerState::paused;
    m.request_seek_locked(seconds, accurate);
    m.post(PlayerEventType::seeking, seconds);
    return true;
}

double MediaPlayer::position() const {
    Impl& m = *m_impl;
    std::lock_guard<std::mutex> lock(m.mu);
    if (!m.opened) return 0.0;
    return m.position_from(m.clock_locked());
}

PlayerState MediaPlayer::state() const noexcept {
    std::lock_guard<std::mutex> lock(m_impl->mu);
    return m_impl->state;
}

bool MediaPlayer::is_playing() const noexcept {
    std::lock_guard<std::mutex> lock(m_impl->mu);
    return m_impl->opened && m_impl->user_playing && !m_impl->ended_flag;
}

bool MediaPlayer::is_paused() const noexcept {
    std::lock_guard<std::mutex> lock(m_impl->mu);
    return !m_impl->user_playing || m_impl->ended_flag;
}

bool MediaPlayer::is_seeking() const noexcept {
    std::lock_guard<std::mutex> lock(m_impl->mu);
    return m_impl->seeking || m_impl->cmd_seek;
}

bool MediaPlayer::is_buffering() const noexcept {
    std::lock_guard<std::mutex> lock(m_impl->mu);
    return m_impl->seeking || m_impl->cmd_seek || m_impl->cmd_audio || m_impl->restarting || m_impl->waiting;
}

bool MediaPlayer::ended() const noexcept {
    std::lock_guard<std::mutex> lock(m_impl->mu);
    return m_impl->ended_flag;
}

std::string MediaPlayer::error() const {
    std::lock_guard<std::mutex> lock(m_impl->mu);
    return m_impl->error;
}

void MediaPlayer::set_loop(bool loop) {
    Impl& m = *m_impl;
    std::lock_guard<std::mutex> lock(m.mu);
    m.opts.loop = loop;
    if (m.loop == loop) return;
    m.loop = loop;
    m.loop_flag->store(loop);
    if (loop && m.period > 0.0) m.last_cycle = static_cast<long long>(std::floor(std::max(0.0, m.clock_locked()) / m.period));
    if (loop && m.demux_eos && m.opened && !m.ended_flag) {
        m.request_seek_locked(m.position_from(m.clock_locked()), true);
    }
}

bool MediaPlayer::loop() const noexcept {
    std::lock_guard<std::mutex> lock(m_impl->mu);
    return m_impl->loop;
}

bool MediaPlayer::set_rate(double rate) {
    Impl& m = *m_impl;
    std::lock_guard<std::mutex> lock(m.mu);
    if (!(rate == rate) || rate <= 0.0) return false;
    rate = std::min(4.0, std::max(0.25, rate));
    m.opts.rate = rate;
    if (rate == m.rate) return true;
    if (!m.pinned && !m.audio_master) {
        m.base_time = m.clock_locked();
        m.base_ns = now_ns();
    }
    m.rate = rate;
    if (m.audio_on && (m.voice || m.audio_master)) m.request_audio_restart_locked();
    m.post(PlayerEventType::rate_changed, m.position_from(m.clock_locked()));
    return true;
}

double MediaPlayer::rate() const noexcept {
    std::lock_guard<std::mutex> lock(m_impl->mu);
    return m_impl->rate;
}

void MediaPlayer::set_preserve_pitch(bool preserve) {
    Impl& m = *m_impl;
    std::lock_guard<std::mutex> lock(m.mu);
    m.opts.preserve_pitch = preserve;
    if (m.preserve == preserve) return;
    m.preserve = preserve;
    if (m.voice && std::fabs(m.rate - 1.0) > 1e-9) m.request_audio_restart_locked();
}

bool MediaPlayer::preserve_pitch() const noexcept {
    std::lock_guard<std::mutex> lock(m_impl->mu);
    return m_impl->preserve;
}

void MediaPlayer::set_volume(float volume) {
    Impl& m = *m_impl;
    std::lock_guard<std::mutex> lock(m.mu);
    if (!(volume == volume)) return;
    volume = std::max(0.0f, volume);
    m.opts.volume = volume;
    if (m.volume == volume) return;
    m.volume = volume;
    if (m.voice_mixer && m.voice) m.voice_mixer->set_gain(m.voice, m.muted ? 0.0f : volume);
    m.post(PlayerEventType::volume_changed, m.position_from(m.clock_locked()));
}

float MediaPlayer::volume() const noexcept {
    std::lock_guard<std::mutex> lock(m_impl->mu);
    return m_impl->volume;
}

void MediaPlayer::set_muted(bool muted) {
    Impl& m = *m_impl;
    std::lock_guard<std::mutex> lock(m.mu);
    m.opts.muted = muted;
    if (m.muted == muted) return;
    m.muted = muted;
    if (m.voice_mixer && m.voice) m.voice_mixer->set_gain(m.voice, muted ? 0.0f : m.volume);
    m.post(PlayerEventType::volume_changed, m.position_from(m.clock_locked()));
}

void MediaPlayer::set_deinterlace(DeinterlaceSetting setting) {
    std::lock_guard<std::mutex> lock(m_impl->mu);
    m_impl->opts.deinterlace = setting;
}

void MediaPlayer::set_deinterlace_mode(DeinterlaceMode mode, bool field_rate) {
    std::lock_guard<std::mutex> lock(m_impl->mu);
    m_impl->opts.deinterlace_mode = mode;
    m_impl->opts.deinterlace_field_rate = field_rate;
}

DeinterlaceSetting MediaPlayer::deinterlace() const noexcept {
    std::lock_guard<std::mutex> lock(m_impl->mu);
    return m_impl->opts.deinterlace;
}

DeinterlaceMode MediaPlayer::deinterlace_mode() const noexcept {
    std::lock_guard<std::mutex> lock(m_impl->mu);
    return m_impl->opts.deinterlace_mode;
}

bool MediaPlayer::deinterlace_field_rate() const noexcept {
    std::lock_guard<std::mutex> lock(m_impl->mu);
    return m_impl->opts.deinterlace_field_rate;
}

bool MediaPlayer::is_deinterlacing() const noexcept {
    std::lock_guard<std::mutex> lock(m_impl->mu);
    return m_impl->deinterlacing;
}

bool MediaPlayer::muted() const noexcept {
    std::lock_guard<std::mutex> lock(m_impl->mu);
    return m_impl->muted;
}

std::vector<PlayerEvent> MediaPlayer::poll_events() {
    std::lock_guard<std::mutex> lock(m_impl->ev_mu);
    std::vector<PlayerEvent> out(m_impl->events.begin(), m_impl->events.end());
    m_impl->events.clear();
    return out;
}

void MediaPlayer::set_event_callback(EventCallback callback) {
    std::lock_guard<std::mutex> lock(m_impl->ev_mu);
    m_impl->callback = std::move(callback);
    if (!m_impl->callback) m_impl->dispatch.clear();
}

bool MediaPlayer::current_frame(const VideoFrame*& frame, std::uint64_t& serial) {
    Impl& m = *m_impl;
    std::lock_guard<std::mutex> lock(m.mu);
    if (m.opened && m.video_on) m.present_locked(m.clock_locked() + m.opts.presentation_offset);
    else if (m.presented && !m.seeking && !m.cmd_seek) m.drop_presented_locked();
    frame = m.presented ? &m.presented->frame : nullptr;
    const bool changed = serial != m.serial;
    serial = m.serial;
    return changed;
}

bool MediaPlayer::current_rgba(const std::uint8_t*& rgba, std::uint32_t& width, std::uint32_t& height, std::uint64_t& serial) {
    Impl& m = *m_impl;
    std::lock_guard<std::mutex> lock(m.mu);
    if (m.opened && m.video_on) m.present_locked(m.clock_locked() + m.opts.presentation_offset);
    else if (m.presented && !m.seeking && !m.cmd_seek) m.drop_presented_locked();
    const bool changed = serial != m.serial;
    serial = m.serial;
    if (!m.presented || m.presented->frame.empty()) {
        rgba = nullptr;
        width = 0;
        height = 0;
        return changed;
    }
    QFrame& q = *m.presented;
    const std::size_t bytes = static_cast<std::size_t>(q.frame.width) * q.frame.height * 4;
    if (q.rgba.size() != bytes) {
        q.rgba.resize(bytes);
        to_rgba(q.frame, q.rgba.data(), static_cast<std::size_t>(q.frame.width) * 4);
    }
    rgba = q.rgba.data();
    width = q.frame.width;
    height = q.frame.height;
    return changed;
}

bool MediaPlayer::frame_rgba(std::vector<std::uint8_t>& rgba, std::uint32_t& width, std::uint32_t& height, std::uint64_t& serial) {
    const std::uint8_t* p = nullptr;
    const std::uint64_t before = serial;
    std::uint64_t now = serial;
    current_rgba(p, width, height, now);
    if (now == before && !rgba.empty()) return false;
    serial = now;
    if (!p) {
        rgba.clear();
        return now != before;
    }
    std::lock_guard<std::mutex> lock(m_impl->mu);
    if (!m_impl->presented) return false;
    rgba.assign(p, p + static_cast<std::size_t>(width) * height * 4);
    return true;
}

std::uint64_t MediaPlayer::frame_serial() const noexcept {
    std::lock_guard<std::mutex> lock(m_impl->mu);
    return m_impl->serial;
}

double MediaPlayer::frame_time() const noexcept {
    std::lock_guard<std::mutex> lock(m_impl->mu);
    return m_impl->presented ? m_impl->position_from(m_impl->presented->t) : -1.0;
}

double MediaPlayer::next_frame_delay() const {
    Impl& m = *m_impl;
    std::lock_guard<std::mutex> lock(m.mu);
    if (!m.opened || !m.video_on || m.frames.empty() || m.frames[0]->gen != m.vgen) return -1.0;
    if (m.pinned) return m.presented && m.presented_gen == m.vgen ? -1.0 : 0.0;
    const double u = m.clock_locked() + m.opts.presentation_offset;
    const double d = (m.frames[0]->t - u) / std::max(0.01, m.rate);
    return d < 0.0 ? 0.0 : d;
}

PlayerStats MediaPlayer::stats() const {
    Impl& m = *m_impl;
    std::lock_guard<std::mutex> lock(m.mu);
    PlayerStats s;
    s.frames_decoded = m.frames_decoded;
    s.frames_presented = m.frames_presented;
    s.frames_dropped = m.frames_dropped;
    s.frames_skipped = m.frames_skipped;
    s.packets_skipped = m.packets_skipped;
    s.decode_errors = m.decode_errors;
    s.queued_packets = m.packets.size();
    s.queued_frames = m.frames.size();
    s.average_decode_ms = m.decode_count ? m.decode_seconds * 1000.0 / static_cast<double>(m.decode_count) : 0.0;
    s.average_convert_ms = m.convert_count ? m.convert_seconds * 1000.0 / static_cast<double>(m.convert_count) : 0.0;
    s.audio_clock = m.audio_master;
    s.frames_deinterlaced = m.frames_deinterlaced;
    s.deinterlacing = m.deinterlacing;
    if (m.voice_mixer && m.voice) {
        s.audio_underruns = m.voice_mixer->stream_underruns(m.voice);
        s.audio_buffered = m.voice_mixer->buffered_seconds(m.voice);
    }
    return s;
}

} // namespace video
} // namespace fizmo
