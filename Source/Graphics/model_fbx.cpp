#include "fizmo_library.hpp"
#include "model_3d.hpp"
#include "../Images/Formats/format_util.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <string>
#include <system_error>
#include <unordered_map>
#include <utility>
#include <vector>

namespace fizmo {
namespace graphics {

namespace {
namespace fbx {

constexpr std::size_t max_array = std::size_t(1) << 27;
constexpr int max_depth = 64;
constexpr double ticks_per_second = 46186158000.0;
constexpr double pi = 3.14159265358979323846;
constexpr std::size_t max_samples = std::size_t(1) << 20;
constexpr std::size_t max_joints = 65535;

std::int64_t to_int(double v) noexcept {
    if (!(v == v)) return 0;
    if (v >= 9.2e18) return std::numeric_limits<std::int64_t>::max();
    if (v <= -9.2e18) return std::numeric_limits<std::int64_t>::min();
    return static_cast<std::int64_t>(v);
}

struct Prop {
    char                      type = 0;
    std::int64_t              i = 0;
    double                    d = 0.0;
    std::string               s;
    std::vector<double>       f;
    std::vector<std::int64_t> n;

    bool is_string() const noexcept { return type == 'S' || type == 'R'; }
    bool is_number() const noexcept { return type == 'C' || type == 'Y' || type == 'I' || type == 'L' || type == 'F' || type == 'D'; }
};

struct Node {
    std::string       name;
    std::vector<Prop> props;
    std::vector<Node> kids;

    const Node* child(const char* key) const noexcept {
        for (const Node& k : kids) if (k.name == key) return &k;
        return nullptr;
    }
    double num(std::size_t k, double fallback = 0.0) const noexcept { return k < props.size() && props[k].is_number() ? props[k].d : fallback; }
    std::int64_t integer(std::size_t k, std::int64_t fallback = 0) const noexcept { return k < props.size() && props[k].is_number() ? props[k].i : fallback; }
    std::string str(std::size_t k) const { return k < props.size() && props[k].is_string() ? props[k].s : std::string(); }
};

std::vector<double> doubles(const Node* node) {
    std::vector<double> out;
    if (!node) return out;
    for (const Prop& p : node->props) {
        if (!p.f.empty()) out.insert(out.end(), p.f.begin(), p.f.end());
        else if (!p.n.empty()) for (std::int64_t v : p.n) out.push_back(static_cast<double>(v));
        else if (p.is_number()) out.push_back(p.d);
        if (out.size() > max_array) { out.resize(max_array); break; }
    }
    return out;
}

std::vector<std::int64_t> ints(const Node* node) {
    std::vector<std::int64_t> out;
    if (!node) return out;
    for (const Prop& p : node->props) {
        if (!p.n.empty()) out.insert(out.end(), p.n.begin(), p.n.end());
        else if (!p.f.empty()) for (double v : p.f) out.push_back(to_int(v));
        else if (p.is_number()) out.push_back(p.i);
        if (out.size() > max_array) { out.resize(max_array); break; }
    }
    return out;
}

std::vector<std::uint8_t> base64(const std::string& s) {
    auto val = [](char c) -> int {
        if (c >= 'A' && c <= 'Z') return c - 'A';
        if (c >= 'a' && c <= 'z') return c - 'a' + 26;
        if (c >= '0' && c <= '9') return c - '0' + 52;
        if (c == '+' || c == '-') return 62;
        if (c == '/' || c == '_') return 63;
        return -1;
    };
    std::vector<std::uint8_t> out;
    out.reserve(s.size() * 3 / 4);
    std::uint32_t buf = 0;
    int bits = 0;
    for (char c : s) {
        const int v = val(c);
        if (v < 0) continue;
        buf = ((buf << 6) | static_cast<std::uint32_t>(v)) & 0xFFFFFFu;
        bits += 6;
        if (bits >= 8) { bits -= 8; out.push_back(static_cast<std::uint8_t>((buf >> bits) & 0xFFu)); }
    }
    return out;
}

class Binary {
public:
    Binary(const std::uint8_t* data, std::size_t size, std::uint32_t version) noexcept
        : m_data(data), m_size(size), m_wide(version >= 7500),
          m_budget(std::min<std::uint64_t>(std::uint64_t(16) << 30, (std::uint64_t(1) << 30) + std::uint64_t(size) * 128u)) {}

    bool parse(Node& root, std::string& error) {
        std::size_t off = 27;
        const std::size_t header = m_wide ? 25 : 13;
        while (off + header <= m_size) {
            Node node;
            bool null_record = false;
            if (!read_node(off, m_size, node, null_record, 0)) { error = m_error.empty() ? "malformed binary FBX" : m_error; return false; }
            if (null_record) break;
            root.kids.push_back(std::move(node));
        }
        return true;
    }

private:
    const std::uint8_t* m_data;
    std::size_t         m_size;
    bool                m_wide;
    std::uint64_t       m_budget;
    std::string         m_error;

    bool fail(const char* why) { m_error = why; return false; }

    template <class T>
    bool get(std::size_t off, std::size_t limit, T& v) const noexcept {
        if (off > limit || sizeof(T) > limit - off) return false;
        std::memcpy(&v, m_data + off, sizeof(T));
        return true;
    }

    bool read_node(std::size_t& off, std::size_t limit, Node& out, bool& null_record, int depth) {
        std::uint64_t end = 0, count = 0, plen = 0;
        std::uint8_t nlen = 0;
        const std::size_t header = m_wide ? 25 : 13;
        if (off > limit || header > limit - off) return fail("truncated FBX node header");
        if (m_wide) {
            get(off, limit, end); get(off + 8, limit, count); get(off + 16, limit, plen); get(off + 24, limit, nlen);
        } else {
            std::uint32_t e = 0, c = 0, p = 0;
            get(off, limit, e); get(off + 4, limit, c); get(off + 8, limit, p); get(off + 12, limit, nlen);
            end = e; count = c; plen = p;
        }
        if (end == 0 && count == 0 && plen == 0 && nlen == 0) { null_record = true; off += header; return true; }
        if (end <= off + header || end > limit) return fail("FBX node extends past its parent");
        std::size_t p = off + header;
        const std::size_t node_end = static_cast<std::size_t>(end);
        if (nlen > node_end - p) return fail("truncated FBX node name");
        out.name.assign(reinterpret_cast<const char*>(m_data + p), nlen);
        p += nlen;
        if (plen > node_end - p) return fail("FBX property list extends past its node");
        const std::size_t pend = p + static_cast<std::size_t>(plen);
        if (count > plen) return fail("bad FBX property count");
        out.props.reserve(static_cast<std::size_t>(count));
        for (std::uint64_t k = 0; k < count; ++k) {
            Prop prop;
            if (!read_prop(p, pend, prop)) return false;
            out.props.push_back(std::move(prop));
        }
        p = pend;
        while (p < node_end) {
            if (depth + 1 > max_depth) return fail("FBX nodes nested too deeply");
            Node kid;
            bool kid_null = false;
            if (!read_node(p, node_end, kid, kid_null, depth + 1)) return false;
            if (kid_null) break;
            out.kids.push_back(std::move(kid));
        }
        off = node_end;
        return true;
    }

    bool read_prop(std::size_t& off, std::size_t end, Prop& p) {
        if (off >= end) return fail("truncated FBX property");
        const char t = static_cast<char>(m_data[off++]);
        p.type = t;
        switch (t) {
            case 'C': { std::uint8_t v = 0; if (!get(off, end, v)) return fail("truncated FBX property"); off += 1; p.i = v ? 1 : 0; p.d = static_cast<double>(p.i); return true; }
            case 'Y': { std::int16_t v = 0; if (!get(off, end, v)) return fail("truncated FBX property"); off += 2; p.i = v; p.d = v; return true; }
            case 'I': { std::int32_t v = 0; if (!get(off, end, v)) return fail("truncated FBX property"); off += 4; p.i = v; p.d = v; return true; }
            case 'L': { std::int64_t v = 0; if (!get(off, end, v)) return fail("truncated FBX property"); off += 8; p.i = v; p.d = static_cast<double>(v); return true; }
            case 'F': { float v = 0; if (!get(off, end, v)) return fail("truncated FBX property"); off += 4; p.d = v; p.i = to_int(v); return true; }
            case 'D': { double v = 0; if (!get(off, end, v)) return fail("truncated FBX property"); off += 8; p.d = v; p.i = to_int(v); return true; }
            case 'S':
            case 'R': {
                std::uint32_t len = 0;
                if (!get(off, end, len)) return fail("truncated FBX string");
                off += 4;
                if (len > end - off) return fail("FBX string extends past its node");
                p.s.assign(reinterpret_cast<const char*>(m_data + off), len);
                off += len;
                return true;
            }
            case 'f': case 'd': case 'i': case 'l': case 'b': {
                std::uint32_t count = 0, enc = 0, clen = 0;
                if (!get(off, end, count) || !get(off + 4, end, enc) || !get(off + 8, end, clen)) return fail("truncated FBX array header");
                off += 12;
                if (count > max_array) return fail("FBX array is too large");
                const std::size_t elem = (t == 'd' || t == 'l') ? 8 : (t == 'b') ? 1 : 4;
                const std::size_t bytes = static_cast<std::size_t>(count) * elem;
                if (clen > end - off) return fail("FBX array extends past its node");
                const std::uint8_t* src = m_data + off;
                off += clen;
                std::vector<std::uint8_t> tmp;
                const std::uint8_t* raw = src;
                if (enc == 0) {
                    if (clen < bytes) return fail("truncated FBX array");
                } else if (enc == 1) {
                    if (bytes > m_budget) return fail("FBX file decompresses to too much data");
                    m_budget -= bytes;
                    if (bytes > 0) {
                        if (!images::fmt::zlib_inflate(src, clen, tmp, bytes) || tmp.size() != bytes) return fail("corrupt zlib data in FBX array");
                        raw = tmp.data();
                    }
                } else {
                    return fail("unknown FBX array encoding");
                }
                if (t == 'f' || t == 'd') p.f.resize(count);
                else p.n.resize(count);
                for (std::size_t k = 0; k < count; ++k) {
                    const std::uint8_t* q = raw + k * elem;
                    switch (t) {
                        case 'f': { float v; std::memcpy(&v, q, 4); p.f[k] = v; break; }
                        case 'd': { double v; std::memcpy(&v, q, 8); p.f[k] = v; break; }
                        case 'i': { std::int32_t v; std::memcpy(&v, q, 4); p.n[k] = v; break; }
                        case 'l': { std::int64_t v; std::memcpy(&v, q, 8); p.n[k] = v; break; }
                        default:  p.n[k] = *q ? 1 : 0; break;
                    }
                }
                return true;
            }
            default:
                return fail("unknown FBX property type");
        }
    }
};

class Ascii {
public:
    Ascii(const char* data, std::size_t size) noexcept : m_p(data), m_end(data + size) {}

