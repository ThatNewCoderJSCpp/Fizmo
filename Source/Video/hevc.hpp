#ifndef FIZMO_VIDEO_HEVC_HPP
#define FIZMO_VIDEO_HEVC_HPP

#include "frame.hpp"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace fizmo {
namespace video {

struct HevcInfo {
    int profile_idc = 0;
    int level_idc = 0;
    int tier = 0;
    int chroma_format_idc = 1;
    int bit_depth = 8;
    int bit_depth_chroma = 8;
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    std::uint32_t coded_width = 0;
    std::uint32_t coded_height = 0;
    std::uint32_t sar_num = 1;
    std::uint32_t sar_den = 1;
    bool progressive = true;
    bool field_coded = false;
    int max_num_reorder_pics = -1;
    int dpb_frames = 0;
    int temporal_layers = 1;
    double frame_rate = 0.0;
    ColorInfo color;
};

struct HevcStats {
    std::uint64_t pictures = 0;
    std::uint64_t skipped_rasl = 0;
    std::uint64_t hash_checked = 0;
    std::uint64_t hash_mismatches = 0;
    std::uint64_t missing_refs = 0;
    std::uint64_t fields_woven = 0;
};

bool is_annexb_hevc(const void* data, std::size_t size) noexcept;

class HevcDecoder {
public:
    struct State;

    HevcDecoder();
    ~HevcDecoder();
    HevcDecoder(HevcDecoder&&) noexcept;
    HevcDecoder& operator=(HevcDecoder&&) noexcept;
    HevcDecoder(const HevcDecoder&) = delete;
    HevcDecoder& operator=(const HevcDecoder&) = delete;

    bool open(const std::uint8_t* hvcc, std::size_t size, std::string* error = nullptr);
    bool open_annexb();
    bool decode(const std::uint8_t* data, std::size_t size, double pts, std::string* error = nullptr);
    bool receive(VideoFrame& out);
    void flush();
    void reset();
    void close();
    bool is_open() const noexcept;
    bool is_disposable(const std::uint8_t* data, std::size_t size) const noexcept;

    void set_verify_hash(bool on) noexcept;
    void set_weave_fields(bool on) noexcept;
    const HevcInfo& info() const noexcept;
    const HevcStats& stats() const noexcept;
    std::uint64_t frames_decoded() const noexcept;
    std::uint64_t error_count() const noexcept;

private:
    std::unique_ptr<State> m_state;
};

class HevcFileReader {
public:
    HevcFileReader() = default;

    bool open(const std::string& path, std::string* error = nullptr);
    bool open(const void* data, std::size_t size);
    void close();
    bool is_open() const noexcept { return m_open; }
    bool read(std::vector<std::uint8_t>& access_unit);
    void rewind() noexcept { m_pos = m_first; }

private:
    std::size_t next_start(std::size_t from, std::size_t* code_size) const noexcept;

    std::vector<std::uint8_t> m_data;
    std::size_t m_pos = 0;
    std::size_t m_first = 0;
    bool m_open = false;
};

} // namespace video
} // namespace fizmo

#endif // FIZMO_VIDEO_HEVC_HPP
