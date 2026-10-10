#ifndef FIZMO_SYSTEM_JOBS_HPP
#define FIZMO_SYSTEM_JOBS_HPP

#include <algorithm>
#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <exception>
#include <functional>
#include <future>
#include <initializer_list>
#include <memory>
#include <mutex>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

namespace fizmo {
namespace system {

class JobSystem;

namespace detail {

struct Job {
    std::function<void()>             work;
    std::atomic<int>                  waiting_on{ 1 };
    std::mutex                        lock;
    std::vector<std::shared_ptr<Job>> continuations;
    std::atomic<bool>                 done{ false };
    std::exception_ptr                error;
};

} // namespace detail

class JobHandle {
private:
    friend class JobSystem;
    std::shared_ptr<detail::Job> m_job;

    explicit JobHandle(std::shared_ptr<detail::Job> job) noexcept : m_job(std::move(job)) {}

public:
    JobHandle() = default;

    bool valid() const noexcept { return static_cast<bool>(m_job); }
    bool done() const noexcept { return !m_job || m_job->done.load(std::memory_order_acquire); }
    bool failed() const noexcept { return m_job && done() && m_job->error; }

    void rethrow() const {
        if (m_job && done() && m_job->error) std::rethrow_exception(m_job->error);
    }
};

class JobSystem {
private:
    struct Worker {
        std::mutex                                lock;
        std::deque<std::shared_ptr<detail::Job>>  queue;
    };

    std::vector<std::unique_ptr<Worker>> m_workers;
    std::vector<std::thread>             m_threads;
    std::mutex                           m_sleep_lock;
    std::condition_variable              m_wake;
    std::condition_variable              m_finished;
    std::atomic<std::size_t>             m_queued{ 0 };
    std::atomic<std::size_t>             m_in_flight{ 0 };
    std::atomic<std::size_t>             m_next{ 0 };
    std::atomic<bool>                    m_stop{ false };

    static int& worker_index() noexcept {
        static thread_local int index = -1;
        return index;
    }

    static const JobSystem*& worker_owner() noexcept {
        static thread_local const JobSystem* owner = nullptr;
        return owner;
    }

    int current_worker() const noexcept { return worker_owner() == this ? worker_index() : -1; }

    void enqueue(std::shared_ptr<detail::Job> job);

    std::shared_ptr<detail::Job> take(int self);

    void finish(const std::shared_ptr<detail::Job>& job);

    void release(std::shared_ptr<detail::Job> job) {
        if (job->waiting_on.fetch_sub(1, std::memory_order_acq_rel) == 1) enqueue(std::move(job));
    }

    void run(const std::shared_ptr<detail::Job>& job);

    void loop(int index);

    JobHandle make(std::function<void()> work, const JobHandle* deps, std::size_t dep_count);

public:
    explicit JobSystem(unsigned int threads = 0);

    ~JobSystem();

    JobSystem(const JobSystem&) = delete;
    JobSystem& operator=(const JobSystem&) = delete;

    static JobSystem& instance() {
        static JobSystem jobs;
        return jobs;
    }

    std::size_t thread_count() const noexcept { return m_threads.size(); }
    std::size_t pending() const noexcept { return m_in_flight.load(std::memory_order_acquire); }
    bool on_worker_thread() const noexcept { return current_worker() >= 0; }

    JobHandle schedule(std::function<void()> work) { return make(std::move(work), nullptr, 0); }
    JobHandle schedule(std::function<void()> work, const JobHandle& after) { return make(std::move(work), &after, 1); }
    JobHandle schedule(std::function<void()> work, std::initializer_list<JobHandle> after) { return make(std::move(work), after.begin(), after.size()); }
    JobHandle schedule(std::function<void()> work, const std::vector<JobHandle>& after) { return make(std::move(work), after.data(), after.size()); }

    JobHandle when_all(const std::vector<JobHandle>& handles) { return make(nullptr, handles.data(), handles.size()); }

    template <typename F, typename R = std::invoke_result_t<std::decay_t<F>>>
    std::future<R> submit(F&& fn) {
        auto task = std::make_shared<std::packaged_task<R()>>(std::forward<F>(fn));
        std::future<R> f = task->get_future();
        schedule([task] { (*task)(); });
        return f;
    }

    bool run_one() {
        auto job = take(current_worker());
        if (!job) return false;
        run(job);
        return true;
    }

    void wait(const JobHandle& handle);

    void wait(std::initializer_list<JobHandle> handles) { for (const JobHandle& h : handles) wait(h); }
    void wait(const std::vector<JobHandle>& handles) { for (const JobHandle& h : handles) wait(h); }

    void wait_all();

    template <typename Index, typename F>
    JobHandle parallel_for_async(Index begin, Index end, Index grain, F&& fn, const JobHandle& after = JobHandle()) {
        if (end <= begin) return when_all({ after });
        const Index total = end - begin;
        const std::size_t lanes = (thread_count() + 1) * 4;
        if (grain <= 0) grain = static_cast<Index>(std::max<std::size_t>(1, static_cast<std::size_t>(total) / lanes));
        struct Shared {
            explicit Shared(std::decay_t<F> f) : fn(std::move(f)) {}
            std::decay_t<F>    fn;
            std::mutex         lock;
            std::exception_ptr error;
        };
        auto shared = std::make_shared<Shared>(std::forward<F>(fn));
        std::vector<JobHandle> parts;

        for (Index b = begin; b < end; b += grain) {
            const Index e = static_cast<Index>(std::min<Index>(end, b + grain));
            parts.push_back(schedule([shared, b, e] {
                try {
                    if constexpr (std::is_invocable_v<std::decay_t<F>&, Index, Index>) shared->fn(b, e);
                    else for (Index i = b; i < e; ++i) shared->fn(i);
                } catch (...) {
                    std::lock_guard<std::mutex> g(shared->lock);
                    if (!shared->error) shared->error = std::current_exception();
                }
            }, after));
            if (e == end) break;
        }

        return schedule([shared] { if (shared->error) std::rethrow_exception(shared->error); }, parts);
    }

    template <typename Index, typename F>
    void parallel_for(Index begin, Index end, Index grain, F&& fn) {
        JobHandle h = parallel_for_async(begin, end, grain, std::forward<F>(fn));
        wait(h);
        h.rethrow();
    }

    template <typename Index, typename F>
    void parallel_for(Index begin, Index end, F&& fn) { parallel_for(begin, end, Index(0), std::forward<F>(fn)); }
};

template <typename Index, typename F>
inline void parallel_for(Index begin, Index end, F&& fn) { JobSystem::instance().parallel_for(begin, end, std::forward<F>(fn)); }

template <typename F>
inline JobHandle schedule(F&& fn) { return JobSystem::instance().schedule(std::forward<F>(fn)); }

} // namespace system
} // namespace fizmo

#endif // FIZMO_SYSTEM_JOBS_HPP
