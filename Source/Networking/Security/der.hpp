#ifndef FIZMO_SECURITY_DER_HPP
#define FIZMO_SECURITY_DER_HPP

#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <string>
#include <vector>

namespace fizmo {
namespace networking {
namespace security {
namespace der {

enum Tag : std::uint8_t {
    Boolean         = 0x01,
    Integer         = 0x02,
    BitString       = 0x03,
    OctetString     = 0x04,
    Null            = 0x05,
    ObjectId        = 0x06,
    Utf8String      = 0x0C,
    PrintableString = 0x13,
    IA5String       = 0x16,
    UtcTime         = 0x17,
    GeneralizedTime = 0x18,
    Sequence        = 0x30,
    Set             = 0x31
};

struct Node {
    std::uint8_t        tag = 0;
    const std::uint8_t* raw = nullptr;
    std::size_t         raw_len = 0;
    const std::uint8_t* data = nullptr;
    std::size_t         len = 0;

    bool constructed() const noexcept { return (tag & 0x20) != 0; }
    bool context(std::uint8_t n) const noexcept { return (tag & 0xC0) == 0x80 && (tag & 0x1F) == n; }
    std::vector<std::uint8_t> bytes() const { return std::vector<std::uint8_t>(data, data + len); }
    std::vector<std::uint8_t> raw_bytes() const { return std::vector<std::uint8_t>(raw, raw + raw_len); }
};

bool parse(const std::uint8_t* p, std::size_t n, Node& out) noexcept;

class Reader {
public:
    Reader(const std::uint8_t* p, std::size_t n) noexcept : m_p(p), m_end(p + n) {}
    explicit Reader(const Node& n) noexcept : m_p(n.data), m_end(n.data + n.len) {}

    bool next(Node& out) noexcept;
    bool peek(Node& out) const noexcept;
    bool at_end() const noexcept { return m_p >= m_end; }

private:
    const std::uint8_t* m_p;
    const std::uint8_t* m_end;
};

void encode_length(std::size_t len, std::vector<std::uint8_t>& out);
std::vector<std::uint8_t> encode(std::uint8_t tag, const std::uint8_t* content, std::size_t len);
std::vector<std::uint8_t> encode(std::uint8_t tag, const std::vector<std::uint8_t>& content);
std::vector<std::uint8_t> encode_oid(const std::string& dotted);
std::vector<std::uint8_t> concat(std::initializer_list<std::vector<std::uint8_t>> parts);

std::string oid_to_string(const std::uint8_t* p, std::size_t n);
std::string oid_to_string(const Node& node);

bool certificate_spki(const std::uint8_t* cert, std::size_t len, std::vector<std::uint8_t>& spki);
bool spki_algorithm(const std::vector<std::uint8_t>& spki, std::string& algorithm_oid, std::string& parameter_oid);

bool wrap_traditional_key(const std::string& pem_label, const std::vector<std::uint8_t>& key, std::vector<std::uint8_t>& pkcs8);

} // namespace der
} // namespace security
} // namespace networking
} // namespace fizmo

#endif // FIZMO_SECURITY_DER_HPP
