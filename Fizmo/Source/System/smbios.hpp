#ifndef FIZMO_SYSTEM_SMBIOS_HPP
#define FIZMO_SYSTEM_SMBIOS_HPP

#include "common.hpp"
#include "platform_linux.hpp"
#include "platform_windows.hpp"

namespace fizmo {
namespace system {

struct MemoryModule {
    std::string   locator;
    std::string   bank;
    std::string   type;
    std::string   manufacturer;
    std::string   part_number;
    std::uint64_t size_bytes           = 0;
    std::uint32_t speed_mts            = 0;
    std::uint32_t configured_speed_mts = 0;
};

namespace detail {

 const char* memory_type_name(std::uint8_t t) noexcept;

 std::vector<MemoryModule> parse_smbios_memory(const std::uint8_t* data, std::size_t size);

 std::vector<MemoryModule> read_memory_modules();

} // namespace detail
} // namespace system
} // namespace fizmo

#endif // FIZMO_SYSTEM_SMBIOS_HPP
