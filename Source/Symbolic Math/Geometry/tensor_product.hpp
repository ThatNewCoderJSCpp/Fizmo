#ifndef FIZMO_SYMBOLIC_TENSOR_PRODUCT_HPP
#define FIZMO_SYMBOLIC_TENSOR_PRODUCT_HPP

#include "riemann_manifold.hpp"

namespace fizmo {
namespace math {
namespace tensors {

inline const SymbolicTensor& to_symbolic_tensor(const SymbolicTensor& t) { return t; }
inline SymbolicTensor to_symbolic_tensor(const SymbolicSquareTensor& t) { return t.tensor(); }
inline SymbolicTensor to_symbolic_tensor(const IndexedTensor& t) { return t.tensor().tensor(); }

template <typename T, typename = typename std::enable_if<is_symbolic_tensor_v<T>>::type, typename = decltype(std::declval<const T&>().raw())>
inline SymbolicTensor to_symbolic_tensor(const T& t) { return t.raw().inner(); }

template <typename T, typename = typename std::enable_if<is_symbolic_vector_v<T> || is_symbolic_matrix_v<T>>::type>
inline SymbolicTensor to_symbolic_tensor(const T& t) { return SymbolicTensor::from(t); }

template <
    typename A, typename B,
    typename = typename std::enable_if<
        (is_symbolic_vector_v<A> || is_symbolic_matrix_v<A> || is_symbolic_tensor_v<A>) &&
        (is_symbolic_vector_v<B> || is_symbolic_matrix_v<B> || is_symbolic_tensor_v<B>)
    >::type
>
inline SymbolicTensor tensor_product(const A& a, const B& b) {
    return SymbolicTensor::outer(to_symbolic_tensor(a), to_symbolic_tensor(b));
}

} // namespace tensors
} // namesoace math
} // namespace fizmo

#endif // FIZMO_SYMBOLIC_TENSOR_PRODUCT_HPP