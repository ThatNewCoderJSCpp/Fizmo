#ifndef FIZMO_MEMORY_ARENA_HPP
#define FIZMO_MEMORY_ARENA_HPP

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <functional>
#include <initializer_list>
#include <iterator>
#include <memory>
#include <mutex>
#include <new>
#include <stdexcept>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace fizmo {

class arena_error : public std::logic_error {
public:
    using std::logic_error::logic_error;
};

struct ArenaOptions {
    std::size_t block_size     = 64 * 1024;
    std::size_t max_block_size = 16 * 1024 * 1024;
    bool        thread_safe    = false;
};

struct ArenaStats {
    std::size_t   bytes_used     = 0;
    std::size_t   bytes_reserved = 0;
    std::size_t   blocks         = 0;
    std::size_t   objects        = 0;
    std::size_t   handles        = 0;
    std::uint64_t generation     = 0;
};

namespace detail {

class ArenaState {
public:
    struct Block {
        std::unique_ptr<unsigned char[]> data;
        std::size_t                      size = 0;
        std::size_t                      used = 0;
    };

    struct Destructor {
        void*       object;
        std::size_t count;
        void      (*destroy)(void*, std::size_t) noexcept;
    };

    explicit ArenaState(const ArenaOptions& o)
        : m_block_size(std::max<std::size_t>(o.block_size, 64)),
          m_max_block_size(std::max(o.max_block_size, std::max<std::size_t>(o.block_size, 64))),
          m_next_size(m_block_size),
          m_thread_safe(o.thread_safe) {}

    ArenaState(const ArenaState&) = delete;
    ArenaState& operator=(const ArenaState&) = delete;
    ~ArenaState() { destroy_objects(); }

    std::uint64_t generation() const noexcept { return m_generation.load(std::memory_order_acquire); }
    bool thread_safe() const noexcept { return m_thread_safe; }

    void* allocate(std::size_t size, std::size_t align) {
        if (align == 0 || (align & (align - 1)) != 0) throw arena_error("Arena alignment must be a power of two");
        if (size == 0) size = 1;
        Lock lock(*this);
        return allocate_unlocked(size, align);
    }

    void add_destructor(void* object, std::size_t count, void (*destroy)(void*, std::size_t) noexcept) {
        Lock lock(*this);
        m_destructors.push_back(Destructor{ object, count, destroy });
        m_objects += count;
    }

    void count_objects(std::size_t n) {
        Lock lock(*this);
        m_objects += n;
    }

    void reset() {
        Lock lock(*this);
        destroy_objects();
        for (Block& b : m_blocks) b.used = 0;
        m_current = 0;
        m_objects = 0;
        m_generation.fetch_add(1, std::memory_order_acq_rel);
    }

    void shrink_to_fit() {
        Lock lock(*this);
        std::size_t keep = 0;
        for (std::size_t i = 0; i < m_blocks.size(); ++i) if (m_blocks[i].used > 0) keep = i + 1;
        m_blocks.resize(keep);
        if (m_current >= m_blocks.size()) m_current = m_blocks.empty() ? 0 : m_blocks.size() - 1;
        m_next_size = m_block_size;
    }

    void reserve(std::size_t bytes) {
        Lock lock(*this);
        std::size_t free_bytes = 0;
        for (std::size_t i = m_current; i < m_blocks.size(); ++i) free_bytes += m_blocks[i].size - m_blocks[i].used;
        if (free_bytes >= bytes) return;
        add_block(bytes - free_bytes);
    }

    bool owns(const void* p) const noexcept {
        Lock lock(*this);
        const auto* c = static_cast<const unsigned char*>(p);
        for (const Block& b : m_blocks) if (c >= b.data.get() && c < b.data.get() + b.used) return true;
        return false;
    }

    ArenaStats stats() const {
        Lock lock(*this);
        ArenaStats s;
        for (const Block& b : m_blocks) { s.bytes_used += b.used; s.bytes_reserved += b.size; }
        s.blocks = m_blocks.size();
        s.objects = m_objects;
        s.generation = generation();
        return s;
    }

private:
    class Lock {
        const ArenaState& m_s;

    public:
        explicit Lock(const ArenaState& s) : m_s(s) { if (m_s.m_thread_safe) m_s.m_mutex.lock(); }
        ~Lock() { if (m_s.m_thread_safe) m_s.m_mutex.unlock(); }
        Lock(const Lock&) = delete;
        Lock& operator=(const Lock&) = delete;
    };

