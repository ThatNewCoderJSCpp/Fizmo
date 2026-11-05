#include "buffer.hpp"

#include <algorithm>
#include <cmath>
#include <utility>

namespace fizmo {
namespace audio {

AudioBuffer::AudioBuffer(std::size_t channels, std::uint32_t sample_rate, std::size_t frames)
    : m_channels(channels), m_rate(sample_rate), m_samples(frames * channels, 0.0f) {}

AudioBuffer::AudioBuffer(std::size_t channels, std::uint32_t sample_rate, std::vector<float> interleaved)
    : m_channels(channels), m_rate(sample_rate), m_samples(std::move(interleaved)) {
    if (m_channels && m_samples.size() % m_channels) m_samples.resize(m_samples.size() - m_samples.size() % m_channels);
}

double AudioBuffer::duration() const noexcept {
    return m_rate ? static_cast<double>(frames()) / static_cast<double>(m_rate) : 0.0;
}

void AudioBuffer::set_channels(std::size_t channels) {
    if (channels == m_channels || channels == 0) return;
    if (m_channels == 0 || m_samples.empty()) {
        m_channels = channels;
        m_samples.clear();
        return;
    }
    *this = with_channels(channels);
}

void AudioBuffer::resize(std::size_t frames) {
    m_samples.resize(frames * m_channels, 0.0f);
}

void AudioBuffer::reserve(std::size_t frames) {
    m_samples.reserve(frames * m_channels);
}

void AudioBuffer::silence() noexcept {
    std::fill(m_samples.begin(), m_samples.end(), 0.0f);
}

void AudioBuffer::append(const float* interleaved, std::size_t frames) {
    if (!interleaved || frames == 0) return;
    m_samples.insert(m_samples.end(), interleaved, interleaved + frames * m_channels);
}

bool AudioBuffer::append(const AudioBuffer& other) {
    if (other.empty()) return true;
    if (m_channels == 0) {
        *this = other;
        return true;
    }
    if (other.m_channels != m_channels || other.m_rate != m_rate) return false;
    m_samples.insert(m_samples.end(), other.m_samples.begin(), other.m_samples.end());
    return true;
}

bool AudioBuffer::insert(std::size_t at_frame, const AudioBuffer& other) {
    if (other.empty()) return true;
    if (m_channels == 0) {
        if (at_frame != 0) return false;
        *this = other;
        return true;
    }
    if (other.m_channels != m_channels || other.m_rate != m_rate) return false;
    at_frame = std::min(at_frame, frames());
    m_samples.insert(m_samples.begin() + static_cast<std::ptrdiff_t>(at_frame * m_channels), other.m_samples.begin(), other.m_samples.end());
    return true;
}

void AudioBuffer::erase(std::size_t start_frame, std::size_t frame_count) {
    const std::size_t total = frames();
    if (start_frame >= total) return;
    const std::size_t end = std::min(total, start_frame + frame_count);
    m_samples.erase(m_samples.begin() + static_cast<std::ptrdiff_t>(start_frame * m_channels), m_samples.begin() + static_cast<std::ptrdiff_t>(end * m_channels));
}

bool AudioBuffer::mix_in(const AudioBuffer& other, std::size_t at_frame, float gain) {
    if (other.empty()) return true;
    if (m_channels == 0) {
        m_channels = other.m_channels;
        m_rate = other.m_rate;
    }
    if (other.m_rate != m_rate) return false;
    const AudioBuffer* src = &other;
    AudioBuffer remixed;
    if (other.m_channels != m_channels) {
        remixed = other.with_channels(m_channels);
        src = &remixed;
    }
    const std::size_t need = at_frame + src->frames();
    if (need > frames()) resize(need);
    float* dst = frame(at_frame);
    const float* s = src->data();
    const std::size_t n = src->sample_count();
    for (std::size_t i = 0; i < n; ++i) dst[i] += s[i] * gain;
    return true;
}

AudioBuffer AudioBuffer::slice(std::size_t start_frame, std::size_t frame_count) const {
    AudioBuffer out(m_channels, m_rate);
    const std::size_t total = frames();
    if (start_frame >= total) return out;
    const std::size_t end = std::min(total, start_frame + frame_count);
    out.m_samples.assign(m_samples.begin() + static_cast<std::ptrdiff_t>(start_frame * m_channels), m_samples.begin() + static_cast<std::ptrdiff_t>(end * m_channels));
    return out;
}

AudioBuffer AudioBuffer::with_channels(std::size_t channels) const {
    if (channels == 0 || channels == m_channels) return *this;
    AudioBuffer out(channels, m_rate, frames());
    remix(m_samples.data(), m_channels, out.data(), channels, frames());
    return out;
}

AudioBuffer AudioBuffer::resampled(std::uint32_t rate, ResampleQuality quality) const {
    return resample(*this, rate, quality);
}

AudioBuffer AudioBuffer::converted(std::size_t channels, std::uint32_t rate, ResampleQuality quality) const {
    if (channels != 0 && channels < m_channels) return with_channels(channels).resampled(rate, quality);
    AudioBuffer r = resampled(rate, quality);
    return channels ? r.with_channels(channels) : r;
}

std::vector<float> AudioBuffer::channel(std::size_t index) const {
    std::vector<float> out;
    if (index >= m_channels) return out;
    const std::size_t n = frames();
    out.resize(n);
    for (std::size_t i = 0; i < n; ++i) out[i] = m_samples[i * m_channels + index];
    return out;
}

void AudioBuffer::set_channel(std::size_t index, const std::vector<float>& values) {
    if (index >= m_channels) return;
    if (values.size() > frames()) resize(values.size());
    for (std::size_t i = 0; i < values.size(); ++i) m_samples[i * m_channels + index] = values[i];
}

std::size_t AudioBuffer::frame_at(double seconds) const noexcept {
    if (seconds <= 0.0 || m_rate == 0) return 0;
    const double f = std::floor(seconds * static_cast<double>(m_rate) + 0.5);
    return std::min(static_cast<std::size_t>(f), frames());
}

double AudioBuffer::seconds_at(std::size_t frame_index) const noexcept {
    return m_rate ? static_cast<double>(frame_index) / static_cast<double>(m_rate) : 0.0;
}

float AudioBuffer::peak() const noexcept {
    float p = 0.0f;
    for (float v : m_samples) p = std::max(p, std::fabs(v));
    return p;
}

float AudioBuffer::rms() const noexcept {
    if (m_samples.empty()) return 0.0f;
    double sum = 0.0;
    for (float v : m_samples) sum += static_cast<double>(v) * static_cast<double>(v);
    return static_cast<float>(std::sqrt(sum / static_cast<double>(m_samples.size())));
}

void AudioBuffer::apply_gain(float gain) noexcept {
    for (float& v : m_samples) v *= gain;
}

void AudioBuffer::normalize(float target_peak) noexcept {
    const float p = peak();
    if (p > 0.0f) apply_gain(target_peak / p);
}

void AudioBuffer::clamp() noexcept {
    for (float& v : m_samples) v = v < -1.0f ? -1.0f : (v > 1.0f ? 1.0f : v);
}

AudioBuffer resample(const AudioBuffer& input, std::uint32_t rate, ResampleQuality quality) {
    if (rate == 0 || input.sample_rate() == 0 || rate == input.sample_rate() || input.channels() == 0) {
        AudioBuffer copy = input;
        if (rate != 0 && input.sample_rate() == 0) copy.set_sample_rate(rate);
        return copy;
    }
    Resampler r(input.channels(), static_cast<double>(input.sample_rate()), static_cast<double>(rate), quality);
    std::vector<float> out;
    const double expected = std::ceil(static_cast<double>(input.frames()) * static_cast<double>(rate) / static_cast<double>(input.sample_rate()));
    out.reserve(static_cast<std::size_t>(expected + 8.0) * input.channels());
    r.process(input.data(), input.frames(), out);
    r.flush(out);
    return AudioBuffer(input.channels(), rate, std::move(out));
}

} // namespace audio
} // namespace fizmo
