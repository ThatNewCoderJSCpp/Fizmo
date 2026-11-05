#include "wav.hpp"

#include <algorithm>
#include <cstring>

namespace fizmo {
namespace audio {

using detail::append_le16;
using detail::append_le32;
using detail::append_tag;
using detail::put_le16;
using detail::put_le32;
using detail::put_le64;
using detail::read_le16;
using detail::read_le32;
using detail::read_le64;

struct WavReader::Source {
    virtual ~Source() = default;
    virtual std::size_t read(void* data, std::size_t bytes) = 0;
    virtual bool seek(std::uint64_t offset) = 0;
    virtual std::uint64_t size() = 0;
};

namespace {

struct FileSource : WavReader::Source {
    detail::File file;
    std::uint64_t cached_size = 0;
    std::size_t read(void* data, std::size_t bytes) override { return file.read(data, bytes); }
    bool seek(std::uint64_t offset) override { return file.seek(static_cast<std::int64_t>(offset)); }
    std::uint64_t size() override { return cached_size; }
};

struct MemorySource : WavReader::Source {
    const std::uint8_t* data = nullptr;
    std::size_t length = 0;
    std::size_t pos = 0;
    std::size_t read(void* out, std::size_t bytes) override {
        const std::size_t n = std::min(bytes, length - std::min(pos, length));
        if (n) std::memcpy(out, data + pos, n);
        pos += n;
        return n;
    }
    bool seek(std::uint64_t offset) override {
        if (offset > length) return false;
        pos = static_cast<std::size_t>(offset);
        return true;
    }
    std::uint64_t size() override { return length; }
};

bool tag_is(const std::uint8_t* p, const char* tag) noexcept {
    return std::memcmp(p, tag, 4) == 0;
}

bool format_from(std::uint16_t tag, std::size_t container_bytes, SampleFormat& out) noexcept {
    switch (tag) {
        case 1:
            switch (container_bytes) {
                case 1: out = SampleFormat::u8; return true;
                case 2: out = SampleFormat::s16; return true;
                case 3: out = SampleFormat::s24; return true;
                case 4: out = SampleFormat::s32; return true;
                default: return false;
            }
        case 3:
            if (container_bytes == 4) { out = SampleFormat::f32; return true; }
            if (container_bytes == 8) { out = SampleFormat::f64; return true; }
            return false;
        case 6:
            if (container_bytes == 1) { out = SampleFormat::alaw; return true; }
            return false;
        case 7:
            if (container_bytes == 1) { out = SampleFormat::ulaw; return true; }
            return false;
        default:
            return false;
    }
}

std::uint16_t tag_for(SampleFormat f) noexcept {
    switch (f) {
        case SampleFormat::f32:
        case SampleFormat::f64: return 3;
        case SampleFormat::alaw: return 6;
        case SampleFormat::ulaw: return 7;
        default: return 1;
    }
}

const char* info_name(const std::uint8_t* id) noexcept {
    struct Map { const char* id; const char* name; };
    static const Map names[] = {
        { "INAM", "title" }, { "IART", "artist" }, { "IPRD", "album" }, { "ICMT", "comment" },
        { "ICRD", "date" }, { "IGNR", "genre" }, { "ITRK", "track" }, { "IPRT", "track" },
        { "ICOP", "copyright" }, { "ISFT", "software" }, { "IENG", "engineer" }, { "ISBJ", "subject" },
    };
    for (const Map& m : names) {
        if (std::memcmp(id, m.id, 4) == 0) return m.name;
    }
    return nullptr;
}

void parse_info(const std::vector<std::uint8_t>& list, WavInfo& info) {
    if (list.size() < 4 || !tag_is(list.data(), "INFO")) return;
    std::size_t p = 4;
    while (p + 8 <= list.size()) {
        const std::uint8_t* id = &list[p];
        const std::uint32_t n = read_le32(&list[p + 4]);
        p += 8;
        if (n > list.size() - p) break;
        const char* name = info_name(id);
        if (name) {
            std::string value(reinterpret_cast<const char*>(&list[p]), n);
            while (!value.empty() && value.back() == '\0') value.pop_back();
            info.tags.emplace_back(name, value);
        }
        p += n + (n & 1);
    }
}

bool parse_header(WavReader::Source& src, WavInfo& info, std::uint64_t& data_offset, std::uint64_t& data_bytes, std::string* error) {
    auto fail = [&](const char* message) {
        if (error) *error = message;
        return false;
    };
    std::uint8_t head[12];
    if (src.read(head, 12) != 12) return fail("the file is too short to be a WAV file");
    const bool rf64 = tag_is(head, "RF64") || tag_is(head, "BW64");
    if ((!tag_is(head, "RIFF") && !rf64) || !tag_is(head + 8, "WAVE")) return fail("not a WAV file");
    info = WavInfo();
    info.rf64 = rf64;
    const std::uint64_t file_size = src.size();
    std::uint64_t pos = 12;
    std::uint64_t ds64_data = 0;
    std::uint64_t ds64_frames = 0;
    bool have_fmt = false;
    bool have_data = false;
    std::uint16_t tag = 0;
    std::uint16_t block_align = 0;
    std::uint16_t bits = 0;
    while (pos + 8 <= file_size) {
        if (!src.seek(pos)) break;
        std::uint8_t ch[8];
        if (src.read(ch, 8) != 8) break;
        std::uint64_t size = read_le32(ch + 4);
        const std::uint64_t body = pos + 8;
        if (tag_is(ch, "ds64")) {
            std::uint8_t d[24];
            if (size >= 24 && src.read(d, 24) == 24) {
                ds64_data = read_le64(d + 8);
                ds64_frames = read_le64(d + 16);
            }
        } else if (tag_is(ch, "fmt ")) {
            std::uint8_t f[40] = {};
            const std::size_t want = static_cast<std::size_t>(std::min<std::uint64_t>(size, 40));
            if (want < 16 || src.read(f, want) != want) return fail("the WAV format chunk is damaged");
            tag = read_le16(f);
            info.channels = read_le16(f + 2);
            info.sample_rate = read_le32(f + 4);
            block_align = read_le16(f + 12);
            bits = read_le16(f + 14);
            info.valid_bits = bits;
            if (tag == 0xFFFE && want >= 40) {
                info.valid_bits = read_le16(f + 18);
                info.channel_mask = read_le32(f + 20);
                tag = read_le16(f + 24);
            }
            have_fmt = true;
        } else if (tag_is(ch, "LIST")) {
            if (size >= 4 && size < (1u << 20)) {
                std::vector<std::uint8_t> list(static_cast<std::size_t>(size));
                if (src.read(list.data(), list.size()) == list.size()) parse_info(list, info);
            }
        } else if (tag_is(ch, "data")) {
            if (rf64 && size == 0xFFFFFFFFull) size = ds64_data;
            const std::uint64_t avail = file_size > body ? file_size - body : 0;
            if (size == 0 || size > avail) size = avail;
            data_offset = body;
            data_bytes = size;
            have_data = true;
        }
        if (size == 0xFFFFFFFFull && !have_data) break;
        pos = body + size + (size & 1);
        if (have_data && have_fmt && pos >= file_size) break;
    }
    if (!have_fmt) return fail("the WAV file has no format chunk");
    if (!have_data) return fail("the WAV file has no data chunk");
    if (info.channels == 0 || info.sample_rate == 0) return fail("the WAV file has no channels or no sample rate");
    if (block_align == 0) block_align = static_cast<std::uint16_t>(info.channels * ((bits + 7) / 8));
    if (block_align % info.channels) return fail("the WAV block size doesn't fit its channel count");
    const std::size_t container = block_align / info.channels;
    if (!format_from(tag, container, info.format)) {
        if (error) *error = "unsupported WAV encoding (format " + std::to_string(tag) + ", " + std::to_string(container * 8) + " bits)";
        return false;
    }
    info.frames = data_bytes / block_align;
    if (rf64 && ds64_frames && ds64_frames < info.frames && tag != 1 && tag != 3) info.frames = ds64_frames;
    if (info.channel_mask == 0) info.channel_mask = default_channel_mask(info.channels);
    return true;
}

void build_header(std::vector<std::uint8_t>& out, std::size_t channels, std::uint32_t rate, SampleFormat format, std::uint64_t frames, bool junk, std::int64_t* fact_pos, std::int64_t* data_size_pos) {
    const std::size_t bps = bytes_per_sample(format);
    const std::uint32_t block = static_cast<std::uint32_t>(bps * channels);
    const std::uint16_t tag = tag_for(format);
    const bool extensible = channels > 2 || (tag == 1 && bps > 2);
    const std::uint64_t data_bytes = frames * block;
    out.clear();
    append_tag(out, "RIFF");
    append_le32(out, 0);
    append_tag(out, "WAVE");
    if (junk) {
        append_tag(out, "JUNK");
        append_le32(out, 28);
        out.insert(out.end(), 28, 0);
    }
    append_tag(out, "fmt ");
    append_le32(out, extensible ? 40 : (tag == 1 ? 16 : 18));
    append_le16(out, extensible ? 0xFFFE : tag);
    append_le16(out, static_cast<std::uint16_t>(channels));
    append_le32(out, rate);
    append_le32(out, rate * block);
    append_le16(out, static_cast<std::uint16_t>(block));
    append_le16(out, static_cast<std::uint16_t>(bps * 8));
    if (extensible) {
        append_le16(out, 22);
        append_le16(out, static_cast<std::uint16_t>(bps * 8));
        append_le32(out, default_channel_mask(channels));
        append_le16(out, tag);
        static const std::uint8_t guid_tail[14] = { 0x00, 0x00, 0x00, 0x00, 0x10, 0x00, 0x80, 0x00, 0x00, 0xAA, 0x00, 0x38, 0x9B, 0x71 };
        out.insert(out.end(), guid_tail, guid_tail + 14);
    } else if (tag != 1) {
        append_le16(out, 0);
    }
    if (tag != 1) {
        append_tag(out, "fact");
        append_le32(out, 4);
        if (fact_pos) *fact_pos = static_cast<std::int64_t>(out.size());
        append_le32(out, static_cast<std::uint32_t>(std::min<std::uint64_t>(frames, 0xFFFFFFFFull)));
    } else if (fact_pos) {
        *fact_pos = -1;
    }
    append_tag(out, "data");
    if (data_size_pos) *data_size_pos = static_cast<std::int64_t>(out.size());
    append_le32(out, static_cast<std::uint32_t>(std::min<std::uint64_t>(data_bytes, 0xFFFFFFFFull)));
}

} // namespace

std::uint32_t default_channel_mask(std::size_t channels) noexcept {
    switch (channels) {
        case 1: return 0x4;
        case 2: return 0x3;
        case 3: return 0x7;
        case 4: return 0x33;
        case 5: return 0x37;
        case 6: return 0x3F;
        case 7: return 0x70F;
        case 8: return 0x63F;
        default: return 0;
    }
}

bool is_wav(const void* data, std::size_t size) noexcept {
    if (!data || size < 12) return false;
    const std::uint8_t* p = static_cast<const std::uint8_t*>(data);
    return (tag_is(p, "RIFF") || tag_is(p, "RF64") || tag_is(p, "BW64")) && tag_is(p + 8, "WAVE");
}

WavReader::WavReader() = default;
WavReader::~WavReader() = default;
WavReader::WavReader(WavReader&&) noexcept = default;
WavReader& WavReader::operator=(WavReader&&) noexcept = default;

bool WavReader::open(const std::string& path, std::string* error) {
    close();
    auto src = std::make_unique<FileSource>();
    if (!src->file.open(path, "rb")) {
        if (error) *error = "can't open " + path;
        return false;
    }
    const std::int64_t size = src->file.size();
    src->cached_size = size > 0 ? static_cast<std::uint64_t>(size) : 0;
    std::uint64_t bytes = 0;
    if (!parse_header(*src, m_info, m_data_offset, bytes, error)) return false;
    m_source = std::move(src);
    m_block = bytes_per_sample(m_info.format) * m_info.channels;
    m_position = 0;
    return m_source->seek(m_data_offset);
}

bool WavReader::open(const void* data, std::size_t size, std::string* error) {
    close();
    auto src = std::make_unique<MemorySource>();
    src->data = static_cast<const std::uint8_t*>(data);
    src->length = size;
    std::uint64_t bytes = 0;
    if (!parse_header(*src, m_info, m_data_offset, bytes, error)) return false;
    m_source = std::move(src);
    m_block = bytes_per_sample(m_info.format) * m_info.channels;
    m_position = 0;
    return m_source->seek(m_data_offset);
}

void WavReader::close() {
    m_source.reset();
    m_info = WavInfo();
    m_position = 0;
}

bool WavReader::is_open() const noexcept {
    return m_source != nullptr;
}

std::size_t WavReader::read(float* interleaved, std::size_t frames) {
    if (!m_source || !interleaved) return 0;
    const std::uint64_t left = m_info.frames - std::min(m_position, m_info.frames);
    frames = static_cast<std::size_t>(std::min<std::uint64_t>(frames, left));
    std::size_t done = 0;
    const std::size_t chunk = 8192;
    while (done < frames) {
        const std::size_t n = std::min(chunk, frames - done);
        m_raw.resize(n * m_block);
        const std::size_t got = m_source->read(m_raw.data(), m_raw.size()) / m_block;
        if (got == 0) break;
        to_float(m_raw.data(), m_info.format, interleaved + done * m_info.channels, got * m_info.channels);
        done += got;
        m_position += got;
        if (got < n) break;
    }
    return done;
}

std::size_t WavReader::read(AudioBuffer& out, std::size_t frames) {
    if (!m_source) return 0;
    if (out.channels() != m_info.channels || out.sample_rate() != m_info.sample_rate) out = AudioBuffer(m_info.channels, m_info.sample_rate);
    const std::size_t start = out.frames();
    out.resize(start + frames);
    const std::size_t got = read(out.frame(start), frames);
    out.resize(start + got);
    return got;
}

bool WavReader::seek(std::uint64_t frame) {
    if (!m_source) return false;
    frame = std::min(frame, m_info.frames);
    if (!m_source->seek(m_data_offset + frame * m_block)) return false;
    m_position = frame;
    return true;
}

bool read_wav(const std::string& path, AudioBuffer& out, std::string* error, WavInfo* info) {
    WavReader r;
    if (!r.open(path, error)) return false;
    out = AudioBuffer(r.info().channels, r.info().sample_rate);
    const std::size_t frames = static_cast<std::size_t>(r.info().frames);
    r.read(out, frames);
    if (info) *info = r.info();
    return true;
}

bool read_wav(const void* data, std::size_t size, AudioBuffer& out, std::string* error, WavInfo* info) {
    WavReader r;
    if (!r.open(data, size, error)) return false;
    out = AudioBuffer(r.info().channels, r.info().sample_rate);
    r.read(out, static_cast<std::size_t>(r.info().frames));
    if (info) *info = r.info();
    return true;
}

std::vector<std::uint8_t> encode_wav(const AudioBuffer& buffer, SampleFormat format, bool dither) {
    std::vector<std::uint8_t> out;
    const std::size_t channels = buffer.channels() ? buffer.channels() : 1;
    const std::uint64_t frames = buffer.frames();
    const std::uint64_t data_bytes = frames * channels * bytes_per_sample(format);
    const bool large = data_bytes + 100 > 0xFFFFFFFFull;
    build_header(out, channels, buffer.sample_rate(), format, frames, large, nullptr, nullptr);
    const std::size_t header = out.size();
    out.resize(header + static_cast<std::size_t>(data_bytes) + (data_bytes & 1), 0);
    from_float(buffer.data(), out.data() + header, format, buffer.sample_count(), dither);
    const std::uint64_t riff = out.size() - 8;
    if (large) {
        std::memcpy(out.data(), "RF64", 4);
        put_le32(out.data() + 4, 0xFFFFFFFFu);
        std::memcpy(out.data() + 12, "ds64", 4);
        put_le64(out.data() + 20, riff);
        put_le64(out.data() + 28, data_bytes);
        put_le64(out.data() + 36, frames);
        put_le32(out.data() + 44, 0);
        put_le32(out.data() + header - 4, 0xFFFFFFFFu);
    } else {
        put_le32(out.data() + 4, static_cast<std::uint32_t>(riff));
    }
    return out;
}

bool write_wav(const std::string& path, const AudioBuffer& buffer, SampleFormat format, std::string* error, bool dither) {
    if (buffer.channels() == 0 || buffer.sample_rate() == 0) {
        if (error) *error = "the buffer has no channels or no sample rate";
        return false;
    }
    WavWriter w;
    if (!w.open(path, buffer.channels(), buffer.sample_rate(), format, error, dither)) return false;
    if (!w.write(buffer)) {
        if (error) *error = "can't write " + path;
        w.close();
        return false;
    }
    return w.close(error);
}

WavWriter::~WavWriter() {
    close();
}

bool WavWriter::open(const std::string& path, std::size_t channels, std::uint32_t sample_rate, SampleFormat format, std::string* error, bool dither) {
    close();
    if (channels == 0 || channels > 65535 || sample_rate == 0) {
        if (error) *error = "a WAV file needs at least one channel and a sample rate";
        return false;
    }
    if (!m_file.open(path, "wb")) {
        if (error) *error = "can't create " + path;
        return false;
    }
    m_channels = channels;
    m_rate = sample_rate;
    m_format = format;
    m_dither = dither;
    m_frames = 0;
    m_failed = false;
    std::vector<std::uint8_t> header;
    build_header(header, channels, sample_rate, format, 0, true, &m_fact, &m_data_size_pos);
    m_header_riff = 4;
    m_ds64 = 12;
    m_data_start = static_cast<std::int64_t>(header.size());
    if (!m_file.write(header.data(), header.size())) {
        if (error) *error = "can't write " + path;
        m_file.close();
        return false;
    }
    return true;
}

bool WavWriter::write(const float* interleaved, std::size_t frames) {
    if (!m_file.is_open() || m_failed) return false;
    if (frames == 0) return true;
    m_raw.resize(frames * m_channels * bytes_per_sample(m_format));
    from_float(interleaved, m_raw.data(), m_format, frames * m_channels, m_dither);
    if (!m_file.write(m_raw.data(), m_raw.size())) {
        m_failed = true;
        return false;
    }
    m_frames += frames;
    return true;
}

bool WavWriter::write(const AudioBuffer& buffer) {
    if (buffer.channels() != m_channels) {
        const AudioBuffer remixed = buffer.with_channels(m_channels);
        return write(remixed.data(), remixed.frames());
    }
    return write(buffer.data(), buffer.frames());
}

bool WavWriter::close(std::string* error) {
    if (!m_file.is_open()) return true;
    bool ok = !m_failed;
    const std::uint64_t data_bytes = m_frames * m_channels * bytes_per_sample(m_format);
    if (data_bytes & 1) {
        const std::uint8_t zero = 0;
        ok = m_file.write(&zero, 1) && ok;
    }
    const std::int64_t end = m_file.tell();
    const std::uint64_t riff = end > 8 ? static_cast<std::uint64_t>(end) - 8 : 0;
    std::uint8_t b[8];
    if (riff > 0xFFFFFFFFull || data_bytes > 0xFFFFFFFFull) {
        ok = m_file.seek(0) && m_file.write("RF64", 4) && ok;
        put_le32(b, 0xFFFFFFFFu);
        ok = m_file.write(b, 4) && ok;
        ok = m_file.seek(m_ds64) && m_file.write("ds64", 4) && ok;
        std::uint8_t d[32];
        put_le32(d, 28);
        put_le64(d + 4, riff);
        put_le64(d + 12, data_bytes);
        put_le64(d + 20, m_frames);
        put_le32(d + 28, 0);
        ok = m_file.write(d, 32) && ok;
        put_le32(b, 0xFFFFFFFFu);
        ok = m_file.seek(m_data_size_pos) && m_file.write(b, 4) && ok;
    } else {
        put_le32(b, static_cast<std::uint32_t>(riff));
        ok = m_file.seek(m_header_riff) && m_file.write(b, 4) && ok;
        put_le32(b, static_cast<std::uint32_t>(data_bytes));
        ok = m_file.seek(m_data_size_pos) && m_file.write(b, 4) && ok;
    }
    if (m_fact >= 0) {
        put_le32(b, static_cast<std::uint32_t>(std::min<std::uint64_t>(m_frames, 0xFFFFFFFFull)));
        ok = m_file.seek(m_fact) && m_file.write(b, 4) && ok;
    }
    ok = m_file.flush() && ok;
    m_file.close();
    if (!ok && error) *error = "can't finish writing the WAV file";
    return ok;
}

} // namespace audio
} // namespace fizmo
