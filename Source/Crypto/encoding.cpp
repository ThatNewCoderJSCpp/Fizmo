#include "fizmo_library.hpp"
#include "encoding.hpp"
#include "../Random/random_std_int.hpp"

#include <stdexcept>

namespace fizmo {
namespace crypto {

namespace {

const char kB64[]    = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
const char kB64Url[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";
const char kB32[]    = "ABCDEFGHIJKLMNOPQRSTUVWXYZ234567";

int hex_value(char c) noexcept {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

std::string b64_encode(const char* alphabet, const void* data, std::size_t len, bool pad) {
    const auto* p = static_cast<const std::uint8_t*>(data);
    std::string out;
    out.reserve((len + 2) / 3 * 4);
    std::size_t i = 0;

    for (; i + 3 <= len; i += 3) {
        const std::uint32_t v = (std::uint32_t(p[i]) << 16) | (std::uint32_t(p[i + 1]) << 8) | p[i + 2];
        out += alphabet[(v >> 18) & 63]; out += alphabet[(v >> 12) & 63];
        out += alphabet[(v >> 6) & 63];  out += alphabet[v & 63];
    }

    const std::size_t rest = len - i;

    if (rest == 1) {
        const std::uint32_t v = std::uint32_t(p[i]) << 16;
        out += alphabet[(v >> 18) & 63]; out += alphabet[(v >> 12) & 63];
        if (pad) out += "==";
    } else if (rest == 2) {
        const std::uint32_t v = (std::uint32_t(p[i]) << 16) | (std::uint32_t(p[i + 1]) << 8);
        out += alphabet[(v >> 18) & 63]; out += alphabet[(v >> 12) & 63]; out += alphabet[(v >> 6) & 63];
        if (pad) out += '=';
    }

    return out;
}

int b64_value(char c, bool url) noexcept {
    if (c >= 'A' && c <= 'Z') return c - 'A';
    if (c >= 'a' && c <= 'z') return c - 'a' + 26;
    if (c >= '0' && c <= '9') return c - '0' + 52;
    if (!url && c == '+') return 62;
    if (!url && c == '/') return 63;
    if (url && c == '-') return 62;
    if (url && c == '_') return 63;
    return -1;
}

bool b64_decode(const std::string& text, std::vector<std::uint8_t>& out, bool url) {
    out.clear();
    out.reserve(text.size() * 3 / 4);
    std::uint32_t acc = 0;
    int bits = 0;
    std::size_t pads = 0, symbols = 0;

    for (char c : text) {
        if (c == ' ' || c == '\r' || c == '\n' || c == '\t') continue;
        if (c == '=') { ++pads; continue; }
        if (pads > 0) return false;
        const int v = b64_value(c, url);
        if (v < 0) return false;
        ++symbols;
        acc = (acc << 6) | static_cast<std::uint32_t>(v);
        bits += 6;

        if (bits >= 8) {
            bits -= 8;
            out.push_back(static_cast<std::uint8_t>((acc >> bits) & 0xFFu));
        }
    }

    if (symbols % 4 == 1 || pads > 2) return false;
    if (pads > 0 && (symbols + pads) % 4 != 0) return false;
    if (bits > 0 && (acc & ((1u << bits) - 1u)) != 0) return false;
    return true;
}

} // namespace

std::string to_hex(const void* data, std::size_t len, bool upper) {
    const char* digits = upper ? "0123456789ABCDEF" : "0123456789abcdef";
    const auto* p = static_cast<const std::uint8_t*>(data);
    std::string out(len * 2, '0');

    for (std::size_t i = 0; i < len; ++i) {
        out[2 * i]     = digits[p[i] >> 4];
        out[2 * i + 1] = digits[p[i] & 15];
    }

    return out;
}

bool from_hex(const std::string& hex, std::vector<std::uint8_t>& out) {
    out.clear();
    std::size_t start = 0;
    if (hex.size() >= 2 && hex[0] == '0' && (hex[1] == 'x' || hex[1] == 'X')) start = 2;
    if ((hex.size() - start) % 2 != 0) return false;
    out.reserve((hex.size() - start) / 2);

    for (std::size_t i = start; i < hex.size(); i += 2) {
        const int hi = hex_value(hex[i]), lo = hex_value(hex[i + 1]);
        if (hi < 0 || lo < 0) { out.clear(); return false; }
        out.push_back(static_cast<std::uint8_t>((hi << 4) | lo));
    }

    return true;
}

std::vector<std::uint8_t> from_hex(const std::string& hex) {
    std::vector<std::uint8_t> out;
    if (!from_hex(hex, out)) throw std::invalid_argument("from_hex: malformed input");
    return out;
}

std::string base64_encode(const void* data, std::size_t len, bool pad) { return b64_encode(kB64, data, len, pad); }

std::string base64url_encode(const void* data, std::size_t len, bool pad) { return b64_encode(kB64Url, data, len, pad); }

bool base64_decode(const std::string& text, std::vector<std::uint8_t>& out) { return b64_decode(text, out, false); }

std::vector<std::uint8_t> base64_decode(const std::string& text) {
    std::vector<std::uint8_t> out;
    if (!b64_decode(text, out, false)) throw std::invalid_argument("base64_decode: malformed input");
    return out;
}

bool base64url_decode(const std::string& text, std::vector<std::uint8_t>& out) { return b64_decode(text, out, true); }

std::vector<std::uint8_t> base64url_decode(const std::string& text) {
    std::vector<std::uint8_t> out;
    if (!b64_decode(text, out, true)) throw std::invalid_argument("base64url_decode: malformed input");
    return out;
}

std::string base32_encode(const void* data, std::size_t len, bool pad) {
    const auto* p = static_cast<const std::uint8_t*>(data);
    std::string out;
    std::uint64_t acc = 0;
    int bits = 0;

    for (std::size_t i = 0; i < len; ++i) {
        acc = (acc << 8) | p[i];
        bits += 8;

        while (bits >= 5) {
            bits -= 5;
            out += kB32[(acc >> bits) & 31];
        }
    }

    if (bits > 0) out += kB32[(acc << (5 - bits)) & 31];
    if (pad) while (out.size() % 8 != 0) out += '=';
    return out;
}

bool base32_decode(const std::string& text, std::vector<std::uint8_t>& out) {
    out.clear();
    std::uint64_t acc = 0;
    int bits = 0;

    for (char c : text) {
        if (c == '=' || c == ' ' || c == '\n' || c == '\r') continue;
        int v;
        if (c >= 'A' && c <= 'Z') v = c - 'A';
        else if (c >= 'a' && c <= 'z') v = c - 'a';
        else if (c >= '2' && c <= '7') v = c - '2' + 26;
        else return false;
        acc = (acc << 5) | static_cast<std::uint64_t>(v);
        bits += 5;

        if (bits >= 8) {
            bits -= 8;
            out.push_back(static_cast<std::uint8_t>((acc >> bits) & 0xFFu));
        }
    }

    return true;
}

std::string url_encode(const std::string& s, bool space_as_plus) {
    static const char* digits = "0123456789ABCDEF";
    std::string out;
    out.reserve(s.size());

    for (unsigned char c : s) {
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' || c == '~') {
            out += static_cast<char>(c);
        } else if (c == ' ' && space_as_plus) {
            out += '+';
        } else {
            out += '%';
            out += digits[c >> 4];
            out += digits[c & 15];
        }
    }

    return out;
}

std::string url_decode(const std::string& s, bool plus_as_space) {
    std::string out;
    out.reserve(s.size());

    for (std::size_t i = 0; i < s.size(); ++i) {
        const char c = s[i];

        if (c == '%' && i + 2 < s.size() && hex_value(s[i + 1]) >= 0 && hex_value(s[i + 2]) >= 0) {
            out += static_cast<char>((hex_value(s[i + 1]) << 4) | hex_value(s[i + 2]));
            i += 2;
        } else if (c == '+' && plus_as_space) {
            out += ' ';
        } else {
            out += c;
        }
    }

    return out;
}

std::string random_token(std::size_t bytes) {
    std::vector<std::uint8_t> buf(bytes);
    if (bytes) random_bytes(buf.data(), bytes);
    return to_hex(buf);
}

std::string random_token_base64url(std::size_t bytes) {
    std::vector<std::uint8_t> buf(bytes);
    if (bytes) random_bytes(buf.data(), bytes);
    return base64url_encode(buf);
}

std::string uuid_v4() {
    std::uint8_t b[16];
    random_bytes(b, sizeof(b));
    b[6] = static_cast<std::uint8_t>((b[6] & 0x0Fu) | 0x40u);
    b[8] = static_cast<std::uint8_t>((b[8] & 0x3Fu) | 0x80u);
    const std::string h = to_hex(b, sizeof(b));
    return h.substr(0, 8) + '-' + h.substr(8, 4) + '-' + h.substr(12, 4) + '-' + h.substr(16, 4) + '-' + h.substr(20, 12);
}

} // namespace crypto
} // namespace fizmo
