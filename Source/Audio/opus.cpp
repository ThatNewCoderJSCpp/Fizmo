#include "opus.hpp"

#include "file_io.hpp"
#include "ogg.hpp"
#include "opus_internal.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <deque>
#include <new>
#include <utility>

namespace fizmo {
namespace audio {

namespace {

using opus_detail::CeltDecoder;
using opus_detail::RangeDecoder;
using opus_detail::Resampler;
using opus_detail::SilkDecoder;

const int k_max_frames = 48;
const int k_mode_silk = 0;
const int k_mode_hybrid = 1;
const int k_mode_celt = 2;

struct Packet {
    int toc = 0;
    int count = 0;
    const std::uint8_t* frames[k_max_frames] = {};
    int sizes[k_max_frames] = {};
    std::size_t consumed = 0;
};

int frame_samples(int toc) noexcept {
    const int config = toc >> 3;
    if (config < 12) {
        static const int s[4] = { 480, 960, 1920, 2880 };
        return s[config & 3];
    }
    if (config < 16) return (config & 1) ? 960 : 480;
    static const int c[4] = { 120, 240, 480, 960 };
    return c[config & 3];
}

int mode_of(int toc) noexcept {
    const int config = toc >> 3;
    if (config < 12) return k_mode_silk;
    if (config < 16) return k_mode_hybrid;
    return k_mode_celt;
}

int bandwidth_of(int toc) noexcept {
    const int config = toc >> 3;
    if (config < 12) return config >> 2;
    if (config < 16) return config < 14 ? 3 : 4;
    static const int map[4] = { 0, 2, 3, 4 };
    return map[(config - 16) >> 2];
}

int end_band(int bandwidth) noexcept {
    switch (bandwidth) {
        case 0: return 13;
        case 1: return 17;
        case 2: return 17;
        case 3: return 19;
        default: return 21;
    }
}

bool read_length(const std::uint8_t* data, std::size_t len, std::size_t& pos, int& out) noexcept {
    if (pos >= len) return false;
    const int b0 = data[pos];
    if (b0 < 252) {
        out = b0;
        pos += 1;
        return true;
    }
    if (pos + 1 >= len) return false;
    out = data[pos + 1] * 4 + b0;
    pos += 2;
    return true;
}

bool parse_packet(const std::uint8_t* data, std::size_t len, bool self_delimited, Packet& p) noexcept {
    if (!data || len < 1) return false;
    p.toc = data[0];
    const int code = p.toc & 3;
    const int dur = frame_samples(p.toc);
    std::size_t pos = 1;
    if (code == 0) {
        p.count = 1;
        int size;
        if (self_delimited) {
            if (!read_length(data, len, pos, size)) return false;
            if (static_cast<std::size_t>(size) > len - pos) return false;
        } else {
            size = static_cast<int>(len - pos);
        }
        if (size > 1275) return false;
        p.frames[0] = data + pos;
        p.sizes[0] = size;
        p.consumed = pos + static_cast<std::size_t>(size);
        return true;
    }
    if (code == 1) {
        p.count = 2;
        int size;
        if (self_delimited) {
            if (!read_length(data, len, pos, size)) return false;
            if (2 * static_cast<std::size_t>(size) > len - pos) return false;
        } else {
            if ((len - pos) & 1) return false;
            size = static_cast<int>((len - pos) / 2);
        }
        if (size > 1275) return false;
        p.frames[0] = data + pos;
        p.frames[1] = data + pos + size;
        p.sizes[0] = size;
        p.sizes[1] = size;
        p.consumed = pos + 2 * static_cast<std::size_t>(size);
        return true;
    }
    if (code == 2) {
        p.count = 2;
        int n1;
        if (!read_length(data, len, pos, n1)) return false;
        int n2;
        if (self_delimited) {
            if (!read_length(data, len, pos, n2)) return false;
            if (static_cast<std::size_t>(n1) + static_cast<std::size_t>(n2) > len - pos) return false;
        } else {
            if (static_cast<std::size_t>(n1) > len - pos) return false;
            n2 = static_cast<int>(len - pos - static_cast<std::size_t>(n1));
        }
        if (n1 > 1275 || n2 > 1275) return false;
        p.frames[0] = data + pos;
        p.frames[1] = data + pos + n1;
        p.sizes[0] = n1;
        p.sizes[1] = n2;
        p.consumed = pos + static_cast<std::size_t>(n1) + static_cast<std::size_t>(n2);
        return true;
    }
    if (len < 2) return false;
    const int ch = data[1];
    pos = 2;
    const bool vbr = (ch & 0x80) != 0;
    const bool padded = (ch & 0x40) != 0;
    const int count = ch & 0x3F;
    if (count < 1 || count * dur > 5760) return false;
    p.count = count;
    std::size_t avail = len - pos;
    std::size_t padding = 0;
    if (padded) {
        while (true) {
            if (avail == 0) return false;
            const int b = data[pos++];
            --avail;
            const std::size_t add = b == 255 ? 254 : static_cast<std::size_t>(b);
            if (add > avail) return false;
            avail -= add;
            padding += add;
            if (b != 255) break;
        }
    }
    if (vbr) {
        std::size_t total = 0;
        const int coded = self_delimited ? count : count - 1;
        for (int i = 0; i < coded; ++i) {
            const std::size_t before = pos;
            int size;
            if (!read_length(data, pos + avail, pos, size)) return false;
            avail -= pos - before;
            if (size > 1275) return false;
            p.sizes[i] = size;
            total += static_cast<std::size_t>(size);
        }
        if (total > avail) return false;
        if (!self_delimited) {
            const std::size_t last = avail - total;
            if (last > 1275) return false;
            p.sizes[count - 1] = static_cast<int>(last);
            total += last;
        }
        std::size_t at = pos;
        for (int i = 0; i < count; ++i) {
            p.frames[i] = data + at;
            at += static_cast<std::size_t>(p.sizes[i]);
        }
        p.consumed = at + padding;
        return true;
    }
    int size;
    if (self_delimited) {
        const std::size_t before = pos;
        if (!read_length(data, pos + avail, pos, size)) return false;
        avail -= pos - before;
        if (static_cast<std::size_t>(size) * static_cast<std::size_t>(count) > avail) return false;
    } else {
        if (avail % static_cast<std::size_t>(count) != 0) return false;
        size = static_cast<int>(avail / static_cast<std::size_t>(count));
    }
    if (size > 1275) return false;
    for (int i = 0; i < count; ++i) {
        p.frames[i] = data + pos + static_cast<std::size_t>(i) * static_cast<std::size_t>(size);
        p.sizes[i] = size;
    }
    p.consumed = pos + static_cast<std::size_t>(size) * static_cast<std::size_t>(count) + padding;
    return true;
}

bool parse_multistream(const std::uint8_t* data, std::size_t len, std::uint32_t streams, std::vector<Packet>& out, int& duration) {
    out.resize(streams);
    std::size_t pos = 0;
    duration = -1;
    for (std::uint32_t s = 0; s < streams; ++s) {
        const bool last = s + 1 == streams;
        if (pos >= len) return false;
        if (!parse_packet(data + pos, len - pos, !last, out[s])) return false;
        const int d = out[s].count * frame_samples(out[s].toc);
        if (duration >= 0 && d != duration) return false;
        duration = d;
        pos += out[s].consumed;
    }
    return true;
}

float window_at(int n48) noexcept {
    const double pi = 3.14159265358979323846;
    const double s = std::sin(0.5 * pi * (n48 + 0.5) / 120.0);
    return static_cast<float>(std::sin(0.5 * pi * s * s));
}

class Concealer {
public:
    void setup(int channels, int rate) {
        m_channels = channels;
        m_rate = rate;
        m_history_len = rate / 25;
        reset();
    }

