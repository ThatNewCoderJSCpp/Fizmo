#ifndef FIZMO_PHYSICS_FLUID_POOL_3D_HPP
#define FIZMO_PHYSICS_FLUID_POOL_3D_HPP

#include "../Rigid Body/common.hpp"

#include <atomic>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <thread>
#include <vector>

namespace fizmo {
namespace physics {
namespace detail {

class FluidTaskPool {
public:
    FluidTaskPool() = default;
    ~FluidTaskPool() { stop(); }
    FluidTaskPool(const FluidTaskPool&) = delete;
    FluidTaskPool& operator=(const FluidTaskPool&) = delete;

    void resize(int threads) {
        threads = std::max(1, threads);
        if (threads == static_cast<int>(m_workers.size()) + 1) return;
        stop();
        m_stop = false;
        for (int i = 1; i < threads; ++i) m_workers.emplace_back([this] { worker(); });
    }

    int size() const noexcept { return static_cast<int>(m_workers.size()) + 1; }

    template <class Fn>
    void parallel_for(std::size_t count, Fn&& fn) {
        if (count == 0) return;
        if (m_workers.empty() || count == 1) {
            for (std::size_t i = 0; i < count; ++i) fn(i);
            return;
        }
        std::function<void(std::size_t)> job = std::forward<Fn>(fn);
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_job = &job;
            m_count = count;
            m_next.store(0);
            m_done.store(0);
            ++m_generation;
        }
        m_wake.notify_all();
        run_items(job);
        std::unique_lock<std::mutex> lock(m_mutex);
        m_finished.wait(lock, [&] { return m_done.load() >= m_count && m_active == 0; });
        m_job = nullptr;
    }

    template <class Fn>
    void parallel_range(std::size_t count, std::size_t block, Fn&& fn) {
        if (count == 0) return;
        block = std::max<std::size_t>(1, block);
        const std::size_t blocks = (count + block - 1) / block;
        parallel_for(blocks, [&](std::size_t b) {
            const std::size_t begin = b * block;
            const std::size_t end = std::min(count, begin + block);
            fn(begin, end);
        });
    }

private:
    void run_items(std::function<void(std::size_t)>& job) {
        for (;;) {
            const std::size_t i = m_next.fetch_add(1);
            if (i >= m_count) break;
            job(i);
            if (m_done.fetch_add(1) + 1 == m_count) {
                std::lock_guard<std::mutex> lock(m_mutex);
                m_finished.notify_all();
            }
        }
    }

    void worker() {
        std::uint64_t seen = 0;
        for (;;) {
            std::function<void(std::size_t)>* job = nullptr;
            {
                std::unique_lock<std::mutex> lock(m_mutex);
                m_wake.wait(lock, [&] { return m_stop || (m_job && m_generation != seen); });
                if (m_stop) return;
                seen = m_generation;
                job = m_job;
                ++m_active;
            }
            if (job) run_items(*job);
            {
                std::lock_guard<std::mutex> lock(m_mutex);
                --m_active;
            }
            m_finished.notify_all();
        }
    }

    void stop() {
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_stop = true;
        }
        m_wake.notify_all();
        for (std::thread& t : m_workers) t.join();
        m_workers.clear();
    }

    std::vector<std::thread> m_workers;
    std::mutex m_mutex;
    std::condition_variable m_wake;
    std::condition_variable m_finished;
    std::function<void(std::size_t)>* m_job = nullptr;
    std::size_t m_count = 0;
    std::atomic<std::size_t> m_next{ 0 };
    std::atomic<std::size_t> m_done{ 0 };
    std::uint64_t m_generation = 0;
    int m_active = 0;
    bool m_stop = false;
};

} // namespace detail
} // namespace physics
} // namespace fizmo

#endif // FIZMO_PHYSICS_FLUID_POOL_3D_HPP
