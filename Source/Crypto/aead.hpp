#ifndef FIZMO_CRYPTO_AEAD_HPP
#define FIZMO_CRYPTO_AEAD_HPP

#include "argon2.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace fizmo {
namespace crypto {

void chacha20_block(const std::uint8_t key[32], std::uint32_t counter, const std::uint8_t nonce[12], std::uint8_t out[64]) noexcept;

void chacha20_xor(const std::uint8_t key[32], std::uint32_t counter, const std::uint8_t nonce[12], const std::uint8_t* in, std::uint8_t* out, std::size_t len) noexcept;

void hchacha20(const std::uint8_t key[32], const std::uint8_t nonce[16], std::uint8_t out[32]) noexcept;

class Poly1305 {
public:
    static constexpr std::size_t key_size = 32;
    static constexpr std::size_t tag_size = 16;

    explicit Poly1305(const std::uint8_t key[32]) noexcept;
    ~Poly1305();

    Poly1305& update(const void* data, std::size_t len) noexcept;
    void finish(std::uint8_t tag[16]) noexcept;

private:
    std::uint32_t m_r[5];
    std::uint32_t m_h[5];
    std::uint32_t m_pad[4];
    std::uint8_t  m_buffer[16];
    std::size_t   m_fill;

    void blocks(const std::uint8_t* data, std::size_t len, bool final_partial) noexcept;
};

struct ChaCha20Poly1305 {
    static constexpr std::size_t key_size = 32;
    static constexpr std::size_t nonce_size = 12;
    static constexpr std::size_t tag_size = 16;

    static void seal(const std::uint8_t key[32], const std::uint8_t nonce[12], const void* plaintext, std::size_t len, const void* associated, std::size_t associated_size, std::uint8_t* ciphertext, std::uint8_t tag[16]) noexcept;

    static bool open(const std::uint8_t key[32], const std::uint8_t nonce[12], const void* ciphertext, std::size_t len, const void* associated, std::size_t associated_size, const std::uint8_t tag[16], std::uint8_t* plaintext) noexcept;
};

struct XChaCha20Poly1305 {
    static constexpr std::size_t key_size = 32;
    static constexpr std::size_t nonce_size = 24;
    static constexpr std::size_t tag_size = 16;

    static void seal(const std::uint8_t key[32], const std::uint8_t nonce[24], const void* plaintext, std::size_t len, const void* associated, std::size_t associated_size, std::uint8_t* ciphertext, std::uint8_t tag[16]) noexcept;

    static bool open(const std::uint8_t key[32], const std::uint8_t nonce[24], const void* ciphertext, std::size_t len, const void* associated, std::size_t associated_size, const std::uint8_t tag[16], std::uint8_t* plaintext) noexcept;
};

std::string generate_key();

std::string encrypt(const std::string& key, const std::string& plaintext, const std::string& associated = std::string());

bool decrypt(const std::string& key, const std::string& sealed, std::string& plaintext, const std::string& associated = std::string());

std::string encrypt_with_password(const std::string& password, const std::string& plaintext, const Argon2Params& params, const std::string& associated = std::string());

std::string encrypt_with_password(const std::string& password, const std::string& plaintext, PasswordStrength strength = PasswordStrength::interactive, const std::string& associated = std::string());

bool decrypt_with_password(const std::string& password, const std::string& sealed, std::string& plaintext, const std::string& associated = std::string());

std::string derive_key(const std::string& password, const std::string& salt, const Argon2Params& params);

std::string derive_key(const std::string& password, const std::string& salt, PasswordStrength strength = PasswordStrength::interactive);

} // namespace crypto
} // namespace fizmo

#endif // FIZMO_CRYPTO_AEAD_HPP
