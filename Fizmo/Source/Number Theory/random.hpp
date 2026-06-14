#ifndef RANDOM_HPP
#define RANDOM_HPP

#include <random>
#include <type_traits>
#include <chrono>
#include "../Util Hpp/util_functions.hpp"

namespace fizmo {
namespace random {

template <typename T, typename = std::enable_if_t<std::is_integral_v<T>>>
T random_integer(const T min, const T max) noexcept {
    if (max < min) { return random_integer(max, min); }
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(min, max);
    return dis(gen);
}

template <typename T, typename = std::enable_if_t<std::is_floating_point_v<T>>>
T random_floater(const T min, const T max) noexcept {
    if (max < min) { return random_floater(max, min); }
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<> dis(min, max);
    return dis(gen);
}

bool flip_coin(const double percentage, const bool is_normalized = true) noexcept {
    const double prob = fizmo::clamp(percentage, 0.0, is_normalized ? 1.0 : 100.0);
    std::random_device rd;
    std::mt19937 gen(rd());
    std::bernoulli_distribution d(is_normalized ? prob : prob / 100);
    return d(gen);
}

std::string random_string(const unsigned int length) noexcept {
    const std::string characters = "`1234567890-=qwertyuiop[]\\asdfghjkl;'zxcvbnm,./~!@#$%^&*()_+QWERTYUIOP{}|ASDFGHJKL:\"ZXCVBNM<>?";
    std::random_device rd;
    std::mt19937 rng(rd());
    std::uniform_int_distribution<> dist(0, characters.size() - 1);
    std::string result = "";
    for (unsigned int i = 0; i < length; ++i) { result += characters[dist(rng)]; }
    return result;
}

std::string random_string(const unsigned int min_length, const unsigned int max_length) noexcept { return random_string(random_integer<unsigned int>(min_length, max_length)); }
std::string random_string() noexcept { return random_string(0, std::numeric_limits<unsigned int>::max()); }

} // namespace random
} // namespace fizmo

#endif