#ifndef FIZMO_ARRAYS_TRAITS_HPP
#define FIZMO_ARRAYS_TRAITS_HPP

#include "config.hpp"
#include <memory>
#include <type_traits>
#include <utility>

namespace fizmo {

template <typename T>
class DynamicArray;

template <typename T, std::size_t N>
class SmallVector;

template <typename T>
struct is_custom_array : std::false_type {};

template <typename T>
struct is_custom_array<DynamicArray<T>> : std::true_type {};

template <typename T, std::size_t N>
struct is_custom_array<SmallVector<T, N>> : std::true_type {};

template <typename T>
constexpr bool is_custom_array_v = is_custom_array<T>::value;

template <typename T, typename U>
static constexpr bool are_compatible_types = std::is_convertible<U, T>::value || std::is_same<typename std::decay<U>::type, typename std::decay<T>::type>::value;

template <typename T>
struct is_trivially_relocatable : std::integral_constant<bool, std::is_trivially_copyable<T>::value && std::is_trivially_destructible<T>::value> {};

template <typename T>
struct is_trivially_relocatable<DynamicArray<T>> : std::true_type {};

template <typename C, typename U>
struct rebind_array;

template <typename T, typename U>
struct rebind_array<DynamicArray<T>, U> { using type = DynamicArray<U>; };

template <typename T, std::size_t N, typename U>
struct rebind_array<SmallVector<T, N>, U> { using type = SmallVector<U, N>; };

template <typename T>
constexpr bool is_trivially_relocatable_v = is_trivially_relocatable<T>::value;

static constexpr std::size_t npos = static_cast<std::size_t>(-1);

enum class Direction : std::uint8_t { All, Front, Back };

namespace detail {
    template <typename T>
    struct is_pointer_like : std::is_pointer<T> {};

    template <typename T>
    struct is_pointer_like<std::unique_ptr<T>> : std::true_type {};

    template <typename T>
    struct is_pointer_like<std::shared_ptr<T>> : std::true_type {};

    template <typename T>
    constexpr bool is_pointer_like_v = is_pointer_like<T>::value;

    template <typename T>
    typename std::enable_if<std::is_pointer<T>::value>::type
    deep_reset_element(T& elem) {
        delete elem;
        elem = nullptr;
    }

    template <typename T>
    typename std::enable_if<!std::is_pointer<T>::value && is_pointer_like_v<T>>::type
    deep_reset_element(T& elem) { elem.reset(); }

    template <typename T>
    typename std::enable_if<is_custom_array_v<T>>::type
    deep_reset_element(T& elem) { elem.deep_reset(); }

