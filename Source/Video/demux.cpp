#include "demux.hpp"
#include "demux_internal.hpp"

#include "../Audio/file_io.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <utility>

namespace fizmo {
namespace video {
namespace demux_detail {

namespace {

const std::size_t k_cache_size = 65536;

class FileSource : public Source {
public:
    bool open(const std::string& path) {
        if (!m_file.open(path, "rb")) return false;
        const std::int64_t s = m_file.size();
        m_size = s > 0 ? static_cast<std::uint64_t>(s) : 0;
        m_path = path;
        return true;
    }

    std::size_t read_at(std::uint64_t offset, void* data, std::size_t bytes) override {
        if (offset >= m_size || bytes == 0) return 0;
        bytes = static_cast<std::size_t>(std::min<std::uint64_t>(bytes, m_size - offset));
        std::uint8_t* out = static_cast<std::uint8_t*>(data);
        std::size_t done = 0;
        while (done < bytes) {
            const std::uint64_t at = offset + done;
            if (at >= m_cache_offset && at < m_cache_offset + m_cache_length) {
                const std::size_t skip = static_cast<std::size_t>(at - m_cache_offset);
                const std::size_t n = std::min(bytes - done, m_cache_length - skip);
                std::memcpy(out + done, m_cache.data() + skip, n);
                done += n;
                continue;
            }
            if (bytes - done >= k_cache_size) {
                if (!m_file.seek(static_cast<std::int64_t>(at))) break;
                done += m_file.read(out + done, bytes - done);
                break;
            }
            m_cache.resize(k_cache_size);
            m_cache_length = 0;
            m_cache_offset = at;
            if (!m_file.seek(static_cast<std::int64_t>(at))) break;
            m_cache_length = m_file.read(m_cache.data(), k_cache_size);
            if (m_cache_length == 0) break;
        }
        return done;
    }

private:
    audio::detail::File m_file;
    std::vector<std::uint8_t> m_cache;
    std::uint64_t m_cache_offset = 0;
    std::size_t m_cache_length = 0;
};

class MemorySource : public Source {
public:
    explicit MemorySource(const void* data, std::size_t size) : m_data(static_cast<const std::uint8_t*>(data), static_cast<const std::uint8_t*>(data) + size) {
        m_size = size;
        m_memory = m_data.data();
    }

    std::size_t read_at(std::uint64_t offset, void* out, std::size_t bytes) override {
        if (offset >= m_size || bytes == 0) return 0;
        const std::size_t n = static_cast<std::size_t>(std::min<std::uint64_t>(bytes, m_size - offset));
        std::memcpy(out, m_data.data() + offset, n);
        return n;
    }

private:
    std::vector<std::uint8_t> m_data;
};

class SharedSource : public Source {
public:
    explicit SharedSource(std::shared_ptr<const std::vector<std::uint8_t>> data) : m_data(std::move(data)) {
        m_size = m_data ? m_data->size() : 0;
        m_memory = m_data && !m_data->empty() ? m_data->data() : nullptr;
    }

