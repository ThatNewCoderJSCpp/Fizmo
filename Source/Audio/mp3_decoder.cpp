#include "mp3.hpp"
#include "file_io.hpp"
#include "mp3_tables.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace fizmo {
namespace audio {

namespace {

namespace mt = detail::mp3tables;

const double k_pi = 3.14159265358979323846;

const std::uint8_t k_band_long[9][22] = {
    { 4, 4, 4, 4, 4, 4, 6, 6, 8, 8, 10, 12, 16, 20, 24, 28, 34, 42, 50, 54, 76, 158 },
    { 4, 4, 4, 4, 4, 4, 6, 6, 6, 8, 10, 12, 16, 18, 22, 28, 34, 40, 46, 54, 54, 192 },
    { 4, 4, 4, 4, 4, 4, 6, 6, 8, 10, 12, 16, 20, 24, 30, 38, 46, 56, 68, 84, 102, 26 },
    { 6, 6, 6, 6, 6, 6, 8, 10, 12, 14, 16, 20, 24, 28, 32, 38, 46, 52, 60, 68, 58, 54 },
    { 6, 6, 6, 6, 6, 6, 8, 10, 12, 14, 16, 18, 22, 26, 32, 38, 46, 54, 62, 70, 76, 36 },
    { 6, 6, 6, 6, 6, 6, 8, 10, 12, 14, 16, 20, 24, 28, 32, 38, 46, 52, 60, 68, 58, 54 },
    { 6, 6, 6, 6, 6, 6, 8, 10, 12, 14, 16, 20, 24, 28, 32, 38, 46, 52, 60, 68, 58, 54 },
    { 6, 6, 6, 6, 6, 6, 8, 10, 12, 14, 16, 20, 24, 28, 32, 38, 46, 52, 60, 68, 58, 54 },
    { 12, 12, 12, 12, 12, 12, 16, 20, 24, 28, 32, 40, 48, 56, 64, 76, 90, 2, 2, 2, 2, 2 },
};

const std::uint8_t k_band_short[9][13] = {
    { 4, 4, 4, 4, 6, 8, 10, 12, 14, 18, 22, 30, 56 },
    { 4, 4, 4, 4, 6, 6, 10, 12, 14, 16, 20, 26, 66 },
    { 4, 4, 4, 4, 6, 8, 12, 16, 20, 26, 34, 42, 12 },
    { 4, 4, 4, 6, 6, 8, 10, 14, 18, 26, 32, 42, 18 },
    { 4, 4, 4, 6, 8, 10, 12, 14, 18, 24, 32, 44, 12 },
    { 4, 4, 4, 6, 8, 10, 12, 14, 18, 24, 30, 40, 18 },
    { 4, 4, 4, 6, 8, 10, 12, 14, 18, 24, 30, 40, 18 },
    { 4, 4, 4, 6, 8, 10, 12, 14, 18, 24, 30, 40, 18 },
    { 8, 8, 8, 12, 16, 20, 24, 28, 36, 2, 2, 2, 26 },
};

const std::uint8_t k_pretab[22] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 3, 3, 3, 2, 0 };
const std::uint8_t k_slen[2][16] = {
    { 0, 0, 0, 0, 3, 1, 1, 1, 2, 2, 2, 3, 3, 3, 4, 4 },
    { 0, 1, 2, 3, 0, 1, 2, 3, 1, 2, 3, 1, 2, 3, 2, 3 },
};
const std::uint8_t k_lsf_nsf[6][3][4] = {
    { { 6, 5, 5, 5 }, { 9, 9, 9, 9 }, { 6, 9, 9, 9 } },
    { { 6, 5, 7, 3 }, { 9, 9, 12, 6 }, { 6, 9, 12, 6 } },
    { { 11, 10, 0, 0 }, { 18, 18, 0, 0 }, { 15, 18, 0, 0 } },
    { { 7, 7, 7, 0 }, { 12, 12, 12, 0 }, { 6, 15, 12, 0 } },
    { { 6, 6, 6, 3 }, { 12, 9, 9, 6 }, { 6, 12, 9, 6 } },
    { { 8, 8, 5, 0 }, { 15, 12, 9, 0 }, { 6, 18, 9, 0 } },
};

const std::uint16_t k_bitrates[2][3][15] = {
    { { 0, 32, 64, 96, 128, 160, 192, 224, 256, 288, 320, 352, 384, 416, 448 },
      { 0, 32, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320, 384 },
      { 0, 32, 40, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320 } },
    { { 0, 32, 48, 56, 64, 80, 96, 112, 128, 144, 160, 176, 192, 224, 256 },
      { 0, 8, 16, 24, 32, 40, 48, 56, 64, 80, 96, 112, 128, 144, 160 },
      { 0, 8, 16, 24, 32, 40, 48, 56, 64, 80, 96, 112, 128, 144, 160 } },
};
const std::uint32_t k_rates[3] = { 44100, 48000, 32000 };

const char* const k_genres[192] = {
    "Blues", "Classic Rock", "Country", "Dance", "Disco", "Funk", "Grunge", "Hip-Hop", "Jazz", "Metal", "New Age", "Oldies", "Other",
    "Pop", "R&B", "Rap", "Reggae", "Rock", "Techno", "Industrial", "Alternative", "Ska", "Death Metal", "Pranks", "Soundtrack",
    "Euro-Techno", "Ambient", "Trip-Hop", "Vocal", "Jazz+Funk", "Fusion", "Trance", "Classical", "Instrumental", "Acid", "House", "Game",
    "Sound Clip", "Gospel", "Noise", "AlternRock", "Bass", "Soul", "Punk", "Space", "Meditative", "Instrumental Pop", "Instrumental Rock",
    "Ethnic", "Gothic", "Darkwave", "Techno-Industrial", "Electronic", "Pop-Folk", "Eurodance", "Dream", "Southern Rock", "Comedy", "Cult",
    "Gangsta", "Top 40", "Christian Rap", "Pop/Funk", "Jungle", "Native American", "Cabaret", "New Wave", "Psychedelic", "Rave",
    "Showtunes", "Trailer", "Lo-Fi", "Tribal", "Acid Punk", "Acid Jazz", "Polka", "Retro", "Musical", "Rock & Roll", "Hard Rock", "Folk",
    "Folk-Rock", "National Folk", "Swing", "Fast Fusion", "Bebop", "Latin", "Revival", "Celtic", "Bluegrass", "Avantgarde", "Gothic Rock",
    "Progressive Rock", "Psychedelic Rock", "Symphonic Rock", "Slow Rock", "Big Band", "Chorus", "Easy Listening", "Acoustic", "Humour",
    "Speech", "Chanson", "Opera", "Chamber Music", "Sonata", "Symphony", "Booty Bass", "Primus", "Porn Groove", "Satire", "Slow Jam",
    "Club", "Tango", "Samba", "Folklore", "Ballad", "Power Ballad", "Rhythmic Soul", "Freestyle", "Duet", "Punk Rock", "Drum Solo",
    "A Cappella", "Euro-House", "Dance Hall", "Goa", "Drum & Bass", "Club-House", "Hardcore Techno", "Terror", "Indie", "BritPop",
    "Negerpunk", "Polsk Punk", "Beat", "Christian Gangsta Rap", "Heavy Metal", "Black Metal", "Crossover", "Contemporary Christian",
    "Christian Rock", "Merengue", "Salsa", "Thrash Metal", "Anime", "Jpop", "Synthpop", "Abstract", "Art Rock", "Baroque", "Bhangra",
    "Big Beat", "Breakbeat", "Chillout", "Downtempo", "Dub", "EBM", "Eclectic", "Electro", "Electroclash", "Emo", "Experimental", "Garage",
    "Global", "IDM", "Illbient", "Industro-Goth", "Jam Band", "Krautrock", "Leftfield", "Lounge", "Math Rock", "New Romantic", "Nu-Breakz",
    "Post-Punk", "Post-Rock", "Psytrance", "Shoegaze", "Space Rock", "Trop Rock", "World Music", "Neoclassical", "Audiobook",
    "Audio Theatre", "Neue Deutsche Welle", "Podcast", "Indie Rock", "G-Funk", "Dubstep", "Garage Rock", "Psybient",
};

std::uint64_t load_be64(const std::uint8_t* p) noexcept {
    return (static_cast<std::uint64_t>(p[0]) << 56) | (static_cast<std::uint64_t>(p[1]) << 48) |
           (static_cast<std::uint64_t>(p[2]) << 40) | (static_cast<std::uint64_t>(p[3]) << 32) |
           (static_cast<std::uint64_t>(p[4]) << 24) | (static_cast<std::uint64_t>(p[5]) << 16) |
           (static_cast<std::uint64_t>(p[6]) << 8) | static_cast<std::uint64_t>(p[7]);
}

std::uint32_t load_be32(const std::uint8_t* p) noexcept {
    return (static_cast<std::uint32_t>(p[0]) << 24) | (static_cast<std::uint32_t>(p[1]) << 16) |
           (static_cast<std::uint32_t>(p[2]) << 8) | static_cast<std::uint32_t>(p[3]);
}

class BitReader {
public:
    BitReader(const std::uint8_t* data, std::size_t size) noexcept : m_data(data), m_size(size) {}

    std::uint32_t peek(int n) const noexcept {
        const std::size_t byte = m_pos >> 3;
        std::uint64_t v = 0;
        if (byte < m_size && m_size - byte >= 8) {
            v = load_be64(m_data + byte);
        } else {
            for (std::size_t i = 0; i < 8; ++i) {
                v <<= 8;
                if (byte < m_size && i < m_size - byte) v |= m_data[byte + i];
            }
        }
        v <<= (m_pos & 7);
        return static_cast<std::uint32_t>(v >> (64 - n));
    }

    std::uint32_t read(int n) noexcept {
        if (n <= 0) return 0;
        const std::uint32_t v = peek(n);
        m_pos += static_cast<std::size_t>(n);
        return v;
    }

    int bit() noexcept { return static_cast<int>(read(1)); }
    void skip(std::size_t n) noexcept { m_pos += n; }
    std::size_t pos() const noexcept { return m_pos; }
    void set_pos(std::size_t p) noexcept { m_pos = p; }

private:
    const std::uint8_t* m_data;
    std::size_t m_size;
    std::size_t m_pos = 0;
};

struct HuffCode {
    std::uint32_t code;
    int length;
    int symbol;
};

void build_lut(std::vector<std::uint32_t>& lut, std::size_t at, int bits, const std::vector<HuffCode>& codes) {
    const std::size_t count = std::size_t(1) << bits;
    for (std::size_t i = 0; i < count; ++i) lut[at + i] = static_cast<std::uint32_t>(bits) << 8;
    std::vector<std::vector<HuffCode>> groups(count);
    std::vector<int> deepest(count, 0);
    for (const HuffCode& c : codes) {
        if (c.length <= bits) {
            const std::size_t first = static_cast<std::size_t>(c.code) << (bits - c.length);
            const std::size_t n = std::size_t(1) << (bits - c.length);
            for (std::size_t i = 0; i < n; ++i) lut[at + first + i] = (static_cast<std::uint32_t>(c.length) << 8) | static_cast<std::uint32_t>(c.symbol);
        } else {
            const int rest = c.length - bits;
            const std::size_t prefix = static_cast<std::size_t>(c.code >> rest);
            groups[prefix].push_back({ c.code & ((1u << rest) - 1u), rest, c.symbol });
            deepest[prefix] = std::max(deepest[prefix], rest);
        }
    }
    for (std::size_t p = 0; p < count; ++p) {
        if (groups[p].empty()) continue;
        const int sub = std::min(deepest[p], 8);
        const std::size_t offset = lut.size();
        lut.resize(offset + (std::size_t(1) << sub));
        lut[at + p] = 0x80000000u | (static_cast<std::uint32_t>(sub) << 24) | static_cast<std::uint32_t>(offset);
        build_lut(lut, offset, sub, groups[p]);
    }
}

struct Tables {
    float pow43[8207];
    float pow2q[640];
    std::vector<std::uint32_t> lut;
    std::size_t lut_start[34];
    int lut_root[34];
    float dct3_even[9][12];
    float dct4_odd[9][12];
    float dct4_scale[18];
    float dct4_short[6][6];
    float win_long[4][36];
    float win_short[12];
    float synth_window[512];
    float dct_coef[32];
    float cs[8];
    float ca[8];
    float is_mpeg1[7][2];
    float is_lsf[2][32][2];
    float l12_scale[64];
    std::uint16_t long_index[9][23];
    std::uint16_t short_index[9][14];
    std::uint8_t l2_class[5][32][16];

