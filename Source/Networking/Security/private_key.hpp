#ifndef FIZMO_PRIVATE_KEY_HPP
#define FIZMO_PRIVATE_KEY_HPP

#include "certificate.hpp"
#include "certificate_chain.hpp"
#include "Backend/native_backend.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace fizmo {
namespace networking {
namespace security {

class PrivateKey {
public:
    PrivateKey() noexcept = default;
    ~PrivateKey() noexcept = default;

    PrivateKey(const PrivateKey&) = delete;
    PrivateKey& operator=(const PrivateKey&) = delete;
    PrivateKey(PrivateKey&&) noexcept = default;
    PrivateKey& operator=(PrivateKey&&) noexcept = default;

    bool load_from_pem(const std::string& pem, const std::string& passphrase = {}) noexcept;
    bool load_from_der(const std::uint8_t* der, std::size_t len, const std::string& passphrase = {}) noexcept;
    bool load_from_der(const std::vector<std::uint8_t>& der, const std::string& passphrase = {}) noexcept {
        return load_from_der(der.data(), der.size(), passphrase);
    }
    bool load_from_file(const std::string& path, const std::string& passphrase = {}) noexcept;

    void release() noexcept { m_native.release(); }
    bool loaded() const noexcept { return m_native.valid(); }
    explicit operator bool() const noexcept { return loaded(); }

    KeyType key_type() const noexcept { return m_native.key_type(); }
    int     key_size_bits() const noexcept { return m_native.key_bits(); }
    bool    matches_certificate(const Certificate& cert) const noexcept;

    bool to_pkcs8_der(std::vector<std::uint8_t>& out) const noexcept { return m_native.to_pkcs8_der(out); }
    std::string to_pem() const;
    bool to_pkcs12(const Certificate& leaf, const std::vector<const Certificate*>& chain, const std::string& password, std::vector<std::uint8_t>& out) const noexcept;
    bool to_pkcs12(const CertificateChain& chain, const std::string& password, std::vector<std::uint8_t>& out) const noexcept;

    const std::string& last_error() const noexcept { return m_native.last_error(); }
    void* native_handle() const noexcept { return m_native.native(); }
    const detail::NativePrivateKey& native() const noexcept { return m_native; }

    std::string to_string() const;

private:
    detail::NativePrivateKey m_native;
};

} // namespace security
} // namespace networking
} // namespace fizmo

#endif // FIZMO_PRIVATE_KEY_HPP
