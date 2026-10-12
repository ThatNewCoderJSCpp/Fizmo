#include "fizmo_library.hpp"
#include "hash.hpp"

#include <cstring>

namespace fizmo {
namespace crypto {

namespace {

inline std::uint32_t rotl32(std::uint32_t x, unsigned n) noexcept { return (x << n) | (x >> (32 - n)); }
inline std::uint32_t rotr32(std::uint32_t x, unsigned n) noexcept { return (x >> n) | (x << (32 - n)); }
inline std::uint64_t rotr64(std::uint64_t x, unsigned n) noexcept { return (x >> n) | (x << (64 - n)); }

inline std::uint32_t load_be32(const std::uint8_t* p) noexcept {
    return (std::uint32_t(p[0]) << 24) | (std::uint32_t(p[1]) << 16) | (std::uint32_t(p[2]) << 8) | std::uint32_t(p[3]);
}

inline std::uint64_t load_be64(const std::uint8_t* p) noexcept {
    return (std::uint64_t(load_be32(p)) << 32) | load_be32(p + 4);
}

inline void store_be32(std::uint8_t* p, std::uint32_t v) noexcept {
    p[0] = std::uint8_t(v >> 24); p[1] = std::uint8_t(v >> 16); p[2] = std::uint8_t(v >> 8); p[3] = std::uint8_t(v);
}

inline void store_be64(std::uint8_t* p, std::uint64_t v) noexcept {
    store_be32(p, std::uint32_t(v >> 32));
    store_be32(p + 4, std::uint32_t(v));
}

template <typename Self, std::size_t Block>
void absorb(Self& self, std::uint8_t* buffer, std::size_t& fill, std::uint64_t& length, const void* data, std::size_t len) noexcept {
    const auto* p = static_cast<const std::uint8_t*>(data);
    length += len;

    if (fill > 0) {
        const std::size_t take = (len < Block - fill) ? len : Block - fill;
        std::memcpy(buffer + fill, p, take);
        fill += take; p += take; len -= take;
        if (fill < Block) return;
        self(buffer);
        fill = 0;
    }

    while (len >= Block) {
        self(p);
        p += Block; len -= Block;
    }

    if (len > 0) { std::memcpy(buffer, p, len); fill = len; }
}

const std::uint32_t K256[64] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
    0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
    0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
    0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
    0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
};

const std::uint64_t K512[80] = {
    0x428a2f98d728ae22ull, 0x7137449123ef65cdull, 0xb5c0fbcfec4d3b2full, 0xe9b5dba58189dbbcull,
    0x3956c25bf348b538ull, 0x59f111f1b605d019ull, 0x923f82a4af194f9bull, 0xab1c5ed5da6d8118ull,
    0xd807aa98a3030242ull, 0x12835b0145706fbeull, 0x243185be4ee4b28cull, 0x550c7dc3d5ffb4e2ull,
    0x72be5d74f27b896full, 0x80deb1fe3b1696b1ull, 0x9bdc06a725c71235ull, 0xc19bf174cf692694ull,
    0xe49b69c19ef14ad2ull, 0xefbe4786384f25e3ull, 0x0fc19dc68b8cd5b5ull, 0x240ca1cc77ac9c65ull,
    0x2de92c6f592b0275ull, 0x4a7484aa6ea6e483ull, 0x5cb0a9dcbd41fbd4ull, 0x76f988da831153b5ull,
    0x983e5152ee66dfabull, 0xa831c66d2db43210ull, 0xb00327c898fb213full, 0xbf597fc7beef0ee4ull,
    0xc6e00bf33da88fc2ull, 0xd5a79147930aa725ull, 0x06ca6351e003826full, 0x142929670a0e6e70ull,
    0x27b70a8546d22ffcull, 0x2e1b21385c26c926ull, 0x4d2c6dfc5ac42aedull, 0x53380d139d95b3dfull,
    0x650a73548baf63deull, 0x766a0abb3c77b2a8ull, 0x81c2c92e47edaee6ull, 0x92722c851482353bull,
    0xa2bfe8a14cf10364ull, 0xa81a664bbc423001ull, 0xc24b8b70d0f89791ull, 0xc76c51a30654be30ull,
    0xd192e819d6ef5218ull, 0xd69906245565a910ull, 0xf40e35855771202aull, 0x106aa07032bbd1b8ull,
    0x19a4c116b8d2d0c8ull, 0x1e376c085141ab53ull, 0x2748774cdf8eeb99ull, 0x34b0bcb5e19b48a8ull,
    0x391c0cb3c5c95a63ull, 0x4ed8aa4ae3418acbull, 0x5b9cca4f7763e373ull, 0x682e6ff3d6b2b8a3ull,
    0x748f82ee5defb2fcull, 0x78a5636f43172f60ull, 0x84c87814a1f0ab72ull, 0x8cc702081a6439ecull,
    0x90befffa23631e28ull, 0xa4506cebde82bde9ull, 0xbef9a3f7b2c67915ull, 0xc67178f2e372532bull,
    0xca273eceea26619cull, 0xd186b8c721c0c207ull, 0xeada7dd6cde0eb1eull, 0xf57d4f7fee6ed178ull,
    0x06f067aa72176fbaull, 0x0a637dc5a2c898a6ull, 0x113f9804bef90daeull, 0x1b710b35131c471bull,
    0x28db77f523047d84ull, 0x32caab7b40c72493ull, 0x3c9ebe0a15c9bebcull, 0x431d67c49c100d4cull,
    0x4cc5d4becb3e42b6ull, 0x597f299cfc657e2aull, 0x5fcb6fab3ad6faecull, 0x6c44198c4a475817ull
};

struct CrcTables {
    std::uint32_t ieee[256];
    std::uint32_t castagnoli[256];

