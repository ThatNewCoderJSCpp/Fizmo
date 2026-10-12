#include "fizmo_library.hpp"
#include "client_hello.hpp"

namespace fizmo {
namespace networking {
namespace security {

namespace {

struct Cursor {
    const std::uint8_t* p;
    std::size_t         n;
    std::size_t         i = 0;

    bool u8(std::uint8_t& v) { if (i + 1 > n) return false; v = p[i++]; return true; }
    bool u16(std::uint16_t& v) { if (i + 2 > n) return false; v = static_cast<std::uint16_t>((p[i] << 8) | p[i + 1]); i += 2; return true; }
    bool u24(std::uint32_t& v) { if (i + 3 > n) return false; v = (std::uint32_t(p[i]) << 16) | (std::uint32_t(p[i + 1]) << 8) | p[i + 2]; i += 3; return true; }
    bool skip(std::size_t k) { if (i + k > n) return false; i += k; return true; }
    bool sub(std::size_t k, Cursor& out) { if (i + k > n) return false; out = Cursor{ p + i, k }; i += k; return true; }
    bool done() const { return i >= n; }
};

bool parse_extensions(Cursor& c, ClientHelloInfo& out) {
    std::uint16_t total;
    if (!c.u16(total)) return true;
    Cursor ext;
    if (!c.sub(total, ext)) return false;

    while (!ext.done()) {
        std::uint16_t type, len;
        if (!ext.u16(type) || !ext.u16(len)) return false;
        Cursor body;
        if (!ext.sub(len, body)) return false;
        out.extensions.push_back(type);

        switch (type) {
            case 0: {
                std::uint16_t list;
                if (!body.u16(list)) return false;

                while (!body.done()) {
                    std::uint8_t name_type;
                    std::uint16_t name_len;
                    if (!body.u8(name_type) || !body.u16(name_len) || body.i + name_len > body.n) return false;
                    if (name_type == 0 && out.server_name.empty()) out.server_name.assign(reinterpret_cast<const char*>(body.p + body.i), name_len);
                    body.skip(name_len);
                }

                break;
            }

            case 5:
                out.requests_ocsp = true;
                break;

            case 10: {
                std::uint16_t list;
                if (!body.u16(list)) return false;
                std::uint16_t g;
                while (body.u16(g)) out.supported_groups.push_back(g);
                break;
            }

            case 13: {
                std::uint16_t list;
                if (!body.u16(list)) return false;
                std::uint16_t s;
                while (body.u16(s)) out.signature_algorithms.push_back(s);
                break;
            }

            case 16: {
                std::uint16_t list;
                if (!body.u16(list)) return false;

                while (!body.done()) {
                    std::uint8_t plen;
                    if (!body.u8(plen) || body.i + plen > body.n) return false;
                    out.alpn_protocols.emplace_back(reinterpret_cast<const char*>(body.p + body.i), plen);
                    body.skip(plen);
                }

                break;
            }

            case 42:
                out.offers_early_data = true;
                break;

            case 43: {
                std::uint8_t list;
                if (!body.u8(list)) return false;
                std::uint16_t v;

                while (body.u16(v)) {
                    out.supported_versions.push_back(v);
                    if (v == 0x0304) out.offers_tls13 = true;
                }

                break;
            }

            case 49:
                out.offers_post_handshake_auth = true;
                break;

            default:
                break;
        }
    }

    return true;
}

} // namespace

ClientHelloStatus parse_client_hello(const std::uint8_t* data, std::size_t len, ClientHelloInfo& out) {
    out = ClientHelloInfo();
    if (len == 0) return ClientHelloStatus::NeedMoreData;
    if (data[0] != 22) return ClientHelloStatus::NotTls;
    std::vector<std::uint8_t> handshake;
    std::size_t pos = 0;

    for (;;) {
        if (len - pos < 5) return ClientHelloStatus::NeedMoreData;
        if (data[pos] != 22) return ClientHelloStatus::Malformed;
        if (data[pos + 1] != 3) return ClientHelloStatus::NotTls;
        const std::size_t rlen = (std::size_t(data[pos + 3]) << 8) | data[pos + 4];
        if (rlen == 0 || rlen > 16384 + 256) return ClientHelloStatus::Malformed;
        if (len - pos - 5 < rlen) return ClientHelloStatus::NeedMoreData;
        handshake.insert(handshake.end(), data + pos + 5, data + pos + 5 + rlen);
        pos += 5 + rlen;
        if (handshake.size() < 4) continue;
        if (handshake[0] != 1) return ClientHelloStatus::Malformed;
        const std::size_t mlen = (std::size_t(handshake[1]) << 16) | (std::size_t(handshake[2]) << 8) | handshake[3];
        if (mlen > 1u << 17) return ClientHelloStatus::Malformed;
        if (handshake.size() >= 4 + mlen) break;
    }

    const std::size_t mlen = (std::size_t(handshake[1]) << 16) | (std::size_t(handshake[2]) << 8) | handshake[3];
    Cursor c{ handshake.data() + 4, mlen };
    std::uint8_t sid_len, comp_len;
    std::uint16_t cs_len;
    if (!c.u16(out.legacy_version) || !c.skip(32) || !c.u8(sid_len) || c.i + sid_len > c.n) return ClientHelloStatus::Malformed;
    out.session_id.assign(c.p + c.i, c.p + c.i + sid_len);
    c.skip(sid_len);
    if (!c.u16(cs_len) || (cs_len & 1) || c.i + cs_len > c.n) return ClientHelloStatus::Malformed;

    for (std::size_t k = 0; k < cs_len; k += 2) {
        std::uint16_t s;
        c.u16(s);
        out.cipher_suites.push_back(s);
    }

    if (!c.u8(comp_len) || !c.skip(comp_len)) return ClientHelloStatus::Malformed;
    if (!parse_extensions(c, out)) return ClientHelloStatus::Malformed;
    return ClientHelloStatus::Complete;
}

} // namespace security
} // namespace networking
} // namespace fizmo
