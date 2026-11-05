#include "aac.hpp"
#include "aac_internal.hpp"
#include "file_io.hpp"

#include <algorithm>
#include <cstring>

namespace fizmo {
namespace audio {

namespace aac_internal {

namespace {

int rate_to_index(std::uint32_t rate) noexcept {
    if (rate >= 92017) return 0;
    if (rate >= 75132) return 1;
    if (rate >= 55426) return 2;
    if (rate >= 46009) return 3;
    if (rate >= 37566) return 4;
    if (rate >= 27713) return 5;
    if (rate >= 23004) return 6;
    if (rate >= 18783) return 7;
    if (rate >= 13856) return 8;
    if (rate >= 11502) return 9;
    if (rate >= 9391) return 10;
    return 11;
}

int read_object_type(BitReader& br) {
    int aot = static_cast<int>(br.read(5));
    if (aot == 31) aot = 32 + static_cast<int>(br.read(6));
    return aot;
}

std::uint32_t read_rate(BitReader& br, int& index) {
    index = static_cast<int>(br.read(4));
    if (index == 15) return br.read(24);
    if (index > 12) return 0;
    return k_sample_rates[index];
}

int count_pce_channels(const ProgramConfig& pce) {
    int n = 0;
    for (const ProgramConfig::Entry& e : pce.entries) {
        if (e.position == pce_cc) continue;
        n += e.type == elem_cpe ? 2 : 1;
    }
    return n;
}

const int k_config_channels[15] = { 0, 1, 2, 3, 4, 5, 6, 8, 0, 0, 0, 7, 8, 24, 8 };

} // namespace

bool parse_program_config(BitReader& br, std::size_t align_ref, ProgramConfig& pce) {
    pce = ProgramConfig();
    br.skip(4);
    pce.object_type = static_cast<int>(br.read(2));
    pce.sampling_index = static_cast<int>(br.read(4));
    const int nf = static_cast<int>(br.read(4));
    const int ns = static_cast<int>(br.read(4));
    const int nb = static_cast<int>(br.read(4));
    const int nl = static_cast<int>(br.read(2));
    const int na = static_cast<int>(br.read(3));
    const int nc = static_cast<int>(br.read(4));
    if (br.bit()) br.skip(4);
    if (br.bit()) br.skip(4);
    if (br.bit()) br.skip(3);
    if (br.left() < 5LL * (nf + ns + nb + nc) + 4LL * (nl + na + nc)) return false;
    const int counts[3] = { nf, ns, nb };
    for (int p = 0; p < 3; ++p) {
        for (int i = 0; i < counts[p]; ++i) {
            ProgramConfig::Entry e;
            e.type = br.bit() ? elem_cpe : elem_sce;
            e.tag = static_cast<int>(br.read(4));
            e.position = p;
            pce.entries.push_back(e);
        }
    }
    for (int i = 0; i < nl; ++i) {
        ProgramConfig::Entry e;
        e.type = elem_lfe;
        e.tag = static_cast<int>(br.read(4));
        e.position = pce_lfe;
        pce.entries.push_back(e);
    }
    br.skip(static_cast<std::size_t>(na) * 4);
    for (int i = 0; i < nc; ++i) {
        ProgramConfig::Entry e;
        br.skip(1);
        e.type = elem_cce;
        e.tag = static_cast<int>(br.read(4));
        e.position = pce_cc;
        pce.entries.push_back(e);
    }
    const std::size_t rel = (br.pos() - align_ref) & 7;
    if (rel) br.skip(8 - rel);
    const std::size_t comment = br.read(8);
    if (br.left() < static_cast<long long>(comment * 8)) return false;
    br.skip(comment * 8);
    return !br.overrun();
}

bool parse_asc(const std::uint8_t* data, std::size_t size, StreamConfig& cfg, std::string* error) {
    cfg = StreamConfig();
    if (!data || size < 2) {
        if (error) *error = "AudioSpecificConfig too short";
        return false;
    }
    BitReader br(data, size);
    int aot = read_object_type(br);
    int sri = 0;
    std::uint32_t rate = read_rate(br, sri);
    const int chan = static_cast<int>(br.read(4));
    int sbr = -1;
    int ps = -1;
    std::uint32_t ext_rate = 0;
    if (aot == aot_sbr || aot == aot_ps) {
        if (aot == aot_ps) ps = 1;
        sbr = 1;
        int esri = 0;
        ext_rate = read_rate(br, esri);
        aot = read_object_type(br);
        if (aot == 22) br.skip(4);
    }
    if (rate == 0) {
        if (error) *error = "invalid sample rate";
        return false;
    }
    if (sri > 12) sri = rate_to_index(rate);
    if (aot != aot_main && aot != aot_lc && aot != aot_ssr && aot != aot_ltp) {
        if (error) *error = "unsupported audio object type " + std::to_string(aot);
        return false;
    }
    if (aot == aot_ssr) {
        if (error) *error = "AAC SSR is not supported";
        return false;
    }
    if (chan > 14 || (chan > 7 && chan != 11 && chan != 12 && chan != 14)) {
        if (error) *error = "unsupported channel configuration " + std::to_string(chan);
        return false;
    }
    cfg.frame_length_short = br.bit() != 0;
    if (br.bit()) br.skip(14);
    const bool ext_flag = br.bit() != 0;
    if (chan == 0) {
        if (!parse_program_config(br, 0, cfg.pce)) {
            if (error) *error = "invalid program config element";
            return false;
        }
        cfg.has_pce = true;
    }
    if (ext_flag) br.skip(1);
    if (br.overrun()) {
        if (error) *error = "AudioSpecificConfig truncated";
        return false;
    }
    if (sbr == -1) {
        while (br.left() > 15) {
            if (br.peek(11) == 0x2b7) {
                br.skip(11);
                const int ext = read_object_type(br);
                if (ext == aot_sbr) {
                    sbr = br.bit();
                    if (sbr == 1) {
                        int esri = 0;
                        ext_rate = read_rate(br, esri);
                        if (ext_rate == rate) sbr = -1;
                    }
                    if (br.left() > 11 && br.read(11) == 0x548) ps = br.bit();
                }
                break;
            }
            br.skip(1);
        }
    }
    const int channels = chan ? k_config_channels[chan] : count_pce_channels(cfg.pce);
    if (sbr == 0) ps = 0;
    if ((ps == -1 && aot != aot_lc) || channels > 1) ps = 0;
    if (cfg.frame_length_short && sbr == 1) {
        sbr = 0;
        ps = 0;
    }
    if (channels <= 1 && sbr == 1 && ps == -1) ps = 1;
    if (channels > 1) ps = 0;
    cfg.object_type = aot;
    cfg.sampling_index = sri;
    cfg.sample_rate = rate;
    cfg.channel_config = chan;
    cfg.sbr = sbr;
    cfg.ps = ps;
    cfg.ext_sample_rate = sbr != 0 ? ext_rate : 0;
    return true;
}

bool parse_adts_header(const std::uint8_t* p, std::size_t size, AdtsHeader& h) noexcept {
    if (!p || size < 7) return false;
    if (p[0] != 0xFF || (p[1] & 0xF6) != 0xF0) return false;
    h.id = (p[1] >> 3) & 1;
    h.protection_absent = p[1] & 1;
    h.object_type = ((p[2] >> 6) & 3) + 1;
    h.sampling_index = (p[2] >> 2) & 15;
    h.channel_config = ((p[2] & 1) << 2) | ((p[3] >> 6) & 3);
    h.frame_length = (static_cast<std::size_t>(p[3] & 3) << 11) | (static_cast<std::size_t>(p[4]) << 3) | (p[5] >> 5);
    h.buffer_fullness = ((p[5] & 31) << 6) | (p[6] >> 2);
    h.raw_blocks = p[6] & 3;
    h.header_size = h.protection_absent ? 7 : 9;
    if (h.sampling_index > 12) return false;
    if (h.frame_length < h.header_size + 1) return false;
    return true;
}

} // namespace aac_internal

using namespace aac_internal;

namespace {

void fill_config(const CoreDecoder& core, AacConfig& c) {
    const StreamConfig& cfg = core.stream();
    c.object_type = cfg.object_type;
    c.sample_rate = cfg.sample_rate;
    c.channel_config = cfg.channel_config;
    c.sample_rate_index = cfg.sampling_index;
    int coded = 0;
    if (cfg.has_pce) {
        for (const ProgramConfig::Entry& e : cfg.pce.entries) {
            if (e.position != pce_cc) coded += e.type == elem_cpe ? 2 : 1;
        }
    } else if (cfg.channel_config > 0 && cfg.channel_config < 15) {
        static const int counts[15] = { 0, 1, 2, 3, 4, 5, 6, 8, 0, 0, 0, 7, 8, 24, 8 };
        coded = counts[cfg.channel_config];
    }
    c.sbr = core.sbr_active();
    c.ps = core.ps_active();
    if (c.ps) coded = 1;
    if (!coded) coded = core.output_channels();
    c.channels = static_cast<std::uint16_t>(coded);
    c.output_rate = core.output_rate();
    c.output_channels = static_cast<std::uint16_t>(core.output_channels());
    c.frame_length = core.output_frame_length();
    c.core_frame_length = cfg.frame_length_short ? 960 : 1024;
    c.extension_rate = core.extension_rate();
    c.channel_mask = core.channel_mask();
    c.implicit_signalling = cfg.sbr == -1;
}

} // namespace

bool parse_audio_specific_config(const std::uint8_t* data, std::size_t size, AacConfig& config, std::string* error) {
    StreamConfig cfg;
    if (!parse_asc(data, size, cfg, error)) return false;
    CoreDecoder core;
    if (!core.configure(cfg, error)) return false;
    config = AacConfig();
    fill_config(core, config);
    return true;
}

struct AacDecoder::Impl {
    CoreDecoder core;
    bool open = false;
    bool adts = false;
    bool configured = false;
    StreamConfig stream;
};

AacDecoder::AacDecoder() : m_impl(new Impl()) {}
AacDecoder::~AacDecoder() = default;
AacDecoder::AacDecoder(AacDecoder&&) noexcept = default;
AacDecoder& AacDecoder::operator=(AacDecoder&&) noexcept = default;

bool AacDecoder::is_open() const noexcept {
    return m_impl && m_impl->open;
}

bool AacDecoder::open(const std::uint8_t* asc, std::size_t size, std::string* error) {
    if (!m_impl) m_impl.reset(new Impl());
    Impl& d = *m_impl;
    d.open = false;
    d.configured = false;
    d.adts = false;
    m_config = AacConfig();
    StreamConfig cfg;
    if (!parse_asc(asc, size, cfg, error)) return false;
    if (!d.core.configure(cfg, error)) return false;
    d.stream = cfg;
    d.configured = true;
    d.open = true;
    refresh();
    return true;
}

bool AacDecoder::open_adts() {
    if (!m_impl) m_impl.reset(new Impl());
    Impl& d = *m_impl;
    d.open = true;
    d.adts = true;
    d.configured = false;
    m_config = AacConfig();
    return true;
}

void AacDecoder::reset() {
    if (!m_impl) return;
    if (m_impl->configured) m_impl->core.reset();
    refresh();
}

std::uint32_t AacDecoder::delay() const noexcept {
    if (!m_impl || !m_impl->configured) return 0;
    return m_impl->core.delay();
}

void AacDecoder::refresh() {
    if (!m_impl || !m_impl->configured) return;
    fill_config(m_impl->core, m_config);
}

std::size_t AacDecoder::decode(const std::uint8_t* au, std::size_t size, std::vector<float>& out, std::string* error) {
    if (!m_impl || !m_impl->open) {
        if (error) *error = "decoder is not open";
        return 0;
    }
    Impl& d = *m_impl;
    if (!au || size == 0) {
        if (error) *error = "empty access unit";
        return 0;
    }
    if (error) error->clear();
    const std::size_t before = out.size();
    AdtsHeader h;
    if (size >= 7 && parse_adts_header(au, size, h)) {
        StreamConfig cfg;
        cfg.object_type = h.object_type;
        cfg.sampling_index = h.sampling_index;
        cfg.sample_rate = k_sample_rates[h.sampling_index];
        cfg.channel_config = h.channel_config;
        cfg.sbr = -1;
        cfg.ps = -1;
        if (!d.configured || d.stream.object_type != cfg.object_type || d.stream.sampling_index != cfg.sampling_index || d.stream.channel_config != cfg.channel_config) {
            if (!d.core.configure(cfg, error)) {
                d.configured = false;
                return 0;
            }
            d.stream = cfg;
            d.configured = true;
        }
        const std::size_t avail = std::min(size, h.frame_length);
        std::size_t start = h.header_size;
        if (!h.protection_absent && h.raw_blocks > 0) start = 7 + 2 * static_cast<std::size_t>(h.raw_blocks) + 2;
        if (start >= avail) {
            if (error) *error = "truncated ADTS frame";
            d.core.append_silence(out);
            refresh();
            return d.core.output_channels() ? (out.size() - before) / static_cast<std::size_t>(d.core.output_channels()) : 0;
        }
        BitReader br(au + start, avail - start);
        std::string first_error;
        for (int b = 0; b <= h.raw_blocks; ++b) {
            const std::size_t ref = br.pos();
            std::string err;
            const bool ok = d.core.decode_block(br, ref, out, &err);
            if (!ok && first_error.empty()) first_error = err.empty() ? "decode error" : err;
            if (!ok) {
                for (int r = b + 1; r <= h.raw_blocks; ++r) d.core.append_silence(out);
                break;
            }
            br.align();
            if (!h.protection_absent && h.raw_blocks > 0) br.skip(16);
        }
        if (error && !first_error.empty()) *error = first_error;
    } else {
        if (!d.configured) {
            if (error) *error = "decoder has no configuration";
            return 0;
        }
        BitReader br(au, size);
        std::string err;
        if (!d.core.decode_block(br, 0, out, &err) && error) *error = err.empty() ? "decode error" : err;
    }
    refresh();
    const int ch = d.core.output_channels();
    if (ch <= 0) return 0;
    return (out.size() - before) / static_cast<std::size_t>(ch);
}

namespace {

class ByteSource {
public:
    virtual ~ByteSource() = default;
    virtual std::size_t read_at(std::uint64_t pos, void* dst, std::size_t n) = 0;
    virtual std::uint64_t size() const = 0;
};

class MemorySource : public ByteSource {
public:
    MemorySource(const std::uint8_t* data, std::size_t size) : m_data(data), m_size(size) {}
    std::size_t read_at(std::uint64_t pos, void* dst, std::size_t n) override {
        if (pos >= m_size) return 0;
        const std::size_t avail = static_cast<std::size_t>(std::min<std::uint64_t>(n, m_size - pos));
        std::memcpy(dst, m_data + pos, avail);
        return avail;
    }
    std::uint64_t size() const override { return m_size; }

private:
    const std::uint8_t* m_data;
    std::size_t m_size;
};

class FileSource : public ByteSource {
public:
    bool open(const std::string& path) {
        if (!m_file.open(path, "rb")) return false;
        const std::int64_t s = m_file.size();
        m_size = s > 0 ? static_cast<std::uint64_t>(s) : 0;
        return true;
    }
    std::size_t read_at(std::uint64_t pos, void* dst, std::size_t n) override {
        if (pos >= m_size) return 0;
        if (!m_file.seek(static_cast<std::int64_t>(pos))) return 0;
        return m_file.read(dst, n);
    }
    std::uint64_t size() const override { return m_size; }

private:
    detail::File m_file;
    std::uint64_t m_size = 0;
};

struct FrameEntry {
    std::uint64_t offset;
    std::uint32_t size;
};

std::uint64_t id3v2_size(const std::uint8_t* p, std::size_t n) noexcept {
    if (n < 10 || p[0] != 'I' || p[1] != 'D' || p[2] != '3') return 0;
    if (p[3] == 0xFF || p[4] == 0xFF) return 0;
    if ((p[6] | p[7] | p[8] | p[9]) & 0x80) return 0;
    const std::uint64_t size = (static_cast<std::uint64_t>(p[6]) << 21) | (static_cast<std::uint64_t>(p[7]) << 14) | (static_cast<std::uint64_t>(p[8]) << 7) | p[9];
    return 10 + size + ((p[5] & 0x10) ? 10 : 0);
}

bool same_stream(const AdtsHeader& a, const AdtsHeader& b) noexcept {
    return a.id == b.id && a.object_type == b.object_type && a.sampling_index == b.sampling_index && a.channel_config == b.channel_config;
}

bool header_at(ByteSource& src, std::uint64_t pos, std::uint64_t end, AdtsHeader& h) {
    std::uint8_t b[9];
    if (pos + 7 > end) return false;
    if (src.read_at(pos, b, 7) != 7) return false;
    if (!parse_adts_header(b, 7, h)) return false;
    return pos + h.frame_length <= end;
}

bool locate(ByteSource& src, std::uint64_t from, std::uint64_t end, const AdtsHeader* key, std::uint64_t& found, AdtsHeader& out) {
    std::vector<std::uint8_t> chunk(65536 + 8);
    std::uint64_t pos = from;
    while (pos + 7 <= end) {
        const std::size_t want = static_cast<std::size_t>(std::min<std::uint64_t>(chunk.size(), end - pos));
        const std::size_t got = src.read_at(pos, chunk.data(), want);
        if (got < 7) return false;
        for (std::size_t i = 0; i + 7 <= got; ++i) {
            if (chunk[i] != 0xFF || (chunk[i + 1] & 0xF6) != 0xF0) continue;
            AdtsHeader h;
            if (!parse_adts_header(chunk.data() + i, got - i, h)) continue;
            if (key && !same_stream(h, *key)) continue;
            const std::uint64_t at = pos + i;
            if (at + h.frame_length > end) continue;
            const std::uint64_t next = at + h.frame_length;
            if (next + 7 <= end) {
                AdtsHeader h2;
                if (!header_at(src, next, end, h2) || !same_stream(h, h2)) continue;
            } else if (next != end && next + 7 > end) {
                if (end - next > 0 && !key) continue;
            }
            found = at;
            out = h;
            return true;
        }
        if (got <= 7) return false;
        pos += got - 6;
    }
    return false;
}

std::uint64_t skip_id3(ByteSource& src, std::uint64_t total) {
    std::uint64_t begin = 0;
    for (;;) {
        std::uint8_t head[10];
        if (src.read_at(begin, head, 10) != 10) break;
        const std::uint64_t len = id3v2_size(head, 10);
        if (len == 0 || len > total - begin) break;
        begin += len;
    }
    return begin;
}

bool has_faac_signature(const std::uint8_t* frame, std::size_t size) {
    const char sig[] = "libfaac";
    if (size < 8) return false;
    for (std::size_t i = 0; i + 7 <= size; ++i) {
        if (std::memcmp(frame + i, sig, 7) == 0) return true;
    }
    return false;
}

} // namespace

bool is_aac_adts(const void* data, std::size_t size) noexcept {
    if (!data || size < 7) return false;
    const std::uint8_t* p = static_cast<const std::uint8_t*>(data);
    std::size_t begin = 0;
    while (begin + 10 <= size) {
        const std::uint64_t len = id3v2_size(p + begin, size - begin);
        if (!len) break;
        if (len >= size - begin) return true;
        begin += static_cast<std::size_t>(len);
    }
    const std::size_t limit = std::min<std::size_t>(size, begin + 8192);
    for (std::size_t i = begin; i + 7 <= limit; ++i) {
        if (p[i] != 0xFF || (p[i + 1] & 0xF6) != 0xF0) continue;
        AdtsHeader h;
        if (!parse_adts_header(p + i, size - i, h)) continue;
        if (h.object_type == 3) continue;
        const std::size_t next = i + h.frame_length;
        if (next == size) return i == begin;
        if (next + 7 > size) {
            if (i == begin && next > size) return true;
            continue;
        }
        AdtsHeader h2;
        if (parse_adts_header(p + next, size - next, h2) && same_stream(h, h2)) return true;
    }
    return false;
}

struct AacReader::State {
    std::unique_ptr<ByteSource> source;
    std::vector<FrameEntry> frames;
    AacDecoder decoder;
    std::vector<std::uint8_t> frame_bytes;
    std::vector<float> pcm;
    std::uint64_t pcm_start = 0;
    std::size_t pcm_frames = 0;
    std::size_t next = 0;
    std::uint64_t spf = 1024;
    std::uint64_t skip = 0;
    int channels = 0;

