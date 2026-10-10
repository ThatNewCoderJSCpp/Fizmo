#ifndef FIZMO_PREDICATE_HPP
#define FIZMO_PREDICATE_HPP

#include <type_traits>
#include <functional>

namespace fizmo {

template <typename... Args>
class Predicate {
private:
    std::function<bool(const Args&...)> m_fn;

public:
    template <typename F, typename = typename std::enable_if<!std::is_same<typename std::decay<F>::type, Predicate>::value>::type>
    Predicate(F&& f) : m_fn(std::forward<F>(f)) {}

    bool operator()(const Args&... args) const { return m_fn(args...); }

    Predicate operator!() const {
        auto fn = m_fn;
        return Predicate([fn](const Args&... args) { return !fn(args...); });
    }

    // a && b
    Predicate operator&&(const Predicate& other) const {
        auto a = m_fn;
        auto b = other.m_fn;
        return Predicate([a, b](const Args&... args) {
            return a(args...) && b(args...);
        });
    }

    // a || b
    Predicate operator||(const Predicate& other) const {
        auto a = m_fn;
        auto b = other.m_fn;
        return Predicate([a, b](const Args&... args) { return a(args...) || b(args...); });
    }

    // a != b
    Predicate operator^(const Predicate& other) const {
        auto a = m_fn;
        auto b = other.m_fn;
        return Predicate([a, b](const Args&... args) { return a(args...) != b(args...); });
    }

    // !(a && b)
    Predicate nand(const Predicate& other) const {
        auto a = m_fn;
        auto b = other.m_fn;
        return Predicate([a, b](const Args&... args) { return !(a(args...) && b(args...)); });
    }

    // !(a || b)
    Predicate nor(const Predicate& other) const {
        auto a = m_fn;
        auto b = other.m_fn;
        return Predicate([a, b](const Args&... args) { return !(a(args...) || b(args...)); });
    }

    // a == b
    Predicate xnor(const Predicate& other) const {
        auto a = m_fn;
        auto b = other.m_fn;
        return Predicate([a, b](const Args&... args) { return a(args...) == b(args...); });
    }

    // !a || b
    Predicate implies(const Predicate& other) const {
        auto a = m_fn;
        auto b = other.m_fn;
        return Predicate([a, b](const Args&... args) { return !a(args...) || b(args...); });
    }

    //  a || !b
    Predicate implied_by(const Predicate& other) const { return other.implies(*this); }
};

} // namespace fizmo

#endif // FIZMO_PREDICATE_HPP