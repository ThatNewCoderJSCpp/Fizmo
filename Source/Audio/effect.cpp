#include "effect.hpp"
#include "format.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <utility>

namespace fizmo {
namespace audio {

Parameter::Parameter(const char* name, float value, float minimum, float maximum, const char* unit)
    : m_name(name), m_unit(unit), m_min(minimum), m_max(maximum), m_value(std::min(maximum, std::max(minimum, value))) {}

void Parameter::set(float value) noexcept {
    if (!(value == value)) return;
    m_value.store(std::min(m_max, std::max(m_min, value)), std::memory_order_relaxed);
}

Parameter* Effect::parameter(const std::string& name) noexcept {
    for (Parameter* p : m_parameters) {
        if (name == p->name()) return p;
    }
    return nullptr;
}

const Parameter* Effect::parameter(const std::string& name) const noexcept {
    for (const Parameter* p : m_parameters) {
        if (name == p->name()) return p;
    }
    return nullptr;
}

std::vector<const Parameter*> Effect::parameters() const {
    return std::vector<const Parameter*>(m_parameters.begin(), m_parameters.end());
}

bool Effect::set(const std::string& name, float value) noexcept {
    Parameter* p = parameter(name);
    if (!p) return false;
    p->set(value);
    return true;
}

float Effect::get(const std::string& name) const noexcept {
    for (const Parameter* p : m_parameters) {
        if (name == p->name()) return p->get();
    }
    return 0.0f;
}

void EffectChain::prepare(std::uint32_t sample_rate, std::size_t channels) {
    set_format(sample_rate, channels);
    std::lock_guard<std::mutex> lock(m_mutex);
    for (const auto& e : *m_list) e->prepare(sample_rate, channels);
    m_live = m_list;
    m_dirty.store(false);
    m_retired.reserve(64);
}

void EffectChain::process(float* interleaved, std::size_t frames) noexcept {
    if (m_dirty.load(std::memory_order_acquire) && m_mutex.try_lock()) {
        if (m_live) m_retired.push_back(std::move(m_live));
        m_live = m_list;
        m_dirty.store(false);
        m_mutex.unlock();
    }
    if (!m_live) return;
    for (const auto& e : *m_live) {
        if (e->enabled()) e->process(interleaved, frames);
    }
}

void EffectChain::reset() noexcept {
    std::lock_guard<std::mutex> lock(m_mutex);
    for (const auto& e : *m_list) e->reset();
}

std::size_t EffectChain::latency_frames() const noexcept {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::size_t n = 0;
    for (const auto& e : *m_list) {
        if (e->enabled()) n += e->latency_frames();
    }
    return n;
}

std::size_t EffectChain::tail_frames() const noexcept {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::size_t n = 0;
    for (const auto& e : *m_list) {
        if (e->enabled()) n += e->tail_frames();
    }
    return n;
}

void EffectChain::publish(List next) {
    m_retired.clear();
    m_list = std::make_shared<const List>(std::move(next));
    m_dirty.store(true, std::memory_order_release);
}

void EffectChain::add(std::shared_ptr<Effect> effect) {
    if (!effect) return;
    if (sample_rate()) effect->prepare(sample_rate(), channels());
    std::lock_guard<std::mutex> lock(m_mutex);
    List next = *m_list;
    next.push_back(std::move(effect));
    publish(std::move(next));
}

bool EffectChain::insert(std::size_t index, std::shared_ptr<Effect> effect) {
    if (!effect) return false;
    if (sample_rate()) effect->prepare(sample_rate(), channels());
    std::lock_guard<std::mutex> lock(m_mutex);
    if (index > m_list->size()) return false;
    List next = *m_list;
    next.insert(next.begin() + static_cast<std::ptrdiff_t>(index), std::move(effect));
    publish(std::move(next));
    return true;
}

bool EffectChain::remove(std::size_t index) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (index >= m_list->size()) return false;
    List next = *m_list;
    next.erase(next.begin() + static_cast<std::ptrdiff_t>(index));
    publish(std::move(next));
    return true;
}

bool EffectChain::move(std::size_t from, std::size_t to) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (from >= m_list->size() || to >= m_list->size()) return false;
    List next = *m_list;
    std::shared_ptr<Effect> e = next[from];
    next.erase(next.begin() + static_cast<std::ptrdiff_t>(from));
    next.insert(next.begin() + static_cast<std::ptrdiff_t>(to), std::move(e));
    publish(std::move(next));
    return true;
}

void EffectChain::clear() {
    std::lock_guard<std::mutex> lock(m_mutex);
    publish(List());
}

std::size_t EffectChain::size() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_list->size();
}

std::shared_ptr<Effect> EffectChain::at(std::size_t index) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return index < m_list->size() ? (*m_list)[index] : nullptr;
}

void EffectChain::assign(std::vector<std::shared_ptr<Effect>> effects) {
    List next;
    next.reserve(effects.size());
    for (std::shared_ptr<Effect>& e : effects) {
        if (!e) continue;
        if (sample_rate()) e->prepare(sample_rate(), channels());
        next.push_back(std::move(e));
    }
    std::lock_guard<std::mutex> lock(m_mutex);
    publish(std::move(next));
}

std::vector<std::shared_ptr<Effect>> EffectChain::effects() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return *m_list;
}

Gain::Gain(float initial_db) : db("db", initial_db, -120.0f, 48.0f, "dB") {
    add_parameter(db);
}

void Gain::prepare(std::uint32_t sample_rate, std::size_t channels) {
    set_format(sample_rate, channels);
    m_smooth = 1.0f - std::exp(-1.0f / (0.01f * static_cast<float>(sample_rate ? sample_rate : 48000)));
    reset();
}

void Gain::process(float* interleaved, std::size_t frames) noexcept {
    const float target = db.get() <= -120.0f ? 0.0f : db_to_gain(db.get());
    const std::size_t c = channels() ? channels() : 1;
    for (std::size_t f = 0; f < frames; ++f) {
        m_current += (target - m_current) * m_smooth;
        float* p = interleaved + f * c;
        for (std::size_t ch = 0; ch < c; ++ch) p[ch] *= m_current;
    }
}

void Gain::reset() noexcept {
    m_current = db.get() <= -120.0f ? 0.0f : db_to_gain(db.get());
}

void apply(Effect& effect, AudioBuffer& buffer, bool keep_tail) {
    if (buffer.channels() == 0 || buffer.sample_rate() == 0) return;
    effect.prepare(buffer.sample_rate(), buffer.channels());
    effect.reset();
    const std::size_t original = buffer.frames();
    const std::size_t latency = effect.latency_frames();
    const std::size_t tail = keep_tail ? effect.tail_frames() : 0;
    buffer.resize(original + latency + tail);
    const std::size_t total = buffer.frames();
    const std::size_t block = 4096;
    for (std::size_t pos = 0; pos < total; pos += block) {
        effect.process(buffer.frame(pos), std::min(block, total - pos));
    }
    buffer.erase(0, latency);
    if (!keep_tail) buffer.resize(original);
}

AudioBuffer applied(Effect& effect, const AudioBuffer& buffer, bool keep_tail) {
    AudioBuffer out = buffer;
    apply(effect, out, keep_tail);
    return out;
}

} // namespace audio
} // namespace fizmo
