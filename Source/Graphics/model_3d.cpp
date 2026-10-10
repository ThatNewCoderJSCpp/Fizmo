#include "fizmo_library.hpp"
#include "model_3d.hpp"

namespace fizmo {
namespace graphics {
namespace model_math {

Mat4 mul(const Mat4& a, const Mat4& b) noexcept {
    Mat4 r{};
    for (int i = 0; i < 4; ++i)
        for (int j = 0; j < 4; ++j)
            r[i * 4 + j] = a[i * 4] * b[j] + a[i * 4 + 1] * b[4 + j] + a[i * 4 + 2] * b[8 + j] + a[i * 4 + 3] * b[12 + j];
    return r;
}

Quat normalize(Quat q) noexcept {
    const double l = std::sqrt(q.x * q.x + q.y * q.y + q.z * q.z + q.w * q.w);
    if (l <= 0.0) return {};
    q.x /= l; q.y /= l; q.z /= l; q.w /= l;
    return q;
}

Quat slerp(Quat a, Quat b, double t) noexcept {
    double d = a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w;
    if (d < 0.0) { b.x = -b.x; b.y = -b.y; b.z = -b.z; b.w = -b.w; d = -d; }
    if (d > 0.9995) return normalize({ a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t, a.z + (b.z - a.z) * t, a.w + (b.w - a.w) * t });
    const double th = std::acos(std::min(1.0, d));
    const double s = std::sin(th);
    const double wa = std::sin((1.0 - t) * th) / s, wb = std::sin(t * th) / s;
    return { a.x * wa + b.x * wb, a.y * wa + b.y * wb, a.z * wa + b.z * wb, a.w * wa + b.w * wb };
}

Mat4 trs(const vector3d& t, const Quat& q0, const vector3d& s) noexcept {
    const Quat q = normalize(q0);
    const double xx = q.x * q.x, yy = q.y * q.y, zz = q.z * q.z;
    const double xy = q.x * q.y, xz = q.x * q.z, yz = q.y * q.z, wx = q.w * q.x, wy = q.w * q.y, wz = q.w * q.z;
    return {
        (1 - 2 * (yy + zz)) * s.x, 2 * (xy - wz) * s.y,       2 * (xz + wy) * s.z,       t.x,
        2 * (xy + wz) * s.x,       (1 - 2 * (xx + zz)) * s.y, 2 * (yz - wx) * s.z,       t.y,
        2 * (xz - wy) * s.x,       2 * (yz + wx) * s.y,       (1 - 2 * (xx + yy)) * s.z, t.z,
        0, 0, 0, 1
    };
}

Mat4 inverse(const Mat4& m) noexcept {
    Mat4 inv{};
    inv[0] = m[5] * m[10] * m[15] - m[5] * m[11] * m[14] - m[9] * m[6] * m[15] + m[9] * m[7] * m[14] + m[13] * m[6] * m[11] - m[13] * m[7] * m[10];
    inv[4] = -m[4] * m[10] * m[15] + m[4] * m[11] * m[14] + m[8] * m[6] * m[15] - m[8] * m[7] * m[14] - m[12] * m[6] * m[11] + m[12] * m[7] * m[10];
    inv[8] = m[4] * m[9] * m[15] - m[4] * m[11] * m[13] - m[8] * m[5] * m[15] + m[8] * m[7] * m[13] + m[12] * m[5] * m[11] - m[12] * m[7] * m[9];
    inv[12] = -m[4] * m[9] * m[14] + m[4] * m[10] * m[13] + m[8] * m[5] * m[14] - m[8] * m[6] * m[13] - m[12] * m[5] * m[10] + m[12] * m[6] * m[9];
    inv[1] = -m[1] * m[10] * m[15] + m[1] * m[11] * m[14] + m[9] * m[2] * m[15] - m[9] * m[3] * m[14] - m[13] * m[2] * m[11] + m[13] * m[3] * m[10];
    inv[5] = m[0] * m[10] * m[15] - m[0] * m[11] * m[14] - m[8] * m[2] * m[15] + m[8] * m[3] * m[14] + m[12] * m[2] * m[11] - m[12] * m[3] * m[10];
    inv[9] = -m[0] * m[9] * m[15] + m[0] * m[11] * m[13] + m[8] * m[1] * m[15] - m[8] * m[3] * m[13] - m[12] * m[1] * m[11] + m[12] * m[3] * m[9];
    inv[13] = m[0] * m[9] * m[14] - m[0] * m[10] * m[13] - m[8] * m[1] * m[14] + m[8] * m[2] * m[13] + m[12] * m[1] * m[10] - m[12] * m[2] * m[9];
    inv[2] = m[1] * m[6] * m[15] - m[1] * m[7] * m[14] - m[5] * m[2] * m[15] + m[5] * m[3] * m[14] + m[13] * m[2] * m[7] - m[13] * m[3] * m[6];
    inv[6] = -m[0] * m[6] * m[15] + m[0] * m[7] * m[14] + m[4] * m[2] * m[15] - m[4] * m[3] * m[14] - m[12] * m[2] * m[7] + m[12] * m[3] * m[6];
    inv[10] = m[0] * m[5] * m[15] - m[0] * m[7] * m[13] - m[4] * m[1] * m[15] + m[4] * m[3] * m[13] + m[12] * m[1] * m[7] - m[12] * m[3] * m[5];
    inv[14] = -m[0] * m[5] * m[14] + m[0] * m[6] * m[13] + m[4] * m[1] * m[14] - m[4] * m[2] * m[13] - m[12] * m[1] * m[6] + m[12] * m[2] * m[5];
    inv[3] = -m[1] * m[6] * m[11] + m[1] * m[7] * m[10] + m[5] * m[2] * m[11] - m[5] * m[3] * m[10] - m[9] * m[2] * m[7] + m[9] * m[3] * m[6];
    inv[7] = m[0] * m[6] * m[11] - m[0] * m[7] * m[10] - m[4] * m[2] * m[11] + m[4] * m[3] * m[10] + m[8] * m[2] * m[7] - m[8] * m[3] * m[6];
    inv[11] = -m[0] * m[5] * m[11] + m[0] * m[7] * m[9] + m[4] * m[1] * m[11] - m[4] * m[3] * m[9] - m[8] * m[1] * m[7] + m[8] * m[3] * m[5];
    inv[15] = m[0] * m[5] * m[10] - m[0] * m[6] * m[9] - m[4] * m[1] * m[10] + m[4] * m[2] * m[9] + m[8] * m[1] * m[6] - m[8] * m[2] * m[5];
    const double det = m[0] * inv[0] + m[1] * inv[4] + m[2] * inv[8] + m[3] * inv[12];
    if (std::abs(det) < 1e-300) return identity();
    for (double& v : inv) v /= det;
    return inv;
}

vector3d point(const Mat4& m, double x, double y, double z) noexcept {
    return { m[0] * x + m[1] * y + m[2] * z + m[3], m[4] * x + m[5] * y + m[6] * z + m[7], m[8] * x + m[9] * y + m[10] * z + m[11] };
}

math::Matrix4d to_matrix(const Mat4& m) noexcept {
    math::Matrix4d out = math::Matrix4d::identity();
    for (int i = 0; i < 16; ++i) out.data[static_cast<std::size_t>(i)] = m[static_cast<std::size_t>(i)];
    return out;
}

Mat4 from_matrix(const math::Matrix4d& m) noexcept {
    Mat4 out{};
    for (int i = 0; i < 16; ++i) out[static_cast<std::size_t>(i)] = m.data[static_cast<std::size_t>(i)];
    return out;
}

} // namespace model_math
} // namespace graphics
} // namespace fizmo

namespace fizmo {
namespace graphics {

bool ModelPrimitive::skinned() const noexcept { return !joints.empty() && joints.size() == weights.size() && joints.size() == mesh.vertices().size(); }

void Model3D::make_import(Model3D& m, const ModelImportOptions& o) {
    const double s = o.scale;
    if (o.z_up) m.import_transform = { s, 0, 0, 0, 0, 0, -s, 0, 0, s, 0, 0, 0, 0, 0, 1 };
    else m.import_transform = { s, 0, 0, 0, 0, s, 0, 0, 0, 0, s, 0, 0, 0, 0, 1 };
}

void Model3D::generate_normals(Mesh3D& mesh, bool smooth) {
    auto& v = mesh.edit_vertices();
    const auto& idx = mesh.indices();
    const std::size_t tris = idx.empty() ? v.size() / 3 : idx.size() / 3;
    if (!smooth && !idx.empty()) {
        std::vector<Vertex3D> flat;
        flat.reserve(idx.size());
        for (std::uint32_t i : idx) flat.push_back(v[i]);
        v.swap(flat);
        mesh.edit_indices().clear();
        generate_normals(mesh, true);
        return;
    }
    std::vector<vector3d> acc(v.size(), vector3d{ 0, 0, 0 });
    for (std::size_t t = 0; t < tris; ++t) {
        const std::uint32_t a = idx.empty() ? static_cast<std::uint32_t>(t * 3) : idx[t * 3];
        const std::uint32_t b = idx.empty() ? static_cast<std::uint32_t>(t * 3 + 1) : idx[t * 3 + 1];
        const std::uint32_t c = idx.empty() ? static_cast<std::uint32_t>(t * 3 + 2) : idx[t * 3 + 2];
        if (a >= v.size() || b >= v.size() || c >= v.size()) continue;
        const vector3d e1{ v[b].x - v[a].x, v[b].y - v[a].y, v[b].z - v[a].z };
        const vector3d e2{ v[c].x - v[a].x, v[c].y - v[a].y, v[c].z - v[a].z };
        const vector3d n{ e1.y * e2.z - e1.z * e2.y, e1.z * e2.x - e1.x * e2.z, e1.x * e2.y - e1.y * e2.x };
        acc[a] = acc[a] + n; acc[b] = acc[b] + n; acc[c] = acc[c] + n;
    }
    for (std::size_t i = 0; i < v.size(); ++i) {
        const double l = std::sqrt(acc[i].x * acc[i].x + acc[i].y * acc[i].y + acc[i].z * acc[i].z);
        if (l > 0.0) { v[i].nx = static_cast<float>(acc[i].x / l); v[i].ny = static_cast<float>(acc[i].y / l); v[i].nz = static_cast<float>(acc[i].z / l); }
    }
}

std::vector<std::uint8_t> Model3D::base64(const std::string& s) {
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
    int buf = 0, bits = 0;
    for (char c : s) {
        const int v = val(c);
        if (v < 0) continue;
        buf = (buf << 6) | v;
        bits += 6;
        if (bits >= 8) { bits -= 8; out.push_back(static_cast<std::uint8_t>((buf >> bits) & 0xFF)); }
    }
    return out;
}

bool Model3D::accessor_view(const GltfContext& g, int index, const std::uint8_t*& base, std::size_t& stride, std::size_t& count, int& comps, int& ctype, bool& normalized, std::size_t& avail) {
    const json::Value& acc = g.doc["accessors"][static_cast<std::size_t>(index)];
    if (!acc.is_object()) return false;
    const std::string type = acc["type"].as_string();
    comps = type == "SCALAR" ? 1 : type == "VEC2" ? 2 : type == "VEC3" ? 3 : type == "VEC4" ? 4 : type == "MAT2" ? 4 : type == "MAT3" ? 9 : type == "MAT4" ? 16 : 0;
    ctype = static_cast<int>(acc["componentType"].as_int());
    normalized = acc["normalized"].as_bool();
    count = acc["count"].as_uint();
    const std::size_t csize = (ctype == 5120 || ctype == 5121) ? 1 : (ctype == 5122 || ctype == 5123) ? 2 : 4;
    base = nullptr;
    stride = csize * static_cast<std::size_t>(comps);
    avail = 0;
    if (!acc.has("bufferView")) return comps > 0;
    const json::Value& view = g.doc["bufferViews"][acc["bufferView"].as_uint()];
    const std::size_t buffer = view["buffer"].as_uint();
    if (buffer >= g.buffers.size()) return false;
    const std::size_t view_off = static_cast<std::size_t>(view["byteOffset"].as_number());
    const std::size_t acc_off = static_cast<std::size_t>(acc["byteOffset"].as_number());
    const std::size_t view_len = static_cast<std::size_t>(view["byteLength"].as_number());
    if (view.has("byteStride")) stride = static_cast<std::size_t>(view["byteStride"].as_number());
    const std::vector<std::uint8_t>& data = g.buffers[buffer];
    if (view_off + acc_off > data.size() || acc_off > view_len) return false;
    base = data.data() + view_off + acc_off;
    avail = std::min(view_len - acc_off, data.size() - view_off - acc_off);
    return comps > 0;
}

double Model3D::read_component(const std::uint8_t* p, int ctype, bool normalized) noexcept {
    switch (ctype) {
        case 5120: { std::int8_t v; std::memcpy(&v, p, 1); return normalized ? std::max(v / 127.0, -1.0) : v; }
        case 5121: { std::uint8_t v = *p; return normalized ? v / 255.0 : v; }
        case 5122: { std::int16_t v; std::memcpy(&v, p, 2); return normalized ? std::max(v / 32767.0, -1.0) : v; }
        case 5123: { std::uint16_t v; std::memcpy(&v, p, 2); return normalized ? v / 65535.0 : v; }
        case 5125: { std::uint32_t v; std::memcpy(&v, p, 4); return v; }
        case 5126: { float v; std::memcpy(&v, p, 4); return v; }
        default: return 0.0;
    }
}

bool Model3D::read_accessor(const GltfContext& g, int index, std::vector<double>& out, int& comps) {
    const std::uint8_t* base = nullptr;
    std::size_t stride = 0, count = 0, avail = 0;
    int ctype = 0;
    bool normalized = false;
    if (!accessor_view(g, index, base, stride, count, comps, ctype, normalized, avail)) return false;
    const std::size_t csize = (ctype == 5120 || ctype == 5121) ? 1 : (ctype == 5122 || ctype == 5123) ? 2 : 4;
    out.assign(count * static_cast<std::size_t>(comps), 0.0);

    if (base) {
        for (std::size_t i = 0; i < count; ++i) {
            if (i * stride + csize * comps > avail) return false;
            for (int c = 0; c < comps; ++c) out[i * comps + c] = read_component(base + i * stride + c * csize, ctype, normalized);
        }
    }

    const json::Value& sparse = g.doc["accessors"][static_cast<std::size_t>(index)]["sparse"];
    if (sparse.is_object()) {
        const std::size_t n = sparse["count"].as_uint();
        const json::Value& si = sparse["indices"];
        const json::Value& sv = sparse["values"];
        const json::Value& iv = g.doc["bufferViews"][si["bufferView"].as_uint()];
        const json::Value& vv = g.doc["bufferViews"][sv["bufferView"].as_uint()];
        if (iv["buffer"].as_uint() >= g.buffers.size() || vv["buffer"].as_uint() >= g.buffers.size()) return false;
        const auto& ib = g.buffers[iv["buffer"].as_uint()];
        const auto& vb = g.buffers[vv["buffer"].as_uint()];
        const std::size_t ioff = static_cast<std::size_t>(iv["byteOffset"].as_number() + si["byteOffset"].as_number());
        const std::size_t voff = static_cast<std::size_t>(vv["byteOffset"].as_number() + sv["byteOffset"].as_number());
        const int itype = static_cast<int>(si["componentType"].as_int());
        const std::size_t isize = itype == 5121 ? 1 : itype == 5123 ? 2 : 4;
        for (std::size_t k = 0; k < n; ++k) {
            if (ioff + (k + 1) * isize > ib.size() || voff + (k + 1) * csize * comps > vb.size()) return false;
            const std::size_t target = static_cast<std::size_t>(read_component(ib.data() + ioff + k * isize, itype, false));
            if (target >= count) continue;
            for (int c = 0; c < comps; ++c) out[target * comps + c] = read_component(vb.data() + voff + (k * comps + c) * csize, ctype, normalized);
        }
    }

    return true;
}

bool Model3D::load_buffers(GltfContext& g, const std::vector<std::uint8_t>* glb_bin) {
    for (const json::Value& b : g.doc["buffers"].items()) {
        if (!b.has("uri")) {
            if (!glb_bin) { g.error = "buffer without uri outside GLB"; return false; }
            g.buffers.push_back(*glb_bin);
            continue;
        }
        const std::string uri = b["uri"].as_string();
        if (uri.rfind("data:", 0) == 0) {
            const std::size_t comma = uri.find(',');
            if (comma == std::string::npos || uri.find(";base64", 0) > comma) { g.error = "unsupported data uri"; return false; }
            g.buffers.push_back(base64(uri.substr(comma + 1)));
        } else {
            auto bytes = system::paths::read_bytes(g.dir / system::paths::from_utf8(uri_decode(uri)));
            if (!bytes) { g.error = "cannot read buffer " + uri; return false; }
            g.buffers.push_back(std::move(*bytes));
        }
    }
    return true;
}

std::string Model3D::uri_decode(const std::string& s) {
    std::string out;
    for (std::size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '%' && i + 2 < s.size()) { out.push_back(static_cast<char>(std::strtol(s.substr(i + 1, 2).c_str(), nullptr, 16))); i += 2; }
        else out.push_back(s[i]);
    }
    return out;
}

bool Model3D::load_gltf_doc(GltfContext& g, Model3D& m, const ModelImportOptions& o) {
    const json::Value& d = g.doc;
    if (d["asset"]["version"].as_string().rfind("2", 0) != 0) { g.error = "only glTF 2.0 is supported"; return false; }

    for (const json::Value& im : d["images"].items()) {
        std::optional<images::BitmapImage> img;
        std::string err;
        if (im.has("bufferView")) {
            const json::Value& view = d["bufferViews"][im["bufferView"].as_uint()];
            const std::size_t buf = view["buffer"].as_uint();
            const std::size_t off = static_cast<std::size_t>(view["byteOffset"].as_number()), len = static_cast<std::size_t>(view["byteLength"].as_number());
            if (buf < g.buffers.size() && off + len <= g.buffers[buf].size()) img = system::load_image_memory(g.buffers[buf].data() + off, len, &err);
        } else {
            const std::string uri = im["uri"].as_string();
            if (uri.rfind("data:", 0) == 0) {
                const auto bytes = base64(uri.substr(uri.find(',') + 1));
                img = system::load_image_memory(bytes.data(), bytes.size(), &err);
            } else {
                img = system::load_image(g.dir / system::paths::from_utf8(uri_decode(uri)), &err);
            }
        }
        if (img) m.textures.emplace_back(std::move(*img), o.filter, WrapMode::Repeat);
        else { images::BitmapImage white(1, 1); m.textures.emplace_back(std::move(white)); }
    }

    std::vector<int> texture_image;
    for (const json::Value& t : d["textures"].items()) texture_image.push_back(t.has("source") ? static_cast<int>(t["source"].as_int()) : -1);

    for (const json::Value& mat : d["materials"].items()) {
        ModelMaterial mm;
        mm.name = mat["name"].as_string();
        const json::Value& pbr = mat["pbrMetallicRoughness"];
        const json::Value& bc = pbr["baseColorFactor"];
        if (bc.size() == 4) mm.base_color = Color(static_cast<std::uint8_t>(std::lround(std::min(1.0, bc[0].as_number()) * 255)), static_cast<std::uint8_t>(std::lround(std::min(1.0, bc[1].as_number()) * 255)), static_cast<std::uint8_t>(std::lround(std::min(1.0, bc[2].as_number()) * 255)), static_cast<std::uint8_t>(std::lround(std::min(1.0, bc[3].as_number()) * 255)));
        if (pbr["baseColorTexture"].is_object()) {
            const std::size_t ti = pbr["baseColorTexture"]["index"].as_uint();
            if (ti < texture_image.size() && texture_image[ti] >= 0 && static_cast<std::size_t>(texture_image[ti]) < m.textures.size()) mm.texture = texture_image[ti];
        }
        mm.metallic = pbr["metallicFactor"].as_float(1.0f);
        mm.roughness = pbr["roughnessFactor"].as_float(1.0f);
        auto average = [&](const json::Value& ref, int channel) -> double {
            if (!ref.is_object()) return -1.0;
            const std::size_t ti = ref["index"].as_uint();
            if (ti >= texture_image.size() || texture_image[ti] < 0 || static_cast<std::size_t>(texture_image[ti]) >= m.textures.size()) return -1.0;
            const images::BitmapImage* img = m.textures[static_cast<std::size_t>(texture_image[ti])].image();
            if (!img || !img->is_valid_image()) return -1.0;
            double sum = 0.0;
            std::size_t n = 0;
            const unsigned int step_x = std::max(1u, img->width() / 64), step_y = std::max(1u, img->height() / 64);
            for (unsigned int y = 0; y < img->height(); y += step_y) for (unsigned int x = 0; x < img->width(); x += step_x) {
                const Color c = img->get_pixel(x, y);
                sum += channel == 0 ? c.red() : channel == 1 ? c.green() : channel == 2 ? c.blue() : (0.2126 * c.red() + 0.7152 * c.green() + 0.0722 * c.blue());
                ++n;
            }
            return n ? sum / n / 255.0 : -1.0;
        };
        const double avg_rough = average(pbr["metallicRoughnessTexture"], 1);
        const double avg_metal = average(pbr["metallicRoughnessTexture"], 2);
        if (avg_rough >= 0.0) mm.roughness = static_cast<float>(mm.roughness * avg_rough);
        if (avg_metal >= 0.0) mm.metallic = static_cast<float>(mm.metallic * avg_metal);
        const double avg_emissive = average(mat["emissiveTexture"], 3);
        const json::Value& ef = mat["emissiveFactor"];
        if (ef.size() == 3) {
            const double mx = std::max({ ef[0].as_number(), ef[1].as_number(), ef[2].as_number() });
            if (mx > 0.0) {
                mm.emissive = Color(static_cast<std::uint8_t>(std::lround(ef[0].as_number() / mx * 255)), static_cast<std::uint8_t>(std::lround(ef[1].as_number() / mx * 255)), static_cast<std::uint8_t>(std::lround(ef[2].as_number() / mx * 255)));
                mm.emissive_strength = static_cast<float>(mx * mat["extensions"]["KHR_materials_emissive_strength"]["emissiveStrength"].as_number(1.0) * (avg_emissive >= 0.0 ? avg_emissive : 1.0));
            }
        }
        const std::string alpha = mat["alphaMode"].as_string("OPAQUE");
        mm.transparent = alpha == "BLEND";
        if (alpha == "MASK") mm.alpha_cutoff = mat["alphaCutoff"].as_float(0.5f);
        mm.double_sided = mat["doubleSided"].as_bool();
        mm.unlit = mat["extensions"].has("KHR_materials_unlit");
        m.materials.push_back(mm);
    }

    for (const json::Value& mesh : d["meshes"].items()) {
        ModelMeshData md;
        md.name = mesh["name"].as_string();
        for (const json::Value& prim : mesh["primitives"].items()) {
            const int mode = static_cast<int>(prim["mode"].as_int(4));
            if (mode != 4 && mode != 5 && mode != 6) continue;
            const json::Value& at = prim["attributes"];
            if (!at.has("POSITION")) continue;
            std::vector<double> pos, nrm, uv, col, jnt, wgt;
            int pc = 0, nc = 0, uc = 0, cc = 0, jc = 0, wc = 0;
            if (!read_accessor(g, static_cast<int>(at["POSITION"].as_int()), pos, pc) || pc != 3) { g.error = "bad POSITION accessor"; return false; }
            const std::size_t n = pos.size() / 3;
            if (at.has("NORMAL") && (!read_accessor(g, static_cast<int>(at["NORMAL"].as_int()), nrm, nc) || nc != 3 || nrm.size() / 3 != n)) nrm.clear();
            if (at.has("TEXCOORD_0") && (!read_accessor(g, static_cast<int>(at["TEXCOORD_0"].as_int()), uv, uc) || uc != 2 || uv.size() / 2 != n)) uv.clear();
            if (at.has("COLOR_0") && (!read_accessor(g, static_cast<int>(at["COLOR_0"].as_int()), col, cc) || (cc != 3 && cc != 4) || col.size() / cc != n)) col.clear();
            if (at.has("JOINTS_0") && (!read_accessor(g, static_cast<int>(at["JOINTS_0"].as_int()), jnt, jc) || jc != 4 || jnt.size() / 4 != n)) jnt.clear();
            if (at.has("WEIGHTS_0") && (!read_accessor(g, static_cast<int>(at["WEIGHTS_0"].as_int()), wgt, wc) || wc != 4 || wgt.size() / 4 != n)) wgt.clear();
            ModelPrimitive p;
            p.material = prim.has("material") ? static_cast<int>(prim["material"].as_int()) : -1;
            const Color base = p.material >= 0 && static_cast<std::size_t>(p.material) < m.materials.size() ? m.materials[static_cast<std::size_t>(p.material)].base_color : Color(255, 255, 255, 255);
            auto& verts = p.mesh.edit_vertices();
            verts.resize(n);

            for (std::size_t i = 0; i < n; ++i) {
                Vertex3D& v = verts[i];
                v.x = static_cast<float>(pos[i * 3]); v.y = static_cast<float>(pos[i * 3 + 1]); v.z = static_cast<float>(pos[i * 3 + 2]);
                if (!nrm.empty()) { v.nx = static_cast<float>(nrm[i * 3]); v.ny = static_cast<float>(nrm[i * 3 + 1]); v.nz = static_cast<float>(nrm[i * 3 + 2]); }
                if (!uv.empty()) { v.u = static_cast<float>(uv[i * 2]); v.v = static_cast<float>(o.flip_v ? 1.0 - uv[i * 2 + 1] : uv[i * 2 + 1]); }
                double r = base.red() / 255.0, gg = base.green() / 255.0, b = base.blue() / 255.0, a = base.alpha() / 255.0;
                if (!col.empty()) { r *= col[i * cc]; gg *= col[i * cc + 1]; b *= col[i * cc + 2]; if (cc == 4) a *= col[i * cc + 3]; }
                v.rgba = Vertex3D::pack(Color(static_cast<std::uint8_t>(std::lround(std::min(1.0, r) * 255)), static_cast<std::uint8_t>(std::lround(std::min(1.0, gg) * 255)), static_cast<std::uint8_t>(std::lround(std::min(1.0, b) * 255)), static_cast<std::uint8_t>(std::lround(std::min(1.0, a) * 255))));
            }

            std::vector<std::uint32_t> idx;
            if (prim.has("indices")) {
                std::vector<double> iv;
                int ic = 0;
                if (!read_accessor(g, static_cast<int>(prim["indices"].as_int()), iv, ic) || ic != 1) { g.error = "bad index accessor"; return false; }
                idx.reserve(iv.size());
                for (double x : iv) idx.push_back(static_cast<std::uint32_t>(x));
            } else {
                for (std::size_t i = 0; i < n; ++i) idx.push_back(static_cast<std::uint32_t>(i));
            }

            std::vector<std::uint32_t> tri;
            if (mode == 4) tri = std::move(idx);
            else if (mode == 5) { for (std::size_t i = 2; i < idx.size(); ++i) { if (i % 2 == 0) { tri.push_back(idx[i - 2]); tri.push_back(idx[i - 1]); } else { tri.push_back(idx[i - 1]); tri.push_back(idx[i - 2]); } tri.push_back(idx[i]); } }
            else { for (std::size_t i = 2; i < idx.size(); ++i) { tri.push_back(idx[0]); tri.push_back(idx[i - 1]); tri.push_back(idx[i]); } }
            tri.resize(tri.size() - tri.size() % 3);
            bool valid = true;
            for (std::uint32_t i : tri) if (i >= n) { valid = false; break; }
            if (!valid) { g.error = "index out of range"; return false; }
            p.mesh.edit_indices() = std::move(tri);
            if (nrm.empty() && o.generate_normals) generate_normals(p.mesh, o.smooth_normals);

            if (!jnt.empty() && !wgt.empty()) {
                p.joints.resize(n);
                p.weights.resize(n);
                for (std::size_t i = 0; i < n; ++i) {
                    double sum = 0.0;
                    for (int k = 0; k < 4; ++k) { p.joints[i][k] = static_cast<std::uint16_t>(jnt[i * 4 + k]); p.weights[i][k] = static_cast<float>(wgt[i * 4 + k]); sum += wgt[i * 4 + k]; }
                    if (sum > 0.0) for (int k = 0; k < 4; ++k) p.weights[i][k] = static_cast<float>(p.weights[i][k] / sum);
                }
            }

            md.primitives.push_back(std::move(p));
        }
        m.meshes.push_back(std::move(md));
    }

    const std::size_t node_count = d["nodes"].size();
    m.nodes.resize(node_count);
    for (std::size_t i = 0; i < node_count; ++i) {
        const json::Value& nd = d["nodes"][i];
        ModelNode& n = m.nodes[i];
        n.name = nd["name"].as_string();
        n.mesh = nd.has("mesh") ? static_cast<int>(nd["mesh"].as_int()) : -1;
        n.skin = nd.has("skin") ? static_cast<int>(nd["skin"].as_int()) : -1;
        if (nd["matrix"].size() == 16) {
            float mm[16];
            for (int k = 0; k < 16; ++k) mm[k] = nd["matrix"][static_cast<std::size_t>(k)].as_float();
            n.matrix = model_math::from_column_major(mm);
            n.has_matrix = true;
        }
        if (nd["translation"].size() == 3) n.translation = { nd["translation"][0].as_number(), nd["translation"][1].as_number(), nd["translation"][2].as_number() };
        if (nd["rotation"].size() == 4) n.rotation = { nd["rotation"][0].as_number(), nd["rotation"][1].as_number(), nd["rotation"][2].as_number(), nd["rotation"][3].as_number() };
        if (nd["scale"].size() == 3) n.scale = { nd["scale"][0].as_number(), nd["scale"][1].as_number(), nd["scale"][2].as_number() };
        for (const json::Value& c : nd["children"].items()) {
            const std::size_t ci = c.as_uint();
            if (ci < node_count && ci != i) n.children.push_back(static_cast<int>(ci));
        }
    }
    for (std::size_t i = 0; i < node_count; ++i) for (int c : m.nodes[i].children) if (m.nodes[static_cast<std::size_t>(c)].parent < 0) m.nodes[static_cast<std::size_t>(c)].parent = static_cast<int>(i);

    const std::size_t scene = d.has("scene") ? d["scene"].as_uint() : 0;
    if (d["scenes"].size() > scene) { for (const json::Value& r : d["scenes"][scene]["nodes"].items()) if (r.as_uint() < node_count) m.roots.push_back(static_cast<int>(r.as_uint())); }
    else { for (std::size_t i = 0; i < node_count; ++i) if (m.nodes[i].parent < 0) m.roots.push_back(static_cast<int>(i)); }

    for (const json::Value& sk : d["skins"].items()) {
        ModelSkin s;
        s.name = sk["name"].as_string();
        for (const json::Value& j : sk["joints"].items()) s.joints.push_back(static_cast<int>(j.as_int()));
        if (sk.has("inverseBindMatrices")) {
            std::vector<double> ib;
            int comps = 0;
            if (!read_accessor(g, static_cast<int>(sk["inverseBindMatrices"].as_int()), ib, comps) || comps != 16) { g.error = "bad inverseBindMatrices"; return false; }
            for (std::size_t k = 0; k + 16 <= ib.size(); k += 16) {
                float mm[16];
                for (int q = 0; q < 16; ++q) mm[q] = static_cast<float>(ib[k + q]);
                s.inverse_bind.push_back(model_math::from_column_major(mm));
            }
        }
        s.inverse_bind.resize(s.joints.size(), model_math::identity());
        m.skins.push_back(std::move(s));
    }

    for (const json::Value& an : d["animations"].items()) {
        ModelAnimation a;
        a.name = an["name"].as_string();
        for (const json::Value& ch : an["channels"].items()) {
            const json::Value& target = ch["target"];
            const std::string path = target["path"].as_string();
            if (path != "translation" && path != "rotation" && path != "scale") continue;
            const json::Value& sm = an["samplers"][ch["sampler"].as_uint()];
            AnimChannel c;
            c.node = static_cast<int>(target["node"].as_int(-1));
            if (c.node < 0 || static_cast<std::size_t>(c.node) >= node_count) continue;
            c.path = path == "translation" ? AnimPath::Translation : path == "rotation" ? AnimPath::Rotation : AnimPath::Scale;
            const std::string interp = sm["interpolation"].as_string("LINEAR");
            c.interpolation = interp == "STEP" ? AnimInterpolation::Step : interp == "CUBICSPLINE" ? AnimInterpolation::CubicSpline : AnimInterpolation::Linear;
            std::vector<double> in, out;
            int ic = 0, oc = 0;
            if (!read_accessor(g, static_cast<int>(sm["input"].as_int()), in, ic) || !read_accessor(g, static_cast<int>(sm["output"].as_int()), out, oc)) continue;
            const int want = c.path == AnimPath::Rotation ? 4 : 3;
            if (oc != want || in.empty()) continue;
            const std::size_t per = c.interpolation == AnimInterpolation::CubicSpline ? 3 : 1;
            if (out.size() < in.size() * per * static_cast<std::size_t>(want)) continue;
            c.times.assign(in.begin(), in.end());
            c.values.assign(out.begin(), out.begin() + static_cast<std::ptrdiff_t>(in.size() * per * want));
            a.duration = std::max(a.duration, c.times.back());
            a.channels.push_back(std::move(c));
        }
        m.animations.push_back(std::move(a));
    }

    return true;
}

bool Model3D::load_gltf(const std::filesystem::path& path, Model3D& out, std::string* error, const ModelImportOptions& options) {
    auto fail = [&](const std::string& why) { if (error) *error = why; return false; };
    const auto bytes = system::paths::read_bytes(path);
    if (!bytes) return fail("cannot read " + system::paths::to_utf8(path));
    GltfContext g;
    g.dir = path.parent_path();
    std::vector<std::uint8_t> bin;
    bool glb = bytes->size() >= 12 && std::memcmp(bytes->data(), "glTF", 4) == 0;
    std::string err;

    if (glb) {
        auto u32 = [&](std::size_t o) { std::uint32_t v; std::memcpy(&v, bytes->data() + o, 4); return v; };
        const std::uint32_t total = std::min<std::uint32_t>(u32(8), static_cast<std::uint32_t>(bytes->size()));
        std::size_t off = 12;
        bool have_json = false;
        while (off + 8 <= total) {
            const std::uint32_t len = u32(off), type = u32(off + 4);
            if (off + 8 + len > total) return fail("truncated GLB chunk");
            const char* chunk = reinterpret_cast<const char*>(bytes->data() + off + 8);
            if (type == 0x4E4F534Au) { if (!json::parse(chunk, len, g.doc, &err)) return fail(err); have_json = true; }
            else if (type == 0x004E4942u) bin.assign(bytes->data() + off + 8, bytes->data() + off + 8 + len);
            off += 8 + len;
        }
        if (!have_json) return fail("GLB without JSON chunk");
    } else {
        if (!json::parse(reinterpret_cast<const char*>(bytes->data()), bytes->size(), g.doc, &err)) return fail(err);
    }

    if (!load_buffers(g, glb ? &bin : nullptr)) return fail(g.error);
    Model3D m;
    make_import(m, options);
    if (!load_gltf_doc(g, m, options)) return fail(g.error);
    m.source = system::paths::to_utf8(path);
    out = std::move(m);
    return true;
}

bool Model3D::load_obj(const std::filesystem::path& path, Model3D& out, std::string* error, const ModelImportOptions& options) {
    auto fail = [&](const std::string& why) { if (error) *error = why; return false; };
    const auto text = system::paths::read_text(path);
    if (!text) return fail("cannot read " + system::paths::to_utf8(path));
    Model3D m;
    make_import(m, options);
    std::vector<vector3d> pos, nrm;
    std::vector<std::array<double, 2>> uvs;
    std::vector<Color> vcol;
    std::unordered_map<std::string, int> material_index;
    const std::filesystem::path dir = path.parent_path();

    auto load_mtl = [&](const std::filesystem::path& file) {
        const auto mtl = system::paths::read_text(file);
        if (!mtl) return;
        std::istringstream in(*mtl);
        std::string line;
        ModelMaterial* cur = nullptr;
        while (std::getline(in, line)) {
            if (!line.empty() && line.back() == '\r') line.pop_back();
            std::istringstream ls(line);
            std::string key;
            ls >> key;
            if (key == "newmtl") {
                std::string name;
                std::getline(ls, name);
                name.erase(0, name.find_first_not_of(" \t"));
                material_index[name] = static_cast<int>(m.materials.size());
                m.materials.emplace_back();
                cur = &m.materials.back();
                cur->name = name;
                cur->metallic = 0.0f;
                cur->roughness = 0.8f;
            } else if (!cur) {
                continue;
            } else if (key == "Kd") {
                double r = 1, gg = 1, b = 1;
                ls >> r >> gg >> b;
                cur->base_color = Color(static_cast<std::uint8_t>(std::lround(std::min(1.0, r) * 255)), static_cast<std::uint8_t>(std::lround(std::min(1.0, gg) * 255)), static_cast<std::uint8_t>(std::lround(std::min(1.0, b) * 255)), cur->base_color.alpha());
            } else if (key == "d" || key == "Tr") {
                double v = 1.0;
                ls >> v;
                if (key == "Tr") v = 1.0 - v;
                cur->base_color.set_alpha(static_cast<std::uint8_t>(std::lround(std::max(0.0, std::min(1.0, v)) * 255)));
                cur->transparent = v < 0.999;
            } else if (key == "Ns") {
                double ns = 0.0;
                ls >> ns;
                cur->roughness = static_cast<float>(std::sqrt(2.0 / (std::max(0.0, ns) + 2.0)));
            } else if (key == "Pr") {
                ls >> cur->roughness;
            } else if (key == "Pm") {
                ls >> cur->metallic;
            } else if (key == "Ke") {
                double r = 0, gg = 0, b = 0;
                ls >> r >> gg >> b;
                const double mx = std::max({ r, gg, b });
                if (mx > 0.0) { cur->emissive = Color(static_cast<std::uint8_t>(std::lround(r / mx * 255)), static_cast<std::uint8_t>(std::lround(gg / mx * 255)), static_cast<std::uint8_t>(std::lround(b / mx * 255))); cur->emissive_strength = static_cast<float>(mx); }
            } else if (key == "map_Kd") {
                std::string rest;
                std::getline(ls, rest);
                std::istringstream rs(rest);
                std::string tok, file;
                while (rs >> tok) { if (tok[0] == '-') { std::string skip; if (tok == "-bm" || tok == "-mm" || tok == "-texres" || tok == "-imfchan") rs >> skip; if (tok == "-mm") rs >> skip; if (tok == "-o" || tok == "-s" || tok == "-t") { rs >> skip >> skip >> skip; } continue; } file = tok; }
                if (file.empty()) continue;
                std::string err;
                auto img = system::load_image(file.find(':') != std::string::npos || file[0] == '/' ? system::paths::from_utf8(file) : file.empty() ? std::filesystem::path() : dir / system::paths::from_utf8(file), &err);
                if (img) { cur->texture = static_cast<int>(m.textures.size()); m.textures.emplace_back(std::move(*img), options.filter, WrapMode::Repeat); }
            }
        }
    };

    ModelMeshData mesh;
    mesh.name = path.stem().string();
    int current_material = -1;
    std::unordered_map<std::string, std::uint32_t> dedupe;
    ModelPrimitive prim;
    bool has_normals = false;

    auto flush = [&]() {
        if (!prim.mesh.empty()) {
            if (!has_normals && options.generate_normals) generate_normals(prim.mesh, options.smooth_normals);
            mesh.primitives.push_back(std::move(prim));
        }
        prim = ModelPrimitive();
        prim.material = current_material;
        dedupe.clear();
        has_normals = false;
    };

    prim.material = current_material;
    std::istringstream in(*text);
    std::string line;

    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        std::istringstream ls(line);
        std::string key;
        ls >> key;
        if (key == "v") {
            double x = 0, y = 0, z = 0, r = -1, gg = -1, b = -1;
            ls >> x >> y >> z;
            pos.push_back({ x, y, z });
            if (ls >> r >> gg >> b) vcol.push_back(Color(static_cast<std::uint8_t>(std::lround(std::min(1.0, r) * 255)), static_cast<std::uint8_t>(std::lround(std::min(1.0, gg) * 255)), static_cast<std::uint8_t>(std::lround(std::min(1.0, b) * 255))));
            else vcol.push_back(Color(255, 255, 255));
        } else if (key == "vn") {
            double x = 0, y = 0, z = 0;
            ls >> x >> y >> z;
            nrm.push_back({ x, y, z });
        } else if (key == "vt") {
            double u = 0, v = 0;
            ls >> u >> v;
            uvs.push_back({ u, v });
        } else if (key == "mtllib") {
            std::string file;
            std::getline(ls, file);
            file.erase(0, file.find_first_not_of(" \t"));
            load_mtl(dir / system::paths::from_utf8(file));
        } else if (key == "usemtl") {
            std::string name;
            std::getline(ls, name);
            name.erase(0, name.find_first_not_of(" \t"));
            const auto it = material_index.find(name);
            current_material = it == material_index.end() ? -1 : it->second;
            flush();
        } else if (key == "o" || key == "g") {
            flush();
        } else if (key == "f") {
            std::vector<std::uint32_t> face;
            std::string tok;
            while (ls >> tok) {
                int vi = 0, ti = 0, ni = 0;
                const std::size_t s1 = tok.find('/');
                vi = std::atoi(tok.substr(0, s1).c_str());
                if (s1 != std::string::npos) {
                    const std::size_t s2 = tok.find('/', s1 + 1);
                    const std::string t = tok.substr(s1 + 1, s2 == std::string::npos ? std::string::npos : s2 - s1 - 1);
                    if (!t.empty()) ti = std::atoi(t.c_str());
                    if (s2 != std::string::npos) ni = std::atoi(tok.substr(s2 + 1).c_str());
                }
                if (vi < 0) vi = static_cast<int>(pos.size()) + vi + 1;
                if (ti < 0) ti = static_cast<int>(uvs.size()) + ti + 1;
                if (ni < 0) ni = static_cast<int>(nrm.size()) + ni + 1;
                if (vi <= 0 || static_cast<std::size_t>(vi) > pos.size()) return fail("face references a missing vertex");
                const std::string key2 = std::to_string(vi) + "/" + std::to_string(ti) + "/" + std::to_string(ni);
                const auto f = dedupe.find(key2);
                if (f != dedupe.end()) { face.push_back(f->second); continue; }
                Vertex3D v;
                const vector3d& p = pos[static_cast<std::size_t>(vi - 1)];
                v.x = static_cast<float>(p.x); v.y = static_cast<float>(p.y); v.z = static_cast<float>(p.z);
                if (ti > 0 && static_cast<std::size_t>(ti) <= uvs.size()) { v.u = static_cast<float>(uvs[static_cast<std::size_t>(ti - 1)][0]); v.v = static_cast<float>(1.0 - uvs[static_cast<std::size_t>(ti - 1)][1]); }
                if (ni > 0 && static_cast<std::size_t>(ni) <= nrm.size()) { const vector3d& n = nrm[static_cast<std::size_t>(ni - 1)]; v.nx = static_cast<float>(n.x); v.ny = static_cast<float>(n.y); v.nz = static_cast<float>(n.z); has_normals = true; }
                const Color base = current_material >= 0 ? m.materials[static_cast<std::size_t>(current_material)].base_color : Color(255, 255, 255);
                const Color vc = vcol[static_cast<std::size_t>(vi - 1)];
                v.rgba = Vertex3D::pack(Color(static_cast<std::uint8_t>(base.red() * vc.red() / 255), static_cast<std::uint8_t>(base.green() * vc.green() / 255), static_cast<std::uint8_t>(base.blue() * vc.blue() / 255), base.alpha()));
                const std::uint32_t id = static_cast<std::uint32_t>(prim.mesh.add_vertex(v));
                dedupe[key2] = id;
                face.push_back(id);
            }
            for (std::size_t k = 2; k < face.size(); ++k) prim.mesh.add_triangle(face[0], face[k - 1], face[k]);
        }
    }

