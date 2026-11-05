#ifndef FIZMO_AUDIO_OPUS_INTERNAL_HPP
#define FIZMO_AUDIO_OPUS_INTERNAL_HPP

#include <complex>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

namespace fizmo {
namespace audio {
namespace opus_detail {

class RangeDecoder {
public:
    void init(const std::uint8_t* data, std::uint32_t size) noexcept;
    std::uint32_t decode(std::uint32_t ft) noexcept;
    std::uint32_t decode_bin(unsigned bits) noexcept;
    void update(std::uint32_t fl, std::uint32_t fh, std::uint32_t ft) noexcept;
    bool bit_logp(unsigned logp) noexcept;
    int pdf(const std::uint8_t* freqs, int count, unsigned ftb) noexcept;
    int icdf(const std::uint8_t* table, unsigned ftb) noexcept;
    std::uint32_t uint(std::uint32_t ft) noexcept;
    std::uint32_t bits(unsigned count) noexcept;
    int tell() const noexcept;
    std::uint32_t tell_frac() const noexcept;
    std::uint32_t range() const noexcept { return m_rng; }
    std::uint32_t storage() const noexcept { return m_storage; }
    void shrink(std::uint32_t bytes) noexcept;
    void skip_to_end() noexcept;
    bool failed() const noexcept { return m_error; }

private:
    std::uint32_t front_byte() noexcept;
    std::uint32_t back_byte() noexcept;
    void normalize() noexcept;

    const std::uint8_t* m_data = nullptr;
    std::uint32_t m_storage = 0;
    std::uint32_t m_front = 0;
    std::uint32_t m_back = 0;
    std::uint32_t m_rng = 0;
    std::uint32_t m_val = 0;
    std::uint32_t m_ext = 1;
    std::uint32_t m_rem = 0;
    std::uint32_t m_window = 0;
    int m_window_bits = 0;
    int m_total = 0;
    bool m_error = false;
};

int ilog(std::uint32_t x) noexcept;


struct CeltCache {
    std::vector<int> sizes;
    std::vector<int> kmax;
    std::vector<std::vector<int>> cost;
    std::vector<int> caps;
    int row_for(int n) const noexcept;
};
const CeltCache& celt_cache();

extern const std::uint8_t k_celt_laplace[4][2][21][2];
extern const std::uint8_t k_celt_alloc[11][21];
extern const std::uint8_t k_celt_lw[21];
extern const std::uint8_t k_celt_log2t[24];
extern const std::int8_t k_celt_means[25];
extern const std::uint8_t k_celt_edges[22];

int celt_log2_frac(std::uint32_t v, int frac) noexcept;
std::uint32_t pvq_count(int n, int k) noexcept;
int pulses_of(int q) noexcept;

class Resampler {
public:
    void setup(int in_rate, int out_rate, double delay_seconds);
    void reset();
    int in_rate() const noexcept { return m_in; }
    int out_rate() const noexcept { return m_out; }
    std::size_t process(const float* in, std::size_t count, float* out);

private:
    int m_in = 0;
    int m_out = 0;
    int m_up = 1;
    int m_down = 1;
    int m_taps = 0;
    int m_phase = 0;
    std::vector<float> m_filter;
    std::vector<int> m_offset;
    std::vector<float> m_history;
    std::vector<float> m_scratch;
};

class SilkDecoder {
public:
    SilkDecoder();
    void reset();
    bool decode(RangeDecoder& rd, int internal_rate, int duration_ms, bool stereo_packet, int out_channels, bool lbrr, std::vector<float> out[2], bool* lbrr_complete);
    int internal_rate() const noexcept { return m_rate; }

    struct Channel {
        bool have_prev_lsf = false;
        std::int32_t prev_nlsf[16] = {};
        bool have_prev_gain = false;
        int prev_log_gain = 0;
        float out_hist[320] = {};
        float lpc_hist[16] = {};
    };
    struct Frame;
    struct GainContext {
        bool have = false;
        int value = 0;
    };

private:
    void reset_channel(int c);
    void parse_frame(RangeDecoder& rd, int c, int index, bool lbrr_frame, bool vad, bool independent, bool lbrr_prev_coded, Frame& f, GainContext& gc, int& lag_ctx);
    void synthesize(int c, const Frame& f, float* out);
    void unmix(const float* mid, const float* side, int n, int w0q, int w1q, int out_channels, bool stereo, std::vector<float> out[2], int offset);

    std::vector<Channel> m_ch;
    int m_rate = 0;
    int m_prev_w[2] = { 0, 0 };
    float m_mid_hist[2] = { 0.0f, 0.0f };
    float m_side_hist = 0.0f;
    bool m_prev_stereo = false;
    bool m_side_was_coded = false;
};

class CeltDecoder {
public:
    CeltDecoder();
    ~CeltDecoder();
    CeltDecoder(CeltDecoder&&) noexcept;
    CeltDecoder& operator=(CeltDecoder&&) noexcept;
    CeltDecoder(const CeltDecoder&) = delete;
    CeltDecoder& operator=(const CeltDecoder&) = delete;
    void init(int out_channels, int downsample);
    void reset();
    void decode(RangeDecoder& rd, int frame48, int start, int end, int coded_channels, float* out, bool accumulate);
    void set_phase_inversion(bool enabled) noexcept { m_allow_inversion = enabled; }
    int out_channels() const noexcept { return m_out_channels; }

    struct State;

private:
    void synthesize(const std::vector<float> spectrum[2], int coded_channels, int LM, bool transient, bool silence, int frame48, float* out, bool accumulate, int postfilter_period, float postfilter_gain, int postfilter_tapset);

    int m_out_channels = 1;
    int m_down = 1;
    bool m_allow_inversion = true;
    std::uint32_t m_seed = 0;
    float m_energy[2][21];
    float m_hist1[2][21];
    float m_hist2[2][21];
    std::vector<float> m_overlap[2];
    std::vector<float> m_postfilter_hist[2];
    float m_deemph[2] = { 0.0f, 0.0f };
    int m_pf_period_old = 0;
    int m_pf_period = 0;
    float m_pf_gain_old = 0.0f;
    float m_pf_gain = 0.0f;
    int m_pf_tapset_old = 0;
    int m_pf_tapset = 0;
    float m_scale = 1.0f;
    std::unique_ptr<State> m_state;
    std::vector<float> m_window;
};

} // namespace opus_detail
} // namespace audio
} // namespace fizmo

#endif // FIZMO_AUDIO_OPUS_INTERNAL_HPP
