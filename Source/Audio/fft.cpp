#include "fft.hpp"

#include <algorithm>
#include <cmath>

namespace fizmo {
namespace audio {

namespace {

const double k_pi = 3.14159265358979323846;

void build(std::size_t n, std::vector<std::complex<float>>& twiddles, std::vector<std::size_t>& order) {
    twiddles.resize(n / 2 + 1);
    for (std::size_t k = 0; k <= n / 2; ++k) {
        const double a = -2.0 * k_pi * static_cast<double>(k) / static_cast<double>(n);
        twiddles[k] = std::complex<float>(static_cast<float>(std::cos(a)), static_cast<float>(std::sin(a)));
    }
    order.resize(n);
    std::size_t bits = 0;
    while ((static_cast<std::size_t>(1) << bits) < n) ++bits;
    for (std::size_t i = 0; i < n; ++i) {
        std::size_t r = 0;
        for (std::size_t b = 0; b < bits; ++b) {
            if (i & (static_cast<std::size_t>(1) << b)) r |= static_cast<std::size_t>(1) << (bits - 1 - b);
        }
        order[i] = r;
    }
}

} // namespace

std::size_t next_power_of_two(std::size_t n) noexcept {
    std::size_t p = 1;
    while (p < n) p <<= 1;
    return p;
}

Fft::Fft(std::size_t size) {
    resize(size);
}

void Fft::resize(std::size_t size) {
    const std::size_t n = next_power_of_two(std::max<std::size_t>(size, 4));
    if (n == m_size) return;
    m_size = n;
    build(n, m_twiddles, m_order);
    build(n / 2, m_half_twiddles, m_half_order);
    m_real_twiddles.resize(n / 2 + 1);
    for (std::size_t k = 0; k <= n / 2; ++k) {
        const double a = -2.0 * k_pi * static_cast<double>(k) / static_cast<double>(n);
        m_real_twiddles[k] = std::complex<float>(static_cast<float>(std::cos(a)), static_cast<float>(std::sin(a)));
    }
    m_scratch.assign(n / 2 + 1, std::complex<float>());
}

void Fft::transform(std::complex<float>* data, std::size_t n, const std::vector<std::complex<float>>& twiddles, const std::vector<std::size_t>& order, bool inverse) const {
    for (std::size_t i = 0; i < n; ++i) {
        const std::size_t j = order[i];
        if (i < j) std::swap(data[i], data[j]);
    }
    for (std::size_t len = 2; len <= n; len <<= 1) {
        const std::size_t half = len / 2;
        const std::size_t step = n / len;
        for (std::size_t i = 0; i < n; i += len) {
            for (std::size_t j = 0; j < half; ++j) {
                std::complex<float> w = twiddles[j * step];
                if (inverse) w = std::conj(w);
                const std::complex<float> u = data[i + j];
                const std::complex<float> v = data[i + j + half] * w;
                data[i + j] = u + v;
                data[i + j + half] = u - v;
            }
        }
    }
}

void Fft::forward(const std::complex<float>* input, std::complex<float>* output) const {
    if (input != output) std::copy(input, input + m_size, output);
    transform(output, m_size, m_twiddles, m_order, false);
}

void Fft::inverse(const std::complex<float>* input, std::complex<float>* output) const {
    if (input != output) std::copy(input, input + m_size, output);
    transform(output, m_size, m_twiddles, m_order, true);
    const float scale = 1.0f / static_cast<float>(m_size);
    for (std::size_t i = 0; i < m_size; ++i) output[i] *= scale;
}

void Fft::forward_real(const float* input, std::complex<float>* output) const {
    const std::size_t h = m_size / 2;
    std::complex<float>* z = m_scratch.data();
    for (std::size_t k = 0; k < h; ++k) z[k] = std::complex<float>(input[2 * k], input[2 * k + 1]);
    transform(z, h, m_half_twiddles, m_half_order, false);
    const std::complex<float> z0 = z[0];
    output[0] = std::complex<float>(z0.real() + z0.imag(), 0.0f);
    output[h] = std::complex<float>(z0.real() - z0.imag(), 0.0f);
    for (std::size_t k = 1; k < h; ++k) {
        const std::complex<float> a = z[k];
        const std::complex<float> b = std::conj(z[h - k]);
        const std::complex<float> e = 0.5f * (a + b);
        const std::complex<float> o = std::complex<float>(0.0f, -0.5f) * (a - b);
        output[k] = e + m_real_twiddles[k] * o;
    }
}

void Fft::inverse_real(const std::complex<float>* input, float* output) const {
    const std::size_t h = m_size / 2;
    std::complex<float>* z = m_scratch.data();
    for (std::size_t k = 0; k < h; ++k) {
        const std::complex<float> a = input[k];
        const std::complex<float> b = std::conj(input[h - k]);
        const std::complex<float> e = 0.5f * (a + b);
        const std::complex<float> o = 0.5f * (a - b) * std::conj(m_real_twiddles[k]);
        z[k] = e + std::complex<float>(0.0f, 1.0f) * o;
    }
    transform(z, h, m_half_twiddles, m_half_order, true);
    const float scale = 1.0f / static_cast<float>(h);
    for (std::size_t k = 0; k < h; ++k) {
        output[2 * k] = z[k].real() * scale;
        output[2 * k + 1] = z[k].imag() * scale;
    }
}

std::vector<float> make_window(WindowType type, std::size_t size, bool periodic) {
    std::vector<float> w(size, 1.0f);
    if (size <= 1) return w;
    const double d = periodic ? static_cast<double>(size) : static_cast<double>(size - 1);
    for (std::size_t i = 0; i < size; ++i) {
        const double x = 2.0 * k_pi * static_cast<double>(i) / d;
        double v = 1.0;
        switch (type) {
            case WindowType::rectangular: v = 1.0; break;
            case WindowType::hann: v = 0.5 - 0.5 * std::cos(x); break;
            case WindowType::hamming: v = 0.54 - 0.46 * std::cos(x); break;
            case WindowType::blackman: v = 0.42 - 0.5 * std::cos(x) + 0.08 * std::cos(2.0 * x); break;
            case WindowType::blackman_harris: v = 0.35875 - 0.48829 * std::cos(x) + 0.14128 * std::cos(2.0 * x) - 0.01168 * std::cos(3.0 * x); break;
            case WindowType::sqrt_hann: v = std::sqrt(std::max(0.0, 0.5 - 0.5 * std::cos(x))); break;
        }
        w[i] = static_cast<float>(v);
    }
    return w;
}

} // namespace audio
} // namespace fizmo