    std::size_t read_at(std::uint64_t offset, void* out, std::size_t bytes) override {
        if (offset >= m_size || bytes == 0) return 0;
        const std::size_t n = static_cast<std::size_t>(std::min<std::uint64_t>(bytes, m_size - offset));
        std::memcpy(out, m_data->data() + offset, n);
        return n;
    }

private:
    std::shared_ptr<const std::vector<std::uint8_t>> m_data;
};

const std::uint32_t k_aac_rates[13] = {96000, 88200, 64000, 48000, 44100, 32000, 24000, 22050, 16000, 12000, 11025, 8000, 7350};

std::uint32_t aac_rate(BitReader& br) noexcept {
    const std::uint32_t index = br.bits(4);
    if (index == 15) return br.bits(24);
    return index < 13 ? k_aac_rates[index] : 0;
}

int aac_object(BitReader& br) noexcept {
    int aot = static_cast<int>(br.bits(5));
    if (aot == 31) aot = 32 + static_cast<int>(br.bits(6));
    return aot;
}

int ilog(std::uint32_t v) noexcept {
    int n = 0;
    while (v) {
        ++n;
        v >>= 1;
    }
    return n;
}

std::uint32_t bit_field(const std::uint8_t* data, std::size_t start, int n) noexcept {
    std::uint32_t v = 0;
    for (int k = 0; k < n; ++k) {
        const std::size_t i = start + static_cast<std::size_t>(k);
        v |= static_cast<std::uint32_t>((data[i >> 3] >> (i & 7)) & 1) << k;
    }
    return v;
}

} // namespace

bool Source::read_exact(std::uint64_t offset, void* data, std::size_t bytes) {
    if (bytes == 0) return true;
    return read_at(offset, data, bytes) == bytes;
}

bool Source::read_vector(std::uint64_t offset, std::size_t bytes, std::vector<std::uint8_t>& out) {
    if (offset > m_size || bytes > m_size - offset) return false;
    out.resize(bytes);
    return read_exact(offset, out.data(), bytes);
}

std::unique_ptr<Source> open_file_source(const std::string& path, std::string* error) {
    std::unique_ptr<FileSource> src(new FileSource());
    if (!src->open(path)) {
        if (error) *error = "can't open " + path;
        return nullptr;
    }
    return std::unique_ptr<Source>(src.release());
}

std::unique_ptr<Source> open_memory_source(const void* data, std::size_t size) {
    return std::unique_ptr<Source>(new MemorySource(data, size));
}

std::unique_ptr<Source> open_shared_source(std::shared_ptr<const std::vector<std::uint8_t>> data) {
    return std::unique_ptr<Source>(new SharedSource(std::move(data)));
}

void ByteReader::seek(std::size_t pos) noexcept {
    if (pos > m_size) {
        m_fail = true;
        m_pos = m_size;
    } else {
        m_pos = pos;
    }
}

void ByteReader::skip(std::size_t n) noexcept {
    if (n > left()) {
        m_fail = true;
        m_pos = m_size;
    } else {
        m_pos += n;
    }
}

std::uint8_t ByteReader::u8() noexcept {
    if (m_pos >= m_size) {
        m_fail = true;
        return 0;
    }
    return m_data[m_pos++];
}

std::uint64_t ByteReader::be_n(std::size_t bytes) noexcept {
    if (bytes > left()) {
        m_fail = true;
        m_pos = m_size;
        return 0;
    }
    std::uint64_t v = 0;
    for (std::size_t i = 0; i < bytes; ++i) v = (v << 8) | m_data[m_pos + i];
    m_pos += bytes;
    return v;
}

std::uint16_t ByteReader::be16() noexcept { return static_cast<std::uint16_t>(be_n(2)); }
std::uint32_t ByteReader::be24() noexcept { return static_cast<std::uint32_t>(be_n(3)); }
std::uint32_t ByteReader::be32() noexcept { return static_cast<std::uint32_t>(be_n(4)); }
std::uint64_t ByteReader::be64() noexcept { return be_n(8); }

std::uint16_t ByteReader::le16() noexcept {
    const std::uint16_t v = be16();
    return static_cast<std::uint16_t>((v >> 8) | (v << 8));
}

std::uint32_t ByteReader::le32() noexcept {
    if (left() < 4) {
        m_fail = true;
        m_pos = m_size;
        return 0;
    }
    const std::uint32_t v = audio::detail::read_le32(m_data + m_pos);
    m_pos += 4;
    return v;
}

std::uint64_t ByteReader::le64() noexcept {
    const std::uint64_t lo = le32();
    const std::uint64_t hi = le32();
    return lo | (hi << 32);
}

bool ByteReader::bytes(void* out, std::size_t n) noexcept {
    if (n > left()) {
        m_fail = true;
        m_pos = m_size;
        return false;
    }
    if (n) std::memcpy(out, m_data + m_pos, n);
    m_pos += n;
    return true;
}

std::vector<std::uint8_t> ByteReader::vec(std::size_t n) {
    if (n > left()) {
        m_fail = true;
        m_pos = m_size;
        return std::vector<std::uint8_t>();
    }
    std::vector<std::uint8_t> v(m_data + m_pos, m_data + m_pos + n);
    m_pos += n;
    return v;
}

std::string ByteReader::str(std::size_t n) {
    if (n > left()) {
        m_fail = true;
        m_pos = m_size;
        return std::string();
    }
    std::string s(reinterpret_cast<const char*>(m_data + m_pos), n);
    m_pos += n;
    const std::size_t z = s.find('\0');
    if (z != std::string::npos) s.resize(z);
    return s;
}

std::uint32_t BitReader::bits(int n) noexcept {
    std::uint32_t v = 0;
    for (int i = 0; i < n; ++i) {
        if ((m_bit >> 3) >= m_size) {
            m_fail = true;
            return 0;
        }
        v = (v << 1) | static_cast<std::uint32_t>((m_data[m_bit >> 3] >> (7 - (m_bit & 7))) & 1);
        ++m_bit;
    }
    return v;
}

bool parse_aac_config(const std::uint8_t* data, std::size_t size, AacConfig& out) noexcept {
    if (!data || size < 2) return false;
    BitReader br(data, size);
    AacConfig c;
    c.object_type = aac_object(br);
    c.sample_rate = aac_rate(br);
    c.channels = static_cast<int>(br.bits(4));
    int aot = c.object_type;
    if (aot == 5 || aot == 29) {
        c.extension_rate = aac_rate(br);
        aot = aac_object(br);
        if (aot == 22) br.bits(4);
    }
    switch (aot) {
        case 1: case 2: case 3: case 4: case 6: case 7: case 17: case 19: case 20: case 21: case 22: case 23:
            if (br.bits(1)) c.frame_length = 960;
            break;
        default:
            break;
    }
    if (br.failed() || c.sample_rate == 0) return false;
    if (c.channels == 7) c.channels = 8;
    else if (c.channels > 7) c.channels = 0;
    out = c;
    return true;
}

std::vector<std::uint8_t> make_aac_config(int object_type, std::uint32_t sample_rate, int channels) {
    int index = 15;
    for (int i = 0; i < 13; ++i) {
        if (k_aac_rates[i] == sample_rate) index = i;
    }
    if (object_type < 1 || object_type > 30) object_type = 2;
    if (channels < 0 || channels > 7) channels = 2;
    std::vector<std::uint8_t> out;
    if (index == 15) {
        std::uint64_t v = (static_cast<std::uint64_t>(object_type) << 35) | (std::uint64_t(15) << 31) | (static_cast<std::uint64_t>(sample_rate & 0xFFFFFF) << 7) | (static_cast<std::uint64_t>(channels) << 3);
        for (int i = 4; i >= 0; --i) out.push_back(static_cast<std::uint8_t>((v >> (8 * i)) & 0xFF));
    } else {
        const std::uint32_t v = (static_cast<std::uint32_t>(object_type) << 11) | (static_cast<std::uint32_t>(index) << 7) | (static_cast<std::uint32_t>(channels) << 3);
        out.push_back(static_cast<std::uint8_t>(v >> 8));
        out.push_back(static_cast<std::uint8_t>(v & 0xFF));
    }
    return out;
}

std::uint32_t opus_packet_samples(const std::uint8_t* data, std::size_t size) noexcept {
    if (!data || size < 1) return 0;
    const std::uint8_t toc = data[0];
    const int config = toc >> 3;
    std::uint32_t frame;
    if (config < 12) {
        const std::uint32_t silk[4] = {480, 960, 1920, 2880};
        frame = silk[config & 3];
    } else if (config < 16) {
        frame = (config & 1) ? 960 : 480;
    } else {
        const std::uint32_t celt[4] = {120, 240, 480, 960};
        frame = celt[config & 3];
    }
    std::uint32_t count;
    switch (toc & 3) {
        case 0: count = 1; break;
        case 1: case 2: count = 2; break;
        default:
            if (size < 2) return 0;
            count = data[1] & 0x3F;
            break;
    }
    const std::uint32_t total = frame * count;
    return total > 5760 ? 0 : total;
}

std::uint32_t flac_frame_samples(const std::uint8_t* data, std::size_t size) noexcept {
    if (!data || size < 6) return 0;
    if (data[0] != 0xFF || (data[1] & 0xFE) != 0xF8) return 0;
    const int code = data[2] >> 4;
    std::size_t pos = 4;
    const std::uint8_t lead = data[pos];
    int extra = 0;
    if (lead & 0x80) {
        std::uint8_t mask = 0x40;
        while ((lead & mask) && extra < 6) {
            ++extra;
            mask = static_cast<std::uint8_t>(mask >> 1);
        }
    }
    pos += 1 + static_cast<std::size_t>(extra);
    if (code == 0) return 0;
    if (code == 1) return 192;
    if (code >= 2 && code <= 5) return 576u << (code - 2);
    if (code == 6) {
        if (pos >= size) return 0;
        return static_cast<std::uint32_t>(data[pos]) + 1;
    }
    if (code == 7) {
        if (pos + 1 >= size) return 0;
        return ((static_cast<std::uint32_t>(data[pos]) << 8) | data[pos + 1]) + 1;
    }
    return 256u << (code - 8);
}

std::uint32_t mpeg_audio_frame_samples(const std::uint8_t* data, std::size_t size, std::uint32_t* rate, int* channels) noexcept {
    if (!data || size < 4) return 0;
    if (data[0] != 0xFF || (data[1] & 0xE0) != 0xE0) return 0;
    const int version = (data[1] >> 3) & 3;
    const int layer = (data[1] >> 1) & 3;
    const int rate_index = (data[2] >> 2) & 3;
    if (version == 1 || layer == 0 || rate_index == 3) return 0;
    const std::uint32_t base[3] = {44100, 48000, 32000};
    std::uint32_t r = base[rate_index];
    if (version == 2) r /= 2;
    else if (version == 0) r /= 4;
    if (rate) *rate = r;
    if (channels) *channels = (data[3] >> 6) == 3 ? 1 : 2;
    if (layer == 3) return 384;
    if (layer == 2) return 1152;
    return version == 3 ? 1152 : 576;
}

std::uint32_t ac3_frame_samples(const std::uint8_t* data, std::size_t size, std::uint32_t* rate) noexcept {
    if (!data || size < 6) return 0;
    if (data[0] != 0x0B || data[1] != 0x77) return 0;
    const int bsid = data[5] >> 3;
    if (bsid <= 10) {
        const int fscod = data[4] >> 6;
        const std::uint32_t rates[3] = {48000, 44100, 32000};
        if (fscod == 3) return 0;
        if (rate) *rate = rates[fscod] >> (bsid > 8 ? bsid - 8 : 0);
        return 1536;
    }
    if (bsid > 16) return 0;
    const int fscod = data[4] >> 6;
    if (fscod == 3) {
        const int fscod2 = (data[4] >> 4) & 3;
        const std::uint32_t half[3] = {24000, 22050, 16000};
        if (fscod2 == 3) return 0;
        if (rate) *rate = half[fscod2];
        return 1536;
    }
    const std::uint32_t rates[3] = {48000, 44100, 32000};
    if (rate) *rate = rates[fscod];
    const std::uint32_t blocks[4] = {1, 2, 3, 6};
    return blocks[(data[4] >> 4) & 3] * 256;
}

bool VorbisState::init(const std::uint8_t* ident, std::size_t ident_size, const std::uint8_t* setup, std::size_t setup_size) {
    valid = false;
    mode_long.clear();
    if (!ident || ident_size < 30 || ident[0] != 1 || std::memcmp(ident + 1, "vorbis", 6) != 0) return false;
    channels = ident[11];
    sample_rate = audio::detail::read_le32(ident + 12);
    const int b0 = ident[28] & 15;
    const int b1 = ident[28] >> 4;
    if (b0 < 6 || b0 > 13 || b1 < 6 || b1 > 13 || b0 > b1) return false;
    blocksize[0] = 1u << b0;
    blocksize[1] = 1u << b1;
    previous = blocksize[0];
    if (!setup || setup_size < 8 || setup[0] != 5) return false;
    const std::size_t total = setup_size * 8;
    std::size_t f = total;
    while (f > 0 && bit_field(setup, f - 1, 1) == 0) --f;
    if (f == 0) return false;
    std::size_t cur = f - 1;
    std::vector<std::uint8_t> reversed;
    std::size_t last_valid = 0;
    while (cur >= 41 + 6) {
        const std::uint32_t mapping = bit_field(setup, cur - 8, 8);
        const std::uint32_t transform = bit_field(setup, cur - 24, 16);
        const std::uint32_t window = bit_field(setup, cur - 40, 16);
        const std::uint32_t flag = bit_field(setup, cur - 41, 1);
        if (mapping > 63 || transform != 0 || window != 0) break;
        cur -= 41;
        reversed.push_back(static_cast<std::uint8_t>(flag));
        if (reversed.size() > 64) break;
        const std::uint32_t count = bit_field(setup, cur - 6, 6) + 1;
        if (count == reversed.size()) last_valid = reversed.size();
    }
    if (last_valid == 0) return false;
    mode_long.resize(last_valid);
    for (std::size_t i = 0; i < last_valid; ++i) mode_long[i] = reversed[last_valid - 1 - i];
    valid = true;
    return true;
}

std::uint32_t VorbisState::packet_samples(const std::uint8_t* data, std::size_t size) noexcept {
    if (!valid || !data || size == 0 || (data[0] & 1)) return 0;
    const int bits = ilog(static_cast<std::uint32_t>(mode_long.size() - 1));
    if (static_cast<std::size_t>(bits + 1) > size * 8) return 0;
    const std::uint32_t mode = bit_field(data, 1, bits);
    if (mode >= mode_long.size()) return 0;
    const std::uint32_t current = blocksize[mode_long[mode] ? 1 : 0];
    const std::uint32_t samples = (previous + current) / 4;
    previous = current;
    return samples;
}

std::vector<std::uint8_t> xiph_lace(const std::vector<std::vector<std::uint8_t>>& packets) {
    std::vector<std::uint8_t> out;
    if (packets.empty()) return out;
    out.push_back(static_cast<std::uint8_t>(packets.size() - 1));
    for (std::size_t i = 0; i + 1 < packets.size(); ++i) {
        std::size_t n = packets[i].size();
        while (n >= 255) {
            out.push_back(255);
            n -= 255;
        }
        out.push_back(static_cast<std::uint8_t>(n));
    }
    for (const auto& p : packets) out.insert(out.end(), p.begin(), p.end());
    return out;
}

bool xiph_unlace(const std::uint8_t* data, std::size_t size, std::vector<std::vector<std::uint8_t>>& packets) {
    packets.clear();
    if (!data || size < 1) return false;
    const std::size_t count = static_cast<std::size_t>(data[0]) + 1;
    std::size_t pos = 1;
    std::vector<std::size_t> sizes;
    std::size_t used = 0;
    for (std::size_t i = 0; i + 1 < count; ++i) {
        std::size_t n = 0;
        for (;;) {
            if (pos >= size) return false;
            const std::uint8_t v = data[pos++];
            n += v;
            if (v < 255) break;
        }
        sizes.push_back(n);
        used += n;
        if (used > size) return false;
    }
    if (pos + used > size) return false;
    sizes.push_back(size - pos - used);
    for (std::size_t n : sizes) {
        packets.emplace_back(data + pos, data + pos + n);
        pos += n;
    }
    return true;
}

void DurationEstimator::init(const TrackInfo& track) {
    m_kind = 0;
    m_rate = track.sample_rate;
    m_fixed = 0;
    if (track.type != MediaType::audio) return;
    if (track.codec == "opus") {
        m_kind = 1;
        m_rate = 48000;
    } else if (track.codec == "aac") {
        AacConfig c;
        m_fixed = 1024;
        if (parse_aac_config(track.codec_config.data(), track.codec_config.size(), c)) {
            m_fixed = c.frame_length;
            m_rate = c.sample_rate;
        }
        m_kind = 2;
    } else if (track.codec == "mp3" || track.codec == "mp2" || track.codec == "mp1") {
        m_kind = 3;
    } else if (track.codec == "ac3" || track.codec == "eac3") {
        m_kind = 4;
    } else if (track.codec == "flac") {
        m_kind = 5;
    } else if (track.codec.compare(0, 4, "pcm_") == 0 && track.channels) {
        const std::string& c = track.codec;
        std::uint32_t bytes = 2;
        if (c == "pcm_u8" || c == "pcm_s8" || c == "pcm_alaw" || c == "pcm_mulaw") bytes = 1;
        else if (c.find("24") != std::string::npos) bytes = 3;
        else if (c.find("32") != std::string::npos) bytes = 4;
        else if (c.find("64") != std::string::npos) bytes = 8;
        m_fixed = bytes * track.channels;
        m_kind = 7;
    } else if (track.codec == "vorbis") {
        std::vector<std::vector<std::uint8_t>> headers;
        if (xiph_unlace(track.codec_config.data(), track.codec_config.size(), headers) && headers.size() >= 3 &&
            m_vorbis.init(headers[0].data(), headers[0].size(), headers[2].data(), headers[2].size())) {
            m_kind = 6;
            if (m_vorbis.sample_rate) m_rate = m_vorbis.sample_rate;
        }
    }
}

void DurationEstimator::reset() noexcept {
    m_vorbis.reset();
}

double DurationEstimator::seconds(const std::uint8_t* data, std::size_t size) noexcept {
    std::uint32_t samples = 0;
    std::uint32_t rate = m_rate;
    switch (m_kind) {
        case 1: samples = opus_packet_samples(data, size); break;
        case 2: samples = m_fixed; break;
        case 3: samples = mpeg_audio_frame_samples(data, size, &rate); break;
        case 4: samples = ac3_frame_samples(data, size, &rate); break;
        case 5: samples = flac_frame_samples(data, size); break;
        case 6: samples = m_vorbis.packet_samples(data, size); break;
        case 7: samples = m_fixed ? static_cast<std::uint32_t>(std::min<std::size_t>(size / m_fixed, 0xFFFFFFFFu)) : 0; break;
        default: return 0.0;
    }
    if (rate == 0) return 0.0;
    return static_cast<double>(samples) / rate;
}

std::string to_lower(std::string s) {
    for (char& c : s) {
        if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
    }
    return s;
}

void add_tag(std::vector<std::pair<std::string, std::string>>& tags, const std::string& key, const std::string& value) {
    if (key.empty()) return;
    for (auto& t : tags) {
        if (t.first == key) {
            if (t.second != value) t.second += ";" + value;
            return;
        }
    }
    if (tags.size() < 1024) tags.emplace_back(key, value);
}

void parse_vorbis_comments(const std::uint8_t* data, std::size_t size, std::vector<std::pair<std::string, std::string>>& tags) {
    ByteReader r(data, size);
    const std::uint32_t vendor = r.le32();
    r.skip(vendor);
    const std::uint32_t count = r.le32();
    for (std::uint32_t i = 0; i < count && r.ok(); ++i) {
        const std::uint32_t len = r.le32();
        if (len > r.left()) break;
        const std::string entry(reinterpret_cast<const char*>(r.here()), len);
        r.skip(len);
        const std::size_t eq = entry.find('=');
        if (eq == std::string::npos || eq == 0) continue;
        std::string key = to_lower(entry.substr(0, eq));
        if (key == "tracknumber") key = "track";
        else if (key == "discnumber") key = "disc";
        else if (key == "albumartist") key = "album_artist";
        add_tag(tags, key, entry.substr(eq + 1));
    }
}

bool opus_head_info(const std::vector<std::uint8_t>& head, std::uint16_t& channels, std::uint16_t& pre_skip, std::uint32_t& rate) noexcept {
    if (head.size() < 19 || std::memcmp(head.data(), "OpusHead", 8) != 0) return false;
    channels = head[9];
    pre_skip = audio::detail::read_le16(head.data() + 10);
    rate = audio::detail::read_le32(head.data() + 12);
    return true;
}

int Reader::reference_track() const noexcept {
    int audio = -1;
    int any = -1;
    for (std::size_t i = 0; i < tracks.size(); ++i) {
        if (!is_enabled(i)) continue;
        if (tracks[i].type == MediaType::video) return static_cast<int>(i);
        if (tracks[i].type == MediaType::audio && audio < 0) audio = static_cast<int>(i);
        if (any < 0) any = static_cast<int>(i);
    }
    return audio >= 0 ? audio : any;
}

bool passes_target(SeekTarget& target, const Packet& packet) noexcept {
    if (!target.active) return true;
    const double eps = 1e-7;
    if (target.want_key) {
        if (packet.keyframe && packet.pts >= target.time - eps) {
            target.active = false;
            return true;
        }
        return false;
    }
    if (packet.duration > 0.0 ? packet.pts + packet.duration > target.time + eps : packet.pts >= target.time - eps) {
        target.active = false;
        return true;
    }
    return false;
}

} // namespace demux_detail

const char* media_type_name(MediaType type) noexcept {
    switch (type) {
        case MediaType::video: return "video";
        case MediaType::audio: return "audio";
        case MediaType::subtitle: return "subtitle";
        default: return "other";
    }
}

bool is_mp4(const void* data, std::size_t size) noexcept {
    if (!data || size < 8) return false;
    const std::uint8_t* p = static_cast<const std::uint8_t*>(data);
    static const char* const types[] = {"ftyp", "moov", "mdat", "free", "skip", "wide", "pnot", "styp", "sidx", "moof", nullptr};
    for (int i = 0; types[i]; ++i) {
        if (std::memcmp(p + 4, types[i], 4) == 0) {
            const std::uint32_t box = audio::detail::read_be32(p);
            return box == 0 || box == 1 || box >= 8;
        }
    }
    return false;
}

bool is_matroska(const void* data, std::size_t size) noexcept {
    if (!data || size < 4) return false;
    const std::uint8_t* p = static_cast<const std::uint8_t*>(data);
    return p[0] == 0x1A && p[1] == 0x45 && p[2] == 0xDF && p[3] == 0xA3;
}

bool is_ogg_container(const void* data, std::size_t size) noexcept {
    if (!data || size < 27) return false;
    const std::uint8_t* p = static_cast<const std::uint8_t*>(data);
    return std::memcmp(p, "OggS", 4) == 0 && p[4] == 0;
}

std::string detect_container(const void* data, std::size_t size) {
    if (is_matroska(data, size)) {
        const std::uint8_t* p = static_cast<const std::uint8_t*>(data);
        const std::size_t n = std::min<std::size_t>(size, 64);
        for (std::size_t i = 4; i + 7 <= n; ++i) {
            if (p[i] == 0x42 && p[i + 1] == 0x82) {
                const std::uint8_t first = p[i + 2];
                std::size_t width = 1;
                while (width <= 8 && !(first & (0x80 >> (width - 1)))) ++width;
                if (width > 8) break;
                std::uint64_t len = first & (0xFF >> width);
                for (std::size_t k = 1; k < width && i + 2 + k < n; ++k) len = (len << 8) | p[i + 2 + k];
                const std::size_t at = i + 2 + width;
                if (len >= 4 && at + 4 <= n && std::memcmp(p + at, "webm", 4) == 0) return "webm";
                break;
            }
        }
        return "matroska";
    }
    if (is_ogg_container(data, size)) return "ogg";
    if (is_mp4(data, size)) return "mp4";
    if (demux_detail::is_hevc_annexb(data, size)) return "hevc";
    if (demux_detail::is_av1_elementary(data, size)) return "av1";
    if (demux_detail::is_ivf_vpx(data, size)) return "ivf";
    return std::string();
}

struct Demuxer::Impl {
    std::unique_ptr<demux_detail::Source> source;
    std::unique_ptr<demux_detail::Reader> reader;
    std::vector<TrackInfo> empty;

