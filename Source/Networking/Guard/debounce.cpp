#include "fizmo_library.hpp"
#include "debounce.hpp"
#include "../Node/loop.hpp"

namespace fizmo {
namespace networking {
namespace guard {

struct Debouncer::State : std::enable_shared_from_this<Debouncer::State> {
    std::mutex            m;
    std::function<void()> fn;
    DebounceOptions       options;
    Timer                 wait_timer;
    Timer                 max_timer;
    bool                  in_burst = false;
    bool                  pending = false;

    void invoke() {
        std::function<void()> f;

        {
            std::lock_guard<std::mutex> lk(m);
            f = fn;
        }

        if (f) f();
    }

    void arm_max() {
        if (options.max_wait.count() <= 0) return;
        std::weak_ptr<State> weak = shared_from_this();
        max_timer = set_timeout([weak] { if (auto s = weak.lock()) s->on_max(); }, options.max_wait);
    }

    void on_wait() {
        bool run = false;

        {
            std::lock_guard<std::mutex> lk(m);
            if (!in_burst) return;
            in_burst = false;
            max_timer.cancel();
            run = pending && options.trailing;
            pending = false;
        }

        if (run) invoke();
    }

    void on_max() {
        bool run = false;

        {
            std::lock_guard<std::mutex> lk(m);
            if (!in_burst) return;
            run = pending;
            pending = false;
            arm_max();
        }

        if (run) invoke();
    }
};

Debouncer::Debouncer(std::function<void()> fn, DebounceOptions options) : m_state(std::make_shared<State>()) {
    m_state->fn = std::move(fn);
    m_state->options = options;
}

Debouncer::~Debouncer() {
    if (m_state) cancel();
}

void Debouncer::call() {
    if (!m_state) return;
    State& s = *m_state;
    bool run_now = false;

    {
        std::lock_guard<std::mutex> lk(s.m);

        if (!s.in_burst) {
            s.in_burst = true;
            run_now = s.options.leading;
            s.pending = !run_now;
            s.arm_max();
        } else {
            s.pending = true;
        }

        s.wait_timer.cancel();
        std::weak_ptr<State> weak = m_state;
        s.wait_timer = set_timeout([weak] { if (auto st = weak.lock()) st->on_wait(); }, s.options.wait);
    }

    if (run_now) s.invoke();
}

void Debouncer::cancel() {
    if (!m_state) return;
    std::lock_guard<std::mutex> lk(m_state->m);
    m_state->wait_timer.cancel();
    m_state->max_timer.cancel();
    m_state->in_burst = false;
    m_state->pending = false;
}

void Debouncer::flush() {
    if (!m_state) return;
    bool run = false;

    {
        std::lock_guard<std::mutex> lk(m_state->m);
        m_state->wait_timer.cancel();
        m_state->max_timer.cancel();
        run = m_state->pending;
        m_state->pending = false;
        m_state->in_burst = false;
    }

    if (run) m_state->invoke();
}

bool Debouncer::pending() const {
    if (!m_state) return false;
    std::lock_guard<std::mutex> lk(m_state->m);
    return m_state->pending;
}

bool Debouncer::active() const {
    if (!m_state) return false;
    std::lock_guard<std::mutex> lk(m_state->m);
    return m_state->in_burst;
}

struct Throttler::State {
    std::mutex                m;
    std::function<void()>     fn;
    std::chrono::milliseconds interval{0};
    bool                      trailing = true;
    bool                      ran = false;
    SteadyTime                last_run{};
    Timer                     timer;
    bool                      queued = false;

    void invoke() {
        std::function<void()> f;

        {
            std::lock_guard<std::mutex> lk(m);
            f = fn;
        }

        if (f) f();
    }
};

Throttler::Throttler(std::function<void()> fn, std::chrono::milliseconds interval, bool trailing) : m_state(std::make_shared<State>()) {
    m_state->fn = std::move(fn);
    m_state->interval = interval;
    m_state->trailing = trailing;
}

Throttler::~Throttler() {
    if (m_state) cancel();
}

void Throttler::call() {
    if (!m_state) return;
    State& s = *m_state;
    bool run_now = false;

    {
        std::lock_guard<std::mutex> lk(s.m);
        const SteadyTime now = SteadyClock::now();

        if (!s.ran || now - s.last_run >= s.interval) {
            if (!s.queued) {
                s.ran = true;
                s.last_run = now;
                run_now = true;
            }
        } else if (s.trailing && !s.queued) {
            s.queued = true;
            const auto delay = std::chrono::duration_cast<std::chrono::milliseconds>(s.last_run + s.interval - now);
            std::weak_ptr<State> weak = m_state;

            s.timer = set_timeout([weak] {
                auto st = weak.lock();
                if (!st) return;

                {
                    std::lock_guard<std::mutex> l(st->m);
                    if (!st->queued) return;
                    st->queued = false;
                    st->last_run = SteadyClock::now();
                }

                st->invoke();
            }, delay.count() > 0 ? delay : std::chrono::milliseconds(1));
        }
    }

    if (run_now) s.invoke();
}

void Throttler::cancel() {
    if (!m_state) return;
    std::lock_guard<std::mutex> lk(m_state->m);
    m_state->timer.cancel();
    m_state->queued = false;
}

bool Throttler::pending() const {
    if (!m_state) return false;
    std::lock_guard<std::mutex> lk(m_state->m);
    return m_state->queued;
}

KeyedDebouncer::KeyedDebouncer(Callback fn, DebounceOptions options) : m_shared(std::make_shared<Shared>()) {
    m_shared->fn = std::move(fn);
    m_shared->options = options;
}

void KeyedDebouncer::call(const std::string& key) {
    std::shared_ptr<Debouncer> d;

    {
        std::lock_guard<std::mutex> lk(m_shared->m);
        auto& slot = m_shared->items[key];

        if (!slot) {
            std::weak_ptr<Shared> weak = m_shared;

            slot = std::make_shared<Debouncer>([weak, key] {
                auto sh = weak.lock();
                if (!sh) return;
                Callback fn;
                std::shared_ptr<Debouncer> keep;

                {
                    std::lock_guard<std::mutex> l(sh->m);
                    fn = sh->fn;
                    const auto it = sh->items.find(key);

                    if (it != sh->items.end() && !it->second->active()) {
                        keep = it->second;
                        sh->items.erase(it);
                    }
                }

                if (fn) fn(key);
            }, m_shared->options);
        }

        d = slot;
    }

    d->call();
}

void KeyedDebouncer::cancel(const std::string& key) {
    std::shared_ptr<Debouncer> d;

    {
        std::lock_guard<std::mutex> lk(m_shared->m);
        const auto it = m_shared->items.find(key);
        if (it == m_shared->items.end()) return;
        d = it->second;
        m_shared->items.erase(it);
    }

    d->cancel();
}

void KeyedDebouncer::cancel_all() {
    std::unordered_map<std::string, std::shared_ptr<Debouncer>> items;

    {
        std::lock_guard<std::mutex> lk(m_shared->m);
        items.swap(m_shared->items);
    }

    for (auto& kv : items) kv.second->cancel();
}

void KeyedDebouncer::flush(const std::string& key) {
    std::shared_ptr<Debouncer> d;

    {
        std::lock_guard<std::mutex> lk(m_shared->m);
        const auto it = m_shared->items.find(key);
        if (it == m_shared->items.end()) return;
        d = it->second;
    }

    d->flush();
}

std::size_t KeyedDebouncer::pending() const {
    std::lock_guard<std::mutex> lk(m_shared->m);
    return m_shared->items.size();
}

} // namespace guard
} // namespace networking
} // namespace fizmo