    std::vector<Block>         m_blocks;
    std::vector<Destructor>    m_destructors;
    std::size_t                m_current = 0;
    std::size_t                m_block_size;
    std::size_t                m_max_block_size;
    std::size_t                m_next_size;
    std::size_t                m_objects = 0;
    std::atomic<std::uint64_t> m_generation{ 1 };
    bool                       m_thread_safe;
    mutable std::mutex         m_mutex;

    static bool fits(const Block& b, std::size_t size, std::size_t align, std::size_t& offset) noexcept {
        const std::uintptr_t base = reinterpret_cast<std::uintptr_t>(b.data.get());
        const std::uintptr_t at = base + b.used;
        const std::uintptr_t aligned = (at + (align - 1)) & ~static_cast<std::uintptr_t>(align - 1);
        offset = static_cast<std::size_t>(aligned - base);
        return offset <= b.size && size <= b.size - offset;
    }

    void add_block(std::size_t min_bytes) {
        std::size_t size = std::max(m_next_size, min_bytes);
        Block b;
        b.data.reset(new unsigned char[size]);
        b.size = size;
        m_blocks.push_back(std::move(b));
        m_next_size = std::min(m_max_block_size, m_next_size * 2);
    }

    void* allocate_unlocked(std::size_t size, std::size_t align) {
        std::size_t offset = 0;
        const bool big = size + align > m_block_size / 4;
        for (std::size_t i = m_current; i < m_blocks.size(); ++i) {
            Block& b = m_blocks[i];
            if (fits(b, size, align, offset)) { b.used = offset + size; return b.data.get() + offset; }
            if (i == m_current && !big) ++m_current;
        }
        add_block(size + align);
        Block& b = m_blocks.back();
        if (!fits(b, size, align, offset)) throw std::bad_alloc();
        b.used = offset + size;
        if (!big) m_current = m_blocks.size() - 1;
        return b.data.get() + offset;
    }

    void destroy_objects() noexcept {
        while (!m_destructors.empty()) {
            const Destructor d = m_destructors.back();
            m_destructors.pop_back();
            d.destroy(d.object, d.count);
        }
    }
};

template <typename T>
inline void destroy_n(void* p, std::size_t n) noexcept {
    T* t = static_cast<T*>(p);
    for (std::size_t i = n; i-- > 0;) t[i].~T();
}

} // namespace detail

template <typename T>
class ArenaPtr;

template <typename T>
class ArenaArray;

template <typename T>
class ArenaAllocator;

class Arena {
    template <typename U> friend class ArenaPtr;
    template <typename U> friend class ArenaAllocator;

    mutable std::shared_ptr<detail::ArenaState> m_state;
    ArenaOptions                                m_options;

    detail::ArenaState& state() const {
        if (!m_state) m_state = std::make_shared<detail::ArenaState>(m_options);
        return *m_state;
    }

    template <typename T>
    void track(T* p, std::size_t n) {
        if (std::is_trivially_destructible<T>::value) state().count_objects(n);
        else state().add_destructor(p, n, &detail::destroy_n<T>);
    }

public:
    Arena() : m_state(std::make_shared<detail::ArenaState>(ArenaOptions())), m_options() {}
    explicit Arena(std::size_t block_size) { m_options.block_size = block_size; m_state = std::make_shared<detail::ArenaState>(m_options); }
    explicit Arena(const ArenaOptions& options) : m_state(std::make_shared<detail::ArenaState>(options)), m_options(options) {}
    Arena(const Arena&) = delete;
    Arena& operator=(const Arena&) = delete;
    Arena(Arena&& o) noexcept : m_state(std::move(o.m_state)), m_options(o.m_options) {}
    Arena& operator=(Arena&& o) noexcept { if (this != &o) { m_state = std::move(o.m_state); m_options = o.m_options; } return *this; }
    ~Arena() = default;

    void* allocate(std::size_t bytes, std::size_t align = alignof(std::max_align_t)) { return state().allocate(bytes, align); }

    template <typename T, typename... Args>
    T* create(Args&&... args) {
        void* mem = state().allocate(sizeof(T), alignof(T));
        T* p = ::new (mem) T(std::forward<Args>(args)...);
        track(p, 1);
        return p;
    }