    bool parse(Node& root, std::string& error) {
        next();
        if (!parse_nodes(root.kids, 0, true)) { error = m_error.empty() ? "malformed ASCII FBX" : m_error; return false; }
        return true;
    }

private:
    enum class Tok { End, Key, Str, Word, Count, Comma, Open, Close };

    const char* m_p;
    const char* m_end;
    Tok         m_tok = Tok::End;
    std::string m_text;
    std::string m_error;

    static bool is_space(char c) noexcept { return c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '\f' || c == '\v'; }
    static bool is_delim(char c) noexcept { return is_space(c) || c == ',' || c == '{' || c == '}' || c == '"' || c == ';' || c == ':'; }

    void next() {
        while (true) {
            while (m_p < m_end && is_space(*m_p)) ++m_p;
            if (m_p < m_end && *m_p == ';') { while (m_p < m_end && *m_p != '\n') ++m_p; continue; }
            break;
        }
        m_text.clear();
        if (m_p >= m_end) { m_tok = Tok::End; return; }
        const char c = *m_p;
        if (c == '{') { ++m_p; m_tok = Tok::Open; return; }
        if (c == '}') { ++m_p; m_tok = Tok::Close; return; }
        if (c == ',') { ++m_p; m_tok = Tok::Comma; return; }
        if (c == '"') {
            ++m_p;
            const char* q = static_cast<const char*>(std::memchr(m_p, '"', static_cast<std::size_t>(m_end - m_p)));
            if (!q) q = m_end;
            m_text.assign(m_p, q);
            m_p = q < m_end ? q + 1 : m_end;
            std::size_t at = 0;
            while ((at = m_text.find("&quot;", at)) != std::string::npos) { m_text.replace(at, 6, "\""); ++at; }
            m_tok = Tok::Str;
            return;
        }
        if (c == '*') {
            ++m_p;
            const char* s = m_p;
            while (m_p < m_end && !is_delim(*m_p)) ++m_p;
            m_text.assign(s, m_p);
            m_tok = Tok::Count;
            return;
        }
        const char* s = m_p;
        while (m_p < m_end && !is_delim(*m_p)) ++m_p;
        if (m_p == s) { ++m_p; next(); return; }
        m_text.assign(s, m_p);
        if (m_p < m_end && *m_p == ':') { ++m_p; m_tok = Tok::Key; return; }
        m_tok = Tok::Word;
    }

    static bool number(const std::string& w, double& d, std::int64_t& i, bool& integral) {
        if (w.empty()) return false;
        char* e = nullptr;
        integral = w.find_first_of(".eEnNiI") == std::string::npos;
        if (integral) {
            const long long v = std::strtoll(w.c_str(), &e, 10);
            if (e && *e == 0) { i = v; d = static_cast<double>(v); return true; }
            integral = false;
        }
        d = std::strtod(w.c_str(), &e);
        if (!e || *e != 0) return false;
        i = to_int(d);
        return true;
    }

    bool parse_nodes(std::vector<Node>& out, int depth, bool top) {
        while (true) {
            switch (m_tok) {
                case Tok::End: return true;
                case Tok::Close:
                    next();
                    if (top) continue;
                    return true;
                case Tok::Key: {
                    Node n;
                    n.name = m_text;
                    next();
                    if (!parse_body(n, depth)) return false;
                    out.push_back(std::move(n));
                    continue;
                }
                default:
                    next();
                    continue;
            }
        }
    }

    bool parse_body(Node& n, int depth) {
        bool array = false;
        while (true) {
            switch (m_tok) {
                case Tok::Str: {
                    Prop p;
                    p.type = 'S';
                    p.s = m_text;
                    n.props.push_back(std::move(p));
                    next();
                    break;
                }
                case Tok::Word: {
                    Prop p;
                    bool integral = false;
                    if (number(m_text, p.d, p.i, integral)) p.type = integral ? 'L' : 'D';
                    else { p.type = 'S'; p.s = m_text; }
                    n.props.push_back(std::move(p));
                    next();
                    break;
                }
                case Tok::Count: array = true; next(); break;
                case Tok::Comma: next(); break;
                case Tok::Open:
                    next();
                    if (depth + 1 > max_depth) { m_error = "FBX nodes nested too deeply"; return false; }
                    if (array) return parse_array(n);
                    return parse_nodes(n.kids, depth + 1, false);
                default:
                    return true;
            }
        }
    }

