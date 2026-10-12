#ifndef FIZMO_PACKET_SEQUENCE_HPP
#define FIZMO_PACKET_SEQUENCE_HPP

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace fizmo {
namespace networking {
namespace packet {

inline bool sequence_greater_than(std::uint16_t a, std::uint16_t b) noexcept {
    return ((a > b) && (a - b <= 32768)) || ((a < b) && (b - a > 32768));
}

inline bool sequence_less_than(std::uint16_t a, std::uint16_t b) noexcept {
    return sequence_greater_than(b, a);
}

inline int sequence_difference(std::uint16_t a, std::uint16_t b) noexcept {
    int d = static_cast<int>(a) - static_cast<int>(b);
    if (d > 32768) d -= 65536;
    else if (d < -32768) d += 65536;
    return d;
}

template <typename T>
class SequenceBuffer {
public:
    explicit SequenceBuffer(std::size_t size = 256) : m_entries(size), m_sequences(size, kEmpty) {}

    void reset() {
        m_sequence = 0;
        std::fill(m_sequences.begin(), m_sequences.end(), kEmpty);
        for (auto& e : m_entries) e = T();
    }

    T* insert(std::uint16_t sequence) {
        if (sequence_less_than(sequence, static_cast<std::uint16_t>(m_sequence - capacity()))) return nullptr;

        if (sequence_greater_than(static_cast<std::uint16_t>(sequence + 1), m_sequence)) {
            remove_range(m_sequence, sequence);
            m_sequence = static_cast<std::uint16_t>(sequence + 1);
        }

        const std::size_t i = index(sequence);
        m_sequences[i] = sequence;
        m_entries[i] = T();
        return &m_entries[i];
    }

    void remove(std::uint16_t sequence) {
        const std::size_t i = index(sequence);
        if (m_sequences[i] == sequence) m_sequences[i] = kEmpty;
    }

    bool exists(std::uint16_t sequence) const noexcept {
        return m_sequences[index(sequence)] == static_cast<std::uint32_t>(sequence);
    }

    bool available(std::uint16_t sequence) const noexcept {
        return m_sequences[index(sequence)] == kEmpty;
    }

    T* find(std::uint16_t sequence) noexcept {
        const std::size_t i = index(sequence);
        return m_sequences[i] == static_cast<std::uint32_t>(sequence) ? &m_entries[i] : nullptr;
    }

    const T* find(std::uint16_t sequence) const noexcept {
        const std::size_t i = index(sequence);
        return m_sequences[i] == static_cast<std::uint32_t>(sequence) ? &m_entries[i] : nullptr;
    }

    std::uint16_t sequence() const noexcept { return m_sequence; }
    std::size_t capacity() const noexcept { return m_entries.size(); }

    void generate_ack_bits(std::uint16_t& ack, std::uint32_t& ack_bits) const noexcept {
        ack = static_cast<std::uint16_t>(m_sequence - 1);
        ack_bits = 0;

        for (std::uint32_t i = 0; i < 32; ++i) {
            const std::uint16_t s = static_cast<std::uint16_t>(ack - i);
            if (exists(s)) ack_bits |= 1u << i;
        }
    }

private:
    static constexpr std::uint32_t kEmpty = 0xFFFFFFFFu;

    std::vector<T>             m_entries;
    std::vector<std::uint32_t> m_sequences;
    std::uint16_t              m_sequence = 0;

    std::size_t index(std::uint16_t s) const noexcept { return s % m_entries.size(); }

    void remove_range(std::uint16_t start, std::uint16_t finish) {
        const int span = sequence_difference(finish, start);
        if (span < 0) return;

        if (static_cast<std::size_t>(span) >= capacity()) {
            std::fill(m_sequences.begin(), m_sequences.end(), kEmpty);
            return;
        }

        for (int i = 0; i <= span; ++i) m_sequences[index(static_cast<std::uint16_t>(start + i))] = kEmpty;
    }
};

} // namespace packet
} // namespace networking
} // namespace fizmo

#endif // FIZMO_PACKET_SEQUENCE_HPP