    bool load(std::size_t index) {
        const FrameEntry& e = frames[index];
        frame_bytes.resize(static_cast<std::size_t>(e.size) + 8);
        const std::size_t got = source->read_at(e.offset, frame_bytes.data(), e.size);
        std::memset(frame_bytes.data() + got, 0, frame_bytes.size() - got);
        return got == e.size;
    }

    void decode_next() {
        pcm.clear();
        load(next);
        decoder.decode(frame_bytes.data(), frames[next].size, pcm);
        const std::size_t want = static_cast<std::size_t>(spf) * static_cast<std::size_t>(channels);
        if (pcm.size() != want) pcm.resize(want, 0.0f);
        pcm_start = static_cast<std::uint64_t>(next) * spf;
        pcm_frames = static_cast<std::size_t>(spf);
        ++next;
    }
};

AacReader::AacReader() = default;
AacReader::~AacReader() = default;
AacReader::AacReader(AacReader&&) noexcept = default;
AacReader& AacReader::operator=(AacReader&&) noexcept = default;

bool AacReader::open(const std::string& path, std::string* error) {
    close();
    std::unique_ptr<FileSource> src(new FileSource());
    if (!src->open(path)) {
        if (error) *error = "can't open " + path;
        return false;
    }
    m_state.reset(new State());
    m_state->source = std::move(src);
    return finish_open(error);
}

bool AacReader::open(const void* data, std::size_t size, std::string* error) {
    close();
    if (!data && size) {
        if (error) *error = "no data";
        return false;
    }
    m_state.reset(new State());
    m_state->source.reset(new MemorySource(static_cast<const std::uint8_t*>(data), size));
    return finish_open(error);
}

bool AacReader::finish_open(std::string* error) {
    State& s = *m_state;
    ByteSource& src = *s.source;
    const std::uint64_t total = src.size();
    const std::uint64_t begin = skip_id3(src, total);
    std::uint64_t end = total;
    if (end - begin >= 128) {
        std::uint8_t tag[3];
        if (src.read_at(end - 128, tag, 3) == 3 && tag[0] == 'T' && tag[1] == 'A' && tag[2] == 'G') end -= 128;
    }
    std::uint64_t pos = 0;
    AdtsHeader first;
    if (!locate(src, begin, end, nullptr, pos, first)) {
        if (error) *error = "no ADTS frames found";
        m_state.reset();
        return false;
    }
    if (first.object_type == 3) {
        if (error) *error = "AAC SSR is not supported";
        m_state.reset();
        return false;
    }
    std::uint64_t bytes = 0;
    bool in_sync = true;
    while (pos + 7 <= end) {
        AdtsHeader h;
        bool ok = header_at(src, pos, end, h) && same_stream(h, first);
        if (ok && !in_sync) {
            AdtsHeader h2;
            const std::uint64_t next = pos + h.frame_length;
            ok = next + 7 > end || (header_at(src, next, end, h2) && same_stream(h2, first));
        }
        if (!ok) {
            std::uint64_t found = 0;
            AdtsHeader h3;
            if (!locate(src, pos + 1, end, &first, found, h3)) break;
            pos = found;
            in_sync = false;
            continue;
        }
        in_sync = true;
        s.frames.push_back({ pos, static_cast<std::uint32_t>(h.frame_length) });
        bytes += h.frame_length;
        pos += h.frame_length;
    }
    if (s.frames.empty()) {
        if (error) *error = "no ADTS frames found";
        m_state.reset();
        return false;
    }
    AacDecoder probe;
    probe.open_adts();
    std::vector<float> tmp;
    bool decoded = false;
    bool faac = false;
    for (std::size_t i = 0; i < s.frames.size() && i < 8 && !decoded; ++i) {
        s.load(i);
        if (i == 0) faac = has_faac_signature(s.frame_bytes.data(), s.frames[0].size);
        std::string err;
        tmp.clear();
        probe.decode(s.frame_bytes.data(), s.frames[i].size, tmp, &err);
        decoded = err.empty() && probe.config().output_channels > 0;
    }
    const AacConfig& cfg = probe.config();
    if (cfg.output_channels == 0 || cfg.output_rate == 0 || cfg.frame_length == 0) {
        if (error) *error = "can't decode AAC stream";
        m_state.reset();
        return false;
    }
    s.channels = cfg.output_channels;
    s.spf = cfg.frame_length;
    std::uint64_t blocks = 0;
    for (const FrameEntry& e : s.frames) {
        std::uint8_t b[7];
        AdtsHeader h;
        if (src.read_at(e.offset, b, 7) == 7 && parse_adts_header(b, 7, h)) blocks += static_cast<std::uint64_t>(h.raw_blocks) + 1;
        else blocks += 1;
    }
    if (blocks != s.frames.size()) {
        std::uint8_t b[7];
        AdtsHeader h;
        if (src.read_at(s.frames[0].offset, b, 7) == 7 && parse_adts_header(b, 7, h)) s.spf *= static_cast<std::uint64_t>(h.raw_blocks) + 1;
    }
    s.decoder.open_adts();
    s.skip = faac ? 1024 : 0;
    const std::uint64_t decoded_frames = static_cast<std::uint64_t>(s.frames.size()) * s.spf;
    m_info = AacInfo();
    m_info.sample_rate = cfg.output_rate;
    m_info.channels = cfg.output_channels;
    m_info.frames = decoded_frames > s.skip ? decoded_frames - s.skip : 0;
    m_info.object_type = cfg.object_type;
    m_info.sbr = cfg.sbr;
    m_info.ps = cfg.ps;
    m_info.core_sample_rate = cfg.sample_rate;
    m_info.channel_mask = cfg.channel_mask;
    m_info.adts_frames = s.frames.size();
    m_info.decoder_delay = probe.delay();
    m_info.skipped_samples = static_cast<std::uint32_t>(s.skip);
    if (decoded_frames) m_info.bitrate_kbps = static_cast<std::uint32_t>((bytes * 8u * cfg.output_rate / decoded_frames + 500u) / 1000u);
    m_position = 0;
    return true;
}

void AacReader::close() {
    m_state.reset();
    m_info = AacInfo();
    m_position = 0;
}

bool AacReader::is_open() const noexcept {
    return m_state != nullptr;
}

std::size_t AacReader::read(float* interleaved, std::size_t frames) {
    if (!m_state || !interleaved) return 0;
    State& s = *m_state;
    const std::size_t ch = static_cast<std::size_t>(s.channels);
    std::size_t done = 0;
    while (done < frames && m_position < m_info.frames) {
        const std::uint64_t abs = s.skip + m_position;
        if (s.pcm_frames && abs >= s.pcm_start && abs < s.pcm_start + s.pcm_frames) {
            const std::uint64_t avail = s.pcm_start + s.pcm_frames - abs;
            const std::uint64_t left = m_info.frames - m_position;
            const std::size_t n = static_cast<std::size_t>(std::min<std::uint64_t>(std::min<std::uint64_t>(avail, left), frames - done));
            std::memcpy(interleaved + done * ch, s.pcm.data() + static_cast<std::size_t>(abs - s.pcm_start) * ch, n * ch * sizeof(float));
            done += n;
            m_position += n;
            continue;
        }
        if (s.next >= s.frames.size()) break;
        s.decode_next();
    }
    return done;
}

std::size_t AacReader::read(AudioBuffer& out, std::size_t frames) {
    if (!m_state) return 0;
    if (out.channels() != m_info.channels || out.sample_rate() != m_info.sample_rate) out = AudioBuffer(m_info.channels, m_info.sample_rate);
    const std::size_t start = out.frames();
    out.resize(start + frames);
    const std::size_t got = read(out.frame(start), frames);
    out.resize(start + got);
    return got;
}

bool AacReader::seek(std::uint64_t frame) {
    if (!m_state) return false;
    State& s = *m_state;
    frame = std::min(frame, m_info.frames);
    const std::uint64_t abs = s.skip + frame;
    const std::uint64_t k = abs / s.spf;
    const std::uint64_t preroll = m_info.sbr ? 32 : 2;
    const std::size_t target = static_cast<std::size_t>(std::min<std::uint64_t>(k, s.frames.size()));
    const std::size_t begin = static_cast<std::size_t>(target > preroll ? target - preroll : 0);
    s.decoder.open_adts();
    std::vector<float> scratch;
    for (std::size_t i = begin; i < target; ++i) {
        s.load(i);
        scratch.clear();
        s.decoder.decode(s.frame_bytes.data(), s.frames[i].size, scratch);
    }
    s.next = target;
    s.pcm_frames = 0;
    m_position = frame;
    return true;
}

bool read_aac(const void* data, std::size_t size, AudioBuffer& out, std::string* error, AacInfo* info) {
    AacReader r;
    if (!r.open(data, size, error)) return false;
    out = AudioBuffer(r.info().channels, r.info().sample_rate);
    r.read(out, static_cast<std::size_t>(r.info().frames));
    if (info) *info = r.info();
    return true;
}

bool read_aac(const std::string& path, AudioBuffer& out, std::string* error, AacInfo* info) {
    std::vector<std::uint8_t> bytes;
    if (!detail::read_whole_file(path, bytes, error)) return false;
    return read_aac(bytes.data(), bytes.size(), out, error, info);
}

} // namespace audio
} // namespace fizmo
