#ifndef FIZMO_AUDIO_EFFECT_HPP
#define FIZMO_AUDIO_EFFECT_HPP

#include "buffer.hpp"

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace fizmo {
namespace audio {

class Parameter {
public:
    Parameter(const char* name, float value, float minimum, float maximum, const char* unit = "");

    const char* name() const noexcept { return m_name; }
    const char* unit() const noexcept { return m_unit; }
    float minimum() const noexcept { return m_min; }
    float maximum() const noexcept { return m_max; }
    float get() const noexcept { return m_value.load(std::memory_order_relaxed); }
    void set(float value) noexcept;
    operator float() const noexcept { return get(); }

private:
    const char* m_name;
    const char* m_unit;
    float m_min;
    float m_max;
    std::atomic<float> m_value;
};

class Effect {
public:
    virtual ~Effect() = default;

    virtual const char* name() const noexcept = 0;
    virtual void prepare(std::uint32_t sample_rate, std::size_t channels) = 0;
    virtual void process(float* interleaved, std::size_t frames) noexcept = 0;
    virtual void reset() noexcept = 0;
    virtual std::size_t latency_frames() const noexcept { return 0; }
    virtual std::size_t tail_frames() const noexcept { return 0; }
    virtual const char* type() const noexcept { return name(); }
    virtual std::string save_state() const { return std::string(); }
    virtual bool load_state(const std::string&) { return true; }

    std::vector<Parameter*> parameters() noexcept { return m_parameters; }
    std::vector<const Parameter*> parameters() const;
    const Parameter* parameter(const std::string& name) const noexcept;
    Parameter* parameter(const std::string& name) noexcept;
    bool set(const std::string& name, float value) noexcept;
    float get(const std::string& name) const noexcept;

    void set_enabled(bool on) noexcept { m_enabled.store(on); }
    bool enabled() const noexcept { return m_enabled.load(); }

    std::uint32_t sample_rate() const noexcept { return m_rate; }
    std::size_t channels() const noexcept { return m_channels; }

protected:
    void add_parameter(Parameter& p) { m_parameters.push_back(&p); }
    void clear_parameters() noexcept { m_parameters.clear(); }
    void set_format(std::uint32_t sample_rate, std::size_t channels) noexcept {
        m_rate = sample_rate;
        m_channels = channels;
    }

private:
    std::vector<Parameter*> m_parameters;
    std::atomic<bool> m_enabled{ true };
    std::uint32_t m_rate = 0;
    std::size_t m_channels = 0;
};

class EffectChain : public Effect {
public:
    const char* name() const noexcept override { return "chain"; }
    void prepare(std::uint32_t sample_rate, std::size_t channels) override;
    void process(float* interleaved, std::size_t frames) noexcept override;
    void reset() noexcept override;
    std::size_t latency_frames() const noexcept override;
    std::size_t tail_frames() const noexcept override;

    void add(std::shared_ptr<Effect> effect);
    bool insert(std::size_t index, std::shared_ptr<Effect> effect);
    bool remove(std::size_t index);
    bool move(std::size_t from, std::size_t to);
    void clear();
    std::size_t size() const;
    std::shared_ptr<Effect> at(std::size_t index) const;
    void assign(std::vector<std::shared_ptr<Effect>> effects);
    std::vector<std::shared_ptr<Effect>> effects() const;

private:
    using List = std::vector<std::shared_ptr<Effect>>;
    void publish(List next);

    mutable std::mutex m_mutex;
    std::shared_ptr<const List> m_list = std::make_shared<const List>();
    std::shared_ptr<const List> m_live;
    std::atomic<bool> m_dirty{ false };
    std::vector<std::shared_ptr<const List>> m_retired;
};

class Gain : public Effect {
public:
    explicit Gain(float db = 0.0f);
    const char* name() const noexcept override { return "gain"; }
    void prepare(std::uint32_t sample_rate, std::size_t channels) override;
    void process(float* interleaved, std::size_t frames) noexcept override;
    void reset() noexcept override;

    Parameter db;

private:
    float m_current = 1.0f;
    float m_smooth = 0.001f;
};

void apply(Effect& effect, AudioBuffer& buffer, bool keep_tail = true);
AudioBuffer applied(Effect& effect, const AudioBuffer& buffer, bool keep_tail = true);

} // namespace audio
} // namespace fizmo

#endif // FIZMO_AUDIO_EFFECT_HPP
