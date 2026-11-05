#ifndef FIZMO_AUDIO_AAC_INTERNAL_HPP
#define FIZMO_AUDIO_AAC_INTERNAL_HPP

#include <complex>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace fizmo {
namespace audio {
namespace aac_internal {

class BitReader {
public:
    BitReader() = default;
    BitReader(const std::uint8_t* data, std::size_t size) noexcept : m_data(data), m_bits(data ? size * 8 : 0) {}

    std::uint32_t read(int n) noexcept {
        const std::uint32_t v = peek(n);
        skip(static_cast<std::size_t>(n));
        return v;
    }
    int bit() noexcept { return static_cast<int>(read(1)); }
    std::uint32_t peek(int n) const noexcept {
        std::uint32_t v = 0;
        std::size_t p = m_pos;
        for (int i = 0; i < n; ++i, ++p) {
            v <<= 1;
            if (p < m_bits) v |= (m_data[p >> 3] >> (7 - (p & 7))) & 1u;
        }
        return v;
    }
    void skip(std::size_t n) noexcept {
        m_pos += n;
        if (m_pos > m_bits) m_over = true;
    }
    void align() noexcept {
        const std::size_t r = m_pos & 7;
        if (r) skip(8 - r);
    }
    std::size_t pos() const noexcept { return m_pos; }
    void seek(std::size_t p) noexcept {
        m_pos = p;
        m_over = m_pos > m_bits;
    }
    long long left() const noexcept { return static_cast<long long>(m_bits) - static_cast<long long>(m_pos); }
    bool overrun() const noexcept { return m_over || m_pos > m_bits; }
    std::size_t size_bits() const noexcept { return m_bits; }

private:
    const std::uint8_t* m_data = nullptr;
    std::size_t m_bits = 0;
    std::size_t m_pos = 0;
    bool m_over = false;
};

enum ElementType {
    elem_sce = 0,
    elem_cpe = 1,
    elem_cce = 2,
    elem_lfe = 3,
    elem_dse = 4,
    elem_pce = 5,
    elem_fil = 6,
    elem_end = 7
};

enum ObjectType {
    aot_main = 1,
    aot_lc = 2,
    aot_ssr = 3,
    aot_ltp = 4,
    aot_sbr = 5,
    aot_ps = 29
};

enum PcePosition {
    pce_front = 0,
    pce_side = 1,
    pce_back = 2,
    pce_lfe = 3,
    pce_cc = 4
};

struct ProgramConfig {
    struct Entry {
        int type = elem_sce;
        int tag = 0;
        int position = pce_front;
    };
    int object_type = 0;
    int sampling_index = 0;
    std::vector<Entry> entries;
};

struct StreamConfig {
    int object_type = aot_lc;
    int sampling_index = 4;
    std::uint32_t sample_rate = 44100;
    int channel_config = 0;
    bool frame_length_short = false;
    bool has_pce = false;
    ProgramConfig pce;
    int sbr = -1;
    int ps = -1;
    std::uint32_t ext_sample_rate = 0;
};

struct AdtsHeader {
    int id = 0;
    int protection_absent = 1;
    int object_type = 2;
    int sampling_index = 0;
    int channel_config = 0;
    std::size_t frame_length = 0;
    int buffer_fullness = 0;
    int raw_blocks = 0;
    std::size_t header_size = 7;
};

extern const std::uint32_t k_sample_rates[16];

bool parse_program_config(BitReader& br, std::size_t align_ref, ProgramConfig& pce);
bool parse_asc(const std::uint8_t* data, std::size_t size, StreamConfig& cfg, std::string* error);
bool parse_adts_header(const std::uint8_t* p, std::size_t size, AdtsHeader& h) noexcept;

class Huffman {
public:
    void build(const std::uint32_t* codes, const std::uint8_t* bits, std::size_t count, int root_bits);
    int decode(BitReader& br) const noexcept;

private:
    struct Code {
        std::uint32_t code;
        int length;
        int symbol;
    };
    void build_level(std::size_t base, int level_bits, const std::vector<Code>& codes);

