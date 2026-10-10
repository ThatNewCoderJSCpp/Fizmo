#define FIZMO_MUL_THRESHOLDS_RUNTIME
#include "../../Source/Multiprecision/Big/BigUint Detail/big_uint_mult_detail.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <functional>
#include <limits>
#include <random>
#include <string>
#include <vector>

#if defined(_WIN32)
#include <cstring>
#include <malloc.h>

namespace fizmo {
namespace arrays {
namespace memory {
namespace platform {

void* allocate(std::size_t bytes, std::size_t align, std::size_t& usable) {
    if (align < alignof(std::max_align_t)) align = alignof(std::max_align_t);
    void* p = _aligned_malloc(bytes ? bytes : 1, align);
    usable = p ? bytes : 0;
    return p;
}

void deallocate(void* p, std::size_t, std::size_t) noexcept {
    _aligned_free(p);
}

void* reallocate(void* p, std::size_t, std::size_t used_bytes, std::size_t new_bytes, std::size_t align, std::size_t& usable) {
    void* q = allocate(new_bytes, align, usable);
    if (q && p) std::memcpy(q, p, used_bytes < new_bytes ? used_bytes : new_bytes);
    if (q) _aligned_free(p);
    return q;
}

bool try_expand(void*, std::size_t, std::size_t, std::size_t, std::size_t& usable) noexcept {
    usable = 0;
    return false;
}

void trim_cache() noexcept {}

} // namespace platform
} // namespace memory
} // namespace arrays
} // namespace fizmo
#endif

namespace {

namespace md = fizmo::multiprecision::mdetail;
using Clock = std::chrono::steady_clock;

constexpr std::size_t kNever = std::numeric_limits<std::size_t>::max() / 4;
constexpr double kMargin = -0.01;

struct Operands {
    std::vector<std::uint64_t> a, b, r, ws;

    explicit Operands(std::size_t max_n) : a(max_n), b(max_n), r(2 * max_n + 8), ws(64 * max_n + 4096) {
        std::mt19937_64 rng(0x5eed1234abcdULL);
        for (auto& x : a) x = rng();
        for (auto& x : b) x = rng();
    }
};

double seconds_per_call(const std::function<void()>& fn) {
    fn();
    std::size_t reps = 1;

    for (;;) {
        const auto t0 = Clock::now();
        for (std::size_t i = 0; i < reps; ++i) fn();
        const double t = std::chrono::duration<double>(Clock::now() - t0).count();
        if (t >= 0.002 || reps >= (std::size_t(1) << 24)) break;
        reps *= 2;
    }

    double best = std::numeric_limits<double>::max();

    for (int trial = 0; trial < 5; ++trial) {
        const auto t0 = Clock::now();
        for (std::size_t i = 0; i < reps; ++i) fn();
        const double t = std::chrono::duration<double>(Clock::now() - t0).count() / static_cast<double>(reps);
        best = std::min(best, t);
    }

    return best;
}

std::vector<std::size_t> sizes(std::size_t lo, std::size_t hi, double growth, std::size_t min_step) {
    std::vector<std::size_t> out;

    for (std::size_t n = lo; n <= hi;) {
        out.push_back(n);
        const std::size_t next = static_cast<std::size_t>(static_cast<double>(n) * growth);
        n = std::max(n + min_step, next);
    }

    return out;
}

std::size_t crossover(const char* label, const std::vector<std::size_t>& ns, const std::function<void(std::size_t)>& slow, const std::function<void(std::size_t)>& fast) {
    std::vector<double> log_ratio;
    std::size_t next = 0;

    for (std::size_t idx = 0; idx < ns.size(); ++idx) {
        const std::size_t n = ns[idx];
        const double ts = seconds_per_call([&] { slow(n); });
        const double tf = seconds_per_call([&] { fast(n); });
        log_ratio.push_back(std::log(tf / ts));
        std::printf("  %-10s n=%-6zu %10.3f us  vs %10.3f us%s\n", label, n, ts * 1e6, tf * 1e6, tf < ts ? "  <" : "");
        std::fflush(stdout);

        while (next <= idx && ns[idx] >= 2 * ns[next]) {
            double sum = 0.0;
            std::size_t count = 0;
            for (std::size_t j = next; j <= idx; ++j) { sum += log_ratio[j]; ++count; }
            if (log_ratio[next] < 0.0 && sum < kMargin * static_cast<double>(count)) return ns[next];
            ++next;
        }
    }

    for (; next < ns.size(); ++next) {
        double sum = 0.0;
        for (std::size_t j = next; j < ns.size(); ++j) sum += log_ratio[j];
        if (log_ratio[next] < 0.0 && sum < kMargin * static_cast<double>(ns.size() - next)) return ns[next];
    }

    return ns.back() + 1;
}

bool write_header(const std::string& path, std::size_t k, std::size_t t3, std::size_t t4, std::size_t ntt) {
    const std::string tmp = path + ".tmp";
    {
        std::ofstream f(tmp, std::ios::binary | std::ios::trunc);
        if (!f) return false;
        f << "#ifndef FIZMO_MUL_TUNING_HPP\n"
          << "#define FIZMO_MUL_TUNING_HPP\n\n"
          << "#define FIZMO_MUL_KARATSUBA_THRESHOLD " << k << "\n"
          << "#define FIZMO_MUL_TOOM3_THRESHOLD     " << t3 << "\n"
          << "#define FIZMO_MUL_TOOM4_THRESHOLD     " << t4 << "\n"
          << "#define FIZMO_MUL_NTT_THRESHOLD       " << ntt << "\n\n"
          << "#endif // FIZMO_MUL_TUNING_HPP\n";
        if (!f) return false;
    }
    std::remove(path.c_str());
    return std::rename(tmp.c_str(), path.c_str()) == 0;
}

}

