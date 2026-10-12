#include "fizmo_library.hpp"
#include "loop.hpp"
#include "../Async/timer.hpp"

#include <cstdio>
#include <mutex>
#include <thread>
#include <vector>

namespace fizmo {
namespace networking {

namespace {

struct Runtime {
    async::IOContext                          ctx;
    std::mutex                                resolver_mutex;
    std::unique_ptr<async::Resolver>          resolver;
    std::mutex                                bg_mutex;
    std::vector<std::thread>                  threads;
    std::unique_ptr<async::WorkGuard>         guard;
    std::mutex                                error_mutex;
    std::function<void(const std::string&, const std::error_code&)> error_handler;

    ~Runtime() {
        stop_threads();
        std::lock_guard<std::mutex> lk(resolver_mutex);
        resolver.reset();
    }

    void stop_threads() {
        std::vector<std::thread> joining;

        {
            std::lock_guard<std::mutex> lk(bg_mutex);
            if (guard) guard.reset();
            if (threads.empty()) return;
            ctx.stop();
            joining.swap(threads);
        }

        for (auto& t : joining) {
            if (t.get_id() == std::this_thread::get_id()) t.detach();
            else if (t.joinable()) t.join();
        }
    }
};

Runtime& runtime() {
    static Runtime rt;
    return rt;
}

void prepare(async::IOContext& c) {
    if (c.stopped()) c.restart();
}

} // namespace

async::IOContext& loop() { return runtime().ctx; }

std::size_t run() {
    auto& c = loop();
    prepare(c);
    return c.run();
}

std::size_t run_once() {
    auto& c = loop();
    prepare(c);
    return c.run_one();
}

std::size_t poll() {
    auto& c = loop();
    prepare(c);
    return c.poll();
}

std::size_t run_for(std::chrono::milliseconds duration) {
    auto& c = loop();
    prepare(c);
    return c.run_for(duration);
}

void stop() { loop().stop(); }

bool in_loop_thread() noexcept { return loop().running_in_this_thread(); }

void start_background(std::size_t threads) {
    Runtime& rt = runtime();
    std::lock_guard<std::mutex> lk(rt.bg_mutex);
    if (!rt.threads.empty()) return;
    if (threads == 0) threads = 1;
    prepare(rt.ctx);
    rt.guard.reset(new async::WorkGuard(rt.ctx));

    for (std::size_t i = 0; i < threads; ++i) {
        rt.threads.emplace_back([&rt]() { rt.ctx.run(); });
    }
}

void stop_background() { runtime().stop_threads(); }

bool background_running() noexcept {
    Runtime& rt = runtime();
    std::lock_guard<std::mutex> lk(rt.bg_mutex);
    return !rt.threads.empty();
}

void next_tick(std::function<void()> fn) {
    if (!fn) return;
    loop().post([f = std::move(fn)]() { f(); });
}

void set_unhandled_error_handler(std::function<void(const std::string&, const std::error_code&)> handler) {
    Runtime& rt = runtime();
    std::lock_guard<std::mutex> lk(rt.error_mutex);
    rt.error_handler = std::move(handler);
}

void report_unhandled_error(const std::string& source, const std::error_code& ec) {
    Runtime& rt = runtime();
    std::function<void(const std::string&, const std::error_code&)> h;

    {
        std::lock_guard<std::mutex> lk(rt.error_mutex);
        h = rt.error_handler;
    }

    if (h) {
        h(source, ec);
        return;
    }

    std::fprintf(stderr, "fizmo::networking: unhandled error from %s: %s\n", source.c_str(), ec.message().c_str());
}

namespace detail {

ListenerId next_listener_id() noexcept {
    static std::atomic<ListenerId> ids{0};
    return ++ids;
}

async::Resolver& default_resolver() {
    Runtime& rt = runtime();
    std::lock_guard<std::mutex> lk(rt.resolver_mutex);
    if (!rt.resolver) rt.resolver.reset(new async::Resolver(rt.ctx));
    return *rt.resolver;
}

} // namespace detail

struct Timer::State : std::enable_shared_from_this<Timer::State> {
    std::mutex                m;
    async::SteadyTimer        timer;
    std::function<void()>     fn;
    std::chrono::milliseconds interval{0};
    bool                      repeat = false;
    std::atomic<bool>         active{true};
    std::uint64_t             generation = 0;

    explicit State(async::IOContext& ctx) : timer(ctx) {}

    void arm_locked(async::SteadyTimer::time_point when) {
        const std::uint64_t gen = ++generation;
        timer.expires_at(when);
        auto self = shared_from_this();
        timer.async_wait([self, gen](std::error_code ec) { self->fire(ec, gen); });
    }

    void fire(std::error_code ec, std::uint64_t gen) {
        std::function<void()> f;

        {
            std::lock_guard<std::mutex> lk(m);
            if (ec || !active || gen != generation) return;
            f = fn;
            if (!repeat) { active = false; fn = nullptr; }
        }

        if (f) f();
        if (!repeat) return;
        std::lock_guard<std::mutex> lk(m);
        if (!active || gen != generation) return;
        auto next = timer.expiry() + std::chrono::duration_cast<async::SteadyTimer::duration>(interval);
        const auto now = async::SteadyTimer::clock::now();
        if (next < now) next = now;
        arm_locked(next);
    }
};

void Timer::cancel() const {
    if (!m_state) return;
    std::function<void()> doomed;
    std::lock_guard<std::mutex> lk(m_state->m);
    m_state->active = false;
    ++m_state->generation;
    m_state->timer.cancel();
    doomed.swap(m_state->fn);
}

Timer& Timer::refresh() {
    if (!m_state) return *this;
    std::lock_guard<std::mutex> lk(m_state->m);
    if (!m_state->active || !m_state->fn) return *this;
    m_state->arm_locked(async::SteadyTimer::clock::now() + std::chrono::duration_cast<async::SteadyTimer::duration>(m_state->interval));
    return *this;
}

bool Timer::active() const noexcept { return m_state && m_state->active.load(); }

bool Timer::repeating() const noexcept { return m_state && m_state->repeat; }

std::chrono::milliseconds Timer::delay() const noexcept { return m_state ? m_state->interval : std::chrono::milliseconds(0); }

namespace {

Timer make_timer(std::function<void()> fn, std::chrono::milliseconds delay, bool repeat) {
    if (!fn) return Timer();
    if (delay.count() < 0) delay = std::chrono::milliseconds(0);
    if (repeat && delay.count() == 0) delay = std::chrono::milliseconds(1);
    auto st = std::make_shared<Timer::State>(loop());
    st->fn = std::move(fn);
    st->interval = delay;
    st->repeat = repeat;
    std::lock_guard<std::mutex> lk(st->m);
    st->arm_locked(async::SteadyTimer::clock::now() + std::chrono::duration_cast<async::SteadyTimer::duration>(delay));
    return Timer(st);
}

} // namespace

Timer set_timeout(std::function<void()> fn, std::chrono::milliseconds delay) { return make_timer(std::move(fn), delay, false); }

Timer set_interval(std::function<void()> fn, std::chrono::milliseconds interval) { return make_timer(std::move(fn), interval, true); }

Timer set_immediate(std::function<void()> fn) { return make_timer(std::move(fn), std::chrono::milliseconds(0), false); }

} // namespace networking
} // namespace fizmo
