#ifndef FIZMO_OPTIONAL_HPP
#define FIZMO_OPTIONAL_HPP

#include "../Basic/basic_includes.hpp"
#include "../Basic/fizmo_defines.hpp"

namespace fizmo {

class bad_optional_access : public std::runtime_error {
public:
    bad_optional_access() : std::runtime_error("Bad optional access") {}
    const char* what() const noexcept override { return "Bad optional access"; }
};


template <typename T>
class Optional;

struct in_place_t {
    explicit in_place_t() = default;
};

OPTIONAL_CPP17_INLINE constexpr in_place_t in_place{};

struct null_option_t {
    struct init {};
    explicit constexpr null_option_t(init) {}
};

OPTIONAL_CPP17_INLINE constexpr null_option_t null_option{null_option_t::init{}};

template <typename T>
class Optional {
private:
    static_assert(!std::is_same<typename std::decay<T>::type, null_option_t>::value, "Optional<null_option_t> is not allowed");
    static_assert(!std::is_same<typename std::decay<T>::type, in_place_t>::value, "Optional<in_place_t> is not allowed");
    static_assert(!std::is_reference<T>::value, "Optional references are not allowed");
    static_assert(!std::is_void<T>::value, "Optional<void> is not allowed");

    union Storage {
        char m_dummy;
        T m_value;

        constexpr Storage() noexcept : m_dummy() {}

        template <typename... Args>
        constexpr explicit Storage(in_place_t, Args&&... args) : m_value(std::forward<Args>(args)...) {}

        ~Storage() {}
    };

    bool m_has_value = false;
    Storage m_storage;

    template <typename U>
    using enable_if_convertible = typename std::enable_if<std::is_convertible<U, T>::value>::type;

    template <typename U>
    using enable_if_same_or_convertible = typename std::enable_if<
        std::is_convertible<U, T>::value &&
        !std::is_same<typename std::decay<U>::type, Optional<T>>::value
    >::type;

public:
    constexpr Optional() noexcept : m_has_value(false), m_storage() {}
    constexpr Optional(null_option_t) noexcept : m_has_value(false), m_storage() {}

    Optional(const Optional& other) : m_has_value(other.m_has_value) {
        if (other.m_has_value) {
            ::new (std::addressof(m_storage.m_value)) T(other.m_storage.m_value);
        }
    }

    Optional(Optional&& other) 
    #ifdef CPP14_OR_GREATER
        noexcept(std::is_nothrow_move_constructible<T>::value)
    #endif
        : m_has_value(other.m_has_value) {
        if (other.m_has_value) {
            ::new (std::addressof(m_storage.m_value)) T(std::move(other.m_storage.m_value));
        }
    }

    template <typename U, typename = enable_if_convertible<U>>
    Optional(const Optional<U>& other)
        : m_has_value(other.has_value()) {
        if (other.has_value()) {
            ::new (std::addressof(m_storage.m_value)) T(*other);
        }
    }

    template <typename U, typename = enable_if_convertible<U>>
    Optional(Optional<U>&& other)
        : m_has_value(other.has_value()) {
        if (other.has_value()) {
            ::new (std::addressof(m_storage.m_value)) T(std::move(*other));
        }
    }

    template <typename... Args>
    explicit Optional(in_place_t, Args&&... args) : m_has_value(true), m_storage(in_place, std::forward<Args>(args)...) {}

    template <typename U = T, typename = enable_if_same_or_convertible<U>>
    Optional(U&& value) : m_has_value(true), m_storage(in_place, std::forward<U>(value)) {}

    ~Optional() { reset(); }

public:
    Optional& operator=(null_option_t) noexcept {
        reset();
        return *this;
    }

    Optional& operator=(const Optional& other) {
        if (this != &other) {
            if (other.m_has_value) {
                if (m_has_value) {
                    m_storage.m_value = other.m_storage.m_value;
                } else {
                    ::new (std::addressof(m_storage.m_value)) T(other.m_storage.m_value);
                    m_has_value = true;
                }
            } else {
                reset();
            }
        }
        return *this;
    }

