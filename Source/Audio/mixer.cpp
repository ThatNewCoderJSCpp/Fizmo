#include "mixer.hpp"
#include "fft.hpp"
#include "resampler.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <cstring>
#include <thread>
#include <utility>

namespace fizmo {
namespace audio {

namespace {

struct StreamSource {
    std::unique_ptr<AudioReader> reader;
    std::uint32_t out_rate = 48000;
    std::size_t out_channels = 2;
    std::uint32_t src_rate = 48000;
    std::size_t src_channels = 2;
    double duration = 0.0;

    std::unique_ptr<float[]> ring;
    std::size_t capacity = 0;
    std::size_t mask = 0;
    std::atomic<std::uint64_t> write{ 0 };
    std::atomic<std::uint64_t> read{ 0 };

    std::atomic<std::uint64_t> seg_gen{ 1 };
    std::atomic<std::uint64_t> seg_start{ 0 };
    std::atomic<double> seg_seconds{ 0.0 };
    std::atomic<std::uint64_t> eof_gen{ 0 };
    std::atomic<std::uint64_t> eof_at{ 0 };

    std::atomic<std::uint64_t> req_gen{ 1 };
    std::atomic<double> req_seconds{ 0.0 };
    std::atomic<bool> loop{ false };
    std::atomic<bool> done{ false };
    std::atomic<std::uint64_t> underruns{ 0 };

    std::uint64_t handled_gen = 1;
    bool resample = false;
    Resampler resampler;
    std::vector<float> decoded;
    std::vector<float> remixed;
    std::vector<float> pending;
    std::size_t pending_pos = 0;
    bool reader_eof = false;
    bool eof_published = false;

    std::size_t buffered() const noexcept {
        return static_cast<std::size_t>(write.load(std::memory_order_acquire) - read.load(std::memory_order_acquire));
    }
};

const std::size_t k_decode_chunk = 4096;

void restart_decoder(StreamSource& s, double seconds) {
    if (seconds <= 0.0) {
        s.reader->seek(0);
    } else {
        s.reader->seek_seconds(seconds);
    }
    if (s.resample) s.resampler.reset();
    s.pending.clear();
    s.pending_pos = 0;
    s.reader_eof = false;
    s.eof_published = false;
}

bool service_stream(StreamSource& s) {
    bool work = false;
    const std::uint64_t req = s.req_gen.load(std::memory_order_acquire);
    if (req != s.handled_gen) {
        double sec = std::max(0.0, s.req_seconds.load(std::memory_order_acquire));
        if (s.duration > 0.0 && sec > s.duration) sec = s.duration;
        restart_decoder(s, sec);
        s.eof_gen.store(0, std::memory_order_release);
        s.seg_start.store(s.write.load(std::memory_order_relaxed), std::memory_order_relaxed);
        s.seg_seconds.store(sec, std::memory_order_relaxed);
        s.seg_gen.store(req, std::memory_order_release);
        s.handled_gen = req;
        work = true;
    }
    if (s.eof_published) {
        if (!s.loop.load(std::memory_order_relaxed)) return work;
        s.eof_gen.store(0, std::memory_order_release);
        s.reader->seek(0);
        s.reader_eof = false;
        s.eof_published = false;
        work = true;
    }
    const std::size_t oc = s.out_channels;
    while (true) {
        const std::uint64_t w = s.write.load(std::memory_order_relaxed);
        const std::size_t used = static_cast<std::size_t>(w - s.read.load(std::memory_order_acquire));
        const std::size_t free = used < s.capacity ? s.capacity - used : 0;
        if (s.pending_pos < s.pending.size()) {
            const std::size_t have = (s.pending.size() - s.pending_pos) / oc;
            const std::size_t n = std::min(free, have);
            if (n == 0) break;
            const std::size_t at = static_cast<std::size_t>(w) & s.mask;
            const std::size_t first = std::min(n, s.capacity - at);
            std::memcpy(s.ring.get() + at * oc, s.pending.data() + s.pending_pos, first * oc * sizeof(float));
            if (n > first) std::memcpy(s.ring.get(), s.pending.data() + s.pending_pos + first * oc, (n - first) * oc * sizeof(float));
            s.pending_pos += n * oc;
            s.write.store(w + n, std::memory_order_release);
            work = true;
            continue;
        }
        s.pending.clear();
        s.pending_pos = 0;
        if (s.reader_eof) {
            if (!s.eof_published) {
                s.eof_at.store(w, std::memory_order_relaxed);
                s.eof_gen.store(s.handled_gen, std::memory_order_release);
                s.eof_published = true;
                work = true;
            }
            break;
        }
        if (free < std::min<std::size_t>(s.capacity / 4, 8192)) break;
        std::size_t n = s.reader->read(s.decoded.data(), k_decode_chunk);
        if (n == 0 && s.loop.load(std::memory_order_relaxed) && s.reader->seek(0)) n = s.reader->read(s.decoded.data(), k_decode_chunk);
        if (n == 0) {
            if (s.resample) s.resampler.flush(s.pending);
            s.reader_eof = true;
            work = true;
            continue;
        }
        const float* src = s.decoded.data();
        if (s.src_channels != oc) {
            remix(s.decoded.data(), s.src_channels, s.remixed.data(), oc, n);
            src = s.remixed.data();
        }
        if (s.resample) {
            s.resampler.process(src, n, s.pending);
        } else {
            s.pending.assign(src, src + n * oc);
        }
        work = true;
    }
    return work;
}

} // namespace

struct Mixer::Streamer {
    std::mutex mutex;
    std::condition_variable cv;
    std::vector<std::shared_ptr<StreamSource>> sources;
    std::thread thread;
    bool running = false;
    bool quit = false;

