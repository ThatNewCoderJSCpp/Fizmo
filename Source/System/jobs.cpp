#include "fizmo_library.hpp"
#include "jobs.hpp"

namespace fizmo {
namespace system {

void JobSystem::enqueue(std::shared_ptr<detail::Job> job) {
    const int self = current_worker();
    const std::size_t target = self >= 0 ? static_cast<std::size_t>(self) : m_next.fetch_add(1, std::memory_order_relaxed) % m_workers.size();

    {
        std::lock_guard<std::mutex> g(m_workers[target]->lock);
        m_workers[target]->queue.push_back(std::move(job));
    }

    m_queued.fetch_add(1, std::memory_order_release);
    { std::lock_guard<std::mutex> g(m_sleep_lock); }
    m_wake.notify_one();
}

auto JobSystem::take(int self) -> std::shared_ptr<detail::Job> {
    if (m_queued.load(std::memory_order_acquire) == 0) return nullptr;
    const std::size_t n = m_workers.size();

    if (self >= 0) {
        Worker& w = *m_workers[static_cast<std::size_t>(self)];
        std::lock_guard<std::mutex> g(w.lock);
        if (!w.queue.empty()) {
            auto job = std::move(w.queue.back());
            w.queue.pop_back();
            m_queued.fetch_sub(1, std::memory_order_relaxed);
            return job;
        }
    }

    const std::size_t start = self >= 0 ? static_cast<std::size_t>(self) + 1 : m_next.load(std::memory_order_relaxed);
    for (std::size_t i = 0; i < n; ++i) {
        Worker& w = *m_workers[(start + i) % n];
        std::lock_guard<std::mutex> g(w.lock);
        if (w.queue.empty()) continue;
        auto job = std::move(w.queue.front());
        w.queue.pop_front();
        m_queued.fetch_sub(1, std::memory_order_relaxed);
        return job;
    }

    return nullptr;
}

void JobSystem::finish(const std::shared_ptr<detail::Job>& job) {
    std::vector<std::shared_ptr<detail::Job>> next;

    {
        std::lock_guard<std::mutex> g(job->lock);
        job->done.store(true, std::memory_order_release);
        next.swap(job->continuations);
    }

    job->work = nullptr;
    for (auto& c : next) release(std::move(c));
    m_in_flight.fetch_sub(1, std::memory_order_acq_rel);
    { std::lock_guard<std::mutex> g(m_sleep_lock); }
    m_finished.notify_all();
}

void JobSystem::run(const std::shared_ptr<detail::Job>& job) {
    try {
        if (job->work) job->work();
    } catch (...) {
        job->error = std::current_exception();
    }
    finish(job);
}

void JobSystem::loop(int index) {
    worker_index() = index;
    worker_owner() = this;

    while (!m_stop.load(std::memory_order_acquire)) {
        auto job = take(index);
        if (job) { run(job); continue; }
        std::unique_lock<std::mutex> lk(m_sleep_lock);
        m_wake.wait(lk, [&] { return m_stop.load(std::memory_order_acquire) || m_queued.load(std::memory_order_acquire) > 0; });
    }

    worker_owner() = nullptr;
    worker_index() = -1;
}

auto JobSystem::make(std::function<void()> work, const JobHandle* deps, std::size_t dep_count) -> JobHandle {
    auto job = std::make_shared<detail::Job>();
    job->work = std::move(work);
    m_in_flight.fetch_add(1, std::memory_order_acq_rel);

    for (std::size_t i = 0; i < dep_count; ++i) {
        const auto& d = deps[i].m_job;
        if (!d) continue;
        std::lock_guard<std::mutex> g(d->lock);
        if (d->done.load(std::memory_order_acquire)) continue;
        job->waiting_on.fetch_add(1, std::memory_order_relaxed);
        d->continuations.push_back(job);
    }

    JobHandle h(job);
    release(std::move(job));
    return h;
}

JobSystem::JobSystem(unsigned int threads) {
    if (threads == 0) {
        const unsigned int hw = std::thread::hardware_concurrency();
        threads = hw > 1 ? hw - 1 : 1;
    }
    for (unsigned int i = 0; i < threads; ++i) m_workers.push_back(std::make_unique<Worker>());
    for (unsigned int i = 0; i < threads; ++i) m_threads.emplace_back([this, i] { loop(static_cast<int>(i)); });
}

JobSystem::~JobSystem() {
    wait_all();
    m_stop.store(true, std::memory_order_release);
    { std::lock_guard<std::mutex> g(m_sleep_lock); }
    m_wake.notify_all();
    for (std::thread& t : m_threads) if (t.joinable()) t.join();
}

void JobSystem::wait(const JobHandle& handle) {
    while (!handle.done()) {
        if (run_one()) continue;
        std::unique_lock<std::mutex> lk(m_sleep_lock);
        m_finished.wait_for(lk, std::chrono::milliseconds(1), [&] { return handle.done() || m_queued.load(std::memory_order_acquire) > 0; });
    }
}

void JobSystem::wait_all() {
    while (m_in_flight.load(std::memory_order_acquire) > 0) {
        if (run_one()) continue;
        std::unique_lock<std::mutex> lk(m_sleep_lock);
        m_finished.wait_for(lk, std::chrono::milliseconds(1), [&] { return m_in_flight.load(std::memory_order_acquire) == 0 || m_queued.load(std::memory_order_acquire) > 0; });
    }
}

} // namespace system
} // namespace fizmo
