#include "demux_internal.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <utility>

namespace fizmo {
namespace video {
namespace demux_detail {

namespace {

const std::uint32_t ID_EBML = 0x1A45DFA3;
const std::uint32_t ID_DOCTYPE = 0x4282;
const std::uint32_t ID_SEGMENT = 0x18538067;
const std::uint32_t ID_SEEKHEAD = 0x114D9B74;
const std::uint32_t ID_SEEK = 0x4DBB;
const std::uint32_t ID_SEEKID = 0x53AB;
const std::uint32_t ID_SEEKPOSITION = 0x53AC;
const std::uint32_t ID_INFO = 0x1549A966;
const std::uint32_t ID_TIMECODESCALE = 0x2AD7B1;
const std::uint32_t ID_DURATION = 0x4489;
const std::uint32_t ID_TITLE = 0x7BA9;
const std::uint32_t ID_TRACKS = 0x1654AE6B;
const std::uint32_t ID_TRACKENTRY = 0xAE;
const std::uint32_t ID_TRACKNUMBER = 0xD7;
const std::uint32_t ID_TRACKUID = 0x73C5;
const std::uint32_t ID_TRACKTYPE = 0x83;
const std::uint32_t ID_FLAGENABLED = 0xB9;
const std::uint32_t ID_FLAGDEFAULT = 0x88;
const std::uint32_t ID_DEFAULTDURATION = 0x23E383;
const std::uint32_t ID_NAME = 0x536E;
const std::uint32_t ID_LANGUAGE = 0x22B59C;
const std::uint32_t ID_LANGUAGEBCP47 = 0x22B59D;
const std::uint32_t ID_CODECID = 0x86;
const std::uint32_t ID_CODECPRIVATE = 0x63A2;
const std::uint32_t ID_CODECDELAY = 0x56AA;
const std::uint32_t ID_SEEKPREROLL = 0x56BB;
const std::uint32_t ID_VIDEO = 0xE0;
const std::uint32_t ID_PIXELWIDTH = 0xB0;
const std::uint32_t ID_PIXELHEIGHT = 0xBA;
const std::uint32_t ID_DISPLAYWIDTH = 0x54B0;
const std::uint32_t ID_DISPLAYHEIGHT = 0x54BA;
const std::uint32_t ID_DISPLAYUNIT = 0x54B2;
const std::uint32_t ID_FRAMERATE = 0x2383E3;
const std::uint32_t ID_PROJECTION = 0x7670;
const std::uint32_t ID_PROJECTIONPOSEROLL = 0x7675;
const std::uint32_t ID_AUDIO = 0xE1;
const std::uint32_t ID_SAMPLINGFREQUENCY = 0xB5;
const std::uint32_t ID_OUTPUTSAMPLINGFREQUENCY = 0x78B5;
const std::uint32_t ID_CHANNELS = 0x9F;
const std::uint32_t ID_BITDEPTH = 0x6264;
const std::uint32_t ID_CONTENTENCODINGS = 0x6D80;
const std::uint32_t ID_CONTENTENCODING = 0x6240;
const std::uint32_t ID_CONTENTENCODINGSCOPE = 0x5032;
const std::uint32_t ID_CONTENTENCODINGTYPE = 0x5033;
const std::uint32_t ID_CONTENTCOMPRESSION = 0x5034;
const std::uint32_t ID_CONTENTCOMPALGO = 0x4254;
const std::uint32_t ID_CONTENTCOMPSETTINGS = 0x4255;
const std::uint32_t ID_CONTENTENCRYPTION = 0x5035;
const std::uint32_t ID_CLUSTER = 0x1F43B675;
const std::uint32_t ID_TIMECODE = 0xE7;
const std::uint32_t ID_SIMPLEBLOCK = 0xA3;
const std::uint32_t ID_BLOCKGROUP = 0xA0;
const std::uint32_t ID_BLOCK = 0xA1;
const std::uint32_t ID_BLOCKDURATION = 0x9B;
const std::uint32_t ID_REFERENCEBLOCK = 0xFB;
const std::uint32_t ID_CUES = 0x1C53BB6B;
const std::uint32_t ID_CUEPOINT = 0xBB;
const std::uint32_t ID_CUETIME = 0xB3;
const std::uint32_t ID_CUETRACKPOSITIONS = 0xB7;
const std::uint32_t ID_CUETRACK = 0xF7;
const std::uint32_t ID_CUECLUSTERPOSITION = 0xF1;
const std::uint32_t ID_TAGS = 0x1254C367;
const std::uint32_t ID_TAG = 0x7373;
const std::uint32_t ID_TARGETS = 0x63C0;
const std::uint32_t ID_TAGTRACKUID = 0x63C5;
const std::uint32_t ID_TAGEDITIONUID = 0x63C9;
const std::uint32_t ID_TAGCHAPTERUID = 0x63C4;
const std::uint32_t ID_TAGATTACHMENTUID = 0x63C6;
const std::uint32_t ID_SIMPLETAG = 0x67C8;
const std::uint32_t ID_TAGNAME = 0x45A3;
const std::uint32_t ID_TAGSTRING = 0x4487;
const std::uint32_t ID_CHAPTERS = 0x1043A770;
const std::uint32_t ID_ATTACHMENTS = 0x1941A469;

const std::uint64_t k_unknown = ~std::uint64_t(0);
const std::uint64_t k_max_meta = std::uint64_t(1) << 26;
const std::size_t k_max_cues = std::size_t(1) << 22;
const std::size_t k_max_laces = 256;
const std::size_t k_max_queue = 4096;

bool is_top_level(std::uint32_t id) noexcept {
    switch (id) {
        case ID_CLUSTER: case ID_CUES: case ID_TAGS: case ID_SEEKHEAD: case ID_INFO: case ID_TRACKS: case ID_CHAPTERS:
        case ID_ATTACHMENTS: case ID_EBML: case ID_SEGMENT:
            return true;
        default:
            return false;
    }
}

bool is_segment_child(std::uint32_t id) noexcept {
    return is_top_level(id) || id == 0xEC || id == 0xBF;
}

bool is_cluster_child(std::uint32_t id) noexcept {
    switch (id) {
        case 0xE7: case 0x5854: case 0xA7: case 0xAB: case 0xA3: case 0xA0: case 0xAF: case 0xEC: case 0xBF:
            return true;
        default:
            return false;
    }
}

int vint_length(std::uint8_t first) noexcept {
    for (int i = 0; i < 8; ++i) {
        if (first & (0x80 >> i)) return i + 1;
    }
    return 0;
}

bool parse_id(const std::uint8_t* p, std::size_t n, std::uint32_t& id, std::size_t& used) noexcept {
    if (n < 1) return false;
    const int len = vint_length(p[0]);
    if (len < 1 || len > 4 || static_cast<std::size_t>(len) > n) return false;
    std::uint32_t v = 0;
    for (int i = 0; i < len; ++i) v = (v << 8) | p[i];
    id = v;
    used = static_cast<std::size_t>(len);
    return true;
}

bool parse_size(const std::uint8_t* p, std::size_t n, std::uint64_t& size, std::size_t& used) noexcept {
    if (n < 1) return false;
    const int len = vint_length(p[0]);
    if (len < 1 || static_cast<std::size_t>(len) > n) return false;
    std::uint64_t v = p[0] & (0xFF >> len);
    bool all_ones = v == static_cast<std::uint64_t>(0xFF >> len);
    for (int i = 1; i < len; ++i) {
        v = (v << 8) | p[i];
        if (p[i] != 0xFF) all_ones = false;
    }
    size = all_ones ? k_unknown : v;
    used = static_cast<std::size_t>(len);
    return true;
}

struct Element {
    std::uint32_t id = 0;
    const std::uint8_t* data = nullptr;
    std::size_t size = 0;
};

bool next_element(ByteReader& r, Element& e) {
    std::size_t used = 0;
    if (!parse_id(r.here(), r.left(), e.id, used)) return false;
    r.skip(used);
    std::uint64_t size = 0;
    if (!parse_size(r.here(), r.left(), size, used)) return false;
    r.skip(used);
    if (size == k_unknown || size > r.left()) size = r.left();
    e.data = r.here();
    e.size = static_cast<std::size_t>(size);
    r.skip(e.size);
    return true;
}

std::uint64_t ebml_uint(const Element& e) noexcept {
    std::uint64_t v = 0;
    for (std::size_t i = 0; i < e.size && i < 8; ++i) v = (v << 8) | e.data[i];
    return v;
}

double ebml_float(const Element& e) noexcept {
    if (e.size == 4) {
        const std::uint32_t v = static_cast<std::uint32_t>(ebml_uint(e));
        float f;
        std::memcpy(&f, &v, 4);
        return f;
    }
    if (e.size == 8) {
        const std::uint64_t v = ebml_uint(e);
        double d;
        std::memcpy(&d, &v, 8);
        return d;
    }
    return 0.0;
}

std::string ebml_string(const Element& e) {
    std::string s(reinterpret_cast<const char*>(e.data), e.size);
    const std::size_t z = s.find('\0');
    if (z != std::string::npos) s.resize(z);
    return s;
}

double parse_duration_tag(const std::string& s) {
    int h = 0;
    int m = 0;
    double sec = 0.0;
    std::size_t a = s.find(':');
    if (a == std::string::npos) return 0.0;
    std::size_t b = s.find(':', a + 1);
    if (b == std::string::npos) return 0.0;
    h = std::atoi(s.substr(0, a).c_str());
    m = std::atoi(s.substr(a + 1, b - a - 1).c_str());
    sec = std::atof(s.substr(b + 1).c_str());
    const double v = h * 3600.0 + m * 60.0 + sec;
    return v > 0.0 && v < 1e9 ? v : 0.0;
}

struct MkvTrack {
    std::uint64_t number = 0;
    std::uint64_t uid = 0;
    std::uint64_t default_duration = 0;
    std::uint64_t codec_delay = 0;
    std::uint64_t seek_preroll = 0;
    std::vector<std::uint8_t> strip;
    std::int64_t delay_ticks = 0;
    bool webvtt_lines = false;
    DurationEstimator estimator;
    SeekTarget target;
};

struct Cue {
    double time = 0.0;
    std::uint64_t track = 0;
    std::uint64_t position = 0;
};

struct KeyEntry {
    double time = 0.0;
    std::size_t track = 0;
    std::uint64_t cluster = 0;
};

std::string map_codec(const std::string& id, std::vector<std::uint8_t>& priv, TrackInfo& info) {
    if (id == "V_MPEG4/ISO/AVC") return "h264";
    if (id == "V_MPEGH/ISO/HEVC") return "hevc";
    if (id == "V_AV1") return "av1";
    if (id == "V_VP8") return "vp8";
    if (id == "V_VP9") return "vp9";
    if (id.compare(0, 12, "V_MPEG4/ISO/") == 0) return "mpeg4";
    if (id == "V_MPEG1") return "mpeg1video";
    if (id == "V_MPEG2") return "mpeg2video";
    if (id == "V_THEORA") return "theora";
    if (id == "V_MJPEG") return "mjpeg";
    if (id == "V_PRORES") return "prores";
    if (id == "V_FFV1") return "ffv1";
    if (id == "V_UNCOMPRESSED") return "rawvideo";
    if (id == "V_MS/VFW/FOURCC") {
        if (priv.size() < 40) return "unknown";
        const std::string cc(reinterpret_cast<const char*>(priv.data() + 16), 4);
        std::vector<std::uint8_t> extra(priv.begin() + 40, priv.end());
        priv.swap(extra);
        const std::string up = to_lower(cc);
        if (up == "h264" || up == "avc1" || up == "x264") return "h264";
        if (up == "hevc" || up == "h265" || up == "hvc1") return "hevc";
        if (up == "xvid" || up == "divx" || up == "dx50" || up == "fmp4" || up == "mp4v") return "mpeg4";
        if (up == "mjpg") return "mjpeg";
        return up;
    }
    if (id.compare(0, 5, "A_AAC") == 0) {
        if (priv.empty()) {
            int profile = 2;
            if (id.find("MAIN") != std::string::npos) profile = 1;
            else if (id.find("SSR") != std::string::npos) profile = 3;
            else if (id.find("LTP") != std::string::npos) profile = 4;
            priv = make_aac_config(profile, info.sample_rate, info.channels);
        }
        return "aac";
    }
    if (id == "A_MPEG/L3") return "mp3";
    if (id == "A_MPEG/L2") return "mp2";
    if (id == "A_MPEG/L1") return "mp1";
    if (id == "A_OPUS") return "opus";
    if (id == "A_VORBIS") return "vorbis";
    if (id == "A_FLAC") return "flac";
    if (id == "A_AC3" || id == "A_AC3/BSID9" || id == "A_AC3/BSID10") return "ac3";
    if (id == "A_EAC3") return "eac3";
    if (id == "A_DTS") return "dts";
    if (id == "A_TRUEHD") return "truehd";
    if (id == "A_ALAC") {
        if (priv.size() == 28) priv.erase(priv.begin(), priv.begin() + 4);
        return "alac";
    }
    if (id == "A_PCM/INT/LIT") {
        if (info.bits_per_sample == 8) return "pcm_u8";
        return "pcm_s" + std::to_string(info.bits_per_sample ? info.bits_per_sample : 16) + "le";
    }
    if (id == "A_PCM/INT/BIG") {
        if (info.bits_per_sample == 8) return "pcm_s8";
        return "pcm_s" + std::to_string(info.bits_per_sample ? info.bits_per_sample : 16) + "be";
    }
    if (id == "A_PCM/FLOAT/IEEE") return info.bits_per_sample == 64 ? "pcm_f64le" : "pcm_f32le";
    if (id == "A_MS/ACM") {
        if (priv.size() < 18) return "unknown";
        ByteReader w(priv.data(), priv.size());
        std::uint16_t tag = w.le16();
        const std::uint16_t channels = w.le16();
        const std::uint32_t rate = w.le32();
        w.skip(6);
        const std::uint16_t bits = w.le16();
        const std::uint16_t extra = w.le16();
        if (tag == 0xFFFE && extra >= 22 && w.left() >= 22) {
            ByteReader x(w.here() + 6, 2);
            tag = x.le16();
        }
        if (channels) info.channels = channels;
        if (rate) info.sample_rate = rate;
        if (bits) info.bits_per_sample = bits;
        std::vector<std::uint8_t> rest(w.here(), w.here() + std::min<std::size_t>(extra, w.left()));
        switch (tag) {
            case 1:
                priv.clear();
                if (bits == 8) return "pcm_u8";
                return "pcm_s" + std::to_string(bits ? bits : 16) + "le";
            case 3: priv.clear(); return bits == 64 ? "pcm_f64le" : "pcm_f32le";
            case 6: priv.clear(); return "pcm_alaw";
            case 7: priv.clear(); return "pcm_mulaw";
            case 0x50: priv.clear(); return "mp2";
            case 0x55: priv.clear(); return "mp3";
            case 0xFF: case 0x1610: priv.swap(rest); return "aac";
            case 0x2000: priv.clear(); return "ac3";
            case 0xF1AC: priv.swap(rest); return "flac";
            default: priv.swap(rest); return "unknown";
        }
    }
    if (id == "S_TEXT/UTF8" || id == "S_TEXT/ASCII") return "subrip";
    if (id == "S_TEXT/ASS" || id == "S_ASS" || id == "S_TEXT/SSA" || id == "S_SSA") return "ass";
    if (id == "S_TEXT/WEBVTT" || id == "D_WEBVTT/SUBTITLES" || id == "D_WEBVTT/CAPTIONS" || id == "D_WEBVTT/DESCRIPTIONS") return "webvtt";
    if (id == "S_VOBSUB") return "dvd_subtitle";
    if (id == "S_HDMV/PGS") return "hdmv_pgs_subtitle";
    if (id == "S_DVBSUB") return "dvb_subtitle";
    return to_lower(id);
}

class MatroskaReader : public Reader {
public:
    bool open(Source& source, std::string* error) override;
    bool read(Packet& out) override;
    bool seek(double seconds, bool before) override;

private:
    struct Header {
        std::uint32_t id = 0;
        std::uint64_t offset = 0;
        std::uint64_t body = 0;
        std::uint64_t size = 0;
    };