    bool parse_array(Node& n) {
        Prop p;
        std::vector<double> fs;
        std::vector<std::int64_t> is;
        bool all_int = true;
        while (m_tok != Tok::Close && m_tok != Tok::End) {
            if (m_tok == Tok::Word) {
                double d = 0.0;
                std::int64_t i = 0;
                bool integral = false;
                if (number(m_text, d, i, integral)) {
                    if (fs.size() >= max_array) { m_error = "FBX array is too large"; return false; }
                    fs.push_back(d);
                    if (all_int) { if (integral) is.push_back(i); else { all_int = false; is.clear(); is.shrink_to_fit(); } }
                }
            }
            next();
        }
        if (m_tok == Tok::Close) next();
        if (all_int) { p.type = 'l'; p.n = std::move(is); }
        else { p.type = 'd'; p.f = std::move(fs); }
        n.props.push_back(std::move(p));
        return true;
    }
};

bool parse(const std::uint8_t* data, std::size_t size, Node& root, std::uint32_t& version, std::string& error) {
    static const char magic[] = "Kaydara FBX Binary  ";
    if (size >= 27 && std::memcmp(data, magic, 20) == 0) {
        std::memcpy(&version, data + 23, 4);
        if (version < 7000) { error = "FBX version " + std::to_string(version) + " (FBX 6 or older) is not supported; re-export the model as FBX 7 (2011 or newer) or glTF"; return false; }
        if (version >= 8000) { error = "FBX version " + std::to_string(version) + " is not supported"; return false; }
        return Binary(data, size, version).parse(root, error);
    }
    if (size >= 18 && std::memcmp(data, "Kaydara FBX Binary", 18) == 0) { error = "truncated binary FBX header"; return false; }
    const char* text = reinterpret_cast<const char*>(data);
    const std::string head(text, std::min<std::size_t>(size, 4096));
    const std::size_t tag = head.find("; FBX ");
    if (tag != std::string::npos) {
        int major = 0, minor = 0, patch = 0;
        if (std::sscanf(head.c_str() + tag + 6, "%d.%d.%d", &major, &minor, &patch) >= 2 && major > 0 && major < 100 && minor >= 0 && minor < 10) version = static_cast<std::uint32_t>(major * 1000 + minor * 100 + std::max(0, std::min(patch, 99)));
    }
    if (version > 0 && version < 7000) { error = "FBX version " + std::to_string(version) + " (FBX 6 or older) is not supported; re-export the model as FBX 7 (2011 or newer) or glTF"; return false; }
    if (version == 0 && head.find("FBXHeaderExtension") == std::string::npos && head.find("Objects") == std::string::npos) { error = "not an FBX file"; return false; }
    if (!Ascii(text, size).parse(root, error)) return false;
    if (const Node* ext = root.child("FBXHeaderExtension")) if (const Node* v = ext->child("FBXVersion")) {
        const std::int64_t fv = v->integer(0, 0);
        if (fv > 0 && fv < 100000) version = static_cast<std::uint32_t>(fv);
    }
    if (version == 0) {
        if (const Node* objects = root.child("Objects")) {
            for (const Node& o : objects->kids) { version = !o.props.empty() && o.props[0].is_number() ? 7000u : 6000u; break; }
        }
    }
    if (version == 0) { error = "not an FBX file (no version and no objects)"; return false; }
    if (version < 7000) { error = "FBX version " + std::to_string(version) + " (FBX 6 or older) is not supported; re-export the model as FBX 7 (2011 or newer) or glTF"; return false; }
    if (version >= 8000) { error = "FBX version " + std::to_string(version) + " is not supported"; return false; }
    return true;
}

struct V3 {
    double x = 0.0, y = 0.0, z = 0.0;
};

V3 operator+(V3 a, V3 b) noexcept { return { a.x + b.x, a.y + b.y, a.z + b.z }; }
V3 operator-(V3 a, V3 b) noexcept { return { a.x - b.x, a.y - b.y, a.z - b.z }; }
V3 hadamard(V3 a, V3 b) noexcept { return { a.x * b.x, a.y * b.y, a.z * b.z }; }
V3 cross(V3 a, V3 b) noexcept { return { a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x }; }
bool nonzero(V3 a) noexcept { return std::abs(a.x) > 1e-12 || std::abs(a.y) > 1e-12 || std::abs(a.z) > 1e-12; }
double finite_or(double v, double fallback) noexcept { return std::isfinite(v) ? v : fallback; }

using Quat = model_math::Quat;

Quat qmul(const Quat& a, const Quat& b) noexcept {
    return {
        a.w * b.x + a.x * b.w + a.y * b.z - a.z * b.y,
        a.w * b.y - a.x * b.z + a.y * b.w + a.z * b.x,
        a.w * b.z + a.x * b.y - a.y * b.x + a.z * b.w,
        a.w * b.w - a.x * b.x - a.y * b.y - a.z * b.z
    };
}

Quat conj(const Quat& q) noexcept { return { -q.x, -q.y, -q.z, q.w }; }

Quat axis_angle(int axis, double degrees) noexcept {
    const double h = degrees * pi / 360.0;
    const double s = std::sin(h);
    Quat q{ 0.0, 0.0, 0.0, std::cos(h) };
    if (axis == 0) q.x = s; else if (axis == 1) q.y = s; else q.z = s;
    return q;
}

Quat euler(V3 deg, int order) noexcept {
    const Quat qx = axis_angle(0, deg.x), qy = axis_angle(1, deg.y), qz = axis_angle(2, deg.z);
    switch (order) {
        case 1:  return qmul(qmul(qy, qz), qx);
        case 2:  return qmul(qmul(qx, qz), qy);
        case 3:  return qmul(qmul(qz, qx), qy);
        case 4:  return qmul(qmul(qy, qx), qz);
        case 5:  return qmul(qmul(qx, qy), qz);
        default: return qmul(qmul(qz, qy), qx);
    }
}

V3 rotate(const Quat& q, V3 v) noexcept {
    const V3 u{ q.x, q.y, q.z };
    const V3 t0 = cross(u, v);
    const V3 t{ t0.x * 2.0, t0.y * 2.0, t0.z * 2.0 };
    const V3 c = cross(u, t);
    return { v.x + q.w * t.x + c.x, v.y + q.w * t.y + c.y, v.z + q.w * t.z + c.z };
}

model_math::Mat4 matrix16(const std::vector<double>& m) noexcept {
    if (m.size() < 16) return model_math::identity();
    model_math::Mat4 r{};
    for (int c = 0; c < 4; ++c) for (int rr = 0; rr < 4; ++rr) r[static_cast<std::size_t>(rr * 4 + c)] = finite_or(m[static_cast<std::size_t>(c * 4 + rr)], (rr == c) ? 1.0 : 0.0);
    return r;
}

double det3(const model_math::Mat4& m) noexcept {
    return m[0] * (m[5] * m[10] - m[6] * m[9]) - m[1] * (m[4] * m[10] - m[6] * m[8]) + m[2] * (m[4] * m[9] - m[5] * m[8]);
}

std::string object_name(const std::string& raw) {
    const std::size_t sep = raw.find(std::string("\x00\x01", 2));
    if (sep != std::string::npos) return raw.substr(0, sep);
    const std::size_t colons = raw.find("::");
    if (colons != std::string::npos) return raw.substr(colons + 2);
    return raw;
}

struct Object {
    const Node*  node = nullptr;
    std::int64_t id = 0;
    std::string  cls;
    std::string  sub;
    std::string  name;
};

struct Link {
    std::int64_t id = 0;
    std::string  prop;
};

struct Props {
    const Node* own = nullptr;
    const Node* tmpl = nullptr;

    const Node* find(const std::string& name) const noexcept {
        for (const Node* src : { own, tmpl }) {
            if (!src) continue;
            for (const Node& p : src->kids) if ((p.name == "P" || p.name == "Property") && !p.props.empty() && p.props[0].is_string() && p.props[0].s == name) return &p;
        }
        return nullptr;
    }
    bool has(const std::string& name) const noexcept { return find(name) != nullptr; }
    double num(const std::string& name, double fallback) const noexcept {
        const Node* p = find(name);
        if (!p) return fallback;
        const std::size_t at = p->name == "P" ? 4 : 3;
        return finite_or(p->num(at, fallback), fallback);
    }
    std::int64_t integer(const std::string& name, std::int64_t fallback) const noexcept {
        const Node* p = find(name);
        if (!p) return fallback;
        return p->integer(p->name == "P" ? 4 : 3, fallback);
    }
    V3 vec(const std::string& name, V3 fallback) const noexcept {
        const Node* p = find(name);
        if (!p) return fallback;
        const std::size_t at = p->name == "P" ? 4 : 3;
        return { finite_or(p->num(at, fallback.x), fallback.x), finite_or(p->num(at + 1, fallback.y), fallback.y), finite_or(p->num(at + 2, fallback.z), fallback.z) };
    }
};

struct Xform {
    V3   t, r, s{ 1.0, 1.0, 1.0 };
    V3   roff, rp, soff, sp, pre, post;
    V3   gt, gr, gs{ 1.0, 1.0, 1.0 };
    int  order = 0;
    bool pivots = false;
};

struct Local {
    V3   t;
    Quat r;
    V3   s{ 1.0, 1.0, 1.0 };
};

Local evaluate(const Xform& x, V3 t, V3 r, V3 s) noexcept {
    const Quat q = model_math::normalize(qmul(qmul(euler(x.pre, 0), euler(r, x.order)), conj(euler(x.post, 0))));
    const V3 c1 = x.soff + x.sp - hadamard(s, x.sp);
    return { t + x.roff + x.rp + rotate(q, c1) - rotate(q, x.rp), q, s };
}

struct Curve {
    std::vector<std::int64_t> times;
    std::vector<double>       values;
    std::vector<std::uint8_t> constant;

    double eval(std::int64_t t, double fallback) const noexcept {
        if (times.empty()) return fallback;
        if (t <= times.front()) return values.front();
        if (t >= times.back()) return values.back();
        const std::size_t k = static_cast<std::size_t>(std::upper_bound(times.begin(), times.end(), t) - times.begin()) - 1;
        if (k + 1 >= times.size()) return values.back();
        if (constant[k] == 1) return values[k];
        if (constant[k] == 2) return values[k + 1];
        const double span = static_cast<double>(times[k + 1] - times[k]);
        const double f = span > 0.0 ? static_cast<double>(t - times[k]) / span : 0.0;
        return values[k] + (values[k + 1] - values[k]) * f;
    }
};

struct Layer {
    int                       mapping = -1;
    bool                      indexed = false;
    std::vector<double>       data;
    std::vector<std::int64_t> index;
    std::size_t               count = 0;
    int                       comps = 1;

    bool valid() const noexcept { return mapping >= 0 && count > 0; }

    std::int64_t key(std::size_t corner, std::int64_t cp, std::size_t poly) const noexcept {
        switch (mapping) {
            case 0:  return static_cast<std::int64_t>(corner);
            case 1:  return cp;
            case 2:  return static_cast<std::int64_t>(poly);
            default: return 0;
        }
    }