    void reset() {
        for (int c = 0; c < 2; ++c) m_history[c].assign(static_cast<std::size_t>(m_history_len), 0.0f);
        m_have = false;
        m_active = false;
        m_background = -1.0f;
        m_pending.clear();
        m_seed = 22222;
    }

    void observe(const float* pcm, int frames) {
        if (frames <= 0) return;
        double e = 0.0;
        for (int i = 0; i < frames * m_channels; ++i) e += static_cast<double>(pcm[i]) * pcm[i];
        const float rms = static_cast<float>(std::sqrt(e / (frames * m_channels)));
        const float ms = 1000.0f * static_cast<float>(frames) / static_cast<float>(m_rate);
        if (m_background < 0.0f) {
            m_background = rms;
        } else {
            m_background = std::min(rms, m_background * (1.0f + 0.0025f * ms));
        }
        for (int c = 0; c < m_channels; ++c) {
            std::vector<float>& h = m_history[c];
            if (frames >= m_history_len) {
                for (int i = 0; i < m_history_len; ++i) h[static_cast<std::size_t>(i)] = pcm[static_cast<std::size_t>((frames - m_history_len + i) * m_channels + c)];
            } else {
                std::copy(h.begin() + frames, h.end(), h.begin());
                for (int i = 0; i < frames; ++i) h[static_cast<std::size_t>(m_history_len - frames + i)] = pcm[static_cast<std::size_t>(i * m_channels + c)];
            }
        }
        m_have = true;
    }

    void blend_into(float* pcm, int frames) {
        if (m_pending.empty()) return;
        const int n = std::min(frames, static_cast<int>(m_pending.size()) / m_channels);
        for (int i = 0; i < n; ++i) {
            const float w = window_at(i * 120 / n);
            const float ww = w * w;
            for (int c = 0; c < m_channels; ++c) {
                float& v = pcm[i * m_channels + c];
                v = ww * v + (1.0f - ww) * m_pending[static_cast<std::size_t>(i * m_channels + c)];
            }
        }
        m_pending.clear();
    }

    void good_frame(float* pcm, int frames) {
        blend_into(pcm, frames);
        m_active = false;
        observe(pcm, frames);
    }

    void conceal(float* pcm, int frames) {
        const int fade = m_rate / 400;
        std::vector<float> buf(static_cast<std::size_t>((frames + fade) * m_channels));
        generate(buf.data(), frames + fade);
        std::memcpy(pcm, buf.data(), sizeof(float) * static_cast<std::size_t>(frames * m_channels));
        if (!m_pending.empty()) {
            const int n = std::min(frames, static_cast<int>(m_pending.size()) / m_channels);
            m_pending.clear();
            (void)n;
        }
        m_pending.assign(buf.begin() + static_cast<std::ptrdiff_t>(frames * m_channels), buf.end());
        observe_concealed(pcm, frames);
    }

private:
    void observe_concealed(const float* pcm, int frames) {
        const float bg = m_background;
        observe(pcm, frames);
        m_background = bg;
    }

    float noise() {
        m_seed = 1103515245u * m_seed + 12345u;
        return static_cast<float>(static_cast<std::int32_t>(m_seed)) / 2147483648.0f;
    }

    void analyze() {
        const int n = m_history_len;
        std::vector<double> mixed(static_cast<std::size_t>(n), 0.0);
        for (int c = 0; c < m_channels; ++c) {
            for (int i = 0; i < n; ++i) mixed[static_cast<std::size_t>(i)] += m_history[c][static_cast<std::size_t>(i)];
        }
        const int min_lag = std::max(2, m_rate / 400);
        const int max_lag = std::min(n / 2, m_rate / 50);
        const int win = std::min(n - max_lag, m_rate / 100);
        double best = 0.0;
        int best_lag = max_lag;
        for (int lag = min_lag; lag <= max_lag; ++lag) {
            double xy = 0.0;
            double xx = 0.0;
            double yy = 0.0;
            for (int i = n - win; i < n; ++i) {
                const double a = mixed[static_cast<std::size_t>(i)];
                const double b = mixed[static_cast<std::size_t>(i - lag)];
                xy += a * b;
                xx += a * a;
                yy += b * b;
            }
            const double r = (xx > 1e-12 && yy > 1e-12) ? xy / std::sqrt(xx * yy) : 0.0;
            if (r > best) {
                best = r;
                best_lag = lag;
            }
        }
        m_period = best_lag;
        m_voiced = best > 0.6;
        double energy = 0.0;
        for (int c = 0; c < m_channels; ++c) {
            const std::vector<float>& h = m_history[c];
            const int span = std::min(n, m_rate / 50);
            double r[17] = {};
            for (int k = 0; k <= 16; ++k) {
                for (int i = n - span + k; i < n; ++i) r[k] += static_cast<double>(h[static_cast<std::size_t>(i)]) * h[static_cast<std::size_t>(i - k)];
                r[k] *= std::exp(-0.5 * std::pow(0.01 * k, 2.0));
            }
            energy += r[0] / span;
            r[0] = r[0] * 1.0001 + 1e-9;
            double a[17] = {};
            double err = r[0];
            for (int i = 1; i <= 16; ++i) {
                double acc = r[i];
                for (int j = 1; j < i; ++j) acc -= a[j] * r[i - j];
                const double k = err > 0.0 ? acc / err : 0.0;
                double t[17];
                for (int j = 1; j < i; ++j) t[j] = a[j] - k * a[i - j];
                for (int j = 1; j < i; ++j) a[j] = t[j];
                a[i] = k;
                err *= 1.0 - k * k;
                if (err <= 0.0) break;
            }
            for (int k = 0; k < 16; ++k) {
                m_lpc[c][k] = static_cast<float>(a[k + 1] * std::pow(0.97, k + 1));
                m_mem[c][k] = h[static_cast<std::size_t>(n - 1 - k)];
            }
            m_sigma[c] = static_cast<float>(std::sqrt(std::max(0.0, err) / span));
            m_cycle[c].assign(h.end() - m_period, h.end());
        }
        m_level = static_cast<float>(std::sqrt(energy / m_channels));
        m_phase = 0;
        m_elapsed = 0;
    }

