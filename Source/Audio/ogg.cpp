#include "ogg.hpp"

#include "file_io.hpp"

#include <algorithm>
#include <cstring>
#include <utility>

namespace fizmo {
namespace audio {

struct OggReader::Source {
    virtual ~Source() = default;
    virtual std::size_t read_at(std::uint64_t offset, void* data, std::size_t bytes) = 0;
    std::uint64_t length = 0;
};

namespace {

const std::size_t k_cache_size = 65536;
const std::size_t k_max_streams = 256;
const std::uint64_t k_bisect_window = 65536;

struct FileSource : OggReader::Source {
    detail::File file;
    std::vector<std::uint8_t> cache;
    std::uint64_t cache_offset = 0;
    std::size_t cache_length = 0;

    std::size_t read_at(std::uint64_t offset, void* data, std::size_t bytes) override {
        if (offset >= length || bytes == 0) return 0;
        bytes = static_cast<std::size_t>(std::min<std::uint64_t>(bytes, length - offset));
        std::uint8_t* out = static_cast<std::uint8_t*>(data);
        std::size_t done = 0;
        while (done < bytes) {
            const std::uint64_t at = offset + done;
            if (at >= cache_offset && at < cache_offset + cache_length) {
                const std::size_t skip = static_cast<std::size_t>(at - cache_offset);
                const std::size_t n = std::min(bytes - done, cache_length - skip);
                std::memcpy(out + done, cache.data() + skip, n);
                done += n;
                continue;
            }
            if (bytes - done >= k_cache_size) {
                if (!file.seek(static_cast<std::int64_t>(at))) break;
                const std::size_t got = file.read(out + done, bytes - done);
                done += got;
                break;
            }
            cache.resize(k_cache_size);
            cache_length = 0;
            cache_offset = at;
            if (!file.seek(static_cast<std::int64_t>(at))) break;
            cache_length = file.read(cache.data(), k_cache_size);
            if (cache_length == 0) break;
        }
        return done;
    }
};

struct MemorySource : OggReader::Source {
    const std::uint8_t* data = nullptr;

