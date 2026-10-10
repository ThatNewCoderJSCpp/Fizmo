#ifndef FIZMO_EXPRESSION_ARENA_ALLOCATOR_HPP
#define FIZMO_EXPRESSION_ARENA_ALLOCATOR_HPP

#include "../../Memory/arena.hpp"

namespace fizmo {
namespace math {
namespace cas {
namespace detail {

class ExpressionArena : public fizmo::Arena {
public:
    explicit ExpressionArena(std::size_t block_size = 64 * 1024) : fizmo::Arena(block_size) {}

    template <class T, class... Args>
    T* make(Args&&... args) { return create<T>(std::forward<Args>(args)...); }

    template <class T>
    T* allocate_array(std::size_t n) { return create_array<T>(n); }
};

} // namespace detail
} // namespace cas
} // namespace math
} // namespace fizmo

#endif // FIZMO_EXPRESSION_ARENA_ALLOCATOR_HPP
