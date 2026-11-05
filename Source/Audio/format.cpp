#include "format.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>

namespace fizmo {
namespace audio {

namespace {

std::uint32_t& dither_state() noexcept {
    thread_local std::uint32_t state = 0x9E3779B9u;
    return state;
}

float dither_noise() noexcept {
    std::uint32_t& s = dither_state();
    s ^= s << 13;
    s ^= s >> 17;
    s ^= s << 5;
    const float a = static_cast<float>(s & 0xFFFFu) / 65536.0f;
    s ^= s << 13;
    s ^= s >> 17;
    s ^= s << 5;
    const float b = static_cast<float>(s & 0xFFFFu) / 65536.0f;
    return a - b;
}

float clamp_unit(float v) noexcept {
    if (!(v == v)) return 0.0f;
    return v < -1.0f ? -1.0f : (v > 1.0f ? 1.0f : v);
}

std::int32_t quantize(float v, double scale, double lo, double hi, bool dither) noexcept {
    double x = static_cast<double>(clamp_unit(v)) * scale;
    if (dither) x += static_cast<double>(dither_noise());
    x = std::nearbyint(x);
    if (x < lo) x = lo;
    if (x > hi) x = hi;
    return static_cast<std::int32_t>(x);
}

std::int16_t alaw_decode_linear(std::uint8_t a) noexcept {
    a ^= 0x55;
    int t = (a & 0x0F) << 4;
    const int seg = (a & 0x70) >> 4;
    switch (seg) {
        case 0: t += 8; break;
        case 1: t += 0x108; break;
        default: t += 0x108; t <<= seg - 1; break;
    }
    return static_cast<std::int16_t>((a & 0x80) ? t : -t);
}

std::int16_t ulaw_decode_linear(std::uint8_t u) noexcept {
    u = static_cast<std::uint8_t>(~u);
    int t = ((u & 0x0F) << 3) + 0x84;
    t <<= (u & 0x70) >> 4;
    return static_cast<std::int16_t>((u & 0x80) ? (0x84 - t) : (t - 0x84));
}

int segment_of(int value, const int* ends, int count) noexcept {
    for (int i = 0; i < count; ++i) {
        if (value <= ends[i]) return i;
    }
    return count;
}

} // namespace

std::size_t bytes_per_sample(SampleFormat format) noexcept {
    switch (format) {
        case SampleFormat::u8: return 1;
        case SampleFormat::s16: return 2;
        case SampleFormat::s24: return 3;
        case SampleFormat::s32: return 4;
        case SampleFormat::f32: return 4;
        case SampleFormat::f64: return 8;
        case SampleFormat::alaw: return 1;
        case SampleFormat::ulaw: return 1;
    }
    return 0;
}

std::size_t bits_per_sample(SampleFormat format) noexcept {
    return bytes_per_sample(format) * 8;
}

bool is_float_format(SampleFormat format) noexcept {
    return format == SampleFormat::f32 || format == SampleFormat::f64;
}

const char* format_name(SampleFormat format) noexcept {
    switch (format) {
        case SampleFormat::u8: return "u8";
        case SampleFormat::s16: return "s16";
        case SampleFormat::s24: return "s24";
        case SampleFormat::s32: return "s32";
        case SampleFormat::f32: return "f32";
        case SampleFormat::f64: return "f64";
        case SampleFormat::alaw: return "alaw";
        case SampleFormat::ulaw: return "ulaw";
    }
    return "unknown";
}

float alaw_to_float(std::uint8_t value) noexcept {
    return static_cast<float>(alaw_decode_linear(value)) / 32768.0f;
}

float ulaw_to_float(std::uint8_t value) noexcept {
    return static_cast<float>(ulaw_decode_linear(value)) / 32768.0f;
}

std::uint8_t float_to_alaw(float value) noexcept {
    static const int ends[8] = { 0x1F, 0x3F, 0x7F, 0xFF, 0x1FF, 0x3FF, 0x7FF, 0xFFF };
    int pcm = static_cast<int>(std::lrint(static_cast<double>(clamp_unit(value)) * 32767.0)) >> 3;
    int mask;
    if (pcm >= 0) {
        mask = 0xD5;
    } else {
        mask = 0x55;
        pcm = -pcm - 1;
    }
    const int seg = segment_of(pcm, ends, 8);
    if (seg >= 8) return static_cast<std::uint8_t>(0x7F ^ mask);
    int a = seg << 4;
    a |= seg < 2 ? (pcm >> 1) & 0x0F : (pcm >> seg) & 0x0F;
    return static_cast<std::uint8_t>(a ^ mask);
}

std::uint8_t float_to_ulaw(float value) noexcept {
    static const int ends[8] = { 0x3F, 0x7F, 0xFF, 0x1FF, 0x3FF, 0x7FF, 0xFFF, 0x1FFF };
    int pcm = static_cast<int>(std::lrint(static_cast<double>(clamp_unit(value)) * 32767.0)) >> 2;
    int mask;
    if (pcm < 0) {
        pcm = -pcm;
        mask = 0x7F;
    } else {
        mask = 0xFF;
    }
    if (pcm > 8159) pcm = 8159;
    pcm += 0x21;
    const int seg = segment_of(pcm, ends, 8);
    if (seg >= 8) return static_cast<std::uint8_t>(0x7F ^ mask);
    const int u = (seg << 4) | ((pcm >> (seg + 1)) & 0x0F);
    return static_cast<std::uint8_t>(u ^ mask);
}

void to_float(const void* source, SampleFormat format, float* destination, std::size_t samples) noexcept {
    const std::uint8_t* p = static_cast<const std::uint8_t*>(source);
    switch (format) {
        case SampleFormat::u8:
            for (std::size_t i = 0; i < samples; ++i) destination[i] = (static_cast<float>(p[i]) - 128.0f) / 128.0f;
            break;
        case SampleFormat::s16:
            for (std::size_t i = 0; i < samples; ++i) {
                const std::int16_t v = static_cast<std::int16_t>(static_cast<std::uint16_t>(p[2 * i]) | (static_cast<std::uint16_t>(p[2 * i + 1]) << 8));
                destination[i] = static_cast<float>(v) / 32768.0f;
            }
            break;
        case SampleFormat::s24:
            for (std::size_t i = 0; i < samples; ++i) {
                std::int32_t v = static_cast<std::int32_t>(static_cast<std::uint32_t>(p[3 * i]) | (static_cast<std::uint32_t>(p[3 * i + 1]) << 8) | (static_cast<std::uint32_t>(p[3 * i + 2]) << 16));
                if (v & 0x800000) v -= 0x1000000;
                destination[i] = static_cast<float>(v) / 8388608.0f;
            }
            break;
        case SampleFormat::s32:
            for (std::size_t i = 0; i < samples; ++i) {
                const std::uint32_t u = static_cast<std::uint32_t>(p[4 * i]) | (static_cast<std::uint32_t>(p[4 * i + 1]) << 8) | (static_cast<std::uint32_t>(p[4 * i + 2]) << 16) | (static_cast<std::uint32_t>(p[4 * i + 3]) << 24);
                std::int32_t v;
                std::memcpy(&v, &u, 4);
                destination[i] = static_cast<float>(static_cast<double>(v) / 2147483648.0);
            }
            break;
        case SampleFormat::f32:
            std::memcpy(destination, p, samples * 4);
            break;
        case SampleFormat::f64:
            for (std::size_t i = 0; i < samples; ++i) {
                double v;
                std::memcpy(&v, p + 8 * i, 8);
                destination[i] = static_cast<float>(v);
            }
            break;
        case SampleFormat::alaw:
            for (std::size_t i = 0; i < samples; ++i) destination[i] = alaw_to_float(p[i]);
            break;
        case SampleFormat::ulaw:
            for (std::size_t i = 0; i < samples; ++i) destination[i] = ulaw_to_float(p[i]);
            break;
    }
}

void from_float(const float* source, void* destination, SampleFormat format, std::size_t samples, bool dither) noexcept {
    std::uint8_t* p = static_cast<std::uint8_t*>(destination);
    switch (format) {
        case SampleFormat::u8:
            for (std::size_t i = 0; i < samples; ++i) p[i] = static_cast<std::uint8_t>(quantize(source[i], 128.0, -128.0, 127.0, dither) + 128);
            break;
        case SampleFormat::s16:
            for (std::size_t i = 0; i < samples; ++i) {
                const std::uint16_t v = static_cast<std::uint16_t>(static_cast<std::int16_t>(quantize(source[i], 32768.0, -32768.0, 32767.0, dither)));
                p[2 * i] = static_cast<std::uint8_t>(v & 0xFF);
                p[2 * i + 1] = static_cast<std::uint8_t>(v >> 8);
            }
            break;
        case SampleFormat::s24:
            for (std::size_t i = 0; i < samples; ++i) {
                const std::uint32_t v = static_cast<std::uint32_t>(quantize(source[i], 8388608.0, -8388608.0, 8388607.0, dither));
                p[3 * i] = static_cast<std::uint8_t>(v & 0xFF);
                p[3 * i + 1] = static_cast<std::uint8_t>((v >> 8) & 0xFF);
                p[3 * i + 2] = static_cast<std::uint8_t>((v >> 16) & 0xFF);
            }
            break;
        case SampleFormat::s32:
            for (std::size_t i = 0; i < samples; ++i) {
                double x = static_cast<double>(clamp_unit(source[i])) * 2147483648.0;
                x = std::nearbyint(x);
                if (x > 2147483647.0) x = 2147483647.0;
                if (x < -2147483648.0) x = -2147483648.0;
                const std::uint32_t v = static_cast<std::uint32_t>(static_cast<std::int32_t>(x));
                p[4 * i] = static_cast<std::uint8_t>(v & 0xFF);
                p[4 * i + 1] = static_cast<std::uint8_t>((v >> 8) & 0xFF);
                p[4 * i + 2] = static_cast<std::uint8_t>((v >> 16) & 0xFF);
                p[4 * i + 3] = static_cast<std::uint8_t>(v >> 24);
            }
            break;
        case SampleFormat::f32:
            std::memcpy(p, source, samples * 4);
            break;
        case SampleFormat::f64:
            for (std::size_t i = 0; i < samples; ++i) {
                const double v = static_cast<double>(source[i]);
                std::memcpy(p + 8 * i, &v, 8);
            }
            break;
        case SampleFormat::alaw:
            for (std::size_t i = 0; i < samples; ++i) p[i] = float_to_alaw(source[i]);
            break;
        case SampleFormat::ulaw:
            for (std::size_t i = 0; i < samples; ++i) p[i] = float_to_ulaw(source[i]);
            break;
    }
}

void remix(const float* source, std::size_t source_channels, float* destination, std::size_t destination_channels, std::size_t frames) noexcept {
    const std::size_t sc = source_channels;
    const std::size_t dc = destination_channels;
    if (sc == 0 || dc == 0) return;
    if (sc == dc) {
        if (source != destination) std::memmove(destination, source, frames * sc * sizeof(float));
        return;
    }
    const float h = 0.70710678f;
    if (dc < sc) {
        for (std::size_t f = 0; f < frames; ++f) {
            const float* in = source + f * sc;
            float l = 0.0f;
            float r = 0.0f;
            if (sc == 2) {
                l = in[0];
                r = in[1];
            } else {
                for (std::size_t c = 0; c < sc; ++c) {
                    switch (c) {
                        case 0: l += in[c]; break;
                        case 1: r += in[c]; break;
                        case 2: l += h * in[c]; r += h * in[c]; break;
                        case 3: break;
                        case 4: case 6: l += h * in[c]; break;
                        case 5: case 7: r += h * in[c]; break;
                        default:
                            if (c % 2 == 0) l += 0.5f * in[c];
                            else r += 0.5f * in[c];
                            break;
                    }
                }
            }
            float* out = destination + f * dc;
            if (dc == 1) {
                out[0] = 0.5f * (l + r);
            } else if (dc == 2) {
                out[0] = l;
                out[1] = r;
            } else {
                float tmp[64];
                const std::size_t keep = std::min<std::size_t>(dc, 64);
                for (std::size_t c = 0; c < keep; ++c) tmp[c] = in[c];
                for (std::size_t c = 0; c < keep; ++c) out[c] = tmp[c];
            }
        }
        return;
    }
    for (std::size_t i = frames; i-- > 0;) {
        const float* in = source + i * sc;
        float* out = destination + i * dc;
        float tmp[64];
        const std::size_t keep = std::min<std::size_t>(sc, 64);
        for (std::size_t c = 0; c < keep; ++c) tmp[c] = in[c];
        if (sc == 1) {
            out[0] = tmp[0];
            if (dc > 1) out[1] = tmp[0];
            for (std::size_t c = 2; c < dc; ++c) out[c] = 0.0f;
        } else {
            for (std::size_t c = 0; c < dc; ++c) out[c] = c < keep ? tmp[c] : 0.0f;
        }
    }
}

float db_to_gain(float db) noexcept {
    return std::pow(10.0f, db / 20.0f);
}

float gain_to_db(float gain) noexcept {
    if (gain <= 0.0f) return -std::numeric_limits<float>::infinity();
    return 20.0f * std::log10(gain);
}

} // namespace audio
} // namespace fizmo
