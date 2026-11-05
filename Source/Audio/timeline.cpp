#include "timeline.hpp"
#include "wav.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>
#include <limits>
#include <utility>

namespace fizmo {
namespace audio {

namespace {

const double k_pi = 3.14159265358979323846;
const std::size_t k_block = 64;
const std::size_t k_max_channels = 64;
const std::int64_t k_none = std::numeric_limits<std::int64_t>::min();

float level(float db) noexcept {
    return db <= -120.0f ? 0.0f : db_to_gain(db);
}

std::int64_t to_frames(double seconds, double rate) noexcept {
    return static_cast<std::int64_t>(std::llround(seconds * rate));
}

} // namespace

float fade_gain(FadeShape shape, double t) noexcept {
    if (!(t > 0.0)) return 0.0f;
    if (t >= 1.0) return 1.0f;
    double v = t;
    switch (shape) {
        case FadeShape::linear: v = t; break;
        case FadeShape::equal_power: v = std::sin(t * k_pi * 0.5); break;
        case FadeShape::exponential: v = (std::exp(4.0 * t) - 1.0) / (std::exp(4.0) - 1.0); break;
        case FadeShape::logarithmic: v = 1.0 - (std::exp(4.0 * (1.0 - t)) - 1.0) / (std::exp(4.0) - 1.0); break;
        case FadeShape::s_curve: v = 0.5 - 0.5 * std::cos(t * k_pi); break;
    }
    return static_cast<float>(v);
}

double Clip::source_duration() const noexcept {
    return source ? source->duration() : 0.0;
}

bool Clip::split(double time, Clip& right) {
    if (!(time > position) || !(time < end())) return false;
    const double d = time - position;
    right = *this;
    right.position = time;
    right.source_start = source_start + d;
    right.length = length - d;
    right.fade_in = 0.0;
    right.fade_out = std::min(fade_out, right.length);
    length = d;
    fade_out = 0.0;
    fade_in = std::min(fade_in, length);
    return true;
}

void Clip::trim_start(double time) {
    const double earliest = position - source_start;
    time = std::max(time, earliest);
    time = std::min(time, end());
    const double d = time - position;
    position = time;
    source_start += d;
    length = std::max(0.0, length - d);
    fade_in = std::min(fade_in, length);
    fade_out = std::min(fade_out, length);
}

void Clip::trim_end(double time) {
    double len = std::max(0.0, time - position);
    if (source) len = std::min(len, std::max(0.0, source->duration() - source_start));
    length = len;
    fade_in = std::min(fade_in, length);
    fade_out = std::min(fade_out, length);
}

Clip Clip::duplicate(double at) const {
    Clip c = *this;
    c.position = at;
    return c;
}

float Clip::gain_at(double time) const noexcept {
    if (muted || !contains(time)) return 0.0f;
    float g = level(gain_db);
    const double in = time - position;
    const double out = end() - time;
    if (fade_in > 0.0 && in < fade_in) g *= fade_gain(fade_in_shape, in / fade_in);
    if (fade_out > 0.0 && out < fade_out) g *= fade_gain(fade_out_shape, out / fade_out);
    return g;
}

Clip make_clip(std::shared_ptr<const AudioBuffer> source, double position, std::string name) {
    Clip c;
    c.length = source ? source->duration() : 0.0;
    c.source = std::move(source);
    c.position = position;
    c.name = std::move(name);
    return c;
}

Clip make_clip(const AudioBuffer& source, double position, std::string name) {
    return make_clip(std::make_shared<const AudioBuffer>(source), position, std::move(name));
}

float automation_shape(Curve curve, float tension, double t) noexcept {
    if (!(t > 0.0)) return 0.0f;
    if (t >= 1.0) return 1.0f;
    switch (curve) {
        case Curve::hold: return 0.0f;
        case Curve::linear: return static_cast<float>(t);
        case Curve::s_curve: return static_cast<float>(0.5 - 0.5 * std::cos(t * k_pi));
        case Curve::exponential: {
            const double k = 4.0 * (1.0 + std::max(-0.9, std::min(1.0, static_cast<double>(tension))));
            return static_cast<float>((std::exp(k * t) - 1.0) / (std::exp(k) - 1.0));
        }
        case Curve::bezier: {
            const double tau = std::max(-1.0, std::min(1.0, static_cast<double>(tension)));
            const double cx = (1.0 - tau) * 0.5;
            const double cy = (1.0 + tau) * 0.5;
            const double a = 1.0 - 2.0 * cx;
            double u = 0.0;
            if (std::fabs(a) < 1e-9) {
                u = cx > 0.0 ? t / (2.0 * cx) : t;
            } else {
                u = (-cx + std::sqrt(std::max(0.0, cx * cx + a * t))) / a;
            }
            u = std::max(0.0, std::min(1.0, u));
            return static_cast<float>(2.0 * u * (1.0 - u) * cy + u * u);
        }
    }
    return static_cast<float>(t);
}

AutomationLane::AutomationLane(AutomationTarget target_, std::size_t effect_index_, std::string parameter_)
    : target(target_), effect_index(effect_index_), parameter(std::move(parameter_)) {
    if (target == AutomationTarget::mute) default_value = 0.0f;
}

AutomationLane::AutomationLane(const AutomationLane& other)
    : target(other.target), effect_index(other.effect_index), parameter(other.parameter), default_value(other.default_value), enabled(other.enabled), m_points(other.m_points), m_hint(other.m_hint.load(std::memory_order_relaxed)) {}

AutomationLane& AutomationLane::operator=(const AutomationLane& other) {
    if (this == &other) return *this;
    target = other.target;
    effect_index = other.effect_index;
    parameter = other.parameter;
    default_value = other.default_value;
    enabled = other.enabled;
    m_points = other.m_points;
    m_hint.store(0, std::memory_order_relaxed);
    return *this;
}

std::size_t AutomationLane::add(const AutomationPoint& point) {
    auto it = std::upper_bound(m_points.begin(), m_points.end(), point.time, [](double t, const AutomationPoint& p) { return t < p.time; });
    const std::size_t index = static_cast<std::size_t>(it - m_points.begin());
    m_points.insert(it, point);
    m_hint.store(0, std::memory_order_relaxed);
    return index;
}

std::size_t AutomationLane::add(double time, float value, Curve curve, float tension) {
    AutomationPoint p;
    p.time = time;
    p.value = value;
    p.curve = curve;
    p.tension = tension;
    return add(p);
}

bool AutomationLane::remove(std::size_t index) {
    if (index >= m_points.size()) return false;
    m_points.erase(m_points.begin() + static_cast<std::ptrdiff_t>(index));
    m_hint.store(0, std::memory_order_relaxed);
    return true;
}

std::size_t AutomationLane::move(std::size_t index, double time, float value) {
    if (index >= m_points.size()) return index;
    AutomationPoint p = m_points[index];
    m_points.erase(m_points.begin() + static_cast<std::ptrdiff_t>(index));
    p.time = time;
    p.value = value;
    return add(p);
}

bool AutomationLane::set_curve(std::size_t index, Curve curve, float tension) {
    if (index >= m_points.size()) return false;
    m_points[index].curve = curve;
    m_points[index].tension = tension;
    return true;
}

void AutomationLane::remove_range(double start, double end) {
    m_points.erase(std::remove_if(m_points.begin(), m_points.end(), [&](const AutomationPoint& p) { return p.time >= start && p.time < end; }), m_points.end());
    m_hint.store(0, std::memory_order_relaxed);
}

void AutomationLane::clear() {
    m_points.clear();
    m_hint.store(0, std::memory_order_relaxed);
}

std::size_t AutomationLane::segment(double time) const noexcept {
    const std::size_t n = m_points.size();
    std::size_t h = m_hint.load(std::memory_order_relaxed);
    if (h + 1 < n && m_points[h].time <= time && time < m_points[h + 1].time) return h;
    if (h + 2 < n && m_points[h + 1].time <= time && time < m_points[h + 2].time) {
        m_hint.store(h + 1, std::memory_order_relaxed);
        return h + 1;
    }
    auto it = std::upper_bound(m_points.begin(), m_points.end(), time, [](double t, const AutomationPoint& p) { return t < p.time; });
    h = static_cast<std::size_t>(it - m_points.begin()) - 1;
    m_hint.store(h, std::memory_order_relaxed);
    return h;
}

float AutomationLane::evaluate(std::size_t i, double time) const noexcept {
    const AutomationPoint& a = m_points[i];
    const AutomationPoint& b = m_points[i + 1];
    const double span = b.time - a.time;
    if (!(span > 0.0)) return b.value;
    const double t = (time - a.time) / span;
    if (a.curve == Curve::hold) return a.value;
    if (a.curve == Curve::exponential && ((a.value > 0.0f && b.value > 0.0f) || (a.value < 0.0f && b.value < 0.0f)) && a.tension == 0.0f) {
        return static_cast<float>(static_cast<double>(a.value) * std::pow(static_cast<double>(b.value) / static_cast<double>(a.value), t));
    }
    const float s = automation_shape(a.curve, a.tension, t);
    return a.value + (b.value - a.value) * s;
}

float AutomationLane::value_at(double time) const noexcept {
    const std::size_t n = m_points.size();
    if (n == 0) return default_value;
    if (time < m_points[0].time) return m_points[0].value;
    if (time >= m_points[n - 1].time) return m_points[n - 1].value;
    return evaluate(segment(time), time);
}

void AutomationLane::render(double start_time, double sample_rate, float* out, std::size_t frames) const noexcept {
    const double step = sample_rate > 0.0 ? 1.0 / sample_rate : 0.0;
    for (std::size_t i = 0; i < frames; ++i) out[i] = value_at(start_time + static_cast<double>(i) * step);
}

void AutomationLane::render_frames(std::int64_t first_frame, double sample_rate, float* out, std::size_t frames) const noexcept {
    if (!(sample_rate > 0.0)) return;
    const std::size_t n = m_points.size();
    if (n == 0) {
        std::fill(out, out + frames, default_value);
        return;
    }
    for (std::size_t i = 0; i < frames; ++i) out[i] = value_at(static_cast<double>(first_frame + static_cast<std::int64_t>(i)) / sample_rate);
}

Track::Track(std::string name_, std::size_t channels_) : name(std::move(name_)), channels(channels_), effects(std::make_shared<EffectChain>()) {}

std::size_t Track::add_clip(Clip clip) {
    clips.push_back(std::move(clip));
    return clips.size() - 1;
}

bool Track::remove_clip(std::size_t index) {
    if (index >= clips.size()) return false;
    clips.erase(clips.begin() + static_cast<std::ptrdiff_t>(index));
    return true;
}

bool Track::split_clip(std::size_t index, double time) {
    if (index >= clips.size()) return false;
    Clip right;
    if (!clips[index].split(time, right)) return false;
    clips.insert(clips.begin() + static_cast<std::ptrdiff_t>(index + 1), std::move(right));
    return true;
}

std::vector<std::size_t> Track::clips_at(double time) const {
    std::vector<std::size_t> out;
    for (std::size_t i = 0; i < clips.size(); ++i) {
        if (clips[i].contains(time)) out.push_back(i);
    }
    return out;
}

namespace {

bool lane_matches(const AutomationLane& l, AutomationTarget target, std::size_t effect_index, const std::string& parameter) {
    if (l.target != target) return false;
    if (target != AutomationTarget::effect_parameter) return true;
    return l.effect_index == effect_index && l.parameter == parameter;
}

AutomationLane& find_or_add(std::vector<AutomationLane>& lanes, AutomationTarget target, std::size_t effect_index, const std::string& parameter) {
    for (AutomationLane& l : lanes) {
        if (lane_matches(l, target, effect_index, parameter)) return l;
    }
    lanes.emplace_back(target, effect_index, parameter);
    return lanes.back();
}

} // namespace

AutomationLane& Track::lane(AutomationTarget target, std::size_t effect_index, const std::string& parameter) {
    AutomationLane& l = find_or_add(lanes, target, effect_index, parameter);
    if (l.empty()) {
        if (target == AutomationTarget::volume) l.default_value = volume_db;
        if (target == AutomationTarget::pan) l.default_value = pan;
    }
    return l;
}

const AutomationLane* Track::find_lane(AutomationTarget target, std::size_t effect_index, const std::string& parameter) const {
    for (const AutomationLane& l : lanes) {
        if (lane_matches(l, target, effect_index, parameter)) return &l;
    }
    return nullptr;
}

bool Track::remove_lane(AutomationTarget target, std::size_t effect_index, const std::string& parameter) {
    for (std::size_t i = 0; i < lanes.size(); ++i) {
        if (lane_matches(lanes[i], target, effect_index, parameter)) {
            lanes.erase(lanes.begin() + static_cast<std::ptrdiff_t>(i));
            return true;
        }
    }
    return false;
}

double Track::length() const noexcept {
    double end = 0.0;
    for (const Clip& c : clips) end = std::max(end, c.end());
    return end;
}

std::shared_ptr<const AudioBuffer> ResampleCache::get(const std::shared_ptr<const AudioBuffer>& source, std::uint32_t rate, ResampleQuality quality) {
    if (!source || rate == 0 || source->sample_rate() == 0) return source;
    if (source->sample_rate() == rate) return source;
    std::lock_guard<std::mutex> lock(m_mutex);
    m_entries.erase(std::remove_if(m_entries.begin(), m_entries.end(), [](const Entry& e) { return e.source.expired(); }), m_entries.end());
    for (const Entry& e : m_entries) {
        if (e.key == source.get() && e.rate == rate && e.quality == quality && !e.source.owner_before(source) && !source.owner_before(e.source)) return e.result;
    }
    Entry e;
    e.source = source;
    e.key = source.get();
    e.rate = rate;
    e.quality = quality;
    e.result = std::make_shared<const AudioBuffer>(resample(*source, rate, quality));
    m_entries.push_back(e);
    return e.result;
}

void ResampleCache::clear() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_entries.clear();
}

std::size_t ResampleCache::size() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_entries.size();
}