    void generate(float* out, int frames) {
        if (!m_have) {
            std::fill(out, out + frames * m_channels, 0.0f);
            return;
        }
        if (!m_active) {
            analyze();
            m_active = true;
        }
        const float hold = 0.01f * m_rate;
        const float tau = 0.04f * m_rate;
        const float cn_ratio = m_level > 1e-9f ? std::min(1.0f, std::max(0.0f, m_background) / m_level) : 0.0f;
        for (int i = 0; i < frames; ++i) {
            const float t = static_cast<float>(m_elapsed);
            float decay = t < hold ? 1.0f : std::exp(-(t - hold) / tau);
            float tail = 1.0f;
            const float seconds = t / static_cast<float>(m_rate);
            if (seconds > 2.0f) tail = std::max(0.0f, 1.0f - (seconds - 2.0f));
            for (int c = 0; c < m_channels; ++c) {
                float excitation = noise() * m_sigma[c] * 1.7320508f;
                float v = excitation;
                for (int k = 0; k < 16; ++k) v += m_lpc[c][k] * m_mem[c][k];
                for (int k = 15; k > 0; --k) m_mem[c][k] = m_mem[c][k - 1];
                m_mem[c][0] = v;
                const float periodic = m_cycle[c].empty() ? 0.0f : m_cycle[c][static_cast<std::size_t>(m_phase % static_cast<int>(m_cycle[c].size()))];
                float y;
                if (m_voiced) {
                    y = decay * periodic + (1.0f - decay) * cn_ratio * v * tail;
                } else {
                    y = (decay + (1.0f - decay) * cn_ratio * tail) * v;
                }
                if (!std::isfinite(y)) y = 0.0f;
                out[i * m_channels + c] = std::max(-1.0f, std::min(1.0f, y));
            }
            ++m_phase;
            ++m_elapsed;
        }
    }

    int m_channels = 1;
    int m_rate = 48000;
    int m_history_len = 1920;
    std::vector<float> m_history[2];
    bool m_have = false;
    bool m_active = false;
    float m_background = -1.0f;
    std::vector<float> m_pending;
    std::uint32_t m_seed = 22222;
    int m_period = 480;
    bool m_voiced = false;
    float m_lpc[2][16] = {};
    float m_mem[2][16] = {};
    float m_sigma[2] = {};
    float m_level = 0.0f;
    std::vector<float> m_cycle[2];
    int m_phase = 0;
    long long m_elapsed = 0;
};

class StreamDecoder {
public:
    void init(int channels, int rate) {
        m_channels = channels;
        m_rate = rate;
        m_down = 48000 / rate;
        m_celt.init(channels, m_down);
        m_plc.setup(channels, rate);
        reset();
    }

    void reset() {
        m_silk.reset();
        m_celt.reset();
        for (int c = 0; c < 2; ++c) m_resampler[c] = Resampler();
        m_silk_rate = 0;
        m_prev_mode = -1;
        m_prev_redundancy = false;
        m_final_range = 0;
        m_plc.reset();
    }

    int channels() const noexcept { return m_channels; }
    std::uint32_t final_range() const noexcept { return m_final_range; }

    int decode(const Packet& p, float* out) {
        const int dur = frame_samples(p.toc) / m_down;
        for (int i = 0; i < p.count; ++i) {
            float* dst = out + static_cast<std::size_t>(i * dur * m_channels);
            if (p.sizes[i] == 0) {
                conceal_into(dst, dur);
            } else {
                decode_frame(p.frames[i], p.sizes[i], p.toc, dst);
                m_plc.good_frame(dst, dur);
            }
        }
        return p.count * dur;
    }

    void conceal_into(float* out, int frames) {
        m_plc.conceal(out, frames);
    }

    int decode_fec(const Packet& p, int frames, float* out) {
        const int mode = mode_of(p.toc);
        const int dur48 = frame_samples(p.toc);
        const int dur = dur48 / m_down;
        if (mode == k_mode_celt || p.count < 1 || p.sizes[0] == 0 || frames < dur) {
            conceal_into(out, frames);
            return frames;
        }
        if (frames > dur) conceal_into(out, frames - dur);
        float* dst = out + static_cast<std::size_t>((frames - dur) * m_channels);
        if (!decode_lbrr(p.frames[0], p.sizes[0], p.toc, dst)) {
            conceal_into(dst, dur);
        } else {
            m_plc.good_frame(dst, dur);
        }
        return frames;
    }

private:
    void ensure_resampler(int internal) {
        if (m_silk_rate == internal && m_silk.internal_rate() == internal) return;
        double delay = 0.706e-3;
        if (internal == 8000) delay = 0.538e-3;
        if (internal == 12000) delay = 0.692e-3;
        for (int c = 0; c < m_channels; ++c) m_resampler[c].setup(internal, m_rate, delay);
        m_silk_rate = internal;
    }

    int internal_rate_for(int toc) const noexcept {
        if (mode_of(toc) == k_mode_hybrid) return 16000;
        const int bw = bandwidth_of(toc);
        return bw == 0 ? 8000 : (bw == 1 ? 12000 : 16000);
    }

    void silk_to_output(int internal, std::vector<float> buf[2], float* out, int frames) {
        if (m_silk_rate != internal || m_silk.internal_rate() != internal) {
            ensure_resampler(internal);
        }
        std::vector<float> tmp(static_cast<std::size_t>(frames) + 16);
        for (int c = 0; c < m_channels; ++c) {
            const std::size_t n = m_resampler[c].process(buf[c].data(), buf[c].size(), tmp.data());
            for (int i = 0; i < frames; ++i) out[i * m_channels + c] = i < static_cast<int>(n) ? tmp[static_cast<std::size_t>(i)] : 0.0f;
        }
    }

    bool decode_lbrr(const std::uint8_t* data, int len, int toc, float* out) {
        const int mode = mode_of(toc);
        const int dur48 = frame_samples(toc);
        const int frames = dur48 / m_down;
        const bool stereo = (toc & 4) != 0;
        if (m_prev_mode == k_mode_celt) {
            m_silk.reset();
            m_silk_rate = 0;
        }
        const int internal = internal_rate_for(toc);
        if (m_silk.internal_rate() != internal) m_silk_rate = 0;
        RangeDecoder rd;
        rd.init(data, static_cast<std::uint32_t>(len));
        std::vector<float> buf[2];
        bool complete = false;
        m_silk.decode(rd, internal, std::max(10, dur48 / 48), stereo, m_channels, true, buf, &complete);
        if (!complete) return false;
        silk_to_output(internal, buf, out, frames);
        (void)mode;
        return true;
    }

