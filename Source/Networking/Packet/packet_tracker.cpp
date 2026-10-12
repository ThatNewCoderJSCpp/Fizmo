#include "fizmo_library.hpp"
#include "packet_tracker.hpp"

#include <algorithm>
#include <cmath>
#include <sstream>

namespace fizmo {
namespace networking {
namespace packet {

std::string PacketTrackerStats::to_string() const {
    std::ostringstream os;
    os.precision(3);
    os << std::fixed << "PacketTracker[rtt=" << rtt_ms << "ms var=" << rtt_variance_ms << "ms min=" << min_rtt_ms
       << "ms jitter=" << jitter_ms << "ms loss=" << (packet_loss * 100.0) << "% tx=" << sent_kbps << "kbps rx=" << received_kbps
       << "kbps acked=" << acked_kbps << "kbps sent=" << packets_sent << " recv=" << packets_received << " acked=" << packets_acked
       << " lost=" << packets_lost << " dup=" << packets_duplicate << " stale=" << packets_stale << "]";
    return os.str();
}

PacketTracker::PacketTracker(PacketTrackerConfig config)
    : m_config(config), m_sent(std::max<std::size_t>(64, config.sent_window)), m_received(std::max<std::size_t>(64, config.received_window)) {
    reset();
}

void PacketTracker::reset() {
    m_sent.reset();
    m_received.reset();
    m_next_sequence = 0;
    m_oldest_unchecked = 0;
    m_has_rtt = false;
    m_srtt_ms = static_cast<double>(m_config.initial_rtt.count());
    m_rttvar_ms = m_srtt_ms / 2.0;
    m_last_rtt_ms = 0.0;
    m_stats = PacketTrackerStats();
    m_bw_window_start = time_point();
    m_bw_sent = m_bw_received = m_bw_acked = 0;
    m_window_sent = m_window_lost = 0;
}

std::uint16_t PacketTracker::on_packet_sent(std::size_t bytes, time_point now) {
    const std::uint16_t seq = m_next_sequence++;
    SentPacket* p = m_sent.insert(seq);

    if (p) {
        p->time = now;
        p->bytes = bytes;
    }

    ++m_stats.packets_sent;
    m_bw_sent += bytes;
    return seq;
}

bool PacketTracker::on_packet_received(std::uint16_t sequence, std::size_t bytes, time_point now) {
    if (m_received.exists(sequence)) {
        ++m_stats.packets_duplicate;
        return false;
    }

    ReceivedPacket* p = m_received.insert(sequence);

    if (!p) {
        ++m_stats.packets_stale;
        return false;
    }

    p->time = now;
    p->bytes = bytes;
    ++m_stats.packets_received;
    m_bw_received += bytes;
    return true;
}

void PacketTracker::generate_ack(std::uint16_t& ack, std::uint32_t& ack_bits) const noexcept {
    m_received.generate_ack_bits(ack, ack_bits);
}

std::uint32_t PacketTracker::ack_bits_for(std::uint16_t ack) const noexcept {
    std::uint32_t bits = 0;

    for (std::uint32_t i = 0; i < 32; ++i) {
        if (m_received.exists(static_cast<std::uint16_t>(ack - i))) bits |= 1u << i;
    }

    return bits;
}

void PacketTracker::add_rtt_sample(double ms) {
    if (!m_has_rtt) {
        m_srtt_ms = ms;
        m_rttvar_ms = ms / 2.0;
        m_stats.min_rtt_ms = ms;
        m_has_rtt = true;
    } else {
        m_rttvar_ms = 0.75 * m_rttvar_ms + 0.25 * std::fabs(m_srtt_ms - ms);
        m_srtt_ms = 0.875 * m_srtt_ms + 0.125 * ms;
        m_stats.min_rtt_ms = std::min(m_stats.min_rtt_ms, ms);
        m_stats.jitter_ms += (std::fabs(ms - m_last_rtt_ms) - m_stats.jitter_ms) / 16.0;
    }

    m_last_rtt_ms = ms;
    m_stats.rtt_ms = m_srtt_ms;
    m_stats.rtt_variance_ms = m_rttvar_ms;
}

void PacketTracker::on_ack_received(std::uint16_t ack, std::uint32_t ack_bits, time_point now) {
    for (std::uint32_t i = 0; i < 32; ++i) {
        if ((ack_bits & (1u << i)) == 0) continue;
        const std::uint16_t seq = static_cast<std::uint16_t>(ack - i);
        SentPacket* p = m_sent.find(seq);
        if (!p || p->acked) continue;
        const bool was_lost = p->lost;
        p->acked = true;
        ++m_stats.packets_acked;
        m_bw_acked += p->bytes;

        if (!was_lost) {
            const double ms = std::chrono::duration<double, std::milli>(now - p->time).count();
            add_rtt_sample(ms);
        }

        if (on_acked) on_acked(seq);
    }
}

std::chrono::microseconds PacketTracker::retransmit_timeout() const noexcept {
    const double rto = m_srtt_ms + std::max(4.0 * m_rttvar_ms, 10.0);
    const double lo = static_cast<double>(m_config.min_loss_timeout.count());
    const double hi = static_cast<double>(m_config.max_loss_timeout.count());
    return std::chrono::microseconds(static_cast<long long>(std::min(hi, std::max(lo, rto)) * 1000.0));
}

void PacketTracker::mark_lost(std::uint16_t sequence, SentPacket& p) {
    p.lost = true;
    ++m_stats.packets_lost;
    ++m_window_lost;
    if (on_lost) on_lost(sequence);
}

void PacketTracker::update(time_point now) {
    const auto timeout = retransmit_timeout();

    while (m_oldest_unchecked != m_next_sequence) {
        SentPacket* p = m_sent.find(m_oldest_unchecked);

        if (p && !p->acked && !p->lost) {
            if (now - p->time < timeout) break;
            mark_lost(m_oldest_unchecked, *p);
        }

        if (p) ++m_window_sent;
        ++m_oldest_unchecked;
    }

    if (m_window_sent >= 16) {
        const double sample = static_cast<double>(m_window_lost) / static_cast<double>(m_window_sent);
        m_stats.packet_loss += m_config.loss_smoothing * (sample - m_stats.packet_loss);
        m_window_sent = 0;
        m_window_lost = 0;
    }

    if (m_bw_window_start == time_point()) m_bw_window_start = now;
    const double elapsed = std::chrono::duration<double>(now - m_bw_window_start).count();

    if (elapsed >= 0.25) {
        const double a = m_config.bandwidth_smoothing;
        m_stats.sent_kbps += a * ((static_cast<double>(m_bw_sent) * 8.0 / 1000.0 / elapsed) - m_stats.sent_kbps);
        m_stats.received_kbps += a * ((static_cast<double>(m_bw_received) * 8.0 / 1000.0 / elapsed) - m_stats.received_kbps);
        m_stats.acked_kbps += a * ((static_cast<double>(m_bw_acked) * 8.0 / 1000.0 / elapsed) - m_stats.acked_kbps);
        m_bw_sent = m_bw_received = m_bw_acked = 0;
        m_bw_window_start = now;
    }
}

bool PacketTracker::is_acked(std::uint16_t sequence) const noexcept {
    const SentPacket* p = m_sent.find(sequence);
    return p && p->acked;
}

bool PacketTracker::is_pending(std::uint16_t sequence) const noexcept {
    const SentPacket* p = m_sent.find(sequence);
    return p && !p->acked && !p->lost;
}

} // namespace packet
} // namespace networking
} // namespace fizmo
