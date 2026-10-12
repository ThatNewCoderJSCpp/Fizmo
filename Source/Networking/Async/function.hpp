#ifndef FIZMO_ASYNC_FUNCTION_HPP
#define FIZMO_ASYNC_FUNCTION_HPP

#include <cstddef>
#include <memory>
#include <stdexcept>
#include <type_traits>
#include <utility>

namespace fizmo {
namespace networking {
namespace async {

template <typename Signature>
class UniqueFunction;

template <typename R, typename... Args>
class UniqueFunction<R(Args...)> {
private:
    struct Base {
        virtual ~Base() = default;
        virtual R call(Args... args) = 0;
    };

    template <typename F>
    struct Impl final : Base {
        F fn;
        explicit Impl(F&& f) : fn(std::move(f)) {}
        explicit Impl(const F& f) : fn(f) {}
        R call(Args... args) override { return fn(std::forward<Args>(args)...); }
    };

    std::unique_ptr<Base> m_impl;

public:
    UniqueFunction() noexcept = default;
    UniqueFunction(std::nullptr_t) noexcept {}
    UniqueFunction(UniqueFunction&&) noexcept = default;
    UniqueFunction& operator=(UniqueFunction&&) noexcept = default;
    UniqueFunction(const UniqueFunction&) = delete;
    UniqueFunction& operator=(const UniqueFunction&) = delete;

    template <typename F, typename D = typename std::decay<F>::type,
              typename = typename std::enable_if<!std::is_same<D, UniqueFunction>::value && !std::is_same<D, std::nullptr_t>::value>::type>
    UniqueFunction(F&& f) : m_impl(new Impl<D>(std::forward<F>(f))) {}

    template <typename F, typename D = typename std::decay<F>::type,
              typename = typename std::enable_if<!std::is_same<D, UniqueFunction>::value && !std::is_same<D, std::nullptr_t>::value>::type>
    UniqueFunction& operator=(F&& f) {
        m_impl.reset(new Impl<D>(std::forward<F>(f)));
        return *this;
    }

    UniqueFunction& operator=(std::nullptr_t) noexcept {
        m_impl.reset();
        return *this;
    }

    explicit operator bool() const noexcept { return static_cast<bool>(m_impl); }

    R operator()(Args... args) {
        if (!m_impl) throw std::bad_function_call();
        return m_impl->call(std::forward<Args>(args)...);
    }

    void reset() noexcept { m_impl.reset(); }
};

} // namespace async
} // namespace networking
} // namespace fizmo

#endif // FIZMO_ASYNC_FUNCTION_HPP
