#ifndef FIZMO_CRYPTO_HASH_HPP
#define FIZMO_CRYPTO_HASH_HPP

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace fizmo {
namespace crypto {

class SHA1 {
public:
    static constexpr std::size_t digest_size = 20;
    static constexpr std::size_t block_size  = 64;
    using digest_type = std::array<std::uint8_t, digest_size>;

    SHA1() noexcept { reset(); }

    void reset() noexcept;
    SHA1& update(const void* data, std::size_t len) noexcept;
    SHA1& update(const std::string& s) noexcept { return update(s.data(), s.size()); }
    SHA1& update(const std::vector<std::uint8_t>& v) noexcept { return update(v.data(), v.size()); }
    digest_type finish() noexcept;

    static digest_type hash(const void* data, std::size_t len) noexcept { SHA1 h; h.update(data, len); return h.finish(); }
    static digest_type hash(const std::string& s) noexcept { return hash(s.data(), s.size()); }
    static digest_type hash(const std::vector<std::uint8_t>& v) noexcept { return hash(v.data(), v.size()); }

private:
    std::uint32_t m_state[5];
    std::uint8_t  m_buffer[block_size];
    std::uint64_t m_length;
    std::size_t   m_fill;

    void compress(const std::uint8_t* block) noexcept;
};

class SHA256 {
public:
    static constexpr std::size_t digest_size = 32;
    static constexpr std::size_t block_size  = 64;
    using digest_type = std::array<std::uint8_t, digest_size>;

    SHA256() noexcept { reset(); }

    void reset() noexcept;
    SHA256& update(const void* data, std::size_t len) noexcept;
    SHA256& update(const std::string& s) noexcept { return update(s.data(), s.size()); }
    SHA256& update(const std::vector<std::uint8_t>& v) noexcept { return update(v.data(), v.size()); }
    digest_type finish() noexcept;

    static digest_type hash(const void* data, std::size_t len) noexcept { SHA256 h; h.update(data, len); return h.finish(); }
    static digest_type hash(const std::string& s) noexcept { return hash(s.data(), s.size()); }
    static digest_type hash(const std::vector<std::uint8_t>& v) noexcept { return hash(v.data(), v.size()); }

protected:
    std::uint32_t m_state[8];
    std::uint8_t  m_buffer[block_size];
    std::uint64_t m_length;
    std::size_t   m_fill;

    void compress(const std::uint8_t* block) noexcept;
    void finish_into(std::uint8_t* out, std::size_t words) noexcept;
};

class SHA224 : private SHA256 {
public:
    static constexpr std::size_t digest_size = 28;
    static constexpr std::size_t block_size  = 64;
    using digest_type = std::array<std::uint8_t, digest_size>;

    SHA224() noexcept { reset(); }

    void reset() noexcept;
    SHA224& update(const void* data, std::size_t len) noexcept { SHA256::update(data, len); return *this; }
    SHA224& update(const std::string& s) noexcept { return update(s.data(), s.size()); }
    SHA224& update(const std::vector<std::uint8_t>& v) noexcept { return update(v.data(), v.size()); }
    digest_type finish() noexcept;

    static digest_type hash(const void* data, std::size_t len) noexcept { SHA224 h; h.update(data, len); return h.finish(); }
    static digest_type hash(const std::string& s) noexcept { return hash(s.data(), s.size()); }
    static digest_type hash(const std::vector<std::uint8_t>& v) noexcept { return hash(v.data(), v.size()); }
};

class SHA512 {
public:
    static constexpr std::size_t digest_size = 64;
    static constexpr std::size_t block_size  = 128;
    using digest_type = std::array<std::uint8_t, digest_size>;

    SHA512() noexcept { reset(); }

    void reset() noexcept;
    SHA512& update(const void* data, std::size_t len) noexcept;
    SHA512& update(const std::string& s) noexcept { return update(s.data(), s.size()); }
    SHA512& update(const std::vector<std::uint8_t>& v) noexcept { return update(v.data(), v.size()); }
    digest_type finish() noexcept;

    static digest_type hash(const void* data, std::size_t len) noexcept { SHA512 h; h.update(data, len); return h.finish(); }
    static digest_type hash(const std::string& s) noexcept { return hash(s.data(), s.size()); }
    static digest_type hash(const std::vector<std::uint8_t>& v) noexcept { return hash(v.data(), v.size()); }

protected:
    std::uint64_t m_state[8];
    std::uint8_t  m_buffer[block_size];
    std::uint64_t m_length;
    std::size_t   m_fill;

    void compress(const std::uint8_t* block) noexcept;
    void finish_into(std::uint8_t* out, std::size_t bytes) noexcept;
};

class SHA384 : private SHA512 {
public:
    static constexpr std::size_t digest_size = 48;
    static constexpr std::size_t block_size  = 128;
    using digest_type = std::array<std::uint8_t, digest_size>;

    SHA384() noexcept { reset(); }

    void reset() noexcept;
    SHA384& update(const void* data, std::size_t len) noexcept { SHA512::update(data, len); return *this; }
    SHA384& update(const std::string& s) noexcept { return update(s.data(), s.size()); }
    SHA384& update(const std::vector<std::uint8_t>& v) noexcept { return update(v.data(), v.size()); }
    digest_type finish() noexcept;

    static digest_type hash(const void* data, std::size_t len) noexcept { SHA384 h; h.update(data, len); return h.finish(); }
    static digest_type hash(const std::string& s) noexcept { return hash(s.data(), s.size()); }
    static digest_type hash(const std::vector<std::uint8_t>& v) noexcept { return hash(v.data(), v.size()); }
};

template <typename Hash>
std::vector<std::uint8_t> digest_bytes(const void* data, std::size_t len) {
    const auto d = Hash::hash(data, len);
    return std::vector<std::uint8_t>(d.begin(), d.end());
}

template <typename Hash>
std::vector<std::uint8_t> digest_bytes(const std::string& s) { return digest_bytes<Hash>(s.data(), s.size()); }

std::uint32_t crc32(const void* data, std::size_t len, std::uint32_t seed = 0) noexcept;
std::uint32_t crc32c(const void* data, std::size_t len, std::uint32_t seed = 0) noexcept;
std::uint32_t adler32(const void* data, std::size_t len, std::uint32_t seed = 1) noexcept;
std::uint32_t fnv1a32(const void* data, std::size_t len, std::uint32_t seed = 2166136261u) noexcept;
std::uint64_t fnv1a64(const void* data, std::size_t len, std::uint64_t seed = 14695981039346656037ull) noexcept;

inline std::uint32_t crc32(const std::string& s) noexcept { return crc32(s.data(), s.size()); }
inline std::uint32_t crc32c(const std::string& s) noexcept { return crc32c(s.data(), s.size()); }

} // namespace crypto
} // namespace fizmo

#endif // FIZMO_CRYPTO_HASH_HPP
