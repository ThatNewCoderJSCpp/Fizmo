#ifndef FIZMO_ARRAYS_ALGORITHMS_HASH_HPP
#define FIZMO_ARRAYS_ALGORITHMS_HASH_HPP

#include "../memory/allocator.hpp"
#include "../traits.hpp"
#include <cstring>
#include <functional>

namespace fizmo {
namespace arrays {
namespace hashing {

FIZMO_ARRAY_INLINE std::uint64_t mix64(std::uint64_t x) noexcept {
    x ^= x >> 33;
    x *= 0xff51afd7ed558ccdULL;
    x ^= x >> 33;
    x *= 0xc4ceb9fe1a85ec53ULL;
    x ^= x >> 33;
    return x;
}

FIZMO_ARRAY_INLINE std::uint64_t combine(std::uint64_t seed, std::uint64_t h) noexcept {
    return mix64(seed ^ (h + 0x9e3779b97f4a7c15ULL + (seed << 6) + (seed >> 2)));
}

inline std::uint64_t bytes(const void* data, std::size_t len, std::uint64_t seed = 0x243f6a8885a308d3ULL) noexcept {
    const unsigned char* p = static_cast<const unsigned char*>(data);
    std::uint64_t h = seed ^ (static_cast<std::uint64_t>(len) * 0x9e3779b97f4a7c15ULL);
    while (len >= 8) {
        std::uint64_t k;
        std::memcpy(&k, p, 8);
        h = mix64(h ^ mix64(k));
        p += 8;
        len -= 8;
    }
    std::uint64_t tail = 0;
    if (len) std::memcpy(&tail, p, len);
    return mix64(h ^ tail ^ (static_cast<std::uint64_t>(len) << 56));
}

namespace detail {
template <typename T, typename = void>
struct has_std_hash : std::false_type {};

template <typename T>
struct has_std_hash<T, arrays::detail::void_t<decltype(std::hash<T>{}(std::declval<const T&>()))>> : std::true_type {};

template <typename T, typename = void>
struct has_first_second : std::false_type {};

template <typename T>
struct has_first_second<T, arrays::detail::void_t<decltype(std::declval<const T&>().first), decltype(std::declval<const T&>().second)>> : std::true_type {};

template <typename T, typename = void>
struct is_byte_string : std::false_type {};

template <typename T>
struct is_byte_string<T, arrays::detail::void_t<decltype(std::declval<const T&>().data()), decltype(std::declval<const T&>().size())>> {
    using E = typename std::remove_cv<typename std::remove_pointer<decltype(std::declval<const T&>().data())>::type>::type;
    static constexpr bool value = std::is_pointer<decltype(std::declval<const T&>().data())>::value && (std::is_integral<E>::value || std::is_enum<E>::value);
};
} // namespace detail

template <typename T, typename = void>
struct Hasher {
    static constexpr bool available = false;
};

template <typename T>
struct Hasher<T, typename std::enable_if<std::is_integral<T>::value || std::is_enum<T>::value || std::is_pointer<T>::value>::type> {
    static constexpr bool available = true;
    std::uint64_t operator()(const T& v) const noexcept {
        std::uint64_t u = 0;
        std::memcpy(&u, &v, sizeof(T) < 8 ? sizeof(T) : 8);
        return mix64(u);
    }
};

template <typename T>
struct Hasher<T, typename std::enable_if<std::is_floating_point<T>::value>::type> {
    static constexpr bool available = true;
    std::uint64_t operator()(const T& v) const noexcept {
        if (v == T(0)) return mix64(0);
        if (v != v) return mix64(0x7ff8000000000000ULL);
        return bytes(&v, sizeof(T));
    }
};

template <typename T>
struct Hasher<T, typename std::enable_if<!std::is_arithmetic<T>::value && !std::is_enum<T>::value && !std::is_pointer<T>::value && arrays::detail::has_member_hash<T>::value>::type> {
    static constexpr bool available = true;
    std::uint64_t operator()(const T& v) const noexcept { return mix64(static_cast<std::uint64_t>(v.hash())); }
};

template <typename T>
struct Hasher<T, typename std::enable_if<!std::is_arithmetic<T>::value && !std::is_enum<T>::value && !std::is_pointer<T>::value && !arrays::detail::has_member_hash<T>::value && detail::is_byte_string<T>::value>::type> {
    static constexpr bool available = true;
    std::uint64_t operator()(const T& v) const noexcept {
        using E = typename detail::is_byte_string<T>::E;
        return bytes(v.data(), static_cast<std::size_t>(v.size()) * sizeof(E));
    }
};

template <typename T>
struct Hasher<T, typename std::enable_if<!std::is_arithmetic<T>::value && !std::is_enum<T>::value && !std::is_pointer<T>::value && !arrays::detail::has_member_hash<T>::value && !detail::is_byte_string<T>::value && detail::has_first_second<T>::value>::type> {
    using A = typename std::decay<decltype(std::declval<const T&>().first)>::type;
    using B = typename std::decay<decltype(std::declval<const T&>().second)>::type;
    static constexpr bool available = Hasher<A>::available && Hasher<B>::available;
    std::uint64_t operator()(const T& v) const noexcept { return combine(Hasher<A>()(v.first), Hasher<B>()(v.second)); }
};

template <typename T>
struct Hasher<T, typename std::enable_if<!std::is_arithmetic<T>::value && !std::is_enum<T>::value && !std::is_pointer<T>::value && !arrays::detail::has_member_hash<T>::value && !detail::is_byte_string<T>::value && !detail::has_first_second<T>::value && detail::has_std_hash<T>::value>::type> {
    static constexpr bool available = true;
    std::uint64_t operator()(const T& v) const noexcept { return mix64(static_cast<std::uint64_t>(std::hash<T>{}(v))); }
};

template <typename T>
struct is_hashable : std::integral_constant<bool, Hasher<T>::available && arrays::detail::has_equality<T>::value> {};

template <typename T>
class IndexSet {
    struct Slot {
        std::uint64_t hash;
        std::size_t   index;
    };

