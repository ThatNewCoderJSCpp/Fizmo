#define ALL_FIZMO
#include <fizmo/includes.hpp>
#include "pem_utils.hpp"

namespace fizmo {
namespace networking {
namespace security {
namespace detail {

auto PEM::block_to_der(
        const char* text,
        std::size_t len,
        std::vector<std::uint8_t>& out
) noexcept -> bool {
        if (!text || len == 0) return false;
        std::string body(text, len);
        std::size_t begin = body.find("-----BEGIN");
        if (begin == std::string::npos) return false;
        std::size_t begin_eol = body.find('\n', begin);
        if (begin_eol == std::string::npos) return false;
        std::size_t end = body.find("-----END", begin_eol);
        if (end == std::string::npos) return false;
        return base64_decode(body.data() + begin_eol + 1, end - begin_eol - 1, out);
    }

auto PEM::split_bundle(const std::string& pem) -> std::vector<std::string> {
        std::vector<std::string> blocks;
        const std::string begin_marker = CERT_BEGIN;
        const std::string end_marker   = CERT_END;
        std::size_t pos = 0;

        while (pos < pem.size()) {
            std::size_t begin = pem.find(begin_marker, pos);
            if (begin == std::string::npos) break;
            std::size_t end = pem.find(end_marker, begin);
            if (end == std::string::npos) break;
            end += end_marker.size();
            blocks.push_back(pem.substr(begin, end - begin));

            if (end < pem.size() && (pem[end] == '\r' || pem[end] == '\n')) {
                ++end;
                if (end < pem.size() && pem[end] == '\n') ++end;
            }

            pos = end;
        }

        return blocks;
    }

auto PEM::looks_like_pem(const std::uint8_t* data, std::size_t len) noexcept -> bool {
        if (!data || len < 5) return false;
        return data[0] == '-' && data[1] == '-' && data[2] == '-' && data[3] == '-' && data[4] == '-';
    }

auto PEM::base64_decode(const char* text, std::size_t len, std::vector<std::uint8_t>& out) noexcept -> bool {
        out.clear();
        if (!text) return false;
        out.reserve((len / 4) * 3 + 3);
        std::uint32_t accum = 0;
        int bits = 0;

        for (std::size_t i = 0; i < len; ++i) {
            const char c = text[i];
            if (c == '=') break;
            if (c == '\n' || c == '\r' || c == ' ' || c == '\t') continue;
            if (c == '-') break;  
            int value = decode_char(c);
            if (value < 0) { out.clear(); return false; }
            accum = (accum << 6) | static_cast<std::uint32_t>(value);
            bits += 6;

            if (bits >= 8) {
                bits -= 8;
                out.push_back(static_cast<std::uint8_t>((accum >> bits) & 0xFF));
            }
        }

        return !out.empty();
    }

auto PEM::decode_char(char c) noexcept -> int {
        if (c >= 'A' && c <= 'Z') return c - 'A';
        if (c >= 'a' && c <= 'z') return c - 'a' + 26;
        if (c >= '0' && c <= '9') return c - '0' + 52;
        if (c == '+') return 62;
        if (c == '/') return 63;
        return -1;
    }

} // namespace detail
} // namespace security
} // namespace networking
} // namespace fizmo
