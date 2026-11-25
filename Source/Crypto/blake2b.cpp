#include "fizmo_library.hpp"
#include "blake2b.hpp"
#include "secure_memory.hpp"

#include <cstring>
#include <stdexcept>

namespace fizmo {
namespace crypto {

namespace {

constexpr std::uint64_t iv[8] = {
    0x6a09e667f3bcc908ULL, 0xbb67ae8584caa73bULL, 0x3c6ef372fe94f82bULL, 0xa54ff53a5f1d36f1ULL,
    0x510e527fade682d1ULL, 0x9b05688c2b3e6c1fULL, 0x1f83d9abfb41bd6bULL, 0x5be0cd19137e2179ULL,
};

constexpr std::uint8_t sigma[12][16] = {
    { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15 },
    { 14, 10, 4, 8, 9, 15, 13, 6, 1, 12, 0, 2, 11, 7, 5, 3 },
    { 11, 8, 12, 0, 5, 2, 15, 13, 10, 14, 3, 6, 7, 1, 9, 4 },
    { 7, 9, 3, 1, 13, 12, 11, 14, 2, 6, 5, 10, 4, 0, 15, 8 },
    { 9, 0, 5, 7, 2, 4, 10, 15, 14, 1, 11, 12, 6, 8, 3, 13 },
    { 2, 12, 6, 10, 0, 11, 8, 3, 4, 13, 7, 5, 15, 14, 1, 9 },
    { 12, 5, 1, 15, 14, 13, 4, 10, 0, 7, 6, 3, 9, 2, 8, 11 },
    { 13, 11, 7, 14, 12, 1, 3, 9, 5, 0, 15, 4, 8, 6, 2, 10 },
    { 6, 15, 14, 9, 11, 3, 0, 8, 12, 2, 13, 7, 1, 4, 10, 5 },
    { 10, 2, 8, 4, 7, 6, 1, 5, 15, 11, 9, 14, 3, 12, 13, 0 },
    { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15 },
    { 14, 10, 4, 8, 9, 15, 13, 6, 1, 12, 0, 2, 11, 7, 5, 3 },
};

inline std::uint64_t rotr64(std::uint64_t x, unsigned n) noexcept { return (x >> n) | (x << (64 - n)); }

inline std::uint64_t load_le64(const std::uint8_t* p) noexcept {
    std::uint64_t v = 0;
    for (int i = 7; i >= 0; --i) v = (v << 8) | p[i];
    return v;
}

inline void store_le64(std::uint8_t* p, std::uint64_t v) noexcept {
    for (int i = 0; i < 8; ++i) p[i] = std::uint8_t(v >> (8 * i));
}

inline void store_le32(std::uint8_t* p, std::uint32_t v) noexcept {
    for (int i = 0; i < 4; ++i) p[i] = std::uint8_t(v >> (8 * i));
}

}

BLAKE2b::BLAKE2b(std::size_t digest_size, const void* key, std::size_t key_size) : m_digest_size(digest_size), m_key_size(key_size) {
    if (digest_size == 0 || digest_size > max_digest_size) throw std::invalid_argument("BLAKE2b: the digest size has to be from 1 to 64 bytes");
    if (key_size > max_key_size) throw std::invalid_argument("BLAKE2b: the key can be at most 64 bytes");
    if (key_size && !key) throw std::invalid_argument("BLAKE2b: the key is null");
    std::memset(m_key, 0, sizeof m_key);
    if (key_size) std::memcpy(m_key, key, key_size);
    reset();
}

BLAKE2b::~BLAKE2b() {
    secure_zero(m_key, sizeof m_key);
    secure_zero(m_buffer, sizeof m_buffer);
    secure_zero(m_h, sizeof m_h);
}

void BLAKE2b::reset() {
    for (int i = 0; i < 8; ++i) m_h[i] = iv[i];
    m_h[0] ^= 0x01010000ULL ^ (std::uint64_t(m_key_size) << 8) ^ std::uint64_t(m_digest_size);
    m_t[0] = m_t[1] = 0;
    m_fill = 0;
    std::memset(m_buffer, 0, sizeof m_buffer);
    if (m_key_size) {
        std::memcpy(m_buffer, m_key, m_key_size);
        m_fill = block_size;
    }
}

void BLAKE2b::compress(const std::uint8_t* block, bool last) noexcept {
    std::uint64_t m[16];
    std::uint64_t v[16];
    for (int i = 0; i < 16; ++i) m[i] = load_le64(block + 8 * i);
    for (int i = 0; i < 8; ++i) { v[i] = m_h[i]; v[i + 8] = iv[i]; }
    v[12] ^= m_t[0];
    v[13] ^= m_t[1];
    if (last) v[14] = ~v[14];

    auto g = [&](int a, int b, int c, int d, std::uint64_t x, std::uint64_t y) {
        v[a] = v[a] + v[b] + x; v[d] = rotr64(v[d] ^ v[a], 32);
        v[c] = v[c] + v[d];     v[b] = rotr64(v[b] ^ v[c], 24);
        v[a] = v[a] + v[b] + y; v[d] = rotr64(v[d] ^ v[a], 16);
        v[c] = v[c] + v[d];     v[b] = rotr64(v[b] ^ v[c], 63);
    };

    for (int r = 0; r < 12; ++r) {
        const std::uint8_t* s = sigma[r];
        g(0, 4, 8, 12, m[s[0]], m[s[1]]);
        g(1, 5, 9, 13, m[s[2]], m[s[3]]);
        g(2, 6, 10, 14, m[s[4]], m[s[5]]);
        g(3, 7, 11, 15, m[s[6]], m[s[7]]);
        g(0, 5, 10, 15, m[s[8]], m[s[9]]);
        g(1, 6, 11, 12, m[s[10]], m[s[11]]);
        g(2, 7, 8, 13, m[s[12]], m[s[13]]);
        g(3, 4, 9, 14, m[s[14]], m[s[15]]);
    }

    for (int i = 0; i < 8; ++i) m_h[i] ^= v[i] ^ v[i + 8];
    secure_zero(m, sizeof m);
    secure_zero(v, sizeof v);
}

BLAKE2b& BLAKE2b::update(const void* data, std::size_t len) noexcept {
    const auto* p = static_cast<const std::uint8_t*>(data);
    while (len > 0) {
        if (m_fill == block_size) {
            m_t[0] += block_size;
            if (m_t[0] < block_size) ++m_t[1];
            compress(m_buffer, false);
            m_fill = 0;
        }
        const std::size_t take = len < block_size - m_fill ? len : block_size - m_fill;
        std::memcpy(m_buffer + m_fill, p, take);
        m_fill += take;
        p += take;
        len -= take;
    }
    return *this;
}

void BLAKE2b::finish(void* out) noexcept {
    m_t[0] += m_fill;
    if (m_t[0] < m_fill) ++m_t[1];
    std::memset(m_buffer + m_fill, 0, block_size - m_fill);
    compress(m_buffer, true);
    std::uint8_t full[64];
    for (int i = 0; i < 8; ++i) store_le64(full + 8 * i, m_h[i]);
    std::memcpy(out, full, m_digest_size);
    secure_zero(full, sizeof full);
    reset();
}

std::vector<std::uint8_t> BLAKE2b::finish() {
    std::vector<std::uint8_t> out(m_digest_size);
    finish(out.data());
    return out;
}

std::vector<std::uint8_t> BLAKE2b::hash(const void* data, std::size_t len, std::size_t digest_size, const void* key, std::size_t key_size) {
    BLAKE2b h(digest_size, key, key_size);
    h.update(data, len);
    return h.finish();
}

std::vector<std::uint8_t> BLAKE2b::hash(const std::string& data, std::size_t digest_size, const std::string& key) {
    return hash(data.data(), data.size(), digest_size, key.empty() ? nullptr : key.data(), key.size());
}

std::vector<std::uint8_t> blake2b_long(const void* data, std::size_t len, std::size_t out_size) {
    if (out_size == 0) throw std::invalid_argument("blake2b_long: the output size can't be zero");
    if (out_size > 0xFFFFFFFFULL) throw std::invalid_argument("blake2b_long: the output size has to fit in 32 bits");
    std::uint8_t prefix[4];
    store_le32(prefix, std::uint32_t(out_size));
    std::vector<std::uint8_t> out(out_size);

    if (out_size <= 64) {
        BLAKE2b h(out_size);
        h.update(prefix, 4).update(data, len);
        h.finish(out.data());
        return out;
    }

    std::uint8_t v[64];
    BLAKE2b first(64);
    first.update(prefix, 4).update(data, len);
    first.finish(v);
    std::memcpy(out.data(), v, 32);
    std::size_t done = 32;

    while (out_size - done > 64) {
        BLAKE2b next(64);
        next.update(v, 64);
        next.finish(v);
        std::memcpy(out.data() + done, v, 32);
        done += 32;
    }

    BLAKE2b last(out_size - done);
    last.update(v, 64);
    last.finish(out.data() + done);
    secure_zero(v, sizeof v);
    return out;
}

} // namespace crypto
} // namespace fizmo
