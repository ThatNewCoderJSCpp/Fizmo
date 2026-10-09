#ifndef FIZMO_SYSTEM_PROCESS_HPP
#define FIZMO_SYSTEM_PROCESS_HPP

#include "../Basic/fizmo_defines.hpp"
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#if defined(OS_LINUX)
#include <cerrno>
#include <fcntl.h>
#include <spawn.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
extern char** environ;
#endif

namespace fizmo {
namespace system {

struct ProcessResult {
    bool        started   = false;
    int         exit_code = -1;
    std::string output;
};

namespace detail {

#if defined(OS_WINDOWS)
inline std::wstring process_widen(const std::string& s) {
    if (s.empty()) return {};
    const int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), nullptr, 0);
    std::wstring out(static_cast<std::size_t>(n > 0 ? n : 0), L'\0');
    if (n > 0) MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), &out[0], n);
    return out;
}

inline std::wstring quote_arg(const std::wstring& a) {
    if (!a.empty() && a.find_first_of(L" \t\n\v\"") == std::wstring::npos) return a;
    std::wstring out = L"\"";
    for (std::size_t i = 0;; ++i) {
        std::size_t slashes = 0;
        while (i < a.size() && a[i] == L'\\') { ++i; ++slashes; }
        if (i == a.size()) { out.append(slashes * 2, L'\\'); break; }
        if (a[i] == L'"') { out.append(slashes * 2 + 1, L'\\'); out.push_back(L'"'); }
        else { out.append(slashes, L'\\'); out.push_back(a[i]); }
    }
    out.push_back(L'"');
    return out;
}
#endif

} // namespace detail

 std::string find_program(const std::string& name);

 ProcessResult run_process(const std::string& program, const std::vector<std::string>& args, bool capture_stderr = true);

} // namespace system
} // namespace fizmo

#endif // FIZMO_SYSTEM_PROCESS_HPP
