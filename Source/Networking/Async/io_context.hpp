#ifndef FIZMO_ASYNC_IO_CONTEXT_HPP
#define FIZMO_ASYNC_IO_CONTEXT_HPP

#include "function.hpp"
#include "error.hpp"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <map>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <vector>

namespace fizmo {
namespace networking {
namespace async {

namespace detail {
class Reactor;
} // namespace detail

class IOContext {
public:
    using clock      = std::chrono::steady_clock;
    using time_point = clock::time_point;
    using duration   = clock::duration;

    IOContext();
    ~IOContext();

    IOContext(const IOContext&) = delete;
    IOContext& operator=(const IOContext&) = delete;

public:
    std::size_t run();
    std::size_t run_one();
    std::size_t poll();
    std::size_t poll_one();

    template <typename Rep, typename Period>
    std::size_t run_for(const std::chrono::duration<Rep, Period>& d) {
        return run_until(clock::now() + std::chrono::duration_cast<duration>(d));
    }

    std::size_t run_until(time_point deadline);

    template <typename Rep, typename Period>
    std::size_t run_one_for(const std::chrono::duration<Rep, Period>& d) {
        return run_one_until(clock::now() + std::chrono::duration_cast<duration>(d));
    }

    std::size_t run_one_until(time_point deadline);

    void stop();
    bool stopped() const;
    void restart();

    bool running_in_this_thread() const noexcept;

    template <typename F>
    void post(F&& f) {
        work_started();
        enqueue(UniqueFunction<void()>(std::forward<F>(f)));
    }

    template <typename F>
    void dispatch(F&& f) {
        if (running_in_this_thread()) {
            f();
            return;
        }

        post(std::forward<F>(f));
    }

    template <typename F>
    void defer(F&& f) { post(std::forward<F>(f)); }

    std::size_t outstanding_work() const noexcept { return m_outstanding.load(); }

public:
    void work_started() noexcept { ++m_outstanding; }
    void work_finished();
    void post_completed(UniqueFunction<void()> fn);
    void post_completed_batch(std::vector<UniqueFunction<void()>>& fns);

    detail::Reactor& reactor() noexcept { return *m_reactor; }

    std::uint64_t new_timer_id() noexcept { return ++m_timer_ids; }
    void schedule_timer(std::uint64_t timer_id, time_point when, UniqueFunction<void(std::error_code)> handler);
    std::size_t cancel_timer(std::uint64_t timer_id, std::size_t max_count = static_cast<std::size_t>(-1));
    std::size_t pending_timer_waits(std::uint64_t timer_id) const;

private:
    struct TimerWait {
        std::uint64_t                         timer_id;
        UniqueFunction<void(std::error_code)> handler;
    };

    using TimerQueue = std::multimap<time_point, TimerWait>;

    struct Op {
        UniqueFunction<void()> fn;
        bool                   is_task = false;
    };

    mutable std::mutex                m_mutex;
    std::condition_variable           m_cv;
    std::deque<Op>                    m_queue;
    std::atomic<std::size_t>          m_outstanding;
    std::atomic<std::uint64_t>        m_timer_ids;
    std::unique_ptr<detail::Reactor>  m_reactor;
    TimerQueue                        m_timers;
    std::unordered_map<std::uint64_t, std::vector<TimerQueue::iterator>> m_timers_by_id;
    std::size_t                       m_idle_threads = 0;
    bool                              m_stopped = false;
    bool                              m_task_running = false;
    bool                              m_task_interrupted = false;
    bool                              m_shutting_down = false;

    void enqueue(UniqueFunction<void()> fn);
    void wake_one(std::unique_lock<std::mutex>& lock);
    void stop_locked();
    std::size_t do_run_one(std::unique_lock<std::mutex>& lock, bool block, const time_point* deadline);
    int compute_timeout_ms(bool block, const time_point* deadline) const;
    void collect_expired_timers(std::vector<UniqueFunction<void()>>& out);
    void drain_on_shutdown();

    friend class WorkGuard;
};

class WorkGuard {
public:
    explicit WorkGuard(IOContext& ctx) noexcept : m_ctx(&ctx) { ctx.work_started(); }
    WorkGuard(const WorkGuard& o) noexcept : m_ctx(o.m_ctx) { if (m_ctx) m_ctx->work_started(); }
    WorkGuard(WorkGuard&& o) noexcept : m_ctx(o.m_ctx) { o.m_ctx = nullptr; }
    WorkGuard& operator=(const WorkGuard&) = delete;
    WorkGuard& operator=(WorkGuard&&) = delete;
    ~WorkGuard() { reset(); }

    void reset() {
        if (m_ctx) {
            IOContext* c = m_ctx;
            m_ctx = nullptr;
            c->work_finished();
        }
    }

    bool owns_work() const noexcept { return m_ctx != nullptr; }
    IOContext& context() const noexcept { return *m_ctx; }

private:
    IOContext* m_ctx;
};

inline WorkGuard make_work_guard(IOContext& ctx) { return WorkGuard(ctx); }

template <typename F>
void post(IOContext& ctx, F&& f) { ctx.post(std::forward<F>(f)); }

template <typename F>
void dispatch(IOContext& ctx, F&& f) { ctx.dispatch(std::forward<F>(f)); }

} // namespace async
} // namespace networking
} // namespace fizmo

#endif // FIZMO_ASYNC_IO_CONTEXT_HPP
