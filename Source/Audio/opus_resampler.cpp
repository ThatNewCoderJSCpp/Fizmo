#include "opus_internal.hpp"
#include "fft.hpp"

#include <algorithm>
#include <cmath>
#include <map>
#include <memory>
#include <mutex>
#include <utility>

namespace fizmo {
namespace audio {
namespace opus_detail {

namespace {

const double k_pi = 3.14159265358979323846;

int gcd_of(int a, int b) noexcept {
    while (b) {
        const int t = a % b;
        a = b;
        b = t;
    }
    return a;
}

double bessel_i0(double x) noexcept {
    double sum = 1.0;
    double term = 1.0;
    const double q = x * x / 4.0;
    for (int k = 1; k < 64; ++k) {
        term *= q / (static_cast<double>(k) * k);
        sum += term;
        if (term < sum * 1e-17) break;
    }
    return sum;
}

struct Design {
    int up = 1;
    int down = 1;
    int taps = 0;
    std::vector<float> filter;
};

std::vector<double> minimum_phase(const std::vector<double>& h) {
    std::size_t n = 1;
    while (n < h.size() * 16) n <<= 1;
    Fft fft(n);
    std::vector<std::complex<float>> a(n);
    std::vector<std::complex<float>> b(n);
    for (std::size_t i = 0; i < n; ++i) a[i] = i < h.size() ? std::complex<float>(static_cast<float>(h[i]), 0.0f) : std::complex<float>(0.0f, 0.0f);
    fft.forward(a.data(), b.data());
    double peak = 0.0;
    for (std::size_t i = 0; i < n; ++i) peak = std::max(peak, static_cast<double>(std::abs(b[i])));
    const double floor_mag = peak * 1e-9;
    for (std::size_t i = 0; i < n; ++i) a[i] = std::complex<float>(static_cast<float>(std::log(std::max(floor_mag, static_cast<double>(std::abs(b[i]))))), 0.0f);
    fft.inverse(a.data(), b.data());
    for (std::size_t i = 0; i < n; ++i) {
        float v = b[i].real();
        if (i == 0 || i == n / 2) {
        } else if (i < n / 2) {
            v *= 2.0f;
        } else {
            v = 0.0f;
        }
        a[i] = std::complex<float>(v, 0.0f);
    }
    fft.forward(a.data(), b.data());
    for (std::size_t i = 0; i < n; ++i) a[i] = std::exp(b[i]);
    fft.inverse(a.data(), b.data());
    std::vector<double> out(h.size());
    for (std::size_t i = 0; i < h.size(); ++i) out[i] = b[i].real();
    return out;
}

Design make_design(int in_rate, int out_rate, double delay) {
    Design d;
    const int g = gcd_of(in_rate, out_rate);
    int up = out_rate / g;
    int down = in_rate / g;
    const int scale = std::max(1, (96000 + up * in_rate - 1) / (up * in_rate));
    up *= scale;
    down *= scale;
    const double rate = static_cast<double>(up) * in_rate;
    const double cutoff = out_rate >= in_rate ? 0.5 * in_rate : 0.47 * out_rate;
    const int half = 16 * up;
    const int len = 2 * half + 1;
    const double beta = 8.0;
    const double norm = bessel_i0(beta);
    std::vector<double> lin(static_cast<std::size_t>(len));
    for (int i = 0; i < len; ++i) {
        const double t = static_cast<double>(i - half);
        const double x = 2.0 * cutoff * t / rate;
        const double sinc = std::fabs(x) < 1e-12 ? 1.0 : std::sin(k_pi * x) / (k_pi * x);
        const double r = t / (half + 1);
        const double win = bessel_i0(beta * std::sqrt(std::max(0.0, 1.0 - r * r))) / norm;
        lin[static_cast<std::size_t>(i)] = sinc * win;
    }
    std::vector<double> mp = minimum_phase(lin);
    double sum = 0.0;
    double moment = 0.0;
    for (std::size_t i = 0; i < mp.size(); ++i) {
        sum += mp[i];
        moment += mp[i] * static_cast<double>(i);
    }
    const double group = sum != 0.0 ? moment / sum : 0.0;
    int shift = static_cast<int>(std::lround(delay * rate - group));
    if (shift < 0) shift = 0;
    std::vector<double> h(static_cast<std::size_t>(shift), 0.0);
    h.insert(h.end(), mp.begin(), mp.end());
    const int taps = static_cast<int>((h.size() + static_cast<std::size_t>(up) - 1) / static_cast<std::size_t>(up));
    d.up = up;
    d.down = down;
    d.taps = taps;
    d.filter.assign(static_cast<std::size_t>(up * taps), 0.0f);
    for (int p = 0; p < up; ++p) {
        double s = 0.0;
        for (int i = 0; i < taps; ++i) {
            const std::size_t at = static_cast<std::size_t>(p + i * up);
            if (at < h.size()) s += h[at];
        }
        for (int i = 0; i < taps; ++i) {
            const std::size_t at = static_cast<std::size_t>(p + i * up);
            const double v = at < h.size() ? h[at] : 0.0;
            d.filter[static_cast<std::size_t>(p * taps + i)] = static_cast<float>(s != 0.0 ? v / s : 0.0);
        }
    }
    return d;
}

const Design& design_for(int in_rate, int out_rate, double delay) {
    static std::mutex lock;
    static std::map<std::pair<std::pair<int, int>, long long>, std::unique_ptr<Design>> cache;
    const long long key = std::llround(delay * 1e9);
    std::lock_guard<std::mutex> guard(lock);
    auto& slot = cache[std::make_pair(std::make_pair(in_rate, out_rate), key)];
    if (!slot) slot.reset(new Design(make_design(in_rate, out_rate, delay)));
    return *slot;
}

} // namespace

void Resampler::setup(int in_rate, int out_rate, double delay_seconds) {
    const Design& d = design_for(in_rate, out_rate, delay_seconds);
    m_in = in_rate;
    m_out = out_rate;
    m_up = d.up;
    m_down = d.down;
    m_taps = d.taps;
    m_filter = d.filter;
    reset();
}

void Resampler::reset() {
    m_history.assign(static_cast<std::size_t>(m_taps > 0 ? m_taps : 1), 0.0f);
    m_phase = 0;
}

std::size_t Resampler::process(const float* in, std::size_t count, float* out) {
    if (m_taps <= 0) return 0;
    const std::size_t keep = static_cast<std::size_t>(m_taps);
    std::vector<float>& buf = m_scratch;
    buf.resize(keep + count);
    std::copy(m_history.begin(), m_history.end(), buf.begin());
    std::copy(in, in + count, buf.begin() + static_cast<std::ptrdiff_t>(keep));
    std::size_t produced = 0;
    long long pos = m_phase;
    const long long limit = static_cast<long long>(count) * m_up;
    while (pos < limit) {
        const long long base = pos / m_up;
        const int j = static_cast<int>(pos - base * m_up);
        const float* coef = m_filter.data() + static_cast<std::size_t>(j * m_taps);
        const float* x = buf.data() + keep + static_cast<std::size_t>(base);
        float acc = 0.0f;
        for (int i = 0; i < m_taps; ++i) acc += coef[i] * x[-i];
        out[produced++] = acc;
        pos += m_down;
    }
    m_phase = static_cast<int>(pos - limit);
    std::copy(buf.end() - static_cast<std::ptrdiff_t>(keep), buf.end(), m_history.begin());
    return produced;
}

} // namespace opus_detail
} // namespace audio
} // namespace fizmo
