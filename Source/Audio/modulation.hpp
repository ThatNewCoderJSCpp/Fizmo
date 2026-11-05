#ifndef FIZMO_AUDIO_MODULATION_HPP
#define FIZMO_AUDIO_MODULATION_HPP

#include "effect.hpp"

#include <cstddef>
#include <cstdint>
#include <memory>

namespace fizmo {
namespace audio {

enum class LfoShape {
    sine,
    triangle,
    square,
    saw_up,
    saw_down,
    random
};

float lfo_value(LfoShape shape, double cycles) noexcept;

class Chorus : public Effect {
public:
    Chorus();
    ~Chorus() override;

    const char* name() const noexcept override { return "chorus"; }
    void prepare(std::uint32_t sample_rate, std::size_t channels) override;
    void process(float* interleaved, std::size_t frames) noexcept override;
    void reset() noexcept override;

    Parameter rate;
    Parameter depth_ms;
    Parameter delay_ms;
    Parameter voices;
    Parameter feedback;
    Parameter mix;
    Parameter spread;
    Parameter shape;

private:
    struct State;
    std::unique_ptr<State> m_state;
};

class Flanger : public Effect {
public:
    Flanger();
    ~Flanger() override;

    const char* name() const noexcept override { return "flanger"; }
    void prepare(std::uint32_t sample_rate, std::size_t channels) override;
    void process(float* interleaved, std::size_t frames) noexcept override;
    void reset() noexcept override;

    Parameter rate;
    Parameter depth_ms;
    Parameter delay_ms;
    Parameter feedback;
    Parameter mix;
    Parameter spread;
    Parameter shape;

private:
    struct State;
    std::unique_ptr<State> m_state;
};

class Phaser : public Effect {
public:
    Phaser();
    ~Phaser() override;

    const char* name() const noexcept override { return "phaser"; }
    void prepare(std::uint32_t sample_rate, std::size_t channels) override;
    void process(float* interleaved, std::size_t frames) noexcept override;
    void reset() noexcept override;

    Parameter rate;
    Parameter depth;
    Parameter stages;
    Parameter feedback;
    Parameter min_hz;
    Parameter max_hz;
    Parameter mix;
    Parameter spread;
    Parameter shape;

private:
    struct State;
    std::unique_ptr<State> m_state;
};

class Tremolo : public Effect {
public:
    Tremolo();
    ~Tremolo() override;

    const char* name() const noexcept override { return "tremolo"; }
    void prepare(std::uint32_t sample_rate, std::size_t channels) override;
    void process(float* interleaved, std::size_t frames) noexcept override;
    void reset() noexcept override;

    Parameter rate;
    Parameter depth;
    Parameter spread;
    Parameter shape;

private:
    struct State;
    std::unique_ptr<State> m_state;
};

class Vibrato : public Effect {
public:
    Vibrato();
    ~Vibrato() override;

    const char* name() const noexcept override { return "vibrato"; }
    void prepare(std::uint32_t sample_rate, std::size_t channels) override;
    void process(float* interleaved, std::size_t frames) noexcept override;
    void reset() noexcept override;
    std::size_t latency_frames() const noexcept override;

    Parameter rate;
    Parameter depth_ms;
    Parameter mix;
    Parameter spread;
    Parameter shape;

private:
    struct State;
    std::unique_ptr<State> m_state;
};

class AutoPan : public Effect {
public:
    AutoPan();
    ~AutoPan() override;

    const char* name() const noexcept override { return "autopan"; }
    void prepare(std::uint32_t sample_rate, std::size_t channels) override;
    void process(float* interleaved, std::size_t frames) noexcept override;
    void reset() noexcept override;

    Parameter rate;
    Parameter depth;
    Parameter shape;

private:
    struct State;
    std::unique_ptr<State> m_state;
};

} // namespace audio
} // namespace fizmo

#endif // FIZMO_AUDIO_MODULATION_HPP
