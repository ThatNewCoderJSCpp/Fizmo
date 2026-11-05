#ifndef FIZMO_AUDIO_BUFFER_HPP
#define FIZMO_AUDIO_BUFFER_HPP

#include "format.hpp"
#include "resampler.hpp"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace fizmo {
namespace audio {

class AudioBuffer {
public:
    AudioBuffer() = default;
    AudioBuffer(std::size_t channels, std::uint32_t sample_rate, std::size_t frames = 0);
    AudioBuffer(std::size_t channels, std::uint32_t sample_rate, std::vector<float> interleaved);

    std::size_t channels() const noexcept { return m_channels; }
    std::uint32_t sample_rate() const noexcept { return m_rate; }
    std::size_t frames() const noexcept { return m_channels ? m_samples.size() / m_channels : 0; }
    std::size_t sample_count() const noexcept { return m_samples.size(); }
    double duration() const noexcept;
    bool empty() const noexcept { return m_samples.empty(); }

    float* data() noexcept { return m_samples.data(); }
    const float* data() const noexcept { return m_samples.data(); }
    float* frame(std::size_t index) noexcept { return m_samples.data() + index * m_channels; }
    const float* frame(std::size_t index) const noexcept { return m_samples.data() + index * m_channels; }
    float& at(std::size_t frame_index, std::size_t channel) noexcept { return m_samples[frame_index * m_channels + channel]; }
    float at(std::size_t frame_index, std::size_t channel) const noexcept { return m_samples[frame_index * m_channels + channel]; }
    std::vector<float>& samples() noexcept { return m_samples; }
    const std::vector<float>& samples() const noexcept { return m_samples; }

    void set_sample_rate(std::uint32_t rate) noexcept { m_rate = rate; }
    void set_channels(std::size_t channels);
    void resize(std::size_t frames);
    void reserve(std::size_t frames);
    void clear() noexcept { m_samples.clear(); }
    void silence() noexcept;

    void append(const float* interleaved, std::size_t frames);
    bool append(const AudioBuffer& other);
    bool insert(std::size_t at_frame, const AudioBuffer& other);
    void erase(std::size_t start_frame, std::size_t frame_count);
    bool mix_in(const AudioBuffer& other, std::size_t at_frame = 0, float gain = 1.0f);

    AudioBuffer slice(std::size_t start_frame, std::size_t frame_count) const;
    AudioBuffer with_channels(std::size_t channels) const;
    AudioBuffer resampled(std::uint32_t rate, ResampleQuality quality = ResampleQuality::high) const;
    AudioBuffer converted(std::size_t channels, std::uint32_t rate, ResampleQuality quality = ResampleQuality::high) const;

    std::vector<float> channel(std::size_t index) const;
    void set_channel(std::size_t index, const std::vector<float>& values);

    std::size_t frame_at(double seconds) const noexcept;
    double seconds_at(std::size_t frame_index) const noexcept;

    float peak() const noexcept;
    float rms() const noexcept;
    void apply_gain(float gain) noexcept;
    void normalize(float target_peak = 1.0f) noexcept;
    void clamp() noexcept;

private:
    std::size_t m_channels = 0;
    std::uint32_t m_rate = 0;
    std::vector<float> m_samples;
};

AudioBuffer resample(const AudioBuffer& input, std::uint32_t rate, ResampleQuality quality = ResampleQuality::high);

} // namespace audio
} // namespace fizmo

#endif // FIZMO_AUDIO_BUFFER_HPP
