#ifndef FIZMO_AUDIO_PRESETS_HPP
#define FIZMO_AUDIO_PRESETS_HPP

#include "effect.hpp"
#include "voice.hpp"

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace fizmo {
namespace audio {

class JsonValue {
public:
    enum class Kind {
        null,
        boolean,
        number,
        string,
        array,
        object
    };

    JsonValue() = default;

    static JsonValue make_null() { return JsonValue(); }
    static JsonValue make_bool(bool value);
    static JsonValue make_number(double value);
    static JsonValue make_float(float value);
    static JsonValue make_string(std::string value);
    static JsonValue make_array();
    static JsonValue make_object();

    Kind kind() const noexcept { return m_kind; }
    bool is_null() const noexcept { return m_kind == Kind::null; }
    bool is_bool() const noexcept { return m_kind == Kind::boolean; }
    bool is_number() const noexcept { return m_kind == Kind::number; }
    bool is_string() const noexcept { return m_kind == Kind::string; }
    bool is_array() const noexcept { return m_kind == Kind::array; }
    bool is_object() const noexcept { return m_kind == Kind::object; }
    bool is_single_precision() const noexcept { return m_single; }

    bool as_bool(bool fallback = false) const noexcept;
    double as_number(double fallback = 0.0) const noexcept;
    float as_float(float fallback = 0.0f) const noexcept;
    const std::string& as_string() const noexcept;
    std::string as_string(const std::string& fallback) const;

    std::size_t size() const noexcept;
    const JsonValue& at(std::size_t index) const noexcept;
    JsonValue& at(std::size_t index) noexcept;
    const std::string& key(std::size_t index) const noexcept;
    JsonValue& push(JsonValue value);

    const JsonValue* find(const std::string& name) const noexcept;
    JsonValue* find(const std::string& name) noexcept;
    JsonValue& set(const std::string& name, JsonValue value);
    bool erase(const std::string& name);

private:
    Kind m_kind = Kind::null;
    bool m_bool = false;
    bool m_single = false;
    double m_number = 0.0;
    std::string m_string;
    std::vector<std::string> m_keys;
    std::vector<JsonValue> m_items;
};

bool parse_json(const std::string& text, JsonValue& out, std::string* error = nullptr);
std::string write_json(const JsonValue& value, bool pretty = true);

using EffectFactory = std::function<std::shared_ptr<Effect>()>;

struct EffectTypeInfo {
    std::string type;
    std::string display_name;
    std::string category;
    bool builtin = false;
};

bool register_effect(const std::string& type, EffectFactory factory, const std::string& display_name = "", const std::string& category = "");
bool unregister_effect(const std::string& type);
bool has_effect_type(const std::string& type);
std::shared_ptr<Effect> create_effect(const std::string& type);
std::vector<EffectTypeInfo> effect_types();
std::vector<EffectTypeInfo> effect_types(const std::string& category);
bool effect_type_info(const std::string& type, EffectTypeInfo& out);

struct PresetInfo {
    std::string name;
    std::string author;
    std::string description;
    std::string category;
    std::vector<std::string> tags;
    std::string version;
    int format_version = 1;
};

class MissingEffect : public Effect {
public:
    explicit MissingEffect(std::string type, JsonValue data = JsonValue());

    const char* name() const noexcept override { return "missing"; }
    const char* type() const noexcept override { return m_type.c_str(); }
    void prepare(std::uint32_t sample_rate, std::size_t channels) override;
    void process(float* interleaved, std::size_t frames) noexcept override;
    void reset() noexcept override;

    const std::string& missing_type() const noexcept { return m_type; }
    const JsonValue& data() const noexcept { return m_data; }

private:
    std::string m_type;
    JsonValue m_data;
};

JsonValue effect_to_json(const Effect& effect);
std::shared_ptr<Effect> effect_from_json(const JsonValue& value, std::vector<std::string>* warnings = nullptr, std::string* error = nullptr);

std::string save_effect(const Effect& effect, bool pretty = true);
std::shared_ptr<Effect> load_effect(const std::string& json, std::string* error = nullptr, std::vector<std::string>* warnings = nullptr);
std::shared_ptr<Effect> clone_effect(const Effect& effect);

std::string save_preset(const EffectChain& chain, const PresetInfo& info = PresetInfo());
bool load_preset(const std::string& json, EffectChain& out, PresetInfo* info = nullptr, std::string* error = nullptr, std::vector<std::string>* warnings = nullptr);
std::shared_ptr<EffectChain> load_preset_chain(const std::string& json, PresetInfo* info = nullptr, std::string* error = nullptr, std::vector<std::string>* warnings = nullptr);
bool read_preset_info(const std::string& json, PresetInfo& info, std::string* error = nullptr);
bool save_preset_file(const std::string& path, const EffectChain& chain, const PresetInfo& info = PresetInfo(), std::string* error = nullptr);
bool load_preset_file(const std::string& path, EffectChain& out, PresetInfo* info = nullptr, std::string* error = nullptr, std::vector<std::string>* warnings = nullptr);

std::shared_ptr<EffectChain> builtin_voice_preset(VoicePreset preset);
std::shared_ptr<EffectChain> builtin_voice_preset(const std::string& name);
std::vector<std::string> builtin_voice_preset_names();
std::string builtin_voice_preset_json(VoicePreset preset);

class PresetEffect : public Effect {
public:
    explicit PresetEffect(std::shared_ptr<EffectChain> chain = nullptr);
    ~PresetEffect() override;
    PresetEffect(const PresetEffect&) = delete;
    PresetEffect& operator=(const PresetEffect&) = delete;

    const char* name() const noexcept override { return "preset"; }
    void prepare(std::uint32_t sample_rate, std::size_t channels) override;
    void process(float* interleaved, std::size_t frames) noexcept override;
    void reset() noexcept override;
    std::size_t latency_frames() const noexcept override;
    std::size_t tail_frames() const noexcept override;

    void set_chain(std::shared_ptr<EffectChain> chain);
    std::shared_ptr<EffectChain> chain() const;
    bool crossfading() const noexcept { return m_fading.load(std::memory_order_relaxed); }

    bool load(const std::string& json, std::string* error = nullptr, std::vector<std::string>* warnings = nullptr);
    bool load_file(const std::string& path, std::string* error = nullptr, std::vector<std::string>* warnings = nullptr);
    std::string save() const;
    bool save_file(const std::string& path, std::string* error = nullptr) const;
    PresetInfo info() const;
    void set_info(const PresetInfo& info);

    Parameter mix;
    Parameter output;
    Parameter crossfade_ms;

private:
    struct State;
    std::unique_ptr<State> m_state;
    std::atomic<bool> m_fading{ false };
};

} // namespace audio
} // namespace fizmo

#endif // FIZMO_AUDIO_PRESETS_HPP
