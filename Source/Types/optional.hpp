#ifndef FIZMO_OPTIONAL_HPP
#define FIZMO_OPTIONAL_HPP

#include <cstddef>
#include <functional>
#include <initializer_list>
#include <memory>
#include <new>
#include <stdexcept>
#include <type_traits>
#include <utility>

namespace fizmo {

class bad_optional_access : public std::runtime_error {
public:
    bad_optional_access() : std::runtime_error("Bad optional access") {}
};

struct in_place_t {
    explicit in_place_t() = default;
};

inline constexpr in_place_t in_place{};

struct null_option_t {
    struct init {};
    explicit constexpr null_option_t(init) {}
};

inline constexpr null_option_t null_option{ null_option_t::init{} };

template <class T>
class Optional;

namespace optional_detail {

template <class T>
struct is_optional : std::false_type {};

template <class T>
struct is_optional<Optional<T>> : std::true_type {};

template <class T, class U>
using converts_from_optional = std::integral_constant<bool,
    std::is_constructible<T, Optional<U>&>::value || std::is_constructible<T, const Optional<U>&>::value ||
    std::is_constructible<T, Optional<U>&&>::value || std::is_constructible<T, const Optional<U>&&>::value ||
    std::is_convertible<Optional<U>&, T>::value || std::is_convertible<const Optional<U>&, T>::value ||
    std::is_convertible<Optional<U>&&, T>::value || std::is_convertible<const Optional<U>&&, T>::value>;

} // namespace optional_detail

template <class T>
class Optional {
    static_assert(!std::is_same<typename std::decay<T>::type, null_option_t>::value, "Optional<null_option_t> is not allowed");
    static_assert(!std::is_same<typename std::decay<T>::type, in_place_t>::value, "Optional<in_place_t> is not allowed");
    static_assert(!std::is_reference<T>::value, "Optional references are not allowed");
    static_assert(!std::is_void<T>::value, "Optional<void> is not allowed");
    static_assert(!std::is_array<T>::value, "Optional arrays are not allowed");

public:
    using value_type = T;

    constexpr Optional() noexcept : m_dummy() {}
    constexpr Optional(null_option_t) noexcept : m_dummy() {}

    Optional(const Optional& other) : m_dummy() {
        if (other.m_has) construct(other.m_value);
    }

    Optional(Optional&& other) noexcept(std::is_nothrow_move_constructible<T>::value) : m_dummy() {
        if (other.m_has) construct(std::move(other.m_value));
    }

    template <class U, typename std::enable_if<!std::is_same<U, T>::value && std::is_constructible<T, const U&>::value && !optional_detail::converts_from_optional<T, U>::value, int>::type = 0>
    Optional(const Optional<U>& other) : m_dummy() {
        if (other.has_value()) construct(*other);
    }

    template <class U, typename std::enable_if<!std::is_same<U, T>::value && std::is_constructible<T, U&&>::value && !optional_detail::converts_from_optional<T, U>::value, int>::type = 0>
    Optional(Optional<U>&& other) : m_dummy() {
        if (other.has_value()) construct(std::move(*other));
    }

    template <class... Args, typename std::enable_if<std::is_constructible<T, Args...>::value, int>::type = 0>
    constexpr explicit Optional(in_place_t, Args&&... args) : m_value(std::forward<Args>(args)...), m_has(true) {}

    template <class U, class... Args, typename std::enable_if<std::is_constructible<T, std::initializer_list<U>&, Args...>::value, int>::type = 0>
    constexpr explicit Optional(in_place_t, std::initializer_list<U> il, Args&&... args) : m_value(il, std::forward<Args>(args)...), m_has(true) {}

    template <class U = T, typename std::enable_if<
        std::is_constructible<T, U&&>::value &&
        !std::is_same<typename std::decay<U>::type, in_place_t>::value &&
        !std::is_same<typename std::decay<U>::type, null_option_t>::value &&
        !std::is_same<typename std::decay<U>::type, Optional>::value, int>::type = 0>
    constexpr Optional(U&& value) : m_value(std::forward<U>(value)), m_has(true) {}

    ~Optional() { reset(); }

    Optional& operator=(null_option_t) noexcept {
        reset();
        return *this;
    }

    Optional& operator=(const Optional& other) {
        if (this == &other) return *this;

        if (other.m_has) {
            if (m_has) m_value = other.m_value;
            else construct(other.m_value);
        } else {
            reset();
        }

        return *this;
    }

    Optional& operator=(Optional&& other) noexcept(std::is_nothrow_move_assignable<T>::value && std::is_nothrow_move_constructible<T>::value) {
        if (this == &other) return *this;

        if (other.m_has) {
            if (m_has) m_value = std::move(other.m_value);
            else construct(std::move(other.m_value));
        } else {
            reset();
        }

        return *this;
    }

    template <class U = T, typename std::enable_if<
        !std::is_same<typename std::decay<U>::type, Optional>::value &&
        !std::is_same<typename std::decay<U>::type, null_option_t>::value &&
        std::is_constructible<T, U>::value && std::is_assignable<T&, U>::value, int>::type = 0>
    Optional& operator=(U&& value) {
        if (m_has) m_value = std::forward<U>(value);
        else construct(std::forward<U>(value));
        return *this;
    }

    template <class... Args>
    T& emplace(Args&&... args) {
        reset();
        construct(std::forward<Args>(args)...);
        return m_value;
    }

    template <class U, class... Args>
    T& emplace(std::initializer_list<U> il, Args&&... args) {
        reset();
        construct(il, std::forward<Args>(args)...);
        return m_value;
    }

    void reset() noexcept {
        if (m_has) {
            m_value.~T();
            m_has = false;
        }
    }

