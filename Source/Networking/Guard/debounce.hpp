#ifndef FIZMO_GUARD_DEBOUNCE_HPP
#define FIZMO_GUARD_DEBOUNCE_HPP

#include "guard_types.hpp"

#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <tuple>
#include <unordered_map>
#include <utility>

namespace fizmo {
namespace networking {
namespace guard {

struct DebounceOptions {
    std::chrono::milliseconds wait{250};
    bool                      leading  = false;
    bool                      trailing = true;
    std::chrono::milliseconds max_wait{0};
};

class Debouncer {
public:
    Debouncer() = default;
    Debouncer(std::function<void()> fn, DebounceOptions options = DebounceOptions());
    ~Debouncer();

    Debouncer(Debouncer&&) noexcept = default;
    Debouncer& operator=(Debouncer&&) noexcept = default;
    Debouncer(const Debouncer&) = delete;
    Debouncer& operator=(const Debouncer&) = delete;

    void call();
    void operator()() { call(); }
    void cancel();
    void flush();
    bool pending() const;
    bool active() const;

    struct State;

private:
    std::shared_ptr<State> m_state;
};

class Throttler {
public:
    Throttler() = default;
    Throttler(std::function<void()> fn, std::chrono::milliseconds interval, bool trailing = true);
    ~Throttler();

    Throttler(Throttler&&) noexcept = default;
    Throttler& operator=(Throttler&&) noexcept = default;
    Throttler(const Throttler&) = delete;
    Throttler& operator=(const Throttler&) = delete;

    void call();
    void operator()() { call(); }
    void cancel();
    bool pending() const;

    struct State;

private:
    std::shared_ptr<State> m_state;
};

template <typename... Args>
class Debounced {
public:
    Debounced(std::function<void(Args...)> fn, DebounceOptions options = DebounceOptions())
        : m_args(std::make_shared<Slot>()) {
        auto slot = m_args;
        auto target = std::move(fn);

        m_debouncer = Debouncer([slot, target] {
            std::optional<std::tuple<std::decay_t<Args>...>> args;

            {
                std::lock_guard<std::mutex> lk(slot->m);
                args.swap(slot->last);
            }

            if (args) std::apply(target, *args);
        }, options);
    }

    void operator()(Args... args) {
        {
            std::lock_guard<std::mutex> lk(m_args->m);
            m_args->last.emplace(std::forward<Args>(args)...);
        }

        m_debouncer.call();
    }

    void cancel() {
        m_debouncer.cancel();
        std::lock_guard<std::mutex> lk(m_args->m);
        m_args->last.reset();
    }

    void flush() { m_debouncer.flush(); }
    bool pending() const { return m_debouncer.pending(); }

private:
    struct Slot {
        std::mutex                                          m;
        std::optional<std::tuple<std::decay_t<Args>...>>    last;
    };

    std::shared_ptr<Slot> m_args;
    Debouncer             m_debouncer;
};

class KeyedDebouncer {
public:
    using Callback = std::function<void(const std::string& key)>;

    KeyedDebouncer(Callback fn, DebounceOptions options = DebounceOptions());

    void call(const std::string& key);
    void cancel(const std::string& key);
    void cancel_all();
    void flush(const std::string& key);
    std::size_t pending() const;

private:
    struct Shared {
        Callback                                                    fn;
        DebounceOptions                                             options;
        std::mutex                                                  m;
        std::unordered_map<std::string, std::shared_ptr<Debouncer>> items;
    };

    std::shared_ptr<Shared> m_shared;
};

} // namespace guard
} // namespace networking
} // namespace fizmo

#endif // FIZMO_GUARD_DEBOUNCE_HPP
