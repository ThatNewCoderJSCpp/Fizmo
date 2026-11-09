#ifndef FIZMO
#define FIZMO
#endif
#ifndef FIZMO_GRAPHICS
#define FIZMO_GRAPHICS
#endif
#include <fizmo/includes.hpp>
#include "../Source/Graphics/model_3d.hpp"
#include "../Source/Images/Formats/format_util.hpp"
#include "../Source/Images/Normal/png_codec.hpp"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <random>
#include <string>
#include <vector>

namespace gfx = fizmo::graphics;
namespace mm = fizmo::graphics::model_math;
namespace fs = std::filesystem;
using fizmo::colors::Color;
using fizmo::images::BitmapImage;

namespace {

int g_passed = 0;
int g_failed = 0;

void check(bool ok, const std::string& what) {
    if (ok) {
        ++g_passed;
    } else {
        ++g_failed;
        std::printf("FAIL: %s\n", what.c_str());
    }
}

bool near(double a, double b, double eps = 1e-4) { return std::abs(a - b) <= eps; }

struct Val {
    char                      type = 0;
    std::int64_t              i = 0;
    double                    d = 0.0;
    std::string               s;
    std::vector<double>       f;
    std::vector<std::int64_t> n;
};

Val S(const std::string& s) { Val v; v.type = 'S'; v.s = s; return v; }
Val R(const std::vector<std::uint8_t>& b) { Val v; v.type = 'R'; v.s.assign(b.begin(), b.end()); return v; }
Val I(std::int64_t i) { Val v; v.type = 'I'; v.i = i; return v; }
Val L(std::int64_t i) { Val v; v.type = 'L'; v.i = i; return v; }
Val D(double d) { Val v; v.type = 'D'; v.d = d; return v; }
Val Da(std::vector<double> f) { Val v; v.type = 'd'; v.f = std::move(f); return v; }
Val Fa(std::vector<double> f) { Val v; v.type = 'f'; v.f = std::move(f); return v; }
Val Ia(std::vector<std::int64_t> n) { Val v; v.type = 'i'; v.n = std::move(n); return v; }
Val La(std::vector<std::int64_t> n) { Val v; v.type = 'l'; v.n = std::move(n); return v; }

struct W {
    std::string      name;
    std::vector<Val> props;
    std::vector<W>   kids;
};

W node(const std::string& name, std::vector<Val> props = {}, std::vector<W> kids = {}) { return W{ name, std::move(props), std::move(kids) }; }

W P(const std::string& name, const std::string& type, const std::string& type2, const std::string& flags, std::vector<Val> values) {
    std::vector<Val> props{ S(name), S(type), S(type2), S(flags) };
    for (auto& v : values) props.push_back(std::move(v));
    return node("P", std::move(props));
}

W P3(const std::string& name, double x, double y, double z) { return P(name, name, "", "A", { D(x), D(y), D(z) }); }

class BinaryWriter {
public:
    BinaryWriter(std::uint32_t version, bool compress) : m_wide(version >= 7500), m_compress(compress) {
        const char magic[] = "Kaydara FBX Binary  ";
        m_out.insert(m_out.end(), magic, magic + 20);
        m_out.push_back(0);
        m_out.push_back(0x1A);
        m_out.push_back(0);
        put32(version);
    }

    std::vector<std::uint8_t> finish(const std::vector<W>& top) {
        for (const W& n : top) write(n);
        null_record();
        return m_out;
    }

private:
    std::vector<std::uint8_t> m_out;
    bool                      m_wide;
    bool                      m_compress;

    void put32(std::uint32_t v) { for (int k = 0; k < 4; ++k) m_out.push_back(static_cast<std::uint8_t>(v >> (8 * k))); }
    void put64(std::uint64_t v) { for (int k = 0; k < 8; ++k) m_out.push_back(static_cast<std::uint8_t>(v >> (8 * k))); }
    void put_word(std::uint64_t v) { if (m_wide) put64(v); else put32(static_cast<std::uint32_t>(v)); }
    void patch_word(std::size_t at, std::uint64_t v) {
        const int n = m_wide ? 8 : 4;
        for (int k = 0; k < n; ++k) m_out[at + static_cast<std::size_t>(k)] = static_cast<std::uint8_t>(v >> (8 * k));
    }
    template <class T> void put_raw(const T& v) { const auto* p = reinterpret_cast<const std::uint8_t*>(&v); m_out.insert(m_out.end(), p, p + sizeof(T)); }
    void null_record() { m_out.insert(m_out.end(), m_wide ? 25 : 13, 0); }

