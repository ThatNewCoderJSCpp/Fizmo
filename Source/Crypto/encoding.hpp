#ifndef FIZMO_CRYPTO_ENCODING_HPP
#define FIZMO_CRYPTO_ENCODING_HPP

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace fizmo {
namespace crypto {

std::string to_hex(const void* data, std::size_t len, bool upper = false);
inline std::string to_hex(const std::vector<std::uint8_t>& v, bool upper = false) { return to_hex(v.data(), v.size(), upper); }
inline std::string to_hex(const std::string& s, bool upper = false) { return to_hex(s.data(), s.size(), upper); }

template <std::size_t N>
std::string to_hex(const std::array<std::uint8_t, N>& a, bool upper = false) { return to_hex(a.data(), a.size(), upper); }

bool from_hex(const std::string& hex, std::vector<std::uint8_t>& out);
std::vector<std::uint8_t> from_hex(const std::string& hex);

std::string base64_encode(const void* data, std::size_t len, bool pad = true);
inline std::string base64_encode(const std::vector<std::uint8_t>& v, bool pad = true) { return base64_encode(v.data(), v.size(), pad); }
inline std::string base64_encode(const std::string& s, bool pad = true) { return base64_encode(s.data(), s.size(), pad); }

std::string base64url_encode(const void* data, std::size_t len, bool pad = false);
inline std::string base64url_encode(const std::vector<std::uint8_t>& v, bool pad = false) { return base64url_encode(v.data(), v.size(), pad); }
inline std::string base64url_encode(const std::string& s, bool pad = false) { return base64url_encode(s.data(), s.size(), pad); }

bool base64_decode(const std::string& text, std::vector<std::uint8_t>& out);
std::vector<std::uint8_t> base64_decode(const std::string& text);

bool base64url_decode(const std::string& text, std::vector<std::uint8_t>& out);
std::vector<std::uint8_t> base64url_decode(const std::string& text);

std::string base32_encode(const void* data, std::size_t len, bool pad = true);
bool base32_decode(const std::string& text, std::vector<std::uint8_t>& out);

std::string url_encode(const std::string& s, bool space_as_plus = false);
std::string url_decode(const std::string& s, bool plus_as_space = false);

std::string random_token(std::size_t bytes = 32);
std::string random_token_base64url(std::size_t bytes = 32);
std::string uuid_v4();

} // namespace crypto
} // namespace fizmo

#endif // FIZMO_CRYPTO_ENCODING_HPP