    template <typename T>
    T* create_array(std::size_t n) {
        if (n == 0) return nullptr;
        if (n > static_cast<std::size_t>(-1) / sizeof(T)) throw std::bad_alloc();
        T* p = static_cast<T*>(state().allocate(sizeof(T) * n, alignof(T)));
        std::size_t i = 0;
        try { for (; i < n; ++i) ::new (p + i) T(); } catch (...) { detail::destroy_n<T>(p, i); throw; }
        track(p, n);
        return p;
    }

    template <typename T>
    T* create_array(std::size_t n, const T& fill) {
        if (n == 0) return nullptr;
        if (n > static_cast<std::size_t>(-1) / sizeof(T)) throw std::bad_alloc();
        T* p = static_cast<T*>(state().allocate(sizeof(T) * n, alignof(T)));
        std::size_t i = 0;
        try { for (; i < n; ++i) ::new (p + i) T(fill); } catch (...) { detail::destroy_n<T>(p, i); throw; }
        track(p, n);
        return p;
    }

    template <typename It, typename T = typename std::iterator_traits<It>::value_type>
    T* create_array(It first, It last) {
        const std::size_t n = static_cast<std::size_t>(std::distance(first, last));
        if (n == 0) return nullptr;
        T* p = static_cast<T*>(state().allocate(sizeof(T) * n, alignof(T)));
        std::size_t i = 0;
        try { for (; first != last; ++first, ++i) ::new (p + i) T(*first); } catch (...) { detail::destroy_n<T>(p, i); throw; }
        track(p, n);
        return p;
    }

    template <typename T, typename... Args>
    ArenaPtr<T> make(Args&&... args);

    template <typename T>
    ArenaArray<T> make_array(std::size_t n);

    template <typename T>
    ArenaArray<T> make_array(std::size_t n, const T& fill);

    template <typename T>
    ArenaArray<T> make_array(std::initializer_list<T> values);

    template <typename It, typename T = typename std::iterator_traits<It>::value_type>
    ArenaArray<T> make_array(It first, It last);

    template <typename T>
    ArenaPtr<T> adopt(T* p);

    template <typename T>
    ArenaAllocator<T> allocator() const;

    std::string_view copy_string(std::string_view s) {
        char* p = static_cast<char*>(state().allocate(s.size() + 1, 1));
        if (!s.empty()) std::memcpy(p, s.data(), s.size());
        p[s.size()] = '\0';
        return std::string_view(p, s.size());
    }

    const char* copy_c_string(std::string_view s) { return copy_string(s).data(); }

    void reset() { if (m_state) m_state->reset(); }
    void clear() { reset(); }
    void shrink_to_fit() { if (m_state) m_state->shrink_to_fit(); }
    void reserve(std::size_t bytes) { state().reserve(bytes); }
    bool owns(const void* p) const noexcept { return p && m_state && m_state->owns(p); }

    ArenaStats stats() const {
        if (!m_state) { ArenaStats s; s.generation = 1; return s; }
        ArenaStats s = m_state->stats();
        s.handles = static_cast<std::size_t>(std::max(0L, m_state.use_count() - 1));
        return s;
    }

    std::size_t bytes_used() const { return stats().bytes_used; }
    std::size_t bytes_reserved() const { return stats().bytes_reserved; }
    std::size_t object_count() const { return stats().objects; }
    std::size_t handle_count() const { return stats().handles; }
    std::uint64_t generation() const noexcept { return m_state ? m_state->generation() : 1; }
    bool thread_safe() const noexcept { return m_options.thread_safe; }
    const ArenaOptions& options() const noexcept { return m_options; }
};

template <typename T>
class ArenaPtr {
    template <typename U> friend class ArenaPtr;
    template <typename U> friend class ArenaArray;
    friend class Arena;

    T*                                  m_ptr = nullptr;
    std::shared_ptr<detail::ArenaState> m_state;
    std::uint64_t                       m_generation = 0;

    ArenaPtr(T* p, std::shared_ptr<detail::ArenaState> s, std::uint64_t g) noexcept : m_ptr(p), m_state(std::move(s)), m_generation(g) {}

    bool alive() const noexcept { return m_ptr && m_state && m_state->generation() == m_generation; }

    T* checked() const {
        if (!m_ptr) throw arena_error("ArenaPtr is null");
        if (!alive()) throw arena_error("ArenaPtr used after its arena was reset");
        return m_ptr;
    }

public:
    using element_type = T;