    int m_root = 0;
    std::vector<std::uint32_t> m_table;
};

const Huffman& scalefactor_huffman();
const Huffman& spectral_huffman(int book);
const Huffman& sbr_huffman(int index);
const Huffman& ps_huffman(int index);

extern const std::uint32_t k_sf_codes[121];
extern const std::uint8_t k_sf_bits[121];
extern const std::uint16_t k_spec_codes1[81];
extern const std::uint8_t k_spec_bits1[81];
extern const std::uint16_t k_spec_codes2[81];
extern const std::uint8_t k_spec_bits2[81];
extern const std::uint16_t k_spec_codes3[81];
extern const std::uint8_t k_spec_bits3[81];
extern const std::uint16_t k_spec_codes4[81];
extern const std::uint8_t k_spec_bits4[81];
extern const std::uint16_t k_spec_codes5[81];
extern const std::uint8_t k_spec_bits5[81];
extern const std::uint16_t k_spec_codes6[81];
extern const std::uint8_t k_spec_bits6[81];
extern const std::uint16_t k_spec_codes7[64];
extern const std::uint8_t k_spec_bits7[64];
extern const std::uint16_t k_spec_codes8[64];
extern const std::uint8_t k_spec_bits8[64];
extern const std::uint16_t k_spec_codes9[169];
extern const std::uint8_t k_spec_bits9[169];
extern const std::uint16_t k_spec_codes10[169];
extern const std::uint8_t k_spec_bits10[169];
extern const std::uint16_t k_spec_codes11[289];
extern const std::uint8_t k_spec_bits11[289];

extern const std::uint32_t k_sbr_t_huffman_env_1_5dB_codes[121];
extern const std::uint8_t k_sbr_t_huffman_env_1_5dB_bits[121];
extern const std::uint32_t k_sbr_f_huffman_env_1_5dB_codes[121];
extern const std::uint8_t k_sbr_f_huffman_env_1_5dB_bits[121];
extern const std::uint32_t k_sbr_t_huffman_env_bal_1_5dB_codes[49];
extern const std::uint8_t k_sbr_t_huffman_env_bal_1_5dB_bits[49];
extern const std::uint32_t k_sbr_f_huffman_env_bal_1_5dB_codes[49];
extern const std::uint8_t k_sbr_f_huffman_env_bal_1_5dB_bits[49];
extern const std::uint32_t k_sbr_t_huffman_env_3_0dB_codes[63];
extern const std::uint8_t k_sbr_t_huffman_env_3_0dB_bits[63];
extern const std::uint32_t k_sbr_f_huffman_env_3_0dB_codes[63];
extern const std::uint8_t k_sbr_f_huffman_env_3_0dB_bits[63];
extern const std::uint32_t k_sbr_t_huffman_env_bal_3_0dB_codes[25];
extern const std::uint8_t k_sbr_t_huffman_env_bal_3_0dB_bits[25];
extern const std::uint32_t k_sbr_f_huffman_env_bal_3_0dB_codes[25];
extern const std::uint8_t k_sbr_f_huffman_env_bal_3_0dB_bits[25];
extern const std::uint32_t k_sbr_t_huffman_noise_3_0dB_codes[63];
extern const std::uint8_t k_sbr_t_huffman_noise_3_0dB_bits[63];
extern const std::uint32_t k_sbr_t_huffman_noise_bal_3_0dB_codes[25];
extern const std::uint8_t k_sbr_t_huffman_noise_bal_3_0dB_bits[25];

extern const std::uint32_t k_ps_huff_iid_df1_codes[61];
extern const std::uint8_t k_ps_huff_iid_df1_bits[61];
extern const std::uint32_t k_ps_huff_iid_dt1_codes[61];
extern const std::uint8_t k_ps_huff_iid_dt1_bits[61];
extern const std::uint32_t k_ps_huff_iid_df0_codes[29];
extern const std::uint8_t k_ps_huff_iid_df0_bits[29];
extern const std::uint32_t k_ps_huff_iid_dt0_codes[29];
extern const std::uint8_t k_ps_huff_iid_dt0_bits[29];
extern const std::uint32_t k_ps_huff_icc_df_codes[15];
extern const std::uint8_t k_ps_huff_icc_df_bits[15];
extern const std::uint32_t k_ps_huff_icc_dt_codes[15];
extern const std::uint8_t k_ps_huff_icc_dt_bits[15];
extern const std::uint32_t k_ps_huff_ipd_df_codes[8];
extern const std::uint8_t k_ps_huff_ipd_df_bits[8];
extern const std::uint32_t k_ps_huff_ipd_dt_codes[8];
extern const std::uint8_t k_ps_huff_ipd_dt_bits[8];
extern const std::uint32_t k_ps_huff_opd_df_codes[8];
extern const std::uint8_t k_ps_huff_opd_df_bits[8];
extern const std::uint32_t k_ps_huff_opd_dt_codes[8];
extern const std::uint8_t k_ps_huff_opd_dt_bits[8];

struct SwbLayout {
    int count = 0;
    const std::uint16_t* offsets = nullptr;
};

SwbLayout swb_long(int sampling_index) noexcept;
SwbLayout swb_short(int sampling_index) noexcept;
int tns_max_bands(int sampling_index, bool short_window) noexcept;
int pred_sfb_max(int sampling_index) noexcept;

extern const double k_qmf_prototype[321];
extern const double k_sbr_noise[512][2];

class Fft {
public:
    void init(std::size_t n);
    void forward(std::complex<float>* data) const;

private:
    void pass(std::complex<float>* out, const std::complex<float>* in, std::size_t n, std::size_t stride, std::size_t level) const;

