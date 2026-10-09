#ifndef FIZMO_SYSTEM_TRACKER_HPP
#define FIZMO_SYSTEM_TRACKER_HPP

#include "common.hpp"
#include "../Windows/game_loop.hpp"
#include <condition_variable>
#include <deque>
#include <thread>

namespace fizmo {
namespace system {
namespace detail {

class FrameAccumulator {
private:
    static constexpr std::size_t kMaxSamples = 16384;

    std::uint64_t       m_frames      = 0;
    std::vector<double> m_intervals;
    double              m_cpu_sum     = 0.0;
    double              m_cpu_max     = 0.0;
    double              m_present_sum = 0.0;
    double              m_present_max = 0.0;
    std::uint64_t       m_gpu_count   = 0;
    double              m_gpu_sum     = 0.0;
    double              m_gpu_max     = 0.0;

public:
    void add(const windows::FrameStats& s) {
        ++m_frames;
        if (s.interval_ms > 0.0 && m_intervals.size() < kMaxSamples) m_intervals.push_back(s.interval_ms);
        m_cpu_sum += s.cpu_ms;
        m_cpu_max = std::max(m_cpu_max, s.cpu_ms);
        m_present_sum += s.present_ms;
        m_present_max = std::max(m_present_max, s.present_ms);

        if (s.gpu_valid) {
            ++m_gpu_count;
            m_gpu_sum += s.gpu_ms;
            m_gpu_max = std::max(m_gpu_max, s.gpu_ms);
        }
    }

    FrameSummary take(double seconds) {
        FrameSummary f;
        f.frames = m_frames;

        if (m_frames > 0) {
            const double n = static_cast<double>(m_frames);
            f.cpu_ms         = m_cpu_sum / n;
            f.cpu_ms_max     = m_cpu_max;
            f.present_ms     = m_present_sum / n;
            f.present_ms_max = m_present_max;
        }

        if (!m_intervals.empty()) {
            double sum = 0.0;
            for (double v : m_intervals) sum += v;
            f.frame_ms = sum / static_cast<double>(m_intervals.size());
            std::sort(m_intervals.begin(), m_intervals.end());
            f.frame_ms_min = m_intervals.front();
            f.frame_ms_max = m_intervals.back();
            const std::size_t idx = static_cast<std::size_t>(std::ceil(0.99 * static_cast<double>(m_intervals.size()))) - 1;
            f.frame_ms_p99 = m_intervals[std::min(idx, m_intervals.size() - 1)];
            f.low_1_percent_fps = f.frame_ms_p99 > 0.0 ? 1000.0 / f.frame_ms_p99 : 0.0;
            f.fps = seconds > 0.0 ? static_cast<double>(m_intervals.size()) / seconds : (f.frame_ms > 0.0 ? 1000.0 / f.frame_ms : 0.0);
        }

        if (m_gpu_count > 0) {
            f.gpu_valid  = true;
            f.gpu_ms     = m_gpu_sum / static_cast<double>(m_gpu_count);
            f.gpu_ms_max = m_gpu_max;
        }

        *this = FrameAccumulator();
        return f;
    }
};

class Relay : public windows::FrameObserver {
public:
    using FrameFn = std::function<void(const windows::Renderer&, const windows::FrameStats&)>;
    using StopFn  = std::function<void()>;

private:
    std::mutex m_mutex;
    FrameFn    m_frame;
    StopFn     m_unbind;
    StopFn     m_close;

public:
    void bind(FrameFn frame, StopFn unbind, StopFn close) {
        std::lock_guard<std::mutex> lk(m_mutex);
        m_frame  = std::move(frame);
        m_unbind = std::move(unbind);
        m_close  = std::move(close);
    }

    void reset() noexcept {
        std::lock_guard<std::mutex> lk(m_mutex);
        m_frame  = nullptr;
        m_unbind = nullptr;
        m_close  = nullptr;
    }

    void on_frame(const windows::Renderer& r, const windows::FrameStats& s) noexcept override {
        std::lock_guard<std::mutex> lk(m_mutex);
        if (!m_frame) return;
        try { m_frame(r, s); } catch (...) {}
    }

    void on_unbind(const windows::Renderer&) noexcept override {
        std::lock_guard<std::mutex> lk(m_mutex);
        if (!m_unbind) return;
        try { m_unbind(); } catch (...) {}
    }

    void on_close() noexcept {
        std::lock_guard<std::mutex> lk(m_mutex);
        if (!m_close) return;
        try { m_close(); } catch (...) {}
    }
};

template <typename Derived, typename Sample>
class Tracker {
public:
    using sample_type = Sample;
    using Listener    = std::function<void(const Sample&)>;

private:
    using ListenerPtr = std::shared_ptr<Listener>;

