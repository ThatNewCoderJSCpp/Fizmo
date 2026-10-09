#ifndef FIZMO_SYSTEM_PLATFORM_WINDOWS_HPP
#define FIZMO_SYSTEM_PLATFORM_WINDOWS_HPP

#include "common.hpp"

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

inline std::string narrow(const wchar_t* s, int len = -1) {
    if (!s) return {};
    const int n = WideCharToMultiByte(CP_UTF8, 0, s, len, nullptr, 0, nullptr, nullptr);
    if (n <= 0) return {};
    std::string out(static_cast<std::size_t>(n), '\0');
    WideCharToMultiByte(CP_UTF8, 0, s, len, &out[0], n, nullptr, nullptr);
    if (len < 0 && !out.empty() && out.back() == '\0') out.pop_back();
    return out;
}

inline std::string narrow(const std::wstring& s) { return narrow(s.c_str(), static_cast<int>(s.size())); }

inline std::wstring widen(const std::string& s) {
    if (s.empty()) return {};
    const int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), static_cast<int>(s.size()), nullptr, 0);
    if (n <= 0) return {};
    std::wstring out(static_cast<std::size_t>(n), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), static_cast<int>(s.size()), &out[0], n);
    return out;
}

inline std::uint64_t filetime_u64(const FILETIME& f) noexcept {
    return (static_cast<std::uint64_t>(f.dwHighDateTime) << 32) | f.dwLowDateTime;
}

class Library {
private:
    HMODULE m_handle = nullptr;

public:
    Library() noexcept = default;

    explicit Library(const wchar_t* name, bool system_only = true) noexcept {
        m_handle = LoadLibraryExW(name, nullptr, system_only ? LOAD_LIBRARY_SEARCH_SYSTEM32 : 0);
        if (!m_handle && system_only && GetLastError() == ERROR_INVALID_PARAMETER) m_handle = LoadLibraryW(name);
    }

    ~Library() { if (m_handle) FreeLibrary(m_handle); }
    Library(const Library&) = delete;
    Library& operator=(const Library&) = delete;

    Library(Library&& o) noexcept : m_handle(o.m_handle) { o.m_handle = nullptr; }
    Library& operator=(Library&& o) noexcept {
        if (this != &o) { if (m_handle) FreeLibrary(m_handle); m_handle = o.m_handle; o.m_handle = nullptr; }
        return *this;
    }

    bool valid() const noexcept { return m_handle != nullptr; }

    template <typename F>
    F get(const char* name) const noexcept {
        if (!m_handle) return nullptr;
        FARPROC p = GetProcAddress(m_handle, name);
        F f = nullptr;
        static_assert(sizeof(f) == sizeof(p), "function pointer size");
        std::memcpy(&f, &p, sizeof(f));
        return f;
    }
};

inline std::string registry_string(HKEY root, const wchar_t* key, const wchar_t* value) {
    HKEY h = nullptr;
    if (RegOpenKeyExW(root, key, 0, KEY_READ, &h) != ERROR_SUCCESS) return {};
    DWORD type = 0, size = 0;
    std::string out;

    if (RegQueryValueExW(h, value, nullptr, &type, nullptr, &size) == ERROR_SUCCESS && (type == REG_SZ || type == REG_EXPAND_SZ) && size > 0) {
        std::wstring buf(size / sizeof(wchar_t) + 1, L'\0');
        if (RegQueryValueExW(h, value, nullptr, &type, reinterpret_cast<LPBYTE>(&buf[0]), &size) == ERROR_SUCCESS) {
            buf.resize(wcsnlen(buf.c_str(), buf.size()));
            out = trim(narrow(buf));
        }
    }

    RegCloseKey(h);
    return out;
}

