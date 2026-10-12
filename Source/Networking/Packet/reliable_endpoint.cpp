#include "fizmo_library.hpp"
#include "reliable_endpoint.hpp"

#include <algorithm>
#include <sstream>

namespace fizmo {
namespace networking {
namespace packet {

const char* delivery_to_string(Delivery d) noexcept {
    switch (d) {
        case Delivery::Unreliable:          return "Unreliable";
        case Delivery::UnreliableSequenced: return "UnreliableSequenced";
        case Delivery::ReliableUnordered:   return "ReliableUnordered";
        case Delivery::ReliableOrdered:     return "ReliableOrdered";
    }

    return "Unknown";
}

std::string EndpointStats::to_string() const {
    std::ostringstream os;
    os << "EndpointStats[msgs tx=" << messages_sent << " rx=" << messages_received << " dropped=" << messages_dropped
       << " frags=" << fragments_sent << " resent=" << fragments_resent << " packets tx=" << packets_sent << " rx=" << packets_received
       << " invalid=" << packets_invalid << " bytes tx=" << bytes_sent << " rx=" << bytes_received << "]";
    return os.str();
}

namespace {

bool is_reliable(Delivery d) noexcept {
    return d == Delivery::ReliableOrdered || d == Delivery::ReliableUnordered;
}

struct OutMessage {
    std::vector<std::uint8_t>                          data;
    std::uint16_t                                      frag_count = 1;
    std::uint16_t                                      remaining  = 0;
    std::vector<std::uint8_t>                          acked;
    std::vector<std::chrono::steady_clock::time_point> last_sent;
};

struct InMessage {
    std::vector<std::uint8_t> data;
    std::vector<std::uint8_t> have;
    std::uint16_t             frag_count = 0;
    std::uint16_t             received   = 0;
    std::size_t               last_len   = 0;
    bool                      complete   = false;
};

struct PendingUnreliable {
    std::uint16_t             id = 0;
    std::vector<std::uint8_t> data;
    std::uint16_t             frag_count = 1;
    std::uint16_t             next = 0;
};

std::size_t varint_size(std::size_t v) noexcept {
    std::size_t n = 1;
    while (v >= 0x80) { v >>= 7; ++n; }
    return n;
}

} // namespace

struct ReliableEndpoint::Channel {
    Delivery                       delivery;
    std::uint16_t                  next_id = 0;
    SequenceBuffer<OutMessage>     out;
    std::deque<std::uint16_t>      inflight;
    std::deque<PendingUnreliable>  unreliable;
    SequenceBuffer<InMessage>      in;
    SequenceBuffer<std::uint8_t>   seen;
    std::uint16_t                  recv_next = 0;
    bool                           has_last  = false;
    std::uint16_t                  last_seq  = 0;

