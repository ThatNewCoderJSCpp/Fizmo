#ifndef FIZMO_EASING_TWEEN_HPP
#define FIZMO_EASING_TWEEN_HPP

#include <cmath>
#include <cstdint>
#include <functional>
#include <algorithm>
#include "../Basic/constants.hpp"

namespace fizmo {

enum class Easing : std::uint8_t {
    Linear = 0,
    InQuad,    OutQuad,    InOutQuad,
    InCubic,   OutCubic,   InOutCubic,
    InQuart,   OutQuart,   InOutQuart,
    InQuint,   OutQuint,   InOutQuint,
    InSine,    OutSine,    InOutSine,
    InExpo,    OutExpo,    InOutExpo,
    InCirc,    OutCirc,    InOutCirc,
    InBack,    OutBack,    InOutBack,
    InElastic, OutElastic, InOutElastic,
    InBounce,  OutBounce,  InOutBounce
};

namespace ease {

namespace detail {
    inline double clamp01(double t) noexcept { return (t < 0.0) ? 0.0 : (t > 1.0) ? 1.0 : t; }

    double bounce_out(double t) noexcept;
} // namespace detail

inline double linear(double t) noexcept { return t; }

inline double in_quad(double t) noexcept    { return t * t; }
inline double out_quad(double t) noexcept   { return t * (2.0 - t); }
inline double in_out_quad(double t) noexcept {
    return (t < 0.5) ? 2.0 * t * t : -1.0 + (4.0 - 2.0 * t) * t;
}

inline double in_cubic(double t) noexcept    { return t * t * t; }
inline double out_cubic(double t) noexcept   { double u = t - 1.0; return u * u * u + 1.0; }
inline double in_out_cubic(double t) noexcept {
    return (t < 0.5) ? 4.0 * t * t * t
                     : 1.0 + (t - 1.0) * (2.0 * t - 2.0) * (2.0 * t - 2.0);
}

inline double in_quart(double t) noexcept    { return t * t * t * t; }
inline double out_quart(double t) noexcept   { double u = t - 1.0; return 1.0 - u * u * u * u; }
inline double in_out_quart(double t) noexcept {
    double u = t - 1.0;
    return (t < 0.5) ? 8.0 * t * t * t * t : 1.0 - 8.0 * u * u * u * u;
}

inline double in_quint(double t) noexcept    { return t * t * t * t * t; }
inline double out_quint(double t) noexcept   { double u = t - 1.0; return 1.0 + u * u * u * u * u; }
inline double in_out_quint(double t) noexcept {
    double u = t - 1.0;
    return (t < 0.5) ? 16.0 * t * t * t * t * t : 1.0 + 16.0 * u * u * u * u * u;
}

inline double in_sine(double t) noexcept    { return 1.0 - std::cos(t * constants::pi() * 0.5); }
inline double out_sine(double t) noexcept   { return std::sin(t * constants::pi() * 0.5); }
inline double in_out_sine(double t) noexcept { return 0.5 * (1.0 - std::cos(constants::pi() * t)); }

inline double in_expo(double t) noexcept {
    return (t <= 0.0) ? 0.0 : std::pow(2.0, 10.0 * (t - 1.0));
}
inline double out_expo(double t) noexcept {
    return (t >= 1.0) ? 1.0 : 1.0 - std::pow(2.0, -10.0 * t);
}
double in_out_expo(double t) noexcept;

inline double in_circ(double t) noexcept    { return 1.0 - std::sqrt(1.0 - t * t); }
inline double out_circ(double t) noexcept   { double u = t - 1.0; return std::sqrt(1.0 - u * u); }
double in_out_circ(double t) noexcept;

inline double in_back(double t) noexcept {
    constexpr double s = 1.70158;
    return t * t * ((s + 1.0) * t - s);
}
inline double out_back(double t) noexcept {
    constexpr double s = 1.70158;
    double u = t - 1.0;
    return u * u * ((s + 1.0) * u + s) + 1.0;
}
double in_out_back(double t) noexcept;

double in_elastic(double t) noexcept;
double out_elastic(double t) noexcept;
double in_out_elastic(double t) noexcept;

inline double out_bounce(double t) noexcept { return detail::bounce_out(t); }
inline double in_bounce(double t) noexcept  { return 1.0 - detail::bounce_out(1.0 - t); }
double in_out_bounce(double t) noexcept;

} // namespace ease

std::function<double(double)> easing_from(Easing e);

enum class TweenState : std::uint8_t {
    Idle = 0,
    Running,
    Paused,
    Finished
};

class Tween {
private:
    double       m_from      = 0.0;
    double       m_to        = 0.0;
    double       m_duration  = 1.0;   // seconds
    double       m_elapsed   = 0.0;
    double       m_value     = 0.0;   // current interpolated value
    double       m_delay     = 0.0;   // pre-start delay
    double       m_delay_rem = 0.0;   // remaining delay

