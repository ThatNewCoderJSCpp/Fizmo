#define ALL_FIZMO
#include <fizmo/includes.hpp>
#include "certificate.hpp"

namespace fizmo {
namespace networking {
namespace security {

auto Certificate::operator=(Certificate&& other) noexcept -> Certificate& {
        if (this != &other) {
            m_native = std::move(other.m_native);
            m_der    = std::move(other.m_der);
        }

        return *this;
    }

auto Certificate::load_from_der(const std::uint8_t* data, std::size_t len) noexcept -> bool {
        release();
        if (!data || len == 0) return false;
        m_der.assign(data, data + len);

        if (!m_native.parse(m_der.data(), m_der.size())) {
            m_der.clear();
            return false;
        }

        return true;
    }

auto Certificate::load_from_pem(const std::uint8_t* data, std::size_t len) noexcept -> bool {
        release();
        if (!data || len == 0) return false;
        std::vector<std::uint8_t> der;

        if (!detail::PEM::block_to_der(reinterpret_cast<const char*>(data), len, der)) {
            return false;
        }

        return load_from_der(der.data(), der.size());
    }

auto Certificate::load_from_file(const std::string& path) noexcept -> bool {
        std::ifstream file(path, std::ios::binary);
        if (!file) return false;
        std::vector<std::uint8_t> buf((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        if (buf.empty()) return false;

        if (detail::PEM::looks_like_pem(buf.data(), buf.size())) {
            return load_from_pem(buf.data(), buf.size());
        }

        return load_from_der(buf.data(), buf.size());
    }

auto Certificate::is_expired() const noexcept -> bool {
        if (!loaded()) return true;
        const auto now = static_cast<std::int64_t>(Clock::to_time_t(Clock::now()));
        const auto& info = m_native.info();
        return now < info.not_before || now > info.not_after;
    }

auto Certificate::matches_hostname(const std::string& host) const noexcept -> bool {
        if (!loaded() || host.empty()) return false;
        bool has_dns_san = false;

        for (const auto& san : m_native.info().sans) {
            if (san.type == SubjectAltName::Type::DNS) {
                has_dns_san = true;
                if (wildcard_match(san.value, host)) return true;
            } else if (san.type == SubjectAltName::Type::IP) {
                if (case_insensitive_eq(san.value, host)) return true;
            }
        }
        
        if (!has_dns_san) {
            std::string cn = extract_cn(m_native.info().subject);
            if (!cn.empty() && wildcard_match(cn, host)) return true;
        }

        return false;
    }

auto Certificate::to_string() const -> std::string {
        if (!loaded()) return "Certificate [empty]";
        std::string s = "Certificate [";
        s += "subject=\"" + subject() + "\"";
        s += " issuer=\"" + issuer() + "\"";
        s += " serial=" + serial_number();
        s += is_expired() ? " EXPIRED" : " valid";
        s += is_self_signed() ? " self-signed" : "";
        s += "]";
        return s;
    }

auto Certificate::case_insensitive_eq(const std::string& a, const std::string& b) noexcept -> bool {
        if (a.size() != b.size()) return false;
        for (std::size_t i = 0; i < a.size(); ++i) { if (to_lower(a[i]) != to_lower(b[i])) return false; }
        return true;
    }

auto Certificate::wildcard_match(const std::string& pattern, const std::string& hostname) noexcept -> bool {
        if (pattern.empty() || hostname.empty()) return false;
        std::size_t star = pattern.find('*');
        if (star == std::string::npos) { return case_insensitive_eq(pattern, hostname); }
        std::size_t dot = pattern.find('.');
        if (dot == std::string::npos || star > dot) return false;
        std::string suffix = pattern.substr(dot);
        if (suffix.find('.', 1) == std::string::npos) return false;
        std::string prefix  = pattern.substr(0, star);
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

auto Certificate::extract_cn(const std::string& subject) -> std::string {
        std::size_t pos = 0;

        while (pos < subject.size()) {
            while (pos < subject.size() && subject[pos] == ' ') ++pos;

            if (pos + 3 <= subject.size() &&
                (subject[pos] == 'C' || subject[pos] == 'c') &&
                (subject[pos + 1] == 'N' || subject[pos + 1] == 'n') &&
                subject[pos + 2] == '=') {
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

        if (subject.find('=') == std::string::npos) return subject;
        return {};
    }

} // namespace security
} // namespace networking
} // namespace fizmo