    std::int64_t resolve(std::size_t corner, std::int64_t cp, std::size_t poly) const noexcept {
        if (!valid()) return -1;
        std::int64_t k = key(corner, cp, poly);
        if (k < 0) return -1;
        if (indexed) {
            if (static_cast<std::uint64_t>(k) >= index.size()) return -1;
            k = index[static_cast<std::size_t>(k)];
        }
        return k >= 0 && static_cast<std::uint64_t>(k) < count ? k : -1;
    }
};

int mapping_of(const std::string& s) noexcept {
    if (s == "ByPolygonVertex") return 0;
    if (s == "ByVertice" || s == "ByVertex" || s == "ByControlPoint") return 1;
    if (s == "ByPolygon") return 2;
    if (s == "AllSame") return 3;
    return -1;
}

const Node* layer_element(const Node& geom, const char* name) noexcept {
    const Node* first = nullptr;
    for (const Node& k : geom.kids) {
        if (k.name != name) continue;
        if (k.integer(0, 0) == 0) return &k;
        if (!first) first = &k;
    }
    return first;
}

Layer read_layer(const Node& geom, const char* elem, const char* data_name, const char* index_name, const char* alt_index, int comps) {
    Layer l;
    const Node* e = layer_element(geom, elem);
    if (!e) return l;
    const Node* mi = e->child("MappingInformationType");
    const Node* ri = e->child("ReferenceInformationType");
    l.mapping = mapping_of(mi ? mi->str(0) : std::string("AllSame"));
    const std::string ref = ri ? ri->str(0) : std::string("Direct");
    l.indexed = ref == "IndexToDirect" || ref == "Index";
    l.comps = comps;
    l.data = doubles(e->child(data_name));
    l.count = l.data.size() / static_cast<std::size_t>(comps);
    if (l.indexed) {
        const Node* in = e->child(index_name);
        if (!in && alt_index) in = e->child(alt_index);
        l.index = ints(in);
        if (!in) l.indexed = false;
    }
    return l;
}

struct VertexKey {
    std::int64_t a, b, c, d;
    bool operator==(const VertexKey& o) const noexcept { return a == o.a && b == o.b && c == o.c && d == o.d; }
};

struct VertexKeyHash {
    std::size_t operator()(const VertexKey& k) const noexcept {
        std::uint64_t h = 1469598103934665603ull;
        for (std::int64_t v : { k.a, k.b, k.c, k.d }) { h ^= static_cast<std::uint64_t>(v) + 0x9E3779B97F4A7C15ull + (h << 6) + (h >> 2); }
        return static_cast<std::size_t>(h);
    }
};

struct Influence {
    std::array<int, 4>   joint{ { -1, -1, -1, -1 } };
    std::array<float, 4> weight{ { 0.0f, 0.0f, 0.0f, 0.0f } };

    void add(int j, float w) noexcept {
        if (!(w > 0.0f) || !std::isfinite(w)) return;
        for (int k = 0; k < 4; ++k) if (joint[static_cast<std::size_t>(k)] == j) { weight[static_cast<std::size_t>(k)] += w; return; }
        int lo = 0;
        for (int k = 1; k < 4; ++k) if (weight[static_cast<std::size_t>(k)] < weight[static_cast<std::size_t>(lo)]) lo = k;
        if (w > weight[static_cast<std::size_t>(lo)]) { joint[static_cast<std::size_t>(lo)] = j; weight[static_cast<std::size_t>(lo)] = w; }
    }
};

std::uint8_t to_byte(double v) noexcept { return static_cast<std::uint8_t>(std::lround(std::max(0.0, std::min(1.0, finite_or(v, 0.0))) * 255.0)); }

std::string normalize_slashes(std::string s) {
    for (char& c : s) if (c == '\\') c = '/';
    return s;
}

std::string basename_of(const std::string& s) {
    const std::string n = normalize_slashes(s);
    const std::size_t at = n.find_last_of('/');
    return at == std::string::npos ? n : n.substr(at + 1);
}

bool ends_with(const std::string& s, const char* suffix) {
    const std::size_t n = std::strlen(suffix);
    return s.size() >= n && s.compare(s.size() - n, n, suffix) == 0;
}

struct NormalJob {
    std::size_t mesh;
    std::size_t primitive;
};

class Builder {
public:
    Builder(const Node& root, const std::filesystem::path& path, const ModelImportOptions& o, Model3D& m)
        : m_root(root), m_path(path), m_opt(o), m_model(m) {}

    bool build(std::vector<NormalJob>& normals, model_math::Mat4& axes, std::string& error) {
        index_objects();
        index_templates();
        axes = axis_matrix();
        build_materials();
        build_nodes();
        if (!build_meshes(normals, error)) return false;
        build_animations();
        if (m_model.nodes.empty()) { error = "FBX file contains no models"; return false; }
        return true;
    }

private:
    const Node&                                             m_root;
    std::filesystem::path                                   m_path;
    const ModelImportOptions&                               m_opt;
    Model3D&                                                m_model;
    std::vector<Object>                                     m_objects;
    std::unordered_map<std::int64_t, std::size_t>           m_by_id;
    std::unordered_map<std::int64_t, std::vector<Link>>     m_children;
    std::unordered_map<std::int64_t, std::vector<Link>>     m_parents;
    std::unordered_map<std::string, const Node*>            m_templates;
    std::unordered_map<std::int64_t, int>                   m_node_of;
    std::vector<Xform>                                      m_xforms;
    std::vector<std::int64_t>                               m_node_ids;
    std::unordered_map<std::int64_t, int>                   m_material_of;
    std::unordered_map<std::int64_t, int>                   m_texture_of;
    std::unordered_map<std::int64_t, int>                   m_video_texture;
    std::vector<model_math::Mat4>                           m_rest_global;

    const Object* object(std::int64_t id) const noexcept {
        const auto it = m_by_id.find(id);
        return it == m_by_id.end() ? nullptr : &m_objects[it->second];
    }

    const std::vector<Link>& children(std::int64_t id) const {
        static const std::vector<Link> none;
        const auto it = m_children.find(id);
        return it == m_children.end() ? none : it->second;
    }

    const std::vector<Link>& parents(std::int64_t id) const {
        static const std::vector<Link> none;
        const auto it = m_parents.find(id);
        return it == m_parents.end() ? none : it->second;
    }

    Props props(const Object& o) const {
        Props p;
        p.own = o.node->child("Properties70");
        if (!p.own) p.own = o.node->child("Properties60");
        const auto it = m_templates.find(o.cls);
        if (it != m_templates.end()) p.tmpl = it->second;
        return p;
    }

    void index_objects() {
        if (const Node* objects = m_root.child("Objects")) {
            m_objects.reserve(objects->kids.size());
            for (const Node& n : objects->kids) {
                if (n.props.empty() || !n.props[0].is_number()) continue;
                Object o;
                o.node = &n;
                o.id = n.props[0].i;
                o.cls = n.name;
                o.name = object_name(n.str(1));
                o.sub = n.str(2);
                if (m_by_id.count(o.id)) continue;
                m_by_id[o.id] = m_objects.size();
                m_objects.push_back(std::move(o));
            }
        }
        if (const Node* conns = m_root.child("Connections")) {
            for (const Node& c : conns->kids) {
                if (c.name != "C" && c.name != "Connect") continue;
                if (c.props.size() < 3) continue;
                const std::string kind = c.str(0);
                if (!c.props[1].is_number() || !c.props[2].is_number()) continue;
                const std::int64_t child = c.props[1].i, parent = c.props[2].i;
                if (child == parent) continue;
                const std::string prop = kind == "OP" || kind == "PP" ? c.str(3) : std::string();
                m_children[parent].push_back({ child, prop });
                m_parents[child].push_back({ parent, prop });
            }
        }
    }

    void index_templates() {
        const Node* defs = m_root.child("Definitions");
        if (!defs) return;
        for (const Node& t : defs->kids) {
            if (t.name != "ObjectType") continue;
            const std::string cls = t.str(0);
            for (const Node& pt : t.kids) {
                if (pt.name != "PropertyTemplate") continue;
                if (const Node* p70 = pt.child("Properties70")) { m_templates.emplace(cls, p70); break; }
            }
        }
    }

    model_math::Mat4 axis_matrix() const {
        Props g;
        if (const Node* gs = m_root.child("GlobalSettings")) g.own = gs->child("Properties70");
        int up = static_cast<int>(g.integer("UpAxis", 1)), front = static_cast<int>(g.integer("FrontAxis", 2)), coord = static_cast<int>(g.integer("CoordAxis", 0));
        double up_sign = g.num("UpAxisSign", 1.0) < 0.0 ? -1.0 : 1.0;
        double front_sign = g.num("FrontAxisSign", 1.0) < 0.0 ? -1.0 : 1.0;
        double coord_sign = g.num("CoordAxisSign", 1.0) < 0.0 ? -1.0 : 1.0;
        const bool ok = up >= 0 && up < 3 && front >= 0 && front < 3 && coord >= 0 && coord < 3 && up != front && up != coord && front != coord;
        if (!ok) { up = 1; front = 2; coord = 0; up_sign = front_sign = coord_sign = 1.0; }
        double unit = g.num("UnitScaleFactor", 1.0);
        if (!(unit > 0.0) || !std::isfinite(unit)) unit = 1.0;
        const double k = unit / 100.0;
        model_math::Mat4 c = model_math::identity();
        for (int col = 0; col < 3; ++col) {
            c[static_cast<std::size_t>(col)] = col == coord ? coord_sign * k : 0.0;
            c[static_cast<std::size_t>(4 + col)] = col == up ? up_sign * k : 0.0;
            c[static_cast<std::size_t>(8 + col)] = col == front ? front_sign * k : 0.0;
        }
        return c;
    }

