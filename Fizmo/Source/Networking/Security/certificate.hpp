#ifndef FIZMO_CERTIFICATE_HPP
#define FIZMO_CERTIFICATE_HPP

#include "../../Basic/basic_includes.hpp"
#include <string>
#include <vector>
#include <cstdint>
#include <cstring>
#include <chrono>
#include <algorithm>
#include <fstream>
#include <iterator>

namespace fizmo {
namespace networking {
namespace security {

struct KeyUsage {
    bool digital_signature = false;
    bool key_encipherment  = false;
    bool data_encipherment = false;
    bool key_agreement     = false;
    bool cert_sign         = false;
    bool crl_sign          = false;
    bool non_repudiation   = false;
    bool encipher_only     = false;
    bool decipher_only     = false;

    bool any() const noexcept {
        return digital_signature || key_encipherment || data_encipherment ||
               key_agreement    || cert_sign        || crl_sign          ||
               non_repudiation  || encipher_only    || decipher_only;
    }
};

struct SubjectAltName {
    enum class Type { DNS, IP };
    Type        type;
    std::string value;
};

class Certificate {
public:
    using Clock     = std::chrono::system_clock;
    using TimePoint = Clock::time_point;

    Certificate() noexcept
    #ifdef OS_WINDOWS
        : m_ctx(nullptr)
    #endif
    {}

    ~Certificate() noexcept { release(); }
    Certificate(const Certificate&) = delete;
    Certificate& operator=(const Certificate&) = delete;

    Certificate(Certificate&& other) noexcept
        : m_subject(std::move(other.m_subject)),
          m_issuer(std::move(other.m_issuer)),
          m_serial(std::move(other.m_serial)),
          m_not_before(other.m_not_before),
          m_not_after(other.m_not_after),
          m_sans(std::move(other.m_sans)),
          m_key_usage(other.m_key_usage),
          m_der(std::move(other.m_der))
    #ifdef OS_WINDOWS
        , m_ctx(other.m_ctx)
    #endif
    {
    #ifdef OS_WINDOWS
        other.m_ctx = nullptr;
    #endif
    }

    Certificate& operator=(Certificate&& other) noexcept {
        if (this != &other) {
            release();
            m_subject    = std::move(other.m_subject);
            m_issuer     = std::move(other.m_issuer);
            m_serial     = std::move(other.m_serial);
            m_not_before = other.m_not_before;
            m_not_after  = other.m_not_after;
            m_sans       = std::move(other.m_sans);
            m_key_usage  = other.m_key_usage;
            m_der        = std::move(other.m_der);
        #ifdef OS_WINDOWS
            m_ctx        = other.m_ctx;
            other.m_ctx  = nullptr;
        #endif
        }
        return *this;
    }

    bool load_from_der(const std::uint8_t* data, std::size_t len) noexcept {
        if (!data || len == 0) return false;
        release();
        m_der.assign(data, data + len);
    #ifdef OS_WINDOWS
        return parse_windows();
    #else
        return false;
    #endif
    }

    bool load_from_der(const std::vector<std::uint8_t>& data) noexcept {
        return load_from_der(data.data(), data.size());
    }

    bool load_from_pem(const std::uint8_t* data, std::size_t len) noexcept {
        if (!data || len == 0) return false;
        release();

    #ifdef OS_WINDOWS
        DWORD der_len = 0;
        if (
            !CryptStringToBinaryA(
                reinterpret_cast<const char*>(data),
                static_cast<DWORD>(len),
                CRYPT_STRING_BASE64HEADER,
                nullptr, 
                &der_len, 
                nullptr, nullptr
            )
        ) {
            return false;
        }

        m_der.resize(der_len);

        if (
            !CryptStringToBinaryA(
                reinterpret_cast<const char*>(data),
                static_cast<DWORD>(len),
                CRYPT_STRING_BASE64HEADER,
                m_der.data(), 
                &der_len, 
                nullptr, nullptr
            )
        ) {
            m_der.clear();
            return false;
        }

        m_der.resize(der_len);
        return parse_windows();
    #else
        return false;
    #endif
    }

    bool load_from_pem(const std::string& pem) noexcept {
        return load_from_pem(reinterpret_cast<const std::uint8_t*>(pem.data()), pem.size());
    }