Timeline::Timeline(std::uint32_t sample_rate, std::size_t channels)
    : master_effects(std::make_shared<EffectChain>()),
      m_rate(sample_rate ? sample_rate : 48000),
      m_channels(channels ? std::min(channels, k_max_channels) : 2),
      m_cache(std::make_shared<ResampleCache>()) {}

void Timeline::set_format(std::uint32_t sample_rate, std::size_t channels) {
    m_rate = sample_rate ? sample_rate : 48000;
    m_channels = channels ? std::min(channels, k_max_channels) : 2;
}

void Timeline::set_cache(std::shared_ptr<ResampleCache> cache) {
    m_cache = cache ? std::move(cache) : std::make_shared<ResampleCache>();
}

Track& Timeline::add_track(std::string name, std::size_t channels) {
    tracks.emplace_back(std::move(name), channels);
    return tracks.back();
}

bool Timeline::remove_track(std::size_t index) {
    if (index >= tracks.size()) return false;
    tracks.erase(tracks.begin() + static_cast<std::ptrdiff_t>(index));
    return true;
}

std::size_t Timeline::add_marker(double time, std::string name) {
    Marker m;
    m.time = time;
    m.name = std::move(name);
    auto it = std::upper_bound(markers.begin(), markers.end(), time, [](double t, const Marker& x) { return t < x.time; });
    const std::size_t index = static_cast<std::size_t>(it - markers.begin());
    markers.insert(it, std::move(m));
    return index;
}

