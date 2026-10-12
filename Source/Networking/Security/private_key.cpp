#include "fizmo_library.hpp"
#include "private_key.hpp"
#include "../../Crypto/secure_memory.hpp"

#include <fstream>
#include <iterator>

namespace fizmo {
namespace networking {
namespace security {

bool PrivateKey::load_from_pem(const std::string& pem, const std::string& passphrase) noexcept {
    return m_native.load_pem(pem, passphrase);
}

bool PrivateKey::load_from_der(const std::uint8_t* der, std::size_t len, const std::string& passphrase) noexcept {
    return m_native.load_der(der, len, passphrase);
}

bool PrivateKey::load_from_file(const std::string& path, const std::string& passphrase) noexcept {
    try {
        std::ifstream f(path, std::ios::binary);
        if (!f) return false;
        crypto::SecureVector<char> data((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
        if (data.empty()) return false;

        if (detail::PEM::looks_like_pem(reinterpret_cast<const std::uint8_t*>(data.data()), data.size()) ||
            std::string(data.data(), data.size()).find("-----BEGIN") != std::string::npos) {
            std::string text(data.data(), data.size());
            const bool ok = load_from_pem(text, passphrase);
            crypto::secure_zero(&text[0], text.size());
            return ok;
        }

        return load_from_der(reinterpret_cast<const std::uint8_t*>(data.data()), data.size(), passphrase);
    } catch (...) {
        return false;
    }
}

bool PrivateKey::matches_certificate(const Certificate& cert) const noexcept {
    return loaded() && cert.loaded() && m_native.matches_certificate(cert.native());
}

std::string PrivateKey::to_pem() const {
    std::vector<std::uint8_t> der;
    if (!to_pkcs8_der(der)) return {};
    std::string pem = detail::PEM::encode("PRIVATE KEY", der.data(), der.size());
    crypto::secure_zero(der.data(), der.size());
    return pem;
}

bool PrivateKey::to_pkcs12(const Certificate& leaf, const std::vector<const Certificate*>& chain, const std::string& password, std::vector<std::uint8_t>& out) const noexcept {
    if (!loaded() || !leaf.loaded()) return false;

    try {
        std::vector<const detail::NativeCertificateBase*> extras;
        for (const auto* c : chain) if (c && c->loaded()) extras.push_back(&c->native());
        return m_native.build_pkcs12(leaf.native(), extras, password, out);
    } catch (...) {
        return false;
    }
}

bool PrivateKey::to_pkcs12(const CertificateChain& chain, const std::string& password, std::vector<std::uint8_t>& out) const noexcept {
    if (chain.empty()) return false;

    try {
        return to_pkcs12(chain.leaf(), chain.tail(), password, out);
    } catch (...) {
        return false;
    }
}

std::string PrivateKey::to_string() const {
    if (!loaded()) return "PrivateKey[empty]";
    return std::string("PrivateKey[") + key_type_to_string(key_type()) + " " + std::to_string(key_size_bits()) + " bits]";
}

} // namespace security
} // namespace networking
} // namespace fizmo