    bool start(std::unique_ptr<demux_detail::Source> src, std::string* error) {
        source = std::move(src);
        std::uint8_t head[64];
        const std::size_t got = source->read_at(0, head, sizeof(head));
        const std::string kind = detect_container(head, got);
        if (kind == "matroska" || kind == "webm") {
            reader = demux_detail::make_matroska_reader();
        } else if (kind == "ogg") {
            reader = demux_detail::make_ogg_reader();
        } else if (kind == "mp4") {
            reader = demux_detail::make_mp4_reader();
        } else if (kind == "hevc") {
            reader = demux_detail::make_hevc_es_reader();
        } else if (kind == "av1") {
            reader = demux_detail::make_av1_es_reader();
        } else if (kind == "ivf") {
            reader = demux_detail::make_ivf_reader();
        } else {
            if (error) *error = got == 0 ? "empty file" : "unknown container format";
            source.reset();
            return false;
        }
        if (!reader->open(*source, error)) {
            reader.reset();
            source.reset();
            return false;
        }
        if (reader->enabled.size() != reader->tracks.size()) reader->enabled.assign(reader->tracks.size(), 1);
        return true;
    }
};

Demuxer::Demuxer() = default;
Demuxer::~Demuxer() = default;
Demuxer::Demuxer(Demuxer&&) noexcept = default;
Demuxer& Demuxer::operator=(Demuxer&&) noexcept = default;

bool Demuxer::open(const std::string& path, std::string* error) {
    close();
    std::unique_ptr<demux_detail::Source> src = demux_detail::open_file_source(path, error);
    if (!src) return false;
    std::unique_ptr<Impl> impl(new Impl());
    if (!impl->start(std::move(src), error)) return false;
    m_impl = std::move(impl);
    return true;
}

bool Demuxer::open(const void* data, std::size_t size, std::string* error) {
    close();
    if (!data && size) {
        if (error) *error = "no data";
        return false;
    }
    std::unique_ptr<Impl> impl(new Impl());
    if (!impl->start(demux_detail::open_memory_source(data, size), error)) return false;
    m_impl = std::move(impl);
    return true;
}

bool Demuxer::open_shared(std::shared_ptr<const std::vector<std::uint8_t>> data, std::string* error) {
    close();
    if (!data) {
        if (error) *error = "no data";
        return false;
    }
    std::unique_ptr<Impl> impl(new Impl());
    if (!impl->start(demux_detail::open_shared_source(std::move(data)), error)) return false;
    m_impl = std::move(impl);
    return true;
}

void Demuxer::close() {
    m_impl.reset();
}

bool Demuxer::is_open() const noexcept {
    return m_impl != nullptr;
}

const std::vector<TrackInfo>& Demuxer::tracks() const noexcept {
    static const std::vector<TrackInfo> none;
    return m_impl ? m_impl->reader->tracks : none;
}

double Demuxer::duration() const noexcept {
    return m_impl ? m_impl->reader->duration : 0.0;
}

std::string Demuxer::format_name() const {
    return m_impl ? m_impl->reader->format : std::string();
}

std::vector<std::pair<std::string, std::string>> Demuxer::tags() const {
    return m_impl ? m_impl->reader->tags : std::vector<std::pair<std::string, std::string>>();
}

int Demuxer::best_track(MediaType type) const noexcept {
    if (!m_impl) return -1;
    const std::vector<TrackInfo>& t = m_impl->reader->tracks;
    int first = -1;
    for (std::size_t i = 0; i < t.size(); ++i) {
        if (t[i].type != type) continue;
        if (t[i].codec.empty()) continue;
        if (t[i].is_default) return static_cast<int>(i);
        if (first < 0) first = static_cast<int>(i);
    }
    return first;
}

void Demuxer::select(std::uint32_t track, bool on) {
    if (!m_impl || track >= m_impl->reader->enabled.size()) return;
    const bool was = m_impl->reader->enabled[track] != 0;
    m_impl->reader->enabled[track] = on ? 1 : 0;
    if (was != on) m_impl->reader->selection_changed(track, on);
}

bool Demuxer::selected(std::uint32_t track) const noexcept {
    return m_impl && m_impl->reader->is_enabled(track);
}

bool Demuxer::read(Packet& out) {
    if (!m_impl) return false;
    return m_impl->reader->read(out);
}

bool Demuxer::seek(double seconds, bool to_keyframe_before) {
    if (!m_impl || !(seconds == seconds)) return false;
    if (seconds < 0.0) seconds = 0.0;
    return m_impl->reader->seek(seconds, to_keyframe_before);
}

bool Demuxer::rewind() {
    if (!m_impl) return false;
    return m_impl->reader->rewind();
}

} // namespace video
} // namespace fizmo
