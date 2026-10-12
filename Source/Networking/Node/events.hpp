#ifndef FIZMO_NODE_EVENTS_HPP
#define FIZMO_NODE_EVENTS_HPP

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <system_error>
#include <tuple>
#include <type_traits>
#include <typeindex>
#include <unordered_map>
#include <utility>
#include <vector>

namespace fizmo {
namespace networking {

using ListenerId = std::uint64_t;

void report_unhandled_error(const std::string& source, const std::error_code& ec);

namespace detail {

inline ListenerId next_listener_id() noexcept {
    static std::atomic<ListenerId> ids{0};
    return ++ids;
}

template <typename T>
struct callable_traits : callable_traits<decltype(&T::operator())> {};

template <typename R, typename... A>
struct callable_traits<R(A...)> {
    using args = std::tuple<A...>;
};

template <typename R, typename... A>
struct callable_traits<R(*)(A...)> : callable_traits<R(A...)> {};

template <typename C, typename R, typename... A>
struct callable_traits<R(C::*)(A...)> : callable_traits<R(A...)> {};

template <typename C, typename R, typename... A>
struct callable_traits<R(C::*)(A...) const> : callable_traits<R(A...)> {};

template <typename C, typename R, typename... A>
struct callable_traits<R(C::*)(A...) noexcept> : callable_traits<R(A...)> {};

template <typename C, typename R, typename... A>
struct callable_traits<R(C::*)(A...) const noexcept> : callable_traits<R(A...)> {};

template <typename R, typename... A>
struct callable_traits<std::function<R(A...)>> : callable_traits<R(A...)> {};

template <typename... A>
struct signature_key {
    static std::type_index get() { return std::type_index(typeid(void(*)(std::decay_t<A>...))); }
};

} // namespace detail

template <typename... Args>
class Event {
public:
    using Callback = std::function<void(Args...)>;

    Event() = default;
    Event(const Event&) = delete;
    Event& operator=(const Event&) = delete;

    ListenerId add(Callback cb, bool once = false) {
        if (!cb) return 0;
        auto entry = std::make_shared<Entry>();
        entry->id = detail::next_listener_id();
        entry->once = once;
        entry->cb = std::move(cb);
        std::lock_guard<std::mutex> lk(m_mutex);
        m_listeners.push_back(entry);
        return entry->id;
    }

    ListenerId once(Callback cb) { return add(std::move(cb), true); }

    ListenerId prepend(Callback cb, bool once = false) {
        if (!cb) return 0;
        auto entry = std::make_shared<Entry>();
        entry->id = detail::next_listener_id();
        entry->once = once;
        entry->cb = std::move(cb);
        std::lock_guard<std::mutex> lk(m_mutex);
        m_listeners.insert(m_listeners.begin(), entry);
        return entry->id;
    }

    bool remove(ListenerId id) {
        std::lock_guard<std::mutex> lk(m_mutex);

        for (auto it = m_listeners.begin(); it != m_listeners.end(); ++it) {
            if ((*it)->id != id) continue;
            m_listeners.erase(it);
            return true;
        }

        return false;
    }

    void clear() {
        std::vector<std::shared_ptr<Entry>> doomed;
        std::lock_guard<std::mutex> lk(m_mutex);
        doomed.swap(m_listeners);
    }

    std::size_t count() const {
        std::lock_guard<std::mutex> lk(m_mutex);
        return m_listeners.size();
    }

    bool empty() const { return count() == 0; }

    std::size_t emit(Args... args) {
        std::vector<std::shared_ptr<Entry>> snapshot;

        {
            std::lock_guard<std::mutex> lk(m_mutex);
            snapshot = m_listeners;

            if (!snapshot.empty()) {
                m_listeners.erase(std::remove_if(m_listeners.begin(), m_listeners.end(), [](const std::shared_ptr<Entry>& e) { return e->once; }), m_listeners.end());
            }
        }

        for (auto& e : snapshot) e->cb(args...);
        return snapshot.size();
    }

private:
    struct Entry {
        ListenerId id = 0;
        bool       once = false;
        Callback   cb;
    };

    mutable std::mutex                  m_mutex;
    std::vector<std::shared_ptr<Entry>> m_listeners;
};

class EventEmitter {
public:
    EventEmitter() = default;
    virtual ~EventEmitter() = default;
    EventEmitter(const EventEmitter&) = delete;
    EventEmitter& operator=(const EventEmitter&) = delete;

    template <typename F>
    ListenerId on(const std::string& name, F&& f) { return add_listener(name, std::forward<F>(f), false, false); }

    template <typename F>
    ListenerId once(const std::string& name, F&& f) { return add_listener(name, std::forward<F>(f), true, false); }

    template <typename F>
    ListenerId prepend_listener(const std::string& name, F&& f) { return add_listener(name, std::forward<F>(f), false, true); }

