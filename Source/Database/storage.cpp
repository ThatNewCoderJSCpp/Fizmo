#include "fizmo_library.hpp"
#include "storage.hpp"
#include "../Crypto/hash.hpp"

#include <cstring>
#include <filesystem>
#include <system_error>

#if defined(_WIN32)
    #include <io.h>
#else
    #include <unistd.h>
#endif // defined(_WIN32)

namespace fizmo {
namespace database {

namespace fs = std::filesystem;

void ByteWriter::u32(std::uint32_t v) {
    char b[4];
    for (int i = 0; i < 4; ++i) b[i] = static_cast<char>((v >> (8 * i)) & 0xff);
    m_buf.append(b, 4);
}

void ByteWriter::u64(std::uint64_t v) {
    char b[8];
    for (int i = 0; i < 8; ++i) b[i] = static_cast<char>((v >> (8 * i)) & 0xff);
    m_buf.append(b, 8);
}

void ByteWriter::f64(double v) {
    std::uint64_t bits;
    std::memcpy(&bits, &v, 8);
    u64(bits);
}

void ByteWriter::str(const std::string& s) {
    if (s.size() > 0xffffffffu) throw DatabaseError("value too large to store");
    u32(static_cast<std::uint32_t>(s.size()));
    m_buf.append(s);
}

void ByteWriter::value(const Value& v) {
    u8(static_cast<std::uint8_t>(v.type()));

    switch (v.type()) {
        case ValueType::Null:    break;
        case ValueType::Boolean: u8(v.get_bool() ? 1 : 0); break;
        case ValueType::Integer: i64(v.get_int()); break;
        case ValueType::Real:    f64(v.get_real()); break;
        case ValueType::Text:    str(v.get_text()); break;
        default:                 str(v.get_blob().bytes); break;
    }
}

void ByteWriter::row(const Row& r) {
    u32(static_cast<std::uint32_t>(r.size()));
    for (const auto& v : r) value(v);
}

void ByteWriter::schema(const Schema& s) {
    u32(static_cast<std::uint32_t>(s.size()));

    for (const auto& c : s.columns()) {
        str(c.name);
        u8(static_cast<std::uint8_t>(c.type));
        u8(static_cast<std::uint8_t>((c.nullable ? 1 : 0) | (c.unique ? 2 : 0) | (c.primary_key ? 4 : 0) | (c.auto_increment ? 8 : 0) | (c.default_value ? 16 : 0)));
        if (c.default_value) value(*c.default_value);
    }
}

void ByteWriter::index_def(const IndexDef& d) {
    str(d.name);
    u8(static_cast<std::uint8_t>(d.kind));
    u8(d.unique ? 1 : 0);
    u32(static_cast<std::uint32_t>(d.columns.size()));
    for (const auto& c : d.columns) str(c);
}

void ByteReader::need(std::size_t k) const {
    if (m_n - m_i < k) throw DatabaseError("stored data is truncated or corrupt");
}

std::uint8_t ByteReader::u8() {
    need(1);
    return static_cast<std::uint8_t>(m_p[m_i++]);
}

std::uint32_t ByteReader::u32() {
    need(4);
    std::uint32_t v = 0;
    for (int i = 0; i < 4; ++i) v |= static_cast<std::uint32_t>(static_cast<unsigned char>(m_p[m_i + i])) << (8 * i);
    m_i += 4;
    return v;
}

std::uint64_t ByteReader::u64() {
    need(8);
    std::uint64_t v = 0;
    for (int i = 0; i < 8; ++i) v |= static_cast<std::uint64_t>(static_cast<unsigned char>(m_p[m_i + i])) << (8 * i);
    m_i += 8;
    return v;
}

double ByteReader::f64() {
    const std::uint64_t bits = u64();
    double v;
    std::memcpy(&v, &bits, 8);
    return v;
}

std::string ByteReader::str() {
    const std::uint32_t n = u32();
    need(n);
    std::string s(m_p + m_i, n);
    m_i += n;
    return s;
}

Value ByteReader::value() {
    switch (static_cast<ValueType>(u8())) {
        case ValueType::Null:    return Value();
        case ValueType::Boolean: return Value(u8() != 0);
        case ValueType::Integer: return Value(i64());
        case ValueType::Real:    return Value(f64());
        case ValueType::Text:    return Value(str());
        case ValueType::Blob:    return Value::blob(str());
        default: throw DatabaseError("stored value has an unknown type");
    }
}

Row ByteReader::row() {
    const std::uint32_t n = u32();
    if (n > 4096) throw DatabaseError("stored row is corrupt");
    Row r;
    r.reserve(n);
    for (std::uint32_t i = 0; i < n; ++i) r.push_back(value());
    return r;
}

Schema ByteReader::schema() {
    const std::uint32_t n = u32();
    std::vector<ColumnDef> cols;

    for (std::uint32_t i = 0; i < n; ++i) {
        ColumnDef c;
        c.name = str();
        c.type = static_cast<ColumnType>(u8());
        const std::uint8_t flags = u8();
        c.nullable = flags & 1;
        c.unique = (flags & 2) != 0;
        c.primary_key = (flags & 4) != 0;
        c.auto_increment = (flags & 8) != 0;
        if (flags & 16) c.default_value = value();
        cols.push_back(std::move(c));
    }

    return Schema(std::move(cols));
}

IndexDef ByteReader::index_def() {
    IndexDef d;
    d.name = str();
    d.kind = static_cast<IndexKind>(u8());
    d.unique = u8() != 0;
    const std::uint32_t n = u32();
    for (std::uint32_t i = 0; i < n; ++i) d.columns.push_back(str());
    return d;
}

void sync_file(std::FILE* f) {
    if (!f) return;
    std::fflush(f);
#if defined(_WIN32)
    _commit(_fileno(f));
#else
    fsync(fileno(f));
#endif // defined(_WIN32)
}

bool file_exists(const std::string& path) {
    std::error_code ec;
    return fs::exists(path, ec);
}

bool read_whole_file(const std::string& path, std::string& out) {
    std::FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) return false;
    out.clear();
    char buf[1 << 16];
    std::size_t n;
    while ((n = std::fread(buf, 1, sizeof(buf), f)) > 0) out.append(buf, n);
    const bool ok = !std::ferror(f);
    std::fclose(f);
    return ok;
}

bool atomic_write_file(const std::string& path, const std::string& bytes, bool durable) {
    const std::string tmp = path + ".tmp";
    std::FILE* f = std::fopen(tmp.c_str(), "wb");
    if (!f) return false;
    const bool wrote = std::fwrite(bytes.data(), 1, bytes.size(), f) == bytes.size();
    if (durable) sync_file(f);
    if (std::fclose(f) != 0 || !wrote) { std::remove(tmp.c_str()); return false; }
    std::error_code ec;
    fs::rename(tmp, path, ec);

    if (ec) {
        fs::remove(path, ec);
        ec.clear();
        fs::rename(tmp, path, ec);
        if (ec) return false;
    }

    return true;
}

WriteAheadLog::~WriteAheadLog() {
    close();
}

void WriteAheadLog::open(const std::string& path, SyncMode sync) {
    close();
    m_path = path;
    m_sync = sync;
    m_file = std::fopen(path.c_str(), "ab");
    if (!m_file) throw DatabaseError("cannot open write-ahead log '" + path + "'");
    std::error_code ec;
    const auto sz = fs::file_size(path, ec);
    m_size = ec ? 0 : static_cast<std::uint64_t>(sz);
}

void WriteAheadLog::close() {
    if (m_file) {
        sync_file(m_file);
        std::fclose(m_file);
        m_file = nullptr;
    }
}

void WriteAheadLog::append_batch(const std::vector<std::string>& records) {
    if (!m_file || records.empty()) return;
    std::string out;
    std::size_t total = 0;
    for (const auto& r : records) total += r.size() + 8;
    out.reserve(total);

    for (const auto& r : records) {
        ByteWriter w;
        w.u32(static_cast<std::uint32_t>(r.size()));
        w.u32(crypto::crc32(r.data(), r.size()));
        out += w.data();
        out += r;
    }

    if (std::fwrite(out.data(), 1, out.size(), m_file) != out.size()) throw DatabaseError("writing the write-ahead log failed (disk full?)");
    if (m_sync == SyncMode::Full) sync_file(m_file);
    else if (m_sync == SyncMode::Normal) std::fflush(m_file);
    m_size += out.size();
}

void WriteAheadLog::truncate() {
    if (m_path.empty()) return;
    const SyncMode sync = m_sync;
    close();
    std::FILE* f = std::fopen(m_path.c_str(), "wb");
    if (f) {
        sync_file(f);
        std::fclose(f);
    }
    open(m_path, sync);
}

std::size_t WriteAheadLog::replay(const std::string& path, const Apply& apply, std::uint64_t* good_bytes) {
    std::string data;
    if (good_bytes) *good_bytes = 0;
    if (!read_whole_file(path, data)) return 0;
    std::size_t pos = 0, committed = 0;
    std::vector<std::pair<std::size_t, std::size_t>> pending;
    bool in_txn = false;
    std::uint64_t txn = 0;

    while (data.size() - pos >= 8) {
        ByteReader head(data.data() + pos, 8);
        const std::uint32_t len = head.u32();
        const std::uint32_t crc = head.u32();
        if (data.size() - pos - 8 < len || len == 0) break;
        const char* payload = data.data() + pos + 8;
        if (crypto::crc32(payload, len) != crc) break;
        const WalOp op = static_cast<WalOp>(static_cast<unsigned char>(payload[0]));
        const std::size_t start = pos + 8;
        pos += 8 + len;

        if (op == WalOp::Begin) {
            ByteReader r(payload + 1, len - 1);
            txn = r.u64();
            pending.clear();
            in_txn = true;
            continue;
        }

        if (op == WalOp::Commit) {
            ByteReader r(payload + 1, len - 1);
            if (!in_txn || r.u64() != txn) break;

            for (const auto& rec : pending) {
                ByteReader body(data.data() + rec.first + 1, rec.second - 1);
                apply(static_cast<WalOp>(static_cast<unsigned char>(data[rec.first])), body);
            }

            committed += pending.size();
            pending.clear();
            in_txn = false;
            if (good_bytes) *good_bytes = pos;
            continue;
        }

        if (!in_txn) break;
        pending.emplace_back(start, len);
    }

    return committed;
}

} // namespace database
} // namespace fizmo