    Tables() {
        for (int i = 0; i < 8207; ++i) pow43[i] = static_cast<float>(std::pow(static_cast<double>(i), 4.0 / 3.0));
        for (int i = 0; i < 640; ++i) pow2q[i] = static_cast<float>(std::pow(2.0, (i - 512) / 4.0));
        for (int t = 0; t < 34; ++t) {
            lut_start[t] = 0;
            lut_root[t] = 0;
            const mt::HuffmanTableSpec& spec = mt::k_huffman_tables[t];
            if (!spec.codewords || spec.data_table != t) continue;
            const bool quad = t >= 32;
            const int count = quad ? 16 : spec.xlen * spec.ylen;
            std::vector<HuffCode> codes;
            int maxlen = 0;
            for (int i = 0; i < count; ++i) {
                const mt::HuffmanCodeword& w = spec.codewords[i];
                const int symbol = quad ? i : (((i / spec.ylen) << 4) | (i % spec.ylen));
                codes.push_back({ w.code, static_cast<int>(w.length), symbol });
                maxlen = std::max(maxlen, static_cast<int>(w.length));
            }
            const int root = std::min(maxlen, quad ? 6 : 8);
            lut_start[t] = lut.size();
            lut_root[t] = root;
            lut.resize(lut.size() + (std::size_t(1) << root));
            build_lut(lut, lut_start[t], root, codes);
        }
        for (int t = 0; t < 34; ++t) {
            const int d = mt::k_huffman_tables[t].data_table;
            lut_start[t] = lut_start[d];
            lut_root[t] = lut_root[d];
        }
        for (int p = 0; p < 9; ++p) {
            for (int n = 0; n < 12; ++n) {
                dct3_even[p][n] = 0.0f;
                dct4_odd[p][n] = 0.0f;
            }
            for (int n = 0; n < 9; ++n) {
                dct3_even[p][n] = static_cast<float>(std::cos(k_pi / 18.0 * (2 * n + 1) * p));
                dct4_odd[p][n] = static_cast<float>(std::cos(k_pi / 36.0 * (2 * n + 1) * (2 * p + 1)));
            }
        }
        for (int n = 0; n < 18; ++n) dct4_scale[n] = static_cast<float>(0.5 / std::cos(k_pi / 72.0 * (2 * n + 1)));
        for (int k = 0; k < 6; ++k) {
            for (int n = 0; n < 6; ++n) dct4_short[k][n] = static_cast<float>(std::cos(k_pi / 24.0 * (2 * n + 1) * (2 * k + 1)));
        }
        for (int i = 0; i < 36; ++i) {
            const float normal = static_cast<float>(std::sin(k_pi / 36.0 * (i + 0.5)));
            win_long[0][i] = normal;
            win_long[2][i] = 0.0f;
            if (i < 18) win_long[1][i] = normal;
            else if (i < 24) win_long[1][i] = 1.0f;
            else if (i < 30) win_long[1][i] = static_cast<float>(std::sin(k_pi / 12.0 * (i - 18 + 0.5)));
            else win_long[1][i] = 0.0f;
            if (i < 6) win_long[3][i] = 0.0f;
            else if (i < 12) win_long[3][i] = static_cast<float>(std::sin(k_pi / 12.0 * (i - 6 + 0.5)));
            else if (i < 18) win_long[3][i] = 1.0f;
            else win_long[3][i] = normal;
        }
        for (int i = 0; i < 12; ++i) win_short[i] = static_cast<float>(std::sin(k_pi / 12.0 * (i + 0.5)));
        for (int i = 0; i < 512; ++i) synth_window[i] = static_cast<float>(mt::k_synthesis_window[i]);
        int at = 0;
        for (int n = 32; n >= 2; n /= 2) {
            for (int i = 0; i < n / 2; ++i) dct_coef[at + i] = static_cast<float>(0.5 / std::cos(k_pi * (2 * i + 1) / (2.0 * n)));
            at += n / 2;
        }
        dct_coef[31] = 0.0f;
        static const double c[8] = { -0.6, -0.535, -0.33, -0.185, -0.095, -0.041, -0.0142, -0.0037 };
        for (int i = 0; i < 8; ++i) {
            const double s = std::sqrt(1.0 + c[i] * c[i]);
            cs[i] = static_cast<float>(1.0 / s);
            ca[i] = static_cast<float>(c[i] / s);
        }
        for (int i = 0; i < 7; ++i) {
            if (i == 6) {
                is_mpeg1[i][0] = 1.0f;
                is_mpeg1[i][1] = 0.0f;
            } else {
                const double r = std::tan(i * k_pi / 12.0);
                is_mpeg1[i][0] = static_cast<float>(r / (1.0 + r));
                is_mpeg1[i][1] = static_cast<float>(1.0 / (1.0 + r));
            }
        }
        for (int j = 0; j < 2; ++j) {
            for (int i = 0; i < 32; ++i) {
                const double f = std::pow(2.0, -(j + 1) * ((i + 1) >> 1) / 4.0);
                if (i & 1) {
                    is_lsf[j][i][0] = static_cast<float>(f);
                    is_lsf[j][i][1] = 1.0f;
                } else {
                    is_lsf[j][i][0] = 1.0f;
                    is_lsf[j][i][1] = static_cast<float>(f);
                }
            }
        }
        for (int a = 0; a < 5; ++a) {
            for (int sb = 0; sb < 32; ++sb) {
                for (int idx = 0; idx < 16; ++idx) {
                    const std::uint16_t want = mt::k_layer2_allocations[a].steps[sb][idx];
                    int cls = 0;
                    while (cls < 16 && mt::k_layer2_classes[cls].steps != want) ++cls;
                    l2_class[a][sb][idx] = static_cast<std::uint8_t>(cls);
                }
            }
        }
        for (int i = 0; i < 64; ++i) l12_scale[i] = static_cast<float>(std::pow(2.0, 1.0 - i / 3.0));
        for (int r = 0; r < 9; ++r) {
            long_index[r][0] = 0;
            for (int b = 0; b < 22; ++b) long_index[r][b + 1] = static_cast<std::uint16_t>(long_index[r][b] + k_band_long[r][b]);
            short_index[r][0] = 0;
            for (int b = 0; b < 13; ++b) short_index[r][b + 1] = static_cast<std::uint16_t>(short_index[r][b] + k_band_short[r][b]);
        }
    }
};

const Tables& tables() {
    static const Tables t;
    return t;
}

inline int huff_decode(BitReader& br, const std::uint32_t* lut, std::size_t start, int root) noexcept {
    int bits = root;
    std::uint32_t e = lut[start + br.peek(bits)];
    while (e & 0x80000000u) {
        br.skip(static_cast<std::size_t>(bits));
        bits = static_cast<int>((e >> 24) & 31u);
        e = lut[(e & 0xFFFFFFu) + br.peek(bits)];
    }
    br.skip((e >> 8) & 0xFFu);
    return static_cast<int>(e & 0xFFu);
}

template <int N>
struct FastDct {
    static void run(float* x, float* t, const float* coef) noexcept {
        const float* k = coef + (32 - N);
        for (int i = 0; i < N / 2; ++i) {
            const float a = x[i];
            const float b = x[N - 1 - i];
            t[i] = a + b;
            t[N / 2 + i] = (a - b) * k[i];
        }
        FastDct<N / 2>::run(t, x, coef);
        FastDct<N / 2>::run(t + N / 2, x, coef);
        for (int i = 0; i < N / 2 - 1; ++i) {
            x[2 * i] = t[i];
            x[2 * i + 1] = t[N / 2 + i] + t[N / 2 + i + 1];
        }
        x[N - 2] = t[N / 2 - 1];
        x[N - 1] = t[N - 1];
    }
};

template <>
struct FastDct<4> {
    static void run(float* x, float*, const float* coef) noexcept {
        const float* k = coef + 28;
        const float c2 = coef[30];
        const float a0 = x[0] + x[3];
        const float a1 = x[1] + x[2];
        const float b0 = (x[0] - x[3]) * k[0];
        const float b1 = (x[1] - x[2]) * k[1];
        const float o1 = (b0 - b1) * c2;
        x[0] = a0 + a1;
        x[1] = b0 + b1 + o1;
        x[2] = (a0 - a1) * c2;
        x[3] = o1;
    }
};

struct FrameHeader {
    std::uint32_t raw = 0;
    int version = 0;
    int layer = 0;
    bool crc = false;
    int bitrate_index = 0;
    int padding = 0;
    int mode = 0;
    int mode_ext = 0;
    int channels = 0;
    bool lsf = false;
    int sr_index = 0;
    std::uint32_t sample_rate = 0;
    std::uint32_t bitrate = 0;
    int samples = 0;
    std::size_t size = 0;
    std::size_t side_info = 0;
};

const std::uint32_t k_key_mask = 0xFFFE0C00u;

bool header_valid(std::uint32_t h) noexcept {
    if ((h & 0xFFE00000u) != 0xFFE00000u) return false;
    if (((h >> 19) & 3u) == 1u) return false;
    if (((h >> 17) & 3u) == 0u) return false;
    if (((h >> 12) & 15u) == 15u) return false;
    if (((h >> 10) & 3u) == 3u) return false;
    return true;
}

std::size_t free_slot_bytes(const FrameHeader& f) noexcept {
    return f.layer == 1 ? 4u : 1u;
}

bool parse_header(std::uint32_t h, std::size_t free_size, FrameHeader& f) noexcept {
    if (!header_valid(h)) return false;
    f.raw = h;
    f.version = static_cast<int>((h >> 19) & 3u);
    f.layer = 4 - static_cast<int>((h >> 17) & 3u);
    f.crc = ((h >> 16) & 1u) == 0;
    f.bitrate_index = static_cast<int>((h >> 12) & 15u);
    const int rate = static_cast<int>((h >> 10) & 3u);
    f.padding = static_cast<int>((h >> 9) & 1u);
    f.mode = static_cast<int>((h >> 6) & 3u);
    f.mode_ext = static_cast<int>((h >> 4) & 3u);
    f.channels = f.mode == 3 ? 1 : 2;
    f.lsf = f.version != 3;
    f.sr_index = (f.version == 3 ? 0 : (f.version == 2 ? 3 : 6)) + rate;
    f.sample_rate = k_rates[rate] >> (f.version == 3 ? 0 : (f.version == 2 ? 1 : 2));
    f.samples = f.layer == 1 ? 384 : ((f.layer == 3 && f.lsf) ? 576 : 1152);
    f.side_info = f.layer == 3 ? (f.lsf ? (f.channels == 1 ? 9u : 17u) : (f.channels == 1 ? 17u : 32u)) : 0u;
    if (f.bitrate_index == 0) {
        if (free_size == 0) {
            f.size = 0;
            f.bitrate = 0;
            return true;
        }
        f.size = free_size + (f.padding ? free_slot_bytes(f) : 0u);
        const std::uint64_t per = f.layer == 1 ? 48u : ((f.layer == 3 && f.lsf) ? 72u : 144u);
        f.bitrate = static_cast<std::uint32_t>((static_cast<std::uint64_t>(free_size) * f.sample_rate + per * 500u) / (per * 1000u));
        return true;
    }
    f.bitrate = k_bitrates[f.lsf ? 1 : 0][f.layer - 1][f.bitrate_index];
    if (f.layer == 1) {
        f.size = (12000u * f.bitrate / f.sample_rate + static_cast<std::uint32_t>(f.padding)) * 4u;
    } else {
        const std::uint32_t per = (f.layer == 3 && f.lsf) ? 72000u : 144000u;
        f.size = per * f.bitrate / f.sample_rate + static_cast<std::uint32_t>(f.padding);
    }
    return true;
}

std::uint16_t crc16_update(std::uint16_t crc, const std::uint8_t* p, std::size_t n) noexcept {
    for (std::size_t i = 0; i < n; ++i) {
        crc = static_cast<std::uint16_t>(crc ^ (static_cast<std::uint16_t>(p[i]) << 8));
        for (int b = 0; b < 8; ++b) crc = static_cast<std::uint16_t>((crc & 0x8000u) ? ((crc << 1) ^ 0x8005u) : (crc << 1));
    }
    return crc;
}

struct Granule {
    int part23 = 0;
    int big_values = 0;
    int global_gain = 0;
    int sf_compress = 0;
    int block_type = 0;
    bool mixed = false;
    int table[3] = { 0, 0, 0 };
    int subblock_gain[3] = { 0, 0, 0 };
    int region1 = 0;
    int region2 = 0;
    int preflag = 0;
    int sf_scale = 0;
    int count1_table = 0;
    int long_end = 22;
    int short_start = 13;
};

struct SideInfo {
    int main_data_begin = 0;
    int scfsi[2] = { 0, 0 };
    Granule gr[2][2];
};

class Decoder {
public:
    Decoder() : m_t(tables()) { reset(); }

