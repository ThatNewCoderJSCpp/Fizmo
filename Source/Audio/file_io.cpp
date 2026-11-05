#include "file_io.hpp"

#include <cstring>
#include <utility>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

namespace fizmo {
namespace audio {
namespace detail {

namespace {

#if defined(_WIN32)
std::wstring widen(const std::string& s) {
    if (s.empty()) return std::wstring();
    const int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), nullptr, 0);
    std::wstring w(static_cast<std::size_t>(n), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), &w[0], n);
    return w;
}
#endif

} // namespace

File::~File() {
    close();
}

File::File(File&& other) noexcept : m_file(other.m_file) {
    other.m_file = nullptr;
}

File& File::operator=(File&& other) noexcept {
    if (this != &other) {
        close();
        m_file = other.m_file;
        other.m_file = nullptr;
    }
    return *this;
}

bool File::open(const std::string& path, const char* mode) {
    close();
#if defined(_WIN32)
    const std::wstring wmode = widen(mode);
    m_file = _wfopen(widen(path).c_str(), wmode.c_str());
#else
    m_file = std::fopen(path.c_str(), mode);
#endif
    return m_file != nullptr;
}

void File::close() {
    if (m_file) {
        std::fclose(m_file);
        m_file = nullptr;
    }
}

std::size_t File::read(void* data, std::size_t bytes) {
    if (!m_file || bytes == 0) return 0;
    return std::fread(data, 1, bytes, m_file);
}

bool File::read_exact(void* data, std::size_t bytes) {
    return read(data, bytes) == bytes;
}

bool File::write(const void* data, std::size_t bytes) {
    if (!m_file) return false;
    if (bytes == 0) return true;
    return std::fwrite(data, 1, bytes, m_file) == bytes;
}

bool File::seek(std::int64_t offset, int origin) {
    if (!m_file) return false;
#if defined(_WIN32)
    return _fseeki64(m_file, offset, origin) == 0;
#else
    return fseeko(m_file, static_cast<off_t>(offset), origin) == 0;
#endif
}

std::int64_t File::tell() {
    if (!m_file) return -1;
#if defined(_WIN32)
    return _ftelli64(m_file);
#else
    return static_cast<std::int64_t>(ftello(m_file));
#endif
}

std::int64_t File::size() {
    const std::int64_t here = tell();
    if (here < 0 || !seek(0, SEEK_END)) return -1;
    const std::int64_t end = tell();
    seek(here, SEEK_SET);
    return end;
}

bool File::flush() {
    return m_file && std::fflush(m_file) == 0;
}

bool read_whole_file(const std::string& path, std::vector<std::uint8_t>& out, std::string* error) {
    File f;
    if (!f.open(path, "rb")) {
        if (error) *error = "can't open " + path;
        return false;
    }
    const std::int64_t size = f.size();
    if (size < 0) {
        if (error) *error = "can't read the size of " + path;
        return false;
    }
    out.resize(static_cast<std::size_t>(size));
    if (!f.read_exact(out.data(), out.size())) {
        if (error) *error = "can't read " + path;
        return false;
    }
    return true;
}

bool write_whole_file(const std::string& path, const void* data, std::size_t size, std::string* error) {
    File f;
    if (!f.open(path, "wb")) {
        if (error) *error = "can't create " + path;
        return false;
    }
    if (!f.write(data, size) || !f.flush()) {
        if (error) *error = "can't write " + path;
        return false;
    }
    return true;
}

std::uint16_t read_le16(const std::uint8_t* p) noexcept {
    return static_cast<std::uint16_t>(p[0] | (p[1] << 8));
}

std::uint32_t read_le32(const std::uint8_t* p) noexcept {
    return static_cast<std::uint32_t>(p[0]) | (static_cast<std::uint32_t>(p[1]) << 8) | (static_cast<std::uint32_t>(p[2]) << 16) | (static_cast<std::uint32_t>(p[3]) << 24);
}

std::uint64_t read_le64(const std::uint8_t* p) noexcept {
    return static_cast<std::uint64_t>(read_le32(p)) | (static_cast<std::uint64_t>(read_le32(p + 4)) << 32);
}

std::uint16_t read_be16(const std::uint8_t* p) noexcept {
    return static_cast<std::uint16_t>((p[0] << 8) | p[1]);
}

std::uint32_t read_be32(const std::uint8_t* p) noexcept {
    return (static_cast<std::uint32_t>(p[0]) << 24) | (static_cast<std::uint32_t>(p[1]) << 16) | (static_cast<std::uint32_t>(p[2]) << 8) | static_cast<std::uint32_t>(p[3]);
}

void put_le16(std::uint8_t* p, std::uint16_t v) noexcept {
    p[0] = static_cast<std::uint8_t>(v & 0xFF);
    p[1] = static_cast<std::uint8_t>(v >> 8);
}

void put_le32(std::uint8_t* p, std::uint32_t v) noexcept {
    for (int i = 0; i < 4; ++i) p[i] = static_cast<std::uint8_t>((v >> (8 * i)) & 0xFF);
}

void put_le64(std::uint8_t* p, std::uint64_t v) noexcept {
    for (int i = 0; i < 8; ++i) p[i] = static_cast<std::uint8_t>((v >> (8 * i)) & 0xFF);
}

void append_le16(std::vector<std::uint8_t>& out, std::uint16_t v) {
    std::uint8_t b[2];
    put_le16(b, v);
    out.insert(out.end(), b, b + 2);
}

void append_le32(std::vector<std::uint8_t>& out, std::uint32_t v) {
    std::uint8_t b[4];
    put_le32(b, v);
    out.insert(out.end(), b, b + 4);
}

void append_tag(std::vector<std::uint8_t>& out, const char* tag) {
    out.insert(out.end(), tag, tag + 4);
}

} // namespace detail
} // namespace audio
} // namespace fizmo