    bool read_header(std::uint64_t pos, Header& h);
    bool read_body(const Header& h, std::vector<std::uint8_t>& out);
    void parse_level1(const Header& h);
    void parse_seekhead(const std::uint8_t* data, std::size_t size, std::vector<std::pair<std::uint32_t, std::uint64_t>>& seeks);
    void parse_info(const std::uint8_t* data, std::size_t size);
    void parse_tracks(const std::uint8_t* data, std::size_t size);
    void parse_track(const std::uint8_t* data, std::size_t size);
    void parse_cues(const std::uint8_t* data, std::size_t size);
    void parse_tags(const std::uint8_t* data, std::size_t size);
    int track_index(std::uint64_t number) const noexcept;
    std::uint64_t resync(std::uint64_t from);
    bool parse_block(const std::uint8_t* data, std::size_t size, std::uint64_t position, bool simple, bool has_duration, std::uint64_t block_duration, bool referenced);
    bool parse_blockgroup(const std::uint8_t* data, std::size_t size, std::uint64_t position);
    void build_key_index();
    double ticks_to_seconds(std::int64_t ticks) const noexcept { return static_cast<double>(ticks) * static_cast<double>(m_timecode_scale) / 1e9; }

    Source* m_src = nullptr;
    std::uint64_t m_file = 0;
    std::uint64_t m_seg_start = 0;
    std::uint64_t m_seg_end = 0;
    std::uint64_t m_timecode_scale = 1000000;
    double m_info_duration = 0.0;
    std::string m_title;
    std::vector<MkvTrack> m_tracks;
    std::vector<Cue> m_cues;
    bool m_have_info = false, m_have_tracks = false, m_have_cues = false, m_have_tags = false;
    std::uint64_t m_first_cluster = 0;
    std::uint64_t m_pos = 0;
    bool m_in_cluster = false;
    std::uint64_t m_cluster_end = 0;
    std::uint64_t m_cluster_pos = 0;
    std::int64_t m_cluster_tc = 0;
    std::vector<Packet> m_queue;
    std::size_t m_queue_pos = 0;
    bool m_index_built = false;
    std::vector<KeyEntry> m_keys;
    std::vector<std::pair<double, std::uint64_t>> m_clusters;
    std::vector<std::uint8_t> m_buf;
};

int MatroskaReader::track_index(std::uint64_t number) const noexcept {
    for (std::size_t i = 0; i < m_tracks.size(); ++i) {
        if (m_tracks[i].number == number) return static_cast<int>(i);
    }
    return -1;
}

bool MatroskaReader::read_header(std::uint64_t pos, Header& h) {
    std::uint8_t b[12];
    if (pos >= m_file) return false;
    const std::size_t got = m_src->read_at(pos, b, sizeof(b));
    std::size_t used = 0;
    if (!parse_id(b, got, h.id, used)) return false;
    std::size_t used2 = 0;
    if (!parse_size(b + used, got - used, h.size, used2)) return false;
    h.offset = pos;
    h.body = pos + used + used2;
    return true;
}

bool MatroskaReader::read_body(const Header& h, std::vector<std::uint8_t>& out) {
    if (h.size == k_unknown || h.size > k_max_meta) return false;
    std::uint64_t size = h.size;
    if (h.body >= m_file) return false;
    if (size > m_file - h.body) size = m_file - h.body;
    return m_src->read_vector(h.body, static_cast<std::size_t>(size), out);
}

void MatroskaReader::parse_seekhead(const std::uint8_t* data, std::size_t size, std::vector<std::pair<std::uint32_t, std::uint64_t>>& seeks) {
    ByteReader r(data, size);
    Element e;
    while (next_element(r, e)) {
        if (e.id != ID_SEEK) continue;
        ByteReader s(e.data, e.size);
        Element c;
        std::uint32_t id = 0;
        std::uint64_t pos = k_unknown;
        while (next_element(s, c)) {
            if (c.id == ID_SEEKID) id = static_cast<std::uint32_t>(ebml_uint(c));
            else if (c.id == ID_SEEKPOSITION) pos = ebml_uint(c);
        }
        if (id && pos != k_unknown && seeks.size() < 256) seeks.emplace_back(id, pos);
    }
}

void MatroskaReader::parse_info(const std::uint8_t* data, std::size_t size) {
    m_have_info = true;
    ByteReader r(data, size);
    Element e;
    double dur = 0.0;
    while (next_element(r, e)) {
        if (e.id == ID_TIMECODESCALE) {
            const std::uint64_t v = ebml_uint(e);
            if (v > 0 && v <= 1000000000000ull) m_timecode_scale = v;
        } else if (e.id == ID_DURATION) {
            dur = ebml_float(e);
        } else if (e.id == ID_TITLE) {
            m_title = ebml_string(e);
        }
    }
    if (dur > 0.0 && std::isfinite(dur)) m_info_duration = dur;
}

void MatroskaReader::parse_tracks(const std::uint8_t* data, std::size_t size) {
    m_have_tracks = true;
    ByteReader r(data, size);
    Element e;
    while (next_element(r, e)) {
        if (e.id == ID_TRACKENTRY && m_tracks.size() < k_max_tracks) parse_track(e.data, e.size);
    }
}

void MatroskaReader::parse_track(const std::uint8_t* data, std::size_t size) {
    MkvTrack t;
    TrackInfo info;
    info.is_default = true;
    std::uint64_t type = 0;
    std::string codec_id;
    std::vector<std::uint8_t> priv;
    std::string lang = "eng";
    std::string bcp47;
    double sampling = 0.0;
    double output_sampling = 0.0;
    std::uint64_t display_w = 0, display_h = 0, display_unit = 0;
    double roll = 0.0;
    double frame_rate = 0.0;
    bool unsupported = false;
    bool enabled = true;
    ByteReader r(data, size);
    Element e;
    while (next_element(r, e)) {
        switch (e.id) {
            case ID_TRACKNUMBER: t.number = ebml_uint(e); break;
            case ID_TRACKUID: t.uid = ebml_uint(e); break;
            case ID_TRACKTYPE: type = ebml_uint(e); break;
            case ID_FLAGENABLED: enabled = ebml_uint(e) != 0; break;
            case ID_FLAGDEFAULT: info.is_default = ebml_uint(e) != 0; break;
            case ID_DEFAULTDURATION: t.default_duration = ebml_uint(e); break;
            case ID_NAME: info.name = ebml_string(e); break;
            case ID_LANGUAGE: lang = ebml_string(e); break;
            case ID_LANGUAGEBCP47: bcp47 = ebml_string(e); break;
            case ID_CODECID: codec_id = ebml_string(e); break;
            case ID_CODECPRIVATE: priv.assign(e.data, e.data + e.size); break;
            case ID_CODECDELAY: t.codec_delay = ebml_uint(e); break;
            case ID_SEEKPREROLL: t.seek_preroll = ebml_uint(e); break;
            case ID_VIDEO: {
                ByteReader v(e.data, e.size);
                Element c;
                while (next_element(v, c)) {
                    if (c.id == ID_PIXELWIDTH) info.width = static_cast<std::uint32_t>(std::min<std::uint64_t>(ebml_uint(c), 1u << 20));
                    else if (c.id == ID_PIXELHEIGHT) info.height = static_cast<std::uint32_t>(std::min<std::uint64_t>(ebml_uint(c), 1u << 20));
                    else if (c.id == ID_DISPLAYWIDTH) display_w = ebml_uint(c);
                    else if (c.id == ID_DISPLAYHEIGHT) display_h = ebml_uint(c);
                    else if (c.id == ID_DISPLAYUNIT) display_unit = ebml_uint(c);
                    else if (c.id == ID_FRAMERATE) frame_rate = ebml_float(c);
                    else if (c.id == ID_PROJECTION) {
                        ByteReader p(c.data, c.size);
                        Element pc;
                        while (next_element(p, pc)) {
                            if (pc.id == ID_PROJECTIONPOSEROLL) roll = ebml_float(pc);
                        }
                    }
                }
                break;
            }
            case ID_AUDIO: {
                ByteReader a(e.data, e.size);
                Element c;
                while (next_element(a, c)) {
                    if (c.id == ID_SAMPLINGFREQUENCY) sampling = ebml_float(c);
                    else if (c.id == ID_OUTPUTSAMPLINGFREQUENCY) output_sampling = ebml_float(c);
                    else if (c.id == ID_CHANNELS) info.channels = static_cast<std::uint16_t>(std::min<std::uint64_t>(ebml_uint(c), 255));
                    else if (c.id == ID_BITDEPTH) info.bits_per_sample = static_cast<std::uint16_t>(std::min<std::uint64_t>(ebml_uint(c), 64));
                }
                break;
            }
            case ID_CONTENTENCODINGS: {
                ByteReader cr(e.data, e.size);
                Element enc;
                while (next_element(cr, enc)) {
                    if (enc.id != ID_CONTENTENCODING) continue;
                    ByteReader er(enc.data, enc.size);
                    Element c;
                    std::uint64_t scope = 1;
                    std::uint64_t etype = 0;
                    std::uint64_t algo = 0;
                    bool has_compression = false;
                    std::vector<std::uint8_t> settings;
                    while (next_element(er, c)) {
                        if (c.id == ID_CONTENTENCODINGSCOPE) scope = ebml_uint(c);
                        else if (c.id == ID_CONTENTENCODINGTYPE) etype = ebml_uint(c);
                        else if (c.id == ID_CONTENTENCRYPTION) etype = 1;
                        else if (c.id == ID_CONTENTCOMPRESSION) {
                            has_compression = true;
                            ByteReader cc(c.data, c.size);
                            Element s;
                            while (next_element(cc, s)) {
                                if (s.id == ID_CONTENTCOMPALGO) algo = ebml_uint(s);
                                else if (s.id == ID_CONTENTCOMPSETTINGS) settings.assign(s.data, s.data + s.size);
                            }
                        }
                    }
                    if (etype != 0 || !has_compression || algo != 3) {
                        unsupported = true;
                    } else if (scope & 1) {
                        t.strip = settings;
                    }
                }
                break;
            }
            default:
                break;
        }
    }
    if (t.number == 0 || track_index(t.number) >= 0) return;
    if (!enabled) info.is_default = false;
    switch (type) {
        case 1: info.type = MediaType::video; break;
        case 2: info.type = MediaType::audio; break;
        case 0x11: info.type = MediaType::subtitle; break;
        default: info.type = MediaType::other; break;
    }
    info.id = static_cast<std::uint32_t>(std::min<std::uint64_t>(t.number, 0xFFFFFFFFu));
    info.timescale = 1e9 / static_cast<double>(m_timecode_scale);
    info.language = bcp47.empty() ? lang : bcp47;
    if (info.language == "und") info.language.clear();
    if (output_sampling > 0.0 && output_sampling < 1e7) info.sample_rate = static_cast<std::uint32_t>(output_sampling + 0.5);
    else if (sampling > 0.0 && sampling < 1e7) info.sample_rate = static_cast<std::uint32_t>(sampling + 0.5);
    if (info.type == MediaType::audio && info.channels == 0) info.channels = 1;
    info.codec = map_codec(codec_id, priv, info);
    info.codec_config = std::move(priv);
    if (info.codec == "opus") {
        std::uint16_t ch = 0, pre = 0;
        std::uint32_t rate = 0;
        if (opus_head_info(info.codec_config, ch, pre, rate) && ch) info.channels = ch;
        info.sample_rate = 48000;
    } else if (info.codec == "aac") {
        AacConfig c;
        if (parse_aac_config(info.codec_config.data(), info.codec_config.size(), c) && c.channels) info.channels = static_cast<std::uint16_t>(c.channels);
    }
    if (unsupported) info.codec.clear();
    if (info.type == MediaType::video) {
        if (t.default_duration) info.frame_rate = 1e9 / static_cast<double>(t.default_duration);
        else if (frame_rate > 0.0 && std::isfinite(frame_rate)) info.frame_rate = frame_rate;
        info.display_width = info.width;
        info.display_height = info.height;
        if (display_w && display_h && display_w < (1u << 24) && display_h < (1u << 24)) {
            if (display_unit == 0) {
                info.display_width = static_cast<std::uint32_t>(display_w);
                info.display_height = static_cast<std::uint32_t>(display_h);
            } else if (display_unit == 3 && info.height) {
                info.display_width = static_cast<std::uint32_t>(std::lround(static_cast<double>(info.height) * display_w / display_h));
            }
        }
        if (std::isfinite(roll) && roll != 0.0) {
            int rot = static_cast<int>(std::lround(-roll / 90.0)) * 90;
            info.rotation = ((rot % 360) + 360) % 360;
        }
    }
    if (t.codec_delay) t.delay_ticks = static_cast<std::int64_t>((std::min<std::uint64_t>(t.codec_delay, std::uint64_t(1) << 52) + m_timecode_scale / 2) / m_timecode_scale);
    t.webvtt_lines = codec_id.compare(0, 8, "D_WEBVTT") == 0;
    t.estimator.init(info);
    m_tracks.push_back(std::move(t));
    tracks.push_back(std::move(info));
}

void MatroskaReader::parse_cues(const std::uint8_t* data, std::size_t size) {
    m_have_cues = true;
    ByteReader r(data, size);
    Element e;
    while (next_element(r, e)) {
        if (e.id != ID_CUEPOINT) continue;
        ByteReader p(e.data, e.size);
        Element c;
        std::uint64_t time = 0;
        std::vector<std::pair<std::uint64_t, std::uint64_t>> positions;
        while (next_element(p, c)) {
            if (c.id == ID_CUETIME) {
                time = ebml_uint(c);
            } else if (c.id == ID_CUETRACKPOSITIONS) {
                ByteReader tp(c.data, c.size);
                Element x;
                std::uint64_t track = 0;
                std::uint64_t pos = k_unknown;
                while (next_element(tp, x)) {
                    if (x.id == ID_CUETRACK) track = ebml_uint(x);
                    else if (x.id == ID_CUECLUSTERPOSITION) pos = ebml_uint(x);
                }
                if (pos != k_unknown && positions.size() < 64) positions.emplace_back(track, pos);
            }
        }
        for (const auto& p2 : positions) {
            if (m_cues.size() >= k_max_cues) return;
            if (p2.second >= m_file) continue;
            Cue cue;
            cue.time = ticks_to_seconds(static_cast<std::int64_t>(std::min<std::uint64_t>(time, std::uint64_t(1) << 62)));
            cue.track = p2.first;
            cue.position = p2.second;
            m_cues.push_back(cue);
        }
    }
}

void MatroskaReader::parse_tags(const std::uint8_t* data, std::size_t size) {
    m_have_tags = true;
    ByteReader r(data, size);
    Element e;
    while (next_element(r, e)) {
        if (e.id != ID_TAG) continue;
        ByteReader tr(e.data, e.size);
        Element c;
        std::vector<std::uint64_t> track_uids;
        bool other_target = false;
        std::vector<std::pair<std::string, std::string>> simple;
        while (next_element(tr, c)) {
            if (c.id == ID_TARGETS) {
                ByteReader g(c.data, c.size);
                Element x;
                while (next_element(g, x)) {
                    if (x.id == ID_TAGTRACKUID) {
                        if (ebml_uint(x) != 0 && track_uids.size() < 64) track_uids.push_back(ebml_uint(x));
                    } else if (x.id == ID_TAGEDITIONUID || x.id == ID_TAGCHAPTERUID || x.id == ID_TAGATTACHMENTUID) {
                        if (ebml_uint(x) != 0) other_target = true;
                    }
                }
            } else if (c.id == ID_SIMPLETAG) {
                ByteReader s(c.data, c.size);
                Element x;
                std::string name, value;
                while (next_element(s, x)) {
                    if (x.id == ID_TAGNAME) name = ebml_string(x);
                    else if (x.id == ID_TAGSTRING) value = ebml_string(x);
                }
                if (!name.empty() && simple.size() < 256) simple.emplace_back(to_lower(name), value);
            }
        }
        if (other_target) continue;
        if (track_uids.empty()) {
            for (const auto& kv : simple) add_tag(tags, kv.first, kv.second);
            continue;
        }
        for (std::uint64_t uid : track_uids) {
            for (std::size_t i = 0; i < m_tracks.size(); ++i) {
                if (m_tracks[i].uid != uid) continue;
                for (const auto& kv : simple) {
                    if (kv.first == "duration") {
                        const double d = parse_duration_tag(kv.second);
                        if (d > 0.0) tracks[i].duration = d;
                    } else if (kv.first == "bps") {
                        const double v = std::atof(kv.second.c_str());
                        if (v > 0.0 && v < 4e9) tracks[i].bitrate = static_cast<std::uint32_t>(v);
                    } else if (kv.first == "number_of_frames") {
                        const double v = std::atof(kv.second.c_str());
                        if (v > 0.0 && v < 1e15) tracks[i].frame_count = static_cast<std::uint64_t>(v);
                    } else if (kv.first == "title" && tracks[i].name.empty()) {
                        tracks[i].name = kv.second;
                    }
                }
            }
        }
    }
}

void MatroskaReader::parse_level1(const Header& h) {
    if (h.id == ID_INFO && m_have_info) return;
    if (h.id == ID_TRACKS && m_have_tracks) return;
    if (h.id == ID_CUES && m_have_cues) return;
    if (h.id == ID_TAGS && m_have_tags) return;
    if (h.id != ID_INFO && h.id != ID_TRACKS && h.id != ID_CUES && h.id != ID_TAGS) return;
    std::vector<std::uint8_t> body;
    if (!read_body(h, body)) return;
    if (h.id == ID_INFO) parse_info(body.data(), body.size());
    else if (h.id == ID_TRACKS) parse_tracks(body.data(), body.size());
    else if (h.id == ID_CUES) parse_cues(body.data(), body.size());
    else parse_tags(body.data(), body.size());
}

bool MatroskaReader::open(Source& source, std::string* error) {
    m_src = &source;
    m_file = source.size();
    Header h;
    if (!read_header(0, h) || h.id != ID_EBML || h.size == k_unknown) {
        if (error) *error = "not a Matroska file";
        return false;
    }
    std::vector<std::uint8_t> body;
    format = "matroska";
    if (read_body(h, body)) {
        ByteReader r(body.data(), body.size());
        Element e;
        while (next_element(r, e)) {
            if (e.id == ID_DOCTYPE && ebml_string(e) == "webm") format = "webm";
        }
    }
    std::uint64_t pos = h.body + h.size;
    bool found = false;
    for (int i = 0; i < 64 && pos < m_file; ++i) {
        if (!read_header(pos, h)) break;
        if (h.id == ID_SEGMENT) {
            found = true;
            break;
        }
        if (h.size == k_unknown) break;
        pos = h.body + h.size;
    }
    if (!found) {
        if (error) *error = "no Matroska segment";
        return false;
    }
    m_seg_start = h.body;
    m_seg_end = (h.size == k_unknown || h.size > m_file - std::min(m_file, h.body)) ? m_file : h.body + h.size;
    std::vector<std::pair<std::uint32_t, std::uint64_t>> seeks;
    pos = m_seg_start;
    std::size_t count = 0;
    while (pos < m_seg_end && count++ < 100000) {
        if (!read_header(pos, h)) break;
        if (h.id == ID_CLUSTER) {
            m_first_cluster = pos;
            break;
        }
        if (h.size == k_unknown || !is_segment_child(h.id)) break;
        if (h.id == ID_SEEKHEAD) {
            if (read_body(h, body)) parse_seekhead(body.data(), body.size(), seeks);
        } else {
            parse_level1(h);
        }
        if (h.size > m_file - std::min(m_file, h.body)) {
            pos = m_file;
            break;
        }
        pos = h.body + h.size;
    }
    if (m_first_cluster == 0) m_first_cluster = pos;
    bool second_head = false;
    for (std::size_t i = 0; i < seeks.size(); ++i) {
        const std::uint64_t at = m_seg_start + seeks[i].second;
        if (seeks[i].second >= m_file || at >= m_file) continue;
        Header sh;
        if (!read_header(at, sh) || sh.id != seeks[i].first) continue;
        if (sh.id == ID_SEEKHEAD) {
            if (second_head) continue;
            second_head = true;
            if (read_body(sh, body)) parse_seekhead(body.data(), body.size(), seeks);
            continue;
        }
        parse_level1(sh);
    }
    if (m_tracks.empty()) {
        if (error) *error = "no tracks in Matroska file";
        return false;
    }
    if (!m_title.empty()) {
        bool has = false;
        for (const auto& t : tags) has = has || t.first == "title";
        if (!has) tags.insert(tags.begin(), std::make_pair(std::string("title"), m_title));
    }
    duration = m_info_duration > 0.0 ? m_info_duration * static_cast<double>(m_timecode_scale) / 1e9 : 0.0;
    for (TrackInfo& t : tracks) {
        if (t.duration <= 0.0) t.duration = duration;
        if (duration <= 0.0) duration = std::max(duration, t.duration);
    }
    m_pos = m_first_cluster;
    return true;
}

std::uint64_t MatroskaReader::resync(std::uint64_t from) {
    std::uint8_t buf[4096];
    std::uint64_t pos = from;
    while (pos < m_file) {
        const std::size_t got = m_src->read_at(pos, buf, sizeof(buf));
        if (got < 4) return m_file;
        for (std::size_t i = 0; i + 3 < got; ++i) {
            if (buf[i] == 0x1F && buf[i + 1] == 0x43 && buf[i + 2] == 0xB6 && buf[i + 3] == 0x75) {
                Header h;
                const std::uint64_t at = pos + i;
                if (!read_header(at, h) || h.id != ID_CLUSTER) continue;
                if (h.size != k_unknown && h.size > m_file - std::min(m_file, h.body) + (std::uint64_t(1) << 20)) continue;
                Header c;
                if (!read_header(h.body, c)) continue;
                if (c.id != ID_TIMECODE && c.id != 0xA7 && c.id != 0xAB && c.id != 0xBF && c.id != 0xEC) continue;
                return at;
            }
        }
        pos += got - 3;
    }
    return m_file;
}

bool MatroskaReader::parse_block(const std::uint8_t* data, std::size_t size, std::uint64_t position, bool simple, bool has_duration, std::uint64_t block_duration, bool referenced) {
    std::uint64_t number = 0;
    std::size_t used = 0;
    if (!parse_size(data, size, number, used) || number == k_unknown) return false;
    if (size < used + 3) return false;
    const int index = track_index(number);
    if (index < 0 || !is_enabled(static_cast<std::size_t>(index))) return true;
    const std::size_t ti = static_cast<std::size_t>(index);
    MkvTrack& t = m_tracks[ti];
    const std::int16_t rel = static_cast<std::int16_t>((data[used] << 8) | data[used + 1]);
    const std::uint8_t flags = data[used + 2];
    std::size_t pos = used + 3;
    const int lacing = (flags >> 1) & 3;
    std::vector<std::size_t> sizes;
    if (lacing == 0) {
        sizes.push_back(size - pos);
    } else {
        if (pos >= size) return false;
        const std::size_t count = static_cast<std::size_t>(data[pos++]) + 1;
        if (count > k_max_laces) return false;
        if (lacing == 1) {
            std::size_t total = 0;
            for (std::size_t i = 0; i + 1 < count; ++i) {
                std::size_t n = 0;
                for (;;) {
                    if (pos >= size) return false;
                    const std::uint8_t v = data[pos++];
                    n += v;
                    if (v < 255) break;
                }
                sizes.push_back(n);
                total += n;
                if (total > size) return false;
            }
            if (pos + total > size) return false;
            sizes.push_back(size - pos - total);
        } else if (lacing == 3) {
            std::uint64_t first = 0;
            if (!parse_size(data + pos, size - pos, first, used) || first == k_unknown || first > size) return false;
            pos += used;
            std::int64_t current = static_cast<std::int64_t>(first);
            std::uint64_t total = first;
            sizes.push_back(static_cast<std::size_t>(first));
            for (std::size_t i = 1; i + 1 < count; ++i) {
                std::uint64_t raw = 0;
                if (!parse_size(data + pos, size - pos, raw, used) || raw == k_unknown) return false;
                const std::int64_t bias = (std::int64_t(1) << (7 * used - 1)) - 1;
                current += static_cast<std::int64_t>(raw) - bias;
                pos += used;
                if (current < 0 || static_cast<std::uint64_t>(current) > size) return false;
                sizes.push_back(static_cast<std::size_t>(current));
                total += static_cast<std::uint64_t>(current);
                if (total > size) return false;
            }
            if (pos + total > size) return false;
            sizes.push_back(static_cast<std::size_t>(size - pos - total));
        } else {
            const std::size_t rest = size - pos;
            if (rest % count != 0) return false;
            for (std::size_t i = 0; i < count; ++i) sizes.push_back(rest / count);
        }
    }
    const bool key = simple ? (flags & 0x80) != 0 : !referenced;
    double pts = ticks_to_seconds(m_cluster_tc + rel) - static_cast<double>(std::min<std::uint64_t>(t.codec_delay, std::uint64_t(1) << 52)) / 1e9;
    double each = 0.0;
    if (has_duration) each = ticks_to_seconds(static_cast<std::int64_t>(std::min<std::uint64_t>(block_duration, std::uint64_t(1) << 50))) / static_cast<double>(sizes.size());
    else if (t.default_duration) each = static_cast<double>(t.default_duration) / 1e9;
    for (std::size_t i = 0; i < sizes.size(); ++i) {
        if (pos + sizes[i] > size) return false;
        Packet p;
        p.track = static_cast<std::uint32_t>(ti);
        p.data.reserve(t.strip.size() + sizes[i]);
        p.data.insert(p.data.end(), t.strip.begin(), t.strip.end());
        p.data.insert(p.data.end(), data + pos, data + pos + sizes[i]);
        pos += sizes[i];
        if (t.webvtt_lines) {
            std::size_t cut = 0;
            for (int line = 0; line < 2; ++line) {
                const auto it = std::find(p.data.begin() + static_cast<std::ptrdiff_t>(cut), p.data.end(), static_cast<std::uint8_t>('\n'));
                if (it == p.data.end()) {
                    cut = 0;
                    break;
                }
                cut = static_cast<std::size_t>(it - p.data.begin()) + 1;
            }
            p.data.erase(p.data.begin(), p.data.begin() + static_cast<std::ptrdiff_t>(cut));
        }
        double d = each;
        if (d <= 0.0 && t.estimator.active()) d = t.estimator.seconds(p.data.data(), p.data.size());
        p.pts = pts;
        p.dts = pts;
        p.duration = d;
        p.keyframe = key;
        p.position = position;
        pts += d;
        if (!passes_target(t.target, p)) continue;
        if (m_queue.size() < k_max_queue) m_queue.push_back(std::move(p));
    }
    return true;
}

bool MatroskaReader::parse_blockgroup(const std::uint8_t* data, std::size_t size, std::uint64_t position) {
    ByteReader r(data, size);
    Element e;
    const std::uint8_t* block = nullptr;
    std::size_t block_size = 0;
    bool has_duration = false;
    std::uint64_t duration_ticks = 0;
    bool referenced = false;
    std::int64_t discard = 0;
    while (next_element(r, e)) {
        if (e.id == ID_BLOCK) {
            block = e.data;
            block_size = e.size;
        } else if (e.id == ID_BLOCKDURATION) {
            has_duration = true;
            duration_ticks = ebml_uint(e);
        } else if (e.id == ID_REFERENCEBLOCK) {
            referenced = true;
        } else if (e.id == 0x75A2 && e.size >= 1 && e.size <= 8) {
            const std::uint64_t raw = ebml_uint(e);
            const unsigned bits = static_cast<unsigned>(e.size) * 8u;
            discard = bits < 64 && (raw >> (bits - 1)) ? static_cast<std::int64_t>(raw) - static_cast<std::int64_t>(std::uint64_t(1) << bits) : static_cast<std::int64_t>(raw);
        }
    }
    if (!block) return true;
    const std::size_t before = m_queue.size();
    const bool ok = parse_block(block, block_size, position, false, has_duration, duration_ticks, referenced);
    if (ok && discard > 0 && m_queue.size() > before) m_queue.back().discard_padding = static_cast<double>(discard) / 1e9;
    return ok;
}

bool MatroskaReader::read(Packet& out) {
    std::size_t guard = 0;
    while (m_queue_pos >= m_queue.size()) {
        m_queue.clear();
        m_queue_pos = 0;
        if (++guard > (std::size_t(1) << 24)) return false;
        if (m_pos >= m_file) return false;
        if (m_in_cluster && m_cluster_end != k_unknown && m_pos >= m_cluster_end) {
            m_in_cluster = false;
            continue;
        }
        Header h;
        if (!read_header(m_pos, h)) {
            m_in_cluster = false;
            m_pos = resync(m_pos + 1);
            continue;
        }
        if (m_in_cluster) {
            if (is_top_level(h.id)) {
                m_in_cluster = false;
                continue;
            }
            const bool fits = h.size != k_unknown && (m_cluster_end == k_unknown || h.size <= m_cluster_end - std::min(m_cluster_end, h.body));
            if (!fits || !is_cluster_child(h.id)) {
                m_in_cluster = false;
                m_pos = resync(m_pos + 1);
                continue;
            }
            if (h.id == ID_TIMECODE) {
                if (h.size <= 8) {
                    std::uint8_t b[8];
                    if (m_src->read_exact(h.body, b, static_cast<std::size_t>(h.size))) {
                        std::uint64_t v = 0;
                        for (std::size_t i = 0; i < h.size; ++i) v = (v << 8) | b[i];
                        m_cluster_tc = static_cast<std::int64_t>(std::min<std::uint64_t>(v, std::uint64_t(1) << 52));
                    }
                }
            } else if (h.id == ID_SIMPLEBLOCK || h.id == ID_BLOCKGROUP) {
                if (h.size > k_max_packet + 1024 || h.body + h.size > m_file) {
                    m_pos = m_file;
                    continue;
                }
                if (m_src->read_vector(h.body, static_cast<std::size_t>(h.size), m_buf)) {
                    bool ok;
                    if (h.id == ID_SIMPLEBLOCK) ok = parse_block(m_buf.data(), m_buf.size(), h.offset, true, false, 0, false);
                    else ok = parse_blockgroup(m_buf.data(), m_buf.size(), h.offset);
                    (void)ok;
                }
            }
            m_pos = h.body + h.size;
            continue;
        }
        if (h.id == ID_CLUSTER) {
            m_in_cluster = true;
            m_cluster_pos = h.offset;
            m_cluster_tc = 0;
            m_cluster_end = h.size == k_unknown ? k_unknown : (h.size > m_file - std::min(m_file, h.body) ? m_file : h.body + h.size);
            m_pos = h.body;
            continue;
        }
        if (h.id == ID_SEGMENT) {
            m_pos = h.body;
            continue;
        }
        if (h.size == k_unknown || h.size > m_file - std::min(m_file, h.body) || !is_segment_child(h.id)) {
            m_pos = resync(m_pos + 1);
            continue;
        }
        m_pos = h.body + h.size;
    }
    out = std::move(m_queue[m_queue_pos++]);
    return true;
}

void MatroskaReader::build_key_index() {
    if (m_index_built) return;
    m_index_built = true;
    std::uint64_t pos = m_first_cluster;
    bool in_cluster = false;
    std::uint64_t cluster_end = 0;
    std::uint64_t cluster_pos = 0;
    std::int64_t cluster_tc = 0;
    std::size_t guard = 0;
    std::vector<std::uint8_t> buf;
    while (pos < m_file && ++guard < (std::size_t(1) << 26)) {
        if (in_cluster && cluster_end != k_unknown && pos >= cluster_end) {
            in_cluster = false;
            continue;
        }
        Header h;
        if (!read_header(pos, h)) {
            in_cluster = false;
            pos = resync(pos + 1);
            continue;
        }
        if (in_cluster) {
            if (is_top_level(h.id)) {
                in_cluster = false;
                continue;
            }
            const bool fits = h.size != k_unknown && (cluster_end == k_unknown || h.size <= cluster_end - std::min(cluster_end, h.body));
            if (!fits || !is_cluster_child(h.id)) {
                in_cluster = false;
                pos = resync(pos + 1);
                continue;
            }
            if (h.id == ID_TIMECODE && h.size <= 8) {
                std::uint8_t b[8];
                if (m_src->read_exact(h.body, b, static_cast<std::size_t>(h.size))) {
                    std::uint64_t v = 0;
                    for (std::size_t i = 0; i < h.size; ++i) v = (v << 8) | b[i];
                    cluster_tc = static_cast<std::int64_t>(std::min<std::uint64_t>(v, std::uint64_t(1) << 52));
                    if (m_clusters.size() < k_max_cues) m_clusters.emplace_back(ticks_to_seconds(cluster_tc), cluster_pos);
                }
            } else if (h.id == ID_SIMPLEBLOCK || h.id == ID_BLOCKGROUP) {
                const std::uint8_t* block = nullptr;
                std::size_t block_size = 0;
                bool key = false;
                std::uint8_t head[16];
                if (h.id == ID_SIMPLEBLOCK) {
                    const std::size_t n = m_src->read_at(h.body, head, static_cast<std::size_t>(std::min<std::uint64_t>(h.size, sizeof(head))));
                    block = head;
                    block_size = n;
                } else if (h.size <= k_max_meta && m_src->read_vector(h.body, static_cast<std::size_t>(h.size), buf)) {
                    ByteReader r(buf.data(), buf.size());
                    Element e;
                    bool referenced = false;
                    while (next_element(r, e)) {
                        if (e.id == ID_BLOCK) {
                            block = e.data;
                            block_size = e.size;
                        } else if (e.id == ID_REFERENCEBLOCK) {
                            referenced = true;
                        }
                    }
                    key = !referenced;
                }
                std::uint64_t number = 0;
                std::size_t used = 0;
                if (block && parse_size(block, block_size, number, used) && block_size >= used + 3) {
                    if (h.id == ID_SIMPLEBLOCK) key = (block[used + 2] & 0x80) != 0;
                    const int index = track_index(number);
                    if (index >= 0 && key && tracks[static_cast<std::size_t>(index)].type == MediaType::video && m_keys.size() < k_max_cues) {
                        const std::int16_t rel = static_cast<std::int16_t>((block[used] << 8) | block[used + 1]);
                        KeyEntry k;
                        k.time = ticks_to_seconds(cluster_tc + rel);
                        k.track = static_cast<std::size_t>(index);
                        k.cluster = cluster_pos;
                        m_keys.push_back(k);
                    }
                }
            }
            pos = h.body + h.size;
            continue;
        }
        if (h.id == ID_CLUSTER) {
            in_cluster = true;
            cluster_pos = h.offset;
            cluster_tc = 0;
            cluster_end = h.size == k_unknown ? k_unknown : (h.size > m_file - std::min(m_file, h.body) ? m_file : h.body + h.size);
            pos = h.body;
            continue;
        }
        if (h.id == ID_SEGMENT) {
            pos = h.body;
            continue;
        }
        if (h.size == k_unknown || h.size > m_file - std::min(m_file, h.body) || !is_segment_child(h.id)) {
            pos = resync(pos + 1);
            continue;
        }
        pos = h.body + h.size;
    }
}

bool MatroskaReader::seek(double seconds, bool before) {
    const int ref = reference_track();
    if (ref < 0) return false;
    const std::size_t rt = static_cast<std::size_t>(ref);
    const bool ref_video = tracks[rt].type == MediaType::video;
    double ref_time = seconds;
    double key_time = seconds;
    std::uint64_t cluster = k_unknown;
    bool use_key = false;
    std::vector<KeyEntry> candidates;
    bool from_cues = false;
    for (const Cue& c : m_cues) {
        if (c.track == m_tracks[rt].number) {
            candidates.push_back(KeyEntry{c.time, rt, m_seg_start + c.position});
            from_cues = true;
        }
    }
    if (candidates.empty()) {
        for (const Cue& c : m_cues) candidates.push_back(KeyEntry{c.time, rt, m_seg_start + c.position});
        from_cues = !candidates.empty();
    }
    if (candidates.empty()) {
        build_key_index();
        if (ref_video) {
            for (const KeyEntry& k : m_keys) {
                if (k.track == rt) candidates.push_back(k);
            }
        }
        if (candidates.empty()) {
            for (const auto& c : m_clusters) candidates.push_back(KeyEntry{c.first, rt, c.second});
        }
    }
    const KeyEntry* chosen = nullptr;
    const KeyEntry* first = nullptr;
    const bool earlier = before || !ref_video;
    for (const KeyEntry& k : candidates) {
        if (!first || k.time < first->time) first = &k;
        if (earlier) {
            if (k.time <= seconds + 1e-9 && (!chosen || k.time > chosen->time || (k.time == chosen->time && k.cluster < chosen->cluster))) chosen = &k;
        } else if (k.time >= seconds - 1e-9 && (!chosen || k.time < chosen->time)) {
            chosen = &k;
        }
    }
    if (!chosen && earlier) chosen = first;
    for (MkvTrack& t : m_tracks) {
        t.target.active = false;
        t.estimator.reset();
    }
    m_queue.clear();
    m_queue_pos = 0;
    m_in_cluster = false;
    if (!chosen) {
        m_pos = m_file;
        return true;
    }
    cluster = chosen->cluster;
    if (ref_video) {
        key_time = chosen->time;
        ref_time = key_time;
        use_key = true;
    }
    (void)from_cues;
    m_pos = cluster;
    for (std::size_t i = 0; i < m_tracks.size(); ++i) {
        SeekTarget& target = m_tracks[i].target;
        target.active = true;
        target.want_key = use_key && i == rt;
        target.time = i == rt ? (use_key ? key_time : ref_time) : ref_time;
    }
    return true;
}

} // namespace

std::unique_ptr<Reader> make_matroska_reader() {
    return std::unique_ptr<Reader>(new MatroskaReader());
}

} // namespace demux_detail
} // namespace video
} // namespace fizmo
