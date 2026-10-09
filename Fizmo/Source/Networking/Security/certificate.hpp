#ifndef FIZMO_CERTIFICATE_HPP
#define FIZMO_CERTIFICATE_HPP

#include "Backend/native_backend.hpp"
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

// KeyUsage, SubjectAltName, CertificateInfo now live in Backend/tls_types.hpp
// so the platform backends can populate them without depending on this header.

class Certificate {
public:
    using Clock     = std::chrono::system_clock;
    using TimePoint = Clock::time_point;

    Certificate() noexcept = default;
    ~Certificate() noexcept = default;

    Certificate(const Certificate&) = delete;
    Certificate& operator=(const Certificate&) = delete;

    Certificate(Certificate&& other) noexcept : m_native(std::move(other.m_native)), m_der(std::move(other.m_der)) {}

    Certificate& operator=(Certificate&& other) noexcept;

public:
    bool load_from_der(const std::uint8_t* data, std::size_t len) noexcept;

    bool load_from_der(const std::vector<std::uint8_t>& data) noexcept {
        return load_from_der(data.data(), data.size());
    }

    bool load_from_pem(const std::uint8_t* data, std::size_t len) noexcept;

    bool load_from_pem(const std::string& pem) noexcept {
        return load_from_pem(reinterpret_cast<const std::uint8_t*>(pem.data()), pem.size());
    }

    bool load_from_file(const std::string& path) noexcept;

    void release() noexcept {
        m_native.release();
        m_der.clear();
    }

public:
    bool loaded() const noexcept { return m_native.valid() && !m_der.empty(); }
    explicit operator bool() const noexcept { return loaded(); }

    const std::string& subject()       const noexcept { return m_native.info().subject; }
    const std::string& issuer()        const noexcept { return m_native.info().issuer; }
    const std::string& serial_number() const noexcept { return m_native.info().serial; }

    TimePoint not_before() const noexcept { return Clock::from_time_t(static_cast<std::time_t>(m_native.info().not_before)); }
    TimePoint not_after()  const noexcept { return Clock::from_time_t(static_cast<std::time_t>(m_native.info().not_after)); }

    const std::vector<SubjectAltName>& subject_alt_names() const noexcept { return m_native.info().sans; }
    const KeyUsage&                    key_usage()         const noexcept { return m_native.info().key_usage; }
    const std::vector<std::uint8_t>&   raw_der()           const noexcept { return m_der; }

    const detail::NativeCertificate& native() const noexcept { return m_native; }
    detail::NativeCertificate&       native()       noexcept { return m_native; }

    bool is_expired() const noexcept;

    bool is_self_signed() const noexcept {
        if (!loaded()) return false;
        return m_native.self_signed();
    }

    bool matches_hostname(const std::string& host) const noexcept;

    std::string to_string() const;

private:
    detail::NativeCertificate m_native;
    std::vector<std::uint8_t> m_der;

private:
    static char to_lower(char c) noexcept {
        return (c >= 'A' && c <= 'Z') ? static_cast<char>(c + ('a' - 'A')) : c;
    }

    static bool case_insensitive_eq(const std::string& a, const std::string& b) noexcept;

    static bool wildcard_match(const std::string& pattern, const std::string& hostname) noexcept;

    static std::string extract_cn(const std::string& subject);
};

} // namespace security
} // namespace networking
} // namespace fizmo

#endif // FIZMO_CERTIFICATE_HPP