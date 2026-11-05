#ifndef FIZMO_AUDIO_FFT_HPP
#define FIZMO_AUDIO_FFT_HPP

#include <complex>
#include <cstddef>
#include <vector>

namespace fizmo {
namespace audio {

std::size_t next_power_of_two(std::size_t n) noexcept;

class Fft {
public:
    Fft() = default;
    explicit Fft(std::size_t size);

    void resize(std::size_t size);
    std::size_t size() const noexcept { return m_size; }
    std::size_t bins() const noexcept { return m_size / 2 + 1; }

    void forward(const std::complex<float>* input, std::complex<float>* output) const;
    void inverse(const std::complex<float>* input, std::complex<float>* output) const;
    void forward_real(const float* input, std::complex<float>* output) const;
    void inverse_real(const std::complex<float>* input, float* output) const;

private:
    void transform(std::complex<float>* data, std::size_t n, const std::vector<std::complex<float>>& twiddles, const std::vector<std::size_t>& order, bool inverse) const;

    std::size_t m_size = 0;
    std::vector<std::complex<float>> m_twiddles;
    std::vector<std::size_t> m_order;
    std::vector<std::complex<float>> m_half_twiddles;
    std::vector<std::size_t> m_half_order;
    std::vector<std::complex<float>> m_real_twiddles;
    mutable std::vector<std::complex<float>> m_scratch;
};

enum class WindowType {
    rectangular,
    hann,
    hamming,
    blackman,
    blackman_harris,
    sqrt_hann
};

std::vector<float> make_window(WindowType type, std::size_t size, bool periodic = true);

} // namespace audio
} // namespace fizmo

#endif // FIZMO_AUDIO_FFT_HPP