    const T*    m_keys;
    Slot*       m_slots = nullptr;
    std::size_t m_mask = 0;
    std::size_t m_capacity = 0;
    std::size_t m_count = 0;

    static constexpr std::size_t kEmpty = static_cast<std::size_t>(-1);

    void rehash(std::size_t slots) {
        memory::Block<Slot> b = memory::allocate<Slot>(slots);
        for (std::size_t i = 0; i < slots; ++i) b.data[i].index = kEmpty;
        const std::size_t mask = slots - 1;
        for (std::size_t i = 0; i < m_capacity; ++i) {
            if (m_slots[i].index == kEmpty) continue;
            std::size_t p = static_cast<std::size_t>(m_slots[i].hash) & mask;
            while (b.data[p].index != kEmpty) p = (p + 1) & mask;
            b.data[p] = m_slots[i];
        }
        if (m_slots) memory::deallocate(m_slots, m_capacity);
        m_slots = b.data;
        m_capacity = slots;
        m_mask = mask;
    }

public:
    explicit IndexSet(const T* keys, std::size_t expected = 16) : m_keys(keys) {
        std::size_t slots = 16;
        while (slots < expected * 2) slots <<= 1;
        rehash(slots);
    }

    IndexSet(const IndexSet&) = delete;
    IndexSet& operator=(const IndexSet&) = delete;
    ~IndexSet() { if (m_slots) memory::deallocate(m_slots, m_capacity); }

    void reset_keys(const T* keys) noexcept { m_keys = keys; }
    std::size_t size() const noexcept { return m_count; }

    template <typename K>
    std::size_t find(const K& key, std::uint64_t h) const {
        std::size_t p = static_cast<std::size_t>(h) & m_mask;
        for (;;) {
            const Slot& s = m_slots[p];
            if (s.index == kEmpty) return npos;
            if (s.hash == h && m_keys[s.index] == key) return s.index;
            p = (p + 1) & m_mask;
        }
    }

    std::size_t insert(std::size_t index, std::uint64_t h) {
        if ((m_count + 1) * 4 > m_capacity * 3) rehash(m_capacity * 2);
        std::size_t p = static_cast<std::size_t>(h) & m_mask;
        for (;;) {
            Slot& s = m_slots[p];
            if (s.index == kEmpty) { s.hash = h; s.index = index; ++m_count; return npos; }
            if (s.hash == h && m_keys[s.index] == m_keys[index]) return s.index;
            p = (p + 1) & m_mask;
        }
    }

    void insert_new(std::size_t index, std::uint64_t h) {
        if ((m_count + 1) * 4 > m_capacity * 3) rehash(m_capacity * 2);
        std::size_t p = static_cast<std::size_t>(h) & m_mask;
        while (m_slots[p].index != kEmpty) p = (p + 1) & m_mask;
        m_slots[p].hash = h;
        m_slots[p].index = index;
        ++m_count;
    }
};

} // namespace hashing
} // namespace arrays
} // namespace fizmo

#endif // FIZMO_ARRAYS_ALGORITHMS_HASH_HPP
