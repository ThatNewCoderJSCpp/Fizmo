#ifndef FIZMO_SYSTEM_SMC_MACOS_HPP
#define FIZMO_SYSTEM_SMC_MACOS_HPP

#include "iokit_macos.hpp"

#if defined(OS_MACOS)

#include <mach/mach.h>

namespace fizmo {
namespace system {
namespace detail {
namespace mac {

class Smc {
private:
    struct KeyVersion {
        std::uint8_t  major;
        std::uint8_t  minor;
        std::uint8_t  build;
        std::uint8_t  reserved;
        std::uint16_t release;
    };

    struct KeyLimits {
        std::uint16_t version;
        std::uint16_t length;
        std::uint32_t cpu;
        std::uint32_t gpu;
        std::uint32_t mem;
    };

    struct KeyInfo {
        std::uint32_t size;
        std::uint32_t type;
        std::uint8_t  attributes;
    };

    struct Param {
        std::uint32_t key;
        KeyVersion    version;
        KeyLimits     limits;
        KeyInfo       info;
        std::uint8_t  result;
        std::uint8_t  status;
        std::uint8_t  command;
        std::uint32_t data32;
        std::uint8_t  bytes[32];
    };

    static_assert(sizeof(Param) == 80, "SMC parameter block must be 80 bytes");

    static constexpr std::uint32_t kSelector = 2;
    static constexpr std::uint8_t  kReadKey  = 5;
    static constexpr std::uint8_t  kKeyInfo  = 9;

    io_connect_t m_conn = IO_OBJECT_NULL;
    bool         m_tried = false;

    bool call(Param& in, Param& out) const noexcept {
        std::size_t out_size = sizeof(Param);
        std::memset(&out, 0, sizeof(out));
        if (::IOConnectCallStructMethod(m_conn, kSelector, &in, sizeof(Param), &out, &out_size) != kIOReturnSuccess) return false;
        return out.result == 0;
    }

public:
    struct Value {
        std::uint32_t type = 0;
        std::uint32_t size = 0;
        std::uint8_t  bytes[32] = {};
    };

    Smc() noexcept = default;
    ~Smc() { close(); }
    Smc(const Smc&) = delete;
    Smc& operator=(const Smc&) = delete;

    static constexpr std::uint32_t fourcc(const char* k) noexcept {
        return (static_cast<std::uint32_t>(static_cast<unsigned char>(k[0])) << 24) | (static_cast<std::uint32_t>(static_cast<unsigned char>(k[1])) << 16) |
               (static_cast<std::uint32_t>(static_cast<unsigned char>(k[2])) << 8) | static_cast<std::uint32_t>(static_cast<unsigned char>(k[3]));
    }

    bool open() noexcept {
        if (m_conn != IO_OBJECT_NULL) return true;
        if (m_tried) return false;
        m_tried = true;
        CFMutableDictionaryRef match = ::IOServiceMatching("AppleSMC");
        if (!match) return false;
        IORef service(::IOServiceGetMatchingService(MACH_PORT_NULL, match));
        if (!service) return false;
        io_connect_t conn = IO_OBJECT_NULL;
        if (::IOServiceOpen(service.get(), ::mach_task_self(), 0, &conn) != kIOReturnSuccess) return false;
        m_conn = conn;
        return true;
    }

    void close() noexcept {
        if (m_conn != IO_OBJECT_NULL) ::IOServiceClose(m_conn);
        m_conn = IO_OBJECT_NULL;
    }

    bool valid() const noexcept { return m_conn != IO_OBJECT_NULL; }

    bool read(const char* key, Value& v) const noexcept {
        if (m_conn == IO_OBJECT_NULL || !key || std::strlen(key) != 4) return false;
        Param in{}, out{};
        in.key = fourcc(key);
        in.command = kKeyInfo;
        if (!call(in, out)) return false;
        const KeyInfo info = out.info;
        if (info.size == 0 || info.size > sizeof(v.bytes)) return false;
        in.info.size = info.size;
        in.command = kReadKey;
        if (!call(in, out)) return false;
        v.type = info.type;
        v.size = info.size;
        std::memcpy(v.bytes, out.bytes, info.size);
        return true;
    }

    static std::optional<double> decode(const Value& v) noexcept {
        if (v.type == fourcc("sp78") && v.size >= 2) {
            const std::int16_t raw = static_cast<std::int16_t>((static_cast<std::uint16_t>(v.bytes[0]) << 8) | v.bytes[1]);
            return static_cast<double>(raw) / 256.0;
        }
        if (v.type == fourcc("flt ") && v.size >= 4) {
            float f = 0.0f;
            std::memcpy(&f, v.bytes, sizeof(f));
            return static_cast<double>(f);
        }
        if (v.type == fourcc("fpe2") && v.size >= 2) return static_cast<double>((static_cast<std::uint16_t>(v.bytes[0]) << 8) | v.bytes[1]) / 4.0;
        if (v.type == fourcc("ui8 ") && v.size >= 1) return static_cast<double>(v.bytes[0]);
        if (v.type == fourcc("ui16") && v.size >= 2) return static_cast<double>((static_cast<std::uint16_t>(v.bytes[0]) << 8) | v.bytes[1]);
        return std::nullopt;
    }

    std::optional<double> read_number(const char* key) const noexcept {
        Value v;
        if (!read(key, v)) return std::nullopt;
        return decode(v);
    }

    std::optional<double> read_temperature(const char* key) const noexcept {
        const auto t = read_number(key);
        if (!t || !(*t > 0.0 && *t < 150.0)) return std::nullopt;
        return t;
    }
};

} // namespace mac
} // namespace detail
} // namespace system
} // namespace fizmo

#endif // OS_MACOS
#endif // FIZMO_SYSTEM_SMC_MACOS_HPP