    void decode_frame(const std::uint8_t* data, int len, int toc, float* out) {
        const int mode = mode_of(toc);
        const int bw = bandwidth_of(toc);
        const int dur48 = frame_samples(toc);
        const int frames = dur48 / m_down;
        const bool stereo = (toc & 4) != 0;
        const int C = stereo ? 2 : 1;
        const int f25 = 120 / m_down;
        std::fill(out, out + static_cast<std::size_t>(frames * m_channels), 0.0f);
        RangeDecoder rd;
        rd.init(data, static_cast<std::uint32_t>(len));
        int main_len = len;
        bool redundancy = false;
        bool celt_to_silk = false;
        int red_bytes = 0;
        if (mode != k_mode_celt) {
            if (m_prev_mode == k_mode_celt) {
                m_silk.reset();
                m_silk_rate = 0;
            }
            const int internal = internal_rate_for(toc);
            if (m_silk.internal_rate() != internal) m_silk_rate = 0;
            std::vector<float> buf[2];
            m_silk.decode(rd, internal, std::max(10, dur48 / 48), stereo, m_channels, false, buf, nullptr);
            silk_to_output(internal, buf, out, frames);
            const int tell = rd.tell();
            if (tell + 17 + (mode == k_mode_hybrid ? 20 : 0) <= 8 * len) {
                redundancy = mode == k_mode_hybrid ? rd.bit_logp(12) : true;
            }
            if (redundancy) {
                celt_to_silk = rd.bit_logp(1);
                red_bytes = mode == k_mode_hybrid ? static_cast<int>(rd.uint(256)) + 2 : len - ((rd.tell() + 7) >> 3);
                main_len = len - red_bytes;
                if (red_bytes > len || main_len < 0 || 8 * main_len < rd.tell()) {
                    redundancy = false;
                    red_bytes = 0;
                    main_len = 0;
                    rd.shrink(static_cast<std::uint32_t>(len));
                } else {
                    rd.shrink(static_cast<std::uint32_t>(red_bytes));
                }
            }
        }
        const int start = mode == k_mode_hybrid ? 17 : 0;
        const int end = end_band(bw);
        std::vector<float> red(static_cast<std::size_t>(240 / m_down * m_channels), 0.0f);
        std::uint32_t red_range = 0;
        if (redundancy && celt_to_silk) {
            RangeDecoder rr;
            rr.init(data + main_len, static_cast<std::uint32_t>(red_bytes));
            m_celt.decode(rr, 240, 0, end, C, red.data(), false);
            red_range = rr.range();
        }
        if (mode != k_mode_silk) {
            if (m_prev_mode >= 0 && mode != m_prev_mode && !m_prev_redundancy) m_celt.reset();
            m_celt.decode(rd, dur48, start, end, C, out, mode == k_mode_hybrid);
        } else if (m_prev_mode == k_mode_hybrid && !(redundancy && celt_to_silk && m_prev_redundancy)) {
            static const std::uint8_t silence[2] = { 0xFF, 0xFF };
            RangeDecoder rf;
            rf.init(silence, 2);
            std::vector<float> fade(static_cast<std::size_t>(f25 * m_channels), 0.0f);
            m_celt.decode(rf, 120, 0, 21, C, fade.data(), false);
            for (int i = 0; i < f25 * m_channels && i < frames * m_channels; ++i) out[i] += fade[static_cast<std::size_t>(i)];
        }
        if (redundancy && !celt_to_silk) {
            m_celt.reset();
            RangeDecoder rr;
            rr.init(data + main_len, static_cast<std::uint32_t>(red_bytes));
            m_celt.decode(rr, 240, 0, end, C, red.data(), false);
            red_range = rr.range();
        }
        if (redundancy && celt_to_silk && (m_prev_mode != k_mode_silk || m_prev_redundancy)) {
            for (int i = 0; i < f25 * m_channels; ++i) out[i] = red[static_cast<std::size_t>(i)];
            for (int i = 0; i < f25; ++i) {
                const float w = window_at(i * m_down);
                const float ww = w * w;
                for (int c = 0; c < m_channels; ++c) {
                    const std::size_t at = static_cast<std::size_t>((f25 + i) * m_channels + c);
                    out[at] = ww * out[at] + (1.0f - ww) * red[at];
                }
            }
        }
        if (redundancy && !celt_to_silk) {
            for (int i = 0; i < f25; ++i) {
                const float w = window_at(i * m_down);
                const float ww = w * w;
                for (int c = 0; c < m_channels; ++c) {
                    const std::size_t at = static_cast<std::size_t>((frames - f25 + i) * m_channels + c);
                    out[at] = ww * red[static_cast<std::size_t>((f25 + i) * m_channels + c)] + (1.0f - ww) * out[at];
                }
            }
        }
        m_final_range = main_len <= 1 ? 0 : (rd.range() ^ red_range);
        m_prev_mode = mode;
        m_prev_redundancy = redundancy && !celt_to_silk;
        for (int i = 0; i < frames * m_channels; ++i) {
            if (!std::isfinite(out[i])) out[i] = 0.0f;
        }
    }

