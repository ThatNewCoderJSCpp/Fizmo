#include "fizmo_library.hpp"
#include "smbios.hpp"
#include "platform_windows.hpp"

namespace fizmo {
namespace system {
namespace detail {

const char* memory_type_name(std::uint8_t t) noexcept {
    switch (t) {
        case 0x0F: return "SDRAM";
        case 0x12: return "DDR";
        case 0x13: return "DDR2";
        case 0x18: return "DDR3";
        case 0x1A: return "DDR4";
        case 0x1B: return "LPDDR";
        case 0x1C: return "LPDDR2";
        case 0x1D: return "LPDDR3";
        case 0x1E: return "LPDDR4";
        case 0x1F: return "Logical non-volatile";
        case 0x20: return "HBM";
        case 0x21: return "HBM2";
        case 0x22: return "DDR5";
        case 0x23: return "LPDDR5";
        case 0x24: return "HBM3";
        default:   return "";
    }
}

std::vector<MemoryModule> parse_smbios_memory(const std::uint8_t* data, std::size_t size) {
    std::vector<MemoryModule> out;
    std::size_t off = 0;

    while (off + 4 <= size) {
        const std::uint8_t type = data[off];
        const std::uint8_t len  = data[off + 1];
        if (len < 4 || off + len > size) break;
        const std::uint8_t* rec = data + off;
        std::vector<std::string> strings;
        std::size_t s = off + len;

        while (s < size) {
            const std::size_t start = s;
            while (s < size && data[s] != 0) ++s;
            if (s == start) { ++s; break; }
            strings.emplace_back(reinterpret_cast<const char*>(data + start), s - start);
            ++s;
        }

        if (s < size && s == off + len + 1 && data[s] == 0) ++s;

        auto str = [&](std::size_t at) -> std::string {
            if (at >= len) return {};
            const std::uint8_t idx = rec[at];
            if (idx == 0 || idx > strings.size()) return {};
            return trim(strings[idx - 1]);
        };
        auto word = [&](std::size_t at) -> std::uint32_t { return at + 1 < len ? static_cast<std::uint32_t>(rec[at] | (rec[at + 1] << 8)) : 0u; };
        auto dword = [&](std::size_t at) -> std::uint32_t {
            return at + 3 < len ? static_cast<std::uint32_t>(rec[at] | (rec[at + 1] << 8) | (rec[at + 2] << 16) | (static_cast<std::uint32_t>(rec[at + 3]) << 24)) : 0u;
        };

        if (type == 127) break;

        if (type == 17 && len >= 0x15) {
            const std::uint32_t raw = word(0x0C);

            if (raw != 0 && raw != 0xFFFF) {
                MemoryModule m;
                if (raw == 0x7FFF && len >= 0x20) m.size_bytes = static_cast<std::uint64_t>(dword(0x1C) & 0x7FFFFFFFu) << 20;
                else if (raw & 0x8000u) m.size_bytes = static_cast<std::uint64_t>(raw & 0x7FFFu) << 10;
                else m.size_bytes = static_cast<std::uint64_t>(raw) << 20;
                m.locator      = str(0x10);
                m.bank         = str(0x11);
                m.type         = memory_type_name(rec[0x12]);
                m.speed_mts    = word(0x15);
                m.manufacturer = str(0x17);
                m.part_number  = str(0x1A);
                if (len >= 0x22) m.configured_speed_mts = word(0x20);
                if (m.size_bytes > 0) out.push_back(m);
            }
        }

        off = s;
    }

    return out;
}

std::vector<MemoryModule> read_memory_modules() {
#if defined(OS_LINUX)
    std::string raw;
    if (!lnx::read_text("/sys/firmware/dmi/tables/DMI", raw, 1u << 20) || raw.empty()) return {};
    return parse_smbios_memory(reinterpret_cast<const std::uint8_t*>(raw.data()), raw.size());
#elif defined(OS_WINDOWS)
    const DWORD sig = 'R' << 24 | 'S' << 16 | 'M' << 8 | 'B';
    const UINT n = GetSystemFirmwareTable(sig, 0, nullptr, 0);
    if (n <= 8) return {};
    std::vector<std::uint8_t> buf(n);
    if (GetSystemFirmwareTable(sig, 0, buf.data(), n) != n) return {};
    std::uint32_t length = 0;
    std::memcpy(&length, buf.data() + 4, sizeof(length));
    const std::size_t avail = std::min<std::size_t>(length, buf.size() - 8);
    return parse_smbios_memory(buf.data() + 8, avail);
#else
    return {};
#endif
}

} // namespace detail
} // namespace system
} // namespace fizmo