bool Timeline::remove_marker(std::size_t index) {
    if (index >= markers.size()) return false;
    markers.erase(markers.begin() + static_cast<std::ptrdiff_t>(index));
    return true;
}

AutomationLane& Timeline::master_lane(AutomationTarget target, std::size_t effect_index, const std::string& parameter) {
    AutomationLane& l = find_or_add(master_lanes, target, effect_index, parameter);
    if (l.empty() && target == AutomationTarget::volume) l.default_value = master_volume_db;
    return l;
}

double Timeline::beat_length() const noexcept {
    return tempo > 0.0 ? 60.0 / tempo : 0.5;
}

double Timeline::bar_length() const noexcept {
    return beat_length() * static_cast<double>(beats_per_bar > 0 ? beats_per_bar : 4);
}

double Timeline::beats_to_seconds(double beats) const noexcept {
    return beats * beat_length();
}

double Timeline::seconds_to_beats(double seconds) const noexcept {
    return seconds / beat_length();
}

double Timeline::bars_to_seconds(double bars) const noexcept {
    return bars * bar_length();
}

double Timeline::seconds_to_bars(double seconds) const noexcept {
    return seconds / bar_length();
}

double Timeline::snap(double seconds, double division_beats) const noexcept {
    const double grid = beats_to_seconds(division_beats > 0.0 ? division_beats : 1.0);
    if (!(grid > 0.0)) return seconds;
    return std::round(seconds / grid) * grid;
}

double Timeline::length() const noexcept {
    double end = 0.0;
    for (const Track& t : tracks) end = std::max(end, t.length());
    return end;
}

bool Timeline::any_solo() const noexcept {
    for (const Track& t : tracks) {
        if (t.solo) return true;
    }
    return false;
}

namespace detail {

struct CompiledClip {
    const float* data = nullptr;
    std::size_t channels = 0;
    std::int64_t frames = 0;
    std::int64_t start = 0;
    std::int64_t end = 0;
    std::int64_t offset = 0;
    float gain = 1.0f;
    std::int64_t fade_in = 0;
    std::int64_t fade_out = 0;
    FadeShape in_shape = FadeShape::linear;
    FadeShape out_shape = FadeShape::linear;
};

struct CompiledParameter {
    const AutomationLane* lane = nullptr;
    Parameter* parameter = nullptr;
    std::shared_ptr<Effect> owner;
};

struct CompiledBus {
    std::size_t channels = 2;
    std::vector<CompiledClip> clips;
    std::vector<std::int64_t> reach;
    std::vector<std::shared_ptr<const AudioBuffer>> sources;
    std::shared_ptr<EffectChain> chain;
    std::vector<std::shared_ptr<Effect>> effects;
    std::size_t latency = 0;
    std::size_t tail = 0;
    const AutomationLane* volume = nullptr;
    const AutomationLane* pan = nullptr;
    const AutomationLane* mute = nullptr;
    std::vector<CompiledParameter> parameters;
    float volume_db = 0.0f;
    float pan_value = 0.0f;
    bool audible = true;
};

struct TimelineSnapshot {
    Timeline timeline;
    std::uint32_t rate = 48000;
    std::size_t channels = 2;
    std::vector<CompiledBus> tracks;
    CompiledBus master;
    std::size_t preroll = 0;
    std::size_t tail = 0;
    std::int64_t length = 0;