    std::vector<std::uint8_t> content_of(const Node* video) const {
        std::vector<std::uint8_t> out;
        if (!video) return out;
        const Node* c = video->child("Content");
        if (!c) return out;
        std::string b64;
        for (const Prop& p : c->props) {
            if (p.type == 'R') out.insert(out.end(), p.s.begin(), p.s.end());
            else if (p.type == 'S') b64 += p.s;
        }
        if (out.empty() && !b64.empty()) out = base64(b64);
        return out;
    }

    static std::string child_string(const Node* n, const char* key) {
        if (!n) return {};
        const Node* c = n->child(key);
        return c ? c->str(0) : std::string();
    }

    int load_texture(std::int64_t id) {
        const auto cached = m_texture_of.find(id);
        if (cached != m_texture_of.end()) return cached->second;
        m_texture_of[id] = -1;
        const Object* tex = object(id);
        if (!tex) return -1;
        if (tex->cls == "LayeredTexture") {
            for (const Link& l : children(id)) {
                const Object* o = object(l.id);
                if (o && o->cls == "Texture") { const int r = load_texture(l.id); m_texture_of[id] = r; return r; }
            }
            return -1;
        }
        if (tex->cls != "Texture") return -1;
        const Object* video = nullptr;
        for (const Link& l : children(id)) { const Object* o = object(l.id); if (o && o->cls == "Video") { video = o; break; } }
        if (video) {
            const auto vt = m_video_texture.find(video->id);
            if (vt != m_video_texture.end()) { m_texture_of[id] = vt->second; return vt->second; }
        }
        std::optional<images::BitmapImage> img;
        std::string err;
        const std::vector<std::uint8_t> blob = content_of(video ? video->node : nullptr);
        if (!blob.empty()) img = system::load_image_memory(blob.data(), blob.size(), &err);
        if (!img) {
            std::string rel = child_string(tex->node, "RelativeFilename");
            std::string file = child_string(tex->node, "FileName");
            if (video) {
                if (rel.empty()) rel = child_string(video->node, "RelativeFilename");
                if (file.empty()) file = child_string(video->node, "Filename");
                if (file.empty()) file = props(*video).find("Path") ? props(*video).find("Path")->str(4) : std::string();
            }
            const std::filesystem::path dir = m_path.parent_path();
            std::vector<std::filesystem::path> tries;
            if (!rel.empty()) {
                const std::filesystem::path rp = system::paths::from_utf8(normalize_slashes(rel));
                tries.push_back(rp.is_absolute() ? rp : dir / rp);
            }
            for (const std::string& base : { basename_of(file), basename_of(rel) }) {
                if (base.empty() || base == "." || base == "..") continue;
                const std::filesystem::path b = system::paths::from_utf8(base);
                tries.push_back(dir / b);
                tries.push_back(dir / system::paths::from_utf8(system::paths::to_utf8(m_path.stem()) + ".fbm") / b);
                tries.push_back(dir / "textures" / b);
            }
            if (!file.empty()) {
                const std::filesystem::path fp = system::paths::from_utf8(normalize_slashes(file));
                if (fp.is_absolute()) tries.push_back(fp);
            }
            for (const auto& t : tries) {
                std::error_code ec;
                if (!std::filesystem::is_regular_file(t, ec)) continue;
                img = system::load_image(t, &err);
                if (img) break;
            }
        }
        if (!img) return -1;
        const int index = static_cast<int>(m_model.textures.size());
        m_model.textures.emplace_back(std::move(*img), m_opt.filter, WrapMode::Repeat);
        m_texture_of[id] = index;
        if (video) m_video_texture[video->id] = index;
        return index;
    }

    void build_materials() {
        for (const Object& o : m_objects) {
            if (o.cls != "Material") continue;
            const Props p = props(o);
            ModelMaterial mm;
            mm.name = o.name;
            mm.metallic = 0.0f;
            mm.roughness = 0.8f;
            V3 diffuse = p.has("DiffuseColor") ? p.vec("DiffuseColor", { 1, 1, 1 }) : p.vec("Diffuse", { 1, 1, 1 });
            double metallic = -1.0, roughness = -1.0;
            const Node* own = p.own;
            if (own) {
                for (const Node& prop : own->kids) {
                    if (prop.props.empty()) continue;
                    const std::string name = prop.str(0);
                    const std::size_t bar = name.find_last_of('|');
                    if (bar == std::string::npos) continue;
                    const std::string leaf = name.substr(bar + 1);
                    if (leaf == "base_color" || leaf == "baseColor") { diffuse = { prop.num(4, 1.0), prop.num(5, 1.0), prop.num(6, 1.0) }; }
                    else if (leaf == "metalness" || leaf == "metallic") metallic = prop.num(4, 0.0);
                    else if (leaf == "roughness" || leaf == "specularRoughness") roughness = prop.num(4, 0.5);
                }
            }
            double alpha = 1.0;
            if (p.has("Opacity")) alpha = p.num("Opacity", 1.0);
            else if (p.has("TransparencyFactor")) {
                const V3 tc = p.vec("TransparentColor", { 1, 1, 1 });
                alpha = 1.0 - p.num("TransparencyFactor", 0.0) * (tc.x + tc.y + tc.z) / 3.0;
            }
            alpha = std::max(0.0, std::min(1.0, alpha));
            mm.base_color = Color(to_byte(diffuse.x), to_byte(diffuse.y), to_byte(diffuse.z), to_byte(alpha));
            mm.transparent = alpha < 0.999;
            if (metallic >= 0.0) mm.metallic = static_cast<float>(std::max(0.0, std::min(1.0, metallic)));
            if (roughness >= 0.0) mm.roughness = static_cast<float>(std::max(0.0, std::min(1.0, roughness)));
            else if (p.has("ShininessExponent") || p.has("Shininess")) {
                const double ns = p.has("ShininessExponent") ? p.num("ShininessExponent", 20.0) : p.num("Shininess", 20.0);
                mm.roughness = static_cast<float>(std::sqrt(2.0 / (std::max(0.0, ns) + 2.0)));
            }
            mm.metallic_factor = mm.metallic;
            mm.roughness_factor = mm.roughness;
            const V3 ec = p.has("EmissiveColor") ? p.vec("EmissiveColor", { 0, 0, 0 }) : p.vec("Emissive", { 0, 0, 0 });
            const double ef = p.num("EmissiveFactor", 1.0);
            const V3 e{ std::max(0.0, ec.x * ef), std::max(0.0, ec.y * ef), std::max(0.0, ec.z * ef) };
            const double mx = std::max({ e.x, e.y, e.z });
            if (mx > 0.0) {
                mm.emissive = Color(to_byte(e.x / mx), to_byte(e.y / mx), to_byte(e.z / mx));
                mm.emissive_strength = static_cast<float>(mx);
            }
            for (const Link& l : children(o.id)) {
                const Object* t = object(l.id);
                if (!t || (t->cls != "Texture" && t->cls != "LayeredTexture")) continue;
                const std::string& slot = l.prop;
                const bool base = slot == "DiffuseColor" || slot == "Diffuse" || ends_with(slot, "|base_color_map") || ends_with(slot, "|baseColor") || ends_with(slot, "|TEX_color_map") || ends_with(slot, "|base_color");
                const bool normal = slot == "NormalMap" || slot == "Bump" || ends_with(slot, "|bump_map") || ends_with(slot, "|normal_map") || ends_with(slot, "|TEX_normal_map") || ends_with(slot, "|normalCamera");
                const bool emissive = slot == "EmissiveColor" || slot == "Emissive" || ends_with(slot, "|emission_map") || ends_with(slot, "|emit_color_map") || ends_with(slot, "|TEX_emissive_map") || ends_with(slot, "|emissionColor");
                if (!base && !normal && !emissive) continue;
                const int ti = load_texture(l.id);
                if (ti < 0) continue;
                if (base && mm.texture < 0) {
                    mm.texture = ti;
                    mm.base_color = Color(255, 255, 255, mm.base_color.alpha());
                } else if (normal && mm.normal_texture < 0) {
                    mm.normal_texture = ti;
                    mm.normal_scale = static_cast<float>(p.num("BumpFactor", 1.0));
                } else if (emissive && mm.emissive_texture < 0) {
                    mm.emissive_texture = ti;
                    const double strength = ef > 0.0 ? ef : 1.0;
                    mm.emissive = Color(255, 255, 255);
                    mm.emissive_strength = static_cast<float>(strength);
                    mm.emissive_map_strength = static_cast<float>(strength);
                }
            }
            m_material_of[o.id] = static_cast<int>(m_model.materials.size());
            m_model.materials.push_back(std::move(mm));
        }
    }