inline std::optional<std::uint32_t> registry_dword(HKEY root, const wchar_t* key, const wchar_t* value) {
    HKEY h = nullptr;
    if (RegOpenKeyExW(root, key, 0, KEY_READ, &h) != ERROR_SUCCESS) return std::nullopt;
    DWORD type = 0, data = 0, size = sizeof(data);
    const LONG r = RegQueryValueExW(h, value, nullptr, &type, reinterpret_cast<LPBYTE>(&data), &size);
    RegCloseKey(h);
    if (r != ERROR_SUCCESS || type != REG_DWORD) return std::nullopt;
    return static_cast<std::uint32_t>(data);
}

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
    Pdh() : m_lib(L"pdh.dll") {
        m_open    = m_lib.get<OpenQueryFn>("PdhOpenQueryW");
        m_add     = m_lib.get<AddCounterFn>("PdhAddEnglishCounterW");
        m_collect = m_lib.get<CollectFn>("PdhCollectQueryData");
        m_value   = m_lib.get<ValueFn>("PdhGetFormattedCounterValue");
        m_array   = m_lib.get<ArrayFn>("PdhGetFormattedCounterArrayW");
        m_close   = m_lib.get<CloseQueryFn>("PdhCloseQuery");
        if (m_open && m_add && m_collect && m_value && m_array && m_close && m_open(nullptr, 0, &m_query) != ERROR_SUCCESS) m_query = nullptr;
    }

    ~Pdh() { if (m_query && m_close) m_close(m_query); }
    Pdh(const Pdh&) = delete;
    Pdh& operator=(const Pdh&) = delete;

    bool valid() const noexcept { return m_query != nullptr; }

    Counter add(const wchar_t* path) noexcept {
        if (!m_query) return nullptr;
        PDH_HCOUNTER c = nullptr;
        if (m_add(m_query, path, 0, &c) != ERROR_SUCCESS) return nullptr;
        return c;
    }

    bool collect() noexcept { return m_query && m_collect(m_query) == ERROR_SUCCESS; }

    std::optional<double> value(Counter c, bool cap100 = false) noexcept {
        if (!c) return std::nullopt;
        PDH_FMT_COUNTERVALUE v{};
        if (m_value(c, PDH_FMT_DOUBLE | (cap100 ? 0 : PDH_FMT_NOCAP100), nullptr, &v) != ERROR_SUCCESS) return std::nullopt;
        if (v.CStatus != PDH_CSTATUS_VALID_DATA && v.CStatus != PDH_CSTATUS_NEW_DATA) return std::nullopt;
        if (!std::isfinite(v.doubleValue)) return std::nullopt;
        return v.doubleValue;
    }

    bool items(Counter c, std::vector<Item>& out) {
        out.clear();
        if (!c) return false;
        DWORD size = 0, count = 0;
        LONG r = m_array(c, PDH_FMT_DOUBLE | PDH_FMT_NOCAP100, &size, &count, nullptr);
        if (r != static_cast<LONG>(PDH_MORE_DATA)) return false;

        for (int attempt = 0; attempt < 4; ++attempt) {
            m_buffer.resize(size + 1024);
            size = static_cast<DWORD>(m_buffer.size());
            r = m_array(c, PDH_FMT_DOUBLE | PDH_FMT_NOCAP100, &size, &count, reinterpret_cast<PPDH_FMT_COUNTERVALUE_ITEM_W>(m_buffer.data()));
            if (r != static_cast<LONG>(PDH_MORE_DATA)) break;
        }

        if (r != ERROR_SUCCESS) return false;
        const auto* list = reinterpret_cast<const PDH_FMT_COUNTERVALUE_ITEM_W*>(m_buffer.data());
        out.reserve(count);

        for (DWORD i = 0; i < count; ++i) {
            if (list[i].FmtValue.CStatus != PDH_CSTATUS_VALID_DATA && list[i].FmtValue.CStatus != PDH_CSTATUS_NEW_DATA) continue;
            if (!std::isfinite(list[i].FmtValue.doubleValue)) continue;
            out.push_back({ narrow(list[i].szName), list[i].FmtValue.doubleValue });
        }

        return true;
    }
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

    static std::uint64_t key_of(LUID l) noexcept { return (static_cast<std::uint64_t>(static_cast<std::uint32_t>(l.HighPart)) << 32) | l.LowPart; }

    UINT handle_for(LUID luid) {
        const std::uint64_t k = key_of(luid);
        const auto it = m_handles.find(k);
        if (it != m_handles.end()) return it->second;
        KmtOpenFromLuid o{};
        o.AdapterLuid = luid;
        const UINT h = m_open(&o) >= 0 ? o.hAdapter : 0;
        m_handles[k] = h;
        return h;
    }

public:
    Kmt() : m_lib(L"gdi32.dll") {
        m_open  = m_lib.get<OpenFn>("D3DKMTOpenAdapterFromLuid");
        m_query = m_lib.get<QueryFn>("D3DKMTQueryAdapterInfo");
        m_close = m_lib.get<CloseFn>("D3DKMTCloseAdapter");
    }

    ~Kmt() {
        if (!m_close) return;
        for (const auto& kv : m_handles) if (kv.second) { KmtClose c{ kv.second }; m_close(&c); }
    }

    Kmt(const Kmt&) = delete;
    Kmt& operator=(const Kmt&) = delete;

    bool valid() const noexcept { return m_open && m_query && m_close; }

    Perf query(LUID luid) {
        Perf p;
        if (!valid()) return p;
        const UINT h = handle_for(luid);
        if (!h) return p;

        KmtAdapterPerf a{};
        KmtQueryInfo q{ h, kAdapterPerfData, &a, static_cast<UINT>(sizeof(a)) };

        if (m_query(&q) >= 0) {
            if (a.Temperature > 0 && a.Temperature < 2000) p.temperature_c = a.Temperature / 10.0;
            if (a.FanRPM > 0 && a.FanRPM < 100000) p.fan_rpm = static_cast<double>(a.FanRPM);
            if (a.Power > 0 && a.Power <= 2000) p.power_percent = a.Power / 10.0;
            if (a.MemoryFrequency > 0) p.memory_clock_mhz = static_cast<double>(a.MemoryFrequency) / 1.0e6;
        }

        KmtNodePerf n{};
        KmtQueryInfo qn{ h, kNodePerfData, &n, static_cast<UINT>(sizeof(n)) };

        if (m_query(&qn) >= 0) {
            if (n.Frequency > 0) p.clock_mhz = static_cast<double>(n.Frequency) / 1.0e6;
            if (n.MaxFrequency > 0) p.max_clock_mhz = static_cast<double>(n.MaxFrequency) / 1.0e6;
        }

        return p;
    }
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
    Dxgi() : m_lib(L"dxgi.dll") {
        const CreateFn create = m_lib.get<CreateFn>("CreateDXGIFactory1");
        if (!create) return;
        IDXGIFactory1* factory = nullptr;
        if (FAILED(create(__uuidof(IDXGIFactory1), reinterpret_cast<void**>(&factory))) || !factory) return;

        for (UINT i = 0; i < 64; ++i) {
            IDXGIAdapter1* a = nullptr;
            if (factory->EnumAdapters1(i, &a) == DXGI_ERROR_NOT_FOUND || !a) break;
            DXGI_ADAPTER_DESC1 d{};

            if (SUCCEEDED(a->GetDesc1(&d))) {
                DxgiAdapterInfo info;
                info.name      = trim(narrow(d.Description));
                info.vendor_id = d.VendorId;
                info.device_id = d.DeviceId;
                info.luid      = d.AdapterLuid;
                info.dedicated = static_cast<std::uint64_t>(d.DedicatedVideoMemory);
                info.shared    = static_cast<std::uint64_t>(d.SharedSystemMemory);
                info.software  = (d.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) != 0 || (d.VendorId == 0x1414 && d.DeviceId == 0x8c);
                IDXGIAdapter3* a3 = nullptr;
                if (SUCCEEDED(a->QueryInterface(__uuidof(IDXGIAdapter3), reinterpret_cast<void**>(&a3)))) info.adapter3 = a3;
                m_adapters.push_back(info);
            }

            a->Release();
        }

        factory->Release();
    }

    ~Dxgi() { for (auto& a : m_adapters) if (a.adapter3) a.adapter3->Release(); }
    Dxgi(const Dxgi&) = delete;
    Dxgi& operator=(const Dxgi&) = delete;

    const std::vector<DxgiAdapterInfo>& adapters() const noexcept { return m_adapters; }

    std::optional<std::uint64_t> process_local_usage(std::size_t index) const {
        if (index >= m_adapters.size() || !m_adapters[index].adapter3) return std::nullopt;
        DXGI_QUERY_VIDEO_MEMORY_INFO info{};
        if (FAILED(m_adapters[index].adapter3->QueryVideoMemoryInfo(0, DXGI_MEMORY_SEGMENT_GROUP_LOCAL, &info))) return std::nullopt;
        return static_cast<std::uint64_t>(info.CurrentUsage);
    }
};

inline std::string luid_tag(LUID l) {
    char buf[48];
    std::snprintf(buf, sizeof(buf), "luid_0x%08x_0x%08x", static_cast<unsigned>(l.HighPart), static_cast<unsigned>(l.LowPart));
    return lower(buf);
}

inline std::uint64_t luid_u64(LUID l) noexcept {
    std::uint64_t v = 0;
    std::memcpy(&v, &l, sizeof(v) < sizeof(l) ? sizeof(v) : sizeof(l));
    return v;
}

} // namespace win
} // namespace detail
} // namespace system
} // namespace fizmo

#endif // OS_WINDOWS

#endif // FIZMO_SYSTEM_PLATFORM_WINDOWS_HPP