    bool same_layout(const TimelineSnapshot& other) const noexcept {
        if (preroll != other.preroll || master.latency != other.master.latency || tracks.size() != other.tracks.size()) return false;
        for (std::size_t i = 0; i < tracks.size(); ++i) {
            if (tracks[i].latency != other.tracks[i].latency) return false;
        }
        return true;
    }
};

namespace {

void compile_bus(CompiledBus& bus, std::shared_ptr<EffectChain> chain, std::size_t channels, std::uint32_t rate, const std::vector<AutomationLane>& lanes) {
    bus.channels = std::max<std::size_t>(1, std::min(channels, k_max_channels));
    bus.chain = chain ? std::move(chain) : std::make_shared<EffectChain>();
    if (bus.chain->sample_rate() != rate || bus.chain->channels() != bus.channels) bus.chain->prepare(rate, bus.channels);
    const std::size_t count = bus.chain->size();
    for (std::size_t i = 0; i < count; ++i) {
        std::shared_ptr<Effect> e = bus.chain->at(i);
        if (e) bus.effects.push_back(std::move(e));
    }
    bus.latency = bus.chain->latency_frames();
    bus.tail = bus.chain->tail_frames();
    for (const AutomationLane& lane : lanes) {
        if (!lane.active()) continue;
        switch (lane.target) {
            case AutomationTarget::volume:
                if (!bus.volume) bus.volume = &lane;
                break;
            case AutomationTarget::pan:
                if (!bus.pan) bus.pan = &lane;
                break;
            case AutomationTarget::mute:
                if (!bus.mute) bus.mute = &lane;
                break;
            case AutomationTarget::effect_parameter: {
                std::shared_ptr<Effect> e = bus.chain->at(lane.effect_index);
                Parameter* p = e ? e->parameter(lane.parameter) : nullptr;
                if (p) {
                    CompiledParameter cp;
                    cp.lane = &lane;
                    cp.parameter = p;
                    cp.owner = std::move(e);
                    bus.parameters.push_back(std::move(cp));
                }
                break;
            }
        }
    }
}

std::shared_ptr<const TimelineSnapshot> compile(const Timeline& timeline, std::uint32_t rate, std::size_t channels, ResampleQuality quality, ResampleCache& cache) {
    auto snap = std::make_shared<TimelineSnapshot>();
    snap->timeline = timeline;
    snap->rate = rate ? rate : timeline.sample_rate();
    snap->channels = std::max<std::size_t>(1, std::min(channels ? channels : timeline.channels(), k_max_channels));
    const double r = static_cast<double>(snap->rate);
    const Timeline& tl = snap->timeline;
    const bool solo = tl.any_solo();
    snap->tracks.resize(tl.tracks.size());
    std::size_t max_latency = 0;
    std::size_t max_tail = 0;
    for (std::size_t ti = 0; ti < tl.tracks.size(); ++ti) {
        const Track& track = tl.tracks[ti];
        CompiledBus& bus = snap->tracks[ti];
        compile_bus(bus, track.effects, track.channels ? track.channels : snap->channels, snap->rate, track.lanes);
        bus.volume_db = track.volume_db;
        bus.pan_value = std::max(-1.0f, std::min(1.0f, track.pan));
        bus.audible = !track.mute && (!solo || track.solo);
        for (const Clip& clip : track.clips) {
            if (clip.muted || !clip.source || clip.source->empty() || clip.source->channels() == 0 || clip.source->sample_rate() == 0) continue;
            if (!(clip.length > 0.0)) continue;
            std::shared_ptr<const AudioBuffer> src = cache.get(clip.source, snap->rate, quality);
            if (!src || src->empty()) continue;
            CompiledClip c;
            c.data = src->data();
            c.channels = src->channels();
            c.frames = static_cast<std::int64_t>(src->frames());
            c.start = to_frames(clip.position, r);
            c.end = to_frames(clip.position + clip.length, r);
            if (c.end <= c.start) continue;
            c.offset = to_frames(clip.source_start, r);
            c.gain = level(clip.gain_db);
            c.fade_in = std::max<std::int64_t>(0, to_frames(clip.fade_in, r));
            c.fade_out = std::max<std::int64_t>(0, to_frames(clip.fade_out, r));
            c.in_shape = clip.fade_in_shape;
            c.out_shape = clip.fade_out_shape;
            bus.clips.push_back(c);
            bus.sources.push_back(std::move(src));
        }
        std::stable_sort(bus.clips.begin(), bus.clips.end(), [](const CompiledClip& a, const CompiledClip& b) { return a.start < b.start; });
        bus.reach.resize(bus.clips.size());
        std::int64_t furthest = k_none;
        for (std::size_t i = 0; i < bus.clips.size(); ++i) {
            furthest = std::max(furthest, bus.clips[i].end);
            bus.reach[i] = furthest;
        }
        max_latency = std::max(max_latency, bus.latency);
        max_tail = std::max(max_tail, bus.tail);
    }
    compile_bus(snap->master, tl.master_effects, snap->channels, snap->rate, tl.master_lanes);
    snap->master.volume_db = tl.master_volume_db;
    snap->master.pan_value = 0.0f;
    snap->master.audible = true;
    snap->preroll = snap->master.latency + max_latency;
    snap->tail = max_tail + snap->master.tail;
    snap->length = to_frames(tl.length(), r);
    return snap;
}

} // namespace

class TimelineEngine {
public:
    TimelineEngine() {
        m_bus.assign(k_block * k_max_channels, 0.0f);
        m_mix.assign(k_block * k_max_channels, 0.0f);
        m_tmp.assign(k_block * k_max_channels, 0.0f);
        m_discard.assign(k_block * k_max_channels, 0.0f);
        m_fifo.assign(k_block * k_max_channels, 0.0f);
        m_volume.assign(k_block, 0.0f);
        m_pan.assign(k_block, 0.0f);
        m_mute.assign(k_block, 0.0f);
        m_frame.assign(k_max_channels, 0.0f);
    }