    Optional& operator=(Optional&& other)
    #ifdef CPP14_OR_GREATER
        noexcept(std::is_nothrow_move_assignable<T>::value && std::is_nothrow_move_constructible<T>::value)
    #endif
    {
        if (this != &other) {
            if (other.m_has_value) {
                if (m_has_value) {
                    m_storage.m_value = std::move(other.m_storage.m_value);
                } else {
                    ::new (std::addressof(m_storage.m_value)) T(std::move(other.m_storage.m_value));
                    m_has_value = true;
                }
            } else {
                reset();
            }
        }
        return *this;
    }

    template <typename U = T, typename = enable_if_same_or_convertible<U>>
    Optional& operator=(U&& value) {
        if (m_has_value) {
            m_storage.m_value = std::forward<U>(value);
        } else {
            ::new (std::addressof(m_storage.m_value)) T(std::forward<U>(value));
            m_has_value = true;
        }
        return *this;
    }

public:
    template <typename... Args>
    T& emplace(Args&&... args) {
        reset();
        ::new (std::addressof(m_storage.m_value)) T(std::forward<Args>(args)...);
        m_has_value = true;
        return m_storage.m_value;
    }

    void reset() noexcept {
        if (m_has_value) {
            m_storage.m_value.~T();
            m_has_value = false;
        }
    }

    void swap(Optional& other) 
    #ifdef CPP14_OR_GREATER
        noexcept(std::is_nothrow_move_constructible<T>::value && std::is_nothrow_swappable<T>::value)
    #endif
    {
        if (m_has_value && other.m_has_value) {
            using std::swap;
            swap(m_storage.m_value, other.m_storage.m_value);
        } else if (m_has_value) {
            ::new (std::addressof(other.m_storage.m_value)) T(std::move(m_storage.m_value));
            m_storage.m_value.~T();
            other.m_has_value = true;
            m_has_value = false;
        } else if (other.m_has_value) {
            ::new (std::addressof(m_storage.m_value)) T(std::move(other.m_storage.m_value));
            other.m_storage.m_value.~T();
            m_has_value = true;
            other.m_has_value = false;
        }
    }

    OPTIONAL_NODISCARD constexpr bool has_value() const noexcept { return m_has_value; }
    OPTIONAL_NODISCARD constexpr operator bool() const noexcept { return m_has_value; }

public:
    OPTIONAL_NODISCARD const T& value() const& {
        if (!m_has_value) { throw bad_optional_access(); }
        return m_storage.m_value;
    }

    OPTIONAL_NODISCARD T& value() & {
        if (!m_has_value) { throw bad_optional_access(); }
        return m_storage.m_value;
    }

    OPTIONAL_NODISCARD T&& value() && {
        if (!m_has_value) { throw bad_optional_access(); }
        return std::move(m_storage.m_value);
    }

    OPTIONAL_NODISCARD const T&& value() const&& {
        if (!m_has_value) { throw bad_optional_access(); }
        return std::move(m_storage.m_value);
    }

    template <typename U>
    OPTIONAL_NODISCARD T value_or(U&& default_value) const& { return m_has_value ? m_storage.m_value : static_cast<T>(std::forward<U>(default_value)); }

    template <typename U>
    OPTIONAL_NODISCARD T value_or(U&& default_value) && { return m_has_value ? std::move(m_storage.m_value) : static_cast<T>(std::forward<U>(default_value)); }

public:
    OPTIONAL_NODISCARD const T* operator->() const {
        if (!m_has_value) { throw bad_optional_access(); }
        return std::addressof(m_storage.m_value);
    }
    
    OPTIONAL_NODISCARD T* operator->() {
        if (!m_has_value) { throw bad_optional_access(); }
        return std::addressof(m_storage.m_value);
    }
    
    OPTIONAL_NODISCARD const T& operator*() const& {
        if (!m_has_value) { throw bad_optional_access(); }
        return m_storage.m_value;
    }
    
    OPTIONAL_NODISCARD T& operator*() & {
        if (!m_has_value) { throw bad_optional_access(); }
        return m_storage.m_value;
    }
    
    OPTIONAL_NODISCARD const T&& operator*() const&& {
        if (!m_has_value) { throw bad_optional_access(); }
        return std::move(m_storage.m_value);
    }
    
    OPTIONAL_NODISCARD T&& operator*() && {
        if (!m_has_value) { throw bad_optional_access(); }
        return std::move(m_storage.m_value);
    }
};

} // namespace fizmo

#endif // FIZMO_OPTIONAL_HPP