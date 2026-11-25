#include "fizmo_library.hpp"
#include "aead.hpp"
#include "secure_memory.hpp"
#include "../Random/random_std_int.hpp"

#include <cstring>
#include <stdexcept>

namespace fizmo {
namespace crypto {

namespace {

inline std::uint32_t rotl32(std::uint32_t x, unsigned n) noexcept { return (x << n) | (x >> (32 - n)); }

inline std::uint32_t load_le32(const std::uint8_t* p) noexcept {
    return std::uint32_t(p[0]) | (std::uint32_t(p[1]) << 8) | (std::uint32_t(p[2]) << 16) | (std::uint32_t(p[3]) << 24);
}

inline void store_le32(std::uint8_t* p, std::uint32_t v) noexcept {
    p[0] = std::uint8_t(v); p[1] = std::uint8_t(v >> 8); p[2] = std::uint8_t(v >> 16); p[3] = std::uint8_t(v >> 24);
}

inline void store_le64(std::uint8_t* p, std::uint64_t v) noexcept {
    for (int i = 0; i < 8; ++i) p[i] = std::uint8_t(v >> (8 * i));
}

inline void quarter(std::uint32_t* s, int a, int b, int c, int d) noexcept {
    s[a] += s[b]; s[d] = rotl32(s[d] ^ s[a], 16);
    s[c] += s[d]; s[b] = rotl32(s[b] ^ s[c], 12);
    s[a] += s[b]; s[d] = rotl32(s[d] ^ s[a], 8);
    s[c] += s[d]; s[b] = rotl32(s[b] ^ s[c], 7);
}

void double_rounds(std::uint32_t* s) noexcept {
    for (int i = 0; i < 10; ++i) {
        quarter(s, 0, 4, 8, 12);
        quarter(s, 1, 5, 9, 13);
        quarter(s, 2, 6, 10, 14);
        quarter(s, 3, 7, 11, 15);
        quarter(s, 0, 5, 10, 15);
        quarter(s, 1, 6, 11, 12);
        quarter(s, 2, 7, 8, 13);
        quarter(s, 3, 4, 9, 14);
    }
}

void setup(std::uint32_t* s, const std::uint8_t key[32]) noexcept {
    s[0] = 0x61707865u; s[1] = 0x3320646eu; s[2] = 0x79622d32u; s[3] = 0x6b206574u;
    for (int i = 0; i < 8; ++i) s[4 + i] = load_le32(key + 4 * i);
}

void pad16(Poly1305& mac, std::size_t len) noexcept {
    static const std::uint8_t zeros[16] = {};
    if (len % 16) mac.update(zeros, 16 - len % 16);
}

void compute_tag(const std::uint8_t key[32], const std::uint8_t nonce[12], const void* associated, std::size_t associated_size, const std::uint8_t* ciphertext, std::size_t len, std::uint8_t tag[16]) noexcept {
    std::uint8_t block[64];
    chacha20_block(key, 0, nonce, block);
    Poly1305 mac(block);
    secure_zero(block, sizeof block);
    if (associated_size) mac.update(associated, associated_size);
    pad16(mac, associated_size);
    if (len) mac.update(ciphertext, len);
    pad16(mac, len);
    std::uint8_t lengths[16];
    store_le64(lengths, associated_size);
    store_le64(lengths + 8, len);
    mac.update(lengths, 16);
    mac.finish(tag);
}

const std::uint8_t* bytes(const std::string& s) noexcept { return reinterpret_cast<const std::uint8_t*>(s.data()); }

constexpr char sealed_magic[4] = { 'F', 'Z', 'E', '1' };
constexpr char password_magic[4] = { 'F', 'Z', 'P', '1' };
constexpr std::size_t password_header = 4 + 1 + 4 + 4 + 4 + 16;

void put32(std::string& out, std::uint32_t v) {
    std::uint8_t b[4];
    store_le32(b, v);
    out.append(reinterpret_cast<const char*>(b), 4);
}

}

void chacha20_block(const std::uint8_t key[32], std::uint32_t counter, const std::uint8_t nonce[12], std::uint8_t out[64]) noexcept {
    std::uint32_t s[16];
    setup(s, key);
    s[12] = counter;
    s[13] = load_le32(nonce);
    s[14] = load_le32(nonce + 4);
    s[15] = load_le32(nonce + 8);
    std::uint32_t w[16];
    std::memcpy(w, s, sizeof s);
    double_rounds(w);
    for (int i = 0; i < 16; ++i) store_le32(out + 4 * i, w[i] + s[i]);
    secure_zero(s, sizeof s);
    secure_zero(w, sizeof w);
}

void chacha20_xor(const std::uint8_t key[32], std::uint32_t counter, const std::uint8_t nonce[12], const std::uint8_t* in, std::uint8_t* out, std::size_t len) noexcept {
    std::uint8_t block[64];
    while (len > 0) {
        chacha20_block(key, counter++, nonce, block);
        const std::size_t take = len < 64 ? len : 64;
        for (std::size_t i = 0; i < take; ++i) out[i] = in[i] ^ block[i];
        in += take;
        out += take;
        len -= take;
    }
    secure_zero(block, sizeof block);
}

void hchacha20(const std::uint8_t key[32], const std::uint8_t nonce[16], std::uint8_t out[32]) noexcept {
    std::uint32_t s[16];
    setup(s, key);
    for (int i = 0; i < 4; ++i) s[12 + i] = load_le32(nonce + 4 * i);
    double_rounds(s);
    for (int i = 0; i < 4; ++i) store_le32(out + 4 * i, s[i]);
    for (int i = 0; i < 4; ++i) store_le32(out + 16 + 4 * i, s[12 + i]);
    secure_zero(s, sizeof s);
}

Poly1305::Poly1305(const std::uint8_t key[32]) noexcept : m_fill(0) {
    m_r[0] = load_le32(key + 0) & 0x3ffffff;
    m_r[1] = (load_le32(key + 3) >> 2) & 0x3ffff03;
    m_r[2] = (load_le32(key + 6) >> 4) & 0x3ffc0ff;
    m_r[3] = (load_le32(key + 9) >> 6) & 0x3f03fff;
    m_r[4] = (load_le32(key + 12) >> 8) & 0x00fffff;
    for (int i = 0; i < 5; ++i) m_h[i] = 0;
    for (int i = 0; i < 4; ++i) m_pad[i] = load_le32(key + 16 + 4 * i);
    std::memset(m_buffer, 0, sizeof m_buffer);
}

Poly1305::~Poly1305() {
    secure_zero(m_r, sizeof m_r);
    secure_zero(m_h, sizeof m_h);
    secure_zero(m_pad, sizeof m_pad);
    secure_zero(m_buffer, sizeof m_buffer);
}

void Poly1305::blocks(const std::uint8_t* m, std::size_t len, bool final_partial) noexcept {
    const std::uint32_t hibit = final_partial ? 0 : (1u << 24);
    const std::uint32_t r0 = m_r[0], r1 = m_r[1], r2 = m_r[2], r3 = m_r[3], r4 = m_r[4];
    const std::uint32_t s1 = r1 * 5, s2 = r2 * 5, s3 = r3 * 5, s4 = r4 * 5;
    std::uint32_t h0 = m_h[0], h1 = m_h[1], h2 = m_h[2], h3 = m_h[3], h4 = m_h[4];

    while (len >= 16) {
        h0 += load_le32(m + 0) & 0x3ffffff;
        h1 += (load_le32(m + 3) >> 2) & 0x3ffffff;
        h2 += (load_le32(m + 6) >> 4) & 0x3ffffff;
        h3 += (load_le32(m + 9) >> 6) & 0x3ffffff;
        h4 += (load_le32(m + 12) >> 8) | hibit;

        const std::uint64_t d0 = std::uint64_t(h0) * r0 + std::uint64_t(h1) * s4 + std::uint64_t(h2) * s3 + std::uint64_t(h3) * s2 + std::uint64_t(h4) * s1;
        std::uint64_t d1 = std::uint64_t(h0) * r1 + std::uint64_t(h1) * r0 + std::uint64_t(h2) * s4 + std::uint64_t(h3) * s3 + std::uint64_t(h4) * s2;
        std::uint64_t d2 = std::uint64_t(h0) * r2 + std::uint64_t(h1) * r1 + std::uint64_t(h2) * r0 + std::uint64_t(h3) * s4 + std::uint64_t(h4) * s3;
        std::uint64_t d3 = std::uint64_t(h0) * r3 + std::uint64_t(h1) * r2 + std::uint64_t(h2) * r1 + std::uint64_t(h3) * r0 + std::uint64_t(h4) * s4;
        std::uint64_t d4 = std::uint64_t(h0) * r4 + std::uint64_t(h1) * r3 + std::uint64_t(h2) * r2 + std::uint64_t(h3) * r1 + std::uint64_t(h4) * r0;

        std::uint32_t c = std::uint32_t(d0 >> 26); h0 = std::uint32_t(d0) & 0x3ffffff;
        d1 += c; c = std::uint32_t(d1 >> 26); h1 = std::uint32_t(d1) & 0x3ffffff;
        d2 += c; c = std::uint32_t(d2 >> 26); h2 = std::uint32_t(d2) & 0x3ffffff;
        d3 += c; c = std::uint32_t(d3 >> 26); h3 = std::uint32_t(d3) & 0x3ffffff;
        d4 += c; c = std::uint32_t(d4 >> 26); h4 = std::uint32_t(d4) & 0x3ffffff;
        h0 += c * 5; c = h0 >> 26; h0 &= 0x3ffffff;
        h1 += c;

        m += 16;
        len -= 16;
    }

    m_h[0] = h0; m_h[1] = h1; m_h[2] = h2; m_h[3] = h3; m_h[4] = h4;
}

Poly1305& Poly1305::update(const void* data, std::size_t len) noexcept {
    const auto* p = static_cast<const std::uint8_t*>(data);
    if (m_fill) {
        const std::size_t take = len < 16 - m_fill ? len : 16 - m_fill;
        std::memcpy(m_buffer + m_fill, p, take);
        m_fill += take;
        p += take;
        len -= take;
        if (m_fill < 16) return *this;
        blocks(m_buffer, 16, false);
        m_fill = 0;
    }
    const std::size_t whole = len & ~std::size_t(15);
    if (whole) {
        blocks(p, whole, false);
        p += whole;
        len -= whole;
    }
    if (len) {
        std::memcpy(m_buffer, p, len);
        m_fill = len;
    }
    return *this;
}

void Poly1305::finish(std::uint8_t tag[16]) noexcept {
    if (m_fill) {
        m_buffer[m_fill] = 1;
        for (std::size_t i = m_fill + 1; i < 16; ++i) m_buffer[i] = 0;
        blocks(m_buffer, 16, true);
        m_fill = 0;
    }

    std::uint32_t h0 = m_h[0], h1 = m_h[1], h2 = m_h[2], h3 = m_h[3], h4 = m_h[4];
    std::uint32_t c = h1 >> 26; h1 &= 0x3ffffff;
    h2 += c; c = h2 >> 26; h2 &= 0x3ffffff;
    h3 += c; c = h3 >> 26; h3 &= 0x3ffffff;
    h4 += c; c = h4 >> 26; h4 &= 0x3ffffff;
    h0 += c * 5; c = h0 >> 26; h0 &= 0x3ffffff;
    h1 += c;

    std::uint32_t g0 = h0 + 5; c = g0 >> 26; g0 &= 0x3ffffff;
    std::uint32_t g1 = h1 + c; c = g1 >> 26; g1 &= 0x3ffffff;
    std::uint32_t g2 = h2 + c; c = g2 >> 26; g2 &= 0x3ffffff;
    std::uint32_t g3 = h3 + c; c = g3 >> 26; g3 &= 0x3ffffff;
    std::uint32_t g4 = h4 + c - (1u << 26);

    std::uint32_t mask = (g4 >> 31) - 1;
    g0 &= mask; g1 &= mask; g2 &= mask; g3 &= mask; g4 &= mask;
    mask = ~mask;
    h0 = (h0 & mask) | g0;
    h1 = (h1 & mask) | g1;
    h2 = (h2 & mask) | g2;
    h3 = (h3 & mask) | g3;
    h4 = (h4 & mask) | g4;

    h0 = h0 | (h1 << 26);
    h1 = (h1 >> 6) | (h2 << 20);
    h2 = (h2 >> 12) | (h3 << 14);
    h3 = (h3 >> 18) | (h4 << 8);

    std::uint64_t f = std::uint64_t(h0) + m_pad[0]; h0 = std::uint32_t(f);
    f = std::uint64_t(h1) + m_pad[1] + (f >> 32); h1 = std::uint32_t(f);
    f = std::uint64_t(h2) + m_pad[2] + (f >> 32); h2 = std::uint32_t(f);
    f = std::uint64_t(h3) + m_pad[3] + (f >> 32); h3 = std::uint32_t(f);

    store_le32(tag + 0, h0);
    store_le32(tag + 4, h1);
    store_le32(tag + 8, h2);
    store_le32(tag + 12, h3);
    for (int i = 0; i < 5; ++i) m_h[i] = 0;
}

void ChaCha20Poly1305::seal(const std::uint8_t key[32], const std::uint8_t nonce[12], const void* plaintext, std::size_t len, const void* associated, std::size_t associated_size, std::uint8_t* ciphertext, std::uint8_t tag[16]) noexcept {
    if (len) chacha20_xor(key, 1, nonce, static_cast<const std::uint8_t*>(plaintext), ciphertext, len);
    compute_tag(key, nonce, associated, associated_size, ciphertext, len, tag);
}

bool ChaCha20Poly1305::open(const std::uint8_t key[32], const std::uint8_t nonce[12], const void* ciphertext, std::size_t len, const void* associated, std::size_t associated_size, const std::uint8_t tag[16], std::uint8_t* plaintext) noexcept {
    std::uint8_t expected[16];
    compute_tag(key, nonce, associated, associated_size, static_cast<const std::uint8_t*>(ciphertext), len, expected);
    const bool ok = secure_compare(expected, tag, 16);
    secure_zero(expected, sizeof expected);
    if (!ok) return false;
    if (len) chacha20_xor(key, 1, nonce, static_cast<const std::uint8_t*>(ciphertext), plaintext, len);
    return true;
}

void XChaCha20Poly1305::seal(const std::uint8_t key[32], const std::uint8_t nonce[24], const void* plaintext, std::size_t len, const void* associated, std::size_t associated_size, std::uint8_t* ciphertext, std::uint8_t tag[16]) noexcept {
    std::uint8_t subkey[32];
    std::uint8_t short_nonce[12] = {};
    hchacha20(key, nonce, subkey);
    std::memcpy(short_nonce + 4, nonce + 16, 8);
    ChaCha20Poly1305::seal(subkey, short_nonce, plaintext, len, associated, associated_size, ciphertext, tag);
    secure_zero(subkey, sizeof subkey);
}

bool XChaCha20Poly1305::open(const std::uint8_t key[32], const std::uint8_t nonce[24], const void* ciphertext, std::size_t len, const void* associated, std::size_t associated_size, const std::uint8_t tag[16], std::uint8_t* plaintext) noexcept {
    std::uint8_t subkey[32];
    std::uint8_t short_nonce[12] = {};
    hchacha20(key, nonce, subkey);
    std::memcpy(short_nonce + 4, nonce + 16, 8);
    const bool ok = ChaCha20Poly1305::open(subkey, short_nonce, ciphertext, len, associated, associated_size, tag, plaintext);
    secure_zero(subkey, sizeof subkey);
    return ok;
}

std::string generate_key() {
    std::string key(32, '\0');
    random_bytes(&key[0], key.size());
    return key;
}

std::string encrypt(const std::string& key, const std::string& plaintext, const std::string& associated) {
    if (key.size() != 32) throw std::invalid_argument("encrypt: the key has to be 32 bytes; make one with generate_key() or derive_key()");
    std::string out(4 + 24 + plaintext.size() + 16, '\0');
    std::memcpy(&out[0], sealed_magic, 4);
    auto* nonce = reinterpret_cast<std::uint8_t*>(&out[4]);
    random_bytes(nonce, 24);
    auto* body = reinterpret_cast<std::uint8_t*>(&out[28]);
    std::string header = out.substr(0, 28);
    std::string ad = header + associated;
    XChaCha20Poly1305::seal(bytes(key), nonce, plaintext.data(), plaintext.size(), ad.data(), ad.size(), body, body + plaintext.size());
    return out;
}

bool decrypt(const std::string& key, const std::string& sealed, std::string& plaintext, const std::string& associated) {
    if (key.size() != 32) throw std::invalid_argument("decrypt: the key has to be 32 bytes");
    if (sealed.size() < 4 + 24 + 16 || std::memcmp(sealed.data(), sealed_magic, 4) != 0) return false;
    const std::size_t len = sealed.size() - 4 - 24 - 16;
    std::string ad = sealed.substr(0, 28) + associated;
    std::string out(len, '\0');
    const auto* nonce = bytes(sealed) + 4;
    const auto* body = bytes(sealed) + 28;
    if (!XChaCha20Poly1305::open(bytes(key), nonce, body, len, ad.data(), ad.size(), body + len, reinterpret_cast<std::uint8_t*>(&out[0]))) return false;
    plaintext.swap(out);
    secure_zero(&out[0], out.size());
    return true;
}

std::string derive_key(const std::string& password, const std::string& salt, const Argon2Params& params) {
    Argon2Params p = params;
    p.hash_length = 32;
    std::vector<std::uint8_t> k = argon2(p, password, salt);
    std::string key(reinterpret_cast<const char*>(k.data()), k.size());
    secure_zero(k.data(), k.size());
    return key;
}

std::string derive_key(const std::string& password, const std::string& salt, PasswordStrength strength) {
    return derive_key(password, salt, Argon2Params::of(strength));
}

std::string encrypt_with_password(const std::string& password, const std::string& plaintext, const Argon2Params& params, const std::string& associated) {
    std::string header(password_magic, 4);
    header += char(static_cast<std::uint32_t>(params.type));
    put32(header, params.memory_kib);
    put32(header, params.iterations);
    put32(header, params.parallelism);
    std::string salt(16, '\0');
    random_bytes(&salt[0], salt.size());
    header += salt;
    std::string key = derive_key(password, salt, params);
    std::string sealed = encrypt(key, plaintext, header + associated);
    secure_zero(&key[0], key.size());
    return header + sealed;
}

std::string encrypt_with_password(const std::string& password, const std::string& plaintext, PasswordStrength strength, const std::string& associated) {
    return encrypt_with_password(password, plaintext, Argon2Params::of(strength), associated);
}

bool decrypt_with_password(const std::string& password, const std::string& sealed, std::string& plaintext, const std::string& associated) {
    if (sealed.size() < password_header || std::memcmp(sealed.data(), password_magic, 4) != 0) return false;
    const auto* p = bytes(sealed);
    Argon2Params params;
    const std::uint8_t type = p[4];
    if (type > 2) return false;
    params.type = static_cast<Argon2Type>(type);
    params.memory_kib = load_le32(p + 5);
    params.iterations = load_le32(p + 9);
    params.parallelism = load_le32(p + 13);
    if (params.iterations < 1 || params.parallelism < 1 || params.parallelism > 0xFFFFFFU || params.memory_kib < 8ULL * params.parallelism) return false;
    if (params.memory_kib > verify_memory_limit_kib || params.iterations > verify_iteration_limit || params.parallelism > verify_parallelism_limit) return false;
    const std::string header = sealed.substr(0, password_header);
    const std::string salt = sealed.substr(password_header - 16, 16);
    std::string key = derive_key(password, salt, params);
    const bool ok = decrypt(key, sealed.substr(password_header), plaintext, header + associated);
    secure_zero(&key[0], key.size());
    return ok;
}

} // namespace crypto
} // namespace fizmo