    void set_loop(bool on, std::int64_t a, std::int64_t b) noexcept {
        m_loop = on && b > a;
        m_la = a;
        m_lb = b;
    }

    std::int64_t position() const noexcept {
        const std::int64_t x = map(m_u - buffered());
        return x == k_none ? m_base : x;
    }

    void rebase() noexcept {
        const std::int64_t rem = buffered();
        m_base = position();
        m_u = rem;
    }

    void begin(const TimelineSnapshot& snap, std::int64_t start) noexcept {
        m_base = start;
        m_u = -static_cast<std::int64_t>(snap.preroll);
        m_fifo_pos = 0;
        m_fifo_len = 0;
        const double rate = static_cast<double>(snap.rate);
        auto prime = [&](const CompiledBus& bus, std::int64_t virt) {
            const std::int64_t x = map(std::max<std::int64_t>(virt, 0));
            for (const CompiledParameter& p : bus.parameters) p.parameter->set(p.lane->value_at(static_cast<double>(x) / rate));
            for (const std::shared_ptr<Effect>& e : bus.effects) e->reset();
        };
        const std::int64_t lm = static_cast<std::int64_t>(snap.master.latency);
        for (const CompiledBus& t : snap.tracks) prime(t, m_u + lm + static_cast<std::int64_t>(t.latency));
        prime(snap.master, m_u + lm);
        while (m_u + static_cast<std::int64_t>(k_block) <= 0) block(snap, m_discard.data(), k_block);
        if (m_u < 0) {
            const std::size_t skip = static_cast<std::size_t>(-m_u);
            block(snap, m_fifo.data(), k_block);
            m_fifo_pos = skip;
            m_fifo_len = k_block;
        }
    }

    void process(const TimelineSnapshot& snap, float* out, std::size_t frames) noexcept {
        const std::size_t oc = snap.channels;
        std::size_t done = 0;
        while (done < frames) {
            if (m_fifo_pos >= m_fifo_len) {
                block(snap, m_fifo.data(), k_block);
                m_fifo_pos = 0;
                m_fifo_len = k_block;
            }
            const std::size_t n = std::min(m_fifo_len - m_fifo_pos, frames - done);
            std::memcpy(out + done * oc, m_fifo.data() + m_fifo_pos * oc, n * oc * sizeof(float));
            m_fifo_pos += n;
            done += n;
        }
    }

private:
    std::int64_t buffered() const noexcept {
        return static_cast<std::int64_t>(m_fifo_len - m_fifo_pos);
    }

    bool loop_active() const noexcept {
        return m_loop && m_base < m_lb;
    }

    std::int64_t map(std::int64_t v) const noexcept {
        if (v < 0) return k_none;
        const std::int64_t x = m_base + v;
        if (loop_active() && x >= m_lb) return m_la + (x - m_lb) % (m_lb - m_la);
        return x;
    }

    template <typename F>
    void segments(std::int64_t v0, std::size_t k, F&& fn) const noexcept {
        std::size_t i = 0;
        while (i < k) {
            const std::int64_t v = v0 + static_cast<std::int64_t>(i);
            std::size_t len = k - i;
            if (v < 0) {
                len = std::min<std::size_t>(len, static_cast<std::size_t>(-v));
                fn(i, k_none, len);
            } else {
                const std::int64_t x = map(v);
                if (loop_active() && x < m_lb) len = std::min<std::size_t>(len, static_cast<std::size_t>(m_lb - x));
                fn(i, x, len);
            }
            i += len;
        }
    }

    void mix_clips(const CompiledBus& bus, std::int64_t x, float* dst, std::size_t len) noexcept {
        if (bus.clips.empty()) return;
        const std::size_t tc = bus.channels;
        const std::int64_t xe = x + static_cast<std::int64_t>(len);
        auto it = std::upper_bound(bus.reach.begin(), bus.reach.end(), x);
        for (std::size_t ci = static_cast<std::size_t>(it - bus.reach.begin()); ci < bus.clips.size(); ++ci) {
            const CompiledClip& c = bus.clips[ci];
            if (c.start >= xe) break;
            if (c.end <= x) continue;
            const std::int64_t a = std::max(x, c.start);
            const std::int64_t b = std::min(xe, c.end);
            const std::size_t sc = c.channels;
            for (std::int64_t f = a; f < b; ++f) {
                const std::int64_t s = c.offset + (f - c.start);
                if (s < 0 || s >= c.frames) continue;
                float g = c.gain;
                const std::int64_t in = f - c.start;
                const std::int64_t out = c.end - 1 - f;
                if (in < c.fade_in) g *= fade_gain(c.in_shape, static_cast<double>(in) / static_cast<double>(c.fade_in));
                if (out < c.fade_out) g *= fade_gain(c.out_shape, static_cast<double>(out) / static_cast<double>(c.fade_out));
                const float* src = c.data + static_cast<std::size_t>(s) * sc;
                float* d = dst + static_cast<std::size_t>(f - x) * tc;
                if (sc == tc) {
                    for (std::size_t ch = 0; ch < tc; ++ch) d[ch] += src[ch] * g;
                } else if (sc == 1 && tc <= 2) {
                    for (std::size_t ch = 0; ch < tc; ++ch) d[ch] += src[0] * g;
                } else {
                    float in_frame[k_max_channels];
                    const std::size_t keep = std::min(sc, k_max_channels);
                    for (std::size_t ch = 0; ch < keep; ++ch) in_frame[ch] = src[ch];
                    remix(in_frame, keep, m_frame.data(), tc, 1);
                    for (std::size_t ch = 0; ch < tc; ++ch) d[ch] += m_frame[ch] * g;
                }
            }
        }
    }

    void lane_values(const AutomationLane* lane, float constant, std::int64_t v0, std::size_t k, double rate, float* out) noexcept {
        if (!lane) {
            std::fill(out, out + k, constant);
            return;
        }
        segments(v0, k, [&](std::size_t off, std::int64_t x, std::size_t len) {
            if (x == k_none) {
                std::fill(out + off, out + off + len, lane->value_at(static_cast<double>(m_base) / rate));
            } else {
                lane->render_frames(x, rate, out + off, len);
            }
        });
    }