    void loop() {
        std::vector<std::shared_ptr<StreamSource>> local;
        while (true) {
            {
                std::unique_lock<std::mutex> lock(mutex);
                if (quit) break;
                sources.erase(std::remove_if(sources.begin(), sources.end(), [](const std::shared_ptr<StreamSource>& s) { return s->done.load(); }), sources.end());
                local = sources;
            }
            bool work = false;
            for (const std::shared_ptr<StreamSource>& s : local) {
                if (!s->done.load(std::memory_order_relaxed)) work = service_stream(*s) || work;
            }
            local.clear();
            if (!work) {
                std::unique_lock<std::mutex> lock(mutex);
                if (quit) break;
                cv.wait_for(lock, std::chrono::milliseconds(4));
            }
        }
    }

    void add(std::shared_ptr<StreamSource> s) {
        std::lock_guard<std::mutex> lock(mutex);
        sources.push_back(std::move(s));
        if (!running) {
            running = true;
            quit = false;
            thread = std::thread([this]() { loop(); });
        }
        cv.notify_one();
    }

    void wake() {
        cv.notify_one();
    }

    void stop() {
        {
            std::lock_guard<std::mutex> lock(mutex);
            quit = true;
        }
        cv.notify_one();
        if (thread.joinable()) thread.join();
        std::lock_guard<std::mutex> lock(mutex);
        running = false;
        sources.clear();
    }

    ~Streamer() {
        stop();
    }
};

struct Mixer::Voice {
    VoiceId id = 0;
    Sound sound;
    std::atomic<float> gain{ 1.0f };
    std::atomic<float> pan{ 0.0f };
    std::atomic<float> speed{ 1.0f };
    std::atomic<bool> loop{ false };
    std::atomic<bool> paused{ false };
    std::atomic<bool> stop_requested{ false };
    std::atomic<bool> finished{ false };
    std::atomic<double> fade_out_seconds{ 0.0 };
    std::atomic<double> seek_to{ -1.0 };
    std::atomic<double> position{ 0.0 };

    double pos = 0.0;
    float current_gain = 1.0f;
    double fade_in_total = 0.0;
    double fade_in_left = 0.0;
    double fade_out_total = 0.0;
    double fade_out_left = 0.0;
    bool fading_out = false;

    bool path = false;
    std::atomic<float> pitch{ 0.0f };
    std::atomic<bool> preserve{ false };
    std::unique_ptr<TimeStretcher> stretcher;
    std::shared_ptr<StreamSource> stream;
    std::uint32_t feed_rate = 48000;
    std::size_t feed_channels = 2;
    std::size_t feed_cap = 0;
    std::vector<float> feed;
    std::size_t feed_read = 0;
    std::size_t feed_count = 0;
    bool feed_ended = false;
    std::vector<float> scratch;
    std::size_t scratch_cap = 0;
    std::vector<float> hist;
    std::vector<float> frame;
    std::size_t primed = 0;
    std::size_t pads = 0;
    double frac = 0.0;
    double play_pos = 0.0;
    std::size_t src_index = 0;
    bool source_ended = false;
    std::uint64_t seg_gen = 1;
    double seg_seconds = 0.0;
    bool waiting = false;
    bool starving = false;
    std::size_t prefill = 0;

    std::atomic<std::uint32_t> clock_seq{ 0 };
    std::atomic<double> clock_pos{ 0.0 };
    std::atomic<double> clock_rate{ 0.0 };
    std::atomic<double> clock_floor{ 0.0 };
    std::atomic<std::int64_t> clock_ns{ 0 };
    bool clock_active = false;