    void swap(Optional& other) noexcept(std::is_nothrow_move_constructible<T>::value && std::is_nothrow_swappable<T>::value) {
        if (m_has && other.m_has) {
            using std::swap;
            swap(m_value, other.m_value);
        } else if (m_has) {
            other.construct(std::move(m_value));
            reset();
        } else if (other.m_has) {
            construct(std::move(other.m_value));
            other.reset();
        }
    }

    constexpr bool has_value() const noexcept { return m_has; }
    constexpr explicit operator bool() const noexcept { return m_has; }

    const T& value() const& {
        check();
        return m_value;
    }

    T& value() & {
        check();
        return m_value;
    }

    T&& value() && {
        check();
        return std::move(m_value);
    }

    const T&& value() const&& {
        check();
        return std::move(m_value);
    }

    template <class U>
    T value_or(U&& fallback) const& { return m_has ? m_value : static_cast<T>(std::forward<U>(fallback)); }

    template <class U>
    T value_or(U&& fallback) && { return m_has ? std::move(m_value) : static_cast<T>(std::forward<U>(fallback)); }

    const T* operator->() const {
        check();
        return std::addressof(m_value);
    }

    T* operator->() {
        check();
        return std::addressof(m_value);
    }

    const T& operator*() const& { return value(); }
    T& operator*() & { return value(); }
    T&& operator*() && { return std::move(*this).value(); }
    const T&& operator*() const&& { return std::move(*this).value(); }

    const T* get_if() const noexcept { return m_has ? std::addressof(m_value) : nullptr; }
    T* get_if() noexcept { return m_has ? std::addressof(m_value) : nullptr; }

    template <class F>
    auto and_then(F&& f) const& -> decltype(f(std::declval<const T&>())) {
        if (m_has) return f(m_value);
        return null_option;
    }

    template <class F>
    auto transform(F&& f) const& -> Optional<typename std::decay<decltype(f(std::declval<const T&>()))>::type> {
        if (m_has) return Optional<typename std::decay<decltype(f(std::declval<const T&>()))>::type>(f(m_value));
        return null_option;
    }

    template <class F>
    Optional or_else(F&& f) const& {
        if (m_has) return *this;
        return f();
    }

    template <class Pred>
    Optional filter(Pred&& pred) const& {
        if (m_has && pred(m_value)) return *this;
        return null_option;
    }

private:
    union {
        char m_dummy;
        T    m_value;
    };

    bool m_has = false;

    template <class... Args>
    void construct(Args&&... args) {
        ::new (static_cast<void*>(std::addressof(m_value))) T(std::forward<Args>(args)...);
        m_has = true;
    }

    void check() const {
        if (!m_has) throw bad_optional_access();
    }
};

template <class T>
Optional<typename std::decay<T>::type> make_optional(T&& v) { return Optional<typename std::decay<T>::type>(std::forward<T>(v)); }

template <class T, class... Args>
Optional<T> make_optional(Args&&... args) { return Optional<T>(in_place, std::forward<Args>(args)...); }

template <class T>
void swap(Optional<T>& a, Optional<T>& b) noexcept(noexcept(a.swap(b))) { a.swap(b); }

template <class T, class U>
bool operator==(const Optional<T>& a, const Optional<U>& b) { return a.has_value() == b.has_value() && (!a.has_value() || *a == *b); }

template <class T, class U>
bool operator!=(const Optional<T>& a, const Optional<U>& b) { return !(a == b); }

template <class T, class U>
bool operator<(const Optional<T>& a, const Optional<U>& b) { return b.has_value() && (!a.has_value() || *a < *b); }

template <class T, class U>
bool operator>(const Optional<T>& a, const Optional<U>& b) { return b < a; }

template <class T, class U>
bool operator<=(const Optional<T>& a, const Optional<U>& b) { return !(b < a); }

template <class T, class U>
bool operator>=(const Optional<T>& a, const Optional<U>& b) { return !(a < b); }

template <class T>
bool operator==(const Optional<T>& a, null_option_t) noexcept { return !a.has_value(); }

template <class T>
bool operator==(null_option_t, const Optional<T>& a) noexcept { return !a.has_value(); }

template <class T>
bool operator!=(const Optional<T>& a, null_option_t) noexcept { return a.has_value(); }

template <class T>
bool operator!=(null_option_t, const Optional<T>& a) noexcept { return a.has_value(); }

template <class T, class U, typename std::enable_if<!optional_detail::is_optional<U>::value && !std::is_same<U, null_option_t>::value, int>::type = 0>
bool operator==(const Optional<T>& a, const U& v) { return a.has_value() && *a == v; }

template <class T, class U, typename std::enable_if<!optional_detail::is_optional<U>::value && !std::is_same<U, null_option_t>::value, int>::type = 0>
bool operator==(const U& v, const Optional<T>& a) { return a.has_value() && *a == v; }

template <class T, class U, typename std::enable_if<!optional_detail::is_optional<U>::value && !std::is_same<U, null_option_t>::value, int>::type = 0>
bool operator!=(const Optional<T>& a, const U& v) { return !(a == v); }

template <class T, class U, typename std::enable_if<!optional_detail::is_optional<U>::value && !std::is_same<U, null_option_t>::value, int>::type = 0>
bool operator!=(const U& v, const Optional<T>& a) { return !(a == v); }

} // namespace fizmo

namespace std {

template <class T>
struct hash<fizmo::Optional<T>> {
    std::size_t operator()(const fizmo::Optional<T>& o) const { return o.has_value() ? std::hash<typename std::remove_const<T>::type>()(*o) : static_cast<std::size_t>(0x6a09e667f3bcc909ULL); }
};

} // namespace std

#endif // FIZMO_OPTIONAL_HPP
