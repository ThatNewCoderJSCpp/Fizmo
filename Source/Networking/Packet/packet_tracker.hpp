#ifndef FIZMO_PACKET_TRACKER_HPP
#define FIZMO_PACKET_TRACKER_HPP

#include "sequence.hpp"

#include <chrono>
#include <cstdint>
#include <functional>
#include <string>

namespace fizmo {
namespace networking {
namespace packet {

struct PacketTrackerConfig {
    std::size_t               sent_window       = 1024;
    std::size_t               received_window   = 1024;
    double                    loss_smoothing    = 0.1;
    double                    bandwidth_smoothing = 0.1;
    std::chrono::milliseconds min_loss_timeout{150};
    std::chrono::milliseconds max_loss_timeout{2000};
    std::chrono::milliseconds initial_rtt{100};
};

struct PacketTrackerStats {
    double        rtt_ms            = 0.0;
    double        rtt_variance_ms   = 0.0;
    double        min_rtt_ms        = 0.0;
    double        jitter_ms         = 0.0;
    double        packet_loss       = 0.0;
    double        sent_kbps         = 0.0;
    double        received_kbps     = 0.0;
    double        acked_kbps        = 0.0;
    std::uint64_t packets_sent      = 0;
    std::uint64_t packets_received  = 0;
    std::uint64_t packets_acked     = 0;
    std::uint64_t packets_lost      = 0;
    std::uint64_t packets_duplicate = 0;
    std::uint64_t packets_stale     = 0;

    std::string to_string() const;
};

class PacketTracker {
public:
    using clock      = std::chrono::steady_clock;
    using time_point = clock::time_point;

    explicit PacketTracker(PacketTrackerConfig config = PacketTrackerConfig());

    void reset();

    std::uint16_t next_sequence() const noexcept { return m_next_sequence; }
    std::uint16_t on_packet_sent(std::size_t bytes, time_point now = clock::now());
    bool on_packet_received(std::uint16_t sequence, std::size_t bytes, time_point now = clock::now());
    void on_ack_received(std::uint16_t ack, std::uint32_t ack_bits, time_point now = clock::now());
    void generate_ack(std::uint16_t& ack, std::uint32_t& ack_bits) const noexcept;
    std::uint32_t ack_bits_for(std::uint16_t ack) const noexcept;
    void update(time_point now = clock::now());

    bool is_acked(std::uint16_t sequence) const noexcept;
    bool is_pending(std::uint16_t sequence) const noexcept;

    std::chrono::microseconds rtt() const noexcept { return std::chrono::microseconds(static_cast<long long>(m_srtt_ms * 1000.0)); }
    std::chrono::microseconds retransmit_timeout() const noexcept;
    double packet_loss() const noexcept { return m_stats.packet_loss; }
    const PacketTrackerStats& stats() const noexcept { return m_stats; }

    std::function<void(std::uint16_t sequence)> on_acked;
    std::function<void(std::uint16_t sequence)> on_lost;

private:
    struct SentPacket {
        time_point  time;
        std::size_t bytes = 0;
        bool        acked = false;
        bool        lost  = false;
    };

    struct ReceivedPacket {
        time_point  time;
        std::size_t bytes = 0;
    };

    PacketTrackerConfig            m_config;
    SequenceBuffer<SentPacket>     m_sent;
    SequenceBuffer<ReceivedPacket> m_received;
    std::uint16_t                  m_next_sequence = 0;
    std::uint16_t                  m_oldest_unchecked = 0;
    bool                           m_has_rtt = false;
    double                         m_srtt_ms = 0.0;
    double                         m_rttvar_ms = 0.0;
    double                         m_last_rtt_ms = 0.0;
    PacketTrackerStats             m_stats;
    time_point                     m_bw_window_start;
    std::uint64_t                  m_bw_sent = 0;
    std::uint64_t                  m_bw_received = 0;
    std::uint64_t                  m_bw_acked = 0;
    std::uint64_t                  m_window_sent = 0;
    std::uint64_t                  m_window_lost = 0;

    void add_rtt_sample(double ms);
    void mark_lost(std::uint16_t sequence, SentPacket& p);
};

} // namespace packet
} // namespace networking
} // namespace fizmo

#endif // FIZMO_PACKET_TRACKER_HPP
