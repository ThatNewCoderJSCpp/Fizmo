#ifndef FIZMO_ARRAYS_OPS_CONTAINER_OPS_HPP
#define FIZMO_ARRAYS_OPS_CONTAINER_OPS_HPP

#include "functional_ops.hpp"
#include <tuple>

namespace fizmo {
namespace arrays {
namespace detail {

template <typename C, typename = void>
struct is_std_array_like : std::false_type {};

template <typename C>
struct is_std_array_like<C, void_t<decltype(std::tuple_size<C>::value), decltype(std::declval<C&>().data())>> : std::true_type {};

template <typename Derived, typename T, typename Base>
class ContainerOps : public Base {
protected:
    template <typename C>
    using enable_range = typename std::enable_if<
        is_range_of<C, T>::value && !is_adapter_like<C>::value && !std::is_same<typename std::decay<C>::type, Derived>::value &&
        !std::is_same<typename std::decay<C>::type, std::initializer_list<T>>::value>::type;

    template <typename A>
    using enable_adapter = typename std::enable_if<is_adapter_like<A>::value && std::is_constructible<T, typename A::value_type>::value>::type;

    template <typename It>
    using enable_iterator = typename std::enable_if<is_iterator_like<It>::value>::type;

    template <typename U>
    using enable_value = typename std::enable_if<are_compatible_types<U, T>>::type;

    void init_fill(std::size_t count, const T& value) {
        this->fz_reserve_exact(count);
        construct_fill_n(this->m_data, count, value);
        this->m_size = count;
    }

    template <typename It>
    void init_range(It first, It last) {
        if (is_random_access_like<It>::value) {
            const std::size_t n = static_cast<std::size_t>(distance_of(first, last));
            this->fz_reserve_exact(n);
            this->m_size = construct_copy_range(this->m_data, first, last);
            return;
        }
        for (; !(first == last); ++first) static_cast<Derived&>(*this).emplace_back(*first);
    }

    template <typename It>
    static std::ptrdiff_t distance_of(It first, It last) { return static_cast<std::ptrdiff_t>(last - first); }

    template <typename A>
    void init_adapter(A a) {
        using V = typename A::value_type;
        const std::size_t n = static_cast<std::size_t>(a.size());
        this->fz_reserve_exact(n);
        if constexpr (is_queue_like<A>::value) {
            for (std::size_t i = 0; i < n; ++i) { construct_element(this->m_data + i, static_cast<T>(static_cast<V&&>(a.front()))); ++this->m_size; a.pop(); }
        } else if constexpr (is_stack_like<A>::value) {
            for (std::size_t i = n; i > 0; --i) { construct_element(this->m_data + i - 1, static_cast<T>(static_cast<V&&>(const_cast<V&>(a.top())))); a.pop(); }
            this->m_size = n;
        } else {
            for (std::size_t i = 0; i < n; ++i) { construct_element(this->m_data + i, static_cast<T>(static_cast<V&&>(const_cast<V&>(a.top())))); ++this->m_size; a.pop(); }
        }
    }

public:
    ContainerOps() noexcept = default;

    ContainerOps(std::initializer_list<T> init) { init_range(init.begin(), init.end()); }

    explicit ContainerOps(std::size_t size) {
        this->fz_reserve_exact(size);
        construct_default_n(this->m_data, size);
        this->m_size = size;
    }

    template <typename U, typename = enable_value<U>>
    ContainerOps(std::size_t size, U&& value) { init_fill(size, static_cast<T>(value)); }

    template <typename U, typename = enable_value<U>>
    ContainerOps(std::size_t capacity, std::size_t size, U&& value) {
        this->fz_reserve_exact(capacity > size ? capacity : size);
        construct_fill_n(this->m_data, size, static_cast<T>(value));
        this->m_size = size;
    }

    template <typename It, typename = enable_iterator<It>>
    ContainerOps(It first, It last) { init_range(first, last); }

    template <typename C, typename = enable_range<C>>
    explicit ContainerOps(const C& c) { init_range(c.begin(), c.end()); }

    template <typename A, typename = enable_adapter<A>, typename = void>
    explicit ContainerOps(A a) { init_adapter(static_cast<A&&>(a)); }

    template <typename C, typename = enable_range<C>>
    Derived& operator=(const C& c) {
        Derived& d = static_cast<Derived&>(*this);
        d.clear();
        d.append(c.begin(), c.end());
        return d;
    }

    template <typename A, typename = enable_adapter<A>, typename = void>
    Derived& operator=(A a) {
        Derived& d = static_cast<Derived&>(*this);
        d.clear();
        Derived tmp(static_cast<A&&>(a));
        d = static_cast<Derived&&>(tmp);
        return d;
    }

    template <typename C, typename = typename std::enable_if<
        !is_custom_array_v<C> && !is_std_array_like<C>::value && !is_adapter_like<C>::value && !std::is_same<C, bool>::value &&
        is_constructible_from_range<C, const T*>::value>::type>
    explicit operator C() const { return C(this->m_data, this->m_data + this->m_size); }

    template <typename C, typename = typename std::enable_if<is_std_array_like<C>::value && !is_custom_array_v<C>>::type, typename = void>
    explicit operator C() const {
        C result{};
        const std::size_t n = std::tuple_size<C>::value < this->m_size ? std::tuple_size<C>::value : this->m_size;
        for (std::size_t i = 0; i < n; ++i) result[i] = this->m_data[i];
        return result;
    }

    template <typename C, typename = typename std::enable_if<is_queue_like<C>::value || is_stack_like<C>::value>::type, typename = void, typename = void>
    explicit operator C() const { return C(typename C::container_type(this->m_data, this->m_data + this->m_size)); }

    template <typename C, typename = typename std::enable_if<is_priority_queue_like<C>::value>::type, typename = void, typename = void, typename = void>
    explicit operator C() const { return C(typename C::value_compare(), typename C::container_type(this->m_data, this->m_data + this->m_size)); }

    std::size_t memory_usage() const noexcept { return sizeof(Derived) + (this->fz_owns_heap() ? this->m_capacity * sizeof(T) : 0); }
};

} // namespace detail
} // namespace arrays
} // namespace fizmo

#endif // FIZMO_ARRAYS_OPS_CONTAINER_OPS_HPP