    int m_channels = 1;
    int m_rate = 48000;
    int m_down = 1;
    SilkDecoder m_silk;
    CeltDecoder m_celt;
    Resampler m_resampler[2];
    int m_silk_rate = 0;
    int m_prev_mode = -1;
    bool m_prev_redundancy = false;
    std::uint32_t m_final_range = 0;
    Concealer m_plc;
};

void wave_order(std::uint32_t channels, std::vector<int>& order) {
    order.resize(channels);
    for (std::uint32_t i = 0; i < channels; ++i) order[i] = static_cast<int>(i);
    static const int o3[3] = { 0, 2, 1 };
    static const int o5[5] = { 0, 2, 1, 3, 4 };
    static const int o6[6] = { 0, 2, 1, 5, 3, 4 };
    static const int o7[7] = { 0, 2, 1, 6, 5, 3, 4 };
    static const int o8[8] = { 0, 2, 1, 7, 5, 6, 3, 4 };
    const int* src = nullptr;
    if (channels == 3) src = o3;
    if (channels == 5) src = o5;
    if (channels == 6) src = o6;
    if (channels == 7) src = o7;
    if (channels == 8) src = o8;
    if (src) {
        for (std::uint32_t i = 0; i < channels; ++i) order[i] = src[i];
    }
}

bool set_error(std::string* error, const char* message) {
    if (error) *error = message;
    return false;
}

} // namespace

bool parse_opus_head(const void* data, std::size_t size, OpusHead& head, std::string* error) {
    const std::uint8_t* d = static_cast<const std::uint8_t*>(data);
    if (!d || size < 19 || std::memcmp(d, "OpusHead", 8) != 0) return set_error(error, "not an OpusHead packet");
    OpusHead h;
    h.version = d[8];
    if ((h.version >> 4) != 0) return set_error(error, "unsupported OpusHead major version");
    h.channels = d[9];
    if (h.channels == 0) return set_error(error, "the OpusHead has no channels");
    h.pre_skip = static_cast<std::uint16_t>(d[10] | (d[11] << 8));
    h.input_sample_rate = static_cast<std::uint32_t>(d[12]) | (static_cast<std::uint32_t>(d[13]) << 8) | (static_cast<std::uint32_t>(d[14]) << 16) | (static_cast<std::uint32_t>(d[15]) << 24);
    h.output_gain = static_cast<std::int16_t>(static_cast<std::uint16_t>(d[16] | (d[17] << 8)));
    h.mapping_family = d[18];
    if (h.mapping_family == 0) {
        if (h.channels > 2) return set_error(error, "channel mapping family 0 allows only 1 or 2 channels");
        h.streams = 1;
        h.coupled_streams = static_cast<std::uint8_t>(h.channels - 1);
        h.mapping.clear();
        for (std::uint8_t c = 0; c < h.channels; ++c) h.mapping.push_back(c);
    } else if (h.mapping_family == 1 || h.mapping_family == 255) {
        if (h.mapping_family == 1 && h.channels > 8) return set_error(error, "channel mapping family 1 allows at most 8 channels");
        if (size < 21u + h.channels) return set_error(error, "the OpusHead channel mapping table is truncated");
        h.streams = d[19];
        h.coupled_streams = d[20];
        if (h.streams == 0) return set_error(error, "the OpusHead has no streams");
        if (h.coupled_streams > h.streams) return set_error(error, "more coupled streams than streams");
        if (static_cast<int>(h.streams) + h.coupled_streams > 255) return set_error(error, "too many decoded channels");
        h.mapping.assign(d + 21, d + 21 + h.channels);
        for (std::uint8_t m : h.mapping) {
            if (m != 255 && m >= h.streams + h.coupled_streams) return set_error(error, "a channel mapping refers to a missing stream");
        }
    } else {
        return set_error(error, "unsupported channel mapping family");
    }
    head = h;
    return true;
}

int opus_packet_frames(const void* packet, std::size_t size) noexcept {
    Packet p;
    if (!parse_packet(static_cast<const std::uint8_t*>(packet), size, false, p)) return -1;
    return p.count * frame_samples(p.toc);
}

int opus_packet_frames(const void* packet, std::size_t size, std::uint32_t streams) noexcept {
    if (streams == 0) return -1;
    try {
        std::vector<Packet> ps;
        int d = -1;
        if (!parse_multistream(static_cast<const std::uint8_t*>(packet), size, streams, ps, d)) return -1;
        return d;
    } catch (...) {
        return -1;
    }
}

struct OpusDecoder::Impl {
    OpusHead head;
    std::uint32_t channels = 0;
    std::uint32_t rate = 48000;
    std::vector<StreamDecoder> streams;
    std::vector<int> order;
    std::int16_t gain_q8 = 0;
    float gain = 1.0f;
    std::uint32_t range = 0;
    std::vector<Packet> packets;
    std::vector<std::vector<float>> pcm;

    void set_gain(std::int16_t g) {
        gain_q8 = g;
        gain = static_cast<float>(std::pow(10.0, g / (20.0 * 256.0)));
    }

    void build() {
        streams.clear();
        streams.resize(head.streams);
        for (std::uint32_t s = 0; s < head.streams; ++s) streams[s].init(s < head.coupled_streams ? 2 : 1, static_cast<int>(rate));
        pcm.assign(head.streams, std::vector<float>());
        if (head.mapping_family == 1) {
            wave_order(channels, order);
        } else {
            order.resize(channels);
            for (std::uint32_t i = 0; i < channels; ++i) order[i] = static_cast<int>(i);
        }
    }

    void map_output(std::size_t frames, std::vector<float>& out) {
        out.assign(frames * channels, 0.0f);
        for (std::uint32_t c = 0; c < channels; ++c) {
            const int src_channel = order[c];
            const int idx = head.mapping[static_cast<std::size_t>(src_channel)];
            if (idx == 255) continue;
            std::uint32_t stream;
            int sub;
            if (idx < 2 * head.coupled_streams) {
                stream = static_cast<std::uint32_t>(idx / 2);
                sub = idx & 1;
            } else {
                stream = static_cast<std::uint32_t>(idx - head.coupled_streams);
                sub = 0;
            }
            if (stream >= streams.size()) continue;
            const int sc = streams[stream].channels();
            const std::vector<float>& src = pcm[stream];
            for (std::size_t i = 0; i < frames; ++i) {
                const std::size_t at = i * static_cast<std::size_t>(sc) + static_cast<std::size_t>(sub);
                out[i * channels + c] = at < src.size() ? src[at] * gain : 0.0f;
            }
        }
    }