    CrcTables() noexcept {
        for (std::uint32_t i = 0; i < 256; ++i) {
            std::uint32_t a = i, b = i;

            for (int k = 0; k < 8; ++k) {
                a = (a & 1u) ? (a >> 1) ^ 0xEDB88320u : (a >> 1);
                b = (b & 1u) ? (b >> 1) ^ 0x82F63B78u : (b >> 1);
            }

            ieee[i] = a;
            castagnoli[i] = b;
        }
    }
};

const CrcTables& crc_tables() noexcept {
    static const CrcTables t;
    return t;
}

std::uint32_t crc_run(const std::uint32_t* table, const void* data, std::size_t len, std::uint32_t seed) noexcept {
    const auto* p = static_cast<const std::uint8_t*>(data);
    std::uint32_t c = ~seed;
    for (std::size_t i = 0; i < len; ++i) c = table[(c ^ p[i]) & 0xFFu] ^ (c >> 8);
    return ~c;
}

} // namespace

void SHA1::reset() noexcept {
    m_state[0] = 0x67452301u; m_state[1] = 0xEFCDAB89u; m_state[2] = 0x98BADCFEu; m_state[3] = 0x10325476u; m_state[4] = 0xC3D2E1F0u;
    m_length = 0;
    m_fill = 0;
}

void SHA1::compress(const std::uint8_t* block) noexcept {
    std::uint32_t w[80];
    for (int i = 0; i < 16; ++i) w[i] = load_be32(block + 4 * i);
    for (int i = 16; i < 80; ++i) w[i] = rotl32(w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16], 1);
    std::uint32_t a = m_state[0], b = m_state[1], c = m_state[2], d = m_state[3], e = m_state[4];

    for (int i = 0; i < 80; ++i) {
        std::uint32_t f, k;
        if (i < 20)      { f = (b & c) | (~b & d);           k = 0x5A827999u; }
        else if (i < 40) { f = b ^ c ^ d;                    k = 0x6ED9EBA1u; }
        else if (i < 60) { f = (b & c) | (b & d) | (c & d);  k = 0x8F1BBCDCu; }
        else             { f = b ^ c ^ d;                    k = 0xCA62C1D6u; }
        const std::uint32_t t = rotl32(a, 5) + f + e + k + w[i];
        e = d; d = c; c = rotl32(b, 30); b = a; a = t;
    }

    m_state[0] += a; m_state[1] += b; m_state[2] += c; m_state[3] += d; m_state[4] += e;
}

SHA1& SHA1::update(const void* data, std::size_t len) noexcept {
    auto fn = [this](const std::uint8_t* b) { compress(b); };
    absorb<decltype(fn), block_size>(fn, m_buffer, m_fill, m_length, data, len);
    return *this;
}

auto SHA1::finish() noexcept -> digest_type {
    const std::uint64_t bits = m_length * 8;
    const std::uint8_t pad = 0x80, zero = 0;
    update(&pad, 1);
    while (m_fill != 56) update(&zero, 1);
    std::uint8_t len_be[8];
    store_be64(len_be, bits);
    update(len_be, 8);
    digest_type out;
    for (int i = 0; i < 5; ++i) store_be32(out.data() + 4 * i, m_state[i]);
    reset();
    return out;
}