    ~Voice() {
        if (stream) stream->done.store(true);
    }
};

namespace {

std::int64_t steady_ns() noexcept {
    return static_cast<std::int64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count());
}

} // namespace

void Mixer::publish_clock(Voice& v, double before, double after) noexcept {
    double rate = 0.0;
    if (m_block_seconds > 0.0 && after > before) rate = (after - before) / m_block_seconds;
    const std::uint32_t seq = v.clock_seq.load(std::memory_order_relaxed);
    v.clock_seq.store(seq + 1, std::memory_order_relaxed);
    std::atomic_thread_fence(std::memory_order_release);
    if (!v.clock_active || rate <= 0.0) v.clock_floor.store(rate > 0.0 ? before : after, std::memory_order_relaxed);
    v.clock_pos.store(after, std::memory_order_relaxed);
    v.clock_rate.store(rate, std::memory_order_relaxed);
    v.clock_ns.store(m_render_ns, std::memory_order_relaxed);
    v.clock_seq.store(seq + 2, std::memory_order_release);
    v.clock_active = rate > 0.0;
}

Sound make_sound(AudioBuffer buffer) {
    return std::make_shared<const AudioBuffer>(std::move(buffer));
}

Mixer::Mixer(std::uint32_t sample_rate, std::size_t channels)
    : m_rate(sample_rate ? sample_rate : 48000), m_channels(channels ? std::min<std::size_t>(channels, 64) : 2) {
    m_frame_in.assign(64, 0.0f);
    m_frame_out.assign(64, 0.0f);
    m_active.reserve(256);
    m_pending.reserve(64);
    m_finished.reserve(256);
}

Mixer::~Mixer() {
    stop_output();
    if (m_streamer) m_streamer->stop();
}

void Mixer::collect() const {
    for (const std::shared_ptr<Voice>& v : m_finished) m_voices.erase(v->id);
    m_finished.clear();
    if (m_finished.capacity() < m_voices.size() + 64) m_finished.reserve(m_voices.size() + 64);
}

std::shared_ptr<Mixer::Voice> Mixer::find(VoiceId id) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    collect();
    auto it = m_voices.find(id);
    return it == m_voices.end() ? nullptr : it->second;
}

Mixer::VoiceId Mixer::add_voice(std::shared_ptr<Voice> v) {
    v->id = m_next.fetch_add(1);
    std::lock_guard<std::mutex> lock(m_mutex);
    collect();
    m_voices.emplace(v->id, v);
    m_pending.push_back(v);
    return v->id;
}

Mixer::VoiceId Mixer::play(Sound sound, const PlayOptions& options) {
    if (!sound || sound->empty() || sound->channels() == 0 || sound->sample_rate() == 0) return 0;
    auto v = std::make_shared<Voice>();
    v->gain.store(options.gain);
    v->pan.store(std::max(-1.0f, std::min(1.0f, options.pan)));
    v->speed.store(options.speed > 0.0f ? options.speed : 1.0f);
    v->loop.store(options.loop);
    v->paused.store(options.paused);
    v->pos = std::max(0.0, options.start) * static_cast<double>(sound->sample_rate());
    v->position.store(std::max(0.0, options.start));
    v->current_gain = options.gain;
    v->fade_in_total = std::max(0.0, options.fade_in) * static_cast<double>(m_rate);
    v->fade_in_left = v->fade_in_total;
    if (options.preserve_pitch || options.pitch != 0.0f || options.stretchable) {
        v->path = true;
        v->pitch.store(options.pitch == options.pitch ? options.pitch : 0.0f);
        v->preserve.store(options.preserve_pitch);
        v->feed_rate = sound->sample_rate();
        v->feed_channels = sound->channels();
        v->stretcher.reset(new TimeStretcher(v->feed_channels, v->feed_rate, options.stretch_mode, options.stretch_quality, 1024));
        v->feed_cap = 512;
        v->feed.assign(v->feed_cap * v->feed_channels, 0.0f);
        v->scratch_cap = 1024;
        v->scratch.assign(v->scratch_cap * v->feed_channels, 0.0f);
        v->hist.assign(4 * v->feed_channels, 0.0f);
        v->frame.assign(std::max<std::size_t>(v->feed_channels, 64), 0.0f);
        v->src_index = std::min(sound->frames(), static_cast<std::size_t>(v->pos));
        v->play_pos = static_cast<double>(v->src_index);
    }
    v->sound = std::move(sound);
    return add_voice(std::move(v));
}

Mixer::VoiceId Mixer::play(const AudioBuffer& buffer, const PlayOptions& options) {
    return play(make_sound(buffer), options);
}

Mixer::VoiceId Mixer::play_file(const std::string& path, const PlayOptions& options, std::string* error) {
    std::unique_ptr<AudioReader> reader(new AudioReader());
    if (!reader->open(path, error)) return 0;
    return play_stream(std::move(reader), options, error);
}

