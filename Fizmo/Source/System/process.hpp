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
std::wstring process_widen(const std::string& s);

std::wstring quote_arg(const std::wstring& a);
#endif

} // namespace detail

std::string find_program(const std::string& name);

ProcessResult run_process(const std::string& program, const std::vector<std::string>& args, bool capture_stderr = true);

} // namespace system
} // namespace fizmo

#endif // FIZMO_SYSTEM_PROCESS_HPP