void SHA256::reset() noexcept {
    static const std::uint32_t iv[8] = { 0x6a09e667u, 0xbb67ae85u, 0x3c6ef372u, 0xa54ff53au, 0x510e527fu, 0x9b05688cu, 0x1f83d9abu, 0x5be0cd19u };
    std::memcpy(m_state, iv, sizeof(iv));
    m_length = 0;
    m_fill = 0;
}

void SHA256::compress(const std::uint8_t* block) noexcept {
    std::uint32_t w[64];
    for (int i = 0; i < 16; ++i) w[i] = load_be32(block + 4 * i);

    for (int i = 16; i < 64; ++i) {
        const std::uint32_t s0 = rotr32(w[i - 15], 7) ^ rotr32(w[i - 15], 18) ^ (w[i - 15] >> 3);
        const std::uint32_t s1 = rotr32(w[i - 2], 17) ^ rotr32(w[i - 2], 19) ^ (w[i - 2] >> 10);
        w[i] = w[i - 16] + s0 + w[i - 7] + s1;
    }

    std::uint32_t a = m_state[0], b = m_state[1], c = m_state[2], d = m_state[3];
    std::uint32_t e = m_state[4], f = m_state[5], g = m_state[6], h = m_state[7];

    for (int i = 0; i < 64; ++i) {
        const std::uint32_t S1 = rotr32(e, 6) ^ rotr32(e, 11) ^ rotr32(e, 25);
        const std::uint32_t ch = (e & f) ^ (~e & g);
        const std::uint32_t t1 = h + S1 + ch + K256[i] + w[i];
        const std::uint32_t S0 = rotr32(a, 2) ^ rotr32(a, 13) ^ rotr32(a, 22);
        const std::uint32_t mj = (a & b) ^ (a & c) ^ (b & c);
        const std::uint32_t t2 = S0 + mj;
        h = g; g = f; f = e; e = d + t1; d = c; c = b; b = a; a = t1 + t2;
    }

    m_state[0] += a; m_state[1] += b; m_state[2] += c; m_state[3] += d;
    m_state[4] += e; m_state[5] += f; m_state[6] += g; m_state[7] += h;
}

SHA256& SHA256::update(const void* data, std::size_t len) noexcept {
    auto fn = [this](const std::uint8_t* b) { compress(b); };
    absorb<decltype(fn), block_size>(fn, m_buffer, m_fill, m_length, data, len);
    return *this;
}

void SHA256::finish_into(std::uint8_t* out, std::size_t words) noexcept {
    const std::uint64_t bits = m_length * 8;
    const std::uint8_t pad = 0x80, zero = 0;
    update(&pad, 1);
    while (m_fill != 56) update(&zero, 1);
    std::uint8_t len_be[8];
    store_be64(len_be, bits);
    update(len_be, 8);
    for (std::size_t i = 0; i < words; ++i) store_be32(out + 4 * i, m_state[i]);
}

auto SHA256::finish() noexcept -> digest_type {
    digest_type out;
    finish_into(out.data(), 8);
    reset();
    return out;
}

void SHA224::reset() noexcept {
    static const std::uint32_t iv[8] = { 0xc1059ed8u, 0x367cd507u, 0x3070dd17u, 0xf70e5939u, 0xffc00b31u, 0x68581511u, 0x64f98fa7u, 0xbefa4fa4u };
    std::memcpy(m_state, iv, sizeof(iv));
    m_length = 0;
    m_fill = 0;
}

auto SHA224::finish() noexcept -> digest_type {
    digest_type out;
    finish_into(out.data(), 7);
    reset();
    return out;
}

void SHA512::reset() noexcept {
    static const std::uint64_t iv[8] = {
        0x6a09e667f3bcc908ull, 0xbb67ae8584caa73bull, 0x3c6ef372fe94f82bull, 0xa54ff53a5f1d36f1ull,
        0x510e527fade682d1ull, 0x9b05688c2b3e6c1full, 0x1f83d9abfb41bd6bull, 0x5be0cd19137e2179ull
    };
    std::memcpy(m_state, iv, sizeof(iv));
    m_length = 0;
    m_fill = 0;
}