    void set_parameters(const CompiledBus& bus, std::int64_t virt, double rate) noexcept {
        if (bus.parameters.empty()) return;
        const std::int64_t x = map(std::max<std::int64_t>(virt, 0));
        const double t = static_cast<double>(x) / rate;
        for (const CompiledParameter& p : bus.parameters) p.parameter->set(p.lane->value_at(t));
    }

    void fader(const CompiledBus& bus, const float* src, float* dst, std::size_t out_channels, std::size_t k, std::int64_t v, double rate) noexcept {
        if (!bus.audible) return;
        lane_values(bus.volume, bus.volume_db, v, k, rate, m_volume.data());
        if (bus.mute) {
            lane_values(bus.mute, 0.0f, v, k, rate, m_mute.data());
        } else {
            std::fill(m_mute.begin(), m_mute.begin() + static_cast<std::ptrdiff_t>(k), 0.0f);
        }
        const bool stereo = out_channels == 2;
        if (stereo) lane_values(bus.pan, bus.pan_value, v, k, rate, m_pan.data());
        const float* in = src;
        if (bus.channels != out_channels) {
            remix(src, bus.channels, m_tmp.data(), out_channels, k);
            in = m_tmp.data();
        }
        for (std::size_t i = 0; i < k; ++i) {
            if (m_mute[i] >= 0.5f) continue;
            const float g = level(m_volume[i]);
            const float* s = in + i * out_channels;
            float* d = dst + i * out_channels;
            if (stereo) {
                const float p = std::max(-1.0f, std::min(1.0f, m_pan[i]));
                d[0] += s[0] * g * std::min(1.0f, 1.0f - p);
                d[1] += s[1] * g * std::min(1.0f, 1.0f + p);
            } else {
                for (std::size_t ch = 0; ch < out_channels; ++ch) d[ch] += s[ch] * g;
            }
        }
    }

    void block(const TimelineSnapshot& snap, float* out, std::size_t k) noexcept {
        const std::size_t oc = snap.channels;
        const double rate = static_cast<double>(snap.rate);
        const std::int64_t lm = static_cast<std::int64_t>(snap.master.latency);
        float* mix = m_mix.data();
        std::fill(mix, mix + k * oc, 0.0f);
        for (const CompiledBus& t : snap.tracks) {
            const std::size_t tc = t.channels;
            float* buf = m_bus.data();
            std::fill(buf, buf + k * tc, 0.0f);
            const std::int64_t vin = m_u + lm + static_cast<std::int64_t>(t.latency);
            segments(vin, k, [&](std::size_t off, std::int64_t x, std::size_t len) {
                if (x != k_none) mix_clips(t, x, buf + off * tc, len);
            });
            set_parameters(t, vin, rate);
            t.chain->process(buf, k);
            fader(t, buf, mix, oc, k, m_u + lm, rate);
        }
        set_parameters(snap.master, m_u + lm, rate);
        snap.master.chain->process(mix, k);
        std::fill(out, out + k * oc, 0.0f);
        fader(snap.master, mix, out, oc, k, m_u, rate);
        m_u += static_cast<std::int64_t>(k);
    }

    std::int64_t m_base = 0;
    std::int64_t m_u = 0;
    std::size_t m_fifo_pos = 0;
    std::size_t m_fifo_len = 0;
    bool m_loop = false;
    std::int64_t m_la = 0;
    std::int64_t m_lb = 0;
    std::vector<float> m_bus;
    std::vector<float> m_mix;
    std::vector<float> m_tmp;
    std::vector<float> m_discard;
    std::vector<float> m_fifo;
    std::vector<float> m_volume;
    std::vector<float> m_pan;
    std::vector<float> m_mute;
    std::vector<float> m_frame;
};

} // namespace detail

bool render_stream(const Timeline& timeline, double start, double end, const RenderOptions& options, const std::function<bool(const float*, std::size_t)>& sink) {
    const std::uint32_t rate = options.sample_rate ? options.sample_rate : timeline.sample_rate();
    const std::size_t channels = options.channels ? options.channels : timeline.channels();
    std::shared_ptr<ResampleCache> cache = timeline.cache() ? timeline.cache() : std::make_shared<ResampleCache>();
    std::shared_ptr<const detail::TimelineSnapshot> snap = detail::compile(timeline, rate, channels, options.quality, *cache);
    const double r = static_cast<double>(snap->rate);
    const std::int64_t a = to_frames(std::max(0.0, start), r);
    const std::int64_t b = to_frames(std::max(0.0, end), r);
    std::int64_t total = std::max<std::int64_t>(0, b - a);
    if (options.include_tail) {
        const std::int64_t cap = to_frames(std::max(0.0, options.max_tail), r);
        total += std::min<std::int64_t>(static_cast<std::int64_t>(snap->tail), cap);
    }
    std::unique_ptr<detail::TimelineEngine> engine(new detail::TimelineEngine());
    engine->set_loop(false, 0, 0);
    engine->begin(*snap, a);
    const std::size_t chunk = 4096;
    std::vector<float> buf(chunk * snap->channels);
    std::int64_t done = 0;
    while (done < total) {
        const std::size_t n = static_cast<std::size_t>(std::min<std::int64_t>(static_cast<std::int64_t>(chunk), total - done));
        engine->process(*snap, buf.data(), n);
        if (!sink(buf.data(), n)) return false;
        done += static_cast<std::int64_t>(n);
        if (options.progress && !options.progress(static_cast<double>(done) / static_cast<double>(total))) return false;
    }
    return true;
}

AudioBuffer render(const Timeline& timeline, double start, double end, const RenderOptions& options) {
    const std::uint32_t rate = options.sample_rate ? options.sample_rate : timeline.sample_rate();
    const std::size_t channels = std::max<std::size_t>(1, std::min(options.channels ? options.channels : timeline.channels(), k_max_channels));
    AudioBuffer out(channels, rate);
    if (end > start) out.reserve(static_cast<std::size_t>(std::max(0.0, (end - start) * static_cast<double>(rate))) + 1);
    render_stream(timeline, start, end, options, [&](const float* data, std::size_t frames) {
        out.append(data, frames);
        return true;
    });
    return out;
}

AudioBuffer render(const Timeline& timeline, const RenderOptions& options) {
    return render(timeline, 0.0, timeline.length(), options);
}

