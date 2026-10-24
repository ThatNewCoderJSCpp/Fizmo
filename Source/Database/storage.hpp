#ifndef FIZMO_DATABASE_STORAGE_HPP
#define FIZMO_DATABASE_STORAGE_HPP

#include "table.hpp"

#include <cstdio>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace fizmo {
namespace database {

enum class SyncMode : std::uint8_t {
    Off,
    Normal,
    Full
};

class ByteWriter {
public:
    void u8(std::uint8_t v) { m_buf.push_back(static_cast<char>(v)); }
    void u32(std::uint32_t v);
    void u64(std::uint64_t v);
    void i64(std::int64_t v) { u64(static_cast<std::uint64_t>(v)); }
    void f64(double v);
    void str(const std::string& s);
    void value(const Value& v);
    void row(const Row& r);
    void schema(const Schema& s);
    void index_def(const IndexDef& d);

    const std::string& data() const noexcept { return m_buf; }
    std::string& data() noexcept { return m_buf; }
    void clear() noexcept { m_buf.clear(); }

private:
    std::string m_buf;
};

class ByteReader {
public:
    ByteReader(const char* data, std::size_t size) : m_p(data), m_n(size) {}

    std::uint8_t u8();
    std::uint32_t u32();
    std::uint64_t u64();
    std::int64_t i64() { return static_cast<std::int64_t>(u64()); }
    double f64();
    std::string str();
    Value value();
    Row row();
    Schema schema();
    IndexDef index_def();

    bool done() const noexcept { return m_i >= m_n; }
    std::size_t remaining() const noexcept { return m_n - m_i; }

private:
    const char* m_p;
    std::size_t m_n;
    std::size_t m_i = 0;

    void need(std::size_t k) const;
};

enum class WalOp : std::uint8_t {
    Begin       = 1,
    Commit      = 2,
    CreateTable = 3,
    DropTable   = 4,
    CreateIndex = 5,
    DropIndex   = 6,
    Insert      = 7,
    Delete      = 8,
    Update      = 9,
    Clear       = 10,
    CreatePartitioned   = 11,
    AddPartition        = 12,
    RemovePartition     = 13,
    SetPartitionOptions = 14
};

class WriteAheadLog {
public:
    WriteAheadLog() = default;
    ~WriteAheadLog();

    WriteAheadLog(const WriteAheadLog&) = delete;
    WriteAheadLog& operator=(const WriteAheadLog&) = delete;

    void open(const std::string& path, SyncMode sync);
    void close();
    bool is_open() const noexcept { return m_file != nullptr; }
    void append_batch(const std::vector<std::string>& records);
    void truncate();
    std::uint64_t size() const noexcept { return m_size; }
    const std::string& path() const noexcept { return m_path; }

    using Apply = std::function<void(WalOp op, ByteReader& payload)>;
    static std::size_t replay(const std::string& path, const Apply& apply, std::uint64_t* good_bytes = nullptr);

private:
    std::FILE*    m_file = nullptr;
    std::string   m_path;
    SyncMode      m_sync = SyncMode::Normal;
    std::uint64_t m_size = 0;
};

void sync_file(std::FILE* f);
bool file_exists(const std::string& path);
bool atomic_write_file(const std::string& path, const std::string& bytes, bool durable);
bool read_whole_file(const std::string& path, std::string& out);

} // namespace database
} // namespace fizmo

#endif // FIZMO_DATABASE_STORAGE_HPP
