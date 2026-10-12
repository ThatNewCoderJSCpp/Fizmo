#ifndef FIZMO_RELIABLE_ENDPOINT_HPP
#define FIZMO_RELIABLE_ENDPOINT_HPP

#include "packet.hpp"
#include "packet_tracker.hpp"
#include "sequence.hpp"

#include <chrono>
#include <cstdint>
#include <deque>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace fizmo {
namespace networking {
namespace packet {

enum class Delivery : std::uint8_t {
    Unreliable,
    UnreliableSequenced,
    ReliableUnordered,
    ReliableOrdered
};

const char* delivery_to_string(Delivery d) noexcept;

struct EndpointConfig {
    std::size_t               max_packet_size     = 1200;
    std::size_t               fragment_size       = 1024;
    std::size_t               max_fragments       = 4096;
    std::size_t               reliable_window     = 1024;
    std::size_t               max_packets_per_update = 256;
    std::chrono::milliseconds keepalive_interval{100};
    std::chrono::milliseconds min_resend_delay{30};
    std::chrono::milliseconds max_resend_delay{1000};
    double                    resend_rtt_factor   = 1.25;
    std::chrono::milliseconds fragment_timeout{5000};
    PacketTrackerConfig       tracker;
};

struct EndpointStats {
    std::uint64_t messages_sent       = 0;
    std::uint64_t messages_received   = 0;
    std::uint64_t messages_dropped    = 0;
    std::uint64_t fragments_sent      = 0;
    std::uint64_t fragments_resent    = 0;
    std::uint64_t packets_sent        = 0;
    std::uint64_t packets_received    = 0;
    std::uint64_t packets_invalid     = 0;
    std::uint64_t bytes_sent          = 0;
    std::uint64_t bytes_received      = 0;

    std::string to_string() const;
};

class ReliableEndpoint {
public:
    using clock      = std::chrono::steady_clock;
    using time_point = clock::time_point;

    static constexpr std::uint8_t kReliableOrdered     = 0;
    static constexpr std::uint8_t kUnreliable          = 1;
    static constexpr std::uint8_t kUnreliableSequenced = 2;
    static constexpr std::uint8_t kReliableUnordered   = 3;
    static constexpr std::size_t  kMaxChannels         = 64;

    explicit ReliableEndpoint(EndpointConfig config = EndpointConfig(), bool default_channels = true);
    ~ReliableEndpoint();

    ReliableEndpoint(const ReliableEndpoint&) = delete;
    ReliableEndpoint& operator=(const ReliableEndpoint&) = delete;

    int add_channel(Delivery delivery);
    std::size_t channel_count() const noexcept;
    Delivery channel_delivery(std::uint8_t channel) const;

    bool send(std::uint8_t channel, const void* data, std::size_t len);
    bool send(std::uint8_t channel, std::vector<std::uint8_t> data);
    bool send(std::uint8_t channel, const std::string& data) { return send(channel, data.data(), data.size()); }
    bool can_send(std::uint8_t channel) const;

    void receive_packet(const std::uint8_t* data, std::size_t len, time_point now = clock::now());
    void update(time_point now = clock::now());
    void reset();

    std::size_t pending_reliable(std::uint8_t channel) const;
    std::size_t pending_reliable() const;
    std::size_t queued_unreliable() const;
    bool has_pending_data() const;

    PacketTracker& tracker() noexcept { return m_tracker; }
    const PacketTracker& tracker() const noexcept { return m_tracker; }
    const EndpointStats& stats() const noexcept { return m_stats; }
    const EndpointConfig& config() const noexcept { return m_config; }

    std::function<void(const std::uint8_t* data, std::size_t len)> on_send_packet;
    std::function<void(std::uint8_t channel, std::vector<std::uint8_t> message)> on_message;

    struct Channel;

private:
    struct FragRef {
        std::uint8_t  channel;
        std::uint16_t id;
        std::uint16_t fragment;
    };

    struct PacketRecord {
        std::vector<FragRef> refs;
    };

    EndpointConfig                        m_config;
    PacketTracker                         m_tracker;
    std::vector<std::unique_ptr<Channel>> m_channels;
    SequenceBuffer<PacketRecord>          m_records;
    EndpointStats                         m_stats;
    time_point                            m_last_send;
    bool                                  m_ack_owed = false;
    std::vector<std::uint16_t>            m_ack_queue;

    void cover_acks(std::uint16_t ack);

    void handle_ack(std::uint16_t packet_sequence);
    void deliver_fragment(Channel& ch, std::uint8_t channel, std::uint16_t id, std::uint16_t fragment, std::uint16_t count, const std::uint8_t* data, std::size_t len, time_point now);
    void deliver_ready(Channel& ch, std::uint8_t channel);
    std::chrono::microseconds resend_delay() const;
};

} // namespace packet
} // namespace networking
} // namespace fizmo

#endif // FIZMO_RELIABLE_ENDPOINT_HPP
