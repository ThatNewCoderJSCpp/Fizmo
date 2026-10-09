#include "fizmo_library.hpp"
#include "tween_extras.hpp"

namespace fizmo {

template <>
inline graphics::Color tween_lerp<graphics::Color>(const graphics::Color& a, const graphics::Color& b, double t) {
    auto m = [t](std::uint8_t x, std::uint8_t y) { return static_cast<std::uint8_t>(std::max(0.0, std::min(255.0, std::round(x + (static_cast<double>(y) - x) * t)))); };
    return graphics::Color(m(a.red(), b.red()), m(a.green(), b.green()), m(a.blue(), b.blue()), m(a.alpha(), b.alpha()));
}

void TweenSequence::begin_step() {
    if (m_index >= m_steps.size()) return;
    Step& s = m_steps[m_index];
    m_waited = 0.0;
    for (Tween& t : s.tweens) { t.stop(); t.start(); }
}

auto TweenSequence::then(Tween t) -> TweenSequence& { Step s; s.kind = Step::Kind::Tween; s.tweens.push_back(std::move(t)); m_steps.push_back(std::move(s)); return *this; }

auto TweenSequence::with(std::vector<Tween> ts) -> TweenSequence& { Step s; s.kind = Step::Kind::Parallel; s.tweens = std::move(ts); m_steps.push_back(std::move(s)); return *this; }

auto TweenSequence::wait(double seconds) -> TweenSequence& { Step s; s.kind = Step::Kind::Wait; s.wait = seconds; m_steps.push_back(std::move(s)); return *this; }

auto TweenSequence::call(std::function<void()> fn) -> TweenSequence& { Step s; s.kind = Step::Kind::Call; s.call = std::move(fn); m_steps.push_back(std::move(s)); return *this; }

void TweenSequence::update(double dt) {
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

auto GameScheduler::after(double seconds, std::function<void()> fn) -> Handle {
    m_tasks.push_back({ m_next, m_time + std::max(0.0, seconds), 0.0, 1, std::move(fn), false });
    return m_next++;
}

auto GameScheduler::every(double seconds, std::function<void()> fn, int times) -> Handle {
    const double iv = std::max(1e-6, seconds);
    m_tasks.push_back({ m_next, m_time + iv, iv, times, std::move(fn), false });
    return m_next++;
}

std::size_t GameScheduler::update(double dt) {
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

} // namespace fizmo
