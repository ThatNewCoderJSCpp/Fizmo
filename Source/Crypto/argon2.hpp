#ifndef FIZMO_CRYPTO_ARGON2_HPP
#define FIZMO_CRYPTO_ARGON2_HPP

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace fizmo {
namespace crypto {

enum class Argon2Type : std::uint32_t {
    d  = 0,
    i  = 1,
    id = 2,
};

enum class PasswordStrength {
    minimum,
    interactive,
    moderate,
    sensitive,
};

struct Argon2Params {
    Argon2Type    type        = Argon2Type::id;
    std::uint32_t memory_kib  = 65536;
    std::uint32_t iterations  = 2;
    std::uint32_t parallelism = 1;
    std::uint32_t hash_length = 32;
    std::uint32_t salt_length = 16;

    static Argon2Params of(PasswordStrength strength) noexcept;
};

std::vector<std::uint8_t> argon2(const Argon2Params& params, const void* password, std::size_t password_size, const void* salt, std::size_t salt_size, const void* secret = nullptr, std::size_t secret_size = 0, const void* associated = nullptr, std::size_t associated_size = 0);

std::vector<std::uint8_t> argon2(const Argon2Params& params, const std::string& password, const std::string& salt, const std::string& secret = std::string(), const std::string& associated = std::string());

std::string argon2_encode(const Argon2Params& params, const std::vector<std::uint8_t>& salt, const std::vector<std::uint8_t>& hash);

bool argon2_decode(const std::string& encoded, Argon2Params& params, std::vector<std::uint8_t>& salt, std::vector<std::uint8_t>& hash);

std::string hash_password(const std::string& password, const Argon2Params& params);

std::string hash_password(const std::string& password, PasswordStrength strength = PasswordStrength::interactive);

bool verify_password(const std::string& password, const std::string& encoded);

bool password_needs_rehash(const std::string& encoded, const Argon2Params& params);

bool password_needs_rehash(const std::string& encoded, PasswordStrength strength);

} // namespace crypto
} // namespace fizmo

#endif // FIZMO_CRYPTO_ARGON2_HPP