    constexpr ArenaPtr() noexcept = default;
    constexpr ArenaPtr(std::nullptr_t) noexcept {}

    template <typename U, typename = typename std::enable_if<std::is_convertible<U*, T*>::value>::type>
    ArenaPtr(const ArenaPtr<U>& o) noexcept : m_ptr(o.m_ptr), m_state(o.m_state), m_generation(o.m_generation) {}

    template <typename U, typename = typename std::enable_if<std::is_convertible<U*, T*>::value>::type>
    ArenaPtr(ArenaPtr<U>&& o) noexcept : m_ptr(o.m_ptr), m_state(std::move(o.m_state)), m_generation(o.m_generation) { o.m_ptr = nullptr; }

    template <typename U>
    ArenaPtr(const ArenaPtr<U>& owner, T* p) noexcept : m_ptr(p), m_state(owner.m_state), m_generation(owner.m_generation) {}

    T* get() const noexcept { return alive() ? m_ptr : nullptr; }
    T* get_unchecked() const noexcept { return m_ptr; }
    T& operator*() const { return *checked(); }
    T* operator->() const { return checked(); }
    explicit operator bool() const noexcept { return alive(); }

    bool expired() const noexcept { return m_ptr && !alive(); }
    void reset() noexcept { m_ptr = nullptr; m_state.reset(); m_generation = 0; }
    long use_count() const noexcept { return m_state ? m_state.use_count() : 0; }
    std::uint64_t generation() const noexcept { return m_generation; }

    template <typename U>
    bool same_arena(const ArenaPtr<U>& o) const noexcept { return m_state && m_state == o.m_state; }
    bool from(const Arena& a) const noexcept { return m_state && m_state == a.m_state; }

    void swap(ArenaPtr& o) noexcept { std::swap(m_ptr, o.m_ptr); m_state.swap(o.m_state); std::swap(m_generation, o.m_generation); }

    template <typename U>
    bool operator==(const ArenaPtr<U>& o) const noexcept { return get() == o.get(); }
    template <typename U>
    bool operator!=(const ArenaPtr<U>& o) const noexcept { return get() != o.get(); }
    template <typename U>
    bool operator<(const ArenaPtr<U>& o) const noexcept { return std::less<const void*>()(get(), o.get()); }
    bool operator==(std::nullptr_t) const noexcept { return get() == nullptr; }
    bool operator!=(std::nullptr_t) const noexcept { return get() != nullptr; }

    template <typename U, typename V>
    friend ArenaPtr<U> static_pointer_cast(const ArenaPtr<V>& p) noexcept;
    template <typename U, typename V>
    friend ArenaPtr<U> dynamic_pointer_cast(const ArenaPtr<V>& p) noexcept;
    template <typename U, typename V>
    friend ArenaPtr<U> const_pointer_cast(const ArenaPtr<V>& p) noexcept;
};

template <typename U, typename V>
inline ArenaPtr<U> static_pointer_cast(const ArenaPtr<V>& p) noexcept { return ArenaPtr<U>(static_cast<U*>(p.m_ptr), p.m_state, p.m_generation); }

template <typename U, typename V>
inline ArenaPtr<U> dynamic_pointer_cast(const ArenaPtr<V>& p) noexcept {
    U* u = dynamic_cast<U*>(p.get());
    return u ? ArenaPtr<U>(u, p.m_state, p.m_generation) : ArenaPtr<U>();
}

template <typename U, typename V>
inline ArenaPtr<U> const_pointer_cast(const ArenaPtr<V>& p) noexcept { return ArenaPtr<U>(const_cast<U*>(p.m_ptr), p.m_state, p.m_generation); }

template <typename T>
class ArenaArray {
    friend class Arena;

    T*                                  m_data = nullptr;
    std::size_t                         m_size = 0;
    std::shared_ptr<detail::ArenaState> m_state;
    std::uint64_t                       m_generation = 0;

    ArenaArray(T* p, std::size_t n, std::shared_ptr<detail::ArenaState> s, std::uint64_t g) noexcept : m_data(p), m_size(n), m_state(std::move(s)), m_generation(g) {}

    bool alive() const noexcept { return m_state && m_state->generation() == m_generation; }