    void build_nodes() {
        for (const Object& o : m_objects) {
            if (o.cls != "Model") continue;
            const Props p = props(o);
            Xform x;
            x.t = p.vec("Lcl Translation", {});
            x.r = p.vec("Lcl Rotation", {});
            x.s = p.vec("Lcl Scaling", { 1, 1, 1 });
            x.roff = p.vec("RotationOffset", {});
            x.rp = p.vec("RotationPivot", {});
            x.soff = p.vec("ScalingOffset", {});
            x.sp = p.vec("ScalingPivot", {});
            x.pre = p.vec("PreRotation", {});
            x.post = p.vec("PostRotation", {});
            x.gt = p.vec("GeometricTranslation", {});
            x.gr = p.vec("GeometricRotation", {});
            x.gs = p.vec("GeometricScaling", { 1, 1, 1 });
            const std::int64_t order = p.integer("RotationOrder", 0);
            x.order = order >= 0 && order <= 5 ? static_cast<int>(order) : 0;
            x.pivots = nonzero(x.roff) || nonzero(x.rp) || nonzero(x.soff) || nonzero(x.sp);
            ModelNode n;
            n.name = o.name;
            const Local l = evaluate(x, x.t, x.r, x.s);
            n.translation = { l.t.x, l.t.y, l.t.z };
            n.rotation = l.r;
            n.scale = { l.s.x, l.s.y, l.s.z };
            m_node_of[o.id] = static_cast<int>(m_model.nodes.size());
            m_node_ids.push_back(o.id);
            m_xforms.push_back(x);
            m_model.nodes.push_back(std::move(n));
        }
        const std::size_t count = m_model.nodes.size();
        for (std::size_t i = 0; i < count; ++i) {
            for (const Link& l : parents(m_node_ids[i])) {
                const auto it = m_node_of.find(l.id);
                if (it == m_node_of.end() || it->second == static_cast<int>(i)) continue;
                m_model.nodes[i].parent = it->second;
                break;
            }
        }
        for (std::size_t i = 0; i < count; ++i) {
            int cur = m_model.nodes[i].parent;
            std::size_t steps = 0;
            while (cur >= 0 && steps <= count) { if (cur == static_cast<int>(i)) break; cur = m_model.nodes[static_cast<std::size_t>(cur)].parent; ++steps; }
            if (cur == static_cast<int>(i) || steps > count) m_model.nodes[i].parent = -1;
        }
        for (std::size_t i = 0; i < count; ++i) {
            const int p = m_model.nodes[i].parent;
            if (p >= 0) m_model.nodes[static_cast<std::size_t>(p)].children.push_back(static_cast<int>(i));
            else m_model.roots.push_back(static_cast<int>(i));
        }
        m_rest_global.assign(count, model_math::identity());
        std::vector<int> stack(m_model.roots.rbegin(), m_model.roots.rend());
        while (!stack.empty()) {
            const int i = stack.back();
            stack.pop_back();
            const ModelNode& n = m_model.nodes[static_cast<std::size_t>(i)];
            const model_math::Mat4 local = model_math::trs(n.translation, n.rotation, n.scale);
            m_rest_global[static_cast<std::size_t>(i)] = n.parent >= 0 ? model_math::mul(m_rest_global[static_cast<std::size_t>(n.parent)], local) : local;
            for (int c : n.children) stack.push_back(c);
        }
    }

    bool build_meshes(std::vector<NormalJob>& normal_jobs, std::string& error) {
        for (std::size_t ni = 0; ni < m_model.nodes.size(); ++ni) {
            const std::int64_t model_id = m_node_ids[ni];
            const Object* geom = nullptr;
            std::vector<int> mats;
            for (const Link& l : children(model_id)) {
                const Object* o = object(l.id);
                if (!o) continue;
                if (o->cls == "Geometry" && o->sub == "Mesh" && !geom) geom = o;
                else if (o->cls == "Material") {
                    const auto it = m_material_of.find(o->id);
                    mats.push_back(it == m_material_of.end() ? -1 : it->second);
                }
            }
            if (!geom) continue;
            if (!build_mesh(ni, *geom, mats, normal_jobs, error)) return false;
        }
        return true;
    }

    bool build_mesh(std::size_t ni, const Object& geom, const std::vector<int>& mats, std::vector<NormalJob>& normal_jobs, std::string& error) {
        const Node& g = *geom.node;
        const std::vector<double> pos = doubles(g.child("Vertices"));
        const std::vector<std::int64_t> pvi = ints(g.child("PolygonVertexIndex"));
        const std::size_t ncp = pos.size() / 3;
        if (ncp == 0 || pvi.empty()) return true;
        const Layer normals = read_layer(g, "LayerElementNormal", "Normals", "NormalsIndex", "NormalIndex", 3);
        const Layer uvs = read_layer(g, "LayerElementUV", "UV", "UVIndex", nullptr, 2);
        const Layer colors = read_layer(g, "LayerElementColor", "Colors", "ColorIndex", "ColorsIndex", 4);
        Layer materials;
        if (const Node* e = layer_element(g, "LayerElementMaterial")) {
            const Node* mi = e->child("MappingInformationType");
            materials.mapping = mapping_of(mi ? mi->str(0) : std::string("AllSame"));
            materials.data = doubles(e->child("Materials"));
            materials.count = materials.data.size();
        }

        const Xform& x = m_xforms[ni];
        const model_math::Mat4 gm = model_math::trs({ x.gt.x, x.gt.y, x.gt.z }, euler(x.gr, 0), { x.gs.x, x.gs.y, x.gs.z });
        const model_math::Mat4 gn = [&] {
            model_math::Mat4 inv = model_math::inverse(gm);
            model_math::Mat4 t = model_math::identity();
            for (int r = 0; r < 3; ++r) for (int c = 0; c < 3; ++c) t[static_cast<std::size_t>(r * 4 + c)] = inv[static_cast<std::size_t>(c * 4 + r)];
            return t;
        }();
        const bool mirror = det3(gm) < 0.0;
        const bool want_normals = m_opt.generate_normals;
        const bool has_normals = normals.valid();
        const bool flat = !has_normals && want_normals && !m_opt.smooth_normals;

        struct Building {
            ModelPrimitive                                          prim;
            std::unordered_map<VertexKey, std::uint32_t, VertexKeyHash> dedupe;
            std::vector<std::int64_t>                               cps;
        };
        std::vector<Building> prims;
        std::unordered_map<std::int64_t, std::size_t> slot_prim;

        auto prim_for = [&](std::int64_t slot) -> Building& {
            const auto it = slot_prim.find(slot);
            if (it != slot_prim.end()) return prims[it->second];
            slot_prim[slot] = prims.size();
            prims.emplace_back();
            prims.back().prim.material = slot >= 0 && static_cast<std::size_t>(slot) < mats.size() ? mats[static_cast<std::size_t>(slot)] : (slot < 0 && !mats.empty() ? mats[0] : -1);
            return prims.back();
        };

        std::vector<std::uint32_t> face;
        std::size_t poly = 0, start = 0;
        while (start < pvi.size()) {
            std::size_t end = start;
            while (end < pvi.size() && pvi[end] >= 0) ++end;
            const std::size_t last = std::min(end, pvi.size() - 1);
            const std::size_t corners = last - start + 1;
            bool ok = corners >= 3;
            for (std::size_t c = start; ok && c <= last; ++c) {
                const std::int64_t raw = pvi[c];
                const std::int64_t cp = raw < 0 ? -(raw + 1) : raw;
                if (cp < 0 || static_cast<std::size_t>(cp) >= ncp) ok = false;
            }
            if (ok) {
                std::int64_t slot = -1;
                if (materials.mapping >= 0 && materials.count > 0) {
                    const std::int64_t first_cp = pvi[start] < 0 ? -(pvi[start] + 1) : pvi[start];
                    const std::int64_t k = materials.key(start, first_cp, poly);
                    const std::size_t at = k >= 0 && static_cast<std::size_t>(k) < materials.count ? static_cast<std::size_t>(k) : 0;
                    slot = fbx::to_int(materials.data[at]);
                }
                Building& b = prim_for(slot);
                const Color base = b.prim.material >= 0 && static_cast<std::size_t>(b.prim.material) < m_model.materials.size() ? m_model.materials[static_cast<std::size_t>(b.prim.material)].base_color : Color(255, 255, 255, 255);
                face.clear();
                for (std::size_t c = start; c <= last; ++c) {
                    const std::int64_t raw = pvi[c];
                    const std::int64_t cp = raw < 0 ? -(raw + 1) : raw;
                    const std::int64_t ni_ = normals.resolve(c, cp, poly);
                    const std::int64_t ui = uvs.resolve(c, cp, poly);
                    const std::int64_t ci = colors.resolve(c, cp, poly);
                    const VertexKey key{ cp, flat ? static_cast<std::int64_t>(c) : ni_, ui, ci };
                    const auto found = b.dedupe.find(key);
                    if (found != b.dedupe.end()) { face.push_back(found->second); continue; }
                    Vertex3D v;
                    const std::size_t pi = static_cast<std::size_t>(cp) * 3;
                    const linalg::vector3d p = model_math::point(gm, finite_or(pos[pi], 0.0), finite_or(pos[pi + 1], 0.0), finite_or(pos[pi + 2], 0.0));
                    v.x = static_cast<float>(p.x); v.y = static_cast<float>(p.y); v.z = static_cast<float>(p.z);
                    if (ni_ >= 0) {
                        const std::size_t q = static_cast<std::size_t>(ni_) * 3;
                        const linalg::vector3d n = model_math::direction(gn, finite_or(normals.data[q], 0.0), finite_or(normals.data[q + 1], 0.0), finite_or(normals.data[q + 2], 0.0));
                        const double l = std::sqrt(n.x * n.x + n.y * n.y + n.z * n.z);
                        if (l > 0.0) { v.nx = static_cast<float>(n.x / l); v.ny = static_cast<float>(n.y / l); v.nz = static_cast<float>(n.z / l); }
                    }
                    if (ui >= 0) {
                        const std::size_t q = static_cast<std::size_t>(ui) * 2;
                        const double tv = finite_or(uvs.data[q + 1], 0.0);
                        v.u = static_cast<float>(finite_or(uvs.data[q], 0.0));
                        v.v = static_cast<float>(m_opt.flip_v ? tv : 1.0 - tv);
                    }
                    double r = base.red() / 255.0, gg = base.green() / 255.0, bb = base.blue() / 255.0, a = base.alpha() / 255.0;
                    if (ci >= 0) {
                        const std::size_t q = static_cast<std::size_t>(ci) * 4;
                        r *= finite_or(colors.data[q], 1.0); gg *= finite_or(colors.data[q + 1], 1.0); bb *= finite_or(colors.data[q + 2], 1.0); a *= finite_or(colors.data[q + 3], 1.0);
                    }
                    v.rgba = Vertex3D::pack(Color(to_byte(r), to_byte(gg), to_byte(bb), to_byte(a)));
                    const std::uint32_t id = static_cast<std::uint32_t>(b.prim.mesh.add_vertex(v));
                    b.dedupe.emplace(key, id);
                    b.cps.push_back(cp);
                    face.push_back(id);
                }
                for (std::size_t k = 2; k < face.size(); ++k) {
                    if (mirror) b.prim.mesh.add_triangle(face[0], face[k], face[k - 1]);
                    else b.prim.mesh.add_triangle(face[0], face[k - 1], face[k]);
                }
            }
            ++poly;
            start = end + 1;
        }

        int skin = -1;
        std::vector<Influence> influence;
        build_skin(ni, geom, ncp, influence, skin);

        ModelMeshData md;
        md.name = geom.name.empty() ? m_model.nodes[ni].name : geom.name;
        for (Building& b : prims) {
            if (b.prim.mesh.indices().empty()) continue;
            if (skin >= 0) {
                const std::size_t n = b.prim.mesh.vertices().size();
                b.prim.joints.resize(n);
                b.prim.weights.resize(n);
                const int rigid = static_cast<int>(m_model.skins[static_cast<std::size_t>(skin)].joints.size()) - 1;
                for (std::size_t i = 0; i < n; ++i) {
                    const Influence& inf = influence[static_cast<std::size_t>(b.cps[i])];
                    double sum = 0.0;
                    for (int k = 0; k < 4; ++k) if (inf.joint[static_cast<std::size_t>(k)] >= 0) sum += inf.weight[static_cast<std::size_t>(k)];
                    for (int k = 0; k < 4; ++k) {
                        const bool used = inf.joint[static_cast<std::size_t>(k)] >= 0 && sum > 0.0;
                        b.prim.joints[i][static_cast<std::size_t>(k)] = static_cast<std::uint16_t>(used ? inf.joint[static_cast<std::size_t>(k)] : 0);
                        b.prim.weights[i][static_cast<std::size_t>(k)] = used ? static_cast<float>(inf.weight[static_cast<std::size_t>(k)] / sum) : 0.0f;
                    }
                    if (!(sum > 0.0)) { b.prim.joints[i][0] = static_cast<std::uint16_t>(std::max(0, rigid)); b.prim.weights[i][0] = 1.0f; }
                }
            }
            if (!has_normals && want_normals) normal_jobs.push_back({ m_model.meshes.size(), md.primitives.size() });
            md.primitives.push_back(std::move(b.prim));
        }
        if (md.primitives.empty()) return true;
        if (m_model.meshes.size() >= static_cast<std::size_t>(std::numeric_limits<int>::max())) { error = "too many meshes"; return false; }
        m_model.nodes[ni].mesh = static_cast<int>(m_model.meshes.size());
        m_model.nodes[ni].skin = skin;
        m_model.meshes.push_back(std::move(md));
        return true;
    }

