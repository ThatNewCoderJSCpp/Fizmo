#ifndef FIZMO_VIDEO_DEMUX_INTERNAL_HPP
#define FIZMO_VIDEO_DEMUX_INTERNAL_HPP

#include "demux.hpp"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace fizmo {
namespace video {
namespace demux_detail {

class Source {
public:
    virtual ~Source() = default;
    virtual std::size_t read_at(std::uint64_t offset, void* data, std::size_t bytes) = 0;
    std::uint64_t size() const noexcept { return m_size; }
    const std::string& path() const noexcept { return m_path; }
    const std::uint8_t* memory() const noexcept { return m_memory; }

    bool read_exact(std::uint64_t offset, void* data, std::size_t bytes);
    bool read_vector(std::uint64_t offset, std::size_t bytes, std::vector<std::uint8_t>& out);

protected:
    std::uint64_t m_size = 0;
    std::string m_path;
    const std::uint8_t* m_memory = nullptr;
};

std::unique_ptr<Source> open_file_source(const std::string& path, std::string* error);
std::unique_ptr<Source> open_memory_source(const void* data, std::size_t size);
std::unique_ptr<Source> open_shared_source(std::shared_ptr<const std::vector<std::uint8_t>> data);

class ByteReader {
public:
    ByteReader() = default;
    ByteReader(const std::uint8_t* data, std::size_t size) noexcept : m_data(data), m_size(size) {}

    std::size_t size() const noexcept { return m_size; }
    std::size_t pos() const noexcept { return m_pos; }
    std::size_t left() const noexcept { return m_pos < m_size ? m_size - m_pos : 0; }
    bool failed() const noexcept { return m_fail; }
    bool ok() const noexcept { return !m_fail; }
    const std::uint8_t* data() const noexcept { return m_data; }
    const std::uint8_t* here() const noexcept { return m_data + (m_pos < m_size ? m_pos : m_size); }

    void seek(std::size_t pos) noexcept;
    void skip(std::size_t n) noexcept;
    std::uint8_t u8() noexcept;
    std::uint16_t be16() noexcept;
    std::uint32_t be24() noexcept;
    std::uint32_t be32() noexcept;
    std::uint64_t be64() noexcept;
    std::uint16_t le16() noexcept;
    std::uint32_t le32() noexcept;
    std::uint64_t le64() noexcept;
    std::uint64_t be_n(std::size_t bytes) noexcept;
    bool bytes(void* out, std::size_t n) noexcept;
    std::vector<std::uint8_t> vec(std::size_t n);
    std::string str(std::size_t n);

private:
    const std::uint8_t* m_data = nullptr;
    std::size_t m_size = 0;
    std::size_t m_pos = 0;
    bool m_fail = false;
};

class BitReader {
public:
    BitReader(const std::uint8_t* data, std::size_t size) noexcept : m_data(data), m_size(size) {}
    std::uint32_t bits(int n) noexcept;
    bool failed() const noexcept { return m_fail; }
    std::size_t bit_pos() const noexcept { return m_bit; }

private:
    const std::uint8_t* m_data;
    std::size_t m_size;
    std::size_t m_bit = 0;
    bool m_fail = false;
};

struct AacConfig {
    int object_type = 0;
    std::uint32_t sample_rate = 0;
    std::uint32_t extension_rate = 0;
    int channels = 0;
    std::uint32_t frame_length = 1024;
};

bool parse_aac_config(const std::uint8_t* data, std::size_t size, AacConfig& out) noexcept;
std::vector<std::uint8_t> make_aac_config(int object_type, std::uint32_t sample_rate, int channels);

std::uint32_t opus_packet_samples(const std::uint8_t* data, std::size_t size) noexcept;
std::uint32_t flac_frame_samples(const std::uint8_t* data, std::size_t size) noexcept;
std::uint32_t mpeg_audio_frame_samples(const std::uint8_t* data, std::size_t size, std::uint32_t* rate = nullptr, int* channels = nullptr) noexcept;
std::uint32_t ac3_frame_samples(const std::uint8_t* data, std::size_t size, std::uint32_t* rate = nullptr) noexcept;

struct VorbisState {
    bool valid = false;
    std::uint32_t blocksize[2] = {0, 0};
    std::uint32_t sample_rate = 0;
    int channels = 0;
    std::vector<std::uint8_t> mode_long;
    std::uint32_t previous = 0;

    bool init(const std::uint8_t* ident, std::size_t ident_size, const std::uint8_t* setup, std::size_t setup_size);
    void reset() noexcept { previous = blocksize[0]; }
    std::uint32_t packet_samples(const std::uint8_t* data, std::size_t size) noexcept;
};

std::vector<std::uint8_t> xiph_lace(const std::vector<std::vector<std::uint8_t>>& packets);
bool xiph_unlace(const std::uint8_t* data, std::size_t size, std::vector<std::vector<std::uint8_t>>& packets);

class DurationEstimator {
public:
    void init(const TrackInfo& track);
    void reset() noexcept;
    double seconds(const std::uint8_t* data, std::size_t size) noexcept;
    bool active() const noexcept { return m_kind != 0; }

private:
    int m_kind = 0;
    std::uint32_t m_rate = 0;
    std::uint32_t m_fixed = 0;
    VorbisState m_vorbis;
};

void parse_vorbis_comments(const std::uint8_t* data, std::size_t size, std::vector<std::pair<std::string, std::string>>& tags);
void add_tag(std::vector<std::pair<std::string, std::string>>& tags, const std::string& key, const std::string& value);
std::string to_lower(std::string s);
bool opus_head_info(const std::vector<std::uint8_t>& head, std::uint16_t& channels, std::uint16_t& pre_skip, std::uint32_t& rate) noexcept;

const std::size_t k_max_tracks = 128;
const std::size_t k_max_packet = std::size_t(1) << 28;

class Reader {
public:
    virtual ~Reader() = default;
    virtual bool open(Source& source, std::string* error) = 0;
    virtual bool read(Packet& out) = 0;
    virtual bool seek(double seconds, bool before) = 0;
    virtual bool rewind() { return seek(-1.0e6, true); }
    virtual void selection_changed(std::size_t index, bool on) { (void)index; (void)on; }

    std::vector<TrackInfo> tracks;
    std::vector<char> enabled;
    double duration = 0.0;
    std::string format;
    std::vector<std::pair<std::string, std::string>> tags;

    bool is_enabled(std::size_t index) const noexcept { return index < enabled.size() && enabled[index] != 0; }
    int reference_track() const noexcept;
};

struct SeekTarget {
    bool active = false;
    bool want_key = false;
    double time = 0.0;
};

bool passes_target(SeekTarget& target, const Packet& packet) noexcept;

std::unique_ptr<Reader> make_mp4_reader();
std::unique_ptr<Reader> make_matroska_reader();
std::unique_ptr<Reader> make_ogg_reader();
std::unique_ptr<Reader> make_hevc_es_reader();
bool is_hevc_annexb(const void* data, std::size_t size) noexcept;
std::unique_ptr<Reader> make_av1_es_reader();
bool is_av1_elementary(const void* data, std::size_t size) noexcept;
std::unique_ptr<Reader> make_ivf_reader();
bool is_ivf_vpx(const void* data, std::size_t size) noexcept;

} // namespace demux_detail
} // namespace video
} // namespace fizmo

#endif // FIZMO_VIDEO_DEMUX_INTERNAL_HPP