    int          m_repeat    = 0;     // 0 = play once, n = repeat n extra times, -1 = infinite
    int          m_played    = 0;     // completed iterations so far
    bool         m_yoyo      = false; // reverse direction on each repeat
    bool         m_forward   = true;  // current direction (yoyo state)

    std::function<double(double)> m_ease;
    TweenState                    m_state = TweenState::Idle;

    std::function<void(double)> m_on_update;
    std::function<void()>       m_on_complete;
    std::function<void()>       m_on_repeat;      

public:
    Tween() noexcept : m_ease(ease::linear) {}

    Tween(double from, double to, double duration, Easing easing = Easing::Linear) noexcept
        : m_from(from), m_to(to), m_duration(duration), m_value(from),
          m_ease(easing_from(easing)) {}

    Tween(double from, double to, double duration, std::function<double(double)> fn) noexcept
        : m_from(from), m_to(to), m_duration(duration), m_value(from),
          m_ease(std::move(fn)) {}

public:
    Tween& set_from(double v) noexcept                  { m_from = v;                         return *this; }
    Tween& set_to(double v) noexcept                    { m_to = v;                           return *this; }
    Tween& set_duration(double s) noexcept              { m_duration = (s > 0.0) ? s : 0.001; return *this; }
    Tween& set_easing(Easing e)                         { m_ease = easing_from(e);            return *this; }
    Tween& set_easing(std::function<double(double)> fn) { m_ease = std::move(fn);             return *this; }
    Tween& set_delay(double s) noexcept                 { m_delay = (s > 0.0) ? s : 0.0;      return *this; }
    Tween& set_repeat(int n) noexcept                   { m_repeat = n;                       return *this; } // -1 = infinite
    Tween& set_yoyo(bool y) noexcept                    { m_yoyo = y;                         return *this; }

    Tween& on_update(std::function<void(double)> cb) { m_on_update = std::move(cb);   return *this; }
    Tween& on_complete(std::function<void()> cb)     { m_on_complete = std::move(cb); return *this; }
    Tween& on_repeat(std::function<void()> cb)       { m_on_repeat = std::move(cb);   return *this; }

    double      value()       const noexcept { return m_value; }
    double      progress()    const noexcept { return (m_duration > 0.0) ? m_elapsed / m_duration : 1.0; }
    TweenState  state()       const noexcept { return m_state; }
    bool        is_running()  const noexcept { return m_state == TweenState::Running; }
    bool        is_finished() const noexcept { return m_state == TweenState::Finished; }

    void start() noexcept;

    void pause()  noexcept { if (m_state == TweenState::Running) m_state = TweenState::Paused; }
    void resume() noexcept { if (m_state == TweenState::Paused) m_state = TweenState::Running; }

    void stop() noexcept {
        m_state   = TweenState::Idle;
        m_elapsed = 0.0;
        m_value   = m_from;
    }

    void finish() noexcept;

    void update(double dt) noexcept;

private:
    void apply(double t) noexcept;

    void emit_update() {
        if (m_on_update) m_on_update(m_value);
    }
};

class TweenManager {
private:
    std::vector<Tween> m_tweens;

public:
    Tween& add(Tween t) {
        t.start();
        m_tweens.push_back(std::move(t));
        return m_tweens.back();
    }

    void update(double dt);

    void clear() { m_tweens.clear(); }
    std::size_t active_count() const noexcept { return m_tweens.size(); }
    bool empty() const noexcept { return m_tweens.empty(); }
};

} // namespace fizmo

#endif // FIZMO_EASING_TWEEN_HPP