Mixer::VoiceId Mixer::play_stream(std::unique_ptr<AudioReader> reader, const PlayOptions& options, std::string* error) {
    if (!reader || !reader->is_open()) {
        if (error) *error = "stream is not open";
        return 0;
    }
    const AudioFileInfo& info = reader->info();
    if (info.sample_rate == 0 || info.channels == 0) {
        if (error) *error = "stream has no audio format";
        return 0;
    }
    auto s = std::make_shared<StreamSource>();
    s->src_rate = info.sample_rate;
    s->src_channels = info.channels;
    s->out_rate = m_rate;
    s->out_channels = m_channels;
    s->duration = info.duration > 0.0 ? info.duration : (info.frames ? static_cast<double>(info.frames) / static_cast<double>(info.sample_rate) : 0.0);
    s->reader = std::move(reader);
    const double buffer_seconds = std::min(30.0, std::max(0.25, options.stream_buffer_seconds));
    s->capacity = next_power_of_two(static_cast<std::size_t>(buffer_seconds * static_cast<double>(m_rate)));
    s->mask = s->capacity - 1;
    s->ring.reset(new float[s->capacity * m_channels]);
    std::fill(s->ring.get(), s->ring.get() + s->capacity * m_channels, 0.0f);
    s->resample = s->src_rate != m_rate;
    if (s->resample) s->resampler.configure(m_channels, static_cast<double>(s->src_rate), static_cast<double>(m_rate), ResampleQuality::high);
    s->decoded.assign(k_decode_chunk * s->src_channels, 0.0f);
    s->remixed.assign(k_decode_chunk * m_channels, 0.0f);
    s->pending.reserve((k_decode_chunk * 4 + 1024) * m_channels);
    s->loop.store(options.loop);
    const double start = std::max(0.0, options.start);
    s->req_seconds.store(start);
    s->seg_seconds.store(start);
    if (start > 0.0) restart_decoder(*s, start);
    const std::size_t prefill = std::min(s->capacity / 2, static_cast<std::size_t>(0.5 * static_cast<double>(m_rate)));
    for (int guard = 0; guard < 100000; ++guard) {
        service_stream(*s);
        if (s->buffered() >= prefill || s->eof_published) break;
    }

    auto v = std::make_shared<Voice>();
    v->gain.store(options.gain);
    v->pan.store(std::max(-1.0f, std::min(1.0f, options.pan)));
    v->speed.store(options.speed > 0.0f ? options.speed : 1.0f);
    v->loop.store(options.loop);
    v->paused.store(options.paused);
    v->position.store(start);
    v->current_gain = options.gain;
    v->fade_in_total = std::max(0.0, options.fade_in) * static_cast<double>(m_rate);
    v->fade_in_left = v->fade_in_total;
    v->path = true;
    v->pitch.store(options.pitch == options.pitch ? options.pitch : 0.0f);
    v->preserve.store(options.preserve_pitch);
    v->feed_rate = m_rate;
    v->feed_channels = m_channels;
    if (options.preserve_pitch || options.pitch != 0.0f || options.stretchable) {
        v->stretcher.reset(new TimeStretcher(m_channels, m_rate, options.stretch_mode, options.stretch_quality, 1024));
    }
    v->feed_cap = 512;
    v->feed.assign(v->feed_cap * m_channels, 0.0f);
    v->scratch_cap = 1024;
    v->scratch.assign(v->scratch_cap * m_channels, 0.0f);
    v->hist.assign(4 * m_channels, 0.0f);
    v->frame.assign(64, 0.0f);
    v->seg_gen = 1;
    v->seg_seconds = start;
    v->prefill = std::min(s->capacity / 4, static_cast<std::size_t>(0.15 * static_cast<double>(m_rate)));
    v->stream = s;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (!m_streamer) m_streamer.reset(new Streamer());
    }
    m_streamer->add(s);
    return add_voice(std::move(v));
}

void Mixer::stop(VoiceId id, double fade_out) {
    if (auto v = find(id)) {
        v->fade_out_seconds.store(std::max(0.0, fade_out));
        v->stop_requested.store(true);
    }
}

void Mixer::stop_all(double fade_out) {
    std::lock_guard<std::mutex> lock(m_mutex);
    collect();
    for (auto& kv : m_voices) {
        kv.second->fade_out_seconds.store(std::max(0.0, fade_out));
        kv.second->stop_requested.store(true);
    }
}

void Mixer::set_gain(VoiceId id, float gain) {
    if (auto v = find(id)) v->gain.store(gain);
}

void Mixer::set_pan(VoiceId id, float pan) {
    if (auto v = find(id)) v->pan.store(std::max(-1.0f, std::min(1.0f, pan)));
}

void Mixer::set_speed(VoiceId id, float speed) {
    if (auto v = find(id)) v->speed.store(speed > 0.0f ? speed : 1.0f);
}

void Mixer::set_loop(VoiceId id, bool loop) {
    if (auto v = find(id)) {
        v->loop.store(loop);
        if (v->stream) {
            v->stream->loop.store(loop);
            if (m_streamer) m_streamer->wake();
        }
    }
}

void Mixer::set_paused(VoiceId id, bool paused) {
    if (auto v = find(id)) v->paused.store(paused);
}

bool Mixer::seek(VoiceId id, double seconds) {
    auto v = find(id);
    if (!v || v->finished.load()) return false;
    if (v->stream) {
        v->stream->req_seconds.store(std::max(0.0, seconds), std::memory_order_release);
        v->stream->req_gen.fetch_add(1, std::memory_order_acq_rel);
        if (m_streamer) m_streamer->wake();
        return true;
    }
    v->seek_to.store(std::max(0.0, seconds));
    return true;
}

bool Mixer::set_pitch(VoiceId id, float semitones) {
    auto v = find(id);
    if (!v || !v->stretcher || !(semitones == semitones)) return false;
    v->pitch.store(semitones);
    return true;
}

bool Mixer::set_preserve_pitch(VoiceId id, bool preserve) {
    auto v = find(id);
    if (!v || !v->stretcher) return false;
    v->preserve.store(preserve);
    return true;
}

bool Mixer::is_playing(VoiceId id) const {
    auto v = find(id);
    return v && !v->finished.load();
}

bool Mixer::is_paused(VoiceId id) const {
    auto v = find(id);
    return v && !v->finished.load() && v->paused.load();
}

