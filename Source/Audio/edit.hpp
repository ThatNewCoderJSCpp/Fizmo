#ifndef FIZMO_AUDIO_EDIT_HPP
#define FIZMO_AUDIO_EDIT_HPP

#include "buffer.hpp"

#include <cstddef>
#include <utility>
#include <vector>

namespace fizmo {
namespace audio {

enum class FadeCurve {
    linear,
    equal_power,
    logarithmic,
    exponential,
    s_curve
};

using FrameRange = std::pair<std::size_t, std::size_t>;

float fade_shape(FadeCurve curve, double t) noexcept;
void fade(AudioBuffer& buffer, std::size_t start_frame, std::size_t frames, float from_gain, float to_gain, FadeCurve curve = FadeCurve::linear);
void fade_in(AudioBuffer& buffer, double seconds, FadeCurve curve = FadeCurve::linear);
void fade_out(AudioBuffer& buffer, double seconds, FadeCurve curve = FadeCurve::linear);
AudioBuffer crossfade(const AudioBuffer& a, const AudioBuffer& b, double seconds, FadeCurve curve = FadeCurve::equal_power);
void apply_gain_envelope(AudioBuffer& buffer, std::vector<std::pair<double, float>> points_seconds_db);

void reverse(AudioBuffer& buffer);
void invert_polarity(AudioBuffer& buffer);
void remove_dc(AudioBuffer& buffer);

std::vector<FrameRange> detect_silence(const AudioBuffer& buffer, float threshold_db = -60.0f, double min_duration_seconds = 0.1);
std::vector<FrameRange> trim_silence(AudioBuffer& buffer, float threshold_db = -60.0f, double keep_ms = 10.0);
std::vector<AudioBuffer> split_on_silence(const AudioBuffer& buffer, float threshold_db = -50.0f, double min_silence_seconds = 0.3, double keep_ms = 10.0);

AudioBuffer mix(const AudioBuffer& a, const AudioBuffer& b, float gain_b = 1.0f);
void swap_channels(AudioBuffer& buffer, std::size_t first = 0, std::size_t second = 1);
AudioBuffer mono_to_stereo(const AudioBuffer& buffer);
AudioBuffer stereo_to_mono(const AudioBuffer& buffer);
void pad(AudioBuffer& buffer, double before_seconds, double after_seconds);

} // namespace audio
} // namespace fizmo

#endif // FIZMO_AUDIO_EDIT_HPP
