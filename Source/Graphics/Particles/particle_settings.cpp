#include "fizmo_library.hpp"
#include "particle_settings.hpp"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <utility>

namespace fizmo {
namespace particles {

namespace {

template <class Key>
int insert_key(Key* keys, int& count, int capacity, float t) noexcept {
    int at = count;
    for (int i = 0; i < count; ++i) {
        if (keys[i].t == t) return i;
        if (keys[i].t > t) { at = i; break; }
    }
    if (count >= capacity) return -1;
    for (int i = count; i > at; --i) keys[i] = keys[i - 1];
    ++count;
    keys[at].t = t;
    return at;
}

float clamp01(float t) noexcept { return t < 0.0f ? 0.0f : (t > 1.0f ? 1.0f : t); }

} // namespace

LifeCurve& LifeCurve::add(float t, float v) noexcept {
    const int i = insert_key(keys, count, kMaxKeys, clamp01(t));
    if (i >= 0) keys[i].v = v;
    return *this;
}

float LifeCurve::evaluate(float t) const noexcept {
    if (count <= 0) return 1.0f;
    if (t <= keys[0].t) return keys[0].v;
    for (int i = 1; i < count; ++i) {
        if (t > keys[i].t) continue;
        const float span = keys[i].t - keys[i - 1].t;
        const float f = span > 0.0f ? (t - keys[i - 1].t) / span : 1.0f;
        return keys[i - 1].v + (keys[i].v - keys[i - 1].v) * f;
    }
    return keys[count - 1].v;
}

bool LifeCurve::is_constant() const noexcept {
    for (int i = 1; i < count; ++i) if (keys[i].v != keys[0].v) return false;
    return true;
}

float LifeCurve::peak() const noexcept {
    float m = count > 0 ? keys[0].v : 1.0f;
    for (int i = 1; i < count; ++i) if (keys[i].v > m) m = keys[i].v;
    return m;
}

bool LifeCurve::operator==(const LifeCurve& o) const noexcept {
    if (count != o.count) return false;
    for (int i = 0; i < count; ++i) if (keys[i].t != o.keys[i].t || keys[i].v != o.keys[i].v) return false;
    return true;
}

LifeGradient& LifeGradient::add(float t, const ColorF& c) noexcept {
    const int i = insert_key(keys, count, kMaxKeys, clamp01(t));
    if (i >= 0) keys[i].color = c;
    return *this;
}

ColorF LifeGradient::evaluate(float t) const noexcept {
    if (count <= 0) return ColorF{};
    if (t <= keys[0].t) return keys[0].color;
    for (int i = 1; i < count; ++i) {
        if (t > keys[i].t) continue;
        const float span = keys[i].t - keys[i - 1].t;
        const float f = span > 0.0f ? (t - keys[i - 1].t) / span : 1.0f;
        const ColorF& a = keys[i - 1].color;
        const ColorF& b = keys[i].color;
        return { a.r + (b.r - a.r) * f, a.g + (b.g - a.g) * f, a.b + (b.b - a.b) * f, a.a + (b.a - a.a) * f };
    }
    return keys[count - 1].color;
}

bool LifeGradient::is_constant() const noexcept {
    for (int i = 1; i < count; ++i) if (keys[i].color != keys[0].color) return false;
    return true;
}

bool LifeGradient::operator==(const LifeGradient& o) const noexcept {
    if (count != o.count) return false;
    for (int i = 0; i < count; ++i) if (keys[i].t != o.keys[i].t || keys[i].color != o.keys[i].color) return false;
    return true;
}

namespace {

template <class E, std::size_t N>
const char* name_of(E v, const char* const (&names)[N]) noexcept {
    const std::size_t i = static_cast<std::size_t>(v);
    return i < N ? names[i] : "unknown";
}

std::string_view trim(std::string_view s) noexcept {
    while (!s.empty() && (s.front() == ' ' || s.front() == '\t' || s.front() == '\r' || s.front() == '\n')) s.remove_prefix(1);
    while (!s.empty() && (s.back() == ' ' || s.back() == '\t' || s.back() == '\r' || s.back() == '\n')) s.remove_suffix(1);
    return s;
}

bool same_name(std::string_view a, const char* b) noexcept {
    const std::size_t n = std::strlen(b);
    if (a.size() != n) return false;
    for (std::size_t i = 0; i < n; ++i) {
        char c = a[i];
        if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
        if (c == '-' || c == ' ') c = '_';
        if (c != b[i]) return false;
    }
    return true;
}

template <class E, std::size_t N>
bool parse_name(std::string_view text, E& out, const char* const (&names)[N]) noexcept {
    text = trim(text);
    for (std::size_t i = 0; i < N; ++i) {
        if (same_name(text, names[i])) { out = static_cast<E>(i); return true; }
    }
    return false;
}

const char* const kDimNames[]       = { "3d", "2d" };
const char* const kSpaceNames[]     = { "world", "local" };
const char* const kShapeNames[]     = { "point", "sphere", "hemisphere", "cone", "box", "circle", "ring", "line", "mesh" };
const char* const kDirectionNames[] = { "axis", "outward", "random" };
const char* const kBlendNames[]     = { "alpha", "additive", "premultiplied" };
const char* const kRenderNames[]    = { "billboard", "stretched", "horizontal", "vertical" };
const char* const kSortNames[]      = { "auto", "none", "approximate", "exact" };
const char* const kFlipbookNames[]  = { "over_life", "fps", "random" };
const char* const kOffscreenNames[] = { "auto", "simulate", "throttle", "pause" };
const char* const kTriggerNames[]   = { "death", "collision", "birth" };
const char* const kPresetNames[]    = { "sparks", "smoke", "fire", "explosion", "dust", "rain", "snow", "magic_sparkle", "debris", "bubbles", "steam", "embers", "impact" };

} // namespace

const char* to_string(ParticleDim v) noexcept { return name_of(v, kDimNames); }
const char* to_string(SimulationSpace v) noexcept { return name_of(v, kSpaceNames); }
const char* to_string(EmissionShape v) noexcept { return name_of(v, kShapeNames); }
const char* to_string(DirectionMode v) noexcept { return name_of(v, kDirectionNames); }
const char* to_string(ParticleBlend v) noexcept { return name_of(v, kBlendNames); }
const char* to_string(RenderMode v) noexcept { return name_of(v, kRenderNames); }
const char* to_string(SortMode v) noexcept { return name_of(v, kSortNames); }
const char* to_string(FlipbookMode v) noexcept { return name_of(v, kFlipbookNames); }
const char* to_string(OffscreenMode v) noexcept { return name_of(v, kOffscreenNames); }
const char* to_string(SubEmitterTrigger v) noexcept { return name_of(v, kTriggerNames); }
const char* to_string(ParticlePreset v) noexcept { return name_of(v, kPresetNames); }

bool parse(std::string_view text, ParticleDim& out) noexcept { return parse_name(text, out, kDimNames); }
bool parse(std::string_view text, SimulationSpace& out) noexcept { return parse_name(text, out, kSpaceNames); }
bool parse(std::string_view text, EmissionShape& out) noexcept { return parse_name(text, out, kShapeNames); }
bool parse(std::string_view text, DirectionMode& out) noexcept { return parse_name(text, out, kDirectionNames); }
bool parse(std::string_view text, ParticleBlend& out) noexcept { return parse_name(text, out, kBlendNames); }
bool parse(std::string_view text, RenderMode& out) noexcept { return parse_name(text, out, kRenderNames); }
bool parse(std::string_view text, SortMode& out) noexcept { return parse_name(text, out, kSortNames); }
bool parse(std::string_view text, FlipbookMode& out) noexcept { return parse_name(text, out, kFlipbookNames); }
bool parse(std::string_view text, OffscreenMode& out) noexcept { return parse_name(text, out, kOffscreenNames); }
bool parse(std::string_view text, SubEmitterTrigger& out) noexcept { return parse_name(text, out, kTriggerNames); }
bool parse(std::string_view text, ParticlePreset& out) noexcept { return parse_name(text, out, kPresetNames); }

EmitterSettings::EmitterSettings() = default;
EmitterSettings::EmitterSettings(const EmitterSettings&) = default;
EmitterSettings::EmitterSettings(EmitterSettings&&) noexcept = default;
EmitterSettings& EmitterSettings::operator=(const EmitterSettings&) = default;
EmitterSettings& EmitterSettings::operator=(EmitterSettings&&) noexcept = default;
EmitterSettings::~EmitterSettings() = default;

std::uint32_t EmitterSettings::frames() const noexcept {
    const int fx = frames_x < 1 ? 1 : frames_x;
    const int fy = frames_y < 1 ? 1 : frames_y;
    const int all = fx * fy;
    return static_cast<std::uint32_t>(frame_count > 0 && frame_count < all ? frame_count : all);
}

float EmitterSettings::estimated_reach() const noexcept {
    const float life = max_lifetime();
    const float v = speed.largest() < 0.0f ? -speed.largest() : speed.largest();
    float travel = v * life;
    if (drag > 0.0f) {
        const float limit = v / drag;
        if (limit < travel) travel = limit;
    }
    const float g = std::sqrt(gravity.x * gravity.x + gravity.y * gravity.y + gravity.z * gravity.z) * std::fabs(gravity_scale);
    float fall = 0.5f * g * life * life;
    if (drag > 0.0f) {
        const float limit = g / drag * life;
        if (limit < fall) fall = limit;
    }
    const float wind_len = std::sqrt(wind.x * wind.x + wind.y * wind.y + wind.z * wind.z) * life;
    float extent = shape_radius;
    const float box = 0.5f * std::sqrt(shape_size.x * shape_size.x + shape_size.y * shape_size.y + shape_size.z * shape_size.z);
    if (shape == EmissionShape::Box || shape == EmissionShape::Line) extent = box;
    if (shape == EmissionShape::Point) extent = 0.0f;
    return extent + travel + fall + wind_len + turbulence * life + size.largest() * size_over_life.peak();
}

float EmitterSettings::steady_count() const noexcept {
    const float mean_life = lifetime.mid();
    float n = rate * mean_life;
    for (const Burst& b : bursts) n += static_cast<float>(b.count) * b.probability;
    const float cap = static_cast<float>(max_particles);
    return n < cap ? n : cap;
}

bool EmitterSettings::operator==(const EmitterSettings& o) const {
    return to_text(*this) == to_text(o);
}

namespace {

void fmt_float(std::string& out, float v) {
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%.9g", static_cast<double>(v));
    out += buf;
}

bool read_float(std::string_view& s, float& v) {
    s = trim(s);
    while (!s.empty() && (s.front() == ',' || s.front() == ';' || s.front() == ' ')) s.remove_prefix(1);
    if (s.empty()) return false;
    char buf[64];
    std::size_t n = 0;
    while (n < s.size() && n < sizeof(buf) - 1 && s[n] != ',' && s[n] != ';' && s[n] != ' ' && s[n] != '\t') { buf[n] = s[n]; ++n; }
    buf[n] = '\0';
    char* end = nullptr;
    const double d = std::strtod(buf, &end);
    if (end == buf) return false;
    v = static_cast<float>(d);
    s.remove_prefix(n);
    return true;
}

struct Writer {
    std::string& out;
    std::string  prefix;