    std::size_t read_at(std::uint64_t offset, void* out, std::size_t bytes) override {
        if (offset >= length || bytes == 0) return 0;
        const std::size_t n = static_cast<std::size_t>(std::min<std::uint64_t>(bytes, length - offset));
        std::memcpy(out, data + offset, n);
        return n;
    }
};

struct CrcTables {
    std::uint32_t t[8][256];
    CrcTables() {
        for (std::uint32_t i = 0; i < 256; ++i) {
            std::uint32_t r = i << 24;
            for (int k = 0; k < 8; ++k) r = (r & 0x80000000u) ? ((r << 1) ^ 0x04C11DB7u) : (r << 1);
            t[0][i] = r;
        }
        for (int k = 1; k < 8; ++k) {
            for (std::uint32_t i = 0; i < 256; ++i) t[k][i] = (t[k - 1][i] << 8) ^ t[0][t[k - 1][i] >> 24];
        }
    }
};

const CrcTables& crc_tables() {
    static const CrcTables tables;
    return tables;
}

std::int64_t granule_from(const std::uint8_t* p) noexcept {
    const std::uint64_t v = detail::read_le64(p);
    if (v & 0x8000000000000000ull) return -1;
    return static_cast<std::int64_t>(v);
}

} // namespace

namespace detail {

std::uint32_t ogg_crc(const std::uint8_t* data, std::size_t size, std::uint32_t crc) noexcept {
    const CrcTables& c = crc_tables();
    while (size >= 8) {
        crc ^= (std::uint32_t(data[0]) << 24) | (std::uint32_t(data[1]) << 16) | (std::uint32_t(data[2]) << 8) | std::uint32_t(data[3]);
        crc = c.t[7][crc >> 24] ^ c.t[6][(crc >> 16) & 255] ^ c.t[5][(crc >> 8) & 255] ^ c.t[4][crc & 255]
            ^ c.t[3][data[4]] ^ c.t[2][data[5]] ^ c.t[1][data[6]] ^ c.t[0][data[7]];
        data += 8;
        size -= 8;
    }
    while (size--) crc = (crc << 8) ^ c.t[0][(crc >> 24) ^ *data++];
    return crc;
}

} // namespace detail

std::size_t OggPage::packets_completed() const noexcept {
    std::size_t n = 0;
    for (std::uint8_t v : lacing) {
        if (v < 255) ++n;
    }
    return n;
}

std::size_t OggPage::packets_started() const noexcept {
    std::size_t n = 0;
    bool fresh = !continued();
    for (std::uint8_t v : lacing) {
        if (fresh) ++n;
        fresh = v < 255;
    }
    return n;
}

bool is_ogg(const void* data, std::size_t size) noexcept {
    if (!data || size < 27) return false;
    const std::uint8_t* p = static_cast<const std::uint8_t*>(data);
    return std::memcmp(p, "OggS", 4) == 0 && p[4] == 0 && (p[5] & ~7u) == 0;
}

OggReader::OggReader() = default;
OggReader::~OggReader() = default;
OggReader::OggReader(OggReader&&) noexcept = default;
OggReader& OggReader::operator=(OggReader&&) noexcept = default;

bool OggReader::open(const std::string& path, std::string* error) {
    close();
    auto src = std::make_unique<FileSource>();
    if (!src->file.open(path, "rb")) {
        if (error) *error = "can't open " + path;
        return false;
    }
    const std::int64_t size = src->file.size();
    src->length = size > 0 ? static_cast<std::uint64_t>(size) : 0;
    m_size = src->length;
    m_limit = m_size;
    m_source = std::move(src);
    return true;
}

bool OggReader::open(const void* data, std::size_t size, std::string* error) {
    close();
    if (!data && size) {
        if (error) *error = "no data";
        return false;
    }
    auto src = std::make_unique<MemorySource>();
    src->data = static_cast<const std::uint8_t*>(data);
    src->length = size;
    m_size = size;
    m_limit = size;
    m_source = std::move(src);
    return true;
}

void OggReader::close() {
    m_source.reset();
    m_size = 0;
    m_offset = 0;
    m_limit = 0;
    m_crc_failures = 0;
    m_filtered = false;
    m_filter = 0;
    m_streams.clear();
    m_ready.clear();
    m_ready_pos = 0;
}

bool OggReader::is_open() const noexcept {
    return m_source != nullptr;
}

std::uint64_t OggReader::size() const noexcept {
    return m_size;
}

std::size_t OggReader::read_at(std::uint64_t offset, void* data, std::size_t bytes) {
    return m_source ? m_source->read_at(offset, data, bytes) : 0;
}

bool OggReader::find_capture(std::uint64_t& pos, std::uint64_t end) {
    std::uint8_t buf[4096];
    while (pos < end) {
        const std::size_t got = read_at(pos, buf, sizeof(buf));
        if (got < 4) return false;
        const std::size_t span = got - 3;
        for (std::size_t i = 0; i < span; ++i) {
            if (buf[i] == 'O' && buf[i + 1] == 'g' && buf[i + 2] == 'g' && buf[i + 3] == 'S') {
                pos += i;
                return pos < end;
            }
        }
        pos += span;
    }
    return false;
}

bool OggReader::page_at(std::uint64_t from, std::uint64_t end, OggPage& page) {
    if (!m_source) return false;
    end = std::min(end, m_size);
    std::uint64_t pos = from;
    std::uint8_t head[27];
    while (pos < end) {
        if (!find_capture(pos, end)) return false;
        if (read_at(pos, head, 27) != 27) return false;
        if (head[4] != 0 || (head[5] & ~7u) != 0) {
            ++pos;
            continue;
        }
        const std::size_t segments = head[26];
        page.lacing.resize(segments);
        if (segments && read_at(pos + 27, page.lacing.data(), segments) != segments) {
            ++pos;
            continue;
        }
        std::size_t body = 0;
        for (std::uint8_t v : page.lacing) body += v;
        if (pos + 27 + segments + body > m_size) {
            ++pos;
            continue;
        }
        page.body.resize(body);
        if (body && read_at(pos + 27 + segments, page.body.data(), body) != body) {
            ++pos;
            continue;
        }
        std::uint8_t zeroed[27];
        std::memcpy(zeroed, head, 27);
        std::memset(zeroed + 22, 0, 4);
        std::uint32_t crc = detail::ogg_crc(zeroed, 27);
        crc = detail::ogg_crc(page.lacing.data(), segments, crc);
        crc = detail::ogg_crc(page.body.data(), body, crc);
        if (crc != detail::read_le32(head + 22)) {
            ++m_crc_failures;
            ++pos;
            continue;
        }
        page.offset = pos;
        page.flags = head[5];
        page.granule = granule_from(head + 6);
        page.serial = detail::read_le32(head + 14);
        page.sequence = detail::read_le32(head + 18);
        return true;
    }
    return false;
}

bool OggReader::read_page(OggPage& page) {
    if (!m_source || m_offset >= m_limit) return false;
    if (!page_at(m_offset, m_limit, page)) {
        m_offset = m_limit;
        return false;
    }
    m_offset = page.end();
    return true;
}

void OggReader::select_stream(std::uint32_t serial) {
    m_filtered = true;
    m_filter = serial;
    m_ready.clear();
    m_ready_pos = 0;
}

void OggReader::select_all_streams() {
    m_filtered = false;
}

void OggReader::reset_streams() {
    for (Stream& s : m_streams) {
        s.known = false;
        s.in_packet = false;
        s.oversize = false;
        s.gap = true;
        s.data.clear();
    }
    m_ready.clear();
    m_ready_pos = 0;
}

bool OggReader::seek(std::uint64_t offset) {
    if (!m_source || offset > m_size) return false;
    m_offset = offset;
    reset_streams();
    return true;
}

OggReader::Stream& OggReader::stream_for(std::uint32_t serial) {
    for (Stream& s : m_streams) {
        if (s.serial == serial) return s;
    }
    if (m_streams.size() >= k_max_streams) m_streams.erase(m_streams.begin());
    m_streams.emplace_back();
    m_streams.back().serial = serial;
    return m_streams.back();
}

void OggReader::absorb(const OggPage& page) {
    Stream& s = stream_for(page.serial);
    if (s.known && page.sequence != s.next_sequence) {
        s.data.clear();
        s.in_packet = false;
        s.oversize = false;
        s.gap = true;
    }
    s.known = true;
    s.next_sequence = page.sequence + 1;
    bool skipping = false;
    if (page.continued()) {
        if (!s.in_packet) skipping = true;
    } else if (s.in_packet) {
        s.data.clear();
        s.in_packet = false;
        s.oversize = false;
        s.gap = true;
    }
    std::size_t last_complete = page.lacing.size();
    for (std::size_t i = 0; i < page.lacing.size(); ++i) {
        if (page.lacing[i] < 255) last_complete = i;
    }
    bool first_on_page = true;
    std::size_t pos = 0;
    for (std::size_t i = 0; i < page.lacing.size(); ++i) {
        const std::size_t v = page.lacing[i];
        if (skipping) {
            pos += v;
            if (v < 255) {
                skipping = false;
                first_on_page = false;
                s.gap = true;
            }
            continue;
        }
        if (!s.oversize) {
            if (s.data.size() + v > max_packet_size) {
                s.oversize = true;
                s.data.clear();
            } else {
                s.data.insert(s.data.end(), page.body.begin() + static_cast<std::ptrdiff_t>(pos), page.body.begin() + static_cast<std::ptrdiff_t>(pos + v));
            }
        }
        s.in_packet = true;
        pos += v;
        if (v < 255) {
            if (!s.oversize) {
                OggPacket p;
                p.data = std::move(s.data);
                p.serial = page.serial;
                p.page_offset = page.offset;
                p.bos = page.bos() && first_on_page && !page.continued();
                p.last_on_page = i == last_complete;
                p.granule = p.last_on_page ? page.granule : -1;
                p.eos = p.last_on_page && page.eos();
                p.after_gap = s.gap;
                s.gap = false;
                m_ready.push_back(std::move(p));
            } else {
                s.gap = true;
            }
            s.data.clear();
            s.in_packet = false;
            s.oversize = false;
            first_on_page = false;
        }
    }
}

bool OggReader::read_packet(OggPacket& packet) {
    if (!m_source) return false;
    while (m_ready_pos == m_ready.size()) {
        m_ready.clear();
        m_ready_pos = 0;
        if (!read_page(m_page)) return false;
        if (m_filtered && m_page.serial != m_filter) continue;
        absorb(m_page);
    }
    packet = std::move(m_ready[m_ready_pos++]);
    return true;
}

bool OggReader::find_granule(std::uint32_t serial, std::int64_t target, std::uint64_t begin, std::uint64_t end, OggPage& page) {
    if (!m_source) return false;
    end = std::min(end, m_size);
    OggPage probe;
    bool found = false;
    std::uint64_t lo = begin;
    std::uint64_t hi = end;
    while (hi > lo && hi - lo > k_bisect_window) {
        const std::uint64_t mid = lo + (hi - lo) / 2;
        std::uint64_t pos = mid;
        bool got = false;
        while (page_at(pos, hi, probe)) {
            if (probe.serial == serial && probe.granule >= 0) {
                got = true;
                break;
            }
            pos = probe.end();
        }
        if (!got) {
            hi = mid;
        } else if (probe.granule <= target) {
            page = probe;
            found = true;
            lo = probe.end();
        } else {
            hi = mid;
        }
    }
    std::uint64_t pos = lo;
    while (page_at(pos, hi, probe)) {
        if (probe.serial == serial && probe.granule >= 0) {
            if (probe.granule > target) break;
            page = probe;
            found = true;
        }
        pos = probe.end();
    }
    return found;
}

bool OggReader::last_granule_page(std::uint32_t serial, std::uint64_t begin, std::uint64_t end, OggPage& page) {
    if (!m_source) return false;
    end = std::min(end, m_size);
    OggPage probe;
    std::uint64_t hi = end;
    std::uint64_t chunk = k_bisect_window;
    while (hi > begin) {
        const std::uint64_t lo = hi - begin > chunk ? hi - chunk : begin;
        bool found = false;
        std::uint64_t pos = lo;
        while (page_at(pos, hi, probe)) {
            if (probe.end() > end) break;
            if (probe.serial == serial && probe.granule >= 0) {
                page = probe;
                found = true;
            }
            pos = probe.end();
        }
        if (found) return true;
        hi = lo;
        if (chunk < (std::uint64_t(1) << 24)) chunk *= 2;
    }
    return false;
}

} // namespace audio
} // namespace fizmo
