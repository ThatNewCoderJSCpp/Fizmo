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
    for (std::size_t i = 0; ; ++i) {
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

inline std::string find_program(const std::string& name) {
    if (name.find('/') != std::string::npos || name.find('\\') != std::string::npos) return name;
    const char* path = std::getenv("PATH");
    if (!path) return {};
    const std::string p = path;
#if defined(OS_WINDOWS)
    const char sep = ';';
    const char* exts[] = { "", ".exe", ".bat", ".cmd" };
#else
    const char sep = ':';
    const char* exts[] = { "" };
#endif
    std::size_t start = 0;

    while (start <= p.size()) {
        std::size_t end = p.find(sep, start);
        if (end == std::string::npos) end = p.size();
        const std::string dir = p.substr(start, end - start);

        if (!dir.empty()) {
            for (const char* ext : exts) {
                const std::string full = dir + "/" + name + ext;
#if defined(OS_WINDOWS)
                const DWORD attr = GetFileAttributesW(detail::process_widen(full).c_str());
                if (attr != INVALID_FILE_ATTRIBUTES && !(attr & FILE_ATTRIBUTE_DIRECTORY)) return full;
#else
                struct stat st;
                if (::stat(full.c_str(), &st) == 0 && S_ISREG(st.st_mode) && ::access(full.c_str(), X_OK) == 0) return full;
#endif
            }
        }

        start = end + 1;
    }

    return {};
}

inline ProcessResult run_process(const std::string& program, const std::vector<std::string>& args, bool capture_stderr = true) {
    ProcessResult result;
    const std::string exe = find_program(program);
    if (exe.empty()) return result;

#if defined(OS_LINUX)
    int fds[2];
    if (::pipe2(fds, O_CLOEXEC) != 0) return result;
    posix_spawn_file_actions_t actions;
    posix_spawn_file_actions_init(&actions);
    posix_spawn_file_actions_adddup2(&actions, fds[1], 1);
    if (capture_stderr) posix_spawn_file_actions_adddup2(&actions, fds[1], 2);
    std::vector<char*> argv;
    argv.push_back(const_cast<char*>(exe.c_str()));
    for (const std::string& a : args) argv.push_back(const_cast<char*>(a.c_str()));
    argv.push_back(nullptr);
    pid_t pid = 0;
    const int rc = posix_spawn(&pid, exe.c_str(), &actions, nullptr, argv.data(), environ);
    posix_spawn_file_actions_destroy(&actions);
    ::close(fds[1]);
    if (rc != 0) { ::close(fds[0]); return result; }
    result.started = true;
    char buf[4096];

    for (;;) {
        const ssize_t n = ::read(fds[0], buf, sizeof(buf));
        if (n > 0) { result.output.append(buf, static_cast<std::size_t>(n)); continue; }
        if (n < 0 && errno == EINTR) continue;
        break;
    }

    ::close(fds[0]);
    int status = 0;
    while (::waitpid(pid, &status, 0) < 0 && errno == EINTR) {}
    result.exit_code = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
    return result;
#elif defined(OS_WINDOWS)
    SECURITY_ATTRIBUTES sa{};
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;
    HANDLE read_end = nullptr, write_end = nullptr;
    if (!CreatePipe(&read_end, &write_end, &sa, 0)) return result;
    SetHandleInformation(read_end, HANDLE_FLAG_INHERIT, 0);
    std::wstring cmd = detail::quote_arg(detail::process_widen(exe));
    for (const std::string& a : args) cmd += L" " + detail::quote_arg(detail::process_widen(a));
    STARTUPINFOW si{};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdOutput = write_end;
    si.hStdError = capture_stderr ? write_end : GetStdHandle(STD_ERROR_HANDLE);
    si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
    PROCESS_INFORMATION pi{};
    const std::wstring wexe = detail::process_widen(exe);
    const bool script = exe.size() > 4 && (_stricmp(exe.c_str() + exe.size() - 4, ".bat") == 0 || _stricmp(exe.c_str() + exe.size() - 4, ".cmd") == 0);
    const BOOL ok = CreateProcessW(script ? nullptr : wexe.c_str(), &cmd[0], nullptr, nullptr, TRUE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi);
    CloseHandle(write_end);
    if (!ok) { CloseHandle(read_end); return result; }
    result.started = true;
    char buf[4096];
    DWORD n = 0;
    while (ReadFile(read_end, buf, sizeof(buf), &n, nullptr) && n > 0) result.output.append(buf, n);
    CloseHandle(read_end);
    WaitForSingleObject(pi.hProcess, INFINITE);
    DWORD code = 0;
    GetExitCodeProcess(pi.hProcess, &code);
    result.exit_code = static_cast<int>(code);
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    return result;
#else
    (void)args;
    (void)capture_stderr;
    return result;
#endif
}

} // namespace system
} // namespace fizmo

#endif // FIZMO_SYSTEM_PROCESS_HPP