    mutable std::mutex      m_control;
    mutable std::mutex      m_wait_mutex;
    std::condition_variable m_wake;
    std::thread             m_thread;
    bool                    m_stop = true;

    std::mutex m_collect_mutex;

    mutable std::mutex m_data;
    std::deque<Sample> m_history;
    std::size_t        m_capacity;
    Sample             m_latest{};
    bool               m_has_latest = false;
    std::uint64_t      m_count      = 0;
    std::vector<std::pair<std::uint64_t, ListenerPtr>> m_listeners;
    std::uint64_t      m_next_listener = 1;

    std::atomic<std::int64_t> m_interval_ms;
    Clock::time_point         m_epoch = Clock::now();

    std::mutex             m_frame_mutex;
    FrameAccumulator       m_frames;
    Clock::time_point      m_frames_since = Clock::now();
    std::shared_ptr<Relay> m_relay;

    Derived&       self()       noexcept { return static_cast<Derived&>(*this); }
    const Derived& self() const noexcept { return static_cast<const Derived&>(*this); }

    void run() {
        std::unique_lock<std::mutex> lk(m_wait_mutex);
        Clock::time_point next = Clock::now();

        while (!m_stop) {
            lk.unlock();
            collect_and_publish();
            lk.lock();
            const auto period = std::chrono::milliseconds(std::max<std::int64_t>(10, m_interval_ms.load()));
            next += period;
            const Clock::time_point now = Clock::now();
            if (next <= now) next = now + period;
            m_wake.wait_until(lk, next, [this] { return m_stop; });
        }
    }

    Sample collect_and_publish() {
        Sample s;
        {
            std::lock_guard<std::mutex> lk(m_collect_mutex);
            const Clock::time_point t0 = Clock::now();
            try { s = self().collect(); } catch (...) { s = Sample{}; }
            const Clock::time_point t1 = Clock::now();
            s.time       = seconds_between(m_epoch, t0);
            s.collect_ms = seconds_between(t0, t1) * 1000.0;
        }

        std::vector<ListenerPtr> listeners;
        {
            std::lock_guard<std::mutex> lk(m_data);
            m_latest = s;
            m_has_latest = true;
            ++m_count;
            if (m_capacity > 0) {
                m_history.push_back(s);
                while (m_history.size() > m_capacity) m_history.pop_front();
            }
            listeners.reserve(m_listeners.size());
            for (const auto& l : m_listeners) listeners.push_back(l.second);
        }

        for (const auto& l : listeners) {
            try { (*l)(s); } catch (...) {}
        }

        return s;
    }

    void ensure_relay() {
        if (m_relay) return;
        m_relay = std::make_shared<Relay>();
        m_relay->bind(
            [this](const windows::Renderer& r, const windows::FrameStats& s) {
                {
                    std::lock_guard<std::mutex> lk(m_frame_mutex);
                    m_frames.add(s);
                }
                self().frame_hook(r, s);
            },
            [this] { stop(); },
            [this] { stop(); });
    }

protected:
    explicit Tracker(std::chrono::milliseconds interval, std::size_t history)
        : m_capacity(history), m_interval_ms(static_cast<std::int64_t>(interval.count())) {}

    ~Tracker() { shutdown(); }

    void shutdown() noexcept {
        if (m_relay) m_relay->reset();
        stop();
    }

    FrameSummary take_frames() {
        std::lock_guard<std::mutex> lk(m_frame_mutex);
        const Clock::time_point now = Clock::now();
        const double seconds = seconds_between(m_frames_since, now);
        m_frames_since = now;
        return m_frames.take(seconds);
    }

    void frame_hook(const windows::Renderer&, const windows::FrameStats&) {}
    void renderer_attached(windows::Renderer&) {}

public:
    Tracker(const Tracker&) = delete;
    Tracker& operator=(const Tracker&) = delete;

    bool start() {
        std::lock_guard<std::mutex> lk(m_control);
        if (m_thread.joinable()) {
            std::lock_guard<std::mutex> w(m_wait_mutex);
            if (!m_stop) return true;
        }
        if (m_thread.joinable()) {
            if (m_thread.get_id() == std::this_thread::get_id()) return false;
            m_thread.join();
        }
        {
            std::lock_guard<std::mutex> w(m_wait_mutex);
            m_stop = false;
        }
        {
            std::lock_guard<std::mutex> f(m_frame_mutex);
            m_frames = FrameAccumulator();
            m_frames_since = Clock::now();
        }

        try {
            m_thread = std::thread([this] { run(); });
        } catch (...) {
            std::lock_guard<std::mutex> w(m_wait_mutex);
            m_stop = true;
            return false;
        }

        return true;
    }

