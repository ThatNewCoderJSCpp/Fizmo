#ifndef FIZMO_ARRAYS_STREAM_HPP
#define FIZMO_ARRAYS_STREAM_HPP

#include "views/array_view.hpp"
#include <ostream>

namespace fizmo {

template <class T, class = void>
struct is_streamable : std::false_type {};

template <class T>
struct is_streamable<T, arrays::detail::void_t<decltype(std::declval<std::ostream&>() << std::declval<const T&>())>> : std::true_type {};

template <class T>
inline constexpr bool is_streamable_v = is_streamable<T>::value;

template <class T>
inline std::ostream& print_element(std::ostream& os, const T& value) {
    if constexpr (is_streamable_v<T>) os << value;
    else os << "OBJECT";
    return os;
}

namespace arrays {
namespace detail {

template <class T>
inline std::ostream& print_sequence(std::ostream& os, const char* open, const T* data, std::size_t n) {
    os << open;
    for (std::size_t i = 0; i < n; ++i) {
        os << "    " << i << ": ";
        print_element(os, data[i]);
        if (i + 1 < n) os << ",";
        os << "\n";
    }
    return os << "]";
}

} // namespace detail
} // namespace arrays

template <class T>
inline std::ostream& operator<<(std::ostream& os, const ArrayView<T>& view) { return arrays::detail::print_sequence(os, "ArrayView[\n", view.data(), view.size()); }

template <class T>
inline std::ostream& operator<<(std::ostream& os, const ArraySpan<T>& span) { return arrays::detail::print_sequence(os, "ArraySpan[\n", static_cast<const T*>(span.data()), span.size()); }

template <class T>
inline std::ostream& operator<<(std::ostream& os, const DynamicArray<T>& array) { return arrays::detail::print_sequence(os, "[\n", array.data(), array.size()); }

template <class T, std::size_t N>
inline std::ostream& operator<<(std::ostream& os, const SmallVector<T, N>& array) { return arrays::detail::print_sequence(os, "[\n", array.data(), array.size()); }

template <class T>
inline bool operator==(const DynamicArray<T>& a, const DynamicArray<T>& b) { return a.equals(b); }
template <class T>
inline bool operator!=(const DynamicArray<T>& a, const DynamicArray<T>& b) { return !a.equals(b); }
template <class T>
inline bool operator<(const DynamicArray<T>& a, const DynamicArray<T>& b) { return a.compare(b) < 0; }
template <class T>
inline bool operator<=(const DynamicArray<T>& a, const DynamicArray<T>& b) { return a.compare(b) <= 0; }
template <class T>
inline bool operator>(const DynamicArray<T>& a, const DynamicArray<T>& b) { return a.compare(b) > 0; }
template <class T>
inline bool operator>=(const DynamicArray<T>& a, const DynamicArray<T>& b) { return a.compare(b) >= 0; }

template <class T, std::size_t N, std::size_t M>
inline bool operator==(const SmallVector<T, N>& a, const SmallVector<T, M>& b) { return a.equals(b); }
template <class T, std::size_t N, std::size_t M>
inline bool operator!=(const SmallVector<T, N>& a, const SmallVector<T, M>& b) { return !a.equals(b); }
template <class T, std::size_t N, std::size_t M>
inline bool operator<(const SmallVector<T, N>& a, const SmallVector<T, M>& b) { return a.compare(b) < 0; }
template <class T, std::size_t N, std::size_t M>
inline bool operator<=(const SmallVector<T, N>& a, const SmallVector<T, M>& b) { return a.compare(b) <= 0; }
template <class T, std::size_t N, std::size_t M>
inline bool operator>(const SmallVector<T, N>& a, const SmallVector<T, M>& b) { return a.compare(b) > 0; }
template <class T, std::size_t N, std::size_t M>
inline bool operator>=(const SmallVector<T, N>& a, const SmallVector<T, M>& b) { return a.compare(b) >= 0; }

} // namespace fizmo

#endif // FIZMO_ARRAYS_STREAM_HPP