    template <typename T>
    typename std::enable_if<!std::is_pointer<T>::value && !is_pointer_like_v<T> && !is_custom_array_v<T>>::type
    deep_reset_element(T& elem) { elem = T{}; }
} // namespace detail

namespace arrays {

struct Less {
    template <typename A, typename B>
    constexpr bool operator()(const A& a, const B& b) const noexcept(noexcept(a < b)) { return a < b; }
};

struct Greater {
    template <typename A, typename B>
    constexpr bool operator()(const A& a, const B& b) const noexcept(noexcept(b < a)) { return b < a; }
};

struct EqualTo {
    template <typename A, typename B>
    constexpr bool operator()(const A& a, const B& b) const noexcept(noexcept(a == b)) { return a == b; }
};

namespace detail {

template <typename...>
using void_t = void;

template <typename T, typename = void>
struct has_begin_end : std::false_type {};

template <typename T>
struct has_begin_end<T, void_t<decltype(std::declval<const T&>().begin()), decltype(std::declval<const T&>().end())>> : std::true_type {};

template <typename T, typename = void>
struct has_data_size : std::false_type {};

template <typename T>
struct has_data_size<T, void_t<decltype(std::declval<T&>().data()), decltype(std::declval<const T&>().size())>>
    : std::integral_constant<bool, std::is_pointer<decltype(std::declval<T&>().data())>::value> {};

template <typename T, typename = void>
struct has_size_member : std::false_type {};

template <typename T>
struct has_size_member<T, void_t<decltype(std::declval<const T&>().size())>> : std::true_type {};

template <typename T, typename = void>
struct is_iterator_like : std::false_type {};

template <typename T>
struct is_iterator_like<T, void_t<decltype(*std::declval<T&>()), decltype(++std::declval<T&>()), decltype(std::declval<const T&>() != std::declval<const T&>())>>
    : std::integral_constant<bool, !std::is_integral<T>::value && !std::is_floating_point<T>::value> {};

template <typename T, typename = void>
struct is_random_access_like : std::false_type {};

template <typename T>
struct is_random_access_like<T, void_t<decltype(std::declval<const T&>() - std::declval<const T&>())>> : is_iterator_like<T> {};

template <typename T, typename = void>
struct is_queue_like : std::false_type {};

template <typename T>
struct is_queue_like<T, void_t<decltype(std::declval<T&>().front()), decltype(std::declval<T&>().pop()), decltype(std::declval<T&>().push(std::declval<typename T::value_type>())), typename T::container_type>>
    : std::true_type {};

template <typename T, typename = void>
struct is_priority_queue_like : std::false_type {};

template <typename T>
struct is_priority_queue_like<T, void_t<decltype(std::declval<T&>().top()), decltype(std::declval<T&>().pop()), typename T::value_compare, typename T::container_type>>
    : std::true_type {};

template <typename T, typename = void>
struct is_stack_like : std::false_type {};

template <typename T>
struct is_stack_like<T, void_t<decltype(std::declval<T&>().top()), decltype(std::declval<T&>().pop()), typename T::container_type>>
    : std::integral_constant<bool, !is_priority_queue_like<T>::value> {};

template <typename T>
struct is_adapter_like : std::integral_constant<bool, is_queue_like<T>::value || is_stack_like<T>::value || is_priority_queue_like<T>::value> {};

template <typename C, typename T, typename = void>
struct is_range_of : std::false_type {};

template <typename C, typename T>
struct is_range_of<C, T, void_t<decltype(*std::declval<const C&>().begin())>>
    : std::integral_constant<bool, has_begin_end<C>::value && std::is_constructible<T, decltype(*std::declval<const C&>().begin())>::value> {};

template <typename C, typename It, typename = void>
struct is_constructible_from_range : std::false_type {};

template <typename C, typename It>
struct is_constructible_from_range<C, It, void_t<decltype(C(std::declval<It>(), std::declval<It>()))>> : std::true_type {};

template <typename T, typename = void>
struct has_member_hash : std::false_type {};

template <typename T>
struct has_member_hash<T, void_t<decltype(static_cast<std::size_t>(std::declval<const T&>().hash()))>> : std::true_type {};

template <typename T, typename = void>
struct has_equality : std::false_type {};

template <typename T>
struct has_equality<T, void_t<decltype(static_cast<bool>(std::declval<const T&>() == std::declval<const T&>()))>> : std::true_type {};

template <typename T, typename = void>
struct has_less : std::false_type {};

template <typename T>
struct has_less<T, void_t<decltype(static_cast<bool>(std::declval<const T&>() < std::declval<const T&>()))>> : std::true_type {};

template <typename T, typename = void>
struct has_plus : std::false_type {};

template <typename T>
struct has_plus<T, void_t<decltype(std::declval<const T&>() + std::declval<const T&>())>> : std::true_type {};

template <typename T>
struct is_plain_number : std::integral_constant<bool, (std::is_integral<T>::value && !std::is_same<T, bool>::value) || std::is_floating_point<T>::value> {};

template <typename Cmp, typename T>
struct is_default_less : std::integral_constant<bool, std::is_same<Cmp, Less>::value> {};

template <typename Cmp, typename T>
struct is_default_greater : std::integral_constant<bool, std::is_same<Cmp, Greater>::value> {};

template <typename T>
FIZMO_ARRAY_INLINE void swap_values(T& a, T& b) noexcept(std::is_nothrow_move_constructible<T>::value && std::is_nothrow_move_assignable<T>::value) {
    T t(static_cast<T&&>(a));
    a = static_cast<T&&>(b);
    b = static_cast<T&&>(t);
}

template <typename T>
constexpr const T& min_of(const T& a, const T& b) noexcept { return b < a ? b : a; }

template <typename T>
constexpr const T& max_of(const T& a, const T& b) noexcept { return a < b ? b : a; }

} // namespace detail
} // namespace arrays
} // namespace fizmo

#endif // FIZMO_ARRAYS_TRAITS_HPP