    void stop() noexcept {
        std::thread worker;
        {
            std::lock_guard<std::mutex> lk(m_control);
            {
                std::lock_guard<std::mutex> w(m_wait_mutex);
                m_stop = true;
            }
            m_wake.notify_all();
            if (!m_thread.joinable()) return;
            if (m_thread.get_id() == std::this_thread::get_id()) { m_thread.detach(); return; }
            worker = std::move(m_thread);
        }
        worker.join();
    }

    bool running() const {
        std::lock_guard<std::mutex> lk(m_control);
        std::lock_guard<std::mutex> w(m_wait_mutex);
        return m_thread.joinable() && !m_stop;
    }

    void set_interval(std::chrono::milliseconds interval) noexcept {
        m_interval_ms.store(std::max<std::int64_t>(10, static_cast<std::int64_t>(interval.count())));
        m_wake.notify_all();
    }

    std::chrono::milliseconds interval() const noexcept { return std::chrono::milliseconds(m_interval_ms.load()); }

    void set_history_size(std::size_t samples) {
        std::lock_guard<std::mutex> lk(m_data);
        m_capacity = samples;
        while (m_history.size() > m_capacity) m_history.pop_front();
    }

    std::size_t history_size() const {
        std::lock_guard<std::mutex> lk(m_data);
        return m_capacity;
    }

    void clear_history() {
        std::lock_guard<std::mutex> lk(m_data);
        m_history.clear();
    }

    bool has_sample() const {
        std::lock_guard<std::mutex> lk(m_data);
        return m_has_latest;
    }

    Sample latest() const {
        std::lock_guard<std::mutex> lk(m_data);
        return m_latest;
    }

    std::vector<Sample> history() const {
        std::lock_guard<std::mutex> lk(m_data);
        return std::vector<Sample>(m_history.begin(), m_history.end());
    }

    std::uint64_t sample_count() const {
        std::lock_guard<std::mutex> lk(m_data);
        return m_count;
    }

    Sample sample_now() { return collect_and_publish(); }

    double uptime_seconds() const noexcept { return seconds_between(m_epoch, Clock::now()); }

    std::uint64_t add_listener(Listener listener) {
        if (!listener) return 0;
        std::lock_guard<std::mutex> lk(m_data);
        const std::uint64_t id = m_next_listener++;
        m_listeners.emplace_back(id, std::make_shared<Listener>(std::move(listener)));
        return id;
    }

    bool remove_listener(std::uint64_t id) {
        std::lock_guard<std::mutex> lk(m_data);
        for (auto it = m_listeners.begin(); it != m_listeners.end(); ++it) {
            if (it->first == id) { m_listeners.erase(it); return true; }
        }
        return false;
    }

    void clear_listeners() {
        std::lock_guard<std::mutex> lk(m_data);
        m_listeners.clear();
    }

    template <typename F, typename = decltype(std::declval<F>()(std::declval<const Sample&>()))>
    Statistics stats(F&& select) const {
        std::vector<double> values;
        {
            std::lock_guard<std::mutex> lk(m_data);
            values.reserve(m_history.size());
            for (const Sample& s : m_history) push_value(values, select(s));
        }
        return summarize(std::move(values));
    }

    template <typename T>
    Statistics stats(T Sample::*member) const {
        return stats([member](const Sample& s) -> const T& { return s.*member; });
    }

    void attach(windows::Window& window) {
        ensure_relay();
        std::weak_ptr<Relay> weak = m_relay;
        window.add_event_listener(windows::WindowEventType::WindowClose, [weak](const windows::WindowEvent&) {
            if (const auto r = weak.lock()) r->on_close();
        });
        start();
    }

    void attach(windows::Renderer& renderer) {
        ensure_relay();
        renderer.add_frame_observer(m_relay);
        self().renderer_attached(renderer);
        start();
    }

    void attach(windows::Application& app) {
        attach(app.window());
        attach(app.renderer());
    }

    void attach(windows::GameLoop& loop) { attach(loop.app()); }

    void detach() noexcept {
        if (!m_relay) return;
        m_relay->reset();
        m_relay.reset();
    }

    bool attached() const noexcept { return m_relay != nullptr; }
};

} // namespace detail
} // namespace system
} // namespace fizmo

#endif // FIZMO_SYSTEM_TRACKER_HPP
