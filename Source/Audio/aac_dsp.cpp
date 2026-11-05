#include "aac_internal.hpp"

#include <algorithm>
#include <cmath>

namespace fizmo {
namespace audio {
namespace aac_internal {

namespace {

const double k_pi = 3.14159265358979323846;

template <typename T>
void build_table(Huffman& h, const T* codes, const std::uint8_t* bits, std::size_t count, int root) {
    std::vector<std::uint32_t> c(count);
    for (std::size_t i = 0; i < count; ++i) c[i] = static_cast<std::uint32_t>(codes[i]);
    h.build(c.data(), bits, count, root);
}

struct SpectralBooks {
    Huffman books[11];
    SpectralBooks() {
        build_table(books[0], k_spec_codes1, k_spec_bits1, 81, 9);
        build_table(books[1], k_spec_codes2, k_spec_bits2, 81, 9);
        build_table(books[2], k_spec_codes3, k_spec_bits3, 81, 9);
        build_table(books[3], k_spec_codes4, k_spec_bits4, 81, 9);
        build_table(books[4], k_spec_codes5, k_spec_bits5, 81, 9);
        build_table(books[5], k_spec_codes6, k_spec_bits6, 81, 9);
        build_table(books[6], k_spec_codes7, k_spec_bits7, 64, 9);
        build_table(books[7], k_spec_codes8, k_spec_bits8, 64, 9);
        build_table(books[8], k_spec_codes9, k_spec_bits9, 169, 9);
        build_table(books[9], k_spec_codes10, k_spec_bits10, 169, 9);
        build_table(books[10], k_spec_codes11, k_spec_bits11, 289, 9);
    }
};

struct SbrBooks {
    Huffman books[10];
    SbrBooks() {
        build_table(books[0], k_sbr_t_huffman_env_1_5dB_codes, k_sbr_t_huffman_env_1_5dB_bits, 121, 9);
        build_table(books[1], k_sbr_f_huffman_env_1_5dB_codes, k_sbr_f_huffman_env_1_5dB_bits, 121, 9);
        build_table(books[2], k_sbr_t_huffman_env_bal_1_5dB_codes, k_sbr_t_huffman_env_bal_1_5dB_bits, 49, 9);
        build_table(books[3], k_sbr_f_huffman_env_bal_1_5dB_codes, k_sbr_f_huffman_env_bal_1_5dB_bits, 49, 9);
        build_table(books[4], k_sbr_t_huffman_env_3_0dB_codes, k_sbr_t_huffman_env_3_0dB_bits, 63, 9);
        build_table(books[5], k_sbr_f_huffman_env_3_0dB_codes, k_sbr_f_huffman_env_3_0dB_bits, 63, 9);
        build_table(books[6], k_sbr_t_huffman_env_bal_3_0dB_codes, k_sbr_t_huffman_env_bal_3_0dB_bits, 25, 9);
        build_table(books[7], k_sbr_f_huffman_env_bal_3_0dB_codes, k_sbr_f_huffman_env_bal_3_0dB_bits, 25, 9);
        build_table(books[8], k_sbr_t_huffman_noise_3_0dB_codes, k_sbr_t_huffman_noise_3_0dB_bits, 63, 9);
        build_table(books[9], k_sbr_t_huffman_noise_bal_3_0dB_codes, k_sbr_t_huffman_noise_bal_3_0dB_bits, 25, 9);
    }
};

struct PsBooks {
    Huffman books[10];
    PsBooks() {
        build_table(books[0], k_ps_huff_iid_df1_codes, k_ps_huff_iid_df1_bits, 61, 9);
        build_table(books[1], k_ps_huff_iid_dt1_codes, k_ps_huff_iid_dt1_bits, 61, 9);
        build_table(books[2], k_ps_huff_iid_df0_codes, k_ps_huff_iid_df0_bits, 29, 9);
        build_table(books[3], k_ps_huff_iid_dt0_codes, k_ps_huff_iid_dt0_bits, 29, 9);
        build_table(books[4], k_ps_huff_icc_df_codes, k_ps_huff_icc_df_bits, 15, 9);
        build_table(books[5], k_ps_huff_icc_dt_codes, k_ps_huff_icc_dt_bits, 15, 9);
        build_table(books[6], k_ps_huff_ipd_df_codes, k_ps_huff_ipd_df_bits, 8, 5);
        build_table(books[7], k_ps_huff_ipd_dt_codes, k_ps_huff_ipd_dt_bits, 8, 5);
        build_table(books[8], k_ps_huff_opd_df_codes, k_ps_huff_opd_df_bits, 8, 5);
        build_table(books[9], k_ps_huff_opd_dt_codes, k_ps_huff_opd_dt_bits, 8, 5);
    }
};

} // namespace

void Huffman::build(const std::uint32_t* codes, const std::uint8_t* bits, std::size_t count, int root_bits) {
    std::vector<Code> list;
    list.reserve(count);
    for (std::size_t i = 0; i < count; ++i) {
        if (bits[i] == 0 || bits[i] > 31) continue;
        list.push_back({ codes[i], static_cast<int>(bits[i]), static_cast<int>(i) });
    }
    m_root = root_bits;
    m_table.clear();
    m_table.resize(std::size_t(1) << root_bits, 0);
    build_level(0, root_bits, list);
}

void Huffman::build_level(std::size_t base, int level_bits, const std::vector<Code>& codes) {
    const std::size_t count = std::size_t(1) << level_bits;
    std::vector<std::vector<Code>> deeper(count);
    for (const Code& c : codes) {
        if (c.length <= level_bits) {
            const std::size_t first = static_cast<std::size_t>(c.code) << (level_bits - c.length);
            const std::size_t n = std::size_t(1) << (level_bits - c.length);
            for (std::size_t i = 0; i < n && first + i < count; ++i) {
                m_table[base + first + i] = (static_cast<std::uint32_t>(c.length) << 16) | static_cast<std::uint32_t>(c.symbol);
            }
        } else {
            const int rest = c.length - level_bits;
            const std::size_t prefix = static_cast<std::size_t>(c.code >> rest);
            if (prefix < count) deeper[prefix].push_back({ c.code & ((std::uint32_t(1) << rest) - 1), rest, c.symbol });
        }
    }
    for (std::size_t p = 0; p < count; ++p) {
        if (deeper[p].empty()) continue;
        int longest = 0;
        for (const Code& c : deeper[p]) longest = std::max(longest, c.length);
        const int sub_bits = std::min(longest, 9);
        const std::size_t offset = m_table.size();
        m_table.resize(offset + (std::size_t(1) << sub_bits), 0);
        m_table[base + p] = 0x80000000u | (static_cast<std::uint32_t>(sub_bits) << 24) | static_cast<std::uint32_t>(offset);
        build_level(offset, sub_bits, deeper[p]);
    }
}

int Huffman::decode(BitReader& br) const noexcept {
    std::size_t base = 0;
    int bits = m_root;
    for (int depth = 0; depth < 8; ++depth) {
        const std::uint32_t e = m_table[base + br.peek(bits)];
        if (e & 0x80000000u) {
            br.skip(static_cast<std::size_t>(bits));
            base = e & 0xFFFFFFu;
            bits = static_cast<int>((e >> 24) & 31u);
            continue;
        }
        if (e == 0) return -1;
        br.skip(static_cast<std::size_t>(e >> 16));
        return static_cast<int>(e & 0xFFFFu);
    }
    return -1;
}

const Huffman& scalefactor_huffman() {
    static const Huffman table = [] {
        Huffman h;
        h.build(k_sf_codes, k_sf_bits, 121, 9);
        return h;
    }();
    return table;
}

const Huffman& spectral_huffman(int book) {
    static const SpectralBooks books;
    return books.books[std::min(std::max(book, 1), 11) - 1];
}

const Huffman& sbr_huffman(int index) {
    static const SbrBooks books;
    return books.books[std::min(std::max(index, 0), 9)];
}

const Huffman& ps_huffman(int index) {
    static const PsBooks books;
    return books.books[std::min(std::max(index, 0), 9)];
}

void Fft::init(std::size_t n) {
    m_n = n;
    m_factors.clear();
    std::size_t r = n;
    while (r > 1 && r % 4 == 0) {
        m_factors.push_back(4);
        r /= 4;
    }
    while (r > 1 && r % 2 == 0) {
        m_factors.push_back(2);
        r /= 2;
    }
    for (std::size_t f = 3; r > 1; f += 2) {
        while (r % f == 0) {
            m_factors.push_back(f);
            r /= f;
        }
    }
    m_twiddles.resize(n);
    for (std::size_t i = 0; i < n; ++i) {
        const double a = -2.0 * k_pi * static_cast<double>(i) / static_cast<double>(n);
        m_twiddles[i] = std::complex<double>(std::cos(a), std::sin(a));
    }
    m_work.assign(n, std::complex<float>());
}

void Fft::pass(std::complex<float>* out, const std::complex<float>* in, std::size_t n, std::size_t stride, std::size_t level) const {
    if (n == 1) {
        out[0] = in[0];
        return;
    }
    const std::size_t p = m_factors[level];
    const std::size_t m = n / p;
    for (std::size_t q = 0; q < p; ++q) pass(out + q * m, in + q * stride, m, stride * p, level + 1);
    const std::size_t step = m_n / n;
    const std::size_t pstep = m_n / p;
    std::complex<double> t[16];
    std::vector<std::complex<double>> big;
    std::complex<double>* tp = t;
    if (p > 16) {
        big.resize(p);
        tp = big.data();
    }
    for (std::size_t k = 0; k < m; ++k) {
        for (std::size_t q = 0; q < p; ++q) {
            const std::complex<float> v = out[q * m + k];
            tp[q] = std::complex<double>(v.real(), v.imag()) * m_twiddles[(q * k * step) % m_n];
        }
        if (p == 2) {
            const std::complex<double> a = tp[0] + tp[1];
            const std::complex<double> b = tp[0] - tp[1];
            out[k] = std::complex<float>(static_cast<float>(a.real()), static_cast<float>(a.imag()));
            out[m + k] = std::complex<float>(static_cast<float>(b.real()), static_cast<float>(b.imag()));
        } else if (p == 4) {
            const std::complex<double> a0 = tp[0] + tp[2];
            const std::complex<double> a1 = tp[0] - tp[2];
            const std::complex<double> a2 = tp[1] + tp[3];
            const std::complex<double> d = tp[1] - tp[3];
            const std::complex<double> a3(d.imag(), -d.real());
            const std::complex<double> r0 = a0 + a2;
            const std::complex<double> r1 = a1 + a3;
            const std::complex<double> r2 = a0 - a2;
            const std::complex<double> r3 = a1 - a3;
            out[k] = std::complex<float>(static_cast<float>(r0.real()), static_cast<float>(r0.imag()));
            out[m + k] = std::complex<float>(static_cast<float>(r1.real()), static_cast<float>(r1.imag()));
            out[2 * m + k] = std::complex<float>(static_cast<float>(r2.real()), static_cast<float>(r2.imag()));
            out[3 * m + k] = std::complex<float>(static_cast<float>(r3.real()), static_cast<float>(r3.imag()));
        } else {
            for (std::size_t s = 0; s < p; ++s) {
                std::complex<double> acc(0.0, 0.0);
                for (std::size_t q = 0; q < p; ++q) acc += tp[q] * m_twiddles[((q * s) % p) * pstep];
                out[s * m + k] = std::complex<float>(static_cast<float>(acc.real()), static_cast<float>(acc.imag()));
            }
        }
    }
}

void Fft::forward(std::complex<float>* data) const {
    if (m_n <= 1) return;
    std::copy(data, data + m_n, m_work.begin());
    pass(data, m_work.data(), m_n, 1, 0);
}

void DctIV::init(std::size_t n) {
    m_n = n;
    const std::size_t h = n / 2;
    m_fft.init(h);
    m_pre.resize(h);
    m_post.resize(h);
    for (std::size_t i = 0; i < h; ++i) {
        const double a = -k_pi * (static_cast<double>(i) + 0.125) / static_cast<double>(n);
        m_pre[i] = std::complex<float>(static_cast<float>(std::cos(a)), static_cast<float>(std::sin(a)));
        m_post[i] = m_pre[i];
    }
    m_buf.assign(h, std::complex<float>());
}

void DctIV::transform(const float* in, float* out) const {
    const std::size_t n = m_n;
    const std::size_t h = n / 2;
    for (std::size_t i = 0; i < h; ++i) m_buf[i] = std::complex<float>(in[2 * i], in[n - 1 - 2 * i]) * m_pre[i];
    m_fft.forward(m_buf.data());
    for (std::size_t k = 0; k < h; ++k) {
        const std::complex<float> u = m_buf[k] * m_post[k];
        out[2 * k] = u.real();
        out[n - 1 - 2 * k] = -u.imag();
    }
}

void Imdct::init(std::size_t coefficients) {
    m_n = coefficients;
    m_dct.init(coefficients);
    m_tmp.assign(coefficients, 0.0f);
    m_fold.assign(coefficients, 0.0f);
}

void Imdct::transform(const float* in, float* out) const {
    const std::size_t m = m_n;
    const std::size_t half = m / 2;
    m_dct.transform(in, m_tmp.data());
    const float scale = 1.0f / static_cast<float>(m);
    for (std::size_t n = 0; n < half; ++n) out[n] = m_tmp[n + half] * scale;
    for (std::size_t n = half; n < 3 * half; ++n) out[n] = -m_tmp[3 * half - 1 - n] * scale;
    for (std::size_t n = 3 * half; n < 2 * m; ++n) out[n] = -m_tmp[n - 3 * half] * scale;
}

void Imdct::forward(const float* in, float* out) const {
    const std::size_t m = m_n;
    const std::size_t half = m / 2;
    for (std::size_t n = 0; n < half; ++n) m_fold[n + half] = in[n];
    for (std::size_t n = 0; n < half; ++n) m_fold[n] = -in[n + 3 * half];
    for (std::size_t n = half; n < 3 * half; ++n) m_fold[3 * half - 1 - n] -= in[n];
    m_dct.transform(m_fold.data(), out);
    for (std::size_t k = 0; k < m; ++k) out[k] *= 2.0f;
}

} // namespace aac_internal
} // namespace audio
} // namespace fizmo
