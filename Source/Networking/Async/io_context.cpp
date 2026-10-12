#include "fizmo_library.hpp"
#include "io_context.hpp"
#include "reactor.hpp"

#include <climits>

namespace fizmo {
namespace networking {
namespace async {

namespace {

struct ContextScope {
    const IOContext* ctx;
    ContextScope*    prev;
};

thread_local ContextScope* t_scope = nullptr;

struct ScopeGuard {
    ContextScope scope;

    explicit ScopeGuard(const IOContext* c) noexcept : scope{ c, t_scope } { t_scope = &scope; }
    ~ScopeGuard() { t_scope = scope.prev; }
};

} // namespace

IOContext::IOContext() : m_outstanding(0), m_timer_ids(0) {
    m_reactor.reset(new detail::Reactor(*this));
    Op task;
    task.is_task = true;
    m_queue.push_back(std::move(task));
}

IOContext::~IOContext() {
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_shutting_down = true;
        m_stopped = true;
    }

    drain_on_shutdown();
    m_reactor.reset();
}

void IOContext::drain_on_shutdown() {
    for (;;) {
        std::deque<Op> queue;
        TimerQueue     timers;

        {
            std::lock_guard<std::mutex> lock(m_mutex);
            queue.swap(m_queue);
            timers.swap(m_timers);
            m_timers_by_id.clear();
        }

        const bool had_work = queue.size() > 0 || !timers.empty();
        queue.clear();
        timers.clear();
        const std::size_t ops = m_reactor->shutdown();
        if (!had_work && ops == 0) break;
    }
}

bool IOContext::running_in_this_thread() const noexcept {
    for (ContextScope* s = t_scope; s; s = s->prev) {
        if (s->ctx == this) return true;
    }

    return false;
}

std::size_t IOContext::run() {
    ScopeGuard guard(this);
    std::unique_lock<std::mutex> lock(m_mutex);
    std::size_t n = 0;

    while (do_run_one(lock, true, nullptr)) {
        if (n != static_cast<std::size_t>(-1)) ++n;
        lock.lock();
    }

    return n;
}

std::size_t IOContext::run_one() {
    ScopeGuard guard(this);
    std::unique_lock<std::mutex> lock(m_mutex);
    return do_run_one(lock, true, nullptr);
}

std::size_t IOContext::poll() {
    ScopeGuard guard(this);
    std::unique_lock<std::mutex> lock(m_mutex);
    std::size_t n = 0;

    while (do_run_one(lock, false, nullptr)) {
        if (n != static_cast<std::size_t>(-1)) ++n;
        lock.lock();
    }

    return n;
}

std::size_t IOContext::poll_one() {
    ScopeGuard guard(this);
    std::unique_lock<std::mutex> lock(m_mutex);
    return do_run_one(lock, false, nullptr);
}

std::size_t IOContext::run_until(time_point deadline) {
    ScopeGuard guard(this);
    std::unique_lock<std::mutex> lock(m_mutex);
    std::size_t n = 0;

    while (do_run_one(lock, true, &deadline)) {
        if (n != static_cast<std::size_t>(-1)) ++n;
        lock.lock();
    }

    return n;
}

std::size_t IOContext::run_one_until(time_point deadline) {
    ScopeGuard guard(this);
    std::unique_lock<std::mutex> lock(m_mutex);
    return do_run_one(lock, true, &deadline);
}

void IOContext::stop() {
    std::lock_guard<std::mutex> lock(m_mutex);
    stop_locked();
}

void IOContext::stop_locked() {
    m_stopped = true;
    m_cv.notify_all();

    if (m_task_running && !m_task_interrupted) {
        m_task_interrupted = true;
        m_reactor->interrupt();
    }
}

bool IOContext::stopped() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_stopped;
}

void IOContext::restart() {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_shutting_down) m_stopped = false;
}

void IOContext::work_finished() {
    if (--m_outstanding == 0) stop();
}

void IOContext::enqueue(UniqueFunction<void()> fn) {
    std::unique_lock<std::mutex> lock(m_mutex);
    Op op;
    op.fn = std::move(fn);
    m_queue.push_back(std::move(op));
    wake_one(lock);
}

void IOContext::post_completed(UniqueFunction<void()> fn) {
    enqueue(std::move(fn));
}

void IOContext::post_completed_batch(std::vector<UniqueFunction<void()>>& fns) {
    if (fns.empty()) return;
    std::unique_lock<std::mutex> lock(m_mutex);

    for (auto& f : fns) {
        Op op;
        op.fn = std::move(f);
        m_queue.push_back(std::move(op));
    }

    fns.clear();
    if (m_idle_threads > 0) m_cv.notify_all();
    else wake_one(lock);
}

void IOContext::wake_one(std::unique_lock<std::mutex>&) {
    if (m_idle_threads > 0) {
        m_cv.notify_one();
        return;
    }

    if (m_task_running && !m_task_interrupted) {
        m_task_interrupted = true;
        m_reactor->interrupt();
    }
}

