#ifndef FIZMO_PACKET_HPP
#define FIZMO_PACKET_HPP

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <string>
#include <type_traits>
#include <vector>

namespace fizmo {
namespace networking {
namespace packet {

enum class ByteOrder : std::uint8_t {
    Big,
    Little
};

class PacketWriter {
public:
    explicit PacketWriter(ByteOrder order = ByteOrder::Big, std::size_t reserve = 256) : m_order(order) { m_data.reserve(reserve); }

    PacketWriter& write_u8(std::uint8_t v) { m_data.push_back(v); return *this; }
    PacketWriter& write_i8(std::int8_t v) { return write_u8(static_cast<std::uint8_t>(v)); }
    PacketWriter& write_bool(bool v) { return write_u8(v ? 1 : 0); }
    PacketWriter& write_u16(std::uint16_t v) { return write_uint(v, 2); }
    PacketWriter& write_i16(std::int16_t v) { return write_uint(static_cast<std::uint16_t>(v), 2); }
    PacketWriter& write_u32(std::uint32_t v) { return write_uint(v, 4); }
    PacketWriter& write_i32(std::int32_t v) { return write_uint(static_cast<std::uint32_t>(v), 4); }
    PacketWriter& write_u64(std::uint64_t v) { return write_uint(v, 8); }
    PacketWriter& write_i64(std::int64_t v) { return write_uint(static_cast<std::uint64_t>(v), 8); }
    PacketWriter& write_f32(float v);
    PacketWriter& write_f64(double v);
    PacketWriter& write_varint(std::uint64_t v);
    PacketWriter& write_varint_signed(std::int64_t v);
    PacketWriter& write_string(const std::string& s);
    PacketWriter& write_bytes(const void* data, std::size_t len);
    PacketWriter& write_raw(const void* data, std::size_t len);
    PacketWriter& write_blob(const std::vector<std::uint8_t>& v) { return write_bytes(v.data(), v.size()); }
    PacketWriter& write_quantized(float value, float min, float max, int bits);
    PacketWriter& write_angle(float radians, int bits = 16);
    PacketWriter& write_unit_vector(float x, float y, float z, int bits_per_component = 16);
    PacketWriter& write_zeros(std::size_t n) { m_data.insert(m_data.end(), n, 0); return *this; }

    template <typename T>
    typename std::enable_if<std::is_integral<T>::value && !std::is_same<T, bool>::value, PacketWriter&>::type write(T v) {
        using U = typename std::make_unsigned<T>::type;
        return write_uint(static_cast<std::uint64_t>(static_cast<U>(v)), sizeof(T));
    }

    PacketWriter& write(bool v) { return write_bool(v); }
    PacketWriter& write(float v) { return write_f32(v); }
    PacketWriter& write(double v) { return write_f64(v); }
    PacketWriter& write(const std::string& v) { return write_string(v); }

    PacketWriter& patch_u16(std::size_t offset, std::uint16_t v);
    PacketWriter& patch_u32(std::size_t offset, std::uint32_t v);

    const std::vector<std::uint8_t>& data() const noexcept { return m_data; }
    std::vector<std::uint8_t>& data() noexcept { return m_data; }
    std::vector<std::uint8_t> release() { std::vector<std::uint8_t> out; out.swap(m_data); return out; }
    std::size_t size() const noexcept { return m_data.size(); }
    void clear() noexcept { m_data.clear(); }
    void reserve(std::size_t n) { m_data.reserve(n); }
    ByteOrder order() const noexcept { return m_order; }

private:
    std::vector<std::uint8_t> m_data;
    ByteOrder                 m_order;

    PacketWriter& write_uint(std::uint64_t v, std::size_t bytes);
};

class PacketReader {
public:
    PacketReader(const void* data, std::size_t len, ByteOrder order = ByteOrder::Big) noexcept
        : m_data(static_cast<const std::uint8_t*>(data)), m_size(len), m_order(order) {}
    explicit PacketReader(const std::vector<std::uint8_t>& v, ByteOrder order = ByteOrder::Big) noexcept : PacketReader(v.data(), v.size(), order) {}

    std::uint8_t  read_u8() noexcept;
    std::int8_t   read_i8() noexcept { return static_cast<std::int8_t>(read_u8()); }
    bool          read_bool() noexcept { return read_u8() != 0; }
    std::uint16_t read_u16() noexcept { return static_cast<std::uint16_t>(read_uint(2)); }
    std::int16_t  read_i16() noexcept { return static_cast<std::int16_t>(read_uint(2)); }
    std::uint32_t read_u32() noexcept { return static_cast<std::uint32_t>(read_uint(4)); }
    std::int32_t  read_i32() noexcept { return static_cast<std::int32_t>(read_uint(4)); }
    std::uint64_t read_u64() noexcept { return read_uint(8); }
    std::int64_t  read_i64() noexcept { return static_cast<std::int64_t>(read_uint(8)); }
    float         read_f32() noexcept;
    double        read_f64() noexcept;
    std::uint64_t read_varint() noexcept;
    std::int64_t  read_varint_signed() noexcept;
    std::string   read_string(std::size_t max_len = 1u << 20);
    std::vector<std::uint8_t> read_bytes(std::size_t max_len = 1u << 24);
    bool          read_raw(void* out, std::size_t len) noexcept;
    float         read_quantized(float min, float max, int bits) noexcept;
    float         read_angle(int bits = 16) noexcept;
    void          read_unit_vector(float& x, float& y, float& z, int bits_per_component = 16) noexcept;
    bool          skip(std::size_t n) noexcept;

