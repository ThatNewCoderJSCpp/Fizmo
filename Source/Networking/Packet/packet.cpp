#include "fizmo_library.hpp"
#include "packet.hpp"
#include "../../Crypto/hash.hpp"

#include <algorithm>
#include <cmath>

namespace fizmo {
namespace networking {
namespace packet {

namespace {

constexpr double kTwoPi = 6.283185307179586476925286766559;

std::uint64_t quantize(float value, float min, float max, int bits) noexcept {
    if (bits <= 0) return 0;
    if (bits > 32) bits = 32;
    const std::uint64_t steps = (std::uint64_t(1) << bits) - 1;
    if (!(max > min)) return 0;
    double t = (static_cast<double>(value) - min) / (static_cast<double>(max) - min);
    if (!(t >= 0.0)) t = 0.0;
    if (t > 1.0) t = 1.0;
    return static_cast<std::uint64_t>(std::llround(t * static_cast<double>(steps)));
}

float dequantize(std::uint64_t q, float min, float max, int bits) noexcept {
    if (bits <= 0) return min;
    if (bits > 32) bits = 32;
    const std::uint64_t steps = (std::uint64_t(1) << bits) - 1;
    if (q > steps) q = steps;
    return static_cast<float>(static_cast<double>(min) + (static_cast<double>(max) - min) * (static_cast<double>(q) / static_cast<double>(steps)));
}

std::size_t quant_bytes(int bits) noexcept {
    if (bits <= 8) return 1;
    if (bits <= 16) return 2;
    return 4;
}

} // namespace

PacketWriter& PacketWriter::write_uint(std::uint64_t v, std::size_t bytes) {
    if (m_order == ByteOrder::Big) {
        for (std::size_t i = bytes; i-- > 0;) m_data.push_back(static_cast<std::uint8_t>(v >> (8 * i)));
    } else {
        for (std::size_t i = 0; i < bytes; ++i) m_data.push_back(static_cast<std::uint8_t>(v >> (8 * i)));
    }

    return *this;
}

PacketWriter& PacketWriter::write_f32(float v) {
    std::uint32_t bits;
    std::memcpy(&bits, &v, 4);
    return write_uint(bits, 4);
}

PacketWriter& PacketWriter::write_f64(double v) {
    std::uint64_t bits;
    std::memcpy(&bits, &v, 8);
    return write_uint(bits, 8);
}

PacketWriter& PacketWriter::write_varint(std::uint64_t v) {
    while (v >= 0x80) {
        m_data.push_back(static_cast<std::uint8_t>((v & 0x7F) | 0x80));
        v >>= 7;
    }

    m_data.push_back(static_cast<std::uint8_t>(v));
    return *this;
}

PacketWriter& PacketWriter::write_varint_signed(std::int64_t v) {
    const std::uint64_t zz = (static_cast<std::uint64_t>(v) << 1) ^ static_cast<std::uint64_t>(v >> 63);
    return write_varint(zz);
}

PacketWriter& PacketWriter::write_string(const std::string& s) {
    write_varint(s.size());
    m_data.insert(m_data.end(), s.begin(), s.end());
    return *this;
}

PacketWriter& PacketWriter::write_bytes(const void* data, std::size_t len) {
    write_varint(len);
    return write_raw(data, len);
}

PacketWriter& PacketWriter::write_raw(const void* data, std::size_t len) {
    const auto* p = static_cast<const std::uint8_t*>(data);
    if (len) m_data.insert(m_data.end(), p, p + len);
    return *this;
}

PacketWriter& PacketWriter::write_quantized(float value, float min, float max, int bits) {
    return write_uint(quantize(value, min, max, bits), quant_bytes(bits));
}

PacketWriter& PacketWriter::write_angle(float radians, int bits) {
    double a = std::fmod(static_cast<double>(radians), kTwoPi);
    if (a < 0) a += kTwoPi;
    const std::uint64_t steps = std::uint64_t(1) << (bits > 32 ? 32 : bits);
    std::uint64_t q = static_cast<std::uint64_t>(std::llround(a / kTwoPi * static_cast<double>(steps)));
    if (q >= steps) q = 0;
    return write_uint(q, quant_bytes(bits));
}

PacketWriter& PacketWriter::write_unit_vector(float x, float y, float z, int bits) {
    write_quantized(x, -1.0f, 1.0f, bits);
    write_quantized(y, -1.0f, 1.0f, bits);
    return write_quantized(z, -1.0f, 1.0f, bits);
}

PacketWriter& PacketWriter::patch_u16(std::size_t offset, std::uint16_t v) {
    if (offset + 2 > m_data.size()) return *this;
    if (m_order == ByteOrder::Big) { m_data[offset] = static_cast<std::uint8_t>(v >> 8); m_data[offset + 1] = static_cast<std::uint8_t>(v); }
    else { m_data[offset] = static_cast<std::uint8_t>(v); m_data[offset + 1] = static_cast<std::uint8_t>(v >> 8); }
    return *this;
}

PacketWriter& PacketWriter::patch_u32(std::size_t offset, std::uint32_t v) {
    if (offset + 4 > m_data.size()) return *this;

    for (int i = 0; i < 4; ++i) {
        const int shift = m_order == ByteOrder::Big ? 8 * (3 - i) : 8 * i;
        m_data[offset + static_cast<std::size_t>(i)] = static_cast<std::uint8_t>(v >> shift);
    }

    return *this;
}

std::uint64_t PacketReader::read_uint(std::size_t bytes) noexcept {
    if (m_failed || m_pos + bytes > m_size) { m_failed = true; return 0; }
    std::uint64_t v = 0;

    if (m_order == ByteOrder::Big) {
        for (std::size_t i = 0; i < bytes; ++i) v = (v << 8) | m_data[m_pos + i];
    } else {
        for (std::size_t i = bytes; i-- > 0;) v = (v << 8) | m_data[m_pos + i];
    }

    m_pos += bytes;
    return v;
}

std::uint8_t PacketReader::read_u8() noexcept {
    return static_cast<std::uint8_t>(read_uint(1));
}

float PacketReader::read_f32() noexcept {
    const std::uint32_t bits = static_cast<std::uint32_t>(read_uint(4));
    float v;
    std::memcpy(&v, &bits, 4);
    return v;
}

double PacketReader::read_f64() noexcept {
    const std::uint64_t bits = read_uint(8);
    double v;
    std::memcpy(&v, &bits, 8);
    return v;
}

std::uint64_t PacketReader::read_varint() noexcept {
    std::uint64_t v = 0;

    for (int shift = 0; shift < 64; shift += 7) {
        if (m_failed || m_pos >= m_size) { m_failed = true; return 0; }
        const std::uint8_t b = m_data[m_pos++];
        v |= static_cast<std::uint64_t>(b & 0x7F) << shift;
        if ((b & 0x80) == 0) return v;
    }

    m_failed = true;
    return 0;
}

std::int64_t PacketReader::read_varint_signed() noexcept {
    const std::uint64_t zz = read_varint();
    return static_cast<std::int64_t>((zz >> 1) ^ (~(zz & 1) + 1));
}

std::string PacketReader::read_string(std::size_t max_len) {
    const std::uint64_t len = read_varint();
    if (m_failed || len > max_len || len > remaining()) { m_failed = true; return {}; }
    std::string s(reinterpret_cast<const char*>(m_data + m_pos), static_cast<std::size_t>(len));
    m_pos += static_cast<std::size_t>(len);
    return s;
}

std::vector<std::uint8_t> PacketReader::read_bytes(std::size_t max_len) {
    const std::uint64_t len = read_varint();
    if (m_failed || len > max_len || len > remaining()) { m_failed = true; return {}; }
    std::vector<std::uint8_t> v(m_data + m_pos, m_data + m_pos + len);
    m_pos += static_cast<std::size_t>(len);
    return v;
}

bool PacketReader::read_raw(void* out, std::size_t len) noexcept {
    if (m_failed || len > remaining()) { m_failed = true; return false; }
    if (len) std::memcpy(out, m_data + m_pos, len);
    m_pos += len;
    return true;
}

float PacketReader::read_quantized(float min, float max, int bits) noexcept {
    return dequantize(read_uint(quant_bytes(bits)), min, max, bits);
}

float PacketReader::read_angle(int bits) noexcept {
    const std::uint64_t steps = std::uint64_t(1) << (bits > 32 ? 32 : bits);
    const std::uint64_t q = read_uint(quant_bytes(bits));
    return static_cast<float>(static_cast<double>(q % steps) / static_cast<double>(steps) * kTwoPi);
}

void PacketReader::read_unit_vector(float& x, float& y, float& z, int bits) noexcept {
    x = read_quantized(-1.0f, 1.0f, bits);
    y = read_quantized(-1.0f, 1.0f, bits);
    z = read_quantized(-1.0f, 1.0f, bits);
}

bool PacketReader::skip(std::size_t n) noexcept {
    if (m_failed || n > remaining()) { m_failed = true; return false; }
    m_pos += n;
    return true;
}

int BitWriter::bits_required(std::uint64_t max) noexcept {
    int bits = 0;
    while (max > 0) { ++bits; max >>= 1; }
    return bits;
}

BitWriter& BitWriter::write_bits(std::uint64_t value, int bits) {
    if (bits <= 0) return *this;
    if (bits < 64) value &= (std::uint64_t(1) << bits) - 1;

    for (int i = 0; i < bits; ++i) {
        const std::size_t byte = m_bits / 8;
        if (byte >= m_data.size()) m_data.push_back(0);
        if ((value >> i) & 1u) m_data[byte] = static_cast<std::uint8_t>(m_data[byte] | (1u << (m_bits % 8)));
        ++m_bits;
    }

    return *this;
}

BitWriter& BitWriter::write_int(std::int64_t value, std::int64_t min, std::int64_t max) {
    if (value < min) value = min;
    if (value > max) value = max;
    const std::uint64_t range = static_cast<std::uint64_t>(max) - static_cast<std::uint64_t>(min);
    return write_bits(static_cast<std::uint64_t>(value) - static_cast<std::uint64_t>(min), bits_required(range));
}

BitWriter& BitWriter::write_uint(std::uint64_t value, std::uint64_t max) {
    if (value > max) value = max;
    return write_bits(value, bits_required(max));
}

BitWriter& BitWriter::write_float(float v) {
    std::uint32_t bits;
    std::memcpy(&bits, &v, 4);
    return write_bits(bits, 32);
}

BitWriter& BitWriter::write_quantized(float value, float min, float max, int bits) {
    return write_bits(quantize(value, min, max, bits), bits);
}

BitWriter& BitWriter::write_varint(std::uint64_t v) {
    while (v >= 0x80) {
        write_bits((v & 0x7F) | 0x80, 8);
        v >>= 7;
    }

    return write_bits(v, 8);
}

BitWriter& BitWriter::write_bytes(const void* data, std::size_t len) {
    const auto* p = static_cast<const std::uint8_t*>(data);
    for (std::size_t i = 0; i < len; ++i) write_bits(p[i], 8);
    return *this;
}

BitWriter& BitWriter::align() {
    const std::size_t pad = (8 - m_bits % 8) % 8;
    return write_bits(0, static_cast<int>(pad));
}

std::vector<std::uint8_t> BitWriter::finish() {
    align();
    return m_data;
}

std::uint64_t BitReader::read_bits(int bits) noexcept {
    if (bits <= 0) return 0;
    if (m_failed || m_pos + static_cast<std::size_t>(bits) > m_total) { m_failed = true; return 0; }
    std::uint64_t v = 0;

    for (int i = 0; i < bits; ++i) {
        const std::uint8_t byte = m_data[m_pos / 8];
        if ((byte >> (m_pos % 8)) & 1u) v |= std::uint64_t(1) << i;
        ++m_pos;
    }

    return v;
}

std::int64_t BitReader::read_int(std::int64_t min, std::int64_t max) noexcept {
    const std::uint64_t range = static_cast<std::uint64_t>(max) - static_cast<std::uint64_t>(min);
    const std::uint64_t v = read_bits(BitWriter::bits_required(range));
    if (v > range) { m_failed = true; return min; }
    return static_cast<std::int64_t>(static_cast<std::uint64_t>(min) + v);
}

std::uint64_t BitReader::read_uint(std::uint64_t max) noexcept {
    const std::uint64_t v = read_bits(BitWriter::bits_required(max));
    if (v > max) { m_failed = true; return max; }
    return v;
}

float BitReader::read_float() noexcept {
    const std::uint32_t bits = static_cast<std::uint32_t>(read_bits(32));
    float v;
    std::memcpy(&v, &bits, 4);
    return v;
}

float BitReader::read_quantized(float min, float max, int bits) noexcept {
    return dequantize(read_bits(bits), min, max, bits);
}

std::uint64_t BitReader::read_varint() noexcept {
    std::uint64_t v = 0;

    for (int shift = 0; shift < 64; shift += 7) {
        const std::uint64_t b = read_bits(8);
        if (m_failed) return 0;
        v |= (b & 0x7F) << shift;
        if ((b & 0x80) == 0) return v;
    }

    m_failed = true;
    return 0;
}

bool BitReader::read_bytes(void* out, std::size_t len) noexcept {
    auto* p = static_cast<std::uint8_t*>(out);
    for (std::size_t i = 0; i < len; ++i) p[i] = static_cast<std::uint8_t>(read_bits(8));
    return !m_failed;
}

void BitReader::align() noexcept {
    const std::size_t pad = (8 - m_pos % 8) % 8;
    if (m_pos + pad > m_total) { m_failed = true; return; }
    m_pos += pad;
}

void PacketHeader::write(PacketWriter& w) const {
    w.write_u16(sequence).write_u16(ack).write_u32(ack_bits).write_u8(flags);
}

bool PacketHeader::read(PacketReader& r) {
    sequence = r.read_u16();
    ack = r.read_u16();
    ack_bits = r.read_u32();
    flags = r.read_u8();
    return r.ok();
}

std::uint32_t packet_checksum(std::uint64_t protocol_id, const std::uint8_t* data, std::size_t len) noexcept {
    std::uint8_t id[8];
    for (int i = 0; i < 8; ++i) id[i] = static_cast<std::uint8_t>(protocol_id >> (8 * i));
    const std::uint32_t seed = crypto::crc32c(id, sizeof(id));
    return crypto::crc32c(data, len, seed);
}

} // namespace packet
} // namespace networking
} // namespace fizmo
