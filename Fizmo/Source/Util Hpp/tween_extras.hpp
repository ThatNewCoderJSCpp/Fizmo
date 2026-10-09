#ifndef FIZMO_TWEEN_EXTRAS_HPP
#define FIZMO_TWEEN_EXTRAS_HPP

#include "ease_tween.hpp"
#include "../Graphics/color.hpp"
#include "../Vectors/vectors.hpp"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <functional>
#include <memory>
#include <type_traits>
#include <vector>

namespace fizmo {

template <typename T>
inline T tween_lerp(const T& a, const T& b, double t) {
    if constexpr (std::is_arithmetic<T>::value) return static_cast<T>(a + (b - a) * t);
    else return a + (b - a) * t;
}

template <>
inline graphics::Color tween_lerp<graphics::Color>(const graphics::Color& a, const graphics::Color& b, double t) {
    auto m = [t](std::uint8_t x, std::uint8_t y) { return static_cast<std::uint8_t>(std::max(0.0, std::min(255.0, std::round(x + (static_cast<double>(y) - x) * t)))); };
    return graphics::Color(m(a.red(), b.red()), m(a.green(), b.green()), m(a.blue(), b.blue()), m(a.alpha(), b.alpha()));
}

template <typename T>
inline Tween tween_to(T& target, const T& to, double duration, Easing easing = Easing::Linear) {
    auto from = std::make_shared<T>(target);
    T* ptr = &target;
    Tween tw(0.0, 1.0, duration, easing);
    tw.on_update([ptr, from, to](double t) { *ptr = tween_lerp(*from, to, t); });
    return tw;
}

template <typename T>
inline Tween tween_between(std::function<void(const T&)> apply, const T& from, const T& to, double duration, Easing easing = Easing::Linear) {
    Tween tw(0.0, 1.0, duration, easing);
    tw.on_update([apply = std::move(apply), from, to](double t) { apply(tween_lerp(from, to, t)); });
    return tw;
}

class TweenSequence {
private:
    struct Step {
        enum class Kind : std::uint8_t { Tween = 0, Wait, Call, Parallel } kind = Kind::Wait;
        std::vector<Tween>    tweens;
        double                wait = 0.0;
        std::function<void()> call;
    };

    std::vector<Step> m_steps;
    std::size_t       m_index = 0;
    double            m_waited = 0.0;
    bool              m_started = false;
    bool              m_running = false;
    int               m_loops = 0;
    int               m_played = 0;
    std::function<void()> m_on_complete;

    void begin_step() {
        if (m_index >= m_steps.size()) return;
        Step& s = m_steps[m_index];
        m_waited = 0.0;
        for (Tween& t : s.tweens) { t.stop(); t.start(); }
    }

public:
    TweenSequence& then(Tween t) { Step s; s.kind = Step::Kind::Tween; s.tweens.push_back(std::move(t)); m_steps.push_back(std::move(s)); return *this; }
    TweenSequence& with(std::vector<Tween> ts) { Step s; s.kind = Step::Kind::Parallel; s.tweens = std::move(ts); m_steps.push_back(std::move(s)); return *this; }
    TweenSequence& wait(double seconds) { Step s; s.kind = Step::Kind::Wait; s.wait = seconds; m_steps.push_back(std::move(s)); return *this; }
    TweenSequence& call(std::function<void()> fn) { Step s; s.kind = Step::Kind::Call; s.call = std::move(fn); m_steps.push_back(std::move(s)); return *this; }
    TweenSequence& set_loops(int loops) noexcept { m_loops = loops; return *this; }
    TweenSequence& on_complete(std::function<void()> fn) { m_on_complete = std::move(fn); return *this; }

    void start() { m_index = 0; m_played = 0; m_started = true; m_running = true; begin_step(); }
    void stop() noexcept { m_running = false; }
    bool running() const noexcept { return m_running; }
    bool finished() const noexcept { return m_started && !m_running; }
    std::size_t step() const noexcept { return m_index; }

    void update(double dt) {
        if (!m_running) return;
        int guard = 0;

        while (m_running && guard++ < 1024) {
            if (m_index >= m_steps.size()) {
                if (m_loops == -1 || m_played < m_loops) { ++m_played; m_index = 0; begin_step(); if (m_steps.empty()) { m_running = false; break; } continue; }
                m_running = false;
                if (m_on_complete) m_on_complete();
                break;
            }

            Step& s = m_steps[m_index];
            if (s.kind == Step::Kind::Call) { if (s.call) s.call(); ++m_index; begin_step(); continue; }

            if (s.kind == Step::Kind::Wait) {
                m_waited += dt;
                if (m_waited < s.wait) break;
                dt = m_waited - s.wait;
                ++m_index;
                begin_step();
                continue;
            }

            bool done = true;
            for (Tween& t : s.tweens) { t.update(dt); if (!t.is_finished()) done = false; }
            if (!done) break;
            dt = 0.0;
            ++m_index;
            begin_step();
        }
    }
};

class GameScheduler {
public:
    using Handle = std::uint64_t;

private:
    struct Task {
        Handle                id = 0;
        double                due = 0.0;
        double                interval = 0.0;
        int                   remaining = 1;
        std::function<void()> fn;
        bool                  cancelled = false;
    };

    std::vector<Task> m_tasks;
    double            m_time = 0.0;
    double            m_scale = 1.0;
    bool              m_paused = false;
    Handle            m_next = 1;

public:
    Handle after(double seconds, std::function<void()> fn) {
        m_tasks.push_back({ m_next, m_time + std::max(0.0, seconds), 0.0, 1, std::move(fn), false });
        return m_next++;
    }

    Handle every(double seconds, std::function<void()> fn, int times = -1) {
        const double iv = std::max(1e-6, seconds);
        m_tasks.push_back({ m_next, m_time + iv, iv, times, std::move(fn), false });
        return m_next++;
    }

    bool cancel(Handle h) noexcept {
        for (Task& t : m_tasks) if (t.id == h && !t.cancelled) { t.cancelled = true; return true; }
        return false;
    }

    void clear() noexcept { m_tasks.clear(); }
    void set_time_scale(double s) noexcept { m_scale = std::max(0.0, s); }
    double time_scale() const noexcept { return m_scale; }
    void pause() noexcept { m_paused = true; }
    void resume() noexcept { m_paused = false; }
    bool paused() const noexcept { return m_paused; }
    double time() const noexcept { return m_time; }

    std::size_t pending() const noexcept {
        std::size_t n = 0;
        for (const Task& t : m_tasks) if (!t.cancelled) ++n;
        return n;
    }

    std::size_t update(double dt) {
        if (m_paused) return 0;
        m_time += dt * m_scale;
        std::size_t fired = 0;

        for (std::size_t i = 0; i < m_tasks.size(); ++i) {
            while (!m_tasks[i].cancelled && m_tasks[i].due <= m_time + 1e-9) {
                std::function<void()> fn = m_tasks[i].fn;
                if (m_tasks[i].remaining > 0) --m_tasks[i].remaining;
                const bool last = m_tasks[i].remaining == 0 || m_tasks[i].interval <= 0.0;
                if (last) m_tasks[i].cancelled = true;
                else m_tasks[i].due += m_tasks[i].interval;
                if (fn) fn();
                ++fired;
                if (last) break;
            }
        }

        m_tasks.erase(std::remove_if(m_tasks.begin(), m_tasks.end(), [](const Task& t) { return t.cancelled; }), m_tasks.end());
        return fired;
    }
};

} // namespace fizmo

#endif // FIZMO_TWEEN_EXTRAS_HPP
