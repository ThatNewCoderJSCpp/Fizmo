#ifndef FIZMO_ARRAYS_THREADS_WINDOWS_HPP
#define FIZMO_ARRAYS_THREADS_WINDOWS_HPP

#include "../../config.hpp"
#include "../../../../Basic/fizmo_defines.hpp"
#include <windows.h>

namespace fizmo {
namespace arrays {
namespace threads {
namespace platform {

inline unsigned hardware_threads() noexcept {
    DWORD_PTR process = 0, system = 0;
    if (::GetProcessAffinityMask(::GetCurrentProcess(), &process, &system) && process) {
        unsigned c = 0;
        for (DWORD_PTR m = process; m; m &= m - 1) ++c;
        if (c) return c;
    }
    SYSTEM_INFO info;
    ::GetSystemInfo(&info);
    return info.dwNumberOfProcessors ? static_cast<unsigned>(info.dwNumberOfProcessors) : 1u;
}

struct Thread {
    HANDLE handle = nullptr;
};

struct StartPack {
    void* (*fn)(void*);
    void* arg;
};

inline DWORD WINAPI trampoline(LPVOID p) {
    StartPack local = *static_cast<StartPack*>(p);
    ::HeapFree(::GetProcessHeap(), 0, p);
    local.fn(local.arg);
    return 0;
}

inline bool start(Thread& t, void* (*fn)(void*), void* arg) noexcept {
    StartPack* pack = static_cast<StartPack*>(::HeapAlloc(::GetProcessHeap(), 0, sizeof(StartPack)));
    if (!pack) return false;
    pack->fn = fn;
    pack->arg = arg;
    t.handle = ::CreateThread(nullptr, std::size_t(1) << 20, &trampoline, pack, 0, nullptr);
    if (!t.handle) { ::HeapFree(::GetProcessHeap(), 0, pack); return false; }
    return true;
}

inline void join(Thread& t) noexcept {
    if (!t.handle) return;
    ::WaitForSingleObject(t.handle, INFINITE);
    ::CloseHandle(t.handle);
    t.handle = nullptr;
}

inline const char* name() noexcept { return "win32"; }

} // namespace platform
} // namespace threads
} // namespace arrays
} // namespace fizmo

#endif // FIZMO_ARRAYS_THREADS_WINDOWS_HPP
