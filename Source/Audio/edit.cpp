#include "edit.hpp"
#include "format.hpp"

#include <algorithm>
#include <cmath>

namespace fizmo {
namespace audio {

namespace {

const double k_pi = 3.14159265358979323846;

std::size_t seconds_to_frames(const AudioBuffer& buffer, double seconds) noexcept {
    if (!(seconds > 0.0)) return 0;
    return static_cast<std::size_t>(std::llround(seconds * static_cast<double>(buffer.sample_rate())));
}

bool frame_loud(const AudioBuffer& buffer, std::size_t f, float threshold) noexcept {
    const float* p = buffer.frame(f);
    for (std::size_t ch = 0; ch < buffer.channels(); ++ch) {
        if (std::fabs(p[ch]) > threshold) return true;
    }
    return false;
}

} // namespace

float fade_shape(FadeCurve curve, double t) noexcept {
    t = std::min(1.0, std::max(0.0, t));
    switch (curve) {
        case FadeCurve::linear: return static_cast<float>(t);
        case FadeCurve::equal_power: return static_cast<float>(std::sin(t * k_pi * 0.5));
        case FadeCurve::logarithmic: return static_cast<float>(std::log10(1.0 + 999.0 * t) / 3.0);
        case FadeCurve::exponential: return static_cast<float>((std::pow(1000.0, t) - 1.0) / 999.0);
        case FadeCurve::s_curve: return static_cast<float>(0.5 - 0.5 * std::cos(t * k_pi));
    }
    return static_cast<float>(t);
}

void fade(AudioBuffer& buffer, std::size_t start_frame, std::size_t frames, float from_gain, float to_gain, FadeCurve curve) {
    const std::size_t total = buffer.frames();
    if (start_frame >= total || frames == 0) return;
    const std::size_t n = std::min(frames, total - start_frame);
    const std::size_t c = buffer.channels();
    const bool rising = to_gain >= from_gain;
    const double span = static_cast<double>(frames);
    for (std::size_t i = 0; i < n; ++i) {
        const double t = (static_cast<double>(i) + 0.5) / span;
        const float g = rising ? from_gain + (to_gain - from_gain) * fade_shape(curve, t) : to_gain + (from_gain - to_gain) * fade_shape(curve, 1.0 - t);
        float* p = buffer.frame(start_frame + i);
        for (std::size_t ch = 0; ch < c; ++ch) p[ch] *= g;
    }
}

void fade_in(AudioBuffer& buffer, double seconds, FadeCurve curve) {
    const std::size_t n = std::min(buffer.frames(), seconds_to_frames(buffer, seconds));
    fade(buffer, 0, n, 0.0f, 1.0f, curve);
}

void fade_out(AudioBuffer& buffer, double seconds, FadeCurve curve) {
    const std::size_t n = std::min(buffer.frames(), seconds_to_frames(buffer, seconds));
    fade(buffer, buffer.frames() - n, n, 1.0f, 0.0f, curve);
}

AudioBuffer crossfade(const AudioBuffer& a, const AudioBuffer& b_in, double seconds, FadeCurve curve) {
    if (a.empty()) return b_in;
    if (b_in.empty()) return a;
    AudioBuffer converted;
    const AudioBuffer* bp = &b_in;
    if (b_in.channels() != a.channels() || b_in.sample_rate() != a.sample_rate()) {
        converted = b_in.converted(a.channels(), a.sample_rate());
        bp = &converted;
    }
    const AudioBuffer& b = *bp;
    const std::size_t overlap = std::min(seconds_to_frames(a, seconds), std::min(a.frames(), b.frames()));
    const std::size_t c = a.channels();
    AudioBuffer out(c, a.sample_rate(), a.frames() + b.frames() - overlap);
    const std::size_t head = a.frames() - overlap;
    std::copy(a.data(), a.data() + head * c, out.data());
    const double span = static_cast<double>(overlap);
    for (std::size_t i = 0; i < overlap; ++i) {
        const double t = (static_cast<double>(i) + 0.5) / span;
        const float gin = fade_shape(curve, t);
        const float gout = fade_shape(curve, 1.0 - t);
        const float* pa = a.frame(head + i);
        const float* pb = b.frame(i);
        float* po = out.frame(head + i);
        for (std::size_t ch = 0; ch < c; ++ch) po[ch] = pa[ch] * gout + pb[ch] * gin;
    }
    std::copy(b.data() + overlap * c, b.data() + b.sample_count(), out.data() + (head + overlap) * c);
    return out;
}

void apply_gain_envelope(AudioBuffer& buffer, std::vector<std::pair<double, float>> points) {
    if (points.empty() || buffer.empty() || buffer.sample_rate() == 0) return;
    std::stable_sort(points.begin(), points.end(), [](const std::pair<double, float>& x, const std::pair<double, float>& y) { return x.first < y.first; });
    const std::size_t c = buffer.channels();
    const double rate = static_cast<double>(buffer.sample_rate());
    std::size_t seg = 0;
    for (std::size_t f = 0; f < buffer.frames(); ++f) {
        const double t = static_cast<double>(f) / rate;
        while (seg + 1 < points.size() && points[seg + 1].first <= t) ++seg;
        double db = 0.0;
        if (t <= points.front().first) {
            db = points.front().second;
        } else if (seg + 1 >= points.size()) {
            db = points.back().second;
        } else {
            const double t0 = points[seg].first;
            const double t1 = points[seg + 1].first;
            const double u = t1 > t0 ? (t - t0) / (t1 - t0) : 1.0;
            db = points[seg].second + (points[seg + 1].second - points[seg].second) * u;
        }
        const float g = db <= -144.0 ? 0.0f : static_cast<float>(std::pow(10.0, db / 20.0));
        float* p = buffer.frame(f);
        for (std::size_t ch = 0; ch < c; ++ch) p[ch] *= g;
    }
}

void reverse(AudioBuffer& buffer) {
    const std::size_t n = buffer.frames();
    const std::size_t c = buffer.channels();
    for (std::size_t i = 0; i < n / 2; ++i) {
        float* x = buffer.frame(i);
        float* y = buffer.frame(n - 1 - i);
        for (std::size_t ch = 0; ch < c; ++ch) std::swap(x[ch], y[ch]);
    }
}

void invert_polarity(AudioBuffer& buffer) {
    for (float& s : buffer.samples()) s = -s;
}

void remove_dc(AudioBuffer& buffer) {
    const std::size_t n = buffer.frames();
    const std::size_t c = buffer.channels();
    if (n == 0) return;
    for (std::size_t ch = 0; ch < c; ++ch) {
        double sum = 0.0;
        for (std::size_t f = 0; f < n; ++f) sum += buffer.at(f, ch);
        const float mean = static_cast<float>(sum / static_cast<double>(n));
        for (std::size_t f = 0; f < n; ++f) buffer.at(f, ch) -= mean;
    }
}

std::vector<FrameRange> detect_silence(const AudioBuffer& buffer, float threshold_db, double min_duration_seconds) {
    std::vector<FrameRange> out;
    const std::size_t n = buffer.frames();
    if (n == 0) return out;
    const float threshold = db_to_gain(threshold_db);
    const std::size_t min_frames = std::max<std::size_t>(1, seconds_to_frames(buffer, min_duration_seconds));
    std::size_t start = 0;
    bool in_silence = false;
    for (std::size_t f = 0; f < n; ++f) {
        const bool loud = frame_loud(buffer, f, threshold);
        if (!loud && !in_silence) {
            in_silence = true;
            start = f;
        } else if (loud && in_silence) {
            in_silence = false;
            if (f - start >= min_frames) out.emplace_back(start, f);
        }
    }
    if (in_silence && n - start >= min_frames) out.emplace_back(start, n);
    return out;
}

std::vector<FrameRange> trim_silence(AudioBuffer& buffer, float threshold_db, double keep_ms) {
    std::vector<FrameRange> removed;
    const std::size_t n = buffer.frames();
    if (n == 0) return removed;
    const float threshold = db_to_gain(threshold_db);
    std::size_t first = n;
    std::size_t last = 0;
    for (std::size_t f = 0; f < n; ++f) {
        if (frame_loud(buffer, f, threshold)) {
            first = f;
            break;
        }
    }
    if (first == n) {
        removed.emplace_back(0, n);
        buffer.clear();
        return removed;
    }
    for (std::size_t f = n; f-- > 0;) {
        if (frame_loud(buffer, f, threshold)) {
            last = f;
            break;
        }
    }
    const std::size_t keep = seconds_to_frames(buffer, keep_ms * 0.001);
    const std::size_t start = first > keep ? first - keep : 0;
    const std::size_t end = std::min(n, last + 1 + keep);
    if (start > 0) removed.emplace_back(0, start);
    if (end < n) removed.emplace_back(end, n);
    if (end < n) buffer.erase(end, n - end);
    if (start > 0) buffer.erase(0, start);
    return removed;
}

std::vector<AudioBuffer> split_on_silence(const AudioBuffer& buffer, float threshold_db, double min_silence_seconds, double keep_ms) {
    std::vector<AudioBuffer> out;
    const std::size_t n = buffer.frames();
    if (n == 0) return out;
    const std::vector<FrameRange> silences = detect_silence(buffer, threshold_db, min_silence_seconds);
    const std::size_t keep = seconds_to_frames(buffer, keep_ms * 0.001);
    std::size_t pos = 0;
    for (std::size_t i = 0; i <= silences.size(); ++i) {
        const std::size_t seg_end = i < silences.size() ? silences[i].first : n;
        if (seg_end > pos) {
            const std::size_t lo = pos >= keep ? pos - keep : 0;
            const std::size_t hi = std::min(i < silences.size() ? silences[i].second : n, seg_end + keep);
            const std::size_t a = i > 0 ? std::max(lo, silences[i - 1].first) : lo;
            out.push_back(buffer.slice(a, hi - a));
        }
        if (i < silences.size()) pos = silences[i].second;
    }
    return out;
}

AudioBuffer mix(const AudioBuffer& a, const AudioBuffer& b, float gain_b) {
    if (a.channels() == 0) {
        AudioBuffer out = b;
        out.apply_gain(gain_b);
        return out;
    }
    AudioBuffer out = a;
    if (b.empty()) return out;
    if (b.sample_rate() != a.sample_rate() || b.channels() != a.channels()) {
        out.mix_in(b.converted(a.channels(), a.sample_rate()), 0, gain_b);
    } else {
        out.mix_in(b, 0, gain_b);
    }
    return out;
}

void swap_channels(AudioBuffer& buffer, std::size_t first, std::size_t second) {
    const std::size_t c = buffer.channels();
    if (first >= c || second >= c || first == second) return;
    for (std::size_t f = 0; f < buffer.frames(); ++f) {
        float* p = buffer.frame(f);
        std::swap(p[first], p[second]);
    }
}

AudioBuffer mono_to_stereo(const AudioBuffer& buffer) {
    if (buffer.channels() != 1) return buffer.with_channels(2);
    AudioBuffer out(2, buffer.sample_rate(), buffer.frames());
    for (std::size_t f = 0; f < buffer.frames(); ++f) {
        const float v = buffer.at(f, 0);
        out.at(f, 0) = v;
        out.at(f, 1) = v;
    }
    return out;
}

AudioBuffer stereo_to_mono(const AudioBuffer& buffer) {
    const std::size_t c = buffer.channels();
    if (c <= 1) return buffer;
    AudioBuffer out(1, buffer.sample_rate(), buffer.frames());
    const float scale = 1.0f / static_cast<float>(c);
    for (std::size_t f = 0; f < buffer.frames(); ++f) {
        const float* p = buffer.frame(f);
        float s = 0.0f;
        for (std::size_t ch = 0; ch < c; ++ch) s += p[ch];
        out.at(f, 0) = s * scale;
    }
    return out;
}

void pad(AudioBuffer& buffer, double before_seconds, double after_seconds) {
    const std::size_t before = seconds_to_frames(buffer, before_seconds);
    const std::size_t after = seconds_to_frames(buffer, after_seconds);
    if (before == 0 && after == 0) return;
    const std::size_t c = buffer.channels();
    std::vector<float> s(buffer.sample_count() + (before + after) * c, 0.0f);
    std::copy(buffer.samples().begin(), buffer.samples().end(), s.begin() + static_cast<std::ptrdiff_t>(before * c));
    buffer.samples().swap(s);
}

} // namespace audio
} // namespace fizmo
