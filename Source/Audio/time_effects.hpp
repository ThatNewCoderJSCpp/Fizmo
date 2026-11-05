#ifndef FIZMO_AUDIO_TIME_EFFECTS_HPP
#define FIZMO_AUDIO_TIME_EFFECTS_HPP

#include "effect.hpp"

#include <cstddef>
#include <cstdint>
#include <memory>

namespace fizmo {
namespace audio {

std::size_t delay_note_count() noexcept;
const char* delay_note_name(std::size_t index) noexcept;
float delay_note_beats(std::size_t index) noexcept;

class Delay : public Effect {
public:
    Delay();
    ~Delay() override;

    const char* name() const noexcept override { return "delay"; }
    void prepare(std::uint32_t sample_rate, std::size_t channels) override;
    void process(float* interleaved, std::size_t frames) noexcept override;
    void reset() noexcept override;
    std::size_t tail_frames() const noexcept override;

    float delay_seconds() const noexcept;

    Parameter time_ms;
    Parameter sync;
    Parameter bpm;
    Parameter note;
    Parameter feedback;
    Parameter pingpong;
    Parameter low_cut;
    Parameter high_cut;
    Parameter mix;

private:
    struct State;
    std::unique_ptr<State> m_state;
};

class Reverb : public Effect {
public:
    Reverb();
    ~Reverb() override;

    const char* name() const noexcept override { return "reverb"; }
    void prepare(std::uint32_t sample_rate, std::size_t channels) override;
    void process(float* interleaved, std::size_t frames) noexcept override;
    void reset() noexcept override;
    std::size_t tail_frames() const noexcept override;

    Parameter size;
    Parameter decay;
    Parameter damping;
    Parameter predelay_ms;
    Parameter diffusion;
    Parameter width;
    Parameter mix;

private:
    struct State;
    std::unique_ptr<State> m_state;
};

enum class DistortionMode {
    soft,
    hard,
    tube,
    fold
};

class Distortion : public Effect {
public:
    Distortion();
    ~Distortion() override;

    const char* name() const noexcept override { return "distortion"; }
    void prepare(std::uint32_t sample_rate, std::size_t channels) override;
    void process(float* interleaved, std::size_t frames) noexcept override;
    void reset() noexcept override;
    std::size_t latency_frames() const noexcept override;

    Parameter mode;
    Parameter drive;
    Parameter tone;
    Parameter mix;
    Parameter output;
    Parameter oversample;

private:
    struct State;
    std::unique_ptr<State> m_state;
};

class Bitcrusher : public Effect {
public:
    Bitcrusher();
    ~Bitcrusher() override;

    const char* name() const noexcept override { return "bitcrusher"; }
    void prepare(std::uint32_t sample_rate, std::size_t channels) override;
    void process(float* interleaved, std::size_t frames) noexcept override;
    void reset() noexcept override;

    Parameter bits;
    Parameter rate_hz;
    Parameter mix;

private:
    struct State;
    std::unique_ptr<State> m_state;
};

} // namespace audio
} // namespace fizmo

#endif // FIZMO_AUDIO_TIME_EFFECTS_HPP
