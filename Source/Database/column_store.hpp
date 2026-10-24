#ifndef FIZMO_DATABASE_COLUMN_STORE_HPP
#define FIZMO_DATABASE_COLUMN_STORE_HPP

#include "bitmap.hpp"
#include "schema.hpp"

#include <limits>
#include <string>
#include <vector>

namespace fizmo {
namespace database {

using RowId = std::uint32_t;
constexpr RowId kNoRow = std::numeric_limits<RowId>::max();
constexpr std::size_t kZoneRows = 1024;

struct Zone {
    std::int64_t  int_min = std::numeric_limits<std::int64_t>::max();
    std::int64_t  int_max = std::numeric_limits<std::int64_t>::min();
    double        real_min = std::numeric_limits<double>::infinity();
    double        real_max = -std::numeric_limits<double>::infinity();
    std::uint32_t values = 0;
    std::uint32_t nulls = 0;
    bool          has_nan = false;

    bool empty() const noexcept { return values == 0; }
};

class ColumnData {
public:
    explicit ColumnData(ColumnType type = ColumnType::Text);

    ColumnType type() const noexcept { return m_type; }
    std::size_t size() const noexcept { return m_size; }
    void resize(std::size_t slots);

    void set(RowId slot, const Value& v);
    void clear_slot(RowId slot);
    Value get(RowId slot) const;
    bool is_null(RowId slot) const noexcept { return m_nulls.test(slot); }

    std::int64_t int_at(RowId slot) const noexcept { return m_ints[slot]; }
    double real_at(RowId slot) const noexcept { return m_reals[slot]; }
    const std::string& bytes_at(RowId slot) const noexcept { return m_texts[slot]; }

    const std::int64_t* ints() const noexcept { return m_ints.data(); }
    const double* reals() const noexcept { return m_reals.data(); }
    const std::string* texts() const noexcept { return m_texts.data(); }
    const Bitmap& nulls() const noexcept { return m_nulls; }

    bool has_zones() const noexcept { return m_type == ColumnType::Integer || m_type == ColumnType::Real || m_type == ColumnType::Boolean; }
    std::size_t zone_count() const noexcept { return m_zones.size(); }
    const Zone& zone(std::size_t z) const noexcept { return m_zones[z]; }
    void rebuild_zones(const Bitmap& live);
    std::size_t stale_writes() const noexcept { return m_stale; }

    std::size_t memory_bytes() const noexcept;

private:
    ColumnType                m_type;
    std::size_t               m_size = 0;
    std::vector<std::int64_t> m_ints;
    std::vector<double>       m_reals;
    std::vector<std::string>  m_texts;
    Bitmap                    m_nulls;
    std::vector<Zone>         m_zones;
    std::size_t               m_stale = 0;

    void widen(RowId slot);
};

} // namespace database
} // namespace fizmo

#endif // FIZMO_DATABASE_COLUMN_STORE_HPP