bool export_wav(const Timeline& timeline, const std::string& path, SampleFormat format, std::string* error, const RenderOptions& options, bool dither) {
    const std::uint32_t rate = options.sample_rate ? options.sample_rate : timeline.sample_rate();
    const std::size_t channels = std::max<std::size_t>(1, std::min(options.channels ? options.channels : timeline.channels(), k_max_channels));
    WavWriter writer;
    if (!writer.open(path, channels, rate, format, error, dither)) return false;
    const bool ok = render_stream(timeline, 0.0, timeline.length(), options, [&](const float* data, std::size_t frames) { return writer.write(data, frames); });
    if (!ok) {
        writer.close();
        if (error && error->empty()) *error = "rendering the timeline failed or was cancelled";
        return false;
    }
    return writer.close(error);
}

bool export_audio(const Timeline& timeline, const std::string& path, const std::function<bool(const AudioBuffer&, const std::string&)>& writer, const RenderOptions& options) {
    if (!writer) return false;
    return writer(render(timeline, options), path);
}

TimelinePlayer::TimelinePlayer(std::uint32_t sample_rate, std::size_t channels)
    : m_rate(sample_rate ? sample_rate : 48000),
      m_channels(channels ? std::min(channels, k_max_channels) : 2),
      m_cache(std::make_shared<ResampleCache>()),
      m_engine(new detail::TimelineEngine()) {
    m_retired.reserve(16);
}

TimelinePlayer::~TimelinePlayer() {
    stop_output();
}

void TimelinePlayer::collect() {
    m_retired.clear();
}

void TimelinePlayer::update(const Timeline& timeline) {
    ResampleCache& cache = timeline.cache() ? *timeline.cache() : *m_cache;
    std::shared_ptr<const detail::TimelineSnapshot> snap = detail::compile(timeline, m_rate, m_channels, m_quality, cache);
    std::shared_ptr<const detail::TimelineSnapshot> old;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        collect();
        old = std::move(m_pending);
        m_pending = std::move(snap);
        m_dirty.store(true, std::memory_order_release);
    }
}

void TimelinePlayer::play() {
    const int s = m_state.load();
    if (s == static_cast<int>(State::playing)) return;
    if (s == static_cast<int>(State::stopped)) m_origin.store(position_frame());
    m_state.store(static_cast<int>(State::playing));
}

void TimelinePlayer::pause() {
    int expected = static_cast<int>(State::playing);
    m_state.compare_exchange_strong(expected, static_cast<int>(State::paused));
}

void TimelinePlayer::stop() {
    const int s = m_state.exchange(static_cast<int>(State::stopped));
    if (s == static_cast<int>(State::playing) || s == static_cast<int>(State::paused)) {
        seek_frame(m_origin.load());
    } else {
        seek_frame(position_frame());
    }
}

void TimelinePlayer::seek(double seconds) {
    seek_frame(to_frames(std::max(0.0, seconds), static_cast<double>(m_rate)));
}

void TimelinePlayer::seek_frame(std::int64_t frame) {
    frame = std::max<std::int64_t>(0, frame);
    m_seek.store(frame);
    if (m_state.load() != static_cast<int>(State::playing)) m_origin.store(frame);
}

void TimelinePlayer::set_loop(double start, double end, bool enabled) {
    const double r = static_cast<double>(m_rate);
    m_loop_a.store(to_frames(std::max(0.0, start), r));
    m_loop_b.store(to_frames(std::max(0.0, end), r));
    m_loop_on.store(enabled);
    m_loop_version.fetch_add(1);
}

void TimelinePlayer::set_looping(bool enabled) {
    m_loop_on.store(enabled);
    m_loop_version.fetch_add(1);
}

double TimelinePlayer::loop_start() const noexcept {
    return static_cast<double>(m_loop_a.load()) / static_cast<double>(m_rate);
}

double TimelinePlayer::loop_end() const noexcept {
    return static_cast<double>(m_loop_b.load()) / static_cast<double>(m_rate);
}

std::int64_t TimelinePlayer::position_frame() const noexcept {
    const std::int64_t s = m_seek.load();
    return s >= 0 ? s : m_position.load();
}

double TimelinePlayer::position() const noexcept {
    return static_cast<double>(position_frame()) / static_cast<double>(m_rate);
}

void TimelinePlayer::render(float* output, std::size_t frames) noexcept {
    std::memset(output, 0, frames * m_channels * sizeof(float));
    if (m_dirty.load(std::memory_order_acquire) && m_mutex.try_lock()) {
        if (m_pending) {
            if (m_live && !m_live->same_layout(*m_pending)) m_restart = true;
            if (m_live) m_retired.push_back(std::move(m_live));
            m_live = std::move(m_pending);
            m_swaps.fetch_add(1, std::memory_order_relaxed);
        }
        m_dirty.store(false);
        m_mutex.unlock();
    }
    const std::uint64_t lv = m_loop_version.load();
    if (lv != m_seen_loop) {
        m_seen_loop = lv;
        m_engine->set_loop(m_loop_on.load(), m_loop_a.load(), m_loop_b.load());
        if (!m_restart) m_engine->rebase();
    }
    const std::int64_t seek = m_seek.load();
    std::int64_t target = -1;
    if (seek >= 0) {
        target = seek;
        m_restart = true;
    }
    const int st = m_state.load();
    if (st == static_cast<int>(State::stopped)) m_restart = true;
    if (st != static_cast<int>(State::playing) || !m_live) {
        if (target >= 0) {
            m_position.store(target);
            std::int64_t expected = target;
            m_seek.compare_exchange_strong(expected, -1);
        }
        m_peak.store(0.0f, std::memory_order_relaxed);
        return;
    }
    const detail::TimelineSnapshot& snap = *m_live;
    if (m_restart) {
        const std::int64_t from = target >= 0 ? target : m_position.load();
        m_engine->begin(snap, from);
        m_restart = false;
        if (target >= 0) {
            std::int64_t expected = target;
            m_seek.compare_exchange_strong(expected, -1);
        }
    }
    if (snap.channels == m_channels) {
        m_engine->process(snap, output, frames);
    }
    m_position.store(m_engine->position());
    float peak = 0.0f;
    const std::size_t n = frames * m_channels;
    for (std::size_t i = 0; i < n; ++i) peak = std::max(peak, std::fabs(output[i]));
    m_peak.store(peak, std::memory_order_relaxed);
}