    template <typename T>
    typename std::enable_if<std::is_integral<T>::value && !std::is_same<T, bool>::value, bool>::type read(T& out) noexcept {
        using U = typename std::make_unsigned<T>::type;
        out = static_cast<T>(static_cast<U>(read_uint(sizeof(T))));
        return ok();
    }

    bool read(bool& out) noexcept { out = read_bool(); return ok(); }
    bool read(float& out) noexcept { out = read_f32(); return ok(); }
    bool read(double& out) noexcept { out = read_f64(); return ok(); }
    bool read(std::string& out) { out = read_string(); return ok(); }

    bool ok() const noexcept { return !m_failed; }
    bool failed() const noexcept { return m_failed; }
    explicit operator bool() const noexcept { return ok(); }
    std::size_t position() const noexcept { return m_pos; }
    std::size_t remaining() const noexcept { return m_pos <= m_size ? m_size - m_pos : 0; }
    std::size_t size() const noexcept { return m_size; }
    bool at_end() const noexcept { return m_pos >= m_size; }
    const std::uint8_t* current() const noexcept { return m_data + m_pos; }

private:
    const std::uint8_t* m_data;
    std::size_t         m_size;
    std::size_t         m_pos = 0;
    ByteOrder           m_order;
    bool                m_failed = false;

    std::uint64_t read_uint(std::size_t bytes) noexcept;
};

class BitWriter {
public:
    BitWriter() = default;

    BitWriter& write_bits(std::uint64_t value, int bits);
    BitWriter& write_bool(bool v) { return write_bits(v ? 1 : 0, 1); }
    BitWriter& write_int(std::int64_t value, std::int64_t min, std::int64_t max);
    BitWriter& write_uint(std::uint64_t value, std::uint64_t max);
    BitWriter& write_float(float v);
    BitWriter& write_quantized(float value, float min, float max, int bits);
    BitWriter& write_varint(std::uint64_t v);
    BitWriter& write_bytes(const void* data, std::size_t len);
    BitWriter& align();

    std::size_t bits_written() const noexcept { return m_bits; }
    std::size_t bytes() const noexcept { return (m_bits + 7) / 8; }
    std::vector<std::uint8_t> finish();
    const std::vector<std::uint8_t>& data() const noexcept { return m_data; }
    void clear() noexcept { m_data.clear(); m_bits = 0; }

    static int bits_required(std::uint64_t max) noexcept;

private:
    std::vector<std::uint8_t> m_data;
    std::size_t               m_bits = 0;
};

class BitReader {
public:
    BitReader(const void* data, std::size_t len) noexcept : m_data(static_cast<const std::uint8_t*>(data)), m_total(len * 8) {}
    explicit BitReader(const std::vector<std::uint8_t>& v) noexcept : BitReader(v.data(), v.size()) {}

    std::uint64_t read_bits(int bits) noexcept;
    bool          read_bool() noexcept { return read_bits(1) != 0; }
    std::int64_t  read_int(std::int64_t min, std::int64_t max) noexcept;
    std::uint64_t read_uint(std::uint64_t max) noexcept;
    float         read_float() noexcept;
    float         read_quantized(float min, float max, int bits) noexcept;
    std::uint64_t read_varint() noexcept;
    bool          read_bytes(void* out, std::size_t len) noexcept;
    void          align() noexcept;

    bool ok() const noexcept { return !m_failed; }
    std::size_t bits_read() const noexcept { return m_pos; }
    std::size_t bits_remaining() const noexcept { return m_pos <= m_total ? m_total - m_pos : 0; }

private:
    const std::uint8_t* m_data;
    std::size_t         m_total;
    std::size_t         m_pos = 0;
    bool                m_failed = false;
};

struct PacketHeader {
    std::uint16_t sequence = 0;
    std::uint16_t ack      = 0;
    std::uint32_t ack_bits = 0;
    std::uint8_t  flags    = 0;

    static constexpr std::size_t size = 9;

    void write(PacketWriter& w) const;
    bool read(PacketReader& r);
};

std::uint32_t packet_checksum(std::uint64_t protocol_id, const std::uint8_t* data, std::size_t len) noexcept;

} // namespace packet
} // namespace networking
} // namespace fizmo

#endif // FIZMO_PACKET_HPP
