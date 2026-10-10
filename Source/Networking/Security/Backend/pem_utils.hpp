#ifndef FIZMO_PEM_UTILS_HPP
#define FIZMO_PEM_UTILS_HPP

#include <string>
#include <vector>
#include <cstdint>

namespace fizmo {
namespace networking {
namespace security {
namespace detail {

class PEM {
public:
    static constexpr const char* CERT_BEGIN = "-----BEGIN CERTIFICATE-----";
    static constexpr const char* CERT_END   = "-----END CERTIFICATE-----";

    static bool block_to_der(
        const char* text,
        std::size_t len,
        std::vector<std::uint8_t>& out
    ) noexcept;

    static bool block_to_der(const std::string& text, std::vector<std::uint8_t>& out) noexcept {
        return block_to_der(text.data(), text.size(), out);
    }

    static std::vector<std::string> split_bundle(const std::string& pem);

    static bool looks_like_pem(const std::uint8_t* data, std::size_t len) noexcept;

    static bool base64_decode(const char* text, std::size_t len, std::vector<std::uint8_t>& out) noexcept;

private:
    static int decode_char(char c) noexcept;
};

} // namespace detail
} // namespace security
} // namespace networking
} // namespace fizmo

#endif // FIZMO_PEM_UTILS_HPP