    void write_prop(const Val& v) {
        m_out.push_back(static_cast<std::uint8_t>(v.type));
        switch (v.type) {
            case 'I': put_raw(static_cast<std::int32_t>(v.i)); break;
            case 'L': put_raw(static_cast<std::int64_t>(v.i)); break;
            case 'D': put_raw(v.d); break;
            case 'S': case 'R': put32(static_cast<std::uint32_t>(v.s.size())); m_out.insert(m_out.end(), v.s.begin(), v.s.end()); break;
            default: {
                std::vector<std::uint8_t> raw;
                const std::size_t count = v.type == 'd' || v.type == 'f' ? v.f.size() : v.n.size();
                for (std::size_t k = 0; k < count; ++k) {
                    const std::uint8_t* p = nullptr;
                    std::size_t sz = 0;
                    double d; float f; std::int32_t i; std::int64_t l;
                    if (v.type == 'd') { d = v.f[k]; p = reinterpret_cast<const std::uint8_t*>(&d); sz = 8; }
                    else if (v.type == 'f') { f = static_cast<float>(v.f[k]); p = reinterpret_cast<const std::uint8_t*>(&f); sz = 4; }
                    else if (v.type == 'i') { i = static_cast<std::int32_t>(v.n[k]); p = reinterpret_cast<const std::uint8_t*>(&i); sz = 4; }
                    else { l = v.n[k]; p = reinterpret_cast<const std::uint8_t*>(&l); sz = 8; }
                    raw.insert(raw.end(), p, p + sz);
                }
                put32(static_cast<std::uint32_t>(count));
                if (m_compress) {
                    std::vector<std::uint8_t> z;
                    fizmo::images::fmt::zlib_deflate(raw.data(), raw.size(), z, 6);
                    put32(1);
                    put32(static_cast<std::uint32_t>(z.size()));
                    m_out.insert(m_out.end(), z.begin(), z.end());
                } else {
                    put32(0);
                    put32(static_cast<std::uint32_t>(raw.size()));
                    m_out.insert(m_out.end(), raw.begin(), raw.end());
                }
            }
        }
    }

