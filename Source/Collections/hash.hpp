#ifndef FIZMO_COLLECTIONS_HASH_HPP
#define FIZMO_COLLECTIONS_HASH_HPP

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>

namespace fizmo {
namespace collections {

inline std::uint64_t mix64(std::uint64_t x) noexcept {
    x ^= x >> 30;
    x *= 0xbf58476d1ce4e5b9ULL;
    x ^= x >> 27;
    x *= 0x94d049bb133111ebULL;
    x ^= x >> 31;
    return x;
}

inline std::size_t hash_combine(std::size_t seed, std::size_t value) noexcept {
    return static_cast<std::size_t>(mix64(static_cast<std::uint64_t>(seed) + 0x9e3779b97f4a7c15ULL + (static_cast<std::uint64_t>(value) << 6) + (static_cast<std::uint64_t>(value) >> 2)));
}

inline std::size_t hash_bytes(const void* data, std::size_t size) noexcept {
    const unsigned char* p = static_cast<const unsigned char*>(data);
    std::uint64_t h = 0xcbf29ce484222325ULL ^ static_cast<std::uint64_t>(size);

    while (size >= 8) {
        std::uint64_t k = 0;
        for (int i = 0; i < 8; ++i) k |= static_cast<std::uint64_t>(p[i]) << (8 * i);
        h = mix64(h ^ k) * 0x9e3779b97f4a7c15ULL;
        p += 8;
        size -= 8;
    }

    std::uint64_t tail = 0;
    for (std::size_t i = 0; i < size; ++i) tail |= static_cast<std::uint64_t>(p[i]) << (8 * i);
    return static_cast<std::size_t>(mix64(h ^ tail));
}

template <class T, class = void>
struct Hash {
    std::size_t operator()(const T& v) const noexcept(noexcept(std::hash<T>()(v))) {
        return static_cast<std::size_t>(mix64(static_cast<std::uint64_t>(std::hash<T>()(v))));
    }
};

template <>
struct Hash<std::string> {
    std::size_t operator()(const std::string& s) const noexcept { return hash_bytes(s.data(), s.size()); }
};

template <>
struct Hash<std::string_view> {
    std::size_t operator()(std::string_view s) const noexcept { return hash_bytes(s.data(), s.size()); }
};

template <>
struct Hash<const char*> {
    std::size_t operator()(const char* s) const noexcept { return s ? hash_bytes(s, std::char_traits<char>::length(s)) : 0; }
};

template <class A, class B>
struct Hash<std::pair<A, B>> {
    std::size_t operator()(const std::pair<A, B>& p) const { return hash_combine(Hash<A>()(p.first), Hash<B>()(p.second)); }
};

template <class... Ts>
struct Hash<std::tuple<Ts...>> {
    std::size_t operator()(const std::tuple<Ts...>& t) const { return apply(t, std::index_sequence_for<Ts...>()); }

private:
    template <std::size_t... I>
    static std::size_t apply(const std::tuple<Ts...>& t, std::index_sequence<I...>) {
        std::size_t h = sizeof...(Ts);
        (void)std::initializer_list<int>{ (h = hash_combine(h, Hash<typename std::tuple_element<I, std::tuple<Ts...>>::type>()(std::get<I>(t))), 0)... };
        return h;
    }
};

template <class T>
std::size_t hash_value(const T& v) { return Hash<T>()(v); }

} // namespace collections
} // namespace fizmo

#endif // FIZMO_COLLECTIONS_HASH_HPP
