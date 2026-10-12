#include "fizmo_library.hpp"
#include "der.hpp"

#include <cstdlib>

namespace fizmo {
namespace networking {
namespace security {
namespace der {

bool parse(const std::uint8_t* p, std::size_t n, Node& out) noexcept {
    if (!p || n < 2) return false;
    const std::uint8_t tag = p[0];
    if ((tag & 0x1F) == 0x1F) return false;
    std::size_t i = 1;
    std::size_t len = p[i++];

    if (len & 0x80) {
        const std::size_t count = len & 0x7F;
        if (count == 0 || count > sizeof(std::size_t) || i + count > n) return false;
        len = 0;
        for (std::size_t k = 0; k < count; ++k) len = (len << 8) | p[i++];
    }

    if (len > n - i) return false;
    out.tag = tag;
    out.raw = p;
    out.raw_len = i + len;
    out.data = p + i;
    out.len = len;
    return true;
}

bool Reader::next(Node& out) noexcept {
    if (m_p >= m_end) return false;
    if (!parse(m_p, static_cast<std::size_t>(m_end - m_p), out)) return false;
    m_p += out.raw_len;
    return true;
}

bool Reader::peek(Node& out) const noexcept {
    if (m_p >= m_end) return false;
    return parse(m_p, static_cast<std::size_t>(m_end - m_p), out);
}

void encode_length(std::size_t len, std::vector<std::uint8_t>& out) {
    if (len < 0x80) {
        out.push_back(static_cast<std::uint8_t>(len));
        return;
    }

    std::uint8_t tmp[sizeof(std::size_t)];
    std::size_t n = 0;

    while (len > 0) {
        tmp[n++] = static_cast<std::uint8_t>(len & 0xFF);
        len >>= 8;
    }

    out.push_back(static_cast<std::uint8_t>(0x80 | n));
    while (n > 0) out.push_back(tmp[--n]);
}

std::vector<std::uint8_t> encode(std::uint8_t tag, const std::uint8_t* content, std::size_t len) {
    std::vector<std::uint8_t> out;
    out.reserve(len + 6);
    out.push_back(tag);
    encode_length(len, out);
    if (len) out.insert(out.end(), content, content + len);
    return out;
}

std::vector<std::uint8_t> encode(std::uint8_t tag, const std::vector<std::uint8_t>& content) {
    return encode(tag, content.data(), content.size());
}

std::vector<std::uint8_t> encode_oid(const std::string& dotted) {
    std::vector<unsigned long long> arcs;
    std::size_t pos = 0;

    while (pos <= dotted.size()) {
        const std::size_t dot = dotted.find('.', pos);
        const std::string part = dotted.substr(pos, dot == std::string::npos ? std::string::npos : dot - pos);
        arcs.push_back(std::strtoull(part.c_str(), nullptr, 10));
        if (dot == std::string::npos) break;
        pos = dot + 1;
    }

    std::vector<std::uint8_t> body;
    if (arcs.size() < 2) return encode(ObjectId, body);
    arcs[1] += arcs[0] * 40;

    for (std::size_t i = 1; i < arcs.size(); ++i) {
        unsigned long long v = arcs[i];
        std::uint8_t tmp[10];
        std::size_t n = 0;
        tmp[n++] = static_cast<std::uint8_t>(v & 0x7F);
        v >>= 7;

        while (v > 0) {
            tmp[n++] = static_cast<std::uint8_t>(0x80 | (v & 0x7F));
            v >>= 7;
        }

        while (n > 0) body.push_back(tmp[--n]);
    }

    return encode(ObjectId, body);
}

std::vector<std::uint8_t> concat(std::initializer_list<std::vector<std::uint8_t>> parts) {
    std::vector<std::uint8_t> out;
    for (const auto& p : parts) out.insert(out.end(), p.begin(), p.end());
    return out;
}

std::string oid_to_string(const std::uint8_t* p, std::size_t n) {
    if (!p || n == 0) return {};
    std::string out;
    unsigned long long value = 0;
    bool first = true;

    for (std::size_t i = 0; i < n; ++i) {
        value = (value << 7) | (p[i] & 0x7F);
        if (p[i] & 0x80) continue;

        if (first) {
            const unsigned long long a = value < 40 ? 0 : (value < 80 ? 1 : 2);
            out += std::to_string(a) + "." + std::to_string(value - a * 40);
            first = false;
        } else {
            out += "." + std::to_string(value);
        }

        value = 0;
    }

    return out;
}

std::string oid_to_string(const Node& node) {
    return node.tag == ObjectId ? oid_to_string(node.data, node.len) : std::string();
}

bool certificate_spki(const std::uint8_t* cert, std::size_t len, std::vector<std::uint8_t>& spki) {
    Node certificate, tbs;
    if (!parse(cert, len, certificate) || certificate.tag != Sequence) return false;
    Reader top(certificate);
    if (!top.next(tbs) || tbs.tag != Sequence) return false;
    Reader r(tbs);
    Node field;
    if (!r.next(field)) return false;
    if (field.context(0) && !r.next(field)) return false;
    if (field.tag != Integer) return false;

    for (int i = 0; i < 4; ++i) {
        if (!r.next(field)) return false;
    }

    if (!r.next(field) || field.tag != Sequence) return false;
    spki = field.raw_bytes();
    return true;
}

bool spki_algorithm(const std::vector<std::uint8_t>& spki, std::string& algorithm_oid, std::string& parameter_oid) {
    Node root, alg, oid, param;
    if (!parse(spki.data(), spki.size(), root) || root.tag != Sequence) return false;
    Reader r(root);
    if (!r.next(alg) || alg.tag != Sequence) return false;
    Reader a(alg);
    if (!a.next(oid) || oid.tag != ObjectId) return false;
    algorithm_oid = oid_to_string(oid);
    parameter_oid.clear();
    if (a.next(param) && param.tag == ObjectId) parameter_oid = oid_to_string(param);
    return true;
}

bool wrap_traditional_key(const std::string& label, const std::vector<std::uint8_t>& key, std::vector<std::uint8_t>& pkcs8) {
    const std::vector<std::uint8_t> version = { Integer, 0x01, 0x00 };

    if (label == "RSA PRIVATE KEY") {
        const std::vector<std::uint8_t> null_param = { Null, 0x00 };
        auto alg = encode(Sequence, concat({ encode_oid("1.2.840.113549.1.1.1"), null_param }));
        pkcs8 = encode(Sequence, concat({ version, alg, encode(OctetString, key) }));
        return true;
    }

    if (label == "EC PRIVATE KEY") {
        Node root, field;
        if (!parse(key.data(), key.size(), root) || root.tag != Sequence) return false;
        Reader r(root);
        std::vector<std::uint8_t> curve;

        while (r.next(field)) {
            if (field.context(0)) {
                Node oid;
                if (parse(field.data, field.len, oid) && oid.tag == ObjectId) curve = oid.raw_bytes();
            }
        }

        if (curve.empty()) return false;
        auto alg = encode(Sequence, concat({ encode_oid("1.2.840.10045.2.1"), curve }));
        pkcs8 = encode(Sequence, concat({ version, alg, encode(OctetString, key) }));
        return true;
    }

    return false;
}

} // namespace der
} // namespace security
} // namespace networking
} // namespace fizmo