double Mixer::position(VoiceId id) const {
    auto v = find(id);
    return v ? v->position.load() : 0.0;
}

double Mixer::duration(VoiceId id) const {
    auto v = find(id);
    if (!v) return 0.0;
    if (v->stream) return v->stream->duration;
    return v->sound ? v->sound->duration() : 0.0;
}

std::size_t Mixer::voice_count() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    collect();
    std::size_t n = 0;
    for (const auto& kv : m_voices) {
        if (!kv.second->finished.load()) ++n;
    }
    return n;
}

bool Mixer::is_streaming(VoiceId id) const {
    auto v = find(id);
    return v && v->stream;
}

bool Mixer::is_stretched(VoiceId id) const {
    auto v = find(id);
    return v && v->stretcher;
}

float Mixer::pitch(VoiceId id) const {
    auto v = find(id);
    return v ? v->pitch.load() : 0.0f;
}

std::uint64_t Mixer::stream_underruns(VoiceId id) const {
    auto v = find(id);
    return v && v->stream ? v->stream->underruns.load() : 0;
}

double Mixer::buffered_seconds(VoiceId id) const {
    auto v = find(id);
    if (!v || !v->stream) return 0.0;
    const StreamSource& s = *v->stream;
    const std::uint64_t gen = s.seg_gen.load(std::memory_order_acquire);
    if (s.req_gen.load(std::memory_order_acquire) != gen) return 0.0;
    const std::uint64_t w = s.write.load(std::memory_order_acquire);
    std::uint64_t r = s.read.load(std::memory_order_acquire);
    const std::uint64_t start = s.seg_start.load(std::memory_order_relaxed);
    if (s.seg_gen.load(std::memory_order_acquire) == gen && start > r) r = start;
    return w > r ? static_cast<double>(w - r) / static_cast<double>(m_rate) : 0.0;
}

void Mixer::render_voice(Voice& v, float* out, std::size_t frames) noexcept {
    const AudioBuffer& src = *v.sound;
    const std::size_t sc = src.channels();
    const std::size_t mc = m_channels;
    const std::size_t total = src.frames();
    const double src_rate = static_cast<double>(src.sample_rate());
    const double seek = v.seek_to.exchange(-1.0);
    if (seek >= 0.0) v.pos = seek * src_rate;
    if (v.stop_requested.load(std::memory_order_relaxed) && !v.fading_out) {
        const double fade = v.fade_out_seconds.load();
        if (fade <= 0.0 || v.paused.load()) {
            v.finished.store(true);
            return;
        }
        v.fading_out = true;
        v.fade_out_total = fade * static_cast<double>(m_rate);
        v.fade_out_left = v.fade_out_total;
    }
    if (v.paused.load(std::memory_order_relaxed)) {
        if (v.clock_active) publish_clock(v, v.pos / src_rate, v.pos / src_rate);
        return;
    }
    const double clock_before = v.pos / src_rate;
    const double step = src_rate / static_cast<double>(m_rate) * static_cast<double>(std::max(0.01f, v.speed.load(std::memory_order_relaxed)));
    const float target = v.gain.load(std::memory_order_relaxed);
    const float pan = v.pan.load(std::memory_order_relaxed);
    const bool loop = v.loop.load(std::memory_order_relaxed);
    const float pan_l = mc == 2 ? std::min(1.0f, 1.0f - pan) : 1.0f;
    const float pan_r = mc == 2 ? std::min(1.0f, 1.0f + pan) : 1.0f;
    const float smooth = 1.0f - std::exp(-1.0f / (0.005f * static_cast<float>(m_rate)));
    const float* data = src.data();
    const long long last = static_cast<long long>(total) - 1;
    auto at = [&](long long i, std::size_t c) -> float {
        if (i < 0) i = loop ? i + static_cast<long long>(total) : 0;
        else if (i > last) i = loop ? i - static_cast<long long>(total) : last;
        if (i < 0 || i > last) return 0.0f;
        return data[static_cast<std::size_t>(i) * sc + c];
    };
    float* in = m_frame_in.data();
    float* mixed = m_frame_out.data();
    for (std::size_t f = 0; f < frames; ++f) {
        if (v.pos >= static_cast<double>(total)) {
            if (loop && total > 0) {
                v.pos = std::fmod(v.pos, static_cast<double>(total));
            } else {
                v.finished.store(true);
                break;
            }
        }
        const double fl = std::floor(v.pos);
        const long long i = static_cast<long long>(fl);
        const float t = static_cast<float>(v.pos - fl);
        const std::size_t keep = std::min<std::size_t>(sc, 64);
        for (std::size_t c = 0; c < keep; ++c) {
            if (t == 0.0f) {
                in[c] = at(i, c);
                continue;
            }
            const float p0 = at(i - 1, c);
            const float p1 = at(i, c);
            const float p2 = at(i + 1, c);
            const float p3 = at(i + 2, c);
            const float a = 0.5f * (p3 - p0) + 1.5f * (p1 - p2);
            const float b = p0 - 2.5f * p1 + 2.0f * p2 - 0.5f * p3;
            const float cc = 0.5f * (p2 - p0);
            in[c] = ((a * t + b) * t + cc) * t + p1;
        }
        const float* frame = in;
        if (keep != mc) {
            remix(in, keep, mixed, mc, 1);
            frame = mixed;
        }
        v.current_gain += (target - v.current_gain) * smooth;
        float g = v.current_gain;
        if (v.fade_in_left > 0.0) {
            g *= static_cast<float>(1.0 - v.fade_in_left / v.fade_in_total);
            v.fade_in_left -= 1.0;
        }
        if (v.fading_out) {
            g *= static_cast<float>(std::max(0.0, v.fade_out_left / v.fade_out_total));
            v.fade_out_left -= 1.0;
        }
        float* o = out + f * mc;
        if (mc == 2) {
            o[0] += frame[0] * g * pan_l;
            o[1] += frame[1] * g * pan_r;
        } else {
            for (std::size_t c = 0; c < mc; ++c) o[c] += frame[c] * g;
        }
        if (v.fading_out && v.fade_out_left <= 0.0) {
            v.finished.store(true);
            break;
        }
        v.pos += step;
    }
    v.position.store(v.pos / src_rate);
    publish_clock(v, clock_before, v.pos / src_rate);
}