    T* checked() const {
        if (!alive()) throw arena_error(m_state ? "ArenaArray used after its arena was reset" : "ArenaArray is empty");
        return m_data;
    }

public:
    using value_type = T;
    using iterator = T*;
    using const_iterator = const T*;

    ArenaArray() noexcept = default;

    std::size_t size() const noexcept { return alive() ? m_size : 0; }
    bool empty() const noexcept { return size() == 0; }
    explicit operator bool() const noexcept { return alive(); }
    bool expired() const noexcept { return m_state && !alive(); }
    T* data() const noexcept { return alive() ? m_data : nullptr; }

    T& operator[](std::size_t i) const {
        T* d = checked();
        if (i >= m_size) throw std::out_of_range("ArenaArray index out of range");
        return d[i];
    }

    T& at(std::size_t i) const { return (*this)[i]; }
    T& front() const { return (*this)[0]; }
    T& back() const { return (*this)[m_size ? m_size - 1 : 0]; }
    T* begin() const { return m_size ? checked() : m_data; }
    T* end() const { return m_size ? checked() + m_size : m_data; }

    ArenaPtr<T> element(std::size_t i) const {
        T* d = checked();
        if (i >= m_size) throw std::out_of_range("ArenaArray index out of range");
        return ArenaPtr<T>(d + i, m_state, m_generation);
    }

    void reset() noexcept { m_data = nullptr; m_size = 0; m_state.reset(); m_generation = 0; }
};

template <typename T>
class ArenaAllocator {
    template <typename U> friend class ArenaAllocator;

    std::shared_ptr<detail::ArenaState> m_state;

public:
    using value_type = T;
    using propagate_on_container_copy_assignment = std::true_type;
    using propagate_on_container_move_assignment = std::true_type;
    using propagate_on_container_swap = std::true_type;

    explicit ArenaAllocator(const Arena& a) : m_state((a.state(), a.m_state)) {}
    template <typename U>
    ArenaAllocator(const ArenaAllocator<U>& o) noexcept : m_state(o.m_state) {}

    T* allocate(std::size_t n) {
        if (n > static_cast<std::size_t>(-1) / sizeof(T)) throw std::bad_alloc();
        return static_cast<T*>(m_state->allocate(sizeof(T) * n, alignof(T)));
    }

    void deallocate(T*, std::size_t) noexcept {}

    template <typename U>
    bool operator==(const ArenaAllocator<U>& o) const noexcept { return m_state == o.m_state; }
    template <typename U>
    bool operator!=(const ArenaAllocator<U>& o) const noexcept { return m_state != o.m_state; }
};

template <typename T, typename... Args>
inline ArenaPtr<T> Arena::make(Args&&... args) {
    T* p = create<T>(std::forward<Args>(args)...);
    return ArenaPtr<T>(p, m_state, m_state->generation());
}

template <typename T>
inline ArenaArray<T> Arena::make_array(std::size_t n) {
    T* p = create_array<T>(n);
    return ArenaArray<T>(p, n, (state(), m_state), m_state->generation());
}

template <typename T>
inline ArenaArray<T> Arena::make_array(std::size_t n, const T& fill) {
    T* p = create_array<T>(n, fill);
    return ArenaArray<T>(p, n, (state(), m_state), m_state->generation());
}

template <typename T>
inline ArenaArray<T> Arena::make_array(std::initializer_list<T> values) {
    return make_array(values.begin(), values.end());
}

template <typename It, typename T>
inline ArenaArray<T> Arena::make_array(It first, It last) {
    const std::size_t n = static_cast<std::size_t>(std::distance(first, last));
    T* p = create_array(first, last);
    return ArenaArray<T>(p, n, (state(), m_state), m_state->generation());
}

template <typename T>
inline ArenaPtr<T> Arena::adopt(T* p) {
    if (!p) return ArenaPtr<T>();
    if (!owns(p)) throw arena_error("Arena::adopt: pointer was not allocated by this arena");
    return ArenaPtr<T>(p, m_state, m_state->generation());
}

template <typename T>
inline ArenaAllocator<T> Arena::allocator() const { return ArenaAllocator<T>(*this); }

} // namespace fizmo

namespace std {

template <typename T>
struct hash<fizmo::ArenaPtr<T>> {
    std::size_t operator()(const fizmo::ArenaPtr<T>& p) const noexcept { return std::hash<const void*>()(p.get()); }
};

} // namespace std

#endif // FIZMO_MEMORY_ARENA_HPP