    flush();
    if (mesh.primitives.empty()) return fail("OBJ has no faces");
    m.meshes.push_back(std::move(mesh));
    ModelNode node;
    node.name = m.meshes.back().name;
    node.mesh = 0;
    m.nodes.push_back(node);
    m.roots.push_back(0);
    m.source = system::paths::to_utf8(path);
    out = std::move(m);
    return true;
}

bool Model3D::load(const std::filesystem::path& path, Model3D& out, std::string* error, const ModelImportOptions& options) {
    const std::string ext = lower(path.extension().string());
    if (ext == ".obj") return load_obj(path, out, error, options);
    if (ext == ".gltf" || ext == ".glb") return load_gltf(path, out, error, options);
    if (error) *error = "unsupported model format: " + ext;
    return false;
}

auto Model3D::texture_of(int material) const noexcept -> const Texture* {
    if (material < 0 || static_cast<std::size_t>(material) >= materials.size()) return nullptr;
    const int t = materials[static_cast<std::size_t>(material)].texture;
    return t >= 0 && static_cast<std::size_t>(t) < textures.size() ? &textures[static_cast<std::size_t>(t)] : nullptr;
}

auto Model3D::material3d(int material, const Material3D* override_material) const noexcept -> Material3D {
    Material3D out = override_material ? *override_material : Material3D{};
    if (material < 0 || static_cast<std::size_t>(material) >= materials.size()) { if (!override_material) out.pbr = true; return out; }
    const ModelMaterial& mm = materials[static_cast<std::size_t>(material)];
    if (!out.texture) out.texture = texture_of(material);
    if (override_material) return out;
    out.pbr = !mm.unlit;
    out.lit = !mm.unlit;
    out.metallic = mm.metallic;
    out.roughness = mm.roughness;
    out.emissive = mm.emissive;
    out.emissive_strength = mm.emissive_strength;
    if (mm.double_sided) out.cull = Cull3D::None;
    if (mm.transparent) { out.blend = Blend3D::Alpha; out.depth_write = false; out.shadow = Shadow3D::None; }
    return out;
}