namespace {

std::size_t read_memory(const AudioBuffer& src, std::size_t& index, bool loop, float* dst, std::size_t frames, bool& ended) noexcept {
    const std::size_t total = src.frames();
    const std::size_t c = src.channels();
    std::size_t done = 0;
    ended = false;
    while (done < frames) {
        if (index >= total) {
            if (loop && total > 0) {
                index = 0;
            } else {
                ended = true;
                break;
            }
        }
        const std::size_t n = std::min(frames - done, total - index);
        std::memcpy(dst + done * c, src.data() + index * c, n * c * sizeof(float));
        index += n;
        done += n;
    }
    return done;
}

} // namespace

void Mixer::render_path_voice(Voice& v, float* out, std::size_t frames) noexcept {
    const std::size_t mc = m_channels;
    const std::size_t fc = v.feed_channels;
    StreamSource* stream = v.stream.get();
    bool restart = false;
    if (!stream) {
        const double seek = v.seek_to.exchange(-1.0);
        if (seek >= 0.0) {
            const std::size_t total = v.sound->frames();
            v.src_index = std::min(total, static_cast<std::size_t>(seek * static_cast<double>(v.feed_rate)));
            v.play_pos = static_cast<double>(v.src_index);
            restart = true;
        }
    } else {
        const std::uint64_t g = stream->seg_gen.load(std::memory_order_acquire);
        if (g != v.seg_gen) {
            const std::uint64_t start = stream->seg_start.load(std::memory_order_relaxed);
            const double sec = stream->seg_seconds.load(std::memory_order_relaxed);
            if (stream->seg_gen.load(std::memory_order_acquire) == g) {
                v.seg_gen = g;
                v.seg_seconds = sec;
                v.play_pos = 0.0;
                stream->read.store(start, std::memory_order_release);
                v.waiting = true;
                restart = true;
            }
        }
    }
    if (restart) {
        if (v.stretcher) v.stretcher->reset();
        v.feed_read = 0;
        v.feed_count = 0;
        v.feed_ended = false;
        v.source_ended = false;
        v.primed = 0;
        v.pads = 0;
        v.frac = 0.0;
    }
    if (v.stop_requested.load(std::memory_order_relaxed) && !v.fading_out) {
        const double fade = v.fade_out_seconds.load();
        if (fade <= 0.0 || v.paused.load()) {
            v.finished.store(true);
            if (stream) stream->done.store(true);
            return;
        }
        v.fading_out = true;
        v.fade_out_total = fade * static_cast<double>(m_rate);
        v.fade_out_left = v.fade_out_total;
    }
    const double clock_before = restart && stream ? v.seg_seconds : v.position.load(std::memory_order_relaxed);
    if (v.paused.load(std::memory_order_relaxed) || (stream && stream->req_gen.load(std::memory_order_acquire) != v.seg_gen)) {
        if (v.clock_active || restart) publish_clock(v, clock_before, clock_before);
        return;
    }

    const float speed = std::max(0.01f, v.speed.load(std::memory_order_relaxed));
    const bool preserve = v.preserve.load(std::memory_order_relaxed);
    double unit = 1.0;
    double step = static_cast<double>(v.feed_rate) / static_cast<double>(m_rate);
    if (v.stretcher) {
        const double tempo = preserve ? static_cast<double>(speed) : 1.0;
        v.stretcher->set_tempo(tempo);
        v.stretcher->set_pitch_semitones(static_cast<double>(v.pitch.load(std::memory_order_relaxed)));
        unit = v.stretcher->tempo();
        if (!preserve) step *= static_cast<double>(speed);
    } else {
        step *= static_cast<double>(speed);
    }
    const bool loop = v.loop.load(std::memory_order_relaxed);
    const float target = v.gain.load(std::memory_order_relaxed);
    const float pan = v.pan.load(std::memory_order_relaxed);
    const float pan_l = mc == 2 ? std::min(1.0f, 1.0f - pan) : 1.0f;
    const float pan_r = mc == 2 ? std::min(1.0f, 1.0f + pan) : 1.0f;
    const float smooth = 1.0f - std::exp(-1.0f / (0.005f * static_cast<float>(m_rate)));

    auto source_read = [&](float* dst, std::size_t want, bool& ended) -> std::size_t {
        ended = false;
        if (!stream) return read_memory(*v.sound, v.src_index, loop, dst, want, ended);
        const std::uint64_t r = stream->read.load(std::memory_order_relaxed);
        const std::uint64_t w = stream->write.load(std::memory_order_acquire);
        const bool eof = stream->eof_gen.load(std::memory_order_acquire) == v.seg_gen;
        const std::uint64_t eof_at = stream->eof_at.load(std::memory_order_relaxed);
        const std::size_t avail = static_cast<std::size_t>(w - r);
        if (v.waiting) {
            if (avail < v.prefill && !eof) return 0;
            v.waiting = false;
        }
        const std::size_t n = std::min(want, avail);
        const std::size_t at = static_cast<std::size_t>(r) & stream->mask;
        const std::size_t first = std::min(n, stream->capacity - at);
        std::memcpy(dst, stream->ring.get() + at * mc, first * mc * sizeof(float));
        if (n > first) std::memcpy(dst + first * mc, stream->ring.get(), (n - first) * mc * sizeof(float));
        stream->read.store(r + n, std::memory_order_release);
        if (eof && r + n >= eof_at) ended = true;
        return n;
    };

    auto fill_feed = [&]() -> std::size_t {
        if (v.stretcher) {
            TimeStretcher& st = *v.stretcher;
            int guard = 0;
            while (st.available() < v.feed_cap && guard++ < 64) {
                const std::size_t need = st.input_required();
                if (need == 0) break;
                bool ended = false;
                std::size_t n = 0;
                if (!v.source_ended) n = source_read(v.scratch.data(), std::min(need, v.scratch_cap), ended);
                if (n) st.push(v.scratch.data(), n);
                if (ended || v.source_ended) {
                    v.source_ended = true;
                    st.finish();
                    break;
                }
                if (n == 0) break;
            }
            const std::size_t got = st.pull(v.feed.data(), v.feed_cap);
            if (got == 0 && st.finished()) v.feed_ended = true;
            return got;
        }
        bool ended = false;
        const std::size_t got = source_read(v.feed.data(), v.feed_cap, ended);
        if (ended && got == 0) v.feed_ended = true;
        if (ended) v.source_ended = true;
        if (got == 0 && v.source_ended) v.feed_ended = true;
        return got;
    };

    auto shift_in = [&]() -> bool {
        if (v.feed_read >= v.feed_count && !v.feed_ended) {
            v.feed_count = fill_feed();
            v.feed_read = 0;
        }
        float* h = v.hist.data();
        if (v.feed_read < v.feed_count) {
            std::memmove(h, h + fc, 3 * fc * sizeof(float));
            std::memcpy(h + 3 * fc, v.feed.data() + v.feed_read * fc, fc * sizeof(float));
            ++v.feed_read;
            return true;
        }
        if (!v.feed_ended) return false;
        std::memmove(h, h + fc, 3 * fc * sizeof(float));
        std::fill(h + 3 * fc, h + 4 * fc, 0.0f);
        ++v.pads;
        return true;
    };

    const double duration_frames = stream ? stream->duration * static_cast<double>(m_rate) : static_cast<double>(v.sound->frames());
    float* mixed = m_frame_out.data();
    float* in = v.frame.data();
    v.starving = false;
    for (std::size_t f = 0; f < frames; ++f) {
        bool ok = true;
        while (v.primed < 3) {
            if (!shift_in()) {
                ok = false;
                break;
            }
            if (++v.primed == 3) std::memcpy(v.hist.data(), v.hist.data() + fc, fc * sizeof(float));
        }
        while (ok && v.frac >= 1.0) {
            if (!shift_in()) {
                ok = false;
                break;
            }
            v.frac -= 1.0;
            v.play_pos += unit;
        }
        if (!ok) {
            if (stream && !v.waiting) stream->underruns.fetch_add(1, std::memory_order_relaxed);
            v.starving = true;
            break;
        }
        if (v.pads >= 3) {
            v.finished.store(true);
            if (stream) stream->done.store(true);
            break;
        }
        const float t = static_cast<float>(v.frac);
        const float* h = v.hist.data();
        const std::size_t keep = std::min<std::size_t>(fc, 64);
        for (std::size_t c = 0; c < keep; ++c) {
            const float p0 = h[c];
            const float p1 = h[fc + c];
            if (t == 0.0f) {
                in[c] = p1;
                continue;
            }
            const float p2 = h[2 * fc + c];
            const float p3 = h[3 * fc + c];
            const float a = 0.5f * (p3 - p0) + 1.5f * (p1 - p2);
            const float b = p0 - 2.5f * p1 + 2.0f * p2 - 0.5f * p3;
            const float cc = 0.5f * (p2 - p0);
            in[c] = ((a * t + b) * t + cc) * t + p1;
        }
        const float* frame = in;
        if (keep != mc) {
            remix(in, keep, mixed, mc, 1);
            frame = mixed;
        }
        v.current_gain += (target - v.current_gain) * smooth;
        float g = v.current_gain;
        if (v.fade_in_left > 0.0) {
            g *= static_cast<float>(1.0 - v.fade_in_left / v.fade_in_total);
            v.fade_in_left -= 1.0;
        }
        if (v.fading_out) {
            g *= static_cast<float>(std::max(0.0, v.fade_out_left / v.fade_out_total));
            v.fade_out_left -= 1.0;
        }
        float* o = out + f * mc;
        if (mc == 2) {
            o[0] += frame[0] * g * pan_l;
            o[1] += frame[1] * g * pan_r;
        } else {
            for (std::size_t c = 0; c < mc; ++c) o[c] += frame[c] * g;
        }
        if (v.fading_out && v.fade_out_left <= 0.0) {
            v.finished.store(true);
            if (stream) stream->done.store(true);
            break;
        }
        v.frac += step;
    }
    double p = v.play_pos;
    if (loop && duration_frames > 0.0) p = std::fmod(p + (stream ? v.seg_seconds * static_cast<double>(m_rate) : 0.0), duration_frames) - (stream ? v.seg_seconds * static_cast<double>(m_rate) : 0.0);
    double clock_after;
    if (stream) {
        clock_after = v.seg_seconds + p / static_cast<double>(m_rate);
    } else {
        clock_after = p / static_cast<double>(v.feed_rate);
    }
    v.position.store(clock_after);
    publish_clock(v, clock_before, clock_after);
}

