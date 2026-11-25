#ifndef FIZMO_CRYPTO_BLAKE2B_HPP
#define FIZMO_CRYPTO_BLAKE2B_HPP

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace fizmo {
namespace crypto {

class BLAKE2b {
public:
    static constexpr std::size_t block_size = 128;
    static constexpr std::size_t max_digest_size = 64;
    static constexpr std::size_t max_key_size = 64;

    explicit BLAKE2b(std::size_t digest_size = max_digest_size, const void* key = nullptr, std::size_t key_size = 0);

    void reset();
    BLAKE2b& update(const void* data, std::size_t len) noexcept;
    BLAKE2b& update(const std::string& s) noexcept { return update(s.data(), s.size()); }
    void finish(void* out) noexcept;
    std::vector<std::uint8_t> finish();
    std::size_t digest_size() const noexcept { return m_digest_size; }

    static std::vector<std::uint8_t> hash(const void* data, std::size_t len, std::size_t digest_size = max_digest_size, const void* key = nullptr, std::size_t key_size = 0);
    static std::vector<std::uint8_t> hash(const std::string& data, std::size_t digest_size = max_digest_size, const std::string& key = std::string());

    ~BLAKE2b();

private:
    std::uint64_t m_h[8];
    std::uint64_t m_t[2];
    std::uint8_t  m_buffer[block_size];
    std::size_t   m_fill;
    std::size_t   m_digest_size;
    std::uint8_t  m_key[max_key_size];
    std::size_t   m_key_size;

    void compress(const std::uint8_t* block, bool last) noexcept;
};

std::vector<std::uint8_t> blake2b_long(const void* data, std::size_t len, std::size_t out_size);

} // namespace crypto
} // namespace fizmo

#endif // FIZMO_CRYPTO_BLAKE2B_HPP