int main(int argc, char** argv) {
    const bool quiet = argc > 2 && std::string(argv[2]) == "--quiet";
    if (quiet && !std::freopen(
#if defined(_WIN32)
        "NUL",
#else
        "/dev/null",
#endif
        "w", stdout)) return 1;

    constexpr std::size_t kMaxN = 40000;
    Operands op(kMaxN);
    auto* a = op.a.data();
    auto* b = op.b.data();
    auto* r = op.r.data();
    auto* w = op.ws.data();
    if (!md::nttdetail::self_check()) {
        std::fprintf(stderr, "fizmo-tune: the NTT self check failed\n");
        return 1;
    }

    md::mul_threshold::karatsuba = kNever;
    md::mul_threshold::toom3     = kNever;
    md::mul_threshold::toom4     = kNever;
    md::mul_threshold::ntt       = kNever;

    std::printf("basecase vs karatsuba\n");
    std::size_t k = crossover("karatsuba", sizes(8, 160, 1.0, 2),
        [&](std::size_t n) { md::mul_basecase(r, a, n, b, n); },
        [&](std::size_t n) { md::mul_karatsuba(r, a, b, n, w); });
    k = std::max<std::size_t>(k, 8);
    md::mul_threshold::karatsuba = k;

    std::printf("karatsuba vs toom-3\n");
    std::size_t t3 = crossover("toom3", sizes(std::max<std::size_t>(k + 1, 24), 600, 1.04, 2),
        [&](std::size_t n) { md::mul_karatsuba(r, a, b, n, w); },
        [&](std::size_t n) { md::mul_toom3(r, a, b, n, w); });
    t3 = std::max<std::size_t>(t3, std::max<std::size_t>(k + 1, 24));
    md::mul_threshold::toom3 = t3;

    std::printf("toom-3 vs toom-4\n");
    std::size_t t4 = crossover("toom4", sizes(std::max<std::size_t>(t3 + 1, 52), 1200, 1.04, 2),
        [&](std::size_t n) { md::mul_toom3(r, a, b, n, w); },
        [&](std::size_t n) { md::mul_toom4(r, a, b, n, w); });
    t4 = std::max<std::size_t>(t4, std::max<std::size_t>(t3 + 1, 52));
    md::mul_threshold::toom4 = t4;

    std::printf("toom vs ntt\n");
    std::size_t ntt = crossover("ntt", sizes(std::max<std::size_t>(t4 + 1, 256), kMaxN, 1.1, 16),
        [&](std::size_t n) { md::mul_n(r, a, b, n, w); },
        [&](std::size_t n) { md::ntt_mul(r, a, n, b, n); });
    ntt = std::max<std::size_t>(ntt, t4 + 1);

    std::fprintf(stderr, "fizmo-tune: karatsuba %zu, toom-3 %zu, toom-4 %zu, ntt %zu limbs\n", k, t3, t4, ntt);

    if (argc > 1 && !write_header(argv[1], k, t3, t4, ntt)) {
        std::fprintf(stderr, "fizmo-tune: cannot write %s\n", argv[1]);
        return 1;
    }

    return 0;
}
