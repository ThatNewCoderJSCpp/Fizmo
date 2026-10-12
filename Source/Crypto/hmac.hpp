#ifndef FIZMO_CRYPTO_HMAC_HPP
#define FIZMO_CRYPTO_HMAC_HPP

#include "hash.hpp"
#include "secure_memory.hpp"

#include <algorithm>
#include <stdexcept>

namespace fizmo {
namespace crypto {

template <typename Hash>
class HMAC {
public:
    static constexpr std::size_t digest_size = Hash::digest_size;
    using digest_type = typename Hash::digest_type;

    HMAC(const void* key, std::size_t key_len) noexcept { set_key(key, key_len); }
    explicit HMAC(const std::string& key) noexcept { set_key(key.data(), key.size()); }
    explicit HMAC(const std::vector<std::uint8_t>& key) noexcept { set_key(key.data(), key.size()); }
    ~HMAC() noexcept { secure_zero(m_opad, sizeof(m_opad)); secure_zero(m_ipad, sizeof(m_ipad)); }

    HMAC(const HMAC&) = default;
    HMAC& operator=(const HMAC&) = default;

    void set_key(const void* key, std::size_t key_len) noexcept {
        std::uint8_t k[Hash::block_size] = {};

        if (key_len > Hash::block_size) {
            const auto d = Hash::hash(key, key_len);
            std::copy(d.begin(), d.end(), k);
        } else if (key_len > 0) {
            std::memcpy(k, key, key_len);
        }

        for (std::size_t i = 0; i < Hash::block_size; ++i) {
            m_ipad[i] = static_cast<std::uint8_t>(k[i] ^ 0x36u);
            m_opad[i] = static_cast<std::uint8_t>(k[i] ^ 0x5Cu);
        }

        secure_zero(k, sizeof(k));
        reset();
    }

    void reset() noexcept {
        m_inner.reset();
        m_inner.update(m_ipad, Hash::block_size);
    }

    HMAC& update(const void* data, std::size_t len) noexcept { m_inner.update(data, len); return *this; }
    HMAC& update(const std::string& s) noexcept { return update(s.data(), s.size()); }
    HMAC& update(const std::vector<std::uint8_t>& v) noexcept { return update(v.data(), v.size()); }

    digest_type finish() noexcept {
        const auto inner = m_inner.finish();
        Hash outer;
        outer.update(m_opad, Hash::block_size);
        outer.update(inner.data(), inner.size());
        reset();
        return outer.finish();
    }

    static digest_type mac(const void* key, std::size_t key_len, const void* data, std::size_t len) noexcept {
        HMAC h(key, key_len);
        h.update(data, len);
        return h.finish();
    }

    static digest_type mac(const std::string& key, const std::string& data) noexcept {
        return mac(key.data(), key.size(), data.data(), data.size());
    }

    static bool verify(const void* key, std::size_t key_len, const void* data, std::size_t len, const void* expected, std::size_t expected_len) noexcept {
        if (expected_len == 0 || expected_len > digest_size) return false;
        const auto d = mac(key, key_len, data, len);
        return secure_compare(d.data(), expected, expected_len);
    }

private:
    Hash         m_inner;
    std::uint8_t m_ipad[Hash::block_size];
    std::uint8_t m_opad[Hash::block_size];
};

using HMAC_SHA1   = HMAC<SHA1>;
using HMAC_SHA256 = HMAC<SHA256>;
using HMAC_SHA384 = HMAC<SHA384>;
using HMAC_SHA512 = HMAC<SHA512>;

template <typename Hash>
typename Hash::digest_type hkdf_extract(const void* salt, std::size_t salt_len, const void* ikm, std::size_t ikm_len) noexcept {
    if (salt_len == 0) {
        std::uint8_t zeros[Hash::digest_size] = {};
        return HMAC<Hash>::mac(zeros, sizeof(zeros), ikm, ikm_len);
    }

    return HMAC<Hash>::mac(salt, salt_len, ikm, ikm_len);
}

template <typename Hash>
std::vector<std::uint8_t> hkdf_expand(const void* prk, std::size_t prk_len, const void* info, std::size_t info_len, std::size_t length) {
    if (length > 255 * Hash::digest_size) throw std::length_error("hkdf_expand: output too long");
    std::vector<std::uint8_t> out;
    out.reserve(length);
    HMAC<Hash> h(prk, prk_len);
    typename Hash::digest_type t{};
    std::size_t t_len = 0;

    for (std::uint8_t counter = 1; out.size() < length; ++counter) {
        h.update(t.data(), t_len);
        h.update(info, info_len);
        h.update(&counter, 1);
        t = h.finish();
        t_len = t.size();
        const std::size_t take = std::min(t.size(), length - out.size());
        out.insert(out.end(), t.begin(), t.begin() + static_cast<std::ptrdiff_t>(take));
    }

    secure_zero(t.data(), t.size());
    return out;
}

template <typename Hash>
std::vector<std::uint8_t> hkdf(const void* salt, std::size_t salt_len, const void* ikm, std::size_t ikm_len, const void* info, std::size_t info_len, std::size_t length) {
    auto prk = hkdf_extract<Hash>(salt, salt_len, ikm, ikm_len);
    auto out = hkdf_expand<Hash>(prk.data(), prk.size(), info, info_len, length);
    secure_zero(prk.data(), prk.size());
    return out;
}

template <typename Hash>
std::vector<std::uint8_t> pbkdf2(const void* password, std::size_t password_len, const void* salt, std::size_t salt_len, std::uint32_t iterations, std::size_t length) {
    if (iterations == 0) throw std::invalid_argument("pbkdf2: iterations must be positive");
    std::vector<std::uint8_t> out;
    out.reserve(length);
    HMAC<Hash> prf(password, password_len);

    for (std::uint32_t block = 1; out.size() < length; ++block) {
        const std::uint8_t be[4] = { std::uint8_t(block >> 24), std::uint8_t(block >> 16), std::uint8_t(block >> 8), std::uint8_t(block) };
        prf.update(salt, salt_len);
        prf.update(be, 4);
        auto u = prf.finish();
        auto t = u;

        for (std::uint32_t i = 1; i < iterations; ++i) {
            prf.update(u.data(), u.size());
            u = prf.finish();
            for (std::size_t j = 0; j < t.size(); ++j) t[j] ^= u[j];
        }

        const std::size_t take = std::min(t.size(), length - out.size());
        out.insert(out.end(), t.begin(), t.begin() + static_cast<std::ptrdiff_t>(take));
        secure_zero(u.data(), u.size());
        secure_zero(t.data(), t.size());
    }

    return out;
}

template <typename Hash>
std::vector<std::uint8_t> pbkdf2(const std::string& password, const std::string& salt, std::uint32_t iterations, std::size_t length) {
    return pbkdf2<Hash>(password.data(), password.size(), salt.data(), salt.size(), iterations, length);
}

} // namespace crypto
} // namespace fizmo

#endif // FIZMO_CRYPTO_HMAC_HPP
