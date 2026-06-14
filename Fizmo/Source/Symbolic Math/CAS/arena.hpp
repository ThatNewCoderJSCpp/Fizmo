#ifndef FIZMO_EXPRESSION_ARENA_ALLOCATOR_HPP
#define FIZMO_EXPRESSION_ARENA_ALLOCATOR_HPP

#include <cstddef>
#include <cstdint>
#include <new>
#include <vector>
#include <algorithm>

namespace fizmo {
namespace math {
namespace cas {
namespace detail {

class ExpressionArena {
public:
    explicit ExpressionArena(std::size_t block_size = 64 * 1024) : block_size_(block_size) { add_block(block_size_); }
    ExpressionArena(const ExpressionArena&) = delete;
    ExpressionArena& operator=(const ExpressionArena&) = delete;
    ExpressionArena(ExpressionArena&& other) noexcept : blocks_(std::move(other.blocks_)) {}

    ExpressionArena& operator=(ExpressionArena&& other) noexcept {
        if (this != &other) {
            destroy_all();
            blocks_ = std::move(other.blocks_);
        }
        return *this;
    }

    ~ExpressionArena() { destroy_all(); }

    void reset() {
        if (!blocks_.empty()) {
            Block& b = blocks_.front();
            b.ptr = b.begin;
            b.end = b.begin + b.capacity;
            blocks_.erase(blocks_.begin() + 1, blocks_.end());
        }
    }

    template <class T, class... Args>
    T* make(Args&&... args) {
        void* mem = allocate_raw(sizeof(T), alignof(T));
        return ::new (mem) T(std::forward<Args>(args)...);
    }

    template <class T>
    T* allocate_array(std::size_t n) {
        void* mem = allocate_raw(sizeof(T) * n, alignof(T));
        T* p = static_cast<T*>(mem);
        for (std::size_t i = 0; i < n; ++i) { ::new (p + i) T(); }
        return p;
    }

private:
    struct Block {
        std::uint8_t* begin;
        std::uint8_t* ptr;
        std::uint8_t* end;
        std::size_t   capacity;

        Block(std::size_t cap)
            : begin(static_cast<std::uint8_t*>(::operator new(cap))),
              ptr(begin),
              end(begin + cap),
              capacity(cap)
        {}

        ~Block() { ::operator delete(begin); }
    };

    std::size_t block_size_;
    std::vector<Block> blocks_;

    static std::uintptr_t align_up(std::uintptr_t p, std::size_t align) {
        const std::uintptr_t mask = align - 1;
        return (p + mask) & ~mask;
    }

    void* allocate_raw(std::size_t size, std::size_t align) {
        Block* b = &blocks_.back();
        std::uintptr_t p = reinterpret_cast<std::uintptr_t>(b->ptr);
        std::uintptr_t aligned = align_up(p, align);
        std::size_t padding = aligned - p;

        if (aligned + size > reinterpret_cast<std::uintptr_t>(b->end)) {
            std::size_t new_cap = std::max(block_size_, size + align);
            add_block(new_cap);
            b = &blocks_.back();
            p = reinterpret_cast<std::uintptr_t>(b->ptr);
            aligned = align_up(p, align);
            padding = aligned - p;
        }

        b->ptr = reinterpret_cast<std::uint8_t*>(aligned + size);
        return reinterpret_cast<void*>(aligned);
    }

    void add_block(std::size_t cap) { blocks_.emplace_back(cap); }
    void destroy_all() { blocks_.clear(); }
};

} // namespace detail
} // namespace cas
} // namespace math
} // namespace fizmo

#endif // FIZMO_EXPRESSION_ARENA_ALLOCATOR_HPP