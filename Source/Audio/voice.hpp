#ifndef FIZMO_AUDIO_VOICE_HPP
#define FIZMO_AUDIO_VOICE_HPP

#include "effect.hpp"
#include "modulation.hpp"
#include "pitch.hpp"
#include "time_effects.hpp"

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>

namespace fizmo {
namespace audio {

enum class VoicePreset {
    none,
    helium,
    deep,
    robot,
    alien,
    monster,
    radio,
    telephone,
    megaphone,
    cave,
    underwater,
    whisper,
    feminine,
    masculine
};

std::size_t voice_preset_count() noexcept;
const char* voice_preset_name(VoicePreset preset) noexcept;

class VoiceEQ : public Effect {
public:
    VoiceEQ();
    ~VoiceEQ() override;

    const char* name() const noexcept override { return "voice_eq"; }
    void prepare(std::uint32_t sample_rate, std::size_t channels) override;
    void process(float* interleaved, std::size_t frames) noexcept override;
    void reset() noexcept override;

    Parameter low_cut;
    Parameter high_cut;
    Parameter peak_hz;
    Parameter peak_db;
    Parameter peak_q;

private:
    struct State;
    std::unique_ptr<State> m_state;
};

class RingModulator : public Effect {
public:
    RingModulator();
    ~RingModulator() override;

    const char* name() const noexcept override { return "ring_modulator"; }
    void prepare(std::uint32_t sample_rate, std::size_t channels) override;
    void process(float* interleaved, std::size_t frames) noexcept override;
    void reset() noexcept override;

    Parameter frequency;
    Parameter shape;
    Parameter mix;

private:
    struct State;
    std::unique_ptr<State> m_state;
};

enum class VocoderCarrier {
    saw,
    square,
    noise,
    sidechain
};

class Vocoder : public Effect {
public:
    Vocoder();
    ~Vocoder() override;

    const char* name() const noexcept override { return "vocoder"; }
    void prepare(std::uint32_t sample_rate, std::size_t channels) override;
    void process(float* interleaved, std::size_t frames) noexcept override;
    void reset() noexcept override;

    std::size_t push_sidechain(const float* mono, std::size_t frames) noexcept;
    std::size_t sidechain_available() const noexcept;

    Parameter carrier;
    Parameter pitch;
    Parameter bands;
    Parameter attack_ms;
    Parameter release_ms;
    Parameter sibilance;
    Parameter shift;
    Parameter mix;

private:
    struct State;
    std::unique_ptr<State> m_state;
};

class VoiceChanger : public Effect {
public:
    explicit VoiceChanger(VoicePreset initial = VoicePreset::none);
    ~VoiceChanger() override;

    const char* name() const noexcept override { return "voice_changer"; }
    void prepare(std::uint32_t sample_rate, std::size_t channels) override;
    void process(float* interleaved, std::size_t frames) noexcept override;
    void reset() noexcept override;
    std::size_t latency_frames() const noexcept override;
    std::size_t tail_frames() const noexcept override;

    VoicePreset current_preset() const noexcept { return static_cast<VoicePreset>(m_current.load(std::memory_order_relaxed)); }
    bool crossfading() const noexcept { return m_fading.load(std::memory_order_relaxed); }

    Parameter preset;
    Parameter mix;
    Parameter output_gain;

private:
    struct State;
    std::unique_ptr<State> m_state;
    std::atomic<int> m_current{ 0 };
    std::atomic<bool> m_fading{ false };
};

std::shared_ptr<EffectChain> make_voice_chain(VoicePreset preset);

} // namespace audio
} // namespace fizmo

#endif // FIZMO_AUDIO_VOICE_HPP