bool TimelinePlayer::start(const StreamConfig& config, std::string* error) {
    if (m_stream.running()) return true;
    StreamConfig c = config;
    c.kind = DeviceKind::output;
    c.sample_rate = m_rate;
    c.channels = static_cast<std::uint32_t>(m_channels);
    if (!m_stream.open_playback(c, [this](float* out, std::size_t frames) { render(out, frames); }, error)) return false;
    return m_stream.start(error);
}

void TimelinePlayer::stop_output() {
    m_stream.close();
}

TrackRecorder::TrackRecorder() = default;

TrackRecorder::~TrackRecorder() {
    cancel();
}

bool TrackRecorder::begin(std::uint32_t sample_rate, std::size_t channels, double position, const TimelinePlayer* player) {
    if (m_recording.load() || m_run.load()) return false;
    if (m_writer.joinable()) m_writer.join();
    m_rate = sample_rate ? sample_rate : 48000;
    m_channels = channels ? std::min(channels, k_max_channels) : 1;
    m_ring.assign(static_cast<std::size_t>(m_rate) * m_channels * 4, 0.0f);
    m_head.store(0);
    m_tail.store(0);
    {
        std::lock_guard<std::mutex> lock(m_data_mutex);
        m_data.clear();
        m_data.reserve(static_cast<std::size_t>(m_rate) * m_channels * 30);
    }
    m_player = player;
    m_fixed_position = position;
    m_position.store(position);
    m_first.store(true);
    m_dropped.store(0);
    m_frames.store(0);
    m_peak.store(0.0f);
    m_run.store(true);
    m_writer = std::thread([this] { writer_loop(); });
    m_recording.store(true);
    return true;
}

bool TrackRecorder::start(const StreamConfig& config, const TimelinePlayer* player, double position, std::string* error) {
    if (m_recording.load()) {
        if (error) *error = "already recording";
        return false;
    }
    StreamConfig c = config;
    if (c.kind == DeviceKind::output) c.kind = DeviceKind::input;
    if (!m_stream.open_capture(c, [this](const float* in, std::size_t frames) { push(in, frames); }, error)) return false;
    if (!begin(m_stream.sample_rate(), m_stream.channels(), position, player)) {
        m_stream.close();
        if (error) *error = "can't start recording";
        return false;
    }
    if (!m_stream.start(error)) {
        cancel();
        return false;
    }
    return true;
}

void TrackRecorder::push(const float* interleaved, std::size_t frames) noexcept {
    if (!m_recording.load(std::memory_order_acquire) || frames == 0) return;
    if (m_first.load(std::memory_order_relaxed)) {
        m_position.store(m_player ? m_player->position() : m_fixed_position);
        m_first.store(false, std::memory_order_release);
    }
    const std::size_t cap = m_ring.size();
    const std::size_t head = m_head.load(std::memory_order_relaxed);
    const std::size_t tail = m_tail.load(std::memory_order_acquire);
    const std::size_t free_samples = cap - (head - tail);
    const std::size_t want = frames * m_channels;
    const std::size_t n = std::min(want, free_samples - free_samples % m_channels);
    float peak = 0.0f;
    for (std::size_t i = 0; i < n; ++i) {
        const float v = interleaved[i];
        m_ring[(head + i) % cap] = v;
        peak = std::max(peak, std::fabs(v));
    }
    m_head.store(head + n, std::memory_order_release);
    m_frames.fetch_add(n / m_channels, std::memory_order_relaxed);
    if (n < want) m_dropped.fetch_add((want - n) / m_channels, std::memory_order_relaxed);
    m_peak.store(peak, std::memory_order_relaxed);
}

void TrackRecorder::drain() {
    const std::size_t cap = m_ring.size();
    if (cap == 0) return;
    const std::size_t tail = m_tail.load(std::memory_order_relaxed);
    const std::size_t head = m_head.load(std::memory_order_acquire);
    if (head == tail) return;
    {
        std::lock_guard<std::mutex> lock(m_data_mutex);
        for (std::size_t i = tail; i != head; ++i) m_data.push_back(m_ring[i % cap]);
    }
    m_tail.store(head, std::memory_order_release);
}

void TrackRecorder::writer_loop() {
    while (m_run.load()) {
        drain();
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    drain();
}

double TrackRecorder::recorded_seconds() const noexcept {
    return m_rate ? static_cast<double>(m_frames.load()) / static_cast<double>(m_rate) : 0.0;
}

double TrackRecorder::start_position() const noexcept {
    return m_position.load();
}

Clip TrackRecorder::finish(const std::string& name) {
    const std::uint32_t capture_latency = m_stream.is_open() ? m_stream.latency_frames() : 0;
    const std::uint32_t capture_rate = m_stream.is_open() ? m_stream.sample_rate() : m_rate;
    m_recording.store(false);
    m_stream.close();
    m_run.store(false);
    if (m_writer.joinable()) m_writer.join();
    drain();
    std::vector<float> data;
    {
        std::lock_guard<std::mutex> lock(m_data_mutex);
        data.swap(m_data);
    }
    double latency = m_offset.load();
    if (m_compensate.load()) {
        if (capture_latency && capture_rate) latency += static_cast<double>(capture_latency) / static_cast<double>(capture_rate);
        if (m_player && m_player->output_running() && m_player->sample_rate()) latency += static_cast<double>(m_player->stream().latency_frames()) / static_cast<double>(m_player->sample_rate());
    }
    Clip clip = make_clip(std::make_shared<const AudioBuffer>(m_channels ? m_channels : 1, m_rate ? m_rate : 48000, std::move(data)), m_position.load() - latency, name);
    if (clip.position < 0.0) clip.trim_start(0.0);
    m_player = nullptr;
    return clip;
}

void TrackRecorder::cancel() {
    m_recording.store(false);
    m_stream.close();
    m_run.store(false);
    if (m_writer.joinable()) m_writer.join();
    std::lock_guard<std::mutex> lock(m_data_mutex);
    m_data.clear();
    m_player = nullptr;
}

} // namespace audio
} // namespace fizmo
