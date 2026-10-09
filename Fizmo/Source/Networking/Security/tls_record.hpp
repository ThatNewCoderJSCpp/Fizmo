#ifndef FIZMO_TLS_RECORD_HPP
#define FIZMO_TLS_RECORD_HPP

#include "tls_context.hpp"
#include "../../Basic/basic_includes.hpp"
#include <vector>
#include <cstdint>
#include <cstring>
#include <string>
#include <algorithm>

namespace fizmo {
namespace networking {
namespace security {

enum class ContentType : std::uint8_t {
    ChangeCipherSpec = 20,
    Alert            = 21,
    Handshake        = 22,
    ApplicationData  = 23
};

 const char* content_type_to_string(ContentType ct) noexcept;

inline bool is_valid_content_type(std::uint8_t raw) noexcept {
    return raw >= 20 && raw <= 23;
}

struct TLSRecord {
    ContentType                  content_type = ContentType::ApplicationData;
    std::uint16_t                version      = 0x0303;  // TLS 1.2 on the wire
    std::vector<std::uint8_t>    fragment;

    bool empty() const noexcept { return fragment.empty(); }

    std::string to_string() const;
};

enum class ParseStatus : std::uint8_t {
    Ok = 0,          // Record parsed successfully
    NeedMoreData,    // Incomplete record; caller should accumulate bytes
    RecordTooLarge,  // Payload exceeds max record size
    InvalidHeader,   // Unrecognised content type or malformed header
    VersionMismatch  // Record version outside accepted range
};

 const char* parse_status_to_string(ParseStatus s) noexcept;

struct ParseResult {
    ParseStatus  status         = ParseStatus::NeedMoreData;
    std::size_t  bytes_consumed = 0;  
    TLSRecord    record;

    explicit operator bool() const noexcept { return status == ParseStatus::Ok; }
};

class TLSRecordLayer {
public:
    static constexpr std::size_t MAX_PLAINTEXT_LENGTH  = std::size_t(1) << 14;
    static constexpr std::size_t MAX_RECORD_LENGTH     = 5 + MAX_PLAINTEXT_LENGTH + 256;
    static constexpr std::size_t HEADER_LENGTH         = 5;

    TLSRecordLayer() noexcept = default;
    ~TLSRecordLayer() noexcept = default;
    TLSRecordLayer(const TLSRecordLayer&)            = delete;
    TLSRecordLayer& operator=(const TLSRecordLayer&) = delete;
    TLSRecordLayer(TLSRecordLayer&&)                 = default;
    TLSRecordLayer& operator=(TLSRecordLayer&&)      = default;

    void feed(const std::uint8_t* data, std::size_t len) {
        if (!data || len == 0) return;
        m_buf.insert(m_buf.end(), data, data + len);
    }

    void feed(const std::vector<std::uint8_t>& data) { feed(data.data(), data.size()); }
    ParseResult next_record() noexcept { return parse_from(m_buf); }

    static ParseResult parse(const std::uint8_t* data, std::size_t len) noexcept;

    std::size_t buffered() const noexcept { return m_buf.size(); }
    void reset() noexcept { m_buf.clear(); }

    static bool serialise(const TLSRecord& rec, std::vector<std::uint8_t>& out) noexcept;

    static bool build(
        ContentType type, std::uint16_t version,
        const std::uint8_t* payload, std::size_t payload_len,
        std::vector<std::uint8_t>& out
    ) noexcept;

    static std::vector<TLSRecord> segment(
        ContentType type,
        std::uint16_t version,
        const std::uint8_t* payload,
        std::size_t payload_len,
        std::size_t max_fragment = MAX_PLAINTEXT_LENGTH
    ) noexcept;

private:
    std::vector<std::uint8_t> m_buf;  

    ParseResult parse_from(std::vector<std::uint8_t>& buf) noexcept;
};

} // namespace security
} // namespace networking
} // namespace fizmo

#endif // FIZMO_TLS_RECORD_HPP