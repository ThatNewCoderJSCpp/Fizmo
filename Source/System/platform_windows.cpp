#define ALL_FIZMO
#include <fizmo/includes.hpp>
#include "platform_windows.hpp"

namespace fizmo {
namespace system {
namespace detail {
namespace win {

#if defined(OS_WINDOWS)
std::string narrow(const wchar_t* s, int len) {
    if (!s) return {};
    const int n = WideCharToMultiByte(CP_UTF8, 0, s, len, nullptr, 0, nullptr, nullptr);
    if (n <= 0) return {};
    std::string out(static_cast<std::size_t>(n), '\0');
    WideCharToMultiByte(CP_UTF8, 0, s, len, &out[0], n, nullptr, nullptr);
    if (len < 0 && !out.empty() && out.back() == '\0') out.pop_back();
    return out;
}
#endif

#if defined(OS_WINDOWS)
std::string narrow(const std::wstring& s) { return narrow(s.c_str(), static_cast<int>(s.size())); }
#endif

#if defined(OS_WINDOWS)
std::wstring widen(const std::string& s) {
    if (s.empty()) return {};
    const int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), static_cast<int>(s.size()), nullptr, 0);
    if (n <= 0) return {};
    std::wstring out(static_cast<std::size_t>(n), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), static_cast<int>(s.size()), &out[0], n);
    return out;
}
#endif

#if defined(OS_WINDOWS)
std::uint64_t filetime_u64(const FILETIME& f) noexcept {
    return (static_cast<std::uint64_t>(f.dwHighDateTime) << 32) | f.dwLowDateTime;
}
#endif

#if defined(OS_WINDOWS)
Library::Library(const wchar_t* name, bool system_only) noexcept {
    m_handle = LoadLibraryExW(name, nullptr, system_only ? LOAD_LIBRARY_SEARCH_SYSTEM32 : 0);
    if (!m_handle && system_only && GetLastError() == ERROR_INVALID_PARAMETER) m_handle = LoadLibraryW(name);
}
#endif

#if defined(OS_WINDOWS)
Library::~Library() { if (m_handle) FreeLibrary(static_cast<HMODULE>(m_handle)); }
#endif

#if defined(OS_WINDOWS)
Library::Library(Library&& o) noexcept : m_handle(o.m_handle) { o.m_handle = nullptr; }
#endif

#if defined(OS_WINDOWS)
auto Library::operator=(Library&& o) noexcept -> Library& {
    if (this != &o) { if (m_handle) FreeLibrary(static_cast<HMODULE>(m_handle)); m_handle = o.m_handle; o.m_handle = nullptr; }
    return *this;
}
#endif

#if defined(OS_WINDOWS)
bool Library::valid() const noexcept { return m_handle != nullptr; }
#endif

#if defined(OS_WINDOWS)
void* Library::symbol(const char* name) const noexcept {
    if (!m_handle) return nullptr;
    FARPROC p = GetProcAddress(static_cast<HMODULE>(m_handle), name);
    void* out = nullptr;
    static_assert(sizeof(out) == sizeof(p), "function pointer size");
    std::memcpy(&out, &p, sizeof(out));
    return out;
}
#endif

#if defined(OS_WINDOWS)
std::string registry_string(HKEY root, const wchar_t* key, const wchar_t* value) {
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
#endif

#if defined(OS_WINDOWS)
std::optional<std::uint32_t> registry_dword(HKEY root, const wchar_t* key, const wchar_t* value) {
    HKEY h = nullptr;
    if (RegOpenKeyExW(root, key, 0, KEY_READ, &h) != ERROR_SUCCESS) return std::nullopt;
    DWORD type = 0, data = 0, size = sizeof(data);
    const LONG r = RegQueryValueExW(h, value, nullptr, &type, reinterpret_cast<LPBYTE>(&data), &size);
    RegCloseKey(h);
    if (r != ERROR_SUCCESS || type != REG_DWORD) return std::nullopt;
    return static_cast<std::uint32_t>(data);
}
#endif

#if defined(OS_WINDOWS)
Pdh::Pdh() : m_lib(L"pdh.dll") {
    m_open    = m_lib.get<OpenQueryFn>("PdhOpenQueryW");
    m_add     = m_lib.get<AddCounterFn>("PdhAddEnglishCounterW");
    m_collect = m_lib.get<CollectFn>("PdhCollectQueryData");
    m_value   = m_lib.get<ValueFn>("PdhGetFormattedCounterValue");
    m_array   = m_lib.get<ArrayFn>("PdhGetFormattedCounterArrayW");
    m_close   = m_lib.get<CloseQueryFn>("PdhCloseQuery");
    if (m_open && m_add && m_collect && m_value && m_array && m_close && m_open(nullptr, 0, &m_query) != ERROR_SUCCESS) m_query = nullptr;
}
#endif

#if defined(OS_WINDOWS)
Pdh::~Pdh() { if (m_query && m_close) m_close(m_query); }
#endif

#if defined(OS_WINDOWS)
bool Pdh::valid() const noexcept { return m_query != nullptr; }
#endif

#if defined(OS_WINDOWS)
auto Pdh::add(const wchar_t* path) noexcept -> Counter {
    if (!m_query) return nullptr;
    PDH_HCOUNTER c = nullptr;
    if (m_add(m_query, path, 0, &c) != ERROR_SUCCESS) return nullptr;
    return c;
}
#endif

#if defined(OS_WINDOWS)
bool Pdh::collect() noexcept { return m_query && m_collect(m_query) == ERROR_SUCCESS; }
#endif

#if defined(OS_WINDOWS)
std::optional<double> Pdh::value(Counter c, bool cap100) noexcept {
    if (!c) return std::nullopt;
    PDH_FMT_COUNTERVALUE v{};
    if (m_value(c, PDH_FMT_DOUBLE | (cap100 ? 0 : PDH_FMT_NOCAP100), nullptr, &v) != ERROR_SUCCESS) return std::nullopt;
    if (v.CStatus != PDH_CSTATUS_VALID_DATA && v.CStatus != PDH_CSTATUS_NEW_DATA) return std::nullopt;
    if (!std::isfinite(v.doubleValue)) return std::nullopt;
    return v.doubleValue;
}
#endif

#if defined(OS_WINDOWS)
bool Pdh::items(Counter c, std::vector<Item>& out) {
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
#endif

#if defined(OS_WINDOWS)
std::uint64_t Kmt::key_of(LUID l) noexcept { return (static_cast<std::uint64_t>(static_cast<std::uint32_t>(l.HighPart)) << 32) | l.LowPart; }
#endif

#if defined(OS_WINDOWS)
auto Kmt::handle_for(LUID luid) -> UINT {
    const std::uint64_t k = key_of(luid);
    const auto it = m_handles.find(k);
    if (it != m_handles.end()) return it->second;
    KmtOpenFromLuid o{};
    o.AdapterLuid = luid;
    const UINT h = m_open(&o) >= 0 ? o.hAdapter : 0;
    m_handles[k] = h;
    return h;
}
#endif

#if defined(OS_WINDOWS)
Kmt::Kmt() : m_lib(L"gdi32.dll") {
    m_open  = m_lib.get<OpenFn>("D3DKMTOpenAdapterFromLuid");
    m_query = m_lib.get<QueryFn>("D3DKMTQueryAdapterInfo");
    m_close = m_lib.get<CloseFn>("D3DKMTCloseAdapter");
}
#endif

#if defined(OS_WINDOWS)
Kmt::~Kmt() {
    if (!m_close) return;
    for (const auto& kv : m_handles) if (kv.second) { KmtClose c{ kv.second }; m_close(&c); }
}
#endif

#if defined(OS_WINDOWS)
bool Kmt::valid() const noexcept { return m_open && m_query && m_close; }
#endif

#if defined(OS_WINDOWS)
auto Kmt::query(LUID luid) -> Perf {
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
#endif

#if defined(OS_WINDOWS)
Dxgi::Dxgi() : m_lib(L"dxgi.dll") {
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
#endif

#if defined(OS_WINDOWS)
Dxgi::~Dxgi() { for (auto& a : m_adapters) if (a.adapter3) a.adapter3->Release(); }
#endif

#if defined(OS_WINDOWS)
auto Dxgi::adapters() const noexcept -> const std::vector<DxgiAdapterInfo>& { return m_adapters; }
#endif

#if defined(OS_WINDOWS)
std::optional<std::uint64_t> Dxgi::process_local_usage(std::size_t index) const {
    if (index >= m_adapters.size() || !m_adapters[index].adapter3) return std::nullopt;
    DXGI_QUERY_VIDEO_MEMORY_INFO info{};
    if (FAILED(m_adapters[index].adapter3->QueryVideoMemoryInfo(0, DXGI_MEMORY_SEGMENT_GROUP_LOCAL, &info))) return std::nullopt;
    return static_cast<std::uint64_t>(info.CurrentUsage);
}
#endif

#if defined(OS_WINDOWS)
std::string luid_tag(LUID l) {
    char buf[48];
    std::snprintf(buf, sizeof(buf), "luid_0x%08x_0x%08x", static_cast<unsigned>(l.HighPart), static_cast<unsigned>(l.LowPart));
    return lower(buf);
}
#endif

#if defined(OS_WINDOWS)
std::uint64_t luid_u64(LUID l) noexcept {
    std::uint64_t v = 0;
    std::memcpy(&v, &l, sizeof(v) < sizeof(l) ? sizeof(v) : sizeof(l));
    return v;
}
#endif

} // namespace win
} // namespace detail
} // namespace system
} // namespace fizmo