    Channel(Delivery d, std::size_t window) : delivery(d), out(window), in(window), seen(window) {}
};

ReliableEndpoint::ReliableEndpoint(EndpointConfig config, bool default_channels)
    : m_config(config), m_tracker(config.tracker), m_records(std::max<std::size_t>(64, config.tracker.sent_window)) {
    if (m_config.max_packet_size < 64) m_config.max_packet_size = 64;
    if (m_config.fragment_size + 32 > m_config.max_packet_size) m_config.fragment_size = m_config.max_packet_size - 32;
    if (m_config.fragment_size == 0) m_config.fragment_size = 1;
    if (m_config.reliable_window < 16) m_config.reliable_window = 16;
    if (m_config.reliable_window > 16384) m_config.reliable_window = 16384;
    if (m_config.max_fragments > 65535) m_config.max_fragments = 65535;
    m_tracker.on_acked = [this](std::uint16_t seq) { handle_ack(seq); };
    m_last_send = clock::now();

    if (default_channels) {
        add_channel(Delivery::ReliableOrdered);
        add_channel(Delivery::Unreliable);
        add_channel(Delivery::UnreliableSequenced);
        add_channel(Delivery::ReliableUnordered);
    }
}

ReliableEndpoint::~ReliableEndpoint() = default;

int ReliableEndpoint::add_channel(Delivery delivery) {
    if (m_channels.size() >= kMaxChannels) return -1;
    m_channels.emplace_back(new Channel(delivery, m_config.reliable_window));
    return static_cast<int>(m_channels.size() - 1);
}

std::size_t ReliableEndpoint::channel_count() const noexcept { return m_channels.size(); }

Delivery ReliableEndpoint::channel_delivery(std::uint8_t channel) const {
    if (channel >= m_channels.size()) throw std::out_of_range("ReliableEndpoint: unknown channel");
    return m_channels[channel]->delivery;
}

void ReliableEndpoint::reset() {
    const std::size_t n = m_channels.size();
    std::vector<Delivery> kinds;
    for (auto& c : m_channels) kinds.push_back(c->delivery);
    m_channels.clear();
    for (std::size_t i = 0; i < n; ++i) add_channel(kinds[i]);
    m_tracker.reset();
    m_records.reset();
    m_stats = EndpointStats();
    m_ack_owed = false;
    m_ack_queue.clear();
    m_last_send = clock::now();
}

bool ReliableEndpoint::can_send(std::uint8_t channel) const {
    if (channel >= m_channels.size()) return false;
    const Channel& ch = *m_channels[channel];
    if (!is_reliable(ch.delivery)) return true;
    return ch.inflight.size() < m_config.reliable_window;
}

bool ReliableEndpoint::send(std::uint8_t channel, const void* data, std::size_t len) {
    const auto* p = static_cast<const std::uint8_t*>(data);
    return send(channel, std::vector<std::uint8_t>(p, p + len));
}

bool ReliableEndpoint::send(std::uint8_t channel, std::vector<std::uint8_t> data) {
    if (channel >= m_channels.size()) return false;
    const std::size_t fs = m_config.fragment_size;
    if (data.size() > fs * m_config.max_fragments) return false;
    const std::size_t frags = data.size() <= fs ? 1 : (data.size() + fs - 1) / fs;
    Channel& ch = *m_channels[channel];

    if (is_reliable(ch.delivery)) {
        if (ch.inflight.size() >= m_config.reliable_window) return false;
        const std::uint16_t id = ch.next_id++;
        OutMessage* m = ch.out.insert(id);
        if (!m) return false;
        m->data = std::move(data);
        m->frag_count = static_cast<std::uint16_t>(frags);
        m->remaining = static_cast<std::uint16_t>(frags);
        m->acked.assign(frags, 0);
        m->last_sent.assign(frags, time_point());
        ch.inflight.push_back(id);
    } else {
        PendingUnreliable p;
        p.id = ch.next_id++;
        p.data = std::move(data);
        p.frag_count = static_cast<std::uint16_t>(frags);
        ch.unreliable.push_back(std::move(p));
    }

    ++m_stats.messages_sent;
    return true;
}

std::size_t ReliableEndpoint::pending_reliable(std::uint8_t channel) const {
    return channel < m_channels.size() ? m_channels[channel]->inflight.size() : 0;
}

std::size_t ReliableEndpoint::pending_reliable() const {
    std::size_t n = 0;
    for (auto& c : m_channels) n += c->inflight.size();
    return n;
}

std::size_t ReliableEndpoint::queued_unreliable() const {
    std::size_t n = 0;
    for (auto& c : m_channels) n += c->unreliable.size();
    return n;
}

bool ReliableEndpoint::has_pending_data() const {
    return pending_reliable() > 0 || queued_unreliable() > 0;
}

std::chrono::microseconds ReliableEndpoint::resend_delay() const {
    const auto& st = m_tracker.stats();
    const double rtt = st.packets_acked > 0 ? st.rtt_ms + 2.0 * st.rtt_variance_ms : static_cast<double>(m_config.tracker.initial_rtt.count());
    double ms = rtt * m_config.resend_rtt_factor;
    ms = std::max(ms, static_cast<double>(m_config.min_resend_delay.count()));
    ms = std::min(ms, static_cast<double>(m_config.max_resend_delay.count()));
    return std::chrono::microseconds(static_cast<long long>(ms * 1000.0));
}

void ReliableEndpoint::handle_ack(std::uint16_t packet_sequence) {
    PacketRecord* rec = m_records.find(packet_sequence);
    if (!rec) return;

    for (const FragRef& ref : rec->refs) {
        if (ref.channel >= m_channels.size()) continue;
        Channel& ch = *m_channels[ref.channel];
        OutMessage* m = ch.out.find(ref.id);
        if (!m || ref.fragment >= m->frag_count || m->acked[ref.fragment]) continue;
        m->acked[ref.fragment] = 1;

        if (--m->remaining == 0) {
            ch.out.remove(ref.id);
            auto it = std::find(ch.inflight.begin(), ch.inflight.end(), ref.id);
            if (it != ch.inflight.end()) ch.inflight.erase(it);
        }
    }

    m_records.remove(packet_sequence);
}

void ReliableEndpoint::update(time_point now) {
    m_tracker.update(now);
    const auto rd = resend_delay();
    const std::size_t limit = m_config.max_packet_size;
    PacketWriter w(ByteOrder::Big, limit);
    std::vector<FragRef> refs;
    bool open = false;
    std::size_t packets = 0;
    bool budget_left = true;

    bool explicit_ack = false;
    std::uint16_t explicit_base = 0;

    auto finish_packet = [&]() {
        if (!open) return;
        open = false;
        PacketHeader h;
        h.sequence = m_tracker.on_packet_sent(w.size(), now);
        m_tracker.generate_ack(h.ack, h.ack_bits);

        if (explicit_ack) {
            h.ack = explicit_base;
            h.ack_bits = m_tracker.ack_bits_for(explicit_base);
        }

        cover_acks(h.ack);
        PacketWriter hw(ByteOrder::Big, PacketHeader::size);
        h.write(hw);
        std::memcpy(w.data().data(), hw.data().data(), PacketHeader::size);
        PacketRecord* rec = m_records.insert(h.sequence);
        if (rec) rec->refs = refs;
        ++packets;
        ++m_stats.packets_sent;
        m_stats.bytes_sent += w.size();
        m_last_send = now;
        m_ack_owed = false;
        if (on_send_packet) on_send_packet(w.data().data(), w.size());
    };

    auto begin_packet = [&]() {
        w.clear();
        w.write_zeros(PacketHeader::size);
        refs.clear();
        open = true;
    };

    auto room = [&](std::size_t need) -> bool {
        if (!open) {
            if (packets >= m_config.max_packets_per_update) return false;
            begin_packet();
        }

        if (w.size() + need > limit && w.size() > PacketHeader::size) {
            finish_packet();
            if (packets >= m_config.max_packets_per_update) return false;
            begin_packet();
        }

        return true;
    };

    auto write_entry = [&](std::uint8_t channel, std::uint16_t id, bool fragmented, std::uint16_t index, std::uint16_t count, const std::uint8_t* data, std::size_t len) {
        w.write_u8(static_cast<std::uint8_t>(channel | (fragmented ? 0x80 : 0)));
        w.write_u16(id);
        if (fragmented) w.write_u16(index).write_u16(count);
        w.write_varint(len);
        w.write_raw(data, len);
    };

    const std::size_t fs = m_config.fragment_size;

    for (std::size_t c = 0; c < m_channels.size() && budget_left; ++c) {
        Channel& ch = *m_channels[c];
        if (!is_reliable(ch.delivery)) continue;

        for (std::uint16_t id : ch.inflight) {
            OutMessage* m = ch.out.find(id);
            if (!m) continue;
            const bool fragmented = m->frag_count > 1;

            for (std::uint16_t f = 0; f < m->frag_count; ++f) {
                if (m->acked[f]) continue;
                const bool never = m->last_sent[f] == time_point();
                if (!never && now - m->last_sent[f] < rd) continue;
                const std::size_t off = static_cast<std::size_t>(f) * fs;
                const std::size_t len = fragmented ? std::min(fs, m->data.size() - off) : m->data.size();
                const std::size_t need = 3 + (fragmented ? 4 : 0) + varint_size(len) + len;
                if (!room(need)) { budget_left = false; break; }
                write_entry(static_cast<std::uint8_t>(c), id, fragmented, f, m->frag_count, m->data.data() + off, len);
                refs.push_back(FragRef{ static_cast<std::uint8_t>(c), id, f });
                if (!never) ++m_stats.fragments_resent;
                ++m_stats.fragments_sent;
                m->last_sent[f] = now;
            }

            if (!budget_left) break;
        }
    }

    for (std::size_t c = 0; c < m_channels.size() && budget_left; ++c) {
        Channel& ch = *m_channels[c];
        if (is_reliable(ch.delivery)) continue;

        while (!ch.unreliable.empty() && budget_left) {
            PendingUnreliable& p = ch.unreliable.front();
            const bool fragmented = p.frag_count > 1;

            while (p.next < p.frag_count) {
                const std::size_t off = static_cast<std::size_t>(p.next) * fs;
                const std::size_t len = fragmented ? std::min(fs, p.data.size() - off) : p.data.size();
                const std::size_t need = 3 + (fragmented ? 4 : 0) + varint_size(len) + len;
                if (!room(need)) { budget_left = false; break; }
                write_entry(static_cast<std::uint8_t>(c), p.id, fragmented, p.next, p.frag_count, p.data.data() + off, len);
                ++m_stats.fragments_sent;
                ++p.next;
            }

            if (p.next >= p.frag_count) ch.unreliable.pop_front();
        }
    }

    finish_packet();

    if (packets == 0 && (m_ack_owed || now - m_last_send >= m_config.keepalive_interval)) {
        begin_packet();
        finish_packet();
    }

    for (int extra = 0; extra < 32 && !m_ack_queue.empty(); ++extra) {
        std::uint16_t newest = m_ack_queue.front();
        for (std::uint16_t s : m_ack_queue) if (sequence_greater_than(s, newest)) newest = s;
        explicit_ack = true;
        explicit_base = newest;
        begin_packet();
        finish_packet();
    }

    explicit_ack = false;
}

void ReliableEndpoint::cover_acks(std::uint16_t ack) {
    m_ack_queue.erase(std::remove_if(m_ack_queue.begin(), m_ack_queue.end(), [ack](std::uint16_t s) {
        const int d = sequence_difference(ack, s);
        return d >= 0 && d < 32;
    }), m_ack_queue.end());
}

void ReliableEndpoint::receive_packet(const std::uint8_t* data, std::size_t len, time_point now) {
    if (!data || len < PacketHeader::size) { ++m_stats.packets_invalid; return; }
    PacketReader r(data, len);
    PacketHeader h;
    if (!h.read(r)) { ++m_stats.packets_invalid; return; }
    ++m_stats.packets_received;
    m_stats.bytes_received += len;
    const bool fresh = m_tracker.on_packet_received(h.sequence, len, now);
    m_tracker.on_ack_received(h.ack, h.ack_bits, now);
    if (!fresh) return;
    m_ack_owed = true;
    if (m_ack_queue.size() < 4096) m_ack_queue.push_back(h.sequence);

    while (!r.at_end()) {
        const std::uint8_t head = r.read_u8();
        const std::uint8_t channel = head & 0x3F;
        const bool fragmented = (head & 0x80) != 0;
        const std::uint16_t id = r.read_u16();
        std::uint16_t index = 0, count = 1;

        if (fragmented) {
            index = r.read_u16();
            count = r.read_u16();
        }

        const std::uint64_t mlen = r.read_varint();

        if (!r.ok() || mlen > r.remaining() || channel >= m_channels.size() || count == 0 || index >= count || count > m_config.max_fragments ||
            (fragmented && mlen > m_config.fragment_size)) {
            ++m_stats.packets_invalid;
            return;
        }

        const std::uint8_t* payload = r.current();
        r.skip(static_cast<std::size_t>(mlen));
        deliver_fragment(*m_channels[channel], channel, id, index, count, payload, static_cast<std::size_t>(mlen), now);
    }
}

void ReliableEndpoint::deliver_fragment(Channel& ch, std::uint8_t channel, std::uint16_t id, std::uint16_t fragment, std::uint16_t count,
                                        const std::uint8_t* data, std::size_t len, time_point) {
    const std::size_t window = m_config.reliable_window;

    switch (ch.delivery) {
        case Delivery::ReliableOrdered:
            if (sequence_less_than(id, ch.recv_next)) return;
            if (static_cast<std::size_t>(sequence_difference(id, ch.recv_next)) >= window) { ++m_stats.messages_dropped; return; }
            break;
        case Delivery::ReliableUnordered:
            if (ch.seen.exists(id)) return;
            if (sequence_less_than(id, static_cast<std::uint16_t>(ch.seen.sequence() - window))) return;
            break;
        case Delivery::UnreliableSequenced:
            if (ch.has_last && !sequence_greater_than(id, ch.last_seq)) { ++m_stats.messages_dropped; return; }
            break;
        default:
            break;
    }

    std::vector<std::uint8_t> complete;

    if (count == 1) {
        complete.assign(data, data + len);
    } else {
        InMessage* in = ch.in.find(id);

        if (!in) {
            in = ch.in.insert(id);
            if (!in) return;
            in->frag_count = count;
            in->have.assign(count, 0);
            in->data.assign(static_cast<std::size_t>(count) * m_config.fragment_size, 0);
        }

        if (in->complete) return;
        if (in->frag_count != count) { ++m_stats.messages_dropped; return; }
        if (in->have[fragment]) return;
        if (fragment + 1 < count && len != m_config.fragment_size) { ++m_stats.packets_invalid; return; }
        std::memcpy(in->data.data() + static_cast<std::size_t>(fragment) * m_config.fragment_size, data, len);
        in->have[fragment] = 1;
        ++in->received;
        if (fragment + 1 == count) in->last_len = len;
        if (in->received < count) return;
        in->data.resize(static_cast<std::size_t>(count - 1) * m_config.fragment_size + in->last_len);
        complete = std::move(in->data);
        in->data.clear();
        in->complete = true;
        ch.in.remove(id);
    }

    switch (ch.delivery) {
        case Delivery::Unreliable:
            ++m_stats.messages_received;
            if (on_message) on_message(channel, std::move(complete));
            return;

        case Delivery::UnreliableSequenced:
            if (ch.has_last && !sequence_greater_than(id, ch.last_seq)) { ++m_stats.messages_dropped; return; }
            ch.has_last = true;
            ch.last_seq = id;
            ++m_stats.messages_received;
            if (on_message) on_message(channel, std::move(complete));
            return;

        case Delivery::ReliableUnordered:
            ch.seen.insert(id);
            ++m_stats.messages_received;
            if (on_message) on_message(channel, std::move(complete));
            return;

        case Delivery::ReliableOrdered: {
            if (id == ch.recv_next) {
                ++ch.recv_next;
                ++m_stats.messages_received;
                if (on_message) on_message(channel, std::move(complete));
                deliver_ready(ch, channel);
                return;
            }

            InMessage* slot = ch.in.find(id);
            if (!slot) slot = ch.in.insert(id);
            if (!slot) return;
            slot->data = std::move(complete);
            slot->complete = true;
            slot->frag_count = count;
            return;
        }
    }
}

void ReliableEndpoint::deliver_ready(Channel& ch, std::uint8_t channel) {
    for (;;) {
        InMessage* m = ch.in.find(ch.recv_next);
        if (!m || !m->complete) return;
        std::vector<std::uint8_t> data = std::move(m->data);
        ch.in.remove(ch.recv_next);
        ++ch.recv_next;
        ++m_stats.messages_received;
        if (on_message) on_message(channel, std::move(data));
    }
}

} // namespace packet
} // namespace networking
} // namespace fizmo
