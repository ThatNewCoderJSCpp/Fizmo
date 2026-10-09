#include "fizmo_library.hpp"
#include "tracker.hpp"

namespace fizmo {
namespace system {
namespace detail {

void FrameAccumulator::add(const windows::FrameStats& s) {
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

auto FrameAccumulator::take(double seconds) -> FrameSummary {
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

void Relay::bind(FrameFn frame, StopFn unbind, StopFn close) {
    std::lock_guard<std::mutex> lk(m_mutex);
    m_frame  = std::move(frame);
    m_unbind = std::move(unbind);
    m_close  = std::move(close);
}

void Relay::reset() noexcept {
    std::lock_guard<std::mutex> lk(m_mutex);
    m_frame  = nullptr;
    m_unbind = nullptr;
    m_close  = nullptr;
}

void Relay::on_frame(const windows::Renderer& r, const windows::FrameStats& s) noexcept {
    std::lock_guard<std::mutex> lk(m_mutex);
    if (!m_frame) return;
    try { m_frame(r, s); } catch (...) {}
}

} // namespace detail
} // namespace system
} // namespace fizmo
