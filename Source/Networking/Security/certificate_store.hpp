#ifndef FIZMO_CERTIFICATE_STORE_HPP
#define FIZMO_CERTIFICATE_STORE_HPP

#include "certificate.hpp"
#include "certificate_chain.hpp"

#include <memory>
#include <string>
#include <vector>

namespace fizmo {
namespace networking {
namespace security {

class CertificateStore {
public:
    enum class SystemStore {
        Root,
        CA,
        My
    };

    CertificateStore() = default;
    CertificateStore(CertificateStore&&) = default;
    CertificateStore& operator=(CertificateStore&&) = default;
    CertificateStore(const CertificateStore&) = delete;
    CertificateStore& operator=(const CertificateStore&) = delete;

    static CertificateStore open_system(SystemStore which = SystemStore::Root);
    static CertificateStore from_pem_file(const std::string& path);
    static CertificateStore from_pem_string(const std::string& pem);
    static CertificateStore from_directory(const std::string& path);

    std::size_t load_pem_file(const std::string& path);
    std::size_t load_pem_string(const std::string& pem);
    std::size_t load_directory(const std::string& path);
    bool add(Certificate&& cert);
    bool add_der(const std::vector<std::uint8_t>& der);

    std::size_t size() const noexcept { return m_certs.size(); }
    bool empty() const noexcept { return m_certs.empty(); }
    const Certificate& operator[](std::size_t i) const noexcept { return *m_certs[i]; }

    std::vector<const Certificate*> find_by_subject(const std::string& text) const;
    std::vector<const Certificate*> find_by_issuer(const std::string& text) const;
    const Certificate* find_by_thumbprint(const std::string& hex_sha1_or_sha256) const;
    const Certificate* find_by_serial(const std::string& serial) const;
    std::vector<const Certificate*> find_for_host(const std::string& host) const;
    const Certificate* find_issuer_of(const Certificate& cert) const;

    std::vector<const Certificate*> all() const;
    std::size_t remove_expired();

    static std::string thumbprint_sha1(const Certificate& cert);
    static std::string thumbprint_sha256(const Certificate& cert);

    bool write_pem_bundle(const std::string& path) const;

private:
    std::vector<std::unique_ptr<Certificate>> m_certs;
};

} // namespace security
} // namespace networking
} // namespace fizmo

#endif // FIZMO_CERTIFICATE_STORE_HPP