int IOContext::compute_timeout_ms(bool block, const time_point* deadline) const {
    if (!block) return 0;
    bool bounded = false;
    time_point limit = time_point::max();

    if (!m_timers.empty()) {
        limit = m_timers.begin()->first;
        bounded = true;
    }

    if (deadline && (!bounded || *deadline < limit)) {
        limit = *deadline;
        bounded = true;
    }

    if (!bounded) return -1;
    const auto now = clock::now();
    if (limit <= now) return 0;
    const auto us = std::chrono::duration_cast<std::chrono::microseconds>(limit - now).count();
    const long long ms = (us + 999) / 1000;
    return ms > 300000 ? 300000 : static_cast<int>(ms);
}

void IOContext::collect_expired_timers(std::vector<UniqueFunction<void()>>& out) {
    if (m_timers.empty()) return;
    const auto now = clock::now();

    while (!m_timers.empty() && m_timers.begin()->first <= now) {
        auto it = m_timers.begin();
        auto found = m_timers_by_id.find(it->second.timer_id);

        if (found != m_timers_by_id.end()) {
            auto& vec = found->second;

            for (std::size_t i = 0; i < vec.size(); ++i) {
                if (vec[i] == it) {
                    vec.erase(vec.begin() + static_cast<std::ptrdiff_t>(i));
                    break;
                }
            }

            if (vec.empty()) m_timers_by_id.erase(found);
        }

        out.push_back([h = std::move(it->second.handler)]() mutable { h(std::error_code()); });
        m_timers.erase(it);
    }
}

void IOContext::schedule_timer(std::uint64_t timer_id, time_point when, UniqueFunction<void(std::error_code)> handler) {
    work_started();
    std::unique_lock<std::mutex> lock(m_mutex);
    TimerWait wait;
    wait.timer_id = timer_id;
    wait.handler = std::move(handler);
    auto it = m_timers.emplace(when, std::move(wait));
    m_timers_by_id[timer_id].push_back(it);

    if (it == m_timers.begin() && m_task_running && !m_task_interrupted) {
        m_task_interrupted = true;
        m_reactor->interrupt();
    }
}

std::size_t IOContext::cancel_timer(std::uint64_t timer_id, std::size_t max_count) {
    std::unique_lock<std::mutex> lock(m_mutex);
    auto found = m_timers_by_id.find(timer_id);
    if (found == m_timers_by_id.end()) return 0;
    auto& vec = found->second;
    std::size_t n = 0;

    while (!vec.empty() && n < max_count) {
        auto it = vec.front();
        vec.erase(vec.begin());
        Op op;
        op.fn = [h = std::move(it->second.handler)]() mutable { h(make_error_code(error::operation_aborted)); };
        m_queue.push_back(std::move(op));
        m_timers.erase(it);
        ++n;
    }

    if (vec.empty()) m_timers_by_id.erase(found);
    if (n > 0) wake_one(lock);
    return n;
}

std::size_t IOContext::pending_timer_waits(std::uint64_t timer_id) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto found = m_timers_by_id.find(timer_id);
    return found == m_timers_by_id.end() ? 0 : found->second.size();
}

std::size_t IOContext::do_run_one(std::unique_lock<std::mutex>& lock, bool block, const time_point* deadline) {
    if (m_outstanding.load() == 0) {
        stop_locked();
        return 0;
    }

    while (!m_stopped) {
        if (deadline && clock::now() >= *deadline) return 0;

        if (!m_queue.empty()) {
            Op op = std::move(m_queue.front());
            m_queue.pop_front();

            if (op.is_task) {
                const bool more = !m_queue.empty();
                const int timeout = more ? 0 : compute_timeout_ms(block, deadline);
                m_task_interrupted = more;
                m_task_running = true;
                lock.unlock();
                std::vector<UniqueFunction<void()>> ready;
                m_reactor->run(timeout, ready);
                lock.lock();
                m_task_running = false;
                collect_expired_timers(ready);

                for (auto& f : ready) {
                    Op r;
                    r.fn = std::move(f);
                    m_queue.push_back(std::move(r));
                }

                Op task;
                task.is_task = true;
                m_queue.push_back(std::move(task));
                if (!ready.empty() && m_idle_threads > 0) m_cv.notify_all();
                if (!block && ready.empty() && !more) return 0;
                continue;
            }

            if (!m_queue.empty() && m_idle_threads > 0) m_cv.notify_one();
            lock.unlock();

            struct Finish {
                IOContext* ctx;
                ~Finish() { ctx->work_finished(); }
            } finish{ this };

            op.fn();
            return 1;
        }

        if (!block) return 0;
        ++m_idle_threads;

        if (deadline) {
            m_cv.wait_until(lock, *deadline);
        } else {
            m_cv.wait(lock);
        }

        --m_idle_threads;
    }

    return 0;
}

} // namespace async
} // namespace networking
} // namespace fizmo
