#include "app.hpp"

#if defined(OS_ANDROID)

#include <android/asset_manager.h>
#include <android/log.h>

#include <fcntl.h>
#include <unistd.h>

#include <cstdio>
#include <fstream>
#include <thread>

namespace fizmo {
namespace android {

namespace {

std::string clean(const std::string& path) {
    std::string p = path;
    while (!p.empty() && p.front() == '/') p.erase(p.begin());
    while (p.size() >= 2 && p[0] == '.' && p[1] == '/') p.erase(0, 2);
    while (!p.empty() && p.back() == '/') p.pop_back();
    return p;
}

} // namespace

bool Assets::exists(const std::string& path) const noexcept {
    if (!m_manager) return false;
    AAsset* a = AAssetManager_open(m_manager, clean(path).c_str(), AASSET_MODE_UNKNOWN);
    if (!a) return false;
    AAsset_close(a);
    return true;
}

std::optional<std::string> Assets::read(const std::string& path) const {
    if (!m_manager) return std::nullopt;
    AAsset* a = AAssetManager_open(m_manager, clean(path).c_str(), AASSET_MODE_BUFFER);
    if (!a) return std::nullopt;
    const off64_t size = AAsset_getLength64(a);
    std::string out;
    if (size > 0) {
        out.resize(static_cast<std::size_t>(size));
        std::size_t done = 0;
        while (done < out.size()) {
            const int got = AAsset_read(a, &out[done], out.size() - done);
            if (got <= 0) break;
            done += static_cast<std::size_t>(got);
        }
        out.resize(done);
    }
    AAsset_close(a);
    return out;
}

std::vector<std::string> Assets::list(const std::string& directory) const {
    std::vector<std::string> out;
    if (!m_manager) return out;
    AAssetDir* dir = AAssetManager_openDir(m_manager, clean(directory).c_str());
    if (!dir) return out;
    while (const char* name = AAssetDir_getNextFileName(dir)) out.emplace_back(name);
    AAssetDir_close(dir);
    return out;
}

bool Assets::extract(const std::string& path, const std::string& destination) const {
    const std::optional<std::string> data = read(path);
    if (!data) return false;
    const std::string temp = destination + ".part";
    {
        std::ofstream file(temp, std::ios::binary | std::ios::trunc);
        if (!file) return false;
        file.write(data->data(), static_cast<std::streamsize>(data->size()));
        if (!file) return false;
    }
    if (std::rename(temp.c_str(), destination.c_str()) != 0) {
        std::remove(temp.c_str());
        return false;
    }
    return true;
}

void log(LogLevel level, const std::string& tag, const std::string& message) noexcept {
    __android_log_write(static_cast<int>(level), tag.empty() ? "fizmo" : tag.c_str(), message.c_str());
}

void log(const std::string& message) noexcept {
    __android_log_write(ANDROID_LOG_INFO, "fizmo", message.c_str());
}

bool redirect_stdio(const std::string& tag) {
    int fds[2] = { -1, -1 };
    if (pipe(fds) != 0) return false;
    std::setvbuf(stdout, nullptr, _IOLBF, 0);
    std::setvbuf(stderr, nullptr, _IONBF, 0);
    if (dup2(fds[1], STDOUT_FILENO) < 0 || dup2(fds[1], STDERR_FILENO) < 0) {
        close(fds[0]);
        close(fds[1]);
        return false;
    }
    close(fds[1]);
    const int read_end = fds[0];
    const std::string name = tag.empty() ? std::string("fizmo") : tag;
    try {
        std::thread([read_end, name] {
            std::string line;
            char buffer[512];
            while (true) {
                const ssize_t got = ::read(read_end, buffer, sizeof(buffer));
                if (got <= 0) break;
                for (ssize_t i = 0; i < got; ++i) {
                    const char c = buffer[i];
                    if (c == '\n') {
                        __android_log_write(ANDROID_LOG_INFO, name.c_str(), line.c_str());
                        line.clear();
                    } else {
                        line += c;
                        if (line.size() >= 3000) {
                            __android_log_write(ANDROID_LOG_INFO, name.c_str(), line.c_str());
                            line.clear();
                        }
                    }
                }
            }
            if (!line.empty()) __android_log_write(ANDROID_LOG_INFO, name.c_str(), line.c_str());
            close(read_end);
        }).detach();
    } catch (...) {
        close(read_end);
        return false;
    }
    return true;
}

} // namespace android
} // namespace fizmo

#endif // OS_ANDROID