    void build_skin(std::size_t ni, const Object& geom, std::size_t ncp, std::vector<Influence>& influence, int& skin_index) {
        skin_index = -1;
        const Object* skin = nullptr;
        for (const Link& l : children(geom.id)) {
            const Object* o = object(l.id);
            if (o && o->cls == "Deformer" && o->sub == "Skin") { skin = o; break; }
        }
        if (!skin) return;
        ModelSkin s;
        s.name = skin->name;
        std::unordered_map<int, int> joint_of;
        influence.assign(ncp, Influence{});
        for (const Link& l : children(skin->id)) {
            const Object* cluster = object(l.id);
            if (!cluster || cluster->cls != "Deformer" || cluster->sub != "Cluster") continue;
            int bone = -1;
            for (const Link& bl : children(cluster->id)) {
                const auto it = m_node_of.find(bl.id);
                if (it != m_node_of.end()) { bone = it->second; break; }
            }
            if (bone < 0) continue;
            int joint = -1;
            const auto found = joint_of.find(bone);
            if (found != joint_of.end()) joint = found->second;
            else {
                if (s.joints.size() >= max_joints - 1) continue;
                joint = static_cast<int>(s.joints.size());
                joint_of[bone] = joint;
                const Node* tn = cluster->node->child("Transform");
                const Node* ln = cluster->node->child("TransformLink");
                const model_math::Mat4 t = tn ? matrix16(doubles(tn)) : model_math::identity();
                const model_math::Mat4 link = ln ? matrix16(doubles(ln)) : m_rest_global[static_cast<std::size_t>(bone)];
                s.joints.push_back(bone);
                s.inverse_bind.push_back(model_math::mul(model_math::inverse(link), t));
            }
            const std::vector<std::int64_t> idx = ints(cluster->node->child("Indexes"));
            const std::vector<double> w = doubles(cluster->node->child("Weights"));
            const std::size_t n = std::min(idx.size(), w.size());
            for (std::size_t k = 0; k < n; ++k) {
                if (idx[k] < 0 || static_cast<std::size_t>(idx[k]) >= ncp) continue;
                influence[static_cast<std::size_t>(idx[k])].add(joint, static_cast<float>(w[k]));
            }
        }
        if (s.joints.empty()) { influence.clear(); return; }
        bool unweighted = false;
        for (const Influence& inf : influence) {
            float sum = 0.0f;
            for (int k = 0; k < 4; ++k) if (inf.joint[static_cast<std::size_t>(k)] >= 0) sum += inf.weight[static_cast<std::size_t>(k)];
            if (!(sum > 0.0f)) { unweighted = true; break; }
        }
        if (unweighted) {
            s.joints.push_back(static_cast<int>(ni));
            s.inverse_bind.push_back(model_math::identity());
        }
        skin_index = static_cast<int>(m_model.skins.size());
        m_model.skins.push_back(std::move(s));
    }

    static Curve read_curve(const Node& n) {
        Curve c;
        const std::vector<std::int64_t> t = ints(n.child("KeyTime"));
        std::vector<double> v = doubles(n.child("KeyValueFloat"));
        if (v.empty()) v = doubles(n.child("KeyValueDouble"));
        const std::size_t count = std::min(t.size(), v.size());
        if (count == 0) return c;
        const std::vector<std::int64_t> flags = ints(n.child("KeyAttrFlags"));
        const std::vector<std::int64_t> refs = ints(n.child("KeyAttrRefCount"));
        std::vector<std::int64_t> per_key;
        if (!flags.empty() && refs.size() == flags.size()) {
            for (std::size_t g = 0; g < flags.size() && per_key.size() < count; ++g)
                for (std::int64_t r = 0; r < refs[g] && per_key.size() < count; ++r) per_key.push_back(flags[g]);
        } else if (flags.size() == count) per_key = flags;
        else if (flags.size() == 1) per_key.assign(count, flags[0]);
        per_key.resize(count, 0);
        std::vector<std::size_t> order(count);
        for (std::size_t i = 0; i < count; ++i) order[i] = i;
        std::stable_sort(order.begin(), order.end(), [&](std::size_t a, std::size_t b) { return t[a] < t[b]; });
        for (std::size_t i : order) {
            if (!std::isfinite(v[i])) continue;
            if (!c.times.empty() && c.times.back() == t[i]) { c.values.back() = v[i]; continue; }
            c.times.push_back(t[i]);
            c.values.push_back(v[i]);
            const std::int64_t f = per_key[i];
            c.constant.push_back((f & 0x02) ? ((f & 0x100) ? 2 : 1) : 0);
        }
        return c;
    }