    void update_range() {
        range = 0;
        for (const StreamDecoder& s : streams) range ^= s.final_range();
    }
};

OpusDecoder::OpusDecoder() = default;
OpusDecoder::~OpusDecoder() = default;
OpusDecoder::OpusDecoder(OpusDecoder&&) noexcept = default;
OpusDecoder& OpusDecoder::operator=(OpusDecoder&&) noexcept = default;

bool OpusDecoder::open(std::uint32_t channels, std::string* error) {
    if (channels < 1 || channels > 2) return set_error(error, "an Opus decoder without a header takes 1 or 2 channels");
    OpusHead h;
    h.version = 1;
    h.channels = static_cast<std::uint8_t>(channels);
    h.mapping_family = 0;
    h.streams = 1;
    h.coupled_streams = static_cast<std::uint8_t>(channels - 1);
    h.input_sample_rate = 48000;
    for (std::uint32_t c = 0; c < channels; ++c) h.mapping.push_back(static_cast<std::uint8_t>(c));
    return open_head(h, error);
}

bool OpusDecoder::open_head(const std::uint8_t* opus_head, std::size_t size, std::string* error) {
    OpusHead h;
    if (!parse_opus_head(opus_head, size, h, error)) return false;
    return open_head(h, error);
}

bool OpusDecoder::open_head(const OpusHead& head, std::string* error) {
    if (head.channels == 0 || head.streams == 0 || head.coupled_streams > head.streams || head.mapping.size() < head.channels) return set_error(error, "invalid Opus channel layout");
    for (std::size_t c = 0; c < head.channels; ++c) {
        const std::uint8_t m = head.mapping[c];
        if (m != 255 && m >= head.streams + head.coupled_streams) return set_error(error, "a channel mapping refers to a missing stream");
    }
    if (head.mapping_family == 0 && (head.streams != 1 || head.channels > 2)) return set_error(error, "invalid family 0 layout");
    const std::uint32_t rate = m_impl ? m_impl->rate : 48000;
    try {
        std::unique_ptr<Impl> impl(new Impl());
        impl->head = head;
        impl->channels = head.channels;
        impl->rate = rate;
        impl->set_gain(head.output_gain);
        impl->build();
        m_impl = std::move(impl);
    } catch (const std::bad_alloc&) {
        return set_error(error, "out of memory");
    }
    return true;
}

bool OpusDecoder::open_multistream(std::uint32_t channels, std::uint32_t streams, std::uint32_t coupled_streams, const std::uint8_t* mapping, std::string* error) {
    if (channels < 1 || channels > 255 || streams < 1 || coupled_streams > streams || streams + coupled_streams > 255 || !mapping) return set_error(error, "invalid multistream layout");
    OpusHead h;
    h.version = 1;
    h.channels = static_cast<std::uint8_t>(channels);
    h.mapping_family = 255;
    h.streams = static_cast<std::uint8_t>(streams);
    h.coupled_streams = static_cast<std::uint8_t>(coupled_streams);
    h.input_sample_rate = 48000;
    h.mapping.assign(mapping, mapping + channels);
    return open_head(h, error);
}

void OpusDecoder::close() {
    m_impl.reset();
}

bool OpusDecoder::is_open() const noexcept {
    return m_impl != nullptr;
}

std::size_t OpusDecoder::decode(const std::uint8_t* packet, std::size_t size, std::vector<float>& out_interleaved, std::string* error) {
    out_interleaved.clear();
    if (!m_impl) {
        set_error(error, "the Opus decoder isn't open");
        return 0;
    }
    if (!packet || size == 0) {
        set_error(error, "empty Opus packet");
        return 0;
    }
    Impl& m = *m_impl;
    try {
        int duration = -1;
        if (!parse_multistream(packet, size, m.head.streams, m.packets, duration)) {
            set_error(error, "invalid Opus packet");
            return 0;
        }
        const std::size_t frames = static_cast<std::size_t>(duration) * m.rate / 48000;
        for (std::size_t s = 0; s < m.streams.size(); ++s) {
            m.pcm[s].assign(frames * static_cast<std::size_t>(m.streams[s].channels()), 0.0f);
            m.streams[s].decode(m.packets[s], m.pcm[s].data());
        }
        m.update_range();
        m.map_output(frames, out_interleaved);
        if (error) error->clear();
        return frames;
    } catch (const std::bad_alloc&) {
        out_interleaved.clear();
        set_error(error, "out of memory");
        return 0;
    }
}

std::size_t OpusDecoder::decode_fec(const std::uint8_t* packet, std::size_t size, std::size_t frames, std::vector<float>& out_interleaved) {
    out_interleaved.clear();
    if (!m_impl || frames == 0) return 0;
    Impl& m = *m_impl;
    const std::size_t step = m.rate / 400;
    frames = (frames + step - 1) / step * step;
    if (frames > max_packet_frames * m.rate / 48000) frames = max_packet_frames * m.rate / 48000;
    try {
        int duration = -1;
        const bool ok = packet && size > 0 && parse_multistream(packet, size, m.head.streams, m.packets, duration);
        for (std::size_t s = 0; s < m.streams.size(); ++s) {
            m.pcm[s].assign(frames * static_cast<std::size_t>(m.streams[s].channels()), 0.0f);
            if (ok) {
                m.streams[s].decode_fec(m.packets[s], static_cast<int>(frames), m.pcm[s].data());
            } else {
                m.streams[s].conceal_into(m.pcm[s].data(), static_cast<int>(frames));
            }
        }
        m.map_output(frames, out_interleaved);
        return frames;
    } catch (const std::bad_alloc&) {
        out_interleaved.clear();
        return 0;
    }
}

std::size_t OpusDecoder::conceal(std::size_t frames, std::vector<float>& out) {
    out.clear();
    if (!m_impl || frames == 0) return 0;
    Impl& m = *m_impl;
    const std::size_t step = m.rate / 400;
    frames = (frames + step - 1) / step * step;
    if (frames > max_packet_frames * m.rate / 48000) frames = max_packet_frames * m.rate / 48000;
    try {
        for (std::size_t s = 0; s < m.streams.size(); ++s) {
            m.pcm[s].assign(frames * static_cast<std::size_t>(m.streams[s].channels()), 0.0f);
            m.streams[s].conceal_into(m.pcm[s].data(), static_cast<int>(frames));
        }
        m.map_output(frames, out);
        return frames;
    } catch (const std::bad_alloc&) {
        out.clear();
        return 0;
    }
}

void OpusDecoder::reset() {
    if (!m_impl) return;
    for (StreamDecoder& s : m_impl->streams) s.reset();
    m_impl->range = 0;
}

std::uint32_t OpusDecoder::channels() const noexcept {
    return m_impl ? m_impl->channels : 0;
}

std::uint32_t OpusDecoder::streams() const noexcept {
    return m_impl ? m_impl->head.streams : 0;
}

std::uint32_t OpusDecoder::coupled_streams() const noexcept {
    return m_impl ? m_impl->head.coupled_streams : 0;
}

std::uint16_t OpusDecoder::pre_skip() const noexcept {
    return m_impl ? m_impl->head.pre_skip : 0;
}

std::int16_t OpusDecoder::output_gain() const noexcept {
    return m_impl ? m_impl->gain_q8 : 0;
}

void OpusDecoder::set_output_gain(std::int16_t gain_q8) noexcept {
    if (m_impl) m_impl->set_gain(gain_q8);
}

const OpusHead& OpusDecoder::head() const noexcept {
    static const OpusHead empty;
    return m_impl ? m_impl->head : empty;
}

std::uint32_t OpusDecoder::final_range() const noexcept {
    return m_impl ? m_impl->range : 0;
}

bool OpusDecoder::set_output_rate(std::uint32_t rate, std::string* error) {
    if (rate != 8000 && rate != 12000 && rate != 16000 && rate != 24000 && rate != 48000) return set_error(error, "Opus decodes at 8, 12, 16, 24 or 48 kHz");
    if (!m_impl) return set_error(error, "the Opus decoder isn't open");
    try {
        m_impl->rate = rate;
        m_impl->build();
    } catch (const std::bad_alloc&) {
        return set_error(error, "out of memory");
    }
    return true;
}

std::uint32_t OpusDecoder::output_rate() const noexcept {
    return m_impl ? m_impl->rate : sample_rate;
}

namespace {

struct Link {
    std::uint64_t start = 0;
    std::uint64_t end = 0;
    std::uint32_t serial = 0;
    bool opus = false;
    OpusHead head;
    std::int64_t begin = 0;
    std::int64_t last_granule = -1;
    std::uint64_t first_frame = 0;
    std::uint64_t frames = 0;
};

bool is_opus_head(const std::vector<std::uint8_t>& d) {
    return d.size() >= 8 && std::memcmp(d.data(), "OpusHead", 8) == 0;
}

void parse_tags(const std::vector<std::uint8_t>& d, std::string& vendor, std::vector<std::pair<std::string, std::string>>& tags) {
    if (d.size() < 16 || std::memcmp(d.data(), "OpusTags", 8) != 0) return;
    std::size_t pos = 8;
    const std::uint32_t vlen = detail::read_le32(&d[pos]);
    pos += 4;
    if (vlen > d.size() - pos) return;
    vendor.assign(reinterpret_cast<const char*>(&d[pos]), vlen);
    pos += vlen;
    if (d.size() - pos < 4) return;
    std::uint32_t count = detail::read_le32(&d[pos]);
    pos += 4;
    while (count-- > 0) {
        if (d.size() - pos < 4) return;
        const std::uint32_t len = detail::read_le32(&d[pos]);
        pos += 4;
        if (len > d.size() - pos) return;
        const std::string entry(reinterpret_cast<const char*>(&d[pos]), len);
        pos += len;
        const std::size_t eq = entry.find('=');
        if (eq == std::string::npos) continue;
        std::string key = entry.substr(0, eq);
        for (char& ch : key) {
            if (ch >= 'a' && ch <= 'z') ch = static_cast<char>(ch - 'a' + 'A');
        }
        tags.emplace_back(key, entry.substr(eq + 1));
    }
}

int packet_duration(const std::vector<std::uint8_t>& data, std::uint32_t streams) {
    if (data.empty()) return -1;
    return opus_packet_frames(data.data(), data.size(), streams);
}

} // namespace

struct OpusReader::Impl {
    OggReader ogg;
    std::vector<Link> links;
    std::size_t link = 0;
    bool loaded = false;
    OpusDecoder dec;
    std::deque<OggPacket> queue;
    bool anchored = false;
    std::int64_t fallback_anchor = 0;
    std::int64_t pos = 0;
    std::int64_t next_out = 0;
    std::vector<float> pcm;
    std::vector<float> out;
    std::size_t out_frames = 0;
    std::size_t out_read = 0;
    bool done = false;
    std::uint32_t channels = 0;