    bool load_from_file(const std::string& path) noexcept {
        std::ifstream file(path, std::ios::binary);
        if (!file) return false;
        std::vector<std::uint8_t> buf((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        if (buf.empty()) return false;

        if (buf.size() >= 5 && buf[0] == '-' && buf[1] == '-' && buf[2] == '-' && buf[3] == '-' && buf[4] == '-') {
            return load_from_pem(buf.data(), buf.size());
        }

        return load_from_der(buf.data(), buf.size());
    }

    bool loaded()       const noexcept { return !m_der.empty(); }
    explicit operator bool() const noexcept { return loaded(); }

    const std::string&  subject()       const noexcept { return m_subject; }
    const std::string&  issuer()        const noexcept { return m_issuer; }
    const std::string&  serial_number() const noexcept { return m_serial; }

    TimePoint not_before() const noexcept { return m_not_before; }
    TimePoint not_after()  const noexcept { return m_not_after; }

    const std::vector<SubjectAltName>& subject_alt_names() const noexcept { return m_sans; }
    const KeyUsage&                    key_usage()         const noexcept { return m_key_usage; }
    const std::vector<std::uint8_t>&   raw_der()           const noexcept { return m_der; }

#ifdef OS_WINDOWS
    PCCERT_CONTEXT native_handle() const noexcept { return m_ctx; }
#endif

    bool is_expired() const noexcept {
        if (!loaded()) return true;
        auto now = Clock::now();
        return now < m_not_before || now > m_not_after;
    }

    bool is_self_signed() const noexcept {
        if (!loaded()) return false;

    #ifdef OS_WINDOWS
        if (m_ctx) {
            return CertCompareCertificateName(
                X509_ASN_ENCODING | PKCS_7_ASN_ENCODING,
                &m_ctx->pCertInfo->Subject,
                &m_ctx->pCertInfo->Issuer) == TRUE;
        }
    #endif

        return m_subject == m_issuer;
    }

    bool matches_hostname(const std::string& host) const noexcept {
        if (!loaded() || host.empty()) return false;
        bool has_dns_san = false;

        for (const auto& san : m_sans) {
            if (san.type == SubjectAltName::Type::DNS) {
                has_dns_san = true;
                if (wildcard_match(san.value, host)) return true;
            } else if (san.type == SubjectAltName::Type::IP) {
                if (case_insensitive_eq(san.value, host)) return true;
            }
        }

        if (!has_dns_san) {
            std::string cn = extract_cn(m_subject);
            if (!cn.empty() && wildcard_match(cn, host)) return true;
        }

        return false;
    }

    std::string to_string() const {
        if (!loaded()) return "Certificate [empty]";
        std::string s = "Certificate [";
        s += "subject=\"" + m_subject + "\"";
        s += " issuer=\"" + m_issuer + "\"";
        s += " serial=" + m_serial;
        s += is_expired() ? " EXPIRED" : " valid";
        s += is_self_signed() ? " self-signed" : "";
        s += "]";
        return s;
    }

private:
    std::string                 m_subject;
    std::string                 m_issuer;
    std::string                 m_serial;
    TimePoint                   m_not_before{};
    TimePoint                   m_not_after{};
    std::vector<SubjectAltName> m_sans;
    KeyUsage                    m_key_usage;
    std::vector<std::uint8_t>   m_der;

#ifdef OS_WINDOWS
    PCCERT_CONTEXT              m_ctx;
#endif

    void release() noexcept {
    #ifdef OS_WINDOWS
        if (m_ctx) { CertFreeCertificateContext(m_ctx); m_ctx = nullptr; }
    #endif

        m_subject.clear();
        m_issuer.clear();
        m_serial.clear();
        m_not_before = TimePoint{};
        m_not_after  = TimePoint{};
        m_sans.clear();
        m_key_usage  = KeyUsage{};
        m_der.clear();
    }

#ifdef OS_WINDOWS
    bool parse_windows() noexcept {
        m_ctx = CertCreateCertificateContext(
            X509_ASN_ENCODING | PKCS_7_ASN_ENCODING,
            m_der.data(),
            static_cast<DWORD>(m_der.size())
        );

        if (!m_ctx) { m_der.clear(); return false; }
        m_subject = get_name_string(CERT_NAME_SIMPLE_DISPLAY_TYPE, 0);
        m_issuer  = get_name_string(CERT_NAME_SIMPLE_DISPLAY_TYPE, CERT_NAME_ISSUER_FLAG);
        m_serial  = format_serial(m_ctx->pCertInfo->SerialNumber);
        m_not_before = filetime_to_timepoint(m_ctx->pCertInfo->NotBefore);
        m_not_after  = filetime_to_timepoint(m_ctx->pCertInfo->NotAfter);
        parse_sans();
        parse_key_usage();
        return true;
    }

    std::string get_name_string(DWORD type, DWORD flags) const noexcept {
        char buf[512]{};
        DWORD len = CertGetNameStringA(m_ctx, type, flags, nullptr, buf, sizeof(buf));
        return (len > 1) ? std::string(buf) : std::string();
    }

    static std::string format_serial(const CRYPT_INTEGER_BLOB& blob) {
        std::string result;
        result.reserve(blob.cbData * 3);

        for (DWORD i = blob.cbData; i > 0; --i) {
            char hex[4];
            std::snprintf(hex, sizeof(hex), "%02X", blob.pbData[i - 1]);
            if (!result.empty()) result += ':';
            result += hex;
        }

        return result;
    }

    static TimePoint filetime_to_timepoint(const FILETIME& ft) noexcept {
        ULARGE_INTEGER ull;
        ull.LowPart  = ft.dwLowDateTime;
        ull.HighPart = ft.dwHighDateTime;
        auto secs_since_1601 = static_cast<std::int64_t>(ull.QuadPart / 10'000'000ULL);
        auto unix_secs = secs_since_1601 - 11644473600LL;
        return Clock::from_time_t(static_cast<std::time_t>(unix_secs));
    }

    void parse_sans() noexcept {
        PCERT_EXTENSION ext = CertFindExtension(
            szOID_SUBJECT_ALT_NAME2,
            m_ctx->pCertInfo->cExtension,
            m_ctx->pCertInfo->rgExtension
        );

        if (!ext) return;
        DWORD decoded_size = 0;

        if (
            !CryptDecodeObjectEx(
                X509_ASN_ENCODING,
                X509_ALTERNATE_NAME,
                ext->Value.pbData,
                ext->Value.cbData,
                CRYPT_DECODE_ALLOC_FLAG,
                nullptr,
                &decoded_size,    
                &decoded_size
            )
        ) {
            return;
        }

        PCERT_ALT_NAME_INFO info = nullptr;
        DWORD info_size = 0;

        if (
            !CryptDecodeObjectEx(
                X509_ASN_ENCODING,
                X509_ALTERNATE_NAME,
                ext->Value.pbData,
                ext->Value.cbData,
                CRYPT_DECODE_ALLOC_FLAG,
                nullptr,
                &info,
                &info_size
            )
        ) {
            return;
        }

        if (!info) return;

        for (DWORD i = 0; i < info->cAltEntry; ++i) {
            const auto& entry = info->rgAltEntry[i];

            if (entry.dwAltNameChoice == CERT_ALT_NAME_DNS_NAME && entry.pwszDNSName) {
                int needed = WideCharToMultiByte(CP_UTF8, 0, entry.pwszDNSName, -1, nullptr, 0, nullptr, nullptr);
                
                if (needed > 1) {
                    std::string dns(needed - 1, '\0');
                    WideCharToMultiByte(CP_UTF8, 0, entry.pwszDNSName, -1, &dns[0], needed, nullptr, nullptr);
                    m_sans.push_back({SubjectAltName::Type::DNS, std::move(dns)});
                }
            } else if (entry.dwAltNameChoice == CERT_ALT_NAME_IP_ADDRESS) {
                auto& blob = entry.IPAddress;

                if (blob.cbData == 4) {
                    // IPv4
                    char addr[INET_ADDRSTRLEN]{};
                    inet_ntop(AF_INET, blob.pbData, addr, sizeof(addr));
                    m_sans.push_back({SubjectAltName::Type::IP, addr});
                } else if (blob.cbData == 16) {
                    // IPv6
                    char addr[INET6_ADDRSTRLEN]{};
                    inet_ntop(AF_INET6, blob.pbData, addr, sizeof(addr));
                    m_sans.push_back({SubjectAltName::Type::IP, addr});
                }
            }
        }

        LocalFree(info);
    }

    void parse_key_usage() noexcept {
        PCERT_EXTENSION ext = CertFindExtension(
            szOID_KEY_USAGE,
            m_ctx->pCertInfo->cExtension,
            m_ctx->pCertInfo->rgExtension
        );

        if (!ext) return;
        DWORD flags_size = 0;
        CRYPT_BIT_BLOB* bits = nullptr;

        if (
            !CryptDecodeObjectEx(
                X509_ASN_ENCODING,
                X509_KEY_USAGE,
                ext->Value.pbData,
                ext->Value.cbData,
                CRYPT_DECODE_ALLOC_FLAG,
                nullptr,
                &bits,
                &flags_size
            )
        ) {
            return;
        }

        if (!bits || bits->cbData == 0) { if (bits) LocalFree(bits); return; }
        BYTE b = bits->pbData[0];
        m_key_usage.digital_signature = (b & CERT_DIGITAL_SIGNATURE_KEY_USAGE) != 0;
        m_key_usage.non_repudiation   = (b & CERT_NON_REPUDIATION_KEY_USAGE)   != 0;
        m_key_usage.key_encipherment  = (b & CERT_KEY_ENCIPHERMENT_KEY_USAGE)  != 0;
        m_key_usage.data_encipherment = (b & CERT_DATA_ENCIPHERMENT_KEY_USAGE) != 0;
        m_key_usage.key_agreement     = (b & CERT_KEY_AGREEMENT_KEY_USAGE)     != 0;
        m_key_usage.cert_sign         = (b & CERT_KEY_CERT_SIGN_KEY_USAGE)     != 0;
        m_key_usage.crl_sign          = (b & CERT_OFFLINE_CRL_SIGN_KEY_USAGE)  != 0;

        if (bits->cbData > 1) {
            BYTE b2 = bits->pbData[1];
            m_key_usage.decipher_only = (b2 & CERT_DECIPHER_ONLY_KEY_USAGE) != 0;
        }

        if (m_key_usage.key_agreement && bits->cbData > 0) {
            m_key_usage.encipher_only = (b & CERT_ENCIPHER_ONLY_KEY_USAGE) != 0;
        }

        LocalFree(bits);
    }
#endif // OS_WINDOWS

    static bool case_insensitive_eq(const std::string& a, const std::string& b) noexcept {
        if (a.size() != b.size()) return false;
        for (std::size_t i = 0; i < a.size(); ++i) { if (to_lower(a[i]) != to_lower(b[i])) return false; }
        return true;
    }

    static char to_lower(char c) noexcept {
        return (c >= 'A' && c <= 'Z') ? static_cast<char>(c + ('a' - 'A')) : c;
    }

    static bool wildcard_match(const std::string& pattern, const std::string& hostname) noexcept {
        if (pattern.empty() || hostname.empty()) return false;
        std::size_t star = pattern.find('*');
        if (star == std::string::npos) { return case_insensitive_eq(pattern, hostname); }
        std::size_t dot = pattern.find('.');
        if (dot == std::string::npos || star > dot) return false;
        std::string suffix = pattern.substr(dot); 
        if (suffix.find('.', 1) == std::string::npos) return false;
        std::string prefix = pattern.substr(0, star);
        std::string postfix = pattern.substr(star + 1, dot - star - 1);
        if (hostname.size() < suffix.size()) return false;
        std::string host_suffix = hostname.substr(hostname.size() - suffix.size());
        if (!case_insensitive_eq(host_suffix, suffix)) return false;
        std::string host_label = hostname.substr(0, hostname.size() - suffix.size());
        if (host_label.find('.') != std::string::npos) return false;
        if (host_label.size() < prefix.size() + postfix.size()) return false;
        std::string label_start = host_label.substr(0, prefix.size());
        std::string label_end   = host_label.substr(host_label.size() - postfix.size());
        return case_insensitive_eq(label_start, prefix) && (postfix.empty() || case_insensitive_eq(label_end, postfix));
    }

    static std::string extract_cn(const std::string& subject) {
        std::size_t pos = 0;

        while (pos < subject.size()) {
            while (pos < subject.size() && subject[pos] == ' ') ++pos;

            if (
                pos + 3 <= subject.size() &&
                (subject[pos] == 'C' || subject[pos] == 'c') &&
                (subject[pos + 1] == 'N' || subject[pos + 1] == 'n') &&
                subject[pos + 2] == '='
            ) {
                pos += 3;
                while (pos < subject.size() && subject[pos] == ' ') ++pos;
                std::size_t end = subject.find(',', pos);
                if (end == std::string::npos) end = subject.size();
                while (end > pos && subject[end - 1] == ' ') --end;
                return subject.substr(pos, end - pos);
            }

            pos = subject.find(',', pos);
            if (pos == std::string::npos) break;
            ++pos;
        }
        
        return {};
    }
};

} // namespace security
} // namespace networking
} // namespace fizmo

#endif // FIZMO_CERTIFICATE_HPP