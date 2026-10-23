#ifndef FIZMO_SYSTEM_IOKIT_MACOS_HPP
#define FIZMO_SYSTEM_IOKIT_MACOS_HPP

#include "common.hpp"

#if defined(OS_MACOS)

#ifndef __ASSERT_MACROS_DEFINE_VERSIONS_WITHOUT_UNDERSCORES
#define __ASSERT_MACROS_DEFINE_VERSIONS_WITHOUT_UNDERSCORES 0
#endif

#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOKitLib.h>

namespace fizmo {
namespace system {
namespace detail {
namespace mac {

template <typename T>
class CFRef {
private:
    T m_ref = nullptr;

public:
    CFRef() noexcept = default;
    explicit CFRef(T r) noexcept : m_ref(r) {}
    ~CFRef() { if (m_ref) ::CFRelease(m_ref); }
    CFRef(const CFRef&) = delete;
    CFRef& operator=(const CFRef&) = delete;
    CFRef(CFRef&& o) noexcept : m_ref(o.m_ref) { o.m_ref = nullptr; }
    CFRef& operator=(CFRef&& o) noexcept {
        if (this != &o) { if (m_ref) ::CFRelease(m_ref); m_ref = o.m_ref; o.m_ref = nullptr; }
        return *this;
    }
    T get() const noexcept { return m_ref; }
    explicit operator bool() const noexcept { return m_ref != nullptr; }
};

class IORef {
private:
    io_object_t m_obj = IO_OBJECT_NULL;

public:
    IORef() noexcept = default;
    explicit IORef(io_object_t o) noexcept : m_obj(o) {}
    ~IORef() { if (m_obj != IO_OBJECT_NULL) ::IOObjectRelease(m_obj); }
    IORef(const IORef&) = delete;
    IORef& operator=(const IORef&) = delete;
    IORef(IORef&& o) noexcept : m_obj(o.m_obj) { o.m_obj = IO_OBJECT_NULL; }
    IORef& operator=(IORef&& o) noexcept {
        if (this != &o) { if (m_obj != IO_OBJECT_NULL) ::IOObjectRelease(m_obj); m_obj = o.m_obj; o.m_obj = IO_OBJECT_NULL; }
        return *this;
    }
    io_object_t get() const noexcept { return m_obj; }
    explicit operator bool() const noexcept { return m_obj != IO_OBJECT_NULL; }
};

inline CFStringRef cf_key(const char* s) {
    return ::CFStringCreateWithCString(kCFAllocatorDefault, s, kCFStringEncodingUTF8);
}

inline std::string cf_to_string(CFTypeRef v) {
    if (!v) return {};
    if (::CFGetTypeID(v) == ::CFStringGetTypeID()) {
        CFStringRef s = static_cast<CFStringRef>(v);
        if (const char* fast = ::CFStringGetCStringPtr(s, kCFStringEncodingUTF8)) return fast;
        const CFIndex len = ::CFStringGetMaximumSizeForEncoding(::CFStringGetLength(s), kCFStringEncodingUTF8) + 1;
        std::string out(static_cast<std::size_t>(len), '\0');
        if (!::CFStringGetCString(s, &out[0], len, kCFStringEncodingUTF8)) return {};
        out.resize(std::strlen(out.c_str()));
        return out;
    }
    if (::CFGetTypeID(v) == ::CFDataGetTypeID()) {
        CFDataRef d = static_cast<CFDataRef>(v);
        const UInt8* p = ::CFDataGetBytePtr(d);
        const CFIndex n = ::CFDataGetLength(d);
        if (!p || n <= 0) return {};
        std::string out(reinterpret_cast<const char*>(p), static_cast<std::size_t>(n));
        while (!out.empty() && out.back() == '\0') out.pop_back();
        out.resize(std::strlen(out.c_str()));
        return out;
    }
    return {};
}

inline std::optional<double> cf_to_double(CFTypeRef v) {
    if (!v) return std::nullopt;
    if (::CFGetTypeID(v) == ::CFNumberGetTypeID()) {
        double d = 0.0;
        if (::CFNumberGetValue(static_cast<CFNumberRef>(v), kCFNumberDoubleType, &d)) return d;
        return std::nullopt;
    }
    if (::CFGetTypeID(v) == ::CFBooleanGetTypeID()) return ::CFBooleanGetValue(static_cast<CFBooleanRef>(v)) ? 1.0 : 0.0;
    return std::nullopt;
}

inline std::optional<std::uint64_t> cf_to_u64(CFTypeRef v) {
    if (!v) return std::nullopt;
    if (::CFGetTypeID(v) == ::CFNumberGetTypeID()) {
        CFNumberRef n = static_cast<CFNumberRef>(v);
        if (::CFNumberIsFloatType(n)) {
            double d = 0.0;
            if (!::CFNumberGetValue(n, kCFNumberDoubleType, &d) || d < 0.0) return std::nullopt;
            return static_cast<std::uint64_t>(d);
        }
        long long x = 0;
        if (!::CFNumberGetValue(n, kCFNumberLongLongType, &x) || x < 0) return std::nullopt;
        return static_cast<std::uint64_t>(x);
    }
    if (::CFGetTypeID(v) == ::CFDataGetTypeID()) {
        CFDataRef d = static_cast<CFDataRef>(v);
        const UInt8* p = ::CFDataGetBytePtr(d);
        const CFIndex n = ::CFDataGetLength(d);
        if (!p || n <= 0) return std::nullopt;
        std::uint64_t x = 0;
        for (CFIndex i = std::min<CFIndex>(n, 8) - 1; i >= 0; --i) x = (x << 8) | p[i];
        return x;
    }
    return std::nullopt;
}

inline CFRef<CFTypeRef> property(io_registry_entry_t e, const char* key) {
    CFRef<CFStringRef> k(cf_key(key));
    if (!k) return CFRef<CFTypeRef>();
    return CFRef<CFTypeRef>(::IORegistryEntryCreateCFProperty(e, k.get(), kCFAllocatorDefault, 0));
}

inline CFRef<CFTypeRef> search_property(io_registry_entry_t e, const char* key, bool parents) {
    CFRef<CFStringRef> k(cf_key(key));
    if (!k) return CFRef<CFTypeRef>();
    const IOOptionBits opts = kIORegistryIterateRecursively | (parents ? kIORegistryIterateParents : 0);
    return CFRef<CFTypeRef>(::IORegistryEntrySearchCFProperty(e, kIOServicePlane, k.get(), kCFAllocatorDefault, opts));
}

inline CFTypeRef dict_value(CFDictionaryRef d, const char* key) {
    if (!d) return nullptr;
    CFRef<CFStringRef> k(cf_key(key));
    if (!k) return nullptr;
    return ::CFDictionaryGetValue(d, k.get());
}

inline std::string class_name(io_object_t o) {
    io_name_t name = {};
    if (::IOObjectGetClass(o, name) != KERN_SUCCESS) return {};
    return name;
}

} // namespace mac
} // namespace detail
} // namespace system
} // namespace fizmo

#endif // OS_MACOS
#endif // FIZMO_SYSTEM_IOKIT_MACOS_HPP
