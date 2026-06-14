#ifndef FIZMO_BASIC_INCLUDES_HPP
#define FIZMO_BASIC_INCLUDES_HPP

#include "fizmo_defines.hpp"

#include <iostream>
#include <complex>
#include <string>
#include <cstring>
#include <limits.h>
#include <cstdlib>
#include <cmath>
#include <ctime>
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>
#include <map>
#include <vector>
#include <functional>
#include <locale>
#include <cctype>
#include <regex>
#include <fstream>
#include <cstdint>
#include <chrono>
#include <random>
#include <numeric>
#include <thread>
#include <unordered_set>
#include <list>
#include <memory>

#if CPP14_OR_GREATER
    #include <shared_mutex>
#endif

#if CPP17_OR_GREATER
    #include <optional>
    #include <variant>
    #include <filesystem>
    #include <string_view>
#endif

#if CPP20_OR_GREATER
    #include <bit>
    #include <compare>
    #include <concepts>
    #include <numbers>
    #include <ranges>
    #include <span>
    #include <syncstream>
    #include <barrier>
    #include <latch>
    #include <semaphore>
    #include <coroutine>
    #include <source_location>
    #if __has_include(<format>)
        #include <format>
    #endif
#endif

#if CPP24_OR_GREATER
    #if __has_include(<expected>)
        #include <expected>
    #endif
    #if __has_include(<spanstream>)
        #include <spanstream>
    #endif
    #if __has_include(<flat_map>)
        #include <flat_map>
    #endif
    #if __has_include(<flat_set>)
        #include <flat_set>
    #endif
    #if __has_include(<generator>)
        #include <generator>
    #endif
    #if __has_include(<mdspan>)
        #include <mdspan>
    #endif
    #if __has_include(<print>)
        #include <print>
    #endif
    #if __has_include(<stdfloat>)
        #include <stdfloat>
    #endif
#endif

#endif // FIZMO_BASIC_INCLUDES_HPP