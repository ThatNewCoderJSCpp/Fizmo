#ifndef STIELTJES_CONSTANTS_HPP
#define STIELTJES_CONSTANTS_HPP

#include <cmath>
#include <limits>
#include <type_traits>
#include <cstdint>
#include "../Standard Overloads/log.hpp"
#include "../Standard Overloads/pow.hpp"

namespace fizmo {
namespace constants {

template <typename T, typename = typename std::enable_if<std::is_integral<T>::value>::type>
constexpr long double stieltjes_constant(T n_) noexcept {
    if (n_ < 0) { return std::numeric_limits<double>::quiet_NaN(); }
    const std::uint64_t n = static_cast<std::uint64_t>(n_);

    switch (n) {
        case 0: return 0.57721566490153286060651209008240243104215933593992359880576723488486772677L;
        case 1: return -0.0728158454836767248605863758749013191377363383343379525990065597414014335L;
        case 2: return -0.0096903631928723184845303860352125293590658061013407498807013654518507553L;
        case 3: return 0.00205383442030334586616004654275338428571580444541061824548148333691383449L;
        case 4: return 0.00232537006546730005746817017752606800090446941378485099075804090712484100L;
        case 5: return 0.00079332381730106270175333487744444483073153940458488707573425626982314821L;
        case 6: return -0.0002387693454301996098724218419080042777837151563580786314764253073910675L;
        case 7: return -0.0005272895670577510460740975054788582819962534729698953310134042268856827L;
        case 8: return -0.0003521233538030395096020521650012087417291805337923503566573315073642817L;
        case 9: return -0.0000343947744180880481779146237982273906207895385944416297592919048431501L;
        case 10: return 0.00020533281490906479468372228923706530295985377416676430384020871435300902L;
        case 11: return 0.00027018443954390352667290208206795567382784205868840250397373580313679999L;
        case 12: return 0.00016727291210514019335350154334118344660780663280556582804779093765121959L;
        case 13: return -0.0000274638066037601588600076036933551815267853376703955360928330891675705L;
        case 14: return -0.0002092092620592999458371396973445849578315442115060695624342083257187577L;
        case 15: return -0.0002834686553202414466429344749971269770687029807176752539699432929676256L;
    }

    long double sum = 0.0L;
    long double prev_sum = 0.0L;
    long double term = 0.0L;
    std::uint64_t index = 0;

    for (std::uint64_t k = 1; k < std::numeric_limits<std::uint64_t>::max(); ++k) {
        prev_sum = sum;
        term = math::pow_constexpr(math::log_constexpr(k), n) / k;
        sum += term;

        if (abs_constexpr(prev_sum - sum) <= constants::TYPE_EPSILON<long double> || abs_constexpr(term) <= constants::TYPE_EPSILON<long double>) { 
            index = k;
            break; 
        }
    }

    constexpr long double ln_uint64_max = 44.3614195558364998027L;
    const std::uint64_t np1 = n + 1;
    return sum - math::pow_constexpr(math::log_constexpr(index), np1) / np1;
}

} // namespace constants
} // namespace fizmo

#endif // STIELTJES_CONSTANTS_HPP