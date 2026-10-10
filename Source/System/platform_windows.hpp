#ifndef FIZMO_SYSTEM_PLATFORM_WINDOWS_HPP
#define FIZMO_SYSTEM_PLATFORM_WINDOWS_HPP

#include "common.hpp"
#include "win_library.hpp"

#if defined(OS_WINDOWS)

#include <pdh.h>
#include <pdhmsg.h>
#include <psapi.h>
#include <tlhelp32.h>
#include <winioctl.h>
#include <dxgi1_4.h>

namespace fizmo {
namespace system {
namespace detail {
namespace win {

std::string narrow(const wchar_t* s, int len = -1);

std::string narrow(const std::wstring& s);

std::wstring widen(const std::string& s);

std::uint64_t filetime_u64(const FILETIME& f) noexcept;


std::string registry_string(HKEY root, const wchar_t* key, const wchar_t* value);

std::optional<std::uint32_t> registry_dword(HKEY root, const wchar_t* key, const wchar_t* value);

class Pdh {
public:
    using Counter = PDH_HCOUNTER;

    struct Item {
        std::string name;
        double      value = 0.0;
    };

private:
    using OpenQueryFn     = LONG (WINAPI*)(LPCWSTR, DWORD_PTR, PDH_HQUERY*);
    using AddCounterFn    = LONG (WINAPI*)(PDH_HQUERY, LPCWSTR, DWORD_PTR, PDH_HCOUNTER*);
    using CollectFn       = LONG (WINAPI*)(PDH_HQUERY);
    using ValueFn         = LONG (WINAPI*)(PDH_HCOUNTER, DWORD, LPDWORD, PPDH_FMT_COUNTERVALUE);
    using ArrayFn         = LONG (WINAPI*)(PDH_HCOUNTER, DWORD, LPDWORD, LPDWORD, PPDH_FMT_COUNTERVALUE_ITEM_W);
    using CloseQueryFn    = LONG (WINAPI*)(PDH_HQUERY);

    Library      m_lib;
    OpenQueryFn  m_open    = nullptr;
    AddCounterFn m_add     = nullptr;
    CollectFn    m_collect = nullptr;
    ValueFn      m_value   = nullptr;
    ArrayFn      m_array   = nullptr;
    CloseQueryFn m_close   = nullptr;
    PDH_HQUERY   m_query   = nullptr;
    std::vector<unsigned char> m_buffer;

public:
    Pdh();

    ~Pdh();
    Pdh(const Pdh&) = delete;
    Pdh& operator=(const Pdh&) = delete;

    bool valid() const noexcept;

    Counter add(const wchar_t* path) noexcept;

    bool collect() noexcept;

    std::optional<double> value(Counter c, bool cap100 = false) noexcept;

    bool items(Counter c, std::vector<Item>& out);
};

struct KmtAdapterPerf {
    UINT32                PhysicalAdapterIndex;
    alignas(8) ULONGLONG  MemoryFrequency;
    alignas(8) ULONGLONG  MaxMemoryFrequency;
    alignas(8) ULONGLONG  MaxMemoryFrequencyOC;
    alignas(8) ULONGLONG  MemoryBandwidth;
    alignas(8) ULONGLONG  PCIEBandwidth;
    ULONG                 FanRPM;
    ULONG                 Power;
    ULONG                 Temperature;
    UCHAR                 PowerStateOverride;
};

struct KmtNodePerf {
    UINT32                NodeOrdinal;
    UINT32                PhysicalAdapterIndex;
    alignas(8) ULONGLONG  Frequency;
    alignas(8) ULONGLONG  MaxFrequency;
    alignas(8) ULONGLONG  MaxFrequencyOC;
    ULONG                 Voltage;
    ULONG                 VoltageMax;
    ULONG                 VoltageMaxOC;
    alignas(8) ULONGLONG  MaxTransitionLatency;
    alignas(8) ULONGLONG  Reserved;
};

struct KmtOpenFromLuid {
    LUID AdapterLuid;
    UINT hAdapter;
};

struct KmtQueryInfo {
    UINT  hAdapter;
    UINT  Type;
    void* pPrivateDriverData;
    UINT  PrivateDriverDataSize;
};

struct KmtClose {
    UINT hAdapter;
};

class Kmt {
public:
    struct Perf {
        std::optional<double> temperature_c;
        std::optional<double> fan_rpm;
        std::optional<double> power_percent;
        std::optional<double> memory_clock_mhz;
        std::optional<double> clock_mhz;
        std::optional<double> max_clock_mhz;
    };

private:
    static constexpr UINT kNodePerfData    = 61;
    static constexpr UINT kAdapterPerfData = 62;

    using OpenFn  = LONG (APIENTRY*)(KmtOpenFromLuid*);
    using QueryFn = LONG (APIENTRY*)(const KmtQueryInfo*);
    using CloseFn = LONG (APIENTRY*)(const KmtClose*);

    Library m_lib;
    OpenFn  m_open  = nullptr;
    QueryFn m_query = nullptr;
    CloseFn m_close = nullptr;
    std::map<std::uint64_t, UINT> m_handles;

    static std::uint64_t key_of(LUID l) noexcept;

    UINT handle_for(LUID luid);

public:
    Kmt();

    ~Kmt();

    Kmt(const Kmt&) = delete;
    Kmt& operator=(const Kmt&) = delete;

    bool valid() const noexcept;

    Perf query(LUID luid);
};

struct DxgiAdapterInfo {
    std::string   name;
    std::uint32_t vendor_id      = 0;
    std::uint32_t device_id      = 0;
    LUID          luid{};
    std::uint64_t dedicated      = 0;
    std::uint64_t shared         = 0;
    bool          software       = false;
    IDXGIAdapter3* adapter3      = nullptr;
};

class Dxgi {
private:
    using CreateFn = HRESULT (WINAPI*)(REFIID, void**);

    Library                      m_lib;
    std::vector<DxgiAdapterInfo> m_adapters;

public:
    Dxgi();

    ~Dxgi();
    Dxgi(const Dxgi&) = delete;
    Dxgi& operator=(const Dxgi&) = delete;

    const std::vector<DxgiAdapterInfo>& adapters() const noexcept;

    std::optional<std::uint64_t> process_local_usage(std::size_t index) const;
};

std::string luid_tag(LUID l);

std::uint64_t luid_u64(LUID l) noexcept;

} // namespace win
} // namespace detail
} // namespace system
} // namespace fizmo

#endif // OS_WINDOWS

#endif // FIZMO_SYSTEM_PLATFORM_WINDOWS_HPP
