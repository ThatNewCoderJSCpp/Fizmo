#include "fizmo_library.hpp"
#include "tls_record.hpp"

namespace fizmo {
namespace networking {
namespace security {

const char* content_type_to_string(ContentType ct) noexcept {
    switch (ct) {
        case ContentType::ChangeCipherSpec: return "ChangeCipherSpec";
        case ContentType::Alert:            return "Alert";
        case ContentType::Handshake:        return "Handshake";
        case ContentType::ApplicationData:  return "ApplicationData";
        default:                            return "Unknown";
    }
}

std::string TLSRecord::to_string() const {
    std::string s = "TLSRecord[";
    s += content_type_to_string(content_type);
    s += " v0x";
    char hex[8];
    std::snprintf(hex, sizeof(hex), "%04X", version);
    s += hex;
    s += " len=";
    s += std::to_string(fragment.size());
    s += ']';
    return s;
}

const char* parse_status_to_string(ParseStatus s) noexcept {
    switch (s) {
        case ParseStatus::Ok:              return "Ok";
        case ParseStatus::NeedMoreData:    return "NeedMoreData";
        case ParseStatus::RecordTooLarge:  return "RecordTooLarge";
        case ParseStatus::InvalidHeader:   return "InvalidHeader";
        case ParseStatus::VersionMismatch: return "VersionMismatch";
        default:                           return "Unknown";
    }
}

auto TLSRecordLayer::parse(const std::uint8_t* data, std::size_t len) noexcept -> ParseResult {
    ParseResult result;

    if (!data || len < HEADER_LENGTH) {
        result.status = ParseStatus::NeedMoreData;
        return result;
    }

    std::uint8_t raw_type = data[0];

    if (!is_valid_content_type(raw_type)) {
        result.status = ParseStatus::InvalidHeader;
        return result;
    }

    std::uint16_t version = (static_cast<std::uint16_t>(data[1]) << 8) | static_cast<std::uint16_t>(data[2]);

    // Accept 0x0301–0x0304 (TLS 1.0–1.3 on the wire) and the legacy value 0x0300 (SSLv3 compat in ClientHello)
    if (version < 0x0300 || version > 0x0304) {
        result.status = ParseStatus::VersionMismatch;
        return result;
    }

    std::uint16_t frag_len = (static_cast<std::uint16_t>(data[3]) << 8) | static_cast<std::uint16_t>(data[4]);

    if (frag_len > MAX_PLAINTEXT_LENGTH + 256) {
        result.status = ParseStatus::RecordTooLarge;
        return result;
    }

    std::size_t total = HEADER_LENGTH + frag_len;

    if (len < total) {
        result.status = ParseStatus::NeedMoreData;
        return result;
    }

    result.status         = ParseStatus::Ok;
    result.bytes_consumed = total;
    result.record.content_type = static_cast<ContentType>(raw_type);
    result.record.version      = version;
    result.record.fragment.assign(data + HEADER_LENGTH, data + HEADER_LENGTH + frag_len);
    return result;
}

bool TLSRecordLayer::serialise(const TLSRecord& rec, std::vector<std::uint8_t>& out) noexcept {
    if (rec.fragment.size() > MAX_PLAINTEXT_LENGTH + 256) return false;
    std::size_t total = HEADER_LENGTH + rec.fragment.size();
    std::size_t off   = out.size();
    out.resize(off + total);
    auto frag_len = static_cast<std::uint16_t>(rec.fragment.size());
    out[off + 0] = static_cast<std::uint8_t>(rec.content_type);
    out[off + 1] = static_cast<std::uint8_t>(rec.version >> 8);
    out[off + 2] = static_cast<std::uint8_t>(rec.version & 0xFF);
    out[off + 3] = static_cast<std::uint8_t>(frag_len >> 8);
    out[off + 4] = static_cast<std::uint8_t>(frag_len & 0xFF);

    if (!rec.fragment.empty()) {
        std::memcpy(&out[off + HEADER_LENGTH], rec.fragment.data(), rec.fragment.size());
    }

    return true;
}

bool TLSRecordLayer::build(
    ContentType type, std::uint16_t version,
    const std::uint8_t* payload, std::size_t payload_len,
    std::vector<std::uint8_t>& out
) noexcept {
    TLSRecord rec;
    rec.content_type = type;
    rec.version      = version;
    if (payload && payload_len > 0) { rec.fragment.assign(payload, payload + payload_len); }
    return serialise(rec, out);
}

auto TLSRecordLayer::segment(
    ContentType type,
    std::uint16_t version,
    const std::uint8_t* payload,
    std::size_t payload_len,
    std::size_t max_fragment 
) noexcept -> std::vector<TLSRecord> {
    std::vector<TLSRecord> records;
    if (!payload || payload_len == 0) return records;
    if (max_fragment == 0 || max_fragment > MAX_PLAINTEXT_LENGTH) { max_fragment = MAX_PLAINTEXT_LENGTH; }
    std::size_t off = 0;

    while (off < payload_len) {
        std::size_t chunk = std::min(max_fragment, payload_len - off);
        TLSRecord rec;
        rec.content_type = type;
        rec.version      = version;
        rec.fragment.assign(payload + off, payload + off + chunk);
        records.push_back(std::move(rec));
        off += chunk;
    }

    return records;
}

auto TLSRecordLayer::parse_from(std::vector<std::uint8_t>& buf) noexcept -> ParseResult {
    ParseResult result = parse(buf.data(), buf.size());

    if (result.status == ParseStatus::Ok && result.bytes_consumed > 0) {
        buf.erase(buf.begin(), buf.begin() + static_cast<std::ptrdiff_t>(result.bytes_consumed));
    }
        
    return result;
}

} // namespace security
} // namespace networking
} // namespace fizmo