    void reset() noexcept {
        std::memset(m_overlap, 0, sizeof(m_overlap));
        std::memset(m_synth_v, 0, sizeof(m_synth_v));
        m_synth_pos[0] = 0;
        m_synth_pos[1] = 0;
        std::memset(m_sf, 0, sizeof(m_sf));
        std::memset(m_sf_max, 0, sizeof(m_sf_max));
        m_reservoir.clear();
    }

    static std::size_t main_offset(const FrameHeader& h) noexcept {
        return 4u + (h.crc ? 2u : 0u) + h.side_info;
    }

    void feed(const std::uint8_t* frame, std::size_t size, const FrameHeader& h) {
        const std::size_t off = main_offset(h);
        if (h.layer != 3 || off >= size) return;
        m_reservoir.insert(m_reservoir.end(), frame + off, frame + size);
        if (m_reservoir.size() > k_reservoir_keep) m_reservoir.erase(m_reservoir.begin(), m_reservoir.end() - static_cast<std::ptrdiff_t>(k_reservoir_keep));
    }

    void decode(const std::uint8_t* frame, std::size_t size, const FrameHeader& h, float* out, int out_channels) {
        int slots = 0;
        if (h.layer == 3) slots = decode_layer3(frame, size, h);
        else if (h.layer == 2) slots = decode_layer2(frame, size, h);
        else slots = decode_layer1(frame, size, h);
        synthesize(h.channels, slots, out, out_channels);
    }

private:
    static const std::size_t k_reservoir_keep = 2048;

    void synth_slot(int ch, const float* s, float* out, std::size_t stride) noexcept {
        float x[32];
        float scratch[32];
        std::memcpy(x, s, sizeof(x));
        FastDct<32>::run(x, scratch, m_t.dct_coef);
        const int head = m_synth_pos[ch];
        float* fresh = m_synth_v[ch][head];
        for (int i = 0; i < 64; ++i) {
            const int m = i + 16;
            if (m < 32) fresh[i] = x[m];
            else if (m == 32) fresh[i] = 0.0f;
            else if (m < 64) fresh[i] = -x[64 - m];
            else fresh[i] = -x[m - 64];
        }
        std::memcpy(m_synth_v[ch][head + 16], fresh, sizeof(float) * 64);
        float sum[32] = {};
        const float* d = m_t.synth_window;
        for (int i = 0; i < 8; ++i) {
            const float* lower = m_synth_v[ch][head + 2 * i];
            const float* upper = m_synth_v[ch][head + 2 * i + 1] + 32;
            const float* d_lower = d + 64 * i;
            const float* d_upper = d_lower + 32;
            for (int j = 0; j < 32; ++j) sum[j] += d_lower[j] * lower[j] + d_upper[j] * upper[j];
        }
        for (int j = 0; j < 32; ++j) out[static_cast<std::size_t>(j) * stride] = sum[j];
        m_synth_pos[ch] = (head + 15) & 15;
    }

    void synthesize(int channels, int slots, float* out, int out_channels) {
        const std::size_t oc = static_cast<std::size_t>(out_channels);
        if (channels == out_channels) {
            for (int ch = 0; ch < channels; ++ch) {
                for (int s = 0; s < slots; ++s) synth_slot(ch, m_sb[ch][s], out + static_cast<std::size_t>(s) * 32u * oc + static_cast<std::size_t>(ch), oc);
            }
            return;
        }
        float tmp[2][32];
        for (int s = 0; s < slots; ++s) {
            for (int ch = 0; ch < channels; ++ch) synth_slot(ch, m_sb[ch][s], tmp[ch], 1);
            float* o = out + static_cast<std::size_t>(s) * 32u * oc;
            for (int j = 0; j < 32; ++j) {
                if (channels == 1) {
                    for (std::size_t c = 0; c < oc; ++c) o[static_cast<std::size_t>(j) * oc + c] = tmp[0][j];
                } else {
                    const float m = 0.5f * (tmp[0][j] + tmp[1][j]);
                    for (std::size_t c = 0; c < oc; ++c) o[static_cast<std::size_t>(j) * oc + c] = m;
                }
            }
        }
    }

    int decode_layer1(const std::uint8_t* frame, std::size_t size, const FrameHeader& h) {
        std::memset(m_sb, 0, sizeof(m_sb));
        BitReader br(frame, size);
        br.set_pos((4u + (h.crc ? 2u : 0u)) * 8u);
        const int nch = h.channels;
        const int bound = h.mode == 1 ? (h.mode_ext + 1) * 4 : 32;
        int alloc[2][32] = {};
        int scale[2][32] = {};
        for (int i = 0; i < bound; ++i) {
            for (int ch = 0; ch < nch; ++ch) alloc[ch][i] = static_cast<int>(br.read(4));
        }
        for (int i = bound; i < 32; ++i) {
            alloc[0][i] = static_cast<int>(br.read(4));
            alloc[1][i] = alloc[0][i];
        }
        for (int i = 0; i < 32; ++i) {
            for (int ch = 0; ch < nch; ++ch) {
                if (alloc[ch][i]) scale[ch][i] = static_cast<int>(br.read(6));
            }
        }
        for (int s = 0; s < 12; ++s) {
            for (int i = 0; i < 32; ++i) {
                const int chans = i < bound ? nch : 1;
                for (int ch = 0; ch < chans; ++ch) {
                    const int n = alloc[ch][i];
                    if (!n) continue;
                    const int bits = n + 1;
                    const int steps = (1 << bits) - 1;
                    const int mant = static_cast<int>(br.read(bits));
                    const float v = static_cast<float>(mant - (steps >> 1)) * (2.0f / static_cast<float>(steps));
                    if (i < bound) {
                        m_sb[ch][s][i] = v * m_t.l12_scale[scale[ch][i]];
                    } else {
                        m_sb[0][s][i] = v * m_t.l12_scale[scale[0][i]];
                        if (nch == 2) m_sb[1][s][i] = v * m_t.l12_scale[scale[1][i]];
                    }
                }
            }
        }
        return 12;
    }

    int decode_layer2(const std::uint8_t* frame, std::size_t size, const FrameHeader& h) {
        std::memset(m_sb, 0, sizeof(m_sb));
        BitReader br(frame, size);
        br.set_pos((4u + (h.crc ? 2u : 0u)) * 8u);
        const int nch = h.channels;
        int table = 4;
        if (!h.lsf) {
            const std::uint32_t per = h.bitrate / static_cast<std::uint32_t>(nch);
            if ((h.sample_rate == 48000 && per >= 56) || (per >= 56 && per <= 80)) table = 0;
            else if (h.sample_rate != 48000 && per >= 96) table = 1;
            else if (h.sample_rate != 32000 && per <= 48) table = 2;
            else table = 3;
        }
        const mt::Layer2Allocation& al = mt::k_layer2_allocations[table];
        const std::uint8_t* classes = &m_t.l2_class[table][0][0];
        const int sblimit = al.sblimit;
        int bound = h.mode == 1 ? (h.mode_ext + 1) * 4 : sblimit;
        if (bound > sblimit) bound = sblimit;
        int alloc[2][32] = {};
        int scfsi[2][32] = {};
        int scale[2][32][3] = {};
        for (int i = 0; i < bound; ++i) {
            for (int ch = 0; ch < nch; ++ch) alloc[ch][i] = static_cast<int>(br.read(al.nbal[i]));
        }
        for (int i = bound; i < sblimit; ++i) {
            alloc[0][i] = static_cast<int>(br.read(al.nbal[i]));
            alloc[1][i] = alloc[0][i];
        }
        for (int i = 0; i < sblimit; ++i) {
            for (int ch = 0; ch < nch; ++ch) {
                if (alloc[ch][i]) scfsi[ch][i] = static_cast<int>(br.read(2));
            }
        }
        for (int i = 0; i < sblimit; ++i) {
            for (int ch = 0; ch < nch; ++ch) {
                if (!alloc[ch][i]) continue;
                int* sf = scale[ch][i];
                switch (scfsi[ch][i]) {
                    case 0:
                        sf[0] = static_cast<int>(br.read(6));
                        sf[1] = static_cast<int>(br.read(6));
                        sf[2] = static_cast<int>(br.read(6));
                        break;
                    case 1:
                        sf[0] = static_cast<int>(br.read(6));
                        sf[1] = sf[0];
                        sf[2] = static_cast<int>(br.read(6));
                        break;
                    case 2:
                        sf[0] = static_cast<int>(br.read(6));
                        sf[1] = sf[0];
                        sf[2] = sf[0];
                        break;
                    default:
                        sf[0] = static_cast<int>(br.read(6));
                        sf[1] = static_cast<int>(br.read(6));
                        sf[2] = sf[1];
                        break;
                }
            }
        }
        for (int part = 0; part < 3; ++part) {
            for (int g = 0; g < 4; ++g) {
                const int base = part * 12 + g * 3;
                for (int i = 0; i < sblimit; ++i) {
                    const int chans = i < bound ? nch : 1;
                    for (int ch = 0; ch < chans; ++ch) {
                        const int b = alloc[ch][i];
                        if (!b) continue;
                        const mt::Layer2Class& qc = mt::k_layer2_classes[classes[i * 16 + b]];
                        const int steps = qc.steps;
                        const int bits = qc.bits;
                        int v[3];
                        if (qc.grouped) {
                            int code = static_cast<int>(br.read(bits));
                            for (int m = 0; m < 3; ++m) {
                                v[m] = code % steps;
                                code /= steps;
                            }
                        } else {
                            for (int m = 0; m < 3; ++m) v[m] = static_cast<int>(br.read(bits));
                        }
                        const float norm = 2.0f / static_cast<float>(steps);
                        for (int m = 0; m < 3; ++m) {
                            const float f = static_cast<float>(v[m] - (steps >> 1)) * norm;
                            if (i < bound) {
                                m_sb[ch][base + m][i] = f * m_t.l12_scale[scale[ch][i][part]];
                            } else {
                                m_sb[0][base + m][i] = f * m_t.l12_scale[scale[0][i][part]];
                                if (nch == 2) m_sb[1][base + m][i] = f * m_t.l12_scale[scale[1][i][part]];
                            }
                        }
                    }
                }
            }
        }
        return 36;
    }