    bool read_headers(Link& l, std::string* error, std::string* vendor, std::vector<std::pair<std::string, std::string>>* tags) {
        if (!ogg.seek(l.start)) return set_error(error, "can't seek in the Ogg file");
        ogg.set_limit(l.end);
        ogg.select_stream(l.serial);
        OggPacket p;
        if (!ogg.read_packet(p) || !is_opus_head(p.data)) return set_error(error, "the OpusHead packet is missing");
        if (!parse_opus_head(p.data.data(), p.data.size(), l.head, error)) return false;
        if (!ogg.read_packet(p)) return set_error(error, "the OpusTags packet is missing");
        if (vendor && tags) parse_tags(p.data, *vendor, *tags);
        return true;
    }

    bool start_link(std::size_t index) {
        loaded = false;
        link = index;
        Link& l = links[index];
        if (!read_headers(l, nullptr, nullptr, nullptr)) return false;
        std::string err;
        if (!dec.open_head(l.head, &err)) return false;
        queue.clear();
        anchored = true;
        pos = l.begin;
        next_out = l.begin + l.head.pre_skip;
        out_frames = 0;
        out_read = 0;
        done = false;
        loaded = true;
        return true;
    }

    bool load_group() {
        std::vector<OggPacket> group;
        OggPacket p;
        while (group.size() < 4096 && ogg.read_packet(p)) {
            const bool has = p.granule >= 0;
            group.push_back(std::move(p));
            if (has) break;
        }
        if (group.empty()) return false;
        if (!anchored) {
            const std::uint32_t streams = links[link].head.streams;
            std::int64_t total = 0;
            for (const OggPacket& g : group) {
                const int d = packet_duration(g.data, streams);
                if (d > 0) total += d;
            }
            pos = group.back().granule >= 0 ? group.back().granule - total : fallback_anchor;
            anchored = true;
        }
        for (OggPacket& g : group) queue.push_back(std::move(g));
        return true;
    }

