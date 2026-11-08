#ifndef FIZMO_PHYSICS_SOLVER_3D_HPP
#define FIZMO_PHYSICS_SOLVER_3D_HPP

#include "physics_3d.hpp"

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <thread>

namespace fizmo {
namespace physics {

struct SolverBody3D {
    RVec3 v{};
    RVec3 w{};
    RVec3 inv_mass{};
    RMat3 inv_inertia{};
    RVec3 center{};
    RigidBody3D* body = nullptr;
    bool dynamic = false;
    bool contact_impulses = false;
    bool joint_impulses = false;

    void apply(const RVec3& P, const RVec3& L) noexcept {
        v.x += inv_mass.x * P.x;
        v.y += inv_mass.y * P.y;
        v.z += inv_mass.z * P.z;
        w += vec3::mul(inv_inertia, L);
    }

    Real scalar_inv_mass() const noexcept { return std::max({ inv_mass.x, inv_mass.y, inv_mass.z }); }
};

struct SoftParams {
    Real bias_rate = 0.0_r;
    Real mass_scale = 1.0_r;
    Real impulse_scale = 0.0_r;

    static SoftParams make(Real hertz, Real zeta, Real h) noexcept {
        SoftParams p;
        if (hertz <= 0.0_r || h <= 0.0_r) return p;
        const Real omega = 2.0_r * 3.14159265358979323846_r * hertz;
        const Real a1 = 2.0_r * zeta + h * omega;
        const Real a2 = h * omega * a1;
        const Real a3 = 1.0_r / (1.0_r + a2);
        p.bias_rate = omega / a1;
        p.mass_scale = a2 * a3;
        p.impulse_scale = a3;
        return p;
    }
};

namespace detail {

class TaskPool {
public:
    TaskPool() = default;
    ~TaskPool() { stop(); }
    TaskPool(const TaskPool&) = delete;
    TaskPool& operator=(const TaskPool&) = delete;

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

#endif // FIZMO_PHYSICS_SOLVER_3D_HPP
