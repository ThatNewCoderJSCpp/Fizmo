#include "fizmo_library.hpp"
#include "certificate_store.hpp"
#include "Backend/native_backend.hpp"
#include "../../Crypto/hash.hpp"
#include "../../Crypto/encoding.hpp"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iterator>

namespace fizmo {
namespace networking {
namespace security {

namespace {

std::string lower(std::string s) {
    for (auto& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

std::string strip_hex(const std::string& s) {
    std::string out;
    for (char c : s) if (std::isxdigit(static_cast<unsigned char>(c))) out += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return out;
}

} // namespace

CertificateStore CertificateStore::open_system(SystemStore which) {
    CertificateStore store;
    std::vector<std::vector<std::uint8_t>> ders;
    const char* name = which == SystemStore::Root ? "ROOT" : (which == SystemStore::CA ? "CA" : "MY");
    detail::NativeCertOps::load_system_roots(ders, name);
    for (const auto& d : ders) store.add_der(d);
    return store;
}

CertificateStore CertificateStore::from_pem_file(const std::string& path) {
    CertificateStore s;
    s.load_pem_file(path);
    return s;
}

CertificateStore CertificateStore::from_pem_string(const std::string& pem) {
    CertificateStore s;
    s.load_pem_string(pem);
    return s;
}

CertificateStore CertificateStore::from_directory(const std::string& path) {
    CertificateStore s;
    s.load_directory(path);
    return s;
}

std::size_t CertificateStore::load_pem_string(const std::string& pem) {
    std::size_t n = 0;

    for (const auto& block : detail::PEM::decode_all(pem)) {
        if (block.label != "CERTIFICATE" && block.label != "TRUSTED CERTIFICATE") continue;
        if (add_der(block.der)) ++n;
    }

    return n;
}

std::size_t CertificateStore::load_pem_file(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return 0;
    std::string data((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());

    if (data.find("-----BEGIN") == std::string::npos) {
        return add_der(std::vector<std::uint8_t>(data.begin(), data.end())) ? 1 : 0;
    }

    return load_pem_string(data);
}

std::size_t CertificateStore::load_directory(const std::string& path) {
    std::size_t n = 0;
    std::error_code ec;

    for (const auto& entry : std::filesystem::directory_iterator(path, ec)) {
        if (!entry.is_regular_file(ec)) continue;
        const std::string ext = lower(entry.path().extension().string());
        if (ext == ".pem" || ext == ".crt" || ext == ".cer" || ext == ".der" || ext == ".0" || ext.empty()) n += load_pem_file(entry.path().string());
    }

    return n;
}

bool CertificateStore::add(Certificate&& cert) {
    if (!cert.loaded()) return false;

    for (const auto& c : m_certs) {
        if (c->raw_der() == cert.raw_der()) return false;
    }

    m_certs.push_back(std::unique_ptr<Certificate>(new Certificate(std::move(cert))));
    return true;
}

bool CertificateStore::add_der(const std::vector<std::uint8_t>& der) {
    Certificate c;
    if (!c.load_from_der(der)) return false;
    return add(std::move(c));
}

std::vector<const Certificate*> CertificateStore::find_by_subject(const std::string& text) const {
    std::vector<const Certificate*> out;
    const std::string needle = lower(text);
    for (const auto& c : m_certs) if (lower(c->subject()).find(needle) != std::string::npos) out.push_back(c.get());
    return out;
}

std::vector<const Certificate*> CertificateStore::find_by_issuer(const std::string& text) const {
    std::vector<const Certificate*> out;
    const std::string needle = lower(text);
    for (const auto& c : m_certs) if (lower(c->issuer()).find(needle) != std::string::npos) out.push_back(c.get());
    return out;
}

const Certificate* CertificateStore::find_by_thumbprint(const std::string& hex) const {
    const std::string want = strip_hex(hex);

    for (const auto& c : m_certs) {
        if (want.size() == 40 && thumbprint_sha1(*c) == want) return c.get();
        if (want.size() == 64 && thumbprint_sha256(*c) == want) return c.get();
    }

    return nullptr;
}

const Certificate* CertificateStore::find_by_serial(const std::string& serial) const {
    const std::string want = strip_hex(serial);
    for (const auto& c : m_certs) if (strip_hex(c->serial_number()) == want) return c.get();
    return nullptr;
}

std::vector<const Certificate*> CertificateStore::find_for_host(const std::string& host) const {
    std::vector<const Certificate*> out;
    for (const auto& c : m_certs) if (c->matches_hostname(host)) out.push_back(c.get());
    return out;
}

const Certificate* CertificateStore::find_issuer_of(const Certificate& cert) const {
    for (const auto& c : m_certs) {
        if (c->subject() == cert.issuer() && detail::NativeCertOps::issuer_links_to_subject(cert.native(), c->native())) return c.get();
    }

    return nullptr;
}

std::vector<const Certificate*> CertificateStore::all() const {
    std::vector<const Certificate*> out;
    for (const auto& c : m_certs) out.push_back(c.get());
    return out;
}

std::size_t CertificateStore::remove_expired() {
    const std::size_t before = m_certs.size();
    m_certs.erase(std::remove_if(m_certs.begin(), m_certs.end(), [](const std::unique_ptr<Certificate>& c) { return c->is_expired(); }), m_certs.end());
    return before - m_certs.size();
}

std::string CertificateStore::thumbprint_sha1(const Certificate& cert) {
    return crypto::to_hex(crypto::SHA1::hash(cert.raw_der()));
}

std::string CertificateStore::thumbprint_sha256(const Certificate& cert) {
    return crypto::to_hex(crypto::SHA256::hash(cert.raw_der()));
}

bool CertificateStore::write_pem_bundle(const std::string& path) const {
    std::ofstream f(path, std::ios::binary | std::ios::trunc);
    if (!f) return false;
    for (const auto& c : m_certs) f << detail::PEM::encode("CERTIFICATE", c->raw_der().data(), c->raw_der().size());
    return static_cast<bool>(f);
}

} // namespace security
} // namespace networking
} // namespace fizmo