    bool parse_side_info(BitReader& br, const FrameHeader& h, SideInfo& si) const {
        const int nch = h.channels;
        const int ngr = h.lsf ? 1 : 2;
        if (h.lsf) {
            si.main_data_begin = static_cast<int>(br.read(8));
            br.skip(static_cast<std::size_t>(nch));
        } else {
            si.main_data_begin = static_cast<int>(br.read(9));
            br.skip(nch == 2 ? 3u : 5u);
            for (int ch = 0; ch < nch; ++ch) si.scfsi[ch] = static_cast<int>(br.read(4));
        }
        bool ok = true;
        const std::uint16_t* li = m_t.long_index[h.sr_index];
        const std::uint16_t* si_short = m_t.short_index[h.sr_index];
        for (int gr = 0; gr < ngr; ++gr) {
            for (int ch = 0; ch < nch; ++ch) {
                Granule& g = si.gr[gr][ch];
                g = Granule();
                g.part23 = static_cast<int>(br.read(12));
                g.big_values = static_cast<int>(br.read(9));
                if (g.big_values > 288) ok = false;
                g.global_gain = static_cast<int>(br.read(8));
                g.sf_compress = static_cast<int>(br.read(h.lsf ? 9 : 4));
                const bool window_switch = br.bit() != 0;
                if (window_switch) {
                    g.block_type = static_cast<int>(br.read(2));
                    if (g.block_type == 0) ok = false;
                    g.mixed = br.bit() != 0;
                    g.table[0] = static_cast<int>(br.read(5));
                    g.table[1] = static_cast<int>(br.read(5));
                    g.table[2] = 0;
                    for (int i = 0; i < 3; ++i) g.subblock_gain[i] = static_cast<int>(br.read(3));
                    g.region1 = g.block_type == 2 ? si_short[3] * 3 : li[8];
                    g.region2 = 576;
                } else {
                    for (int i = 0; i < 3; ++i) g.table[i] = static_cast<int>(br.read(5));
                    const int r0 = static_cast<int>(br.read(4));
                    const int r1 = static_cast<int>(br.read(3));
                    g.region1 = li[std::min(r0 + 1, 22)];
                    g.region2 = li[std::min(r0 + r1 + 2, 22)];
                }
                if (!h.lsf) g.preflag = br.bit();
                g.sf_scale = br.bit();
                g.count1_table = br.bit();
                if (g.block_type == 2) {
                    if (g.mixed) {
                        g.long_end = h.lsf ? 6 : 8;
                        g.short_start = 3;
                    } else {
                        g.long_end = 0;
                        g.short_start = 0;
                    }
                } else {
                    g.long_end = 22;
                    g.short_start = 13;
                }
                if (h.mode == 1 && (h.mode_ext & 3) == 2) g.global_gain -= 2;
            }
        }
        return ok;
    }

    void read_scalefactors(BitReader& br, const FrameHeader& h, int gr, int ch, Granule& g, int scfsi) {
        std::uint8_t* sf = m_sf[gr][ch];
        std::uint8_t* mx = m_sf_max[gr][ch];
        std::memset(sf, 0, 40);
        std::memset(mx, 0, 40);
        if (!h.lsf) {
            const int slen1 = k_slen[0][g.sf_compress];
            const int slen2 = k_slen[1][g.sf_compress];
            if (g.block_type == 2) {
                const int n = g.mixed ? 17 : 18;
                int j = 0;
                for (int i = 0; i < n; ++i) sf[j++] = static_cast<std::uint8_t>(br.read(slen1));
                for (int i = 0; i < 18; ++i) sf[j++] = static_cast<std::uint8_t>(br.read(slen2));
            } else {
                const std::uint8_t* prev = m_sf[0][ch];
                int j = 0;
                for (int k = 0; k < 4; ++k) {
                    const int n = k == 0 ? 6 : 5;
                    if (gr == 1 && (scfsi & (8 >> k))) {
                        for (int i = 0; i < n; ++i, ++j) sf[j] = prev[j];
                    } else {
                        const int slen = k < 2 ? slen1 : slen2;
                        for (int i = 0; i < n; ++i) sf[j++] = static_cast<std::uint8_t>(br.read(slen));
                    }
                }
            }
            return;
        }
        const int tindex = g.block_type == 2 ? (g.mixed ? 2 : 1) : 0;
        int sfc = g.sf_compress;
        int slen[4] = { 0, 0, 0, 0 };
        int tindex2 = 0;
        if ((h.mode_ext & 1) && ch == 1) {
            sfc >>= 1;
            if (sfc < 180) {
                slen[0] = sfc / 36;
                slen[1] = (sfc % 36) / 6;
                slen[2] = sfc % 6;
                tindex2 = 3;
            } else if (sfc < 244) {
                sfc -= 180;
                slen[0] = sfc / 16;
                slen[1] = (sfc % 16) / 4;
                slen[2] = sfc % 4;
                tindex2 = 4;
            } else {
                sfc -= 244;
                slen[0] = sfc / 3;
                slen[1] = sfc % 3;
                tindex2 = 5;
            }
        } else {
            if (sfc < 400) {
                slen[0] = (sfc >> 4) / 5;
                slen[1] = (sfc >> 4) % 5;
                slen[2] = (sfc & 15) >> 2;
                slen[3] = sfc & 3;
                tindex2 = 0;
            } else if (sfc < 500) {
                sfc -= 400;
                slen[0] = (sfc >> 2) / 5;
                slen[1] = (sfc >> 2) % 5;
                slen[2] = sfc & 3;
                tindex2 = 1;
            } else {
                sfc -= 500;
                slen[0] = sfc / 3;
                slen[1] = sfc % 3;
                tindex2 = 2;
                g.preflag = 1;
            }
        }
        int j = 0;
        for (int k = 0; k < 4; ++k) {
            const int n = k_lsf_nsf[tindex2][tindex][k];
            const int sl = std::min(slen[k], 7);
            for (int i = 0; i < n && j < 40; ++i) {
                sf[j] = static_cast<std::uint8_t>(br.read(sl));
                mx[j] = static_cast<std::uint8_t>((1 << sl) - 1);
                ++j;
            }
        }
    }

    int huffman(BitReader& br, std::size_t end, const Granule& g, int* is) const {
        int index = 0;
        const int big = std::min(g.big_values * 2, 576);
        const int bounds[3] = { std::min(g.region1, big), std::min(std::max(g.region2, g.region1), big), big };
        bool stop = false;
        for (int r = 0; r < 3 && !stop; ++r) {
            const int limit = bounds[r];
            if (index >= limit) continue;
            const int sel = g.table[r];
            const mt::HuffmanTableSpec& spec = mt::k_huffman_tables[sel];
            if (!spec.codewords) {
                for (; index < limit; ++index) is[index] = 0;
                continue;
            }
            const std::uint32_t* lut = m_t.lut.data();
            const std::size_t lut_start = m_t.lut_start[sel];
            const int root = m_t.lut_root[sel];
            const int linbits = spec.linbits;
            while (index < limit) {
                if (br.pos() >= end) {
                    stop = true;
                    break;
                }
                const int sym = huff_decode(br, lut, lut_start, root);
                int x = sym >> 4;
                int y = sym & 15;
                if (x == 15 && linbits) x += static_cast<int>(br.read(linbits));
                if (x && br.bit()) x = -x;
                if (y == 15 && linbits) y += static_cast<int>(br.read(linbits));
                if (y && br.bit()) y = -y;
                is[index] = x;
                is[index + 1] = y;
                index += 2;
            }
        }
        if (stop) {
            for (int i = index; i < 576; ++i) is[i] = 0;
            return index;
        }
        const int qt = 32 + g.count1_table;
        const std::uint32_t* qlut = m_t.lut.data();
        const std::size_t qstart = m_t.lut_start[qt];
        const int qroot = m_t.lut_root[qt];
        std::size_t last = 0;
        bool have_last = false;
        while (index <= 572) {
            const std::size_t pos = br.pos();
            if (pos >= end) {
                if (pos > end && have_last) index -= 4;
                break;
            }
            last = pos;
            have_last = true;
            const int sym = huff_decode(br, qlut, qstart, qroot);
            for (int k = 0; k < 4; ++k) {
                int v = (sym >> (3 - k)) & 1;
                if (v && br.bit()) v = -v;
                is[index + k] = v;
            }
            index += 4;
        }
        (void)last;
        for (int i = index; i < 576; ++i) is[i] = 0;
        return index;
    }

    void dequantize(const FrameHeader& h, const Granule& g, const std::uint8_t* sf, const int* is, int count, float* xr) const {
        const int shift = g.sf_scale + 1;
        const int base = g.global_gain - 210;
        const int r = h.sr_index;
        const std::uint16_t* li = m_t.long_index[r];
        auto gain = [&](int q) {
            q += 512;
            if (q < 0) q = 0;
            if (q > 639) q = 639;
            return m_t.pow2q[q];
        };
        auto apply = [&](int start, int end, float gv) {
            end = std::min(end, count);
            for (int i = start; i < end; ++i) {
                const int v = is[i];
                if (v == 0) {
                    xr[i] = 0.0f;
                } else if (v > 0) {
                    xr[i] = m_t.pow43[std::min(v, 8206)] * gv;
                } else {
                    xr[i] = -m_t.pow43[std::min(-v, 8206)] * gv;
                }
            }
        };
        for (int i = count; i < 576; ++i) xr[i] = 0.0f;
        for (int b = 0; b < g.long_end; ++b) {
            if (li[b] >= count) break;
            const int pre = g.preflag ? k_pretab[b] : 0;
            apply(li[b], li[b + 1], gain(base - ((sf[b] + pre) << shift)));
        }
        if (g.short_start < 13) {
            int pos = g.long_end == 0 ? 0 : li[g.long_end];
            int k = g.long_end;
            for (int b = g.short_start; b < 13; ++b) {
                const int len = k_band_short[r][b];
                for (int w = 0; w < 3; ++w) {
                    if (pos < count) apply(pos, pos + len, gain(base - 8 * g.subblock_gain[w] - (sf[k] << shift)));
                    ++k;
                    pos += len;
                }
            }
        }
    }

    bool intensity_illegal(const FrameHeader& h, int ch1_gr, int index) const noexcept {
        const int v = m_sf[ch1_gr][1][index];
        if (!h.lsf) return v >= 7;
        return v == m_sf_max[ch1_gr][1][index];
    }

