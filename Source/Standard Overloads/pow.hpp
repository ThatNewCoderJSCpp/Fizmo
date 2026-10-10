#ifndef FIZMO_CONSTEXPR_POWERS_HPP
#define FIZMO_CONSTEXPR_POWERS_HPP

#include "abs.hpp"
#include "log.hpp"
#include "num_theory.hpp"
#include "sqrt.hpp"
#include <cstdint>

namespace fizmo {
namespace math {

template <typename T>
constexpr typename std::enable_if<std::is_floating_point<T>::value, T>::type
exp_constexpr(const T x) noexcept {
    if (isnan_constexpr(x)) { return std::numeric_limits<T>::quiet_NaN(); }
    if (is_positive_infinity(x)) { return std::numeric_limits<T>::infinity(); }
    if (is_negative_infinity(x)) { return static_cast<T>(0); }
    if (x == static_cast<T>(0)) { return static_cast<T>(1); }
    if (x == static_cast<T>(1)) { return fizmo::constants::EULER<T>; }
    if (x < 0) { return 1 / exp_constexpr(-x); }
    T result = static_cast<T>(1);
    T term = static_cast<T>(1);  
    
    if (x < static_cast<T>(12)) { 
        for (std::size_t i = 1; i < std::numeric_limits<std::size_t>::max(); ++i) {
            term *= x / i;    
            result += term;   
            if (abs_constexpr(term) < std::numeric_limits<T>::epsilon() * (abs_constexpr(result) + 1)) { break; }
        }
    } else {
        constexpr T ln2 = static_cast<T>(0.69314718055994530738446847324180635261535644531250);
        constexpr T inv_ln2 = static_cast<T>(1.44269504088896340735992468100189213752664595415298);
        const std::size_t k = static_cast<std::size_t>(x * inv_ln2);
        const T r = x - k * ln2;

        for (std::size_t i = 1; i < std::numeric_limits<std::size_t>::max(); ++i) {
            term *= r / i;
            result += term;
            if (abs_constexpr(term) < std::numeric_limits<T>::epsilon() * (abs_constexpr(result) + 1)) { break; }
        }

        for (std::size_t i = 0; i < k; ++i) { result *= 2; }
    }

    return result;
}

template <typename T>
constexpr typename std::enable_if<std::is_integral<T>::value, double>::type
exp_constexpr(const T x) noexcept {
    switch (static_cast<std::int64_t>(x)) {
        case -12: return 6.14421235332820975868230817880553231122398931488825297556850587649988e-6;
        case -11: return 1.670170079024565931263551736058087907793804695928712447812473711352395e-5;
        case -10: return 4.539992976248485153559151556055061023791808886656496925907130565099942e-5;
        case -9: return 1.2340980408667954949763669073003382607215283228893905253448204514517628e-4;
        case -8: return 3.3546262790251183882138912578086101931090013372031936054457574791164052e-4;
        case -7: return 9.1188196555451620800313608440928262647372452743605384081613342188947988e-4;
        case -6: return 2.47875217666635842304516743081666789150647958553394505087862400627619422e-3;
        case -5: return 6.73794699908546709663604842314842424884958502735508543030553157268352251e-3;
        case -4: return 0.01831563888873418029371802127324124221191206755347559476959992743925043159;
        case -3: return 0.04978706836786394297934241565006177663169959218842321556762772760606066773;
        case -2: return 0.13533528323661269189399949497248440340763154590957588146815887265407337410;
        case -1: return 0.36787944117144232159552377016146086744581113103176783450783680169746149574;
        case 0: return 1;
        case 1: return 2.71828182845904523536028747135266249775724709369995957496696762772407663035;
        case 2: return 7.38905609893065022723042746057500781318031557055184732408712782252257379607;
        case 3: return 20.0855369231876677409285296545817178969879078385541501443789342296988458780;
        case 4: return 54.5981500331442390781102612028608784027907370386140687258265939585536620999;
        case 5: return 148.413159102576603421115580040552279623487667593878989046752845110912064820;
        case 6: return 403.428793492735122608387180543388279605899897357129202613967188325151180633;
        case 7: return 1096.63315842845859926372023828812143244221913483361314378273924077612176933;
        case 8: return 2980.95798704172827474359209945288867375596793913283570220896353038773072517;
        case 9: return 8103.08392757538400770999668943275996501147608783161346250015905217827251569;
        case 10: return 22026.4657948067165169579006452842443663535126185567810742354263552252028185;
        case 11: return 59874.1417151978184553264857922577816142610796957409686527715143799324211159;
        case 12: return 162754.791419003920808005204898486783170209284478720770443556248138596770835;
    }

    return exp_constexpr(static_cast<double>(x));
}

template <typename RET = double, typename T, typename E>
constexpr typename std::enable_if<std::is_integral<T>::value && std::is_integral<E>::value && std::is_floating_point<RET>::value, RET>::type
pow_constexpr(const T b, E exp) noexcept {
    if (b == 0) { return exp <= 0 ? std::numeric_limits<RET>::quiet_NaN() : 0; }
    if (exp < 0) { return RET(1) / pow_constexpr<RET>(b, -exp); }
    RET base = static_cast<RET>(b);

    switch (exp) {
        case 0: return 1;
        case 1: return base;
        case 2: return base * base;
        case 3: return base * base * base;
        case 4: return base * base * base * base;
        case 5: return base * base * base * base * base;
        case 6: return base * base * base * base * base * base;
        case 7: return base * base * base * base * base * base * base;
        case 8: return base * base * base * base * base * base * base * base;
        case 9: return base * base * base * base * base * base * base * base * base;
        case 10: return base * base * base * base * base * base * base * base * base * base;
        case 11: return base * base * base * base * base * base * base * base * base * base * base;
        case 12: return base * base * base * base * base * base * base * base * base * base * base * base;
    }
    
    RET result = 1;
    
    while (exp > 0) {
        if (exp & 1) { result *= base; }
        base *= base;
        exp >>= 1;  
    }
    
    return result;
}

template <typename T, typename E>
constexpr typename std::enable_if<std::is_floating_point<T>::value && std::is_integral<E>::value, T>::type
pow_constexpr(T base, E exp) noexcept {
    if (base == 0) { return exp <= 0 ? std::numeric_limits<T>::quiet_NaN() : 0; }
    if (exp < 0) { return static_cast<T>(1) / pow_constexpr(base, -exp); }

    switch (exp) {
        case 0: return 1;
        case 1: return base;
        case 2: return base * base;
        case 3: return base * base * base;
        case 4: return base * base * base * base;
        case 5: return base * base * base * base * base;
        case 6: return base * base * base * base * base * base;
        case 7: return base * base * base * base * base * base * base;
        case 8: return base * base * base * base * base * base * base * base;
        case 9: return base * base * base * base * base * base * base * base * base;
        case 10: return base * base * base * base * base * base * base * base * base * base;
        case 11: return base * base * base * base * base * base * base * base * base * base * base;
        case 12: return base * base * base * base * base * base * base * base * base * base * base * base;
    }
    
    T result = 1;
    
    while (exp > 0) {
        if (exp & 1) { result *= base; }
        base *= base;
        exp >>= 1;  
    }
    
    return result;
}

template <typename T, typename E>
constexpr typename std::enable_if<std::is_floating_point<T>::value && std::is_floating_point<E>::value, typename std::common_type<T, E>::type>::type
pow_constexpr(const T base, const E exp) noexcept {
    using CT = typename std::common_type<T, E>::type;
    if (base == 0) { return exp <= 0 ? std::numeric_limits<CT>::quiet_NaN() : 0; }
    if (exp == 0) { return 1; }
    if (base < 0 && fractional_constexpr(exp) != 0) { return std::numeric_limits<CT>::quiet_NaN(); }
    
    if (base < 0) {
        const CT abs_base = -static_cast<CT>(base);
        return (fractional_constexpr(exp) == 0 && static_cast<int>(exp) % 2 != 0) ? 
            -exp_constexpr(exp * log_constexpr(abs_base)) : 
            exp_constexpr(exp * log_constexpr(abs_base));
    }
    
    return exp_constexpr(exp * log_constexpr(base));
}

template <typename T, typename E>
constexpr typename std::enable_if<std::is_integral<T>::value && std::is_floating_point<E>::value, E>::type
pow_constexpr(const T base, const E exp) noexcept { return pow_constexpr(static_cast<E>(base), exp); }

template <typename T>
constexpr typename std::enable_if<std::is_floating_point<T>::value, T>::type
pow2_constexpr(const T x) noexcept { 
    if (x <= static_cast<T>(-36.0 * log_constexpr(10.0) / log_constexpr(2.0))) { return static_cast<T>(0); } 
    if (x >= log_constexpr(std::numeric_limits<T>::max()) / log_constexpr(2.0)) { return std::numeric_limits<T>::infinity(); }
    return pow_constexpr(2, x); 
}

template <typename T>
constexpr typename std::enable_if<std::is_integral<T>::value, double>::type
pow2_constexpr(const T x) noexcept { 
    if (x >= 0 && x <= 22) {  
        constexpr double powers_of_2[] = {
            1.0, 2.0, 4.0, 8.0, 16.0, 32.0, 64.0, 128.0,
            256.0, 512.0, 1024.0, 2048.0, 4096.0,
            8192.0, 16384.0, 32768.0, 65536.0,
            131072.0, 262144.0, 524288.0,
            1048576.0, 2097152.0, 4194304.0
        };
        return powers_of_2[x];
    }

    if (x < 0 && x >= -22) {  
        constexpr double powers_of_2[] = {
            0.5, 0.25, 0.125, 0.0625, 0.03125, 0.015625, 0.0078125,
            0.00390625, 0.001953125, 0.0009765625, 0.00048828125, 0.000244140625, 0.0001220703125, 
            0.00006103515625, 0.000030517578125,
            0.0000152587890625, 7.62939453125e-6, 3.814697265625e-6, 1.9079486328125e-6,
            9.5367431640625e-7, 4.76837158203125e-7, 2.384185791015625e-7
        };
        return powers_of_2[-x - 1];
    }

    if (x < 0) { return 1.0 / pow2_constexpr(-x); }
    if (x < 63) { return static_cast<double>(1ULL << x); }
    double result = static_cast<double>(1ULL << 63);
    T remaining = x - 63;

    while (remaining > 0) {
        if (remaining >= 63) {
            result *= static_cast<double>(1ULL << 63);
            remaining -= 63;
        } else {
            result *= static_cast<double>(1ULL << remaining);
            remaining = 0;
        }
    }

    return result;
}

template <typename T>
constexpr typename std::enable_if<std::is_floating_point<T>::value, T>::type
pow10_constexpr(const T x) noexcept { 
    if (x <= -36) { return 0.0; }
    if (x >= log_constexpr(std::numeric_limits<T>::max()) / log_constexpr(10.0)) { return std::numeric_limits<T>::infinity(); }
    return pow_constexpr(10, x);
}

template <typename T>
constexpr typename std::enable_if<std::is_integral<T>::value, double>::type
pow10_constexpr(const T x) noexcept {
    if (x >= 0 && x <= 22) {  
        constexpr double powers_of_10[] = {
            1.0, 10.0, 100.0, 1000.0, 10000.0, 100000.0, 1000000.0, 10000000.0,
            100000000.0, 1000000000.0, 10000000000.0, 100000000000.0, 1000000000000.0,
            10000000000000.0, 100000000000000.0, 1000000000000000.0, 10000000000000000.0,
            100000000000000000.0, 1000000000000000000.0, 10000000000000000000.0,
            100000000000000000000.0, 1000000000000000000000.0, 10000000000000000000000.0
        };
        return powers_of_10[x];
    }

    if (x < 0 && x >= -22) {  
        constexpr double powers_of_10[] = {
            0.1, 0.01, 0.001, 0.0001, 0.00001, 1e-6, 1e-7,
            1e-8, 1e-9, 1e-10, 1e-11, 1e-12, 1e-13, 1e-14, 1e-15,
            1e-16, 1e-17, 1e-18, 1e-19, 1e-20, 1e-21, 1e-22
        };
        return powers_of_10[abs_constexpr(x) - 1];
    }

    return pow10_constexpr(static_cast<double>(x));
}

} // namespace math
} // namespace fizmo

#endif // FIZMO_CONSTEXPR_POWERS_HPP