    bool produce() {
        while (!done) {
            if (queue.empty()) {
                if (load_group()) continue;
                if (link + 1 < links.size() && start_link(link + 1)) continue;
                done = true;
                return false;
            }
            OggPacket p = std::move(queue.front());
            queue.pop_front();
            const Link& l = links[link];
            const int d = packet_duration(p.data, l.head.streams);
            if (d <= 0) continue;
            std::size_t n = dec.decode(p.data.data(), p.data.size(), pcm);
            if (n == 0) n = dec.conceal(static_cast<std::size_t>(d), pcm);
            const std::int64_t s = pos;
            pos += d;
            const std::int64_t lo = std::max(s, next_out);
            const std::int64_t hi = std::min(pos, l.last_granule);
            if (hi <= lo) continue;
            const std::size_t count = static_cast<std::size_t>(hi - lo);
            const std::size_t skip = static_cast<std::size_t>(lo - s);
            if (skip + count > n) continue;
            out.assign(pcm.begin() + static_cast<std::ptrdiff_t>(skip * channels), pcm.begin() + static_cast<std::ptrdiff_t>((skip + count) * channels));
            out_frames = count;
            out_read = 0;
            next_out = hi;
            return true;
        }
        return false;
    }
};

bool is_ogg_opus(const void* data, std::size_t size) noexcept {
    if (!is_ogg(data, size)) return false;
    try {
        OggReader r;
        if (!r.open(data, size)) return false;
        OggPage page;
        while (r.read_page(page)) {
            if (!page.bos()) break;
            if (is_opus_head(page.body)) return true;
        }
    } catch (...) {
        return false;
    }
    return false;
}

OpusReader::OpusReader() = default;
OpusReader::~OpusReader() = default;
OpusReader::OpusReader(OpusReader&&) noexcept = default;
OpusReader& OpusReader::operator=(OpusReader&&) noexcept = default;

bool OpusReader::open(const std::string& path, std::string* error) {
    close();
    m_impl = std::make_unique<Impl>();
    if (!m_impl->ogg.open(path, error)) {
        m_impl.reset();
        return false;
    }
    return finish_open(error);
}

bool OpusReader::open(const void* data, std::size_t size, std::string* error) {
    close();
    m_impl = std::make_unique<Impl>();
    if (!m_impl->ogg.open(data, size, error)) {
        m_impl.reset();
        return false;
    }
    return finish_open(error);
}

bool OpusReader::finish_open(std::string* error) {
    Impl& m = *m_impl;
    auto fail = [&](const std::string& message) {
        if (error) *error = message;
        m_impl.reset();
        m_info = OpusInfo();
        return false;
    };
    try {
        std::vector<Link> found;
        {
            OggPage page;
            bool in_bos = false;
            m.ogg.select_all_streams();
            while (m.ogg.read_page(page)) {
                if (page.bos()) {
                    if (found.empty() || !in_bos) {
                        found.emplace_back();
                        found.back().start = page.offset;
                    }
                    in_bos = true;
                    Link& l = found.back();
                    if (!l.opus && !page.continued() && is_opus_head(page.body)) {
                        l.opus = true;
                        l.serial = page.serial;
                    }
                } else {
                    if (found.empty()) continue;
                    in_bos = false;
                }
                Link& l = found.back();
                if (l.opus && page.serial == l.serial && page.granule >= 0) l.last_granule = page.granule;
            }
            for (std::size_t i = 0; i < found.size(); ++i) found[i].end = i + 1 < found.size() ? found[i + 1].start : m.ogg.size();
        }
        if (found.empty()) return fail("not an Ogg file");
        if (!found[0].opus) return fail("the Ogg file has no Opus stream");
        m_info = OpusInfo();
        std::uint64_t total = 0;
        for (std::size_t i = 0; i < found.size(); ++i) {
            Link& l = found[i];
            if (!l.opus) {
                m_info.chain_stopped = true;
                break;
            }
            std::string err;
            if (!m.read_headers(l, &err, i == 0 ? &m_info.vendor : nullptr, i == 0 ? &m_info.tags : nullptr)) {
                if (i == 0) return fail(err.empty() ? std::string("the Opus headers can't be read") : err);
                m_info.chain_stopped = true;
                break;
            }
            if (i == 0) {
                m_info.channels = l.head.channels;
                m_info.input_sample_rate = l.head.input_sample_rate;
                m_info.pre_skip = l.head.pre_skip;
                m_info.output_gain = l.head.output_gain;
                m_info.mapping_family = l.head.mapping_family;
            } else if (l.head.channels != m_info.channels) {
                m_info.chain_stopped = true;
                break;
            }
            std::vector<OggPacket> group;
            OggPacket p;
            while (group.size() < 4096 && m.ogg.read_packet(p)) {
                const bool has = p.granule >= 0;
                group.push_back(std::move(p));
                if (has) break;
            }
            std::int64_t begin = 0;
            if (!group.empty() && group.back().granule >= 0) {
                std::int64_t sum = 0;
                for (const OggPacket& g : group) {
                    const int d = packet_duration(g.data, l.head.streams);
                    if (d > 0) sum += d;
                }
                const std::int64_t g = group.back().granule;
                begin = g - sum;
                if (begin < 0) begin = 0;
            }
            l.begin = begin;
            l.first_frame = total;
            const std::int64_t first_out = begin + l.head.pre_skip;
            l.frames = l.last_granule > first_out ? static_cast<std::uint64_t>(l.last_granule - first_out) : 0;
            total += l.frames;
            m.links.push_back(l);
        }
        m_info.frames = total;
        m_info.links = static_cast<std::uint32_t>(m.links.size());
        m_info.sample_rate = 48000;
        m.channels = m_info.channels;
        if (!m.start_link(0)) return fail("the Opus stream can't be decoded");
    } catch (const std::bad_alloc&) {
        return fail("out of memory");
    }
    m_position = 0;
    return true;
}

void OpusReader::close() {
    m_impl.reset();
    m_info = OpusInfo();
    m_position = 0;
}

bool OpusReader::is_open() const noexcept {
    return m_impl != nullptr;
}

std::size_t OpusReader::read(float* interleaved, std::size_t frames) {
    if (!m_impl || !interleaved) return 0;
    Impl& m = *m_impl;
    const std::size_t ch = m.channels;
    std::size_t done = 0;
    try {
        while (done < frames) {
            if (m.out_read == m.out_frames) {
                if (!m.produce()) break;
            }
            const std::size_t n = std::min(frames - done, m.out_frames - m.out_read);
            std::memcpy(interleaved + done * ch, m.out.data() + m.out_read * ch, n * ch * sizeof(float));
            m.out_read += n;
            done += n;
        }
    } catch (const std::bad_alloc&) {
        m.done = true;
    }
    m_position += done;
    return done;
}

std::size_t OpusReader::read(AudioBuffer& out, std::size_t frames) {
    if (!m_impl) return 0;
    if (out.channels() != m_info.channels || out.sample_rate() != m_info.sample_rate) out = AudioBuffer(m_info.channels, m_info.sample_rate);
    const std::size_t start = out.frames();
    out.resize(start + frames);
    const std::size_t got = read(out.frame(start), frames);
    out.resize(start + got);
    return got;
}

bool OpusReader::seek(std::uint64_t frame) {
    if (!m_impl) return false;
    Impl& m = *m_impl;
    std::size_t index = m.links.size();
    for (std::size_t i = 0; i < m.links.size(); ++i) {
        if (frame >= m.links[i].first_frame && frame < m.links[i].first_frame + m.links[i].frames) {
            index = i;
            break;
        }
    }
    if (index == m.links.size()) {
        m.queue.clear();
        m.out_frames = 0;
        m.out_read = 0;
        m.done = true;
        m_position = frame;
        return true;
    }
    try {
        if (!m.start_link(index)) {
            m.done = true;
            return false;
        }
        const Link& l = m.links[index];
        const std::int64_t target = l.begin + l.head.pre_skip + static_cast<std::int64_t>(frame - l.first_frame);
        const std::int64_t preroll = target - 2 * static_cast<std::int64_t>(seek_preroll);
        if (preroll > l.begin) {
            OggPage page;
            if (m.ogg.find_granule(l.serial, preroll, l.start, l.end, page) && page.granule > l.begin && page.end() < l.end) {
                m.ogg.seek(page.end());
                m.ogg.set_limit(l.end);
                m.ogg.select_stream(l.serial);
                m.dec.reset();
                m.queue.clear();
                m.anchored = false;
                m.fallback_anchor = page.granule;
            }
        }
        m.next_out = target;
    } catch (const std::bad_alloc&) {
        m.done = true;
        return false;
    }
    m_position = frame;
    return true;
}

bool read_opus(const std::string& path, AudioBuffer& out, std::string* error, OpusInfo* info) {
    OpusReader r;
    if (!r.open(path, error)) return false;
    const std::size_t ch = r.info().channels;
    std::vector<float> data;
    const std::uint64_t expect = std::min<std::uint64_t>(r.info().frames, std::uint64_t(1) << 24);
    data.reserve(static_cast<std::size_t>(expect) * ch);
    std::vector<float> chunk(4096 * ch);
    while (true) {
        const std::size_t got = r.read(chunk.data(), 4096);
        if (got == 0) break;
        data.insert(data.end(), chunk.begin(), chunk.begin() + static_cast<std::ptrdiff_t>(got * ch));
    }
    if (info) {
        *info = r.info();
        info->frames = data.size() / ch;
    }
    out = AudioBuffer(ch, r.info().sample_rate, std::move(data));
    return true;
}

bool read_opus(const void* data, std::size_t size, AudioBuffer& out, std::string* error, OpusInfo* info) {
    OpusReader r;
    if (!r.open(data, size, error)) return false;
    const std::size_t ch = r.info().channels;
    std::vector<float> samples;
    const std::uint64_t expect = std::min<std::uint64_t>(r.info().frames, std::uint64_t(1) << 24);
    samples.reserve(static_cast<std::size_t>(expect) * ch);
    std::vector<float> chunk(4096 * ch);
    while (true) {
        const std::size_t got = r.read(chunk.data(), 4096);
        if (got == 0) break;
        samples.insert(samples.end(), chunk.begin(), chunk.begin() + static_cast<std::ptrdiff_t>(got * ch));
    }
    if (info) {
        *info = r.info();
        info->frames = samples.size() / ch;
    }
    out = AudioBuffer(ch, r.info().sample_rate, std::move(samples));
    return true;
}

} // namespace audio
} // namespace fizmo