void Mixer::render(float* output, std::size_t frames) noexcept {
    std::memset(output, 0, frames * m_channels * sizeof(float));
    m_render_ns = steady_ns();
    m_block_seconds = static_cast<double>(frames) / static_cast<double>(m_rate);
    if (m_mutex.try_lock()) {
        for (auto& v : m_pending) m_active.push_back(std::move(v));
        m_pending.clear();
        for (std::size_t i = 0; i < m_active.size();) {
            if (m_active[i]->finished.load()) {
                m_finished.push_back(std::move(m_active[i]));
                m_active[i] = std::move(m_active.back());
                m_active.pop_back();
            } else {
                ++i;
            }
        }
        m_mutex.unlock();
    }
    for (const std::shared_ptr<Voice>& v : m_active) {
        if (v->finished.load(std::memory_order_relaxed)) continue;
        if (v->path) {
            render_path_voice(*v, output, frames);
        } else {
            render_voice(*v, output, frames);
        }
    }
    const float master = m_master.load(std::memory_order_relaxed);
    float peak = 0.0f;
    const std::size_t n = frames * m_channels;
    for (std::size_t i = 0; i < n; ++i) {
        output[i] *= master;
        peak = std::max(peak, std::fabs(output[i]));
    }
    m_peak.store(peak, std::memory_order_relaxed);
}