void SHA512::compress(const std::uint8_t* block) noexcept {
    std::uint64_t w[80];
    for (int i = 0; i < 16; ++i) w[i] = load_be64(block + 8 * i);

    for (int i = 16; i < 80; ++i) {
        const std::uint64_t s0 = rotr64(w[i - 15], 1) ^ rotr64(w[i - 15], 8) ^ (w[i - 15] >> 7);
        const std::uint64_t s1 = rotr64(w[i - 2], 19) ^ rotr64(w[i - 2], 61) ^ (w[i - 2] >> 6);
        w[i] = w[i - 16] + s0 + w[i - 7] + s1;
    }

    std::uint64_t a = m_state[0], b = m_state[1], c = m_state[2], d = m_state[3];
    std::uint64_t e = m_state[4], f = m_state[5], g = m_state[6], h = m_state[7];

    for (int i = 0; i < 80; ++i) {
        const std::uint64_t S1 = rotr64(e, 14) ^ rotr64(e, 18) ^ rotr64(e, 41);
        const std::uint64_t ch = (e & f) ^ (~e & g);
        const std::uint64_t t1 = h + S1 + ch + K512[i] + w[i];
        const std::uint64_t S0 = rotr64(a, 28) ^ rotr64(a, 34) ^ rotr64(a, 39);
        const std::uint64_t mj = (a & b) ^ (a & c) ^ (b & c);
        const std::uint64_t t2 = S0 + mj;
        h = g; g = f; f = e; e = d + t1; d = c; c = b; b = a; a = t1 + t2;
    }

    m_state[0] += a; m_state[1] += b; m_state[2] += c; m_state[3] += d;
    m_state[4] += e; m_state[5] += f; m_state[6] += g; m_state[7] += h;
}

SHA512& SHA512::update(const void* data, std::size_t len) noexcept {
    auto fn = [this](const std::uint8_t* b) { compress(b); };
    absorb<decltype(fn), block_size>(fn, m_buffer, m_fill, m_length, data, len);
    return *this;
}

void SHA512::finish_into(std::uint8_t* out, std::size_t bytes) noexcept {
    const std::uint64_t bits = m_length * 8;
    const std::uint8_t pad = 0x80, zero = 0;
    update(&pad, 1);
    while (m_fill != 112) update(&zero, 1);
    std::uint8_t len_be[16] = {};
    store_be64(len_be + 8, bits);
    store_be64(len_be, m_length >> 61);
    update(len_be, 16);
    std::uint8_t full[64];
    for (int i = 0; i < 8; ++i) store_be64(full + 8 * i, m_state[i]);
    std::memcpy(out, full, bytes);
}

auto SHA512::finish() noexcept -> digest_type {
    digest_type out;
    finish_into(out.data(), 64);
    reset();
    return out;
}

void SHA384::reset() noexcept {
    static const std::uint64_t iv[8] = {
        0xcbbb9d5dc1059ed8ull, 0x629a292a367cd507ull, 0x9159015a3070dd17ull, 0x152fecd8f70e5939ull,
        0x67332667ffc00b31ull, 0x8eb44a8768581511ull, 0xdb0c2e0d64f98fa7ull, 0x47b5481dbefa4fa4ull
    };
    std::memcpy(m_state, iv, sizeof(iv));
    m_length = 0;
    m_fill = 0;
}

auto SHA384::finish() noexcept -> digest_type {
    digest_type out;
    finish_into(out.data(), 48);
    reset();
    return out;
}

std::uint32_t crc32(const void* data, std::size_t len, std::uint32_t seed) noexcept {
    return crc_run(crc_tables().ieee, data, len, seed);
}

std::uint32_t crc32c(const void* data, std::size_t len, std::uint32_t seed) noexcept {
    return crc_run(crc_tables().castagnoli, data, len, seed);
}

std::uint32_t adler32(const void* data, std::size_t len, std::uint32_t seed) noexcept {
    const auto* p = static_cast<const std::uint8_t*>(data);
    std::uint32_t a = seed & 0xFFFFu, b = (seed >> 16) & 0xFFFFu;

    while (len > 0) {
        const std::size_t n = len < 5552 ? len : 5552;
        len -= n;
        for (std::size_t i = 0; i < n; ++i) { a += *p++; b += a; }
        a %= 65521u;
        b %= 65521u;
    }

    return (b << 16) | a;
}

std::uint32_t fnv1a32(const void* data, std::size_t len, std::uint32_t seed) noexcept {
    const auto* p = static_cast<const std::uint8_t*>(data);
    std::uint32_t h = seed;
    for (std::size_t i = 0; i < len; ++i) { h ^= p[i]; h *= 16777619u; }
    return h;
}

std::uint64_t fnv1a64(const void* data, std::size_t len, std::uint64_t seed) noexcept {
    const auto* p = static_cast<const std::uint8_t*>(data);
    std::uint64_t h = seed;
    for (std::size_t i = 0; i < len; ++i) { h ^= p[i]; h *= 1099511628211ull; }
    return h;
}

} // namespace crypto
} // namespace fizmo
