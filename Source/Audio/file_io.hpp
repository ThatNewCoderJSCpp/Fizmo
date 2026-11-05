#ifndef FIZMO_AUDIO_FILE_IO_HPP
#define FIZMO_AUDIO_FILE_IO_HPP

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

namespace fizmo {
namespace audio {
namespace detail {

class File {
public:
    File() = default;
    ~File();
    File(const File&) = delete;
    File& operator=(const File&) = delete;
    File(File&& other) noexcept;
    File& operator=(File&& other) noexcept;

    bool open(const std::string& path, const char* mode);
    void close();
    bool is_open() const noexcept { return m_file != nullptr; }

    std::size_t read(void* data, std::size_t bytes);
    bool read_exact(void* data, std::size_t bytes);
    bool write(const void* data, std::size_t bytes);
    bool seek(std::int64_t offset, int origin = SEEK_SET);
    std::int64_t tell();
    std::int64_t size();
    bool flush();

private:
    std::FILE* m_file = nullptr;
};

bool read_whole_file(const std::string& path, std::vector<std::uint8_t>& out, std::string* error);
bool write_whole_file(const std::string& path, const void* data, std::size_t size, std::string* error);

std::uint16_t read_le16(const std::uint8_t* p) noexcept;
std::uint32_t read_le32(const std::uint8_t* p) noexcept;
std::uint64_t read_le64(const std::uint8_t* p) noexcept;
std::uint16_t read_be16(const std::uint8_t* p) noexcept;
std::uint32_t read_be32(const std::uint8_t* p) noexcept;
void put_le16(std::uint8_t* p, std::uint16_t v) noexcept;
void put_le32(std::uint8_t* p, std::uint32_t v) noexcept;
void put_le64(std::uint8_t* p, std::uint64_t v) noexcept;
void append_le16(std::vector<std::uint8_t>& out, std::uint16_t v);
void append_le32(std::vector<std::uint8_t>& out, std::uint32_t v);
void append_tag(std::vector<std::uint8_t>& out, const char* tag);

} // namespace detail
} // namespace audio
} // namespace fizmo

#endif // FIZMO_AUDIO_FILE_IO_HPP