    void key(const char* k) { out += prefix; out += k; out += ": "; }

    void operator()(const char* k, std::string& v) {
        key(k);
        for (char c : v) out += (c == '\n' || c == '\r') ? ' ' : c;
        out += '\n';
    }
    void operator()(const char* k, bool& v) { key(k); out += v ? "true" : "false"; out += '\n'; }
    void operator()(const char* k, int& v) { key(k); out += std::to_string(v); out += '\n'; }
    void operator()(const char* k, std::uint32_t& v) { key(k); out += std::to_string(v); out += '\n'; }
    void operator()(const char* k, float& v) { key(k); fmt_float(out, v); out += '\n'; }
    void operator()(const char* k, Float3& v) { key(k); fmt_float(out, v.x); out += ", "; fmt_float(out, v.y); out += ", "; fmt_float(out, v.z); out += '\n'; }
    void operator()(const char* k, ColorF& v) {
        key(k); fmt_float(out, v.r); out += ", "; fmt_float(out, v.g); out += ", "; fmt_float(out, v.b); out += ", "; fmt_float(out, v.a); out += '\n';
    }
    void operator()(const char* k, RangeF& v) { key(k); fmt_float(out, v.lo); out += ", "; fmt_float(out, v.hi); out += '\n'; }
    void operator()(const char* k, LifeCurve& v) {
        key(k);
        for (int i = 0; i < v.count; ++i) {
            if (i) out += "; ";
            fmt_float(out, v.keys[i].t); out += ' '; fmt_float(out, v.keys[i].v);
        }
        out += '\n';
    }
    void operator()(const char* k, LifeGradient& v) {
        key(k);
        for (int i = 0; i < v.count; ++i) {
            if (i) out += "; ";
            const ColorF& c = v.keys[i].color;
            fmt_float(out, v.keys[i].t); out += ' ';
            fmt_float(out, c.r); out += ' '; fmt_float(out, c.g); out += ' '; fmt_float(out, c.b); out += ' '; fmt_float(out, c.a);
        }
        out += '\n';
    }
    template <class E>
    auto operator()(const char* k, E& v) -> decltype(to_string(v), void()) { key(k); out += to_string(v); out += '\n'; }
};

struct Reader {
    std::string_view key;
    std::string_view value;
    bool             matched = false;
    bool             ok = true;