auto Model3D::rest_pose() const -> std::vector<NodePose> {
    std::vector<NodePose> p(nodes.size());
    for (std::size_t i = 0; i < nodes.size(); ++i) { p[i].translation = nodes[i].translation; p[i].rotation = nodes[i].rotation; p[i].scale = nodes[i].scale; }
    return p;
}

void Model3D::world_matrices(const std::vector<NodePose>& pose, std::vector<model_math::Mat4>& world) const {
    world.assign(nodes.size(), model_math::identity());
    std::vector<char> done(nodes.size(), 0);
    std::vector<int> stack;
    for (int r : roots) stack.push_back(r);
    for (std::size_t i = 0; i < nodes.size(); ++i) if (nodes[i].parent < 0 && std::find(roots.begin(), roots.end(), static_cast<int>(i)) == roots.end()) stack.push_back(static_cast<int>(i));

    while (!stack.empty()) {
        const int i = stack.back();
        stack.pop_back();
        if (i < 0 || static_cast<std::size_t>(i) >= nodes.size() || done[static_cast<std::size_t>(i)]) continue;
        done[static_cast<std::size_t>(i)] = 1;
        const ModelNode& n = nodes[static_cast<std::size_t>(i)];
        const NodePose& np = pose[static_cast<std::size_t>(i)];
        const model_math::Mat4 local = n.has_matrix ? n.matrix : model_math::trs(np.translation, np.rotation, np.scale);
        world[static_cast<std::size_t>(i)] = n.parent >= 0 ? model_math::mul(world[static_cast<std::size_t>(n.parent)], local) : model_math::mul(import_transform, local);
        for (int c : n.children) stack.push_back(c);
    }
}