    void stereo(const FrameHeader& h, int gr, float* x0, float* x1) const {
        const Granule& g1 = m_si.gr[gr][1];
        const int r = h.sr_index;
        const float isqrt2 = 0.70710678118654752440f;
        if (h.mode_ext & 1) {
            const int j_scale = g1.sf_compress & 1;
            int pos = 576;
            bool nz_short[3] = { false, false, false };
            int k = (13 - g1.short_start) * 3 + g1.long_end - 3;
            for (int i = 12; i >= g1.short_start; --i) {
                if (i != 11) k -= 3;
                const int len = k_band_short[r][i];
                for (int l = 2; l >= 0; --l) {
                    pos -= len;
                    bool do_is = false;
                    if (!nz_short[l]) {
                        bool nz = false;
                        for (int j = 0; j < len; ++j) {
                            if (x1[pos + j] != 0.0f) {
                                nz = true;
                                break;
                            }
                        }
                        if (nz) nz_short[l] = true;
                        else do_is = !intensity_illegal(h, gr, k + l);
                    }
                    if (do_is) {
                        const int p = m_sf[gr][1][k + l];
                        const float v1 = h.lsf ? m_t.is_lsf[j_scale][p & 31][0] : m_t.is_mpeg1[p][0];
                        const float v2 = h.lsf ? m_t.is_lsf[j_scale][p & 31][1] : m_t.is_mpeg1[p][1];
                        for (int j = 0; j < len; ++j) {
                            const float t = x0[pos + j];
                            x0[pos + j] = t * v1;
                            x1[pos + j] = t * v2;
                        }
                    } else if (h.mode_ext & 2) {
                        for (int j = 0; j < len; ++j) {
                            const float a = x0[pos + j];
                            const float b = x1[pos + j];
                            x0[pos + j] = (a + b) * isqrt2;
                            x1[pos + j] = (a - b) * isqrt2;
                        }
                    }
                }
            }
            bool nz_found = nz_short[0] || nz_short[1] || nz_short[2];
            for (int i = g1.long_end - 1; i >= 0; --i) {
                const int len = k_band_long[r][i];
                pos -= len;
                bool do_is = false;
                if (!nz_found) {
                    for (int j = 0; j < len; ++j) {
                        if (x1[pos + j] != 0.0f) {
                            nz_found = true;
                            break;
                        }
                    }
                    if (!nz_found) do_is = !intensity_illegal(h, gr, i == 21 ? 20 : i);
                }
                if (do_is) {
                    const int p = m_sf[gr][1][i == 21 ? 20 : i];
                    const float v1 = h.lsf ? m_t.is_lsf[j_scale][p & 31][0] : m_t.is_mpeg1[p][0];
                    const float v2 = h.lsf ? m_t.is_lsf[j_scale][p & 31][1] : m_t.is_mpeg1[p][1];
                    for (int j = 0; j < len; ++j) {
                        const float t = x0[pos + j];
                        x0[pos + j] = t * v1;
                        x1[pos + j] = t * v2;
                    }
                } else if (h.mode_ext & 2) {
                    for (int j = 0; j < len; ++j) {
                        const float a = x0[pos + j];
                        const float b = x1[pos + j];
                        x0[pos + j] = (a + b) * isqrt2;
                        x1[pos + j] = (a - b) * isqrt2;
                    }
                }
            }
        } else if (h.mode_ext & 2) {
            for (int i = 0; i < 576; ++i) {
                const float a = x0[i];
                const float b = x1[i];
                x0[i] = a + b;
                x1[i] = a - b;
            }
        }
    }

    void reorder(const FrameHeader& h, const Granule& g, float* xr) const {
        if (g.block_type != 2) return;
        const int r = h.sr_index;
        int pos = g.mixed ? (r == 8 ? 72 : 36) : 0;
        float tmp[576];
        for (int b = g.short_start; b < 13; ++b) {
            const int len = k_band_short[r][b];
            if (pos + 3 * len > 576) break;
            for (int j = 0; j < len; ++j) {
                tmp[3 * j] = xr[pos + j];
                tmp[3 * j + 1] = xr[pos + len + j];
                tmp[3 * j + 2] = xr[pos + 2 * len + j];
            }
            std::memcpy(xr + pos, tmp, sizeof(float) * static_cast<std::size_t>(3 * len));
            pos += 3 * len;
        }
    }

    void antialias(const Granule& g, float* xr, int nsb) const {
        int n = 31;
        if (g.block_type == 2) {
            if (!g.mixed) return;
            n = 1;
        }
        n = std::min(n, nsb);
        for (int sb = 1; sb <= n; ++sb) {
            float* p = xr + 18 * sb;
            for (int i = 0; i < 8; ++i) {
                const float a = p[-1 - i];
                const float b = p[i];
                p[-1 - i] = a * m_t.cs[i] - b * m_t.ca[i];
                p[i] = b * m_t.cs[i] + a * m_t.ca[i];
            }
        }
    }

    void imdct_long(const float* in, float* overlap, float* out, const float* win) const noexcept {
        float ev[9];
        float od[9];
        ev[0] = in[0];
        od[0] = in[1] + in[0];
        for (int p = 1; p < 9; ++p) {
            ev[p] = in[2 * p] + in[2 * p - 1];
            od[p] = in[2 * p + 1] + in[2 * p];
        }
        float e[12];
        float o[12];
        for (int n = 0; n < 12; ++n) {
            e[n] = 0.0f;
            o[n] = 0.0f;
        }
        for (int p = 0; p < 9; ++p) {
            const float a = ev[p];
            const float b = od[p];
            const float* ma = m_t.dct3_even[p];
            const float* mb = m_t.dct4_odd[p];
            for (int n = 0; n < 12; ++n) {
                e[n] += a * ma[n];
                o[n] += b * mb[n];
            }
        }
        float y[18];
        for (int n = 0; n < 9; ++n) {
            y[n] = (e[n] + o[n]) * m_t.dct4_scale[n];
            y[17 - n] = (e[n] - o[n]) * m_t.dct4_scale[17 - n];
        }
        for (int i = 0; i < 9; ++i) out[i] = y[9 + i] * win[i] + overlap[i];
        for (int i = 9; i < 18; ++i) out[i] = -y[26 - i] * win[i] + overlap[i];
        for (int i = 18; i < 27; ++i) overlap[i - 18] = -y[26 - i] * win[i];
        for (int i = 27; i < 36; ++i) overlap[i - 18] = -y[i - 27] * win[i];
    }

    void imdct_short(const float* in, float* overlap, float* out) const noexcept {
        float buf[36];
        for (int i = 0; i < 36; ++i) buf[i] = 0.0f;
        for (int w = 0; w < 3; ++w) {
            float y[6];
            for (int n = 0; n < 6; ++n) {
                float s = 0.0f;
                for (int k = 0; k < 6; ++k) s += in[3 * k + w] * m_t.dct4_short[k][n];
                y[n] = s;
            }
            float x[12];
            for (int i = 0; i < 3; ++i) x[i] = y[3 + i];
            for (int j = 0; j < 6; ++j) x[3 + j] = -y[5 - j];
            for (int j = 0; j < 3; ++j) x[9 + j] = -y[j];
            float* dst = buf + 6 + 6 * w;
            for (int i = 0; i < 12; ++i) dst[i] += x[i] * m_t.win_short[i];
        }
        for (int i = 0; i < 18; ++i) {
            out[i] = buf[i] + overlap[i];
            overlap[i] = buf[18 + i];
        }
    }

    void hybrid(int ch, int gr, const Granule& g, const float* xr, int nsb) {
        float out[18];
        for (int sb = 0; sb < 32; ++sb) {
            float* ov = m_overlap[ch][sb];
            if (sb >= nsb) {
                for (int i = 0; i < 18; ++i) {
                    out[i] = ov[i];
                    ov[i] = 0.0f;
                }
            } else {
                const int bt = (g.mixed && sb < 2) ? 0 : g.block_type;
                if (bt == 2) imdct_short(xr + 18 * sb, ov, out);
                else imdct_long(xr + 18 * sb, ov, out, m_t.win_long[bt]);
            }
            if (sb & 1) {
                for (int i = 1; i < 18; i += 2) out[i] = -out[i];
            }
            for (int i = 0; i < 18; ++i) m_sb[ch][gr * 18 + i][sb] = out[i];
        }
    }

    int decode_layer3(const std::uint8_t* frame, std::size_t size, const FrameHeader& h) {
        const int nch = h.channels;
        const int ngr = h.lsf ? 1 : 2;
        const std::size_t off = main_offset(h);
        bool ok = off <= size;
        m_si = SideInfo();
        if (ok) {
            BitReader br(frame, size);
            br.set_pos((4u + (h.crc ? 2u : 0u)) * 8u);
            ok = parse_side_info(br, h, m_si);
            if (ok && h.crc) {
                std::uint16_t crc = crc16_update(0xFFFFu, frame + 2, 2);
                crc = crc16_update(crc, frame + 6, h.side_info);
                const std::uint16_t stored = static_cast<std::uint16_t>((frame[4] << 8) | frame[5]);
                if (crc != stored) ok = false;
            }
        }
        const std::size_t avail = m_reservoir.size();
        const std::size_t main_len = ok ? size - off : 0;
        m_main.assign(m_reservoir.begin(), m_reservoir.end());
        if (main_len) m_main.insert(m_main.end(), frame + off, frame + size);
        const std::size_t data_bytes = m_main.size();
        m_main.resize(data_bytes + 16, 0);
        if (off < size) feed(frame, size, h);
        float xr[2][576];
        int is[576];
        if (!ok) {
            for (int gr = 0; gr < ngr; ++gr) {
                for (int ch = 0; ch < nch; ++ch) hybrid(ch, gr, m_si.gr[gr][ch], xr[ch], 0);
            }
            return ngr * 18;
        }
        const std::int64_t data_bits = static_cast<std::int64_t>(data_bytes) * 8;
        std::int64_t pos = (static_cast<std::int64_t>(avail) - m_si.main_data_begin) * 8;
        BitReader br(m_main.data(), m_main.size());
        for (int gr = 0; gr < ngr; ++gr) {
            int nsb[2] = { 0, 0 };
            if (pos < 0) {
                for (int ch = 0; ch < nch; ++ch) {
                    pos += m_si.gr[gr][ch].part23;
                    hybrid(ch, gr, m_si.gr[gr][ch], xr[ch], 0);
                }
                continue;
            }
            int counts[2] = { 0, 0 };
            for (int ch = 0; ch < nch; ++ch) {
                Granule& g = m_si.gr[gr][ch];
                const std::int64_t start = pos;
                std::int64_t end = start + g.part23;
                pos = end;
                if (end > data_bits) end = data_bits;
                if (start >= data_bits) {
                    std::memset(m_sf[gr][ch], 0, 40);
                    std::memset(m_sf_max[gr][ch], 0, 40);
                    for (int i = 0; i < 576; ++i) xr[ch][i] = 0.0f;
                    counts[ch] = 0;
                    continue;
                }
                br.set_pos(static_cast<std::size_t>(start));
                read_scalefactors(br, h, gr, ch, g, m_si.scfsi[ch]);
                counts[ch] = huffman(br, static_cast<std::size_t>(end), g, is);
                dequantize(h, g, m_sf[gr][ch], is, counts[ch], xr[ch]);
            }
            if (nch == 2 && h.mode == 1) stereo(h, gr, xr[0], xr[1]);
            for (int ch = 0; ch < nch; ++ch) {
                const Granule& g = m_si.gr[gr][ch];
                reorder(h, g, xr[ch]);
                int last = 575;
                while (last >= 0 && xr[ch][last] == 0.0f) --last;
                nsb[ch] = last < 0 ? 0 : last / 18 + 1;
                antialias(g, xr[ch], nsb[ch]);
                const int limit = (g.block_type == 2 && !g.mixed) ? nsb[ch] : std::min(32, nsb[ch] + 1);
                hybrid(ch, gr, g, xr[ch], limit);
            }
        }
        return ngr * 18;
    }

