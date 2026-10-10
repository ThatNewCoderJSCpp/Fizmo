#include "fizmo_library.hpp"
#include "ease_tween.hpp"

namespace fizmo {
namespace ease {
namespace detail {

double bounce_out(double t) noexcept {
    if (t < 1.0 / 2.75) {
        return 7.5625 * t * t;
    } else if (t < 2.0 / 2.75) {
        t -= 1.5 / 2.75;
        return 7.5625 * t * t + 0.75;
    } else if (t < 2.5 / 2.75) {
        t -= 2.25 / 2.75;
        return 7.5625 * t * t + 0.9375;
    } else {
        t -= 2.625 / 2.75;
        return 7.5625 * t * t + 0.984375;
    }
}

} // namespace detail
} // namespace ease
} // namespace fizmo

namespace fizmo {
namespace ease {

double in_out_expo(double t) noexcept {
    if (t <= 0.0) return 0.0;
    if (t >= 1.0) return 1.0;
    return (t < 0.5) ? 0.5 * std::pow(2.0, 20.0 * t - 10.0)
                     : 1.0 - 0.5 * std::pow(2.0, -20.0 * t + 10.0);
}

double in_out_circ(double t) noexcept {
    if (t < 0.5) return 0.5 * (1.0 - std::sqrt(1.0 - 4.0 * t * t));
    double u = 2.0 * t - 2.0;
    return 0.5 * (std::sqrt(1.0 - u * u) + 1.0);
}

double in_out_back(double t) noexcept {
    constexpr double s = 1.70158 * 1.525;
    if (t < 0.5) {
        double u = 2.0 * t;
        return 0.5 * (u * u * ((s + 1.0) * u - s));
    }
    double u = 2.0 * t - 2.0;
    return 0.5 * (u * u * ((s + 1.0) * u + s) + 2.0);
}

double in_elastic(double t) noexcept {
    if (t <= 0.0 || t >= 1.0) return t;
    return -std::pow(2.0, 10.0 * t - 10.0)
           * std::sin((t * 10.0 - 10.75) * (2.0 * constants::pi() / 3.0));
}

double out_elastic(double t) noexcept {
    if (t <= 0.0 || t >= 1.0) return t;
    return std::pow(2.0, -10.0 * t)
           * std::sin((t * 10.0 - 0.75) * (2.0 * constants::pi() / 3.0)) + 1.0;
}

double in_out_elastic(double t) noexcept {
    if (t <= 0.0 || t >= 1.0) return t;
    constexpr double c = (2.0 * constants::pi()) / 4.5;
    if (t < 0.5)
        return -0.5 * std::pow(2.0,  20.0 * t - 10.0) * std::sin((20.0 * t - 11.125) * c);
    return  0.5 * std::pow(2.0, -20.0 * t + 10.0) * std::sin((20.0 * t - 11.125) * c) + 1.0;
}

double in_out_bounce(double t) noexcept {
    return (t < 0.5) ? 0.5 * (1.0 - detail::bounce_out(1.0 - 2.0 * t))
                     : 0.5 * detail::bounce_out(2.0 * t - 1.0) + 0.5;
}

} // namespace ease
} // namespace fizmo

namespace fizmo {

std::function<double(double)> easing_from(Easing e) {
    switch (e) {
        case Easing::Linear:       return ease::linear;
        case Easing::InQuad:       return ease::in_quad;
        case Easing::OutQuad:      return ease::out_quad;
        case Easing::InOutQuad:    return ease::in_out_quad;
        case Easing::InCubic:      return ease::in_cubic;
        case Easing::OutCubic:     return ease::out_cubic;
        case Easing::InOutCubic:   return ease::in_out_cubic;
        case Easing::InQuart:      return ease::in_quart;
        case Easing::OutQuart:     return ease::out_quart;
        case Easing::InOutQuart:   return ease::in_out_quart;
        case Easing::InQuint:      return ease::in_quint;
        case Easing::OutQuint:     return ease::out_quint;
        case Easing::InOutQuint:   return ease::in_out_quint;
        case Easing::InSine:       return ease::in_sine;
        case Easing::OutSine:      return ease::out_sine;
        case Easing::InOutSine:    return ease::in_out_sine;
        case Easing::InExpo:       return ease::in_expo;
        case Easing::OutExpo:      return ease::out_expo;
        case Easing::InOutExpo:    return ease::in_out_expo;
        case Easing::InCirc:       return ease::in_circ;
        case Easing::OutCirc:      return ease::out_circ;
        case Easing::InOutCirc:    return ease::in_out_circ;
        case Easing::InBack:       return ease::in_back;
        case Easing::OutBack:      return ease::out_back;
        case Easing::InOutBack:    return ease::in_out_back;
        case Easing::InElastic:    return ease::in_elastic;
        case Easing::OutElastic:   return ease::out_elastic;
        case Easing::InOutElastic: return ease::in_out_elastic;
        case Easing::InBounce:     return ease::in_bounce;
        case Easing::OutBounce:    return ease::out_bounce;
        case Easing::InOutBounce:  return ease::in_out_bounce;
        default:                   return ease::linear;
    }
}

void Tween::start() noexcept {
    m_elapsed   = 0.0;
    m_played    = 0;
    m_forward   = true;
    m_delay_rem = m_delay;
    m_value     = m_from;
    m_state     = TweenState::Running;
}

void Tween::finish() noexcept {
    m_elapsed = m_duration;
    m_value   = m_to;
    m_state   = TweenState::Finished;
    emit_update();
    if (m_on_complete) m_on_complete();
}

void Tween::update(double dt) noexcept {
    if (m_state != TweenState::Running) return;

    if (m_delay_rem > 0.0) {
        m_delay_rem -= dt;
        if (m_delay_rem > 0.0) return;
        dt = -m_delay_rem;  
        m_delay_rem = 0.0;
    }

    m_elapsed += dt;

    if (m_elapsed >= m_duration) {
        m_elapsed = m_duration;
        apply(1.0);
        emit_update();

        if (m_repeat == -1 || m_played < m_repeat) {
            ++m_played;
            if (m_on_repeat) m_on_repeat();
            if (m_yoyo) m_forward = !m_forward;
            m_elapsed   = 0.0;
            m_delay_rem = m_delay;
            return;
        }

        m_state = TweenState::Finished;
        if (m_on_complete) m_on_complete();
        return;
    }

    double t = m_elapsed / m_duration;
    apply(t);
    emit_update();
}

void Tween::apply(double t) noexcept {
    double eased = m_ease ? m_ease(t) : t;

    if (m_forward) {
        m_value = m_from + (m_to - m_from) * eased;
    } else {
        m_value = m_to + (m_from - m_to) * eased;   // reversed for yoyo
    }
}

void TweenManager::update(double dt) {
    for (auto& tw : m_tweens) tw.update(dt);
        
    m_tweens.erase(
        std::remove_if(m_tweens.begin(), m_tweens.end(), [](const Tween& tw) { return tw.is_finished(); }),
        m_tweens.end()
    );
}

} // namespace fizmo