int Model3D::find_node(const std::string& name) const noexcept {
    for (std::size_t i = 0; i < nodes.size(); ++i) if (nodes[i].name == name) return static_cast<int>(i);
    return -1;
}

int Model3D::find_animation(const std::string& name) const noexcept {
    for (std::size_t i = 0; i < animations.size(); ++i) if (animations[i].name == name) return static_cast<int>(i);
    return -1;
}

std::size_t Model3D::vertex_count() const noexcept {
    std::size_t n = 0;
    for (const auto& m : meshes) for (const auto& p : m.primitives) n += p.mesh.vertices().size();
    return n;
}

void Model3D::bounds(vector3d& lo, vector3d& hi) const {
    std::vector<model_math::Mat4> world;
    world_matrices(rest_pose(), world);
    lo = { 1e300, 1e300, 1e300 };
    hi = { -1e300, -1e300, -1e300 };
    for (std::size_t i = 0; i < nodes.size(); ++i) {
        if (nodes[i].mesh < 0 || static_cast<std::size_t>(nodes[i].mesh) >= meshes.size()) continue;
        for (const auto& p : meshes[static_cast<std::size_t>(nodes[i].mesh)].primitives) {
            for (const Vertex3D& v : p.mesh.vertices()) {
                const vector3d w = model_math::point(world[i], v.x, v.y, v.z);
                lo = { std::min(lo.x, w.x), std::min(lo.y, w.y), std::min(lo.z, w.z) };
                hi = { std::max(hi.x, w.x), std::max(hi.y, w.y), std::max(hi.z, w.z) };
            }
        }
    }
}

