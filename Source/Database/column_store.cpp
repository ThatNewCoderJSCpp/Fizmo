#include "fizmo_library.hpp"
#include "column_store.hpp"

#include <algorithm>
#include <cmath>

namespace fizmo {
namespace database {

ColumnData::ColumnData(ColumnType type) : m_type(type) {}

void ColumnData::resize(std::size_t slots) {
    if (slots <= m_size) return;

    switch (m_type) {
        case ColumnType::Integer:
        case ColumnType::Boolean: m_ints.resize(slots, 0); break;
        case ColumnType::Real:    m_reals.resize(slots, 0.0); break;
        default:                  m_texts.resize(slots); break;
    }

    m_nulls.resize(slots, true);
    if (has_zones()) m_zones.resize((slots + kZoneRows - 1) / kZoneRows);
    m_size = slots;
}

void ColumnData::widen(RowId slot) {
    if (!has_zones()) return;
    Zone& z = m_zones[slot / kZoneRows];

    if (m_nulls.test(slot)) {
        ++z.nulls;
        return;
    }

    ++z.values;

    if (m_type == ColumnType::Real) {
        const double d = m_reals[slot];
        if (std::isnan(d)) { z.has_nan = true; return; }
        z.real_min = std::min(z.real_min, d);
        z.real_max = std::max(z.real_max, d);
    } else {
        const std::int64_t i = m_ints[slot];
        z.int_min = std::min(z.int_min, i);
        z.int_max = std::max(z.int_max, i);
    }
}

void ColumnData::set(RowId slot, const Value& v) {
    if (slot >= m_size) resize(slot + 1);
    if (!m_nulls.test(slot)) ++m_stale;

    if (v.is_null()) {
        m_nulls.set(slot);

        switch (m_type) {
            case ColumnType::Integer:
            case ColumnType::Boolean: m_ints[slot] = 0; break;
            case ColumnType::Real:    m_reals[slot] = 0.0; break;
            default:                  std::string().swap(m_texts[slot]); break;
        }

        widen(slot);
        return;
    }

    m_nulls.reset(slot);

    switch (m_type) {
        case ColumnType::Integer: m_ints[slot] = v.get_int(); break;
        case ColumnType::Boolean: m_ints[slot] = v.get_bool() ? 1 : 0; break;
        case ColumnType::Real:    m_reals[slot] = v.get_real(); break;
        case ColumnType::Text:    m_texts[slot] = v.get_text(); break;
        default:                  m_texts[slot] = v.get_blob().bytes; break;
    }

    widen(slot);
}

void ColumnData::clear_slot(RowId slot) {
    if (slot >= m_size) return;
    if (!m_nulls.test(slot)) ++m_stale;
    m_nulls.set(slot);
    if (m_type == ColumnType::Text || m_type == ColumnType::Blob) std::string().swap(m_texts[slot]);
}

Value ColumnData::get(RowId slot) const {
    if (slot >= m_size || m_nulls.test(slot)) return Value();

    switch (m_type) {
        case ColumnType::Integer: return Value(m_ints[slot]);
        case ColumnType::Boolean: return Value(m_ints[slot] != 0);
        case ColumnType::Real:    return Value(m_reals[slot]);
        case ColumnType::Text:    return Value(m_texts[slot]);
        default:                  return Value::blob(m_texts[slot]);
    }
}

void ColumnData::rebuild_zones(const Bitmap& live) {
    if (!has_zones()) return;
    std::fill(m_zones.begin(), m_zones.end(), Zone());

    for (std::size_t s = 0; s < m_size; ++s) {
        if (s < live.size() && live.test(s)) widen(static_cast<RowId>(s));
    }

    m_stale = 0;
}

std::size_t ColumnData::memory_bytes() const noexcept {
    std::size_t n = m_ints.capacity() * sizeof(std::int64_t) + m_reals.capacity() * sizeof(double) + m_texts.capacity() * sizeof(std::string);
    for (const auto& t : m_texts) if (t.capacity() > 15) n += t.capacity();
    return n + m_nulls.words().capacity() * 8 + m_zones.capacity() * sizeof(Zone);
}

} // namespace database
} // namespace fizmo