    template <typename F>
    ListenerId add_listener(const std::string& name, F&& f) { return add_listener(name, std::forward<F>(f), false, false); }

    bool off(ListenerId id) {
        std::lock_guard<std::mutex> lk(m_mutex);

        for (auto& kv : m_events) {
            auto& list = kv.second.listeners;

            for (auto it = list.begin(); it != list.end(); ++it) {
                if ((*it)->id != id) continue;
                list.erase(it);
                return true;
            }
        }

        return false;
    }

    bool remove_listener(ListenerId id) { return off(id); }

    void remove_all_listeners(const std::string& name) {
        std::lock_guard<std::mutex> lk(m_mutex);
        m_events.erase(name);
    }

    void remove_all_listeners() {
        std::lock_guard<std::mutex> lk(m_mutex);
        m_events.clear();
    }

    std::size_t listener_count(const std::string& name) const {
        std::lock_guard<std::mutex> lk(m_mutex);
        auto it = m_events.find(name);
        return it == m_events.end() ? 0 : it->second.listeners.size();
    }

    std::vector<std::string> event_names() const {
        std::vector<std::string> out;
        std::lock_guard<std::mutex> lk(m_mutex);

        for (auto& kv : m_events) {
            if (!kv.second.listeners.empty()) out.push_back(kv.first);
        }

        return out;
    }

    template <typename... A>
    bool emit(const std::string& name, const A&... args) {
        const std::type_index key = detail::signature_key<A...>::get();
        std::vector<std::shared_ptr<Listener>> snapshot;

        {
            std::lock_guard<std::mutex> lk(m_mutex);
            auto it = m_events.find(name);

            if (it != m_events.end() && !it->second.listeners.empty()) {
                if (it->second.key != key) throw std::invalid_argument("EventEmitter: argument types do not match the listeners of event '" + name + "'");
                snapshot = it->second.listeners;
                auto& list = it->second.listeners;
                list.erase(std::remove_if(list.begin(), list.end(), [](const std::shared_ptr<Listener>& l) { return l->once; }), list.end());
            }
        }

        if (snapshot.empty()) {
            if (name == "error") report_unhandled(args...);
            return false;
        }

        for (auto& l : snapshot) {
            auto* fn = static_cast<std::function<void(const std::decay_t<A>&...)>*>(l->fn.get());
            (*fn)(args...);
        }

        return true;
    }

private:
    struct Listener {
        ListenerId            id = 0;
        bool                  once = false;
        std::shared_ptr<void> fn;
    };

    struct Slot {
        std::type_index                        key = std::type_index(typeid(void));
        std::vector<std::shared_ptr<Listener>> listeners;
    };

    mutable std::mutex                    m_mutex;
    std::unordered_map<std::string, Slot> m_events;

    template <typename F, typename... A>
    static std::shared_ptr<void> wrap(F&& f, std::tuple<A...>*) {
        using Fn = std::function<void(const std::decay_t<A>&...)>;
        auto holder = std::make_shared<Fn>([g = std::forward<F>(f)](const std::decay_t<A>&... a) mutable { g(a...); });
        return std::static_pointer_cast<void>(holder);
    }

    template <typename F, typename... A>
    static std::type_index key_of(std::tuple<A...>*) { return detail::signature_key<A...>::get(); }

    template <typename F>
    ListenerId add_listener(const std::string& name, F&& f, bool once, bool front) {
        using Traits = detail::callable_traits<std::decay_t<F>>;
        using Args = typename Traits::args;
        auto l = std::make_shared<Listener>();
        l->id = detail::next_listener_id();
        l->once = once;
        l->fn = wrap(std::forward<F>(f), static_cast<Args*>(nullptr));
        const std::type_index key = key_of<F>(static_cast<Args*>(nullptr));
        std::lock_guard<std::mutex> lk(m_mutex);
        Slot& slot = m_events[name];

        if (slot.listeners.empty()) slot.key = key;
        else if (slot.key != key) throw std::invalid_argument("EventEmitter: listener signature does not match existing listeners of event '" + name + "'");

        if (front) slot.listeners.insert(slot.listeners.begin(), l);
        else slot.listeners.push_back(l);
        return l->id;
    }

    static void report_unhandled() { report_unhandled_error("EventEmitter", std::make_error_code(std::errc::invalid_argument)); }
    static void report_unhandled(const std::error_code& ec) { report_unhandled_error("EventEmitter", ec); }
    static void report_unhandled(const std::string& message) { report_unhandled_error("EventEmitter: " + message, std::make_error_code(std::errc::invalid_argument)); }

    template <typename... A>
    static void report_unhandled(const A&...) { report_unhandled_error("EventEmitter", std::make_error_code(std::errc::invalid_argument)); }
};

} // namespace networking
} // namespace fizmo

#endif // FIZMO_NODE_EVENTS_HPP