void Model3D::draw(windows::Renderer& r, const math::Matrix4d& transform, const Material3D* override_material) const {
    std::vector<model_math::Mat4> world;
    world_matrices(rest_pose(), world);
    const model_math::Mat4 base = model_math::from_matrix(transform);
    for (std::size_t i = 0; i < nodes.size(); ++i) {
        if (nodes[i].mesh < 0 || static_cast<std::size_t>(nodes[i].mesh) >= meshes.size()) continue;
        const math::Matrix4d m = model_math::to_matrix(model_math::mul(base, world[i]));
        for (const auto& p : meshes[static_cast<std::size_t>(nodes[i].mesh)].primitives) r.draw_mesh(p.mesh, m, material3d(p.material, override_material));
    }
}

double ModelAnimator::sample_index(const std::vector<float>& times, double t, std::size_t& k) noexcept {
    if (times.size() < 2 || t <= times.front()) { k = 0; return 0.0; }
    if (t >= times.back()) { k = times.size() - 1; return 0.0; }
    const auto it = std::upper_bound(times.begin(), times.end(), static_cast<float>(t));
    k = static_cast<std::size_t>(it - times.begin()) - 1;
    const double span = times[k + 1] - times[k];
    return span > 0.0 ? (t - times[k]) / span : 0.0;
}