    void build_animations() {
        std::unordered_map<std::int64_t, Curve> curves;
        for (const Object& stack : m_objects) {
            if (stack.cls != "AnimationStack") continue;
            const Props sp = props(stack);
            ModelAnimation anim;
            anim.name = stack.name;
            const std::int64_t start = sp.has("LocalStart") ? sp.integer("LocalStart", 0) : sp.integer("ReferenceStart", 0);
            const std::int64_t stop = sp.has("LocalStop") ? sp.integer("LocalStop", 0) : sp.integer("ReferenceStop", 0);

            struct Track {
                int          node = -1;
                const Curve* curve[3][3] = { { nullptr, nullptr, nullptr }, { nullptr, nullptr, nullptr }, { nullptr, nullptr, nullptr } };
                V3           def[3];
                bool         has[3] = { false, false, false };
            };
            std::vector<Track> tracks;
            std::unordered_map<int, std::size_t> track_of;

            for (const Link& ll : children(stack.id)) {
                const Object* layer = object(ll.id);
                if (!layer || layer->cls != "AnimationLayer") continue;
                for (const Link& cl : children(layer->id)) {
                    const Object* cn = object(cl.id);
                    if (!cn || cn->cls != "AnimationCurveNode") continue;
                    for (const Link& pl : parents(cn->id)) {
                        const int path = pl.prop == "Lcl Translation" ? 0 : pl.prop == "Lcl Rotation" ? 1 : pl.prop == "Lcl Scaling" ? 2 : -1;
                        if (path < 0) continue;
                        const auto nit = m_node_of.find(pl.id);
                        if (nit == m_node_of.end()) continue;
                        const int node = nit->second;
                        auto tit = track_of.find(node);
                        if (tit == track_of.end()) {
                            tit = track_of.emplace(node, tracks.size()).first;
                            tracks.emplace_back();
                            tracks.back().node = node;
                            const Xform& x = m_xforms[static_cast<std::size_t>(node)];
                            tracks.back().def[0] = x.t; tracks.back().def[1] = x.r; tracks.back().def[2] = x.s;
                        }
                        Track& tr = tracks[tit->second];
                        const Props cp = props(*cn);
                        V3& d = tr.def[path];
                        d = { cp.num("d|X", d.x), cp.num("d|Y", d.y), cp.num("d|Z", d.z) };
                        for (const Link& kl : children(cn->id)) {
                            const Object* co = object(kl.id);
                            if (!co || co->cls != "AnimationCurve") continue;
                            const int axis = kl.prop == "d|X" ? 0 : kl.prop == "d|Y" ? 1 : kl.prop == "d|Z" ? 2 : -1;
                            if (axis < 0) continue;
                            auto cit = curves.find(co->id);
                            if (cit == curves.end()) cit = curves.emplace(co->id, read_curve(*co->node)).first;
                            if (cit->second.times.empty()) continue;
                            tr.curve[path][axis] = &cit->second;
                            tr.has[path] = true;
                        }
                    }
                }
            }

            const std::int64_t step = static_cast<std::int64_t>(ticks_per_second / 30.0);
            float duration = stop > start ? static_cast<float>(static_cast<double>(stop - start) / ticks_per_second) : 0.0f;
            for (Track& tr : tracks) {
                if (!tr.has[0] && !tr.has[1] && !tr.has[2]) continue;
                std::vector<std::int64_t> keys;
                for (int p = 0; p < 3; ++p) for (int a = 0; a < 3; ++a) if (tr.curve[p][a]) {
                    const auto& ts = tr.curve[p][a]->times;
                    if (keys.size() + ts.size() > max_samples * 4) continue;
                    keys.insert(keys.end(), ts.begin(), ts.end());
                }
                std::sort(keys.begin(), keys.end());
                keys.erase(std::unique(keys.begin(), keys.end()), keys.end());
                if (keys.empty()) continue;
                bool before = false;
                std::vector<std::int64_t> clipped;
                clipped.reserve(keys.size());
                for (std::int64_t k : keys) { if (k < start) before = true; else clipped.push_back(k); }
                if (before && (clipped.empty() || clipped.front() != start)) clipped.insert(clipped.begin(), start);
                keys.swap(clipped);
                std::vector<std::int64_t> samples;
                if (keys.size() > max_samples) {
                    const std::int64_t first = keys.front(), last = keys.back();
                    const double span = static_cast<double>(last - first);
                    const std::size_t n = std::min<std::size_t>(max_samples, static_cast<std::size_t>(std::max(2.0, span / static_cast<double>(step) + 1.0)));
                    for (std::size_t i = 0; i < n; ++i) samples.push_back(first + static_cast<std::int64_t>(span * static_cast<double>(i) / static_cast<double>(n - 1)));
                } else if (tr.has[1] && step > 0) {
                    for (std::size_t i = 0; i < keys.size(); ++i) {
                        samples.push_back(keys[i]);
                        if (i + 1 >= keys.size()) break;
                        const std::int64_t a = keys[i], b = keys[i + 1];
                        if (b - a <= step) continue;
                        const std::int64_t base = start;
                        std::int64_t m = (a - base) / step + 1;
                        while (base + m * step < b && samples.size() < max_samples) { if (base + m * step > a) samples.push_back(base + m * step); ++m; }
                        if (samples.size() >= max_samples) { samples.push_back(keys.back()); break; }
                    }
                } else {
                    samples = keys;
                }
                samples.erase(std::unique(samples.begin(), samples.end()), samples.end());

                const Xform& x = m_xforms[static_cast<std::size_t>(tr.node)];
                const bool emit_t = tr.has[0] || ((tr.has[1] || tr.has[2]) && x.pivots);
                AnimChannel ct, cr, cs;
                ct.node = cr.node = cs.node = tr.node;
                ct.path = AnimPath::Translation; cr.path = AnimPath::Rotation; cs.path = AnimPath::Scale;
                Quat prev{};
                bool first = true;
                for (std::int64_t t : samples) {
                    V3 v[3];
                    for (int p = 0; p < 3; ++p) {
                        const V3& d = tr.def[p];
                        v[p] = { tr.curve[p][0] ? tr.curve[p][0]->eval(t, d.x) : d.x, tr.curve[p][1] ? tr.curve[p][1]->eval(t, d.y) : d.y, tr.curve[p][2] ? tr.curve[p][2]->eval(t, d.z) : d.z };
                    }
                    const Local l = evaluate(x, v[0], v[1], v[2]);
                    Quat q = l.r;
                    if (!first && q.x * prev.x + q.y * prev.y + q.z * prev.z + q.w * prev.w < 0.0) q = { -q.x, -q.y, -q.z, -q.w };
                    prev = q;
                    first = false;
                    const float sec = static_cast<float>(static_cast<double>(t - start) / ticks_per_second);
                    if (emit_t) { ct.times.push_back(sec); ct.values.insert(ct.values.end(), { static_cast<float>(l.t.x), static_cast<float>(l.t.y), static_cast<float>(l.t.z) }); }
                    if (tr.has[1]) { cr.times.push_back(sec); cr.values.insert(cr.values.end(), { static_cast<float>(q.x), static_cast<float>(q.y), static_cast<float>(q.z), static_cast<float>(q.w) }); }
                    if (tr.has[2]) { cs.times.push_back(sec); cs.values.insert(cs.values.end(), { static_cast<float>(l.s.x), static_cast<float>(l.s.y), static_cast<float>(l.s.z) }); }
                    duration = std::max(duration, sec);
                }
                if (!ct.times.empty()) anim.channels.push_back(std::move(ct));
                if (!cr.times.empty()) anim.channels.push_back(std::move(cr));
                if (!cs.times.empty()) anim.channels.push_back(std::move(cs));
            }
            anim.duration = duration;
            m_model.animations.push_back(std::move(anim));
        }
    }
};

} // namespace fbx
} // namespace

bool Model3D::load_fbx(const std::filesystem::path& path, Model3D& out, std::string* error, const ModelImportOptions& options) {
    auto fail = [&](const std::string& why) { if (error) *error = why; return false; };
    const auto bytes = system::paths::read_bytes(path);
    if (!bytes) return fail("cannot read " + system::paths::to_utf8(path));
    if (bytes->empty()) return fail("empty FBX file");
    fbx::Node root;
    std::uint32_t version = 0;
    std::string err;
    if (!fbx::parse(bytes->data(), bytes->size(), root, version, err)) return fail(err);
    Model3D m;
    make_import(m, options);
    std::vector<fbx::NormalJob> normal_jobs;
    model_math::Mat4 axes = model_math::identity();
    if (!fbx::Builder(root, path, options, m).build(normal_jobs, axes, err)) return fail(err);
    for (const fbx::NormalJob& j : normal_jobs) generate_normals(m.meshes[j.mesh].primitives[j.primitive].mesh, true);
    if (fbx::det3(axes) < 0.0) {
        for (auto& md : m.meshes) for (auto& p : md.primitives) {
            auto& idx = p.mesh.edit_indices();
            for (std::size_t i = 0; i + 2 < idx.size(); i += 3) std::swap(idx[i + 1], idx[i + 2]);
        }
    }
    m.import_transform = model_math::mul(m.import_transform, axes);
    m.source = system::paths::to_utf8(path);
    out = std::move(m);
    return true;
}

} // namespace graphics
} // namespace fizmo
