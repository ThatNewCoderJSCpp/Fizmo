#ifndef FIZMO_AUDIO_NOISE_HPP
#define FIZMO_AUDIO_NOISE_HPP

#include "buffer.hpp"
#include "effect.hpp"

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace fizmo {
namespace audio {

class NoiseReduction : public Effect {
public:
    explicit NoiseReduction(std::size_t fft_size = 0);
    ~NoiseReduction() override;
    NoiseReduction(const NoiseReduction&) = delete;
    NoiseReduction& operator=(const NoiseReduction&) = delete;

    const char* name() const noexcept override { return "noise_reduction"; }
    void prepare(std::uint32_t sample_rate, std::size_t channels) override;
    void process(float* interleaved, std::size_t frames) noexcept override;
    void reset() noexcept override;
    std::size_t latency_frames() const noexcept override;
    std::string save_state() const override;
    bool load_state(const std::string& state) override;

    bool learn_profile(const AudioBuffer& noise_only);
    void clear_profile();
    bool has_profile() const;
    std::vector<float> profile_spectrum() const;
    std::size_t fft_size() const noexcept;
    std::size_t hop_size() const noexcept;

    Parameter mode;
    Parameter reduction_db;
    Parameter sensitivity;
    Parameter frequency_smoothing;
    Parameter time_smoothing;
    Parameter per_channel;

private:
    struct State;
    void run_frame() noexcept;

    std::unique_ptr<State> m_state;
};

AudioBuffer reduce_noise(const AudioBuffer& input, const AudioBuffer* noise_sample = nullptr, float reduction_db = 18.0f);

class HumRemover : public Effect {
public:
    explicit HumRemover(float frequency = 0.0f);
    ~HumRemover() override;
    HumRemover(const HumRemover&) = delete;
    HumRemover& operator=(const HumRemover&) = delete;

    const char* name() const noexcept override { return "hum_remover"; }
    void prepare(std::uint32_t sample_rate, std::size_t channels) override;
    void process(float* interleaved, std::size_t frames) noexcept override;
    void reset() noexcept override;

    float detected_frequency() const noexcept;
    float active_frequency() const noexcept;

    Parameter frequency;
    Parameter harmonics;
    Parameter width;

private:
    struct State;
    void update_filters(double base, std::size_t count, double bandwidth) noexcept;
    void detect(const float* interleaved, std::size_t frames) noexcept;

    std::unique_ptr<State> m_state;
};

class ClickRemover : public Effect {
public:
    ClickRemover();
    ~ClickRemover() override;
    ClickRemover(const ClickRemover&) = delete;
    ClickRemover& operator=(const ClickRemover&) = delete;

    const char* name() const noexcept override { return "click_remover"; }
    void prepare(std::uint32_t sample_rate, std::size_t channels) override;
    void process(float* interleaved, std::size_t frames) noexcept override;
    void reset() noexcept override;
    std::size_t latency_frames() const noexcept override;

    std::uint64_t clicks_repaired() const noexcept;

    Parameter sensitivity;
    Parameter max_width;

private:
    struct State;
    void run_block(std::size_t channel) noexcept;

    std::unique_ptr<State> m_state;
};

} // namespace audio
} // namespace fizmo

#endif // FIZMO_AUDIO_NOISE_HPP