void ModelAnimator::sample_channel(const AnimChannel& c, double t, NodePose& out) {
    const int comps = c.path == AnimPath::Rotation ? 4 : 3;
    std::size_t k = 0;
    const double f = sample_index(c.times, t, k);
    const bool last = k + 1 >= c.times.size();
    double v[4] = { 0, 0, 0, 1 };

    if (c.interpolation == AnimInterpolation::CubicSpline) {
        const std::size_t stride = static_cast<std::size_t>(comps) * 3;
        const float* p0 = &c.values[k * stride + comps];
        if (last || f == 0.0) { for (int i = 0; i < comps; ++i) v[i] = p0[i]; }
        else {
            const float* b0 = &c.values[k * stride + 2 * comps];
            const float* a1 = &c.values[(k + 1) * stride];
            const float* p1 = &c.values[(k + 1) * stride + comps];
            const double dt = c.times[k + 1] - c.times[k];
            const double t2 = f * f, t3 = t2 * f;
            const double h00 = 2 * t3 - 3 * t2 + 1, h10 = t3 - 2 * t2 + f, h01 = -2 * t3 + 3 * t2, h11 = t3 - t2;
            for (int i = 0; i < comps; ++i) v[i] = h00 * p0[i] + h10 * dt * b0[i] + h01 * p1[i] + h11 * dt * a1[i];
        }
    } else {
        const float* a = &c.values[k * comps];
        if (last || c.interpolation == AnimInterpolation::Step || f == 0.0) { for (int i = 0; i < comps; ++i) v[i] = a[i]; }
        else if (c.path == AnimPath::Rotation) {
            const float* b = &c.values[(k + 1) * comps];
            const model_math::Quat q = model_math::slerp({ a[0], a[1], a[2], a[3] }, { b[0], b[1], b[2], b[3] }, f);
            v[0] = q.x; v[1] = q.y; v[2] = q.z; v[3] = q.w;
        } else {
            const float* b = &c.values[(k + 1) * comps];
            for (int i = 0; i < comps; ++i) v[i] = a[i] + (b[i] - a[i]) * f;
        }
    }

    switch (c.path) {
        case AnimPath::Translation: out.translation = { v[0], v[1], v[2] }; break;
        case AnimPath::Rotation:    out.rotation = model_math::normalize({ v[0], v[1], v[2], v[3] }); break;
        case AnimPath::Scale:       out.scale = { v[0], v[1], v[2] }; break;
    }
}

void ModelAnimator::apply(const Track& tr, std::vector<NodePose>& pose) const {
    if (!m_model || tr.animation < 0 || static_cast<std::size_t>(tr.animation) >= m_model->animations.size()) return;
    for (const AnimChannel& c : m_model->animations[static_cast<std::size_t>(tr.animation)].channels) {
        if (c.node >= 0 && static_cast<std::size_t>(c.node) < pose.size()) sample_channel(c, tr.time, pose[static_cast<std::size_t>(c.node)]);
    }
}