    void write(const W& n) {
        const std::size_t at = m_out.size();
        put_word(0);
        put_word(n.props.size());
        const std::size_t plen_at = m_out.size();
        put_word(0);
        m_out.push_back(static_cast<std::uint8_t>(n.name.size()));
        m_out.insert(m_out.end(), n.name.begin(), n.name.end());
        const std::size_t pstart = m_out.size();
        for (const Val& v : n.props) write_prop(v);
        patch_word(plen_at, m_out.size() - pstart);
        for (const W& k : n.kids) write(k);
        if (!n.kids.empty() || n.props.empty()) null_record();
        patch_word(at, m_out.size());
    }
};

std::string b64(const std::string& in) {
    static const char* t = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;
    std::size_t i = 0;
    for (; i + 2 < in.size(); i += 3) {
        const std::uint32_t v = (std::uint8_t(in[i]) << 16) | (std::uint8_t(in[i + 1]) << 8) | std::uint8_t(in[i + 2]);
        out += t[(v >> 18) & 63]; out += t[(v >> 12) & 63]; out += t[(v >> 6) & 63]; out += t[v & 63];
    }
    if (i + 1 == in.size()) { const std::uint32_t v = std::uint8_t(in[i]) << 16; out += t[(v >> 18) & 63]; out += t[(v >> 12) & 63]; out += "=="; }
    else if (i + 2 == in.size()) { const std::uint32_t v = (std::uint8_t(in[i]) << 16) | (std::uint8_t(in[i + 1]) << 8); out += t[(v >> 18) & 63]; out += t[(v >> 12) & 63]; out += t[(v >> 6) & 63]; out += '='; }
    return out;
}

std::string ascii_value(const Val& v) {
    char buf[64];
    switch (v.type) {
        case 'I': case 'L': return std::to_string(v.i);
        case 'D': std::snprintf(buf, sizeof(buf), "%.17g", v.d); return buf;
        case 'S': return "\"" + v.s + "\"";
        case 'R': {
            const std::string e = b64(v.s);
            const std::size_t half = (e.size() / 2) & ~std::size_t(3);
            return ",\"" + e.substr(0, half) + "\",\"" + e.substr(half) + "\"";
        }
        default: return {};
    }
}

void write_ascii(std::string& out, const W& n, int depth) {
    const std::string pad(static_cast<std::size_t>(depth) * 4, ' ');
    out += pad + n.name + ": ";
    if (n.props.size() == 1 && (n.props[0].type == 'd' || n.props[0].type == 'f' || n.props[0].type == 'i' || n.props[0].type == 'l')) {
        const Val& v = n.props[0];
        const std::size_t count = v.type == 'd' || v.type == 'f' ? v.f.size() : v.n.size();
        out += "*" + std::to_string(count) + " {\n" + pad + "    a: ";
        for (std::size_t k = 0; k < count; ++k) {
            if (k) out += (k % 8 == 0) ? ",\n" + pad + "       " : ",";
            if (v.type == 'd' || v.type == 'f') { char buf[64]; std::snprintf(buf, sizeof(buf), "%.17g", v.f[k]); out += buf; }
            else out += std::to_string(v.n[k]);
        }
        out += "\n" + pad + "} \n";
        return;
    }
    for (std::size_t k = 0; k < n.props.size(); ++k) { if (k) out += ", "; out += ascii_value(n.props[k]); }
    if (!n.kids.empty() || n.props.empty()) {
        out += " {\n";
        for (const W& k : n.kids) write_ascii(out, k, depth + 1);
        out += pad + "}\n";
    } else {
        out += "\n";
    }
}

std::string ascii_file(const std::vector<W>& top) {
    std::string out = "; FBX 7.4.0 project file\n; test writer\n; ----------------------------------------------------\n\n";
    for (const W& n : top) write_ascii(out, n, 0);
    return out;
}

constexpr std::int64_t kSecond = 46186158000LL;

std::vector<std::uint8_t> make_png() {
    BitmapImage img(2, 2);
    img.set_pixel(0, 0, Color(255, 0, 0, 255));
    img.set_pixel(1, 0, Color(0, 255, 0, 255));
    img.set_pixel(0, 1, Color(0, 0, 255, 255));
    img.set_pixel(1, 1, Color(255, 255, 255, 255));
    std::vector<std::uint8_t> png;
    fizmo::images::encode_png(img, png);
    return png;
}

std::vector<double> column_major(const mm::Mat4& m) {
    std::vector<double> out(16);
    for (int c = 0; c < 4; ++c) for (int r = 0; r < 4; ++r) out[static_cast<std::size_t>(c * 4 + r)] = m[static_cast<std::size_t>(r * 4 + c)];
    return out;
}

mm::Quat axis_quat(double ax, double ay, double az, double deg) {
    const double h = deg * 3.14159265358979323846 / 360.0;
    return { ax * std::sin(h), ay * std::sin(h), az * std::sin(h), std::cos(h) };
}

mm::Quat qmul(const mm::Quat& a, const mm::Quat& b) {
    return { a.w * b.x + a.x * b.w + a.y * b.z - a.z * b.y, a.w * b.y - a.x * b.z + a.y * b.w + a.z * b.x, a.w * b.z + a.x * b.y - a.y * b.x + a.z * b.w, a.w * b.w - a.x * b.x - a.y * b.y - a.z * b.z };
}

mm::Mat4 bone2_link() { return mm::trs({ 0, 0, 100 }, axis_quat(0, 0, 1, 30), { 1, 1, 1 }); }

std::vector<W> make_scene(bool ascii, bool embed) {
    auto name = [&](const std::string& n, const std::string& cls) { return ascii ? cls + "::" + n : n + std::string("\x00\x01", 2) + cls; };

    std::vector<double> verts;
    for (int r = 0; r < 3; ++r) {
        const double z = r * 100.0;
        const double ring[4][2] = { { -10, -10 }, { 10, -10 }, { 10, 10 }, { -10, 10 } };
        for (auto& c : ring) { verts.push_back(c[0]); verts.push_back(c[1]); verts.push_back(z); }
    }
    std::vector<std::vector<int>> polys;
    for (int r = 0; r < 2; ++r) for (int k = 0; k < 4; ++k) { const int k1 = (k + 1) % 4; polys.push_back({ r * 4 + k, r * 4 + k1, (r + 1) * 4 + k1, (r + 1) * 4 + k }); }
    polys.push_back({ 3, 2, 1, 0 });
    polys.push_back({ 8, 9, 10, 11 });
    std::vector<std::int64_t> pvi;
    std::vector<double> normals;
    std::vector<std::int64_t> uv_index;
    for (const auto& p : polys) {
        const double* a = &verts[static_cast<std::size_t>(p[0]) * 3];
        const double* b = &verts[static_cast<std::size_t>(p[1]) * 3];
        const double* c = &verts[static_cast<std::size_t>(p[2]) * 3];
        const double e1[3] = { b[0] - a[0], b[1] - a[1], b[2] - a[2] }, e2[3] = { c[0] - a[0], c[1] - a[1], c[2] - a[2] };
        double n[3] = { e1[1] * e2[2] - e1[2] * e2[1], e1[2] * e2[0] - e1[0] * e2[2], e1[0] * e2[1] - e1[1] * e2[0] };
        const double l = std::sqrt(n[0] * n[0] + n[1] * n[1] + n[2] * n[2]);
        for (std::size_t k = 0; k < p.size(); ++k) {
            pvi.push_back(k + 1 == p.size() ? -p[k] - 1 : p[k]);
            normals.insert(normals.end(), { n[0] / l, n[1] / l, n[2] / l });
            uv_index.push_back(p[k]);
        }
    }
    std::vector<double> uvs;
    for (int cp = 0; cp < 12; ++cp) { uvs.push_back((cp % 4) / 4.0); uvs.push_back((cp / 4) / 2.0); }

    W geom = node("Geometry", { L(100), S(name("Body", "Geometry")), S("Mesh") }, {
        node("Properties70"),
        node("GeometryVersion", { I(124) }),
        node("Vertices", { Da(verts) }),
        node("PolygonVertexIndex", { Ia(pvi) }),
    });
    if (!ascii) {
        geom.kids.push_back(node("LayerElementNormal", { I(0) }, {
            node("Version", { I(101) }), node("Name", { S("") }),
            node("MappingInformationType", { S("ByPolygonVertex") }), node("ReferenceInformationType", { S("Direct") }),
            node("Normals", { Da(normals) }) }));
        geom.kids.push_back(node("LayerElementUV", { I(0) }, {
            node("Version", { I(101) }), node("Name", { S("UVMap") }),
            node("MappingInformationType", { S("ByPolygonVertex") }), node("ReferenceInformationType", { S("IndexToDirect") }),
            node("UV", { Da(uvs) }), node("UVIndex", { Ia(uv_index) }) }));
        geom.kids.push_back(node("LayerElementMaterial", { I(0) }, {
            node("Version", { I(101) }), node("Name", { S("") }),
            node("MappingInformationType", { S("AllSame") }), node("ReferenceInformationType", { S("IndexToDirect") }),
            node("Materials", { Ia({ 0 }) }) }));
    } else {
        geom.kids.push_back(node("LayerElementUV", { I(0) }, {
            node("Version", { I(101) }), node("Name", { S("UVMap") }),
            node("MappingInformationType", { S("ByVertice") }), node("ReferenceInformationType", { S("Direct") }),
            node("UV", { Da(uvs) }) }));
        geom.kids.push_back(node("LayerElementMaterial", { I(0) }, {
            node("Version", { I(101) }), node("Name", { S("") }),
            node("MappingInformationType", { S("ByPolygon") }), node("ReferenceInformationType", { S("IndexToDirect") }),
            node("Materials", { Ia(std::vector<std::int64_t>(polys.size(), 0)) }) }));
    }
    geom.kids.push_back(node("Layer", { I(0) }, { node("Version", { I(100) }) }));

    const double ring_w = ascii ? 0.3 : 0.5;
    std::vector<std::int64_t> idx1{ 0, 1, 2, 3, 4, 5, 6, 7 }, idx2{ 4, 5, 6, 7, 8, 9, 10, 11 };
    std::vector<double> w1{ 1, 1, 1, 1, ring_w, ring_w, ring_w, ring_w }, w2{ ring_w, ring_w, ring_w, ring_w, 1, 1, 1, 1 };

    W video = node("Video", { L(700), S(name("DiffuseTex", "Video")), S("Clip") }, {
        node("Type", { S("Clip") }),
        node("Properties70", { }, { P("Path", "KString", "XRefUrl", "", { S("C:\\nowhere\\tex.png") }) }),
        node("UseMipMap", { I(0) }),
        node("Filename", { S("C:\\nowhere\\tex.png") }),
        node("RelativeFilename", { S("..\\missing\\tex.png") }),
    });
    if (embed) video.kids.push_back(node("Content", { R(make_png()) }));

    std::vector<W> objects{
        geom,
        node("Model", { L(200), S(name("Body", "Model")), S("Mesh") }, { node("Version", { I(232) }), node("Properties70") }),
        node("Model", { L(300), S(name("Bone1", "Model")), S("LimbNode") }, { node("Version", { I(232) }), node("Properties70", {}, { P3("Lcl Translation", 0, 0, 0) }) }),
        node("Model", { L(400), S(name("Bone2", "Model")), S("LimbNode") }, { node("Version", { I(232) }), node("Properties70", {}, {
            P("RotationActive", "bool", "", "", { I(1) }),
            P("PreRotation", "Vector3D", "Vector", "", { D(0), D(0), D(30) }),
            P3("Lcl Translation", 0, 0, 100) }) }),
        node("Material", { L(500), S(name("Skin", "Material")), S("") }, {
            node("Version", { I(102) }), node("ShadingModel", { S("phong") }),
            node("Properties70", {}, {
                P("DiffuseColor", "Color", "", "A", { D(0.8), D(0.2), D(0.1) }),
                P("EmissiveColor", "Color", "", "A", { D(0), D(0), D(0) }),
                P("Opacity", "double", "Number", "", { D(1.0) }),
                P("Shininess", "double", "Number", "", { D(50.0) }) }) }),
        node("Texture", { L(600), S(name("DiffuseTex", "Texture")), S("") }, {
            node("Type", { S("TextureVideoClip") }), node("Version", { I(202) }),
            node("TextureName", { S(name("DiffuseTex", "Texture")) }),
            node("Media", { S(name("DiffuseTex", "Video")) }),
            node("FileName", { S("C:\\nowhere\\tex.png") }),
            node("RelativeFilename", { S("..\\missing\\tex.png") }) }),
        video,
        node("Deformer", { L(800), S(name("Armature", "Deformer")), S("Skin") }, { node("Version", { I(101) }), node("Link_DeformAcuracy", { D(50) }) }),
        node("Deformer", { L(810), S(name("Bone1", "SubDeformer")), S("Cluster") }, {
            node("Version", { I(100) }), node("Indexes", { Ia(idx1) }), node("Weights", { Da(w1) }),
            node("Transform", { Da(column_major(mm::identity())) }), node("TransformLink", { Da(column_major(mm::identity())) }) }),
        node("Deformer", { L(820), S(name("Bone2", "SubDeformer")), S("Cluster") }, {
            node("Version", { I(100) }), node("Indexes", { Ia(idx2) }), node("Weights", { Da(w2) }),
            node("Transform", { Da(column_major(mm::identity())) }), node("TransformLink", { Da(column_major(bone2_link())) }) }),
        node("AnimationStack", { L(900), S(name("Take 001", "AnimStack")), S("") }, { node("Properties70", {}, {
            P("LocalStart", "KTime", "Time", "", { L(0) }), P("LocalStop", "KTime", "Time", "", { L(kSecond) }),
            P("ReferenceStart", "KTime", "Time", "", { L(0) }), P("ReferenceStop", "KTime", "Time", "", { L(kSecond) }) }) }),
        node("AnimationLayer", { L(910), S(name("BaseLayer", "AnimLayer")), S("") }),
        node("AnimationCurveNode", { L(920), S(name("R", "AnimCurveNode")), S("") }, { node("Properties70", {}, {
            P("d|X", "Number", "", "A", { D(0) }), P("d|Y", "Number", "", "A", { D(0) }), P("d|Z", "Number", "", "A", { D(0) }) }) }),
        node("AnimationCurve", { L(930), S(name("", "AnimCurve")), S("") }, {
            node("Default", { D(0) }), node("KeyVer", { I(4009) }),
            node("KeyTime", { La({ 0, kSecond }) }), node("KeyValueFloat", { Fa({ 0.0, 90.0 }) }),
            node("KeyAttrFlags", { Ia({ 24840 }) }), node("KeyAttrDataFloat", { Fa({ 0, 0, 255790911, 0 }) }), node("KeyAttrRefCount", { Ia({ 2 }) }) }),
    };

    auto oo = [](std::int64_t c, std::int64_t p) { return node("C", { S("OO"), L(c), L(p) }); };
    auto op = [](std::int64_t c, std::int64_t p, const std::string& prop) { return node("C", { S("OP"), L(c), L(p), S(prop) }); };
    std::vector<W> conns{
        oo(200, 0), oo(300, 0), oo(400, 300), oo(100, 200), oo(500, 200), op(600, 500, "DiffuseColor"), oo(700, 600),
        oo(800, 100), oo(810, 800), oo(820, 800), oo(300, 810), oo(400, 820),
        oo(910, 900), oo(920, 910), op(920, 400, "Lcl Rotation"), op(930, 920, "d|X"),
    };

    return {
        node("FBXHeaderExtension", {}, { node("FBXHeaderVersion", { I(1003) }), node("FBXVersion", { I(7400) }) }),
        node("GlobalSettings", {}, { node("Version", { I(1000) }), node("Properties70", {}, {
            P("UpAxis", "int", "Integer", "", { I(2) }), P("UpAxisSign", "int", "Integer", "", { I(1) }),
            P("FrontAxis", "int", "Integer", "", { I(1) }), P("FrontAxisSign", "int", "Integer", "", { I(-1) }),
            P("CoordAxis", "int", "Integer", "", { I(0) }), P("CoordAxisSign", "int", "Integer", "", { I(1) }),
            P("UnitScaleFactor", "double", "Number", "", { D(1.0) }) }) }),
        node("Definitions", {}, { node("Version", { I(100) }), node("ObjectType", { S("Model") }, {
            node("Count", { I(3) }),
            node("PropertyTemplate", { S("FbxNode") }, { node("Properties70", {}, { P("Lcl Scaling", "Lcl Scaling", "", "A", { D(1), D(1), D(1) }) }) }) }) }),
        node("Objects", {}, objects),
        node("Connections", {}, conns),
    };
}

void write_file(const fs::path& p, const std::vector<std::uint8_t>& b) { std::ofstream f(p, std::ios::binary); f.write(reinterpret_cast<const char*>(b.data()), static_cast<std::streamsize>(b.size())); }
void write_file(const fs::path& p, const std::string& s) { std::ofstream f(p, std::ios::binary); f.write(s.data(), static_cast<std::streamsize>(s.size())); }
std::vector<std::uint8_t> read_file(const fs::path& p) { std::ifstream f(p, std::ios::binary); return std::vector<std::uint8_t>((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>()); }

std::size_t triangles(const gfx::Model3D& m) {
    std::size_t n = 0;
    for (const auto& md : m.meshes) for (const auto& p : md.primitives) n += p.mesh.indices().size() / 3;
    return n;
}

void check_model(const fs::path& file, const std::string& tag, std::size_t want_vertices) {
    gfx::Model3D m;
    std::string err;
    gfx::ModelImportOptions yup;
    yup.z_up = false;
    const bool ok = gfx::Model3D::load(file, m, &err, yup);
    check(ok, tag + ": load (" + err + ")");
    if (!ok) return;

    check(m.vertex_count() == want_vertices, tag + ": vertex count " + std::to_string(m.vertex_count()));
    check(triangles(m) == 20, tag + ": triangle count " + std::to_string(triangles(m)));
    check(m.nodes.size() == 3, tag + ": node count");
    const int body = m.find_node("Body"), bone1 = m.find_node("Bone1"), bone2 = m.find_node("Bone2");
    check(body >= 0 && bone1 >= 0 && bone2 >= 0, tag + ": node names");
    if (body < 0 || bone1 < 0 || bone2 < 0) return;
    check(m.nodes[static_cast<std::size_t>(bone2)].parent == bone1, tag + ": bone parenting");
    check(m.nodes[static_cast<std::size_t>(body)].mesh == 0, tag + ": mesh attached to Body");

    fizmo::linalg::vector3d lo, hi;
    m.bounds(lo, hi);
    check(near(lo.x, -0.1) && near(hi.x, 0.1), tag + ": x bounds " + std::to_string(lo.x) + " " + std::to_string(hi.x));
    check(near(lo.y, 0.0) && near(hi.y, 2.0), tag + ": upright y bounds in metres " + std::to_string(lo.y) + " " + std::to_string(hi.y));
    check(near(lo.z, -0.1) && near(hi.z, 0.1), tag + ": z bounds " + std::to_string(lo.z) + " " + std::to_string(hi.z));

    gfx::Model3D zm;
    if (gfx::Model3D::load_fbx(file, zm, &err)) {
        zm.bounds(lo, hi);
        check(near(lo.z, 0.0) && near(hi.z, 2.0) && near(hi.y - lo.y, 0.2), tag + ": default z-up import is upright " + std::to_string(lo.z) + " " + std::to_string(hi.z));
    } else check(false, tag + ": default options load");

    check(m.materials.size() == 1, tag + ": material count");
    if (!m.materials.empty()) {
        const gfx::ModelMaterial& mat = m.materials[0];
        check(mat.name == "Skin", tag + ": material name '" + mat.name + "'");
        check(near(mat.roughness, std::sqrt(2.0 / 52.0), 1e-3), tag + ": roughness from shininess");
        check(!mat.transparent, tag + ": opaque");
        check(mat.texture >= 0 && static_cast<std::size_t>(mat.texture) < m.textures.size(), tag + ": base color texture");
        const gfx::Texture* t = m.texture_of(0);
        const BitmapImage* img = t ? t->image() : nullptr;
        check(img && img->width() == 2 && img->height() == 2, tag + ": texture size");
        if (img && img->width() == 2) {
            const Color c = img->get_pixel(0, 0), g = img->get_pixel(1, 0);
            check(c.red() == 255 && c.green() == 0 && c.blue() == 0 && g.green() == 255, tag + ": texture pixels");
        }
        check(m.meshes[0].primitives[0].material == 0, tag + ": primitive material");
    }

    check(m.skins.size() == 1, tag + ": skin count");
    if (m.skins.empty()) return;
    const gfx::ModelSkin& skin = m.skins[0];
    check(skin.joints.size() == 2, tag + ": joint count " + std::to_string(skin.joints.size()));
    check(m.nodes[static_cast<std::size_t>(body)].skin == 0, tag + ": skin on mesh node");
    const auto& prim = m.meshes[0].primitives[0];
    check(prim.skinned(), tag + ": primitive skinned");
    bool sums = true, mid = false;
    for (std::size_t i = 0; i < prim.weights.size(); ++i) {
        double s = 0.0;
        for (float w : prim.weights[i]) s += w;
        if (!near(s, 1.0, 1e-5)) sums = false;
        if (near(prim.mesh.vertices()[i].z, 100.0, 1e-3) && near(prim.weights[i][0], 0.5, 1e-5) && near(prim.weights[i][1], 0.5, 1e-5)) mid = true;
    }
    check(sums, tag + ": weights sum to 1");
    check(mid, tag + ": middle ring weights renormalised to 0.5/0.5");

    check(m.animations.size() == 1, tag + ": animation count");
    if (m.animations.empty()) return;
    check(m.animations[0].name == "Take 001", tag + ": animation name '" + m.animations[0].name + "'");
    check(near(m.animations[0].duration, 1.0, 1e-4), tag + ": duration " + std::to_string(m.animations[0].duration));

    gfx::ModelAnimator anim(m);
    const gfx::Mesh3D* rest = anim.skinned_mesh(0, 0);
    check(rest != nullptr, tag + ": skinned mesh exists");
    if (!rest) return;
    std::vector<fizmo::linalg::vector3d> rest_pos;
    double bind_err = 0.0;
    for (std::size_t i = 0; i < rest->vertices().size(); ++i) {
        const auto& sv = rest->vertices()[i];
        const auto& ov = prim.mesh.vertices()[i];
        const auto want = mm::point(m.import_transform, ov.x, ov.y, ov.z);
        bind_err = std::max({ bind_err, std::abs(sv.x - want.x), std::abs(sv.y - want.y), std::abs(sv.z - want.z) });
        rest_pos.push_back({ sv.x, sv.y, sv.z });
    }
    check(bind_err < 1e-4, tag + ": rest pose skinning matches bind pose (err " + std::to_string(bind_err) + ")");

    check(anim.play(0, false), tag + ": play");
    anim.update(0.5);
    const mm::Quat want_q = qmul(axis_quat(0, 0, 1, 30), axis_quat(1, 0, 0, 45));
    const mm::Quat got = anim.pose()[static_cast<std::size_t>(bone2)].rotation;
    const double dot = std::abs(got.x * want_q.x + got.y * want_q.y + got.z * want_q.z + got.w * want_q.w);
    check(dot > 0.99999, tag + ": Bone2 rotation at 0.5 s (dot " + std::to_string(dot) + ")");

    const gfx::Mesh3D* posed = anim.skinned_mesh(0, 0);
    double moved = 0.0, top_err = 0.0;
    const mm::Mat4 bone2_now = mm::mul(mm::trs({ 0, 0, 100 }, want_q, { 1, 1, 1 }), mm::inverse(bone2_link()));
    for (std::size_t i = 0; posed && i < posed->vertices().size(); ++i) {
        const auto& v = posed->vertices()[i];
        moved = std::max(moved, std::abs(v.y - rest_pos[i].y) + std::abs(v.z - rest_pos[i].z) + std::abs(v.x - rest_pos[i].x));
        const auto& ov = prim.mesh.vertices()[i];
        if (near(ov.z, 200.0, 1e-3)) {
            const auto file_pos = mm::point(bone2_now, ov.x, ov.y, ov.z);
            const auto want = mm::point(m.import_transform, file_pos.x, file_pos.y, file_pos.z);
            top_err = std::max({ top_err, std::abs(v.x - want.x), std::abs(v.y - want.y), std::abs(v.z - want.z) });
        }
    }
    check(moved > 0.3, tag + ": skinned vertex moves (" + std::to_string(moved) + ")");
    check(top_err < 1e-4, tag + ": top ring follows Bone2 exactly (err " + std::to_string(top_err) + ")");
}

void fuzz(const fs::path& dir, const std::vector<std::uint8_t>& good, const std::string& tag) {
    int survived = 0;
    for (std::size_t cut = 0; cut < good.size(); cut += 37) {
        const fs::path p = dir / "cut.fbx";
        write_file(p, std::vector<std::uint8_t>(good.begin(), good.begin() + static_cast<std::ptrdiff_t>(cut)));
        gfx::Model3D m;
        std::string err;
        gfx::Model3D::load_fbx(p, m, &err);
        ++survived;
    }
    std::mt19937 rng(1234);
    for (int it = 0; it < 400; ++it) {
        std::vector<std::uint8_t> bad = good;
        const int flips = 1 + static_cast<int>(rng() % 8);
        for (int f = 0; f < flips; ++f) {
            const std::size_t at = rng() % bad.size();
            const std::uint32_t pick = rng();
            bad[at] = (pick & 1) ? static_cast<std::uint8_t>(pick >> 8) : static_cast<std::uint8_t>("{}\",:*;0123456789-.e\n"[(pick >> 8) % 21]);
        }
        const fs::path p = dir / "fuzz.fbx";
        write_file(p, bad);
        gfx::Model3D m;
        std::string err;
        if (gfx::Model3D::load_fbx(p, m, &err)) {
            fizmo::linalg::vector3d lo, hi;
            m.bounds(lo, hi);
            gfx::ModelAnimator a(m);
            if (!m.animations.empty()) { a.play(0); a.update(0.25); }
        }
        ++survived;
    }
    check(survived > 0, "robustness: truncated and corrupted " + tag + " files do not crash");
}

void robustness(const fs::path& dir, const std::vector<std::uint8_t>& good) {

    std::vector<std::uint8_t> huge = good;
    const std::size_t at = 27;
    std::vector<std::uint8_t> bomb(huge.begin(), huge.begin() + static_cast<std::ptrdiff_t>(at));
    const std::uint32_t count = (1u << 27) + 1u;
    std::vector<std::uint8_t> rec;
    auto put32 = [&](std::uint32_t v) { for (int k = 0; k < 4; ++k) rec.push_back(static_cast<std::uint8_t>(v >> (8 * k))); };
    const std::uint32_t plen = 1 + 12 + 4;
    put32(static_cast<std::uint32_t>(at + 13 + 1 + plen));
    put32(1);
    put32(plen);
    rec.push_back(1);
    rec.push_back('A');
    rec.push_back('d');
    put32(count); put32(0); put32(4);
    put32(0);
    bomb.insert(bomb.end(), rec.begin(), rec.end());
    bomb.insert(bomb.end(), 13, 0);
    write_file(dir / "bomb.fbx", bomb);
    gfx::Model3D m;
    std::string err;
    check(!gfx::Model3D::load_fbx(dir / "bomb.fbx", m, &err) && err.find("too large") != std::string::npos, "robustness: oversized array rejected (" + err + ")");

    write_file(dir / "old.fbx", std::string("; FBX 6.1.0 project file\nFBXHeaderExtension:  {\n    FBXVersion: 6100\n}\nObjects:  {\n    Model: \"Model::Cube\", \"Mesh\" {\n    }\n}\n"));
    err.clear();
    check(!gfx::Model3D::load(dir / "old.fbx", m, &err) && err.find("FBX 7") != std::string::npos && err.find("glTF") != std::string::npos, "FBX 6 rejected with a clear message (" + err + ")");

    std::vector<std::uint8_t> old_bin = good;
    const std::uint32_t v6 = 6100;
    std::memcpy(old_bin.data() + 23, &v6, 4);
    write_file(dir / "old_bin.fbx", old_bin);
    err.clear();
    check(!gfx::Model3D::load_fbx(dir / "old_bin.fbx", m, &err) && err.find("FBX 7") != std::string::npos, "binary FBX 6 rejected (" + err + ")");

    write_file(dir / "garbage.fbx", std::string("hello world, this is not a model"));
    err.clear();
    check(!gfx::Model3D::load_fbx(dir / "garbage.fbx", m, &err) && !err.empty(), "garbage rejected (" + err + ")");
}

} // namespace

int main() {
    const fs::path dir = fs::temp_directory_path() / "fizmo_fbx_tests";
    std::error_code ec;
    fs::remove_all(dir, ec);
    fs::create_directories(dir / "external.fbm", ec);

    const std::vector<std::uint8_t> bin74 = BinaryWriter(7400, true).finish(make_scene(false, true));
    write_file(dir / "binary74.fbx", bin74);
    write_file(dir / "ascii74.fbx", ascii_file(make_scene(true, true)));
    write_file(dir / "binary75.fbx", BinaryWriter(7500, false).finish(make_scene(false, true)));
    write_file(dir / "external.fbx", BinaryWriter(7700, true).finish(make_scene(false, false)));
    write_file(dir / "external.fbm" / "tex.png", make_png());

    check_model(dir / "binary74.fbx", "binary 7.4 zlib", 40);
    check_model(dir / "ascii74.fbx", "ascii 7.4", 12);
    check_model(dir / "binary75.fbx", "binary 7.5 64-bit headers", 40);
    check_model(dir / "external.fbx", "binary 7.7 external .fbm texture", 40);

    fuzz(dir, bin74, "binary");
    fuzz(dir, read_file(dir / "ascii74.fbx"), "ascii");
    robustness(dir, bin74);

    std::printf("model fbx tests: %d passed, %d failed\n", g_passed, g_failed);
    return g_failed == 0 ? 0 : 1;
}
