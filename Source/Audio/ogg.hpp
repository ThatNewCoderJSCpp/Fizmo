#ifndef FIZMO_AUDIO_OGG_HPP
#define FIZMO_AUDIO_OGG_HPP

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace fizmo {
namespace audio {

struct OggPage {
    std::uint64_t offset = 0;
    std::uint32_t serial = 0;
    std::uint32_t sequence = 0;
    std::int64_t granule = -1;
    std::uint8_t flags = 0;
    std::vector<std::uint8_t> lacing;
    std::vector<std::uint8_t> body;

    bool continued() const noexcept { return (flags & 1) != 0; }
    bool bos() const noexcept { return (flags & 2) != 0; }
    bool eos() const noexcept { return (flags & 4) != 0; }
    std::size_t size() const noexcept { return 27 + lacing.size() + body.size(); }
    std::uint64_t end() const noexcept { return offset + size(); }
    std::size_t packets_completed() const noexcept;
    std::size_t packets_started() const noexcept;
};

struct OggPacket {
    std::vector<std::uint8_t> data;
    std::uint32_t serial = 0;
    std::int64_t granule = -1;
    std::uint64_t page_offset = 0;
    bool bos = false;
    bool eos = false;
    bool last_on_page = false;
    bool after_gap = false;
};

namespace detail {

std::uint32_t ogg_crc(const std::uint8_t* data, std::size_t size, std::uint32_t crc = 0) noexcept;

} // namespace detail

bool is_ogg(const void* data, std::size_t size) noexcept;

class OggReader {
public:
    struct Source;

    static constexpr std::size_t max_packet_size = std::size_t(1) << 26;

    OggReader();
    ~OggReader();
    OggReader(OggReader&&) noexcept;
    OggReader& operator=(OggReader&&) noexcept;
    OggReader(const OggReader&) = delete;
    OggReader& operator=(const OggReader&) = delete;

    bool open(const std::string& path, std::string* error = nullptr);
    bool open(const void* data, std::size_t size, std::string* error = nullptr);
    void close();
    bool is_open() const noexcept;
    std::uint64_t size() const noexcept;

    bool read_page(OggPage& page);
    bool read_packet(OggPacket& packet);

    void select_stream(std::uint32_t serial);
    void select_all_streams();
    void set_limit(std::uint64_t end) noexcept { m_limit = end; }
    std::uint64_t limit() const noexcept { return m_limit; }

    bool seek(std::uint64_t offset);
    std::uint64_t tell() const noexcept { return m_offset; }

    bool page_at(std::uint64_t from, std::uint64_t end, OggPage& page);
    bool find_granule(std::uint32_t serial, std::int64_t target, std::uint64_t begin, std::uint64_t end, OggPage& page);
    bool last_granule_page(std::uint32_t serial, std::uint64_t begin, std::uint64_t end, OggPage& page);

    std::uint64_t crc_failures() const noexcept { return m_crc_failures; }

private:
    struct Stream {
        std::uint32_t serial = 0;
        std::uint32_t next_sequence = 0;
        bool known = false;
        bool in_packet = false;
        bool oversize = false;
        bool gap = true;
        std::vector<std::uint8_t> data;
    };

    std::size_t read_at(std::uint64_t offset, void* data, std::size_t bytes);
    bool find_capture(std::uint64_t& pos, std::uint64_t end);
    void reset_streams();
    Stream& stream_for(std::uint32_t serial);
    void absorb(const OggPage& page);

    std::unique_ptr<Source> m_source;
    std::uint64_t m_size = 0;
    std::uint64_t m_offset = 0;
    std::uint64_t m_limit = 0;
    std::uint64_t m_crc_failures = 0;
    bool m_filtered = false;
    std::uint32_t m_filter = 0;
    std::vector<Stream> m_streams;
    std::vector<OggPacket> m_ready;
    std::size_t m_ready_pos = 0;
    OggPage m_page;
    std::vector<std::uint8_t> m_scratch;
};

} // namespace audio
} // namespace fizmo

#endif // FIZMO_AUDIO_OGG_HPP
