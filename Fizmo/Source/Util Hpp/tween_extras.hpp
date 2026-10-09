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
graphics::Color tween_lerp<graphics::Color>(const graphics::Color& a, const graphics::Color& b, double t);

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

    void begin_step();

public:
    TweenSequence& then(Tween t);
    TweenSequence& with(std::vector<Tween> ts);
    TweenSequence& wait(double seconds);
    TweenSequence& call(std::function<void()> fn);
    TweenSequence& set_loops(int loops) noexcept { m_loops = loops; return *this; }
    TweenSequence& on_complete(std::function<void()> fn) { m_on_complete = std::move(fn); return *this; }

    void start() { m_index = 0; m_played = 0; m_started = true; m_running = true; begin_step(); }
    void stop() noexcept { m_running = false; }
    bool running() const noexcept { return m_running; }
    bool finished() const noexcept { return m_started && !m_running; }
    std::size_t step() const noexcept { return m_index; }

    void update(double dt);
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
    Handle after(double seconds, std::function<void()> fn);

    Handle every(double seconds, std::function<void()> fn, int times = -1);

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

    std::size_t update(double dt);
};

} // namespace fizmo

#endif // FIZMO_TWEEN_EXTRAS_HPP