bool Mixer::start(const StreamConfig& config, std::string* error) {
    if (m_stream.running()) return true;
    StreamConfig c = config;
    c.kind = DeviceKind::output;
    c.sample_rate = m_rate;
    c.channels = static_cast<std::uint32_t>(m_channels);
    if (!m_stream.open_playback(c, [this](float* out, std::size_t frames) { render(out, frames); }, error)) return false;
    m_stream_latency.store(static_cast<double>(m_stream.latency_frames()) / static_cast<double>(m_rate));
    if (!m_stream.start(error)) {
        m_stream_latency.store(0.0);
        return false;
    }
    return true;
}

void Mixer::stop_output() {
    m_stream.close();
    m_stream_latency.store(0.0);
}

double Mixer::output_latency() const noexcept {
    const double o = m_latency_override.load();
    return o >= 0.0 ? o : m_stream_latency.load();
}

double Mixer::playback_position(VoiceId id) const {
    auto v = find(id);
    if (!v) return 0.0;
    double pos = 0.0;
    double rate = 0.0;
    double floor_pos = 0.0;
    std::int64_t ns = 0;
    for (int attempt = 0; attempt < 64; ++attempt) {
        const std::uint32_t a = v->clock_seq.load(std::memory_order_acquire);
        if (a & 1u) continue;
        pos = v->clock_pos.load(std::memory_order_relaxed);
        rate = v->clock_rate.load(std::memory_order_relaxed);
        floor_pos = v->clock_floor.load(std::memory_order_relaxed);
        ns = v->clock_ns.load(std::memory_order_relaxed);
        std::atomic_thread_fence(std::memory_order_acquire);
        if (v->clock_seq.load(std::memory_order_relaxed) == a) break;
    }
    if (ns == 0) return v->position.load();
    const double elapsed = std::max(0.0, static_cast<double>(steady_ns() - ns) / 1e9);
    const double behind = std::max(0.0, output_latency() - elapsed);
    double audible = pos - behind * rate;
    if (audible < floor_pos) audible = floor_pos;
    if (audible > pos) audible = pos;
    return audible;
}

} // namespace audio
} // namespace fizmo