    std::size_t m_n = 0;
    std::vector<std::size_t> m_factors;
    std::vector<std::complex<double>> m_twiddles;
    mutable std::vector<std::complex<float>> m_work;
};

class DctIV {
public:
    void init(std::size_t n);
    void transform(const float* in, float* out) const;

private:
    std::size_t m_n = 0;
    Fft m_fft;
    std::vector<std::complex<float>> m_pre;
    std::vector<std::complex<float>> m_post;
    mutable std::vector<std::complex<float>> m_buf;
};

class Imdct {
public:
    void init(std::size_t coefficients);
    void transform(const float* in, float* out) const;
    void forward(const float* in, float* out) const;

private:
    std::size_t m_n = 0;
    DctIV m_dct;
    mutable std::vector<float> m_tmp;
    mutable std::vector<float> m_fold;
};

class SbrElement {
public:
    SbrElement();
    ~SbrElement();
    SbrElement(const SbrElement&) = delete;
    SbrElement& operator=(const SbrElement&) = delete;

    void setup(int channels, std::uint32_t sbr_rate, bool downsampled, bool allow_ps);
    void reset();
    bool parse(BitReader& br, std::size_t end_bit, bool crc, bool pair);
    void no_data();
    void process(const float* const* core, float* const* out);
    bool ps_seen() const noexcept;
    int channels() const noexcept;

    struct State;

private:
    std::unique_ptr<State> m_state;
};

class CoreDecoder {
public:
    CoreDecoder();
    ~CoreDecoder();
    CoreDecoder(const CoreDecoder&) = delete;
    CoreDecoder& operator=(const CoreDecoder&) = delete;

    bool configure(const StreamConfig& cfg, std::string* error);
    void reset();
    bool decode_block(BitReader& br, std::size_t align_ref, std::vector<float>& out, std::string* error);
    void append_silence(std::vector<float>& out) const;

    const StreamConfig& stream() const noexcept;
    bool sbr_active() const noexcept;
    bool ps_active() const noexcept;
    int output_channels() const noexcept;
    std::uint32_t output_rate() const noexcept;
    std::uint32_t output_frame_length() const noexcept;
    std::uint32_t extension_rate() const noexcept;
    std::uint32_t channel_mask() const noexcept;
    std::uint32_t delay() const noexcept;

    struct State;

private:
    std::unique_ptr<State> m_state;
};

} // namespace aac_internal
} // namespace audio
} // namespace fizmo

#endif // FIZMO_AUDIO_AAC_INTERNAL_HPP