void ModelAnimator::advance(Track& tr, double duration, double dt, bool& finished) {
    tr.time += dt * tr.speed;
    if (duration <= 0.0) { tr.time = 0.0; return; }
    if (tr.loop) { tr.time = std::fmod(tr.time, duration); if (tr.time < 0.0) tr.time += duration; }
    else if (tr.time >= duration) { tr.time = duration; finished = true; }
    else if (tr.time < 0.0) { tr.time = 0.0; finished = true; }
}

void ModelAnimator::skin() {
    if (!m_model) return;
    m_skinned.resize(m_model->meshes.size());
    for (std::size_t ni = 0; ni < m_model->nodes.size(); ++ni) {
        const ModelNode& node = m_model->nodes[ni];
        if (node.mesh < 0 || node.skin < 0 || static_cast<std::size_t>(node.skin) >= m_model->skins.size() || static_cast<std::size_t>(node.mesh) >= m_model->meshes.size()) continue;
        const ModelSkin& skin = m_model->skins[static_cast<std::size_t>(node.skin)];
        std::vector<model_math::Mat4> joints(skin.joints.size());
        for (std::size_t j = 0; j < skin.joints.size(); ++j) {
            const int jn = skin.joints[j];
            joints[j] = jn >= 0 && static_cast<std::size_t>(jn) < m_world.size() ? model_math::mul(m_world[static_cast<std::size_t>(jn)], skin.inverse_bind[j]) : model_math::identity();
        }
        const ModelMeshData& md = m_model->meshes[static_cast<std::size_t>(node.mesh)];
        auto& out = m_skinned[static_cast<std::size_t>(node.mesh)];
        out.resize(md.primitives.size());

        for (std::size_t pi = 0; pi < md.primitives.size(); ++pi) {
            const ModelPrimitive& prim = md.primitives[pi];
            if (!prim.skinned()) continue;
            Mesh3D& dst = out[pi];
            const auto& src = prim.mesh.vertices();
            if (dst.vertices().size() != src.size()) { dst.edit_vertices() = src; dst.edit_indices() = prim.mesh.indices(); }
            auto& dv = dst.edit_vertices();
            auto body = [&](std::size_t b, std::size_t e) {
                for (std::size_t i = b; i < e; ++i) {
                    const Vertex3D& v = src[i];
                    double m[12] = { 0 };
                    for (int k = 0; k < 4; ++k) {
                        const float w = prim.weights[i][k];
                        if (w == 0.0f) continue;
                        const std::uint16_t jidx = prim.joints[i][k];
                        if (jidx >= joints.size()) continue;
                        const model_math::Mat4& jm = joints[jidx];
                        for (int q = 0; q < 12; ++q) m[q] += jm[static_cast<std::size_t>(q)] * w;
                    }
                    Vertex3D o = v;
                    o.x = static_cast<float>(m[0] * v.x + m[1] * v.y + m[2] * v.z + m[3]);
                    o.y = static_cast<float>(m[4] * v.x + m[5] * v.y + m[6] * v.z + m[7]);
                    o.z = static_cast<float>(m[8] * v.x + m[9] * v.y + m[10] * v.z + m[11]);
                    const double nx = m[0] * v.nx + m[1] * v.ny + m[2] * v.nz, ny = m[4] * v.nx + m[5] * v.ny + m[6] * v.nz, nz = m[8] * v.nx + m[9] * v.ny + m[10] * v.nz;
                    const double nl = std::sqrt(nx * nx + ny * ny + nz * nz);
                    if (nl > 0.0) { o.nx = static_cast<float>(nx / nl); o.ny = static_cast<float>(ny / nl); o.nz = static_cast<float>(nz / nl); }
                    dv[i] = o;
                }
            };
            if (src.size() >= 8192) system::JobSystem::instance().parallel_for(std::size_t(0), src.size(), std::size_t(2048), [&](std::size_t b, std::size_t e) { body(b, e); });
            else body(0, src.size());
        }
    }
}

void ModelAnimator::attach(const Model3D& model) {
    m_model = &model;
    m_pose = model.rest_pose();
    m_current = Track{};
    m_previous = Track{};
    m_skinned.clear();
    refresh();
}

bool ModelAnimator::play(int animation, bool loop, double fade_seconds, double speed) {
    if (!m_model || animation < 0 || static_cast<std::size_t>(animation) >= m_model->animations.size()) return false;
    if (fade_seconds > 0.0 && m_current.animation >= 0) { m_previous = m_current; m_fade = fade_seconds; m_fade_elapsed = 0.0; }
    else { m_previous = Track{}; m_fade = 0.0; }
    m_current = Track{ animation, speed < 0.0 ? m_model->animations[static_cast<std::size_t>(animation)].duration : 0.0, speed, loop, 1.0 };
    m_finished = false;
    return true;
}

double ModelAnimator::duration() const noexcept {
    return m_model && m_current.animation >= 0 ? m_model->animations[static_cast<std::size_t>(m_current.animation)].duration : 0.0;
}

auto ModelAnimator::node_position(int node) const noexcept -> vector3d {
    if (node < 0 || static_cast<std::size_t>(node) >= m_world.size()) return { 0.0, 0.0, 0.0 };
    const auto& w = m_world[static_cast<std::size_t>(node)];
    return { w[3], w[7], w[11] };
}

void ModelAnimator::update(double dt) {
    if (!m_model) return;
    m_pose = m_model->rest_pose();

    if (m_current.animation >= 0) {
        advance(m_current, m_model->animations[static_cast<std::size_t>(m_current.animation)].duration, dt, m_finished);
        apply(m_current, m_pose);
    }

    if (m_previous.animation >= 0 && m_fade > 0.0) {
        bool ignored = false;
        advance(m_previous, m_model->animations[static_cast<std::size_t>(m_previous.animation)].duration, dt, ignored);
        m_fade_elapsed += dt;
        const double w = std::min(1.0, m_fade_elapsed / m_fade);
        m_scratch = m_model->rest_pose();
        apply(m_previous, m_scratch);
        for (std::size_t i = 0; i < m_pose.size(); ++i) {
            NodePose& a = m_pose[i];
            const NodePose& b = m_scratch[i];
            a.translation = b.translation + (a.translation - b.translation) * w;
            a.scale = b.scale + (a.scale - b.scale) * w;
            a.rotation = model_math::slerp(b.rotation, a.rotation, w);
        }
        if (w >= 1.0) { m_previous = Track{}; m_fade = 0.0; }
    }

    refresh();
}

void ModelAnimator::draw(windows::Renderer& r, const math::Matrix4d& transform, const Material3D* override_material) const {
    if (!m_model) return;
    const model_math::Mat4 base = model_math::from_matrix(transform);
    const math::Matrix4d root = model_math::to_matrix(base);

    for (std::size_t i = 0; i < m_model->nodes.size(); ++i) {
        const ModelNode& node = m_model->nodes[i];
        if (node.mesh < 0 || static_cast<std::size_t>(node.mesh) >= m_model->meshes.size()) continue;
        const auto& prims = m_model->meshes[static_cast<std::size_t>(node.mesh)].primitives;
        const math::Matrix4d m = model_math::to_matrix(model_math::mul(base, m_world[i]));

        for (std::size_t p = 0; p < prims.size(); ++p) {
            const Material3D mat = m_model->material3d(prims[p].material, override_material);
            const bool skinned = node.skin >= 0 && prims[p].skinned() && static_cast<std::size_t>(node.mesh) < m_skinned.size() && p < m_skinned[static_cast<std::size_t>(node.mesh)].size();
            if (skinned) r.draw_mesh(m_skinned[static_cast<std::size_t>(node.mesh)][p], root, mat);
            else r.draw_mesh(prims[p].mesh, m, mat);
        }
    }
}

} // namespace graphics
} // namespace fizmo