    const Tables& m_t;
    float m_sb[2][36][32];
    float m_overlap[2][32][18];
    float m_synth_v[2][32][64];
    int m_synth_pos[2];
    std::uint8_t m_sf[2][2][40];
    std::uint8_t m_sf_max[2][2][40];
    SideInfo m_si;
    std::vector<std::uint8_t> m_reservoir;
    std::vector<std::uint8_t> m_main;
};

class ByteSource {
public:
    virtual ~ByteSource() = default;
    virtual std::uint64_t size() const noexcept = 0;
    virtual std::size_t read_at(std::uint64_t offset, std::uint8_t* out, std::size_t n) = 0;
    virtual const std::uint8_t* direct(std::uint64_t, std::size_t) const noexcept { return nullptr; }
};

class MemorySource : public ByteSource {
public:
    MemorySource(const std::uint8_t* data, std::size_t size) noexcept : m_data(data), m_size(size) {}
    std::uint64_t size() const noexcept override { return m_size; }
    std::size_t read_at(std::uint64_t offset, std::uint8_t* out, std::size_t n) override {
        if (offset >= m_size) return 0;
        const std::size_t at = static_cast<std::size_t>(offset);
        const std::size_t k = std::min(n, m_size - at);
        if (k) std::memcpy(out, m_data + at, k);
        return k;
    }
    const std::uint8_t* direct(std::uint64_t offset, std::size_t n) const noexcept override {
        if (offset > m_size || n > m_size - static_cast<std::size_t>(offset)) return nullptr;
        return m_data + offset;
    }

private:
    const std::uint8_t* m_data;
    std::size_t m_size;
};

class FileSource : public ByteSource {
public:
    bool open(const std::string& path) {
        if (!m_file.open(path, "rb")) return false;
        const std::int64_t s = m_file.size();
        m_size = s > 0 ? static_cast<std::uint64_t>(s) : 0;
        return true;
    }
    std::uint64_t size() const noexcept override { return m_size; }
    std::size_t read_at(std::uint64_t offset, std::uint8_t* out, std::size_t n) override {
        std::size_t done = 0;
        while (done < n && offset < m_size) {
            if (offset >= m_cache_offset && offset < m_cache_offset + m_cache_len) {
                const std::size_t at = static_cast<std::size_t>(offset - m_cache_offset);
                const std::size_t k = std::min(n - done, m_cache_len - at);
                std::memcpy(out + done, m_cache.data() + at, k);
                done += k;
                offset += k;
                continue;
            }
            m_cache.resize(k_cache);
            const std::uint64_t start = offset - (offset % 4096u);
            m_cache_len = 0;
            m_cache_offset = start;
            if (!m_file.seek(static_cast<std::int64_t>(start))) break;
            m_cache_len = m_file.read(m_cache.data(), k_cache);
            if (m_cache_len == 0 || offset >= start + m_cache_len) break;
        }
        return done;
    }

private:
    static const std::size_t k_cache = 65536;
    detail::File m_file;
    std::uint64_t m_size = 0;
    std::vector<std::uint8_t> m_cache;
    std::uint64_t m_cache_offset = 0;
    std::size_t m_cache_len = 0;
};

std::uint32_t syncsafe(const std::uint8_t* p) noexcept {
    return (static_cast<std::uint32_t>(p[0] & 0x7F) << 21) | (static_cast<std::uint32_t>(p[1] & 0x7F) << 14) |
           (static_cast<std::uint32_t>(p[2] & 0x7F) << 7) | static_cast<std::uint32_t>(p[3] & 0x7F);
}

std::uint64_t id3v2_length(const std::uint8_t* p, std::size_t n) noexcept {
    if (n < 10 || p[0] != 'I' || p[1] != 'D' || p[2] != '3') return 0;
    if (p[3] == 0xFF || p[4] == 0xFF) return 0;
    if ((p[6] | p[7] | p[8] | p[9]) & 0x80) return 0;
    return 10u + syncsafe(p + 6) + ((p[5] & 0x10) ? 10u : 0u);
}

void append_utf8(std::string& out, std::uint32_t c) {
    if (c < 0x80) {
        out.push_back(static_cast<char>(c));
    } else if (c < 0x800) {
        out.push_back(static_cast<char>(0xC0 | (c >> 6)));
        out.push_back(static_cast<char>(0x80 | (c & 0x3F)));
    } else if (c < 0x10000) {
        out.push_back(static_cast<char>(0xE0 | (c >> 12)));
        out.push_back(static_cast<char>(0x80 | ((c >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (c & 0x3F)));
    } else {
        out.push_back(static_cast<char>(0xF0 | (c >> 18)));
        out.push_back(static_cast<char>(0x80 | ((c >> 12) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | ((c >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (c & 0x3F)));
    }
}

std::string latin1_to_utf8(const std::uint8_t* p, std::size_t n) {
    std::string out;
    for (std::size_t i = 0; i < n; ++i) append_utf8(out, p[i]);
    return out;
}

std::vector<std::string> decode_text(const std::uint8_t* p, std::size_t n) {
    std::vector<std::string> values;
    if (n == 0) return values;
    const int enc = p[0];
    ++p;
    --n;
    std::string cur;
    if (enc == 1 || enc == 2) {
        bool big = enc == 2;
        std::size_t i = 0;
        bool start = true;
        std::uint32_t pending = 0;
        while (i + 1 < n) {
            if (start && enc == 1) {
                if (p[i] == 0xFF && p[i + 1] == 0xFE) {
                    big = false;
                    i += 2;
                    start = false;
                    continue;
                }
                if (p[i] == 0xFE && p[i + 1] == 0xFF) {
                    big = true;
                    i += 2;
                    start = false;
                    continue;
                }
            }
            start = false;
            const std::uint32_t u = big ? ((static_cast<std::uint32_t>(p[i]) << 8) | p[i + 1]) : ((static_cast<std::uint32_t>(p[i + 1]) << 8) | p[i]);
            i += 2;
            if (u == 0) {
                values.push_back(cur);
                cur.clear();
                start = true;
                pending = 0;
                continue;
            }
            if (u >= 0xD800 && u < 0xDC00) {
                pending = u;
                continue;
            }
            if (u >= 0xDC00 && u < 0xE000) {
                if (pending) append_utf8(cur, 0x10000u + ((pending - 0xD800u) << 10) + (u - 0xDC00u));
                pending = 0;
                continue;
            }
            pending = 0;
            append_utf8(cur, u);
        }
    } else {
        for (std::size_t i = 0; i < n; ++i) {
            if (p[i] == 0) {
                values.push_back(cur);
                cur.clear();
                continue;
            }
            if (enc == 3) cur.push_back(static_cast<char>(p[i]));
            else append_utf8(cur, p[i]);
        }
    }
    if (!cur.empty()) values.push_back(cur);
    while (!values.empty() && values.back().empty()) values.pop_back();
    return values;
}

std::string genre_name(const std::string& value) {
    std::string v = value;
    if (v.size() >= 3 && v[0] == '(') {
        const std::size_t close = v.find(')');
        if (close != std::string::npos) {
            const std::string inner = v.substr(1, close - 1);
            const std::string rest = v.substr(close + 1);
            if (!rest.empty()) return rest;
            if (inner == "RX") return "Remix";
            if (inner == "CR") return "Cover";
            v = inner;
        }
    }
    if (v.empty() || v.size() > 3) return value;
    int n = 0;
    for (char c : v) {
        if (c < '0' || c > '9') return value;
        n = n * 10 + (c - '0');
    }
    if (n < static_cast<int>(sizeof(k_genres) / sizeof(k_genres[0]))) return k_genres[n];
    return value;
}

void add_tag(Mp3Info& info, const std::string& key, const std::string& value) {
    if (value.empty()) return;
    for (const auto& kv : info.tags) {
        if (kv.first == key) return;
    }
    info.tags.emplace_back(key, value);
}

const char* id3_key(const char* id, bool v22) noexcept {
    struct Map { const char* id; const char* key; };
    static const Map v2[] = { { "TT2", "title" }, { "TP1", "artist" }, { "TAL", "album" }, { "TRK", "track" }, { "TYE", "date" }, { "TCO", "genre" } };
    static const Map v3[] = { { "TIT2", "title" }, { "TPE1", "artist" }, { "TALB", "album" }, { "TRCK", "track" }, { "TYER", "date" }, { "TDRC", "date" }, { "TCON", "genre" } };
    if (v22) {
        for (const Map& m : v2) {
            if (std::memcmp(id, m.id, 3) == 0) return m.key;
        }
        return nullptr;
    }
    for (const Map& m : v3) {
        if (std::memcmp(id, m.id, 4) == 0) return m.key;
    }
    return nullptr;
}

std::vector<std::uint8_t> unsynchronise(const std::uint8_t* p, std::size_t n) {
    std::vector<std::uint8_t> out;
    out.reserve(n);
    for (std::size_t i = 0; i < n; ++i) {
        out.push_back(p[i]);
        if (p[i] == 0xFF && i + 1 < n && p[i + 1] == 0x00) ++i;
    }
    return out;
}

bool frame_id_ok(const std::uint8_t* p, std::size_t len) noexcept {
    for (std::size_t i = 0; i < len; ++i) {
        const std::uint8_t c = p[i];
        if (!((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9'))) return false;
    }
    return true;
}

void parse_id3v2(const std::uint8_t* tag, std::size_t n, Mp3Info& info) {
    if (n < 10) return;
    const int major = tag[3];
    const int flags = tag[5];
    if (major < 2 || major > 4) return;
    if (major == 2 && (flags & 0x40)) return;
    std::vector<std::uint8_t> body;
    const std::size_t declared = std::min<std::size_t>(n - 10, syncsafe(tag + 6));
    if ((flags & 0x80) && major < 4) body = unsynchronise(tag + 10, declared);
    else body.assign(tag + 10, tag + 10 + declared);
    std::size_t p = 0;
    if (major >= 3 && (flags & 0x40)) {
        if (body.size() < 4) return;
        const std::uint32_t ext = major == 3 ? load_be32(body.data()) + 4u : syncsafe(body.data());
        if (ext > body.size()) return;
        p = ext;
    }
    const std::size_t id_len = major == 2 ? 3 : 4;
    const std::size_t head_len = major == 2 ? 6 : 10;
    while (p + head_len <= body.size()) {
        const std::uint8_t* f = body.data() + p;
        if (f[0] == 0 || !frame_id_ok(f, id_len)) break;
        std::size_t size = 0;
        int fflags = 0;
        if (major == 2) {
            size = (static_cast<std::size_t>(f[3]) << 16) | (static_cast<std::size_t>(f[4]) << 8) | f[5];
        } else if (major == 3) {
            size = load_be32(f + 4);
            fflags = (f[8] << 8) | f[9];
        } else {
            const std::size_t plain = load_be32(f + 4);
            size = syncsafe(f + 4);
            if ((f[4] | f[5] | f[6] | f[7]) & 0x80) {
                size = plain;
            } else if (plain != size) {
                const std::size_t next_safe = p + head_len + size;
                const std::size_t next_plain = p + head_len + plain;
                const bool safe_ok = next_safe == body.size() || (next_safe + 4 <= body.size() && (body[next_safe] == 0 || frame_id_ok(body.data() + next_safe, 4)));
                const bool plain_ok = next_plain == body.size() || (next_plain + 4 <= body.size() && frame_id_ok(body.data() + next_plain, 4));
                if (!safe_ok && plain_ok) size = plain;
            }
            fflags = (f[8] << 8) | f[9];
        }
        p += head_len;
        if (size > body.size() - p) break;
        char id[5] = { 0, 0, 0, 0, 0 };
        std::memcpy(id, f, id_len);
        const char* key = id3_key(id, major == 2);
        if (key) {
            const std::uint8_t* d = body.data() + p;
            std::size_t dn = size;
            bool skip = false;
            std::vector<std::uint8_t> tmp;
            if (major == 3) {
                if (fflags & 0x00C0) skip = true;
                if (fflags & 0x0020) {
                    if (dn < 1) skip = true;
                    else { ++d; --dn; }
                }
            } else if (major == 4) {
                if (fflags & 0x000C) skip = true;
                if (fflags & 0x0040) {
                    if (dn < 1) skip = true;
                    else { ++d; --dn; }
                }
                if (fflags & 0x0001) {
                    if (dn < 4) skip = true;
                    else { d += 4; dn -= 4; }
                }
                if (!skip && ((fflags & 0x0002) || (flags & 0x80))) {
                    tmp = unsynchronise(d, dn);
                    d = tmp.data();
                    dn = tmp.size();
                }
            }
            if (!skip) {
                const std::vector<std::string> values = decode_text(d, dn);
                std::string joined;
                for (const std::string& v : values) {
                    std::string item = std::strcmp(key, "genre") == 0 ? genre_name(v) : v;
                    if (item.empty()) continue;
                    if (!joined.empty()) joined += "; ";
                    joined += item;
                }
                add_tag(info, key, joined);
            }
        }
        p += size;
    }
}

std::string id3v1_field(const std::uint8_t* p, std::size_t n) {
    std::size_t len = 0;
    while (len < n && p[len] != 0) ++len;
    while (len > 0 && p[len - 1] == ' ') --len;
    return latin1_to_utf8(p, len);
}

void parse_id3v1(const std::uint8_t* t, Mp3Info& info) {
    add_tag(info, "title", id3v1_field(t + 3, 30));
    add_tag(info, "artist", id3v1_field(t + 33, 30));
    add_tag(info, "album", id3v1_field(t + 63, 30));
    add_tag(info, "date", id3v1_field(t + 93, 4));
    if (t[125] == 0 && t[126] != 0) add_tag(info, "track", std::to_string(t[126]));
    if (t[127] < sizeof(k_genres) / sizeof(k_genres[0])) add_tag(info, "genre", k_genres[t[127]]);
}

struct FrameEntry {
    std::uint64_t offset;
    std::uint32_t size;
    std::uint32_t header;
};

struct Layout {
    std::uint64_t begin = 0;
    std::uint64_t end = 0;
    std::uint32_t key = 0;
    std::size_t free_size = 0;
};

bool read_u32(ByteSource& src, std::uint64_t pos, std::uint32_t& out) {
    std::uint8_t b[4];
    if (src.read_at(pos, b, 4) != 4) return false;
    out = load_be32(b);
    return true;
}

bool frame_at(ByteSource& src, std::uint64_t pos, const Layout& lay, FrameHeader& h) {
    if (pos + 4 > lay.end) return false;
    std::uint32_t raw = 0;
    if (!read_u32(src, pos, raw)) return false;
    if ((raw & k_key_mask) != lay.key) return false;
    if (!parse_header(raw, lay.free_size, h)) return false;
    if (h.bitrate_index == 0 && lay.free_size == 0) return false;
    if (h.size < 4u + (h.crc ? 2u : 0u) + h.side_info) return false;
    return pos + h.size <= lay.end;
}

std::size_t find_free_size(ByteSource& src, std::uint64_t pos, std::uint64_t end, std::uint32_t raw) {
    FrameHeader h;
    if (!parse_header(raw, 0, h)) return 0;
    const std::uint32_t mask = 0xFFFEFCC0u;
    const std::uint64_t min_len = 4u + h.side_info + 16u;
    const std::uint64_t max_len = 8192;
    std::vector<std::uint8_t> buf(static_cast<std::size_t>(max_len + 4));
    const std::size_t got = src.read_at(pos, buf.data(), buf.size());
    for (std::uint64_t d = min_len; d + 4 <= got && pos + d + 4 <= end; ++d) {
        const std::uint8_t* q = buf.data() + d;
        if (q[0] != 0xFF) continue;
        const std::uint32_t r = load_be32(q);
        if ((r & mask) != (raw & mask)) continue;
        const std::size_t pad = h.padding ? free_slot_bytes(h) : 0u;
        if (d <= pad) continue;
        const std::size_t base = static_cast<std::size_t>(d) - pad;
        if (h.layer == 1 && (base % 4u) != 0) continue;
        return base;
    }
    return 0;
}

bool locate(ByteSource& src, std::uint64_t from, Layout& lay, bool fixed, std::uint64_t& found) {
    std::vector<std::uint8_t> chunk(65536 + 4);
    std::size_t step = 2048;
    std::uint64_t pos = from;
    while (pos + 4 <= lay.end) {
        const std::size_t want = static_cast<std::size_t>(std::min<std::uint64_t>(std::min(step, chunk.size()), lay.end - pos));
        step *= 2;
        const std::size_t got = src.read_at(pos, chunk.data(), want);
        if (got < 4) return false;
        for (std::size_t i = 0; i + 4 <= got; ++i) {
            if (chunk[i] != 0xFF || (chunk[i + 1] & 0xE0) != 0xE0) continue;
            const std::uint32_t raw = load_be32(chunk.data() + i);
            if (!header_valid(raw)) continue;
            const std::uint64_t at = pos + i;
            Layout trial = lay;
            if (!fixed) {
                trial.key = raw & k_key_mask;
                trial.free_size = 0;
            } else if ((raw & k_key_mask) != lay.key) {
                continue;
            }
            if (((raw >> 12) & 15u) == 0 && trial.free_size == 0) {
                trial.free_size = find_free_size(src, at, lay.end, raw);
                if (trial.free_size == 0) continue;
            }
            FrameHeader h;
            if (!frame_at(src, at, trial, h)) continue;
            const std::uint64_t next = at + h.size;
            FrameHeader h2;
            if (next != lay.end && !frame_at(src, next, trial, h2)) continue;
            lay = trial;
            found = at;
            return true;
        }
        if (got <= 4) return false;
        pos += got - 3;
    }
    return false;
}

std::uint64_t trailing_tags(ByteSource& src, std::uint64_t begin, std::uint64_t end, Mp3Info* info) {
    bool progress = true;
    std::uint8_t buf[128];
    bool v1_done = false;
    while (progress && end > begin) {
        progress = false;
        if (end - begin >= 128 && !v1_done && src.read_at(end - 128, buf, 128) == 128 && buf[0] == 'T' && buf[1] == 'A' && buf[2] == 'G') {
            if (info) parse_id3v1(buf, *info);
            end -= 128;
            v1_done = true;
            progress = true;
            if (end - begin >= 227 && src.read_at(end - 227, buf, 4) == 4 && std::memcmp(buf, "TAG+", 4) == 0) end -= 227;
            continue;
        }
        if (end - begin >= 32 && src.read_at(end - 32, buf, 32) == 32 && std::memcmp(buf, "APETAGEX", 8) == 0) {
            const std::uint32_t size = detail::read_le32(buf + 12);
            const std::uint32_t flags = detail::read_le32(buf + 20);
            const std::uint64_t total = static_cast<std::uint64_t>(size) + ((flags & 0x80000000u) ? 32u : 0u);
            if (total >= 32 && total <= end - begin) {
                end -= total;
                progress = true;
                continue;
            }
        }
        if (end - begin >= 15 && src.read_at(end - 15, buf, 15) == 15 && std::memcmp(buf + 6, "LYRICS200", 9) == 0) {
            std::uint64_t size = 0;
            bool digits = true;
            for (int i = 0; i < 6; ++i) {
                if (buf[i] < '0' || buf[i] > '9') digits = false;
                size = size * 10 + static_cast<std::uint64_t>(buf[i] - '0');
            }
            if (digits && size + 15 <= end - begin) {
                end -= size + 15;
                progress = true;
                continue;
            }
        }
        if (end - begin >= 10 && src.read_at(end - 10, buf, 10) == 10 && buf[0] == '3' && buf[1] == 'D' && buf[2] == 'I' && !((buf[6] | buf[7] | buf[8] | buf[9]) & 0x80)) {
            const std::uint64_t total = 20u + syncsafe(buf + 6);
            if (total <= end - begin) {
                end -= total;
                progress = true;
                continue;
            }
        }
    }
    return end;
}

} // namespace

struct Mp3Reader::State {
    std::unique_ptr<ByteSource> source;
    Layout layout;
    std::vector<FrameEntry> frames;
    FrameHeader first;
    int out_channels = 0;
    std::uint64_t spf = 0;
    std::uint64_t skip = 0;
    Decoder decoder;
    std::vector<std::uint8_t> frame_bytes;
    std::vector<float> pcm;
    std::uint64_t pcm_start = 0;
    std::size_t pcm_frames = 0;
    std::size_t next = 0;

    const std::uint8_t* load(std::size_t index, FrameHeader& h) {
        const FrameEntry& e = frames[index];
        parse_header(e.header, layout.free_size, h);
        frame_bytes.resize(static_cast<std::size_t>(e.size) + 16);
        const std::size_t got = source->read_at(e.offset, frame_bytes.data(), e.size);
        std::memset(frame_bytes.data() + got, 0, frame_bytes.size() - got);
        return frame_bytes.data();
    }

    void decode_next() {
        FrameHeader h;
        const std::uint8_t* f = load(next, h);
        pcm.resize(static_cast<std::size_t>(spf) * static_cast<std::size_t>(out_channels));
        if (static_cast<std::uint64_t>(h.samples) == spf) {
            decoder.decode(f, frames[next].size, h, pcm.data(), out_channels);
        } else {
            std::fill(pcm.begin(), pcm.end(), 0.0f);
        }
        pcm_start = static_cast<std::uint64_t>(next) * spf;
        pcm_frames = static_cast<std::size_t>(spf);
        ++next;
    }
};

Mp3Reader::Mp3Reader() = default;
Mp3Reader::~Mp3Reader() = default;
Mp3Reader::Mp3Reader(Mp3Reader&&) noexcept = default;
Mp3Reader& Mp3Reader::operator=(Mp3Reader&&) noexcept = default;

bool Mp3Reader::open(const std::string& path, std::string* error) {
    close();
    auto src = std::make_unique<FileSource>();
    if (!src->open(path)) {
        if (error) *error = "can't open " + path;
        return false;
    }
    m_state = std::make_unique<State>();
    m_state->source = std::move(src);
    return finish_open(error);
}

bool Mp3Reader::open(const void* data, std::size_t size, std::string* error) {
    close();
    if (!data && size) {
        if (error) *error = "no data";
        return false;
    }
    m_state = std::make_unique<State>();
    m_state->source = std::make_unique<MemorySource>(static_cast<const std::uint8_t*>(data), size);
    return finish_open(error);
}

bool Mp3Reader::finish_open(std::string* error) {
    State& s = *m_state;
    ByteSource& src = *s.source;
    const std::uint64_t total = src.size();
    Mp3Info info;
    std::uint64_t begin = 0;
    for (;;) {
        std::uint8_t head[10];
        if (src.read_at(begin, head, 10) != 10) break;
        const std::uint64_t len = id3v2_length(head, 10);
        if (len == 0 || len > total - begin) break;
        const std::size_t want = static_cast<std::size_t>(std::min<std::uint64_t>(len, 1u << 20));
        std::vector<std::uint8_t> tag(want);
        if (src.read_at(begin, tag.data(), want) == want) parse_id3v2(tag.data(), want, info);
        begin += len;
    }
    Mp3Info fallback;
    const std::uint64_t end = trailing_tags(src, begin, total, &fallback);
    for (const auto& kv : fallback.tags) add_tag(info, kv.first, kv.second);
    s.layout.begin = begin;
    s.layout.end = end;
    std::uint64_t pos = 0;
    if (!locate(src, begin, s.layout, false, pos)) {
        if (error) *error = "no MPEG audio frames found";
        m_state.reset();
        return false;
    }
    bool in_sync = true;
    std::uint64_t audio_bytes = 0;
    std::uint32_t first_bitrate = 0;
    bool varied = false;
    while (pos + 4 <= s.layout.end) {
        FrameHeader h;
        bool ok = frame_at(src, pos, s.layout, h);
        if (ok && !in_sync) {
            FrameHeader h2;
            const std::uint64_t next = pos + h.size;
            ok = next == s.layout.end || frame_at(src, next, s.layout, h2);
        }
        if (!ok) {
            std::uint64_t found = 0;
            if (!locate(src, pos + 1, s.layout, true, found)) break;
            pos = found;
            in_sync = false;
            continue;
        }
        in_sync = true;
        s.frames.push_back({ pos, static_cast<std::uint32_t>(h.size), h.raw });
        if (s.frames.size() == 1) first_bitrate = h.bitrate;
        else if (h.bitrate != first_bitrate) varied = true;
        audio_bytes += h.size;
        pos += h.size;
    }
    if (s.frames.empty()) {
        if (error) *error = "no MPEG audio frames found";
        m_state.reset();
        return false;
    }
    parse_header(s.frames[0].header, s.layout.free_size, s.first);
    s.out_channels = s.first.channels;
    s.spf = static_cast<std::uint64_t>(s.first.samples);
    std::uint64_t xing_frames = 0;
    bool lame = false;
    bool has_tag = false;
    std::uint32_t delay = 0;
    std::uint32_t padding = 0;
    bool xing_vbr = false;
    if (s.first.layer == 3) {
        FrameHeader h;
        const std::uint8_t* f = s.load(0, h);
        const std::size_t size = s.frames[0].size;
        const std::size_t xoff = 4u + h.side_info;
        std::size_t at = 0;
        if (xoff + 8 <= size && (std::memcmp(f + xoff, "Xing", 4) == 0 || std::memcmp(f + xoff, "Info", 4) == 0)) at = xoff;
        else if (h.crc && xoff + 10 <= size && (std::memcmp(f + xoff + 2, "Xing", 4) == 0 || std::memcmp(f + xoff + 2, "Info", 4) == 0)) at = xoff + 2;
        if (at) {
            xing_vbr = std::memcmp(f + at, "Xing", 4) == 0;
            const std::uint32_t flags = load_be32(f + at + 4);
            std::size_t p = at + 8;
            std::uint32_t bytes = 0;
            if ((flags & 1u) && p + 4 <= size) {
                xing_frames = load_be32(f + p);
                p += 4;
            }
            if ((flags & 2u) && p + 4 <= size) {
                bytes = load_be32(f + p);
                p += 4;
            }
            if (flags & 4u) p += 100;
            if (flags & 8u) p += 4;
            const std::uint64_t fsize = end > s.frames[0].offset ? end - s.frames[0].offset : 0;
            if (fsize && bytes) {
                const std::uint64_t lo = std::min<std::uint64_t>(fsize, bytes);
                const std::uint64_t delta = std::max<std::uint64_t>(fsize, bytes) - lo;
                if (fsize > bytes && delta > (lo >> 4)) xing_frames = 0;
            }
            if (p + 24 <= size) {
                const std::uint8_t* v = f + p;
                if (std::memcmp(v, "LAME", 4) == 0 || std::memcmp(v, "Lavf", 4) == 0 || std::memcmp(v, "Lavc", 4) == 0 || std::memcmp(v, "Fizmo", 5) == 0) {
                    const std::uint32_t d = (static_cast<std::uint32_t>(v[21]) << 16) | (static_cast<std::uint32_t>(v[22]) << 8) | v[23];
                    delay = d >> 12;
                    padding = d & 4095u;
                    lame = true;
                }
            }
            has_tag = xing_frames != 0 || bytes != 0;
        } else if (36u + 26u <= size && std::memcmp(f + 36, "VBRI", 4) == 0) {
            if (((f[40] << 8) | f[41]) == 1) {
                const std::uint32_t bytes = load_be32(f + 46);
                xing_frames = load_be32(f + 50);
                has_tag = xing_frames != 0 || bytes != 0;
                xing_vbr = true;
            }
        }
    }
    if (has_tag) {
        audio_bytes -= s.frames[0].size;
        s.frames.erase(s.frames.begin());
    }
    const std::uint64_t decoded = static_cast<std::uint64_t>(s.frames.size()) * s.spf;
    std::uint64_t stop = decoded;
    if (lame) {
        s.skip = static_cast<std::uint64_t>(delay) + 529u;
        if (xing_frames) {
            const std::uint64_t full = xing_frames * s.spf + 529u;
            stop = full > padding ? full - padding : 0;
            stop = std::min(stop, decoded);
        }
    }
    info.sample_rate = s.first.sample_rate;
    info.channels = static_cast<std::uint16_t>(s.out_channels);
    info.frames = stop > s.skip ? stop - s.skip : 0;
    info.vbr = xing_vbr || varied;
    info.mpeg_version = s.first.version == 3 ? 1 : (s.first.version == 2 ? 2 : 25);
    info.layer = s.first.layer;
    info.encoder_delay = lame ? delay : 0;
    info.encoder_padding = lame ? padding : 0;
    if (!info.vbr && first_bitrate) {
        info.bitrate_kbps = first_bitrate;
    } else if (decoded) {
        info.bitrate_kbps = static_cast<std::uint32_t>((audio_bytes * 8u * s.first.sample_rate / decoded + 500u) / 1000u);
    }
    m_info = std::move(info);
    m_position = 0;
    return true;
}

void Mp3Reader::close() {
    m_state.reset();
    m_info = Mp3Info();
    m_position = 0;
}

bool Mp3Reader::is_open() const noexcept {
    return m_state != nullptr;
}

std::size_t Mp3Reader::read(float* interleaved, std::size_t frames) {
    if (!m_state || !interleaved) return 0;
    State& s = *m_state;
    const std::size_t ch = static_cast<std::size_t>(s.out_channels);
    std::size_t done = 0;
    while (done < frames && m_position < m_info.frames) {
        const std::uint64_t abs = s.skip + m_position;
        if (s.pcm_frames && abs >= s.pcm_start && abs < s.pcm_start + s.pcm_frames) {
            const std::uint64_t avail = s.pcm_start + s.pcm_frames - abs;
            const std::uint64_t left = m_info.frames - m_position;
            const std::size_t n = static_cast<std::size_t>(std::min<std::uint64_t>(std::min<std::uint64_t>(avail, left), frames - done));
            std::memcpy(interleaved + done * ch, s.pcm.data() + static_cast<std::size_t>(abs - s.pcm_start) * ch, n * ch * sizeof(float));
            done += n;
            m_position += n;
            continue;
        }
        if (s.next >= s.frames.size()) break;
        s.decode_next();
    }
    return done;
}

std::size_t Mp3Reader::read(AudioBuffer& out, std::size_t frames) {
    if (!m_state) return 0;
    if (out.channels() != m_info.channels || out.sample_rate() != m_info.sample_rate) out = AudioBuffer(m_info.channels, m_info.sample_rate);
    const std::size_t start = out.frames();
    out.resize(start + frames);
    const std::size_t got = read(out.frame(start), frames);
    out.resize(start + got);
    return got;
}

bool Mp3Reader::seek(std::uint64_t frame) {
    if (!m_state) return false;
    State& s = *m_state;
    frame = std::min(frame, m_info.frames);
    const std::uint64_t abs = s.skip + frame;
    const std::uint64_t k = abs / s.spf;
    std::size_t begin = static_cast<std::size_t>(std::min<std::uint64_t>(k >= 2 ? k - 2 : 0, s.frames.size()));
    s.decoder.reset();
    if (s.first.layer == 3 && begin > 0) {
        std::size_t j = begin;
        std::size_t acc = 0;
        while (j > 0 && acc < 1024) {
            --j;
            FrameHeader h;
            parse_header(s.frames[j].header, s.layout.free_size, h);
            const std::size_t off = Decoder::main_offset(h);
            if (s.frames[j].size > off) acc += s.frames[j].size - off;
        }
        for (; j < begin; ++j) {
            FrameHeader h;
            const std::uint8_t* f = s.load(j, h);
            s.decoder.feed(f, s.frames[j].size, h);
        }
    }
    s.next = begin;
    s.pcm_frames = 0;
    m_position = frame;
    return true;
}

bool is_mp3(const void* data, std::size_t size) noexcept {
    if (!data || size < 4) return false;
    const std::uint8_t* p = static_cast<const std::uint8_t*>(data);
    if (size >= 10 && id3v2_length(p, size) != 0) return true;
    const std::size_t limit = std::min<std::size_t>(size, 4096);
    for (std::size_t i = 0; i + 4 <= limit; ++i) {
        if (p[i] != 0xFF || (p[i + 1] & 0xE0) != 0xE0) continue;
        FrameHeader h;
        const std::uint32_t raw = load_be32(p + i);
        if (!parse_header(raw, 0, h)) continue;
        if (h.bitrate_index == 0) {
            const std::size_t first = i + 4u + h.side_info + 16u;
            const std::size_t last = std::min<std::size_t>(size, i + 8192u);
            for (std::size_t j = first; j + 4 <= last; ++j) {
                if (p[j] == 0xFF && (load_be32(p + j) & 0xFFFEFCC0u) == (raw & 0xFFFEFCC0u)) return true;
            }
            continue;
        }
        if (h.size < 4u + h.side_info) continue;
        const std::size_t next = i + h.size;
        if (next == size) return true;
        if (next + 4 > size) continue;
        const std::uint32_t r2 = load_be32(p + next);
        FrameHeader h2;
        if ((r2 & k_key_mask) == (load_be32(p + i) & k_key_mask) && parse_header(r2, 0, h2)) return true;
    }
    return false;
}

bool read_mp3(const void* data, std::size_t size, AudioBuffer& out, std::string* error, Mp3Info* info) {
    Mp3Reader r;
    if (!r.open(data, size, error)) return false;
    out = AudioBuffer(r.info().channels, r.info().sample_rate);
    r.read(out, static_cast<std::size_t>(r.info().frames));
    if (info) *info = r.info();
    return true;
}

bool read_mp3(const std::string& path, AudioBuffer& out, std::string* error, Mp3Info* info) {
    std::vector<std::uint8_t> bytes;
    if (!detail::read_whole_file(path, bytes, error)) return false;
    return read_mp3(bytes.data(), bytes.size(), out, error, info);
}

struct Mp3Decoder::Impl {
    Decoder decoder;
    std::uint16_t channels = 0;
    std::uint32_t rate = 0;
    std::vector<std::uint8_t> frame;
    std::vector<float> pcm;
};

Mp3Decoder::Mp3Decoder() = default;
Mp3Decoder::~Mp3Decoder() = default;
Mp3Decoder::Mp3Decoder(Mp3Decoder&&) noexcept = default;
Mp3Decoder& Mp3Decoder::operator=(Mp3Decoder&&) noexcept = default;

bool Mp3Decoder::open(std::uint16_t channels) {
    m_impl.reset(new Impl());
    m_impl->channels = static_cast<std::uint16_t>(std::min<std::uint16_t>(channels, 2));
    return true;
}

void Mp3Decoder::close() {
    m_impl.reset();
}

bool Mp3Decoder::is_open() const noexcept {
    return m_impl != nullptr;
}

std::size_t Mp3Decoder::decode(const std::uint8_t* packet, std::size_t size, std::vector<float>& out, std::string* error) {
    out.clear();
    if (!m_impl) {
        if (error) *error = "the MP3 decoder isn't open";
        return 0;
    }
    if (error) error->clear();
    Impl& m = *m_impl;
    std::size_t pos = 0;
    std::size_t total = 0;
    while (packet && pos + 4 <= size) {
        const std::uint32_t raw = detail::read_be32(packet + pos);
        FrameHeader h;
        if (!parse_header(raw, size - pos, h)) {
            if (total == 0 && error) *error = "not an MPEG audio frame";
            break;
        }
        std::size_t len = h.size;
        if (len == 0 || len > size - pos) len = size - pos;
        if (m.channels == 0) m.channels = static_cast<std::uint16_t>(h.channels);
        if (m.rate == 0) m.rate = h.sample_rate;
        m.frame.assign(packet + pos, packet + pos + len);
        m.frame.resize(len + 16, 0);
        const std::size_t ch = m.channels;
        const std::size_t n = static_cast<std::size_t>(h.samples);
        out.resize((total + n) * ch);
        m.decoder.decode(m.frame.data(), len, h, out.data() + total * ch, static_cast<int>(ch));
        total += n;
        pos += len;
    }
    return total;
}

void Mp3Decoder::reset() {
    if (m_impl) m_impl->decoder.reset();
}

std::uint32_t Mp3Decoder::sample_rate() const noexcept {
    return m_impl ? m_impl->rate : 0;
}

std::uint16_t Mp3Decoder::channels() const noexcept {
    return m_impl ? m_impl->channels : 0;
}

bool mp3_frame_info(const std::uint8_t* data, std::size_t size, std::uint32_t& sample_rate, std::uint16_t& channels, std::uint32_t& samples) noexcept {
    if (!data || size < 4) return false;
    FrameHeader h;
    if (!parse_header(detail::read_be32(data), size, h)) return false;
    sample_rate = h.sample_rate;
    channels = static_cast<std::uint16_t>(h.channels);
    samples = static_cast<std::uint32_t>(h.samples);
    return true;
}

} // namespace audio
} // namespace fizmo