    bool hit(const char* k) {
        if (matched || key != k) return false;
        matched = true;
        return true;
    }

    void operator()(const char* k, std::string& v) { if (hit(k)) v = std::string(value); }
    void operator()(const char* k, bool& v) {
        if (!hit(k)) return;
        if (same_name(value, "true") || value == "1" || same_name(value, "yes") || same_name(value, "on")) v = true;
        else if (same_name(value, "false") || value == "0" || same_name(value, "no") || same_name(value, "off")) v = false;
        else ok = false;
    }
    void operator()(const char* k, int& v) {
        if (!hit(k)) return;
        std::string_view s = value; float f = 0.0f;
        if (read_float(s, f)) v = static_cast<int>(std::lround(f)); else ok = false;
    }
    void operator()(const char* k, std::uint32_t& v) {
        if (!hit(k)) return;
        const std::string tmp(value);
        char* end = nullptr;
        const unsigned long long n = std::strtoull(tmp.c_str(), &end, 0);
        if (end == tmp.c_str()) ok = false; else v = static_cast<std::uint32_t>(n);
    }
    void operator()(const char* k, float& v) { if (!hit(k)) return; std::string_view s = value; if (!read_float(s, v)) ok = false; }
    void operator()(const char* k, Float3& v) {
        if (!hit(k)) return;
        std::string_view s = value;
        Float3 r;
        if (read_float(s, r.x) && read_float(s, r.y) && read_float(s, r.z)) v = r; else ok = false;
    }
    void operator()(const char* k, ColorF& v) {
        if (!hit(k)) return;
        std::string_view s = value;
        ColorF r;
        if (read_float(s, r.r) && read_float(s, r.g) && read_float(s, r.b)) { if (!read_float(s, r.a)) r.a = 1.0f; v = r; } else ok = false;
    }
    void operator()(const char* k, RangeF& v) {
        if (!hit(k)) return;
        std::string_view s = value;
        RangeF r;
        if (read_float(s, r.lo)) { if (!read_float(s, r.hi)) r.hi = r.lo; v = r; } else ok = false;
    }
    void operator()(const char* k, LifeCurve& v) {
        if (!hit(k)) return;
        std::string_view s = value;
        LifeCurve c;
        c.count = 0;
        float t = 0.0f, x = 0.0f;
        while (read_float(s, t)) {
            if (!read_float(s, x)) { ok = false; return; }
            c.add(t, x);
        }
        if (c.count == 0) { ok = false; return; }
        v = c;
    }
    void operator()(const char* k, LifeGradient& v) {
        if (!hit(k)) return;
        std::string_view s = value;
        LifeGradient g;
        g.count = 0;
        float t = 0.0f;
        while (read_float(s, t)) {
            ColorF c;
            if (!read_float(s, c.r) || !read_float(s, c.g) || !read_float(s, c.b) || !read_float(s, c.a)) { ok = false; return; }
            g.add(t, c);
        }
        if (g.count == 0) { ok = false; return; }
        v = g;
    }
    template <class E>
    auto operator()(const char* k, E& v) -> decltype(to_string(v), void()) {
        if (!hit(k)) return;
        if (!parse(value, v)) ok = false;
    }
};

template <class V>
void visit_fields(EmitterSettings& s, V& v) {
    v("name", s.name);
    v("dimension", s.dimension);
    v("space", s.space);
    v("max_particles", s.max_particles);
    v("priority", s.priority);
    v("seed", s.seed);
    v("lod_bias", s.lod_bias);
    v("duration", s.duration);
    v("loop", s.loop);
    v("start_delay", s.start_delay);
    v("prewarm", s.prewarm);
    v("auto_play", s.auto_play);
    v("rate", s.rate);
    v("rate_per_distance", s.rate_per_distance);
    v("lifetime", s.lifetime);
    v("shape", s.shape);
    v("shape_radius", s.shape_radius);
    v("shape_thickness", s.shape_thickness);
    v("shape_arc", s.shape_arc);
    v("shape_size", s.shape_size);
    v("direction", s.direction);
    v("direction_mode", s.direction_mode);
    v("spread", s.spread);
    v("speed", s.speed);
    v("inherit_velocity", s.inherit_velocity);
    v("size", s.size);
    v("rotation", s.rotation);
    v("spin", s.spin);
    v("color", s.color);
    v("color_alt", s.color_alt);
    v("gravity_scale", s.gravity_scale);
    v("gravity", s.gravity);
    v("drag", s.drag);
    v("wind", s.wind);
    v("turbulence", s.turbulence);
    v("turbulence_frequency", s.turbulence_frequency);
    v("turbulence_speed", s.turbulence_speed);
    v("size_over_life", s.size_over_life);
    v("color_over_life", s.color_over_life);
    v("alpha_over_life", s.alpha_over_life);
    v("speed_over_life", s.speed_over_life);
    v("spin_over_life", s.spin_over_life);
    v("render_mode", s.render_mode);
    v("stretch", s.stretch);
    v("blend", s.blend);
    v("lit", s.lit);
    v("soft", s.soft);
    v("soft_distance", s.soft_distance);
    v("near_fade", s.near_fade);
    v("sort", s.sort);
    v("frames_x", s.frames_x);
    v("frames_y", s.frames_y);
    v("frame_count", s.frame_count);
    v("flipbook", s.flipbook);
    v("frame_fps", s.frame_fps);
    v("random_start_frame", s.random_start_frame);
    v("texture_id", s.texture_id);
    v("texture", s.texture);
    v("collide", s.collide);
    v("bounce", s.bounce);
    v("friction", s.friction);
    v("kill_on_collision", s.kill_on_collision);
    v("collision_radius", s.collision_radius);
    v("min_bounce_speed", s.min_bounce_speed);
    v("collision_mask", s.collision_mask);
    v("offscreen", s.offscreen);
    v("cull_radius", s.cull_radius);
}


void write_settings(const EmitterSettings& cs, std::string& out, const std::string& prefix) {
    EmitterSettings& s = const_cast<EmitterSettings&>(cs);
    Writer w{ out, prefix };
    visit_fields(s, w);
    out += prefix; out += "bursts: "; out += std::to_string(s.bursts.size()); out += '\n';
    for (std::size_t i = 0; i < s.bursts.size(); ++i) {
        const Burst& b = s.bursts[i];
        out += prefix; out += "burst."; out += std::to_string(i); out += ": ";
        fmt_float(out, b.time); out += ' ';
        out += std::to_string(b.count); out += ' ';
        out += std::to_string(b.cycles); out += ' ';
        fmt_float(out, b.interval); out += ' ';
        fmt_float(out, b.probability); out += '\n';
    }
    out += prefix; out += "sub_emitters: "; out += std::to_string(s.sub_emitters.size()); out += '\n';
    for (std::size_t i = 0; i < s.sub_emitters.size(); ++i) {
        SubEmitter& sub = s.sub_emitters[i];
        Writer sw{ out, prefix + "sub." + std::to_string(i) + "." };
        sw("trigger", sub.trigger);
        sw("sub_count", sub.count);
        sw("sub_probability", sub.probability);
        sw("sub_inherit_velocity", sub.inherit_velocity);
        write_settings(sub.settings, out, sw.prefix);
    }
}

using KeyValue = std::pair<std::string_view, std::string_view>;

bool read_index(std::string_view& key, std::size_t& index) {
    std::size_t n = 0, i = 0;
    while (i < key.size() && key[i] >= '0' && key[i] <= '9') { n = n * 10 + static_cast<std::size_t>(key[i] - '0'); ++i; }
    if (i == 0 || n > 4096) return false;
    index = n;
    key.remove_prefix(i);
    return true;
}

void fail(bool& ok, std::string* error, std::string_view key, const char* why) {
    ok = false;
    if (error && error->empty()) { *error = std::string(key); *error += ": "; *error += why; }
}

bool read_settings(const std::vector<KeyValue>& lines, EmitterSettings& s, std::string* error, int depth) {
    bool ok = true;
    std::vector<std::vector<KeyValue>> subs;

    for (const KeyValue& kv : lines) {
        std::string_view key = kv.first;
        const std::string_view value = kv.second;

        if (key.substr(0, 4) == "sub.") {
            key.remove_prefix(4);
            std::size_t index = 0;
            if (!read_index(key, index) || key.empty() || key.front() != '.') { fail(ok, error, kv.first, "bad sub-emitter key"); continue; }
            key.remove_prefix(1);
            if (index >= s.sub_emitters.size()) s.sub_emitters.resize(index + 1);
            if (index >= subs.size()) subs.resize(index + 1);
            SubEmitter& sub = s.sub_emitters[index];
            Reader r{ key, value };
            r("trigger", sub.trigger);
            r("sub_count", sub.count);
            r("sub_probability", sub.probability);
            r("sub_inherit_velocity", sub.inherit_velocity);
            if (!r.ok) fail(ok, error, kv.first, "bad value");
            if (!r.matched) subs[index].push_back({ key, value });
            continue;
        }

        if (key.substr(0, 6) == "burst.") {
            key.remove_prefix(6);
            std::size_t index = 0;
            if (!read_index(key, index) || !key.empty()) { fail(ok, error, kv.first, "bad burst key"); continue; }
            if (index >= s.bursts.size()) s.bursts.resize(index + 1);
            Burst b;
            std::string_view v = value;
            float count = 0.0f, cycles = 0.0f;
            if (!read_float(v, b.time) || !read_float(v, count) || !read_float(v, cycles) || !read_float(v, b.interval) || !read_float(v, b.probability)) {
                fail(ok, error, kv.first, "expected: time count cycles interval probability");
                continue;
            }
            b.count = static_cast<int>(std::lround(count));
            b.cycles = static_cast<int>(std::lround(cycles));
            s.bursts[index] = b;
            continue;
        }

        if (key == "bursts" || key == "sub_emitters") {
            std::string_view v = value;
            float n = 0.0f;
            if (!read_float(v, n) || n < 0.0f || n > 4096.0f) { fail(ok, error, kv.first, "bad count"); continue; }
            if (key == "bursts") s.bursts.resize(static_cast<std::size_t>(n));
            else { s.sub_emitters.resize(static_cast<std::size_t>(n)); if (subs.size() < s.sub_emitters.size()) subs.resize(s.sub_emitters.size()); }
            continue;
        }

        Reader r{ key, value };
        visit_fields(s, r);
        if (!r.ok) fail(ok, error, kv.first, "bad value");
    }

    for (std::size_t i = 0; i < subs.size() && i < s.sub_emitters.size(); ++i) {
        if (depth >= 4) { fail(ok, error, "sub", "sub-emitters nested too deep"); break; }
        if (!read_settings(subs[i], s.sub_emitters[i].settings, error, depth + 1)) ok = false;
    }
    return ok;
}

} // namespace

std::string to_text(const EmitterSettings& settings) {
    std::string out;
    out.reserve(4096);
    write_settings(settings, out, std::string());
    return out;
}

bool from_text(std::string_view text, EmitterSettings& out, std::string* error) {
    if (error) error->clear();
    std::vector<KeyValue> lines;
    bool ok = true;
    while (!text.empty()) {
        const std::size_t nl = text.find('\n');
        std::string_view line = trim(text.substr(0, nl));
        text = nl == std::string_view::npos ? std::string_view() : text.substr(nl + 1);
        if (line.empty() || line.front() == '#') continue;
        const std::size_t colon = line.find(':');
        if (colon == std::string_view::npos) { fail(ok, error, line, "expected key: value"); continue; }
        lines.push_back({ trim(line.substr(0, colon)), trim(line.substr(colon + 1)) });
    }
    EmitterSettings s;
    if (!read_settings(lines, s, error, 0)) ok = false;
    out = std::move(s);
    return ok;
}

EmitterSettings to_2d(const EmitterSettings& in, float ppu) {
    EmitterSettings s = in;
    if (in.dimension == ParticleDim::D2) return s;
    s.dimension = ParticleDim::D2;
    s.shape_radius *= ppu;
    s.shape_size = { in.shape_size.x * ppu, in.shape_size.y * ppu, 0.0f };
    s.direction = { in.direction.x, -in.direction.y, 0.0f };
    s.speed = { in.speed.lo * ppu, in.speed.hi * ppu };
    s.size = { in.size.lo * ppu, in.size.hi * ppu };
    s.gravity = { in.gravity.x * ppu, -in.gravity.y * ppu, 0.0f };
    s.wind = { in.wind.x * ppu, -in.wind.y * ppu, 0.0f };
    s.turbulence *= ppu;
    s.turbulence_frequency = ppu > 0.0f ? in.turbulence_frequency / ppu : in.turbulence_frequency;
    s.rate_per_distance = ppu > 0.0f ? in.rate_per_distance / ppu : in.rate_per_distance;
    s.soft_distance *= ppu;
    s.near_fade = 0.0f;
    s.min_bounce_speed *= ppu;
    s.cull_radius *= ppu;
    s.lit = false;
    if (s.shape == EmissionShape::Sphere) s.shape = EmissionShape::Circle;
    if (s.render_mode == RenderMode::Horizontal || s.render_mode == RenderMode::Vertical) s.render_mode = RenderMode::Billboard;
    for (SubEmitter& sub : s.sub_emitters) sub.settings = to_2d(sub.settings, ppu);
    return s;
}

namespace {

EmitterSettings smoke_puff(float scale) {
    EmitterSettings s;
    s.name = "smoke_puff";
    s.max_particles = 256;
    s.rate = 0.0f;
    s.lifetime = { 0.8f, 1.4f };
    s.direction_mode = DirectionMode::Random;
    s.speed = { 0.1f * scale, 0.3f * scale };
    s.size = { 0.15f * scale, 0.25f * scale };
    s.size_over_life = LifeCurve::linear(0.6f, 2.0f);
    s.rotation = { 0.0f, 360.0f };
    s.spin = { -40.0f, 40.0f };
    s.color = { 0.45f, 0.45f, 0.45f, 0.5f };
    s.color_alt = { 0.3f, 0.3f, 0.3f, 0.5f };
    s.alpha_over_life = LifeCurve::three(0.0f, 0.15f, 1.0f, 0.0f);
    s.gravity_scale = -0.03f;
    s.drag = 1.5f;
    s.lit = true;
    s.loop = false;
    return s;
}

EmitterSettings sparks() {
    EmitterSettings s;
    s.name = "sparks";
    s.max_particles = 512;
    s.priority = 1;
    s.loop = false;
    s.duration = 0.1f;
    s.rate = 0.0f;
    s.bursts.push_back({ 0.0f, 40, 1, 1.0f, 1.0f });
    s.lifetime = { 0.4f, 0.9f };
    s.spread = 60.0f;
    s.speed = { 4.0f, 9.0f };
    s.size = { 0.03f, 0.06f };
    s.color = { 1.0f, 0.9f, 0.5f, 1.0f };
    s.color_alt = { 1.0f, 0.65f, 0.25f, 1.0f };
    s.color_over_life = LifeGradient::constant({ 1.0f, 1.0f, 1.0f, 1.0f });
    s.color_over_life.add(0.5f, { 1.0f, 0.7f, 0.3f, 1.0f }).add(1.0f, { 0.8f, 0.2f, 0.05f, 1.0f });
    s.alpha_over_life = LifeCurve::three(1.0f, 0.7f, 1.0f, 0.0f);
    s.gravity_scale = 1.0f;
    s.drag = 0.4f;
    s.render_mode = RenderMode::Stretched;
    s.stretch = 0.035f;
    s.blend = ParticleBlend::Additive;
    s.collide = true;
    s.bounce = 0.35f;
    s.friction = 0.3f;
    s.min_bounce_speed = 0.3f;
    SubEmitter puff;
    puff.trigger = SubEmitterTrigger::Collision;
    puff.count = 1;
    puff.probability = 0.25f;
    puff.settings = smoke_puff(0.5f);
    s.sub_emitters.push_back(puff);
    return s;
}

EmitterSettings smoke() {
    EmitterSettings s;
    s.name = "smoke";
    s.max_particles = 256;
    s.rate = 12.0f;
    s.lifetime = { 3.0f, 5.0f };
    s.shape = EmissionShape::Circle;
    s.shape_radius = 0.3f;
    s.spread = 10.0f;
    s.speed = { 0.4f, 0.8f };
    s.size = { 0.6f, 1.0f };
    s.size_over_life = LifeCurve::linear(0.6f, 2.5f);
    s.rotation = { 0.0f, 360.0f };
    s.spin = { -30.0f, 30.0f };
    s.spin_over_life = LifeCurve::linear(1.0f, 0.2f);
    s.color = { 0.5f, 0.5f, 0.5f, 0.6f };
    s.color_alt = { 0.35f, 0.35f, 0.35f, 0.6f };
    s.alpha_over_life = LifeCurve::three(0.0f, 0.2f, 1.0f, 0.0f);
    s.gravity_scale = -0.05f;
    s.drag = 0.6f;
    s.turbulence = 0.6f;
    s.turbulence_frequency = 0.4f;
    s.lit = true;
    s.soft = true;
    s.near_fade = 1.0f;
    return s;
}

EmitterSettings fire() {
    EmitterSettings s;
    s.name = "fire";
    s.max_particles = 256;
    s.priority = 1;
    s.rate = 40.0f;
    s.lifetime = { 0.6f, 1.1f };
    s.shape = EmissionShape::Circle;
    s.shape_radius = 0.25f;
    s.spread = 8.0f;
    s.speed = { 0.8f, 1.6f };
    s.size = { 0.4f, 0.6f };
    s.size_over_life = LifeCurve::three(0.6f, 0.3f, 1.0f, 0.2f);
    s.rotation = { 0.0f, 360.0f };
    s.spin = { -60.0f, 60.0f };
    s.color = { 1.0f, 0.95f, 0.8f, 1.0f };
    s.color_over_life = LifeGradient::constant({ 1.0f, 0.9f, 0.6f, 1.0f });
    s.color_over_life.add(0.4f, { 1.0f, 0.5f, 0.1f, 1.0f }).add(1.0f, { 0.6f, 0.1f, 0.02f, 1.0f });
    s.alpha_over_life = LifeCurve::three(0.0f, 0.15f, 1.0f, 0.0f);
    s.gravity_scale = -0.25f;
    s.turbulence = 1.0f;
    s.turbulence_frequency = 0.8f;
    s.turbulence_speed = 1.5f;
    s.blend = ParticleBlend::Additive;
    s.soft = true;
    s.near_fade = 0.5f;
    return s;
}

EmitterSettings explosion() {
    EmitterSettings s;
    s.name = "explosion";
    s.max_particles = 128;
    s.priority = 2;
    s.loop = false;
    s.duration = 0.1f;
    s.rate = 0.0f;
    s.bursts.push_back({ 0.0f, 60, 1, 1.0f, 1.0f });
    s.lifetime = { 0.5f, 1.0f };
    s.shape = EmissionShape::Sphere;
    s.shape_radius = 0.5f;
    s.direction_mode = DirectionMode::Outward;
    s.speed = { 3.0f, 8.0f };
    s.size = { 0.6f, 1.2f };
    s.size_over_life = LifeCurve::linear(0.5f, 2.0f);
    s.rotation = { 0.0f, 360.0f };
    s.spin = { -90.0f, 90.0f };
    s.color_over_life = LifeGradient::constant({ 1.0f, 0.95f, 0.7f, 1.0f });
    s.color_over_life.add(0.25f, { 1.0f, 0.55f, 0.15f, 1.0f }).add(0.6f, { 0.5f, 0.12f, 0.04f, 1.0f }).add(1.0f, { 0.15f, 0.12f, 0.1f, 1.0f });
    s.alpha_over_life = LifeCurve::linear(1.0f, 0.0f);
    s.gravity_scale = -0.1f;
    s.drag = 3.0f;
    s.blend = ParticleBlend::Additive;
    s.soft = true;
    s.near_fade = 1.0f;
    SubEmitter smoke_trail;
    smoke_trail.trigger = SubEmitterTrigger::Death;
    smoke_trail.count = 1;
    smoke_trail.probability = 0.6f;
    smoke_trail.inherit_velocity = 0.2f;
    smoke_trail.settings = smoke_puff(3.0f);
    smoke_trail.settings.lifetime = { 1.5f, 3.0f };
    s.sub_emitters.push_back(smoke_trail);
    SubEmitter shrapnel;
    shrapnel.trigger = SubEmitterTrigger::Birth;
    shrapnel.count = 1;
    shrapnel.probability = 0.5f;
    shrapnel.inherit_velocity = 1.5f;
    shrapnel.settings = sparks();
    shrapnel.settings.name = "explosion_sparks";
    shrapnel.settings.bursts.clear();
    shrapnel.settings.sub_emitters.clear();
    shrapnel.settings.speed = { 0.0f, 1.0f };
    shrapnel.settings.direction_mode = DirectionMode::Random;
    s.sub_emitters.push_back(shrapnel);
    return s;
}

EmitterSettings dust() {
    EmitterSettings s;
    s.name = "dust";
    s.max_particles = 256;
    s.priority = -1;
    s.rate = 6.0f;
    s.lifetime = { 4.0f, 7.0f };
    s.shape = EmissionShape::Box;
    s.shape_size = { 8.0f, 2.0f, 8.0f };
    s.direction_mode = DirectionMode::Random;
    s.speed = { 0.02f, 0.08f };
    s.size = { 0.02f, 0.05f };
    s.color = { 0.9f, 0.85f, 0.7f, 0.5f };
    s.alpha_over_life = LifeCurve::three(0.0f, 0.3f, 1.0f, 0.0f);
    s.drag = 1.0f;
    s.turbulence = 0.15f;
    s.turbulence_frequency = 0.3f;
    s.lit = true;
    s.sort = SortMode::None;
    s.prewarm = true;
    return s;
}

EmitterSettings rain() {
    EmitterSettings s;
    s.name = "rain";
    s.max_particles = 4096;
    s.priority = -1;
    s.rate = 600.0f;
    s.lifetime = { 1.0f, 1.2f };
    s.shape = EmissionShape::Box;
    s.shape_size = { 30.0f, 0.1f, 30.0f };
    s.direction = { 0.0f, -1.0f, 0.0f };
    s.spread = 2.0f;
    s.speed = { 10.0f, 12.0f };
    s.size = { 0.015f, 0.02f };
    s.color = { 0.7f, 0.75f, 0.85f, 0.5f };
    s.alpha_over_life = LifeCurve::constant(1.0f);
    s.gravity_scale = 1.0f;
    s.render_mode = RenderMode::Stretched;
    s.stretch = 0.06f;
    s.sort = SortMode::None;
    s.collide = true;
    s.kill_on_collision = true;
    s.prewarm = true;
    s.offscreen = OffscreenMode::Simulate;
    SubEmitter splash;
    splash.trigger = SubEmitterTrigger::Collision;
    splash.count = 2;
    splash.probability = 0.3f;
    splash.settings.name = "rain_splash";
    splash.settings.max_particles = 1024;
    splash.settings.rate = 0.0f;
    splash.settings.loop = false;
    splash.settings.lifetime = { 0.15f, 0.3f };
    splash.settings.spread = 50.0f;
    splash.settings.speed = { 0.8f, 1.6f };
    splash.settings.size = { 0.01f, 0.015f };
    splash.settings.color = { 0.75f, 0.8f, 0.9f, 0.6f };
    splash.settings.gravity_scale = 1.0f;
    splash.settings.sort = SortMode::None;
    s.sub_emitters.push_back(splash);
    return s;
}

EmitterSettings snow() {
    EmitterSettings s;
    s.name = "snow";
    s.max_particles = 3072;
    s.priority = -1;
    s.rate = 150.0f;
    s.lifetime = { 8.0f, 10.0f };
    s.shape = EmissionShape::Box;
    s.shape_size = { 30.0f, 0.1f, 30.0f };
    s.direction = { 0.0f, -1.0f, 0.0f };
    s.spread = 20.0f;
    s.speed = { 0.5f, 1.0f };
    s.size = { 0.03f, 0.06f };
    s.rotation = { 0.0f, 360.0f };
    s.spin = { -90.0f, 90.0f };
    s.color = { 1.0f, 1.0f, 1.0f, 0.9f };
    s.alpha_over_life = LifeCurve::three(0.0f, 0.05f, 1.0f, 1.0f);
    s.gravity_scale = 0.05f;
    s.drag = 0.8f;
    s.wind = { 0.3f, 0.0f, 0.1f };
    s.turbulence = 0.4f;
    s.turbulence_frequency = 0.3f;
    s.sort = SortMode::None;
    s.collide = true;
    s.kill_on_collision = true;
    s.prewarm = true;
    s.offscreen = OffscreenMode::Simulate;
    return s;
}

EmitterSettings magic_sparkle() {
    EmitterSettings s;
    s.name = "magic_sparkle";
    s.max_particles = 256;
    s.rate = 30.0f;
    s.lifetime = { 0.8f, 1.5f };
    s.shape = EmissionShape::Sphere;
    s.shape_radius = 0.6f;
    s.shape_thickness = 0.0f;
    s.direction_mode = DirectionMode::Random;
    s.speed = { 0.1f, 0.4f };
    s.size = { 0.05f, 0.1f };
    s.size_over_life = LifeCurve::three(0.0f, 0.2f, 1.0f, 0.0f);
    s.rotation = { 0.0f, 360.0f };
    s.spin = { -180.0f, 180.0f };
    s.color = { 0.7f, 0.85f, 1.0f, 1.0f };
    s.color_alt = { 0.95f, 0.6f, 1.0f, 1.0f };
    s.color_over_life = LifeGradient::linear({ 1.0f, 1.0f, 1.0f, 1.0f }, { 0.8f, 0.6f, 1.0f, 1.0f });
    s.alpha_over_life = LifeCurve::constant(1.0f);
    s.gravity_scale = -0.05f;
    s.turbulence = 0.8f;
    s.turbulence_frequency = 1.5f;
    s.blend = ParticleBlend::Additive;
    s.sort = SortMode::None;
    return s;
}

EmitterSettings debris() {
    EmitterSettings s;
    s.name = "debris";
    s.max_particles = 128;
    s.loop = false;
    s.duration = 0.1f;
    s.rate = 0.0f;
    s.bursts.push_back({ 0.0f, 25, 1, 1.0f, 1.0f });
    s.lifetime = { 2.0f, 3.0f };
    s.shape = EmissionShape::Sphere;
    s.shape_radius = 0.2f;
    s.spread = 50.0f;
    s.speed = { 3.0f, 7.0f };
    s.size = { 0.05f, 0.15f };
    s.rotation = { 0.0f, 360.0f };
    s.spin = { -360.0f, 360.0f };
    s.color = { 0.45f, 0.4f, 0.35f, 1.0f };
    s.color_alt = { 0.3f, 0.28f, 0.25f, 1.0f };
    s.alpha_over_life = LifeCurve::three(1.0f, 0.85f, 1.0f, 0.0f);
    s.gravity_scale = 1.0f;
    s.drag = 0.1f;
    s.collide = true;
    s.bounce = 0.3f;
    s.friction = 0.6f;
    s.min_bounce_speed = 0.5f;
    s.lit = true;
    s.sort = SortMode::None;
    return s;
}

EmitterSettings bubbles() {
    EmitterSettings s;
    s.name = "bubbles";
    s.max_particles = 128;
    s.rate = 8.0f;
    s.lifetime = { 2.0f, 4.0f };
    s.shape = EmissionShape::Circle;
    s.shape_radius = 0.3f;
    s.spread = 10.0f;
    s.speed = { 0.3f, 0.6f };
    s.size = { 0.04f, 0.1f };
    s.color = { 0.8f, 0.9f, 1.0f, 0.6f };
    s.alpha_over_life = LifeCurve::three(0.0f, 0.1f, 1.0f, 1.0f);
    s.gravity_scale = -0.15f;
    s.drag = 1.0f;
    s.turbulence = 0.3f;
    s.turbulence_frequency = 1.0f;
    s.sort = SortMode::None;
    return s;
}

EmitterSettings steam() {
    EmitterSettings s;
    s.name = "steam";
    s.max_particles = 192;
    s.rate = 25.0f;
    s.lifetime = { 1.5f, 2.5f };
    s.shape = EmissionShape::Cone;
    s.shape_radius = 0.1f;
    s.spread = 15.0f;
    s.speed = { 1.0f, 2.0f };
    s.size = { 0.15f, 0.25f };
    s.size_over_life = LifeCurve::linear(0.5f, 3.0f);
    s.rotation = { 0.0f, 360.0f };
    s.spin = { -20.0f, 20.0f };
    s.color = { 1.0f, 1.0f, 1.0f, 0.35f };
    s.alpha_over_life = LifeCurve::three(0.0f, 0.15f, 1.0f, 0.0f);
    s.gravity_scale = -0.1f;
    s.drag = 1.2f;
    s.turbulence = 0.5f;
    s.lit = true;
    s.soft = true;
    s.near_fade = 0.5f;
    return s;
}

EmitterSettings embers() {
    EmitterSettings s;
    s.name = "embers";
    s.max_particles = 128;
    s.rate = 10.0f;
    s.lifetime = { 2.0f, 4.0f };
    s.shape = EmissionShape::Circle;
    s.shape_radius = 0.4f;
    s.spread = 25.0f;
    s.speed = { 0.5f, 1.5f };
    s.size = { 0.02f, 0.04f };
    s.color_over_life = LifeGradient::constant({ 1.0f, 0.8f, 0.3f, 1.0f });
    s.color_over_life.add(0.5f, { 1.0f, 0.3f, 0.05f, 1.0f }).add(1.0f, { 0.5f, 0.05f, 0.0f, 1.0f });
    s.alpha_over_life = LifeCurve::three(1.0f, 0.8f, 1.0f, 0.0f);
    s.gravity_scale = -0.15f;
    s.drag = 0.5f;
    s.turbulence = 1.2f;
    s.turbulence_frequency = 0.7f;
    s.blend = ParticleBlend::Additive;
    s.sort = SortMode::None;
    return s;
}

EmitterSettings impact() {
    EmitterSettings s;
    s.name = "impact";
    s.max_particles = 64;
    s.loop = false;
    s.duration = 0.1f;
    s.rate = 0.0f;
    s.bursts.push_back({ 0.0f, 12, 1, 1.0f, 1.0f });
    s.lifetime = { 0.3f, 0.6f };
    s.shape = EmissionShape::Hemisphere;
    s.shape_radius = 0.05f;
    s.direction_mode = DirectionMode::Outward;
    s.speed = { 1.0f, 3.0f };
    s.size = { 0.08f, 0.15f };
    s.size_over_life = LifeCurve::linear(0.5f, 2.0f);
    s.rotation = { 0.0f, 360.0f };
    s.color = { 0.75f, 0.7f, 0.62f, 0.8f };
    s.alpha_over_life = LifeCurve::linear(1.0f, 0.0f);
    s.drag = 4.0f;
    s.lit = true;
    SubEmitter chips;
    chips.trigger = SubEmitterTrigger::Birth;
    chips.count = 1;
    chips.probability = 0.4f;
    chips.inherit_velocity = 1.5f;
    chips.settings = debris();
    chips.settings.name = "impact_chips";
    chips.settings.bursts.clear();
    chips.settings.speed = { 0.0f, 0.5f };
    chips.settings.size = { 0.02f, 0.04f };
    chips.settings.lifetime = { 0.6f, 1.0f };
    s.sub_emitters.push_back(chips);
    return s;
}

} // namespace

EmitterSettings preset(ParticlePreset which, ParticleDim dimension, float pixels_per_unit) {
    EmitterSettings s;
    switch (which) {
        case ParticlePreset::Sparks:       s = sparks(); break;
        case ParticlePreset::Smoke:        s = smoke(); break;
        case ParticlePreset::Fire:         s = fire(); break;
        case ParticlePreset::Explosion:    s = explosion(); break;
        case ParticlePreset::Dust:         s = dust(); break;
        case ParticlePreset::Rain:         s = rain(); break;
        case ParticlePreset::Snow:         s = snow(); break;
        case ParticlePreset::MagicSparkle: s = magic_sparkle(); break;
        case ParticlePreset::Debris:       s = debris(); break;
        case ParticlePreset::Bubbles:      s = bubbles(); break;
        case ParticlePreset::Steam:        s = steam(); break;
        case ParticlePreset::Embers:       s = embers(); break;
        case ParticlePreset::Impact:       s = impact(); break;
        default: break;
    }
    return dimension == ParticleDim::D2 ? to_2d(s, pixels_per_unit) : s;
}

} // namespace particles
} // namespace fizmo
