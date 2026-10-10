#include "../Source/System/process.hpp"
#include "../Source/Util Hpp/json.hpp"

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <regex>
#include <set>
#include <sstream>
#include <string>
#include <tuple>
#include <vector>

#ifndef FIZMO_TOOL_SOURCE_DIR
#define FIZMO_TOOL_SOURCE_DIR "."
#endif

namespace {

namespace fs = std::filesystem;
namespace json = fizmo::json;

constexpr std::size_t kChunk          = 12000;
constexpr int         kVersion2D      = 330;
constexpr int         kVersion3D      = 410;
constexpr int         kVersionCompute = 430;

const char* const kExtraNamespace = "fizmo::windows::detail::extra_glsl";
const char* const kSpirvNamespace = "::fizmo::windows::detail::gfx";
const char* const kDepthFixup     = "#ifdef FIZMO_GL_DEPTH_FIXUP\n    gl_Position.z = 2.0 * gl_Position.z - gl_Position.w;\n#endif\n";

const std::set<std::string> kShaders2D = { "draw2d.vert", "draw2d.frag", "present.vert", "present.frag", "upscale.frag" };

struct Failure {
    std::string message;
};

[[noreturn]] void fail(const std::string& message) {
    throw Failure{ message };
}

struct Binding {
    std::string   name;
    std::string   kind;
    std::uint32_t set;
    std::uint32_t binding;
    std::uint32_t count;
};

struct Member {
    std::string   name;
    std::uint32_t offset, base, rows, cols, count, stride, matrix_stride;
};

struct Block {
    std::string         name;
    std::uint32_t       kind, set, binding, size;
    std::vector<Member> members;
};

struct Input {
    std::string   name;
    std::uint32_t location, base, rows;
};

struct Entry {
    std::string                name;
    std::string                symbol;
    int                        version = 0;
    std::string                glsl;
    std::vector<Binding>       bindings;
    std::vector<std::uint32_t> words;
    std::vector<Block>         blocks;
    std::vector<Input>         inputs;
};

std::string extension_of(const std::string& name) {
    const std::size_t dot = name.rfind('.');
    return dot == std::string::npos ? name : name.substr(dot + 1);
}

bool is_stage(const std::string& ext) {
    return ext == "vert" || ext == "frag" || ext == "comp";
}

std::string stage_of(const std::string& name) {
    const std::string ext = extension_of(name);
    if (!is_stage(ext)) fail("cannot tell the shader stage of " + name + " (expected .vert, .frag or .comp)");
    return ext;
}

std::string stage_enum(const std::string& stage) {
    if (stage == "vert") return "Vertex";
    if (stage == "frag") return "Fragment";
    return "Compute";
}

std::uint32_t stage_bits(const std::string& stage) {
    if (stage == "vert") return 1;
    if (stage == "frag") return 2;
    return 4;
}

int renderer_version(const std::string& name) {
    if (name.size() >= 5 && name.compare(name.size() - 5, 5, ".comp") == 0) return kVersionCompute;
    return kShaders2D.count(name) ? kVersion2D : kVersion3D;
}

std::string tool(const char* env, const std::string& fallback) {
    const char* value = std::getenv(env);
    const std::string path = (value && *value) ? value : fallback;
    std::error_code ec;
    const bool has_dir = path.find('/') != std::string::npos || path.find('\\') != std::string::npos;
    const bool found = has_dir ? fs::is_regular_file(path, ec) : !fizmo::system::find_program(path).empty();
    if (!found) fail(fallback + " was not found; install it or set " + env + " to its path");
    return path;
}

std::string run(const std::string& program, const std::vector<std::string>& args, const std::string& what) {
    const fizmo::system::ProcessResult r = fizmo::system::run_process(program, args, false);
    if (!r.started || r.exit_code != 0) fail(what + " failed:\n" + r.output);
    return r.output;
}

std::string normalize_newlines(const std::string& text) {
    std::string out;
    out.reserve(text.size());

    for (std::size_t i = 0; i < text.size(); ++i) {
        if (text[i] == '\r') {
            out += '\n';
            if (i + 1 < text.size() && text[i + 1] == '\n') ++i;
        } else {
            out += text[i];
        }
    }

    return out;
}

std::string read_text(const fs::path& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) fail("cannot read " + path.string());
    std::ostringstream ss;
    ss << f.rdbuf();
    return normalize_newlines(ss.str());
}

void write_text(const fs::path& path, const std::string& text) {
    std::ofstream f(path, std::ios::binary | std::ios::trunc);
    if (!f) fail("cannot write " + path.string());
    f << text;
}

void write_words(const fs::path& path, const std::vector<std::uint32_t>& words) {
    std::ofstream f(path, std::ios::binary | std::ios::trunc);
    if (!f) fail("cannot write " + path.string());

    for (std::uint32_t w : words) {
        const unsigned char b[4] = { static_cast<unsigned char>(w), static_cast<unsigned char>(w >> 8), static_cast<unsigned char>(w >> 16), static_cast<unsigned char>(w >> 24) };
        f.write(reinterpret_cast<const char*>(b), 4);
    }
}

std::vector<std::uint32_t> read_words(const fs::path& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) fail("cannot read " + path.string());
    std::vector<char> data((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    if (data.size() % 4) fail(path.string() + " is not a SPIR-V binary");
    std::vector<std::uint32_t> words(data.size() / 4);

    for (std::size_t i = 0; i < words.size(); ++i) {
        const unsigned char* p = reinterpret_cast<const unsigned char*>(&data[i * 4]);
        words[i] = std::uint32_t(p[0]) | (std::uint32_t(p[1]) << 8) | (std::uint32_t(p[2]) << 16) | (std::uint32_t(p[3]) << 24);
    }

    return words;
}

std::string translate(const std::string& cross, const fs::path& spv, const std::string& name, int version) {
    return run(cross, { spv.string(), "--version", std::to_string(version), "--no-es", "--no-420pack-extension", "--glsl-emit-push-constant-as-ubo" }, "spirv-cross on " + name);
}

json::Value reflect_json(const std::string& cross, const fs::path& spv, const std::string& name) {
    const std::string text = run(cross, { spv.string(), "--reflect" }, "spirv-cross --reflect on " + name);
    json::Value v;
    std::string error;
    if (!json::parse(text, v, &error)) fail("spirv-cross --reflect on " + name + " gave unreadable output: " + error);
    return v;
}

std::int64_t get_int(const json::Value& v, const char* key, std::int64_t fallback) {
    return v.has(key) ? v[key].as_int(fallback) : fallback;
}

std::string get_string(const json::Value& v, const char* key, const std::string& fallback) {
    return v.has(key) ? v[key].as_string(fallback) : fallback;
}

const json::Value& type_of(const json::Value& types, const std::string& id) {
    return types[id];
}

std::uint32_t total_count(const json::Value& entry) {
    std::int64_t total = 1;
    for (const json::Value& n : entry["array"].items()) total *= std::max<std::int64_t>(n.as_int(), 1);
    return static_cast<std::uint32_t>(total);
}

std::vector<Binding> reflect(const std::string& cross, const fs::path& spv, const std::string& name) {
    const json::Value data = reflect_json(cross, spv, name);
    const json::Value& types = data["types"];
    std::vector<Binding> out;

    for (const json::Value& t : data["textures"].items())
        out.push_back({ t["name"].as_string(), "Texture", std::uint32_t(get_int(t, "set", 0)), std::uint32_t(get_int(t, "binding", 0)), total_count(t) });
    for (const json::Value& u : data["ubos"].items())
        out.push_back({ u["name"].as_string(), "UniformBlock", std::uint32_t(get_int(u, "set", 0)), std::uint32_t(get_int(u, "binding", 0)), total_count(u) });

    for (const json::Value& p : data["push_constants"].items()) {
        const std::string type_id = p["type"].as_string();
        const std::string block = types.has(type_id) ? get_string(type_of(types, type_id), "name", p["name"].as_string()) : p["name"].as_string();
        out.push_back({ block, "PushBlock", 0, 0, 1 });
    }

    for (const json::Value& i : data["images"].items())
        out.push_back({ i["name"].as_string(), "StorageImage", std::uint32_t(get_int(i, "set", 0)), std::uint32_t(get_int(i, "binding", 0)), total_count(i) });
    for (const json::Value& s : data["ssbos"].items())
        out.push_back({ s["name"].as_string(), "StorageBlock", std::uint32_t(get_int(s, "set", 0)), std::uint32_t(get_int(s, "binding", 0)), total_count(s) });

    if (data["separate_images"].size() || data["separate_samplers"].size())
        fail(name + " uses separate images or samplers; use combined image samplers");

    return out;
}

std::tuple<std::uint32_t, std::uint32_t, std::uint32_t> parse_type(const std::string& t) {
    static const std::regex vec_re("^(d|i|u|b)?vec([234])$");
    static const std::regex mat_re("^d?mat([234])(?:x([234]))?$");
    std::smatch m;

    if (std::regex_match(t, m, vec_re)) {
        const std::string p = m[1].matched ? m[1].str() : "";
        const std::uint32_t base = p.empty() ? 1 : p == "d" ? 5 : p == "i" ? 2 : p == "u" ? 3 : 4;
        return { base, std::uint32_t(std::stoi(m[2].str())), 1 };
    }

    if (std::regex_match(t, m, mat_re)) {
        const std::uint32_t cols = std::uint32_t(std::stoi(m[1].str()));
        const std::uint32_t rows = m[2].matched ? std::uint32_t(std::stoi(m[2].str())) : cols;
        return { t[0] == 'd' ? 5u : 1u, rows, cols };
    }

    if (t == "float")  return { 1, 1, 1 };
    if (t == "int")    return { 2, 1, 1 };
    if (t == "uint")   return { 3, 1, 1 };
    if (t == "bool")   return { 4, 1, 1 };
    if (t == "double") return { 5, 1, 1 };
    return { 0, 0, 0 };
}

void flatten_members(const json::Value& types, const std::string& type_id, const std::string& prefix, std::int64_t base_offset, std::vector<Member>& out) {
    if (!types.has(type_id)) return;

    for (const json::Value& member : type_of(types, type_id)["members"].items()) {
        const std::string name = prefix + member["name"].as_string();
        const std::int64_t offset = base_offset + get_int(member, "offset", 0);
        const json::Value::Array& array = member["array"].items();
        std::int64_t count = 1;
        for (const json::Value& n : array) count *= n.as_int() ? std::max<std::int64_t>(n.as_int(), 1) : 0;
        const std::int64_t stride = get_int(member, "array_stride", 0);
        const std::string mtype = member["type"].as_string();

        if (types.has(mtype)) {
            if (!array.empty() && count > 0) {
                for (std::int64_t i = 0; i < count; ++i) flatten_members(types, mtype, name + "[" + std::to_string(i) + "].", offset + i * stride, out);
            } else if (array.empty()) {
                flatten_members(types, mtype, name + ".", offset, out);
            }
            continue;
        }

        const auto [base, rows, cols] = parse_type(mtype);
        out.push_back({ name, std::uint32_t(offset), base, rows, cols, std::uint32_t(array.empty() ? 1 : count), std::uint32_t(stride), std::uint32_t(get_int(member, "matrix_stride", 0)) });
    }
}

void reflect_layout(const std::string& cross, const fs::path& spv, const std::string& name, std::vector<Block>& blocks, std::vector<Input>& inputs) {
    const json::Value data = reflect_json(cross, spv, name);
    const json::Value& types = data["types"];
    const std::pair<const char*, std::uint32_t> kinds[] = { { "ubos", 0 }, { "ssbos", 2 }, { "push_constants", 1 } };

    for (const auto& kind : kinds) {
        for (const json::Value& b : data[kind.first].items()) {
            std::vector<Member> members;
            const std::string type_id = b["type"].as_string();
            flatten_members(types, type_id, "", 0, members);
            const bool push = kind.second == 1;
            const std::string block_name = !push ? b["name"].as_string() : (types.has(type_id) ? get_string(type_of(types, type_id), "name", b["name"].as_string()) : b["name"].as_string());
            std::int64_t size = 0;

            if (b.has("block_size")) {
                size = b["block_size"].as_int();
            } else {
                for (const Member& m : members) size = std::max<std::int64_t>(size, m.offset + std::max<std::int64_t>(m.rows, 1) * 4 * std::max<std::int64_t>(m.cols, 1));
            }

            const std::uint32_t set_index = push ? 0 : std::uint32_t(get_int(b, "set", 0));
            const std::uint32_t binding   = push ? 0 : std::uint32_t(get_int(b, "binding", 0));
            blocks.push_back({ block_name, kind.second, set_index, binding, std::uint32_t(size), std::move(members) });
        }
    }

    for (const json::Value& i : data["inputs"].items()) {
        const auto [base, rows, cols] = parse_type(i["type"].as_string());
        (void)cols;
        inputs.push_back({ i["name"].as_string(), std::uint32_t(get_int(i, "location", 0)), base, rows });
    }
}

void put_u32(std::string& out, std::uint32_t v) {
    out += char(v & 0xff);
    out += char((v >> 8) & 0xff);
    out += char((v >> 16) & 0xff);
    out += char((v >> 24) & 0xff);
}

void put_string(std::string& out, const std::string& s) {
    put_u32(out, std::uint32_t(s.size()));
    out += s;
}

std::uint32_t binding_kind(const std::string& kind) {
    if (kind == "Texture")      return 0;
    if (kind == "UniformBlock") return 1;
    if (kind == "PushBlock")    return 2;
    if (kind == "StorageImage") return 3;
    return 4;
}

void write_bundle(const fs::path& path, const Entry& e) {
    std::string out = "FZSH";
    put_u32(out, 1);
    put_u32(out, stage_bits(stage_of(e.name)));
    put_u32(out, std::uint32_t(e.version));
    put_string(out, e.name);
    put_string(out, e.glsl);
    put_u32(out, std::uint32_t(e.bindings.size()));

    for (const Binding& b : e.bindings) {
        put_string(out, b.name);
        put_u32(out, binding_kind(b.kind));
        put_u32(out, b.set);
        put_u32(out, b.binding);
        put_u32(out, b.count);
    }

    put_u32(out, std::uint32_t(e.blocks.size()));

    for (const Block& blk : e.blocks) {
        put_string(out, blk.name);
        put_u32(out, blk.kind);
        put_u32(out, blk.set);
        put_u32(out, blk.binding);
        put_u32(out, blk.size);
        put_u32(out, std::uint32_t(blk.members.size()));

        for (const Member& m : blk.members) {
            put_string(out, m.name);
            for (std::uint32_t v : { m.offset, m.base, m.rows, m.cols, m.count, m.stride, m.matrix_stride }) put_u32(out, v);
        }
    }

    put_u32(out, std::uint32_t(e.inputs.size()));

    for (const Input& i : e.inputs) {
        put_string(out, i.name);
        put_u32(out, i.location);
        put_u32(out, i.base);
        put_u32(out, i.rows);
    }

    put_u32(out, std::uint32_t(e.words.size()));
    for (std::uint32_t w : e.words) put_u32(out, w);
    const fs::path tmp = path.string() + ".tmp";
    write_text(tmp, out);
    std::error_code ec;
    fs::rename(tmp, path, ec);
    if (ec) fail("cannot write " + path.string());
}

bool is_space(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f';
}

std::string strip(const std::string& s) {
    std::size_t a = 0, b = s.size();
    while (a < b && is_space(s[a])) ++a;
    while (b > a && is_space(s[b - 1])) --b;
    return s.substr(a, b - a);
}

std::string rstrip(const std::string& s) {
    std::size_t b = s.size();
    while (b > 0 && is_space(s[b - 1])) --b;
    return s.substr(0, b);
}

std::vector<std::string> split_lines(const std::string& text, bool keep_ends) {
    std::vector<std::string> out;
    std::size_t start = 0;

    for (std::size_t i = 0; i < text.size(); ++i) {
        if (text[i] == '\n') {
            out.push_back(text.substr(start, keep_ends ? i + 1 - start : i - start));
            start = i + 1;
        }
    }

    if (start < text.size()) out.push_back(text.substr(start));
    return out;
}

std::string clean(const std::string& name, const std::string& text) {
    std::vector<std::string> lines;

    for (const std::string& line : split_lines(normalize_newlines(text), false)) {
        if (line.compare(0, 8, "#version") == 0) continue;
        if (line.find("shading_language_420pack") != std::string::npos) continue;
        lines.push_back(line);
    }

    std::size_t first = 0;
    while (first < lines.size() && (strip(lines[first]).empty() || strip(lines[first]) == "#endif")) ++first;
    std::string joined;

    for (std::size_t i = first; i < lines.size(); ++i) {
        if (i > first) joined += '\n';
        joined += lines[i];
    }

    std::string body = strip(joined) + "\n";
    body = std::regex_replace(body, std::regex(R"(\bbinding\s*=\s*\d+\s*,\s*)"), "");
    body = std::regex_replace(body, std::regex(R"(,\s*binding\s*=\s*\d+\s*)"), "");
    body = std::regex_replace(body, std::regex(R"(layout\(\s*binding\s*=\s*\d+\s*\)\s*)"), "");

    if (extension_of(name) == "vert") {
        const std::size_t end = rstrip(body).rfind('}');
        if (end == std::string::npos) fail("no main body in " + name);
        body = body.substr(0, end) + kDepthFixup + body.substr(end);
    }

    return body;
}

std::vector<std::string> chunks(const std::string& text) {
    std::vector<std::string> pieces;
    std::string current;

    for (const std::string& line : split_lines(text, true)) {
        if (!current.empty() && current.size() + line.size() > kChunk) {
            pieces.push_back(current);
            current.clear();
        }
        current += line;
    }

    if (!current.empty()) pieces.push_back(current);
    return pieces;
}

bool is_alnum(char c) {
    return (c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
}

std::string symbol_for(const std::string& name) {
    std::string out = "k";
    bool start = true;

    for (char c : name) {
        if (!is_alnum(c)) { start = true; continue; }
        out += start && c >= 'a' && c <= 'z' ? char(c - 'a' + 'A') : c;
        start = false;
    }

    return out;
}

std::string function_for(const std::string& name) {
    std::string ident;
    for (char c : name) ident += (is_alnum(c) || c == '_') ? c : '_';
    return (!ident.empty() && ident[0] >= '0' && ident[0] <= '9') ? "_" + ident : ident;
}

std::string format_hex(std::uint32_t w) {
    char buf[16];
    std::snprintf(buf, sizeof(buf), "0x%08x", w);
    return buf;
}

void emit_shader(std::vector<std::string>& out, const Entry& e, const std::string& spirv_symbol) {
    out.push_back("inline constexpr int " + e.symbol + "Version = " + std::to_string(e.version) + ";");
    out.push_back("");
    out.push_back("inline constexpr const char* " + e.symbol + "[] = {");
    for (const std::string& piece : chunks(e.glsl)) out.push_back("R\"fizmo_glsl(" + piece + ")fizmo_glsl\",");
    out.push_back("};");
    out.push_back("");
    out.push_back("inline constexpr ::fizmo::gpu::GlslBinding " + e.symbol + "Bindings[] = {");

    for (const Binding& b : e.bindings)
        out.push_back("    { \"" + b.name + "\", ::fizmo::gpu::GlslResource::" + b.kind + ", " + std::to_string(b.set) + ", " + std::to_string(b.binding) + ", " + std::to_string(b.count) + " },");

    if (e.bindings.empty()) out.push_back("    { nullptr, ::fizmo::gpu::GlslResource::Texture, 0, 0, 0 },");
    out.push_back("};");
    out.push_back("");
    out.push_back("inline ::fizmo::gpu::ShaderDesc " + function_for(e.name) + "() noexcept {");
    out.push_back("    ::fizmo::gpu::ShaderDesc d;");
    out.push_back("    d.stage         = ::fizmo::gpu::ShaderStage::" + stage_enum(stage_of(e.name)) + ";");
    out.push_back("    d.spirv         = { " + spirv_symbol + ", sizeof(" + spirv_symbol + ") / sizeof(std::uint32_t) };");
    out.push_back("    d.glsl          = " + e.symbol + ";");
    out.push_back("    d.glsl_pieces   = sizeof(" + e.symbol + ") / sizeof(" + e.symbol + "[0]);");
    out.push_back("    d.glsl_version  = " + e.symbol + "Version;");
    out.push_back("    d.glsl_bindings = { " + e.symbol + "Bindings, " + std::to_string(e.bindings.size()) + " };");
    out.push_back("    d.name          = \"" + e.name + "\";");
    out.push_back("    return d;");
    out.push_back("}");
    out.push_back("");
}

std::vector<std::string> split_namespace(const std::string& ns) {
    std::vector<std::string> out;
    std::size_t start = 0;

    while (start <= ns.size()) {
        const std::size_t sep = ns.find("::", start);
        const std::string part = ns.substr(start, sep == std::string::npos ? std::string::npos : sep - start);
        if (!part.empty()) out.push_back(part);
        if (sep == std::string::npos) break;
        start = sep + 2;
    }

    return out;
}

void open_namespaces(std::vector<std::string>& out, const std::vector<std::string>& names) {
    for (const std::string& n : names) out.push_back("namespace " + n + " {");
    out.push_back("");
}

void close_namespaces(std::vector<std::string>& out, const std::vector<std::string>& names) {
    for (auto it = names.rbegin(); it != names.rend(); ++it) out.push_back("} // namespace " + *it);
}

void emit_words(std::vector<std::string>& out, const std::string& symbol, const std::vector<std::uint32_t>& words) {
    out.push_back("inline constexpr std::uint32_t " + symbol + "[] = {");

    for (std::size_t i = 0; i < words.size(); i += 8) {
        std::string line = "    ";
        for (std::size_t j = i; j < std::min(words.size(), i + 8); ++j) line += (j > i ? ", " : "") + format_hex(words[j]);
        out.push_back(line + ",");
    }

    out.push_back("};");
    out.push_back("");
}

std::string join_lines(const std::vector<std::string>& lines) {
    std::string out;

    for (std::size_t i = 0; i < lines.size(); ++i) {
        if (i) out += '\n';
        out += lines[i];
    }

    return out;
}

std::string display(const fs::path& p) {
    return p.lexically_normal().string();
}

class ScratchDir {
public:
    ScratchDir() {
        static int counter = 0;
        std::error_code ec;
        m_path = fs::temp_directory_path(ec) / ("fizmo_shaders_" + std::to_string(reinterpret_cast<std::uintptr_t>(this)) + "_" + std::to_string(++counter));
        fs::create_directories(m_path, ec);
    }

    ~ScratchDir() {
        std::error_code ec;
        fs::remove_all(m_path, ec);
    }

    const fs::path& path() const noexcept { return m_path; }

private:
    fs::path m_path;
};

struct Options {
    std::vector<std::string> shaders;
    std::string              output;
    std::string              ns = "shaders";
    std::string              include = "<fizmo/includes.hpp>";
    int                      gl_version = kVersion3D;
    int                      compute_version = kVersionCompute;
    std::string              bundle;
    std::string              guard;
    std::string              source_dir = FIZMO_TOOL_SOURCE_DIR;
};

void user_mode(const Options& args) {
    const std::string cross = tool("SPIRV_CROSS", "spirv-cross");
    std::string glslang;
    std::vector<Entry> entries;

    {
        ScratchDir scratch;

        for (const std::string& path : args.shaders) {
            std::error_code ec;
            if (!fs::is_regular_file(path, ec)) fail("cannot open " + path);
            const std::string base = fs::path(path).filename().string();
            Entry e;

            if (base.size() > 4 && base.compare(base.size() - 4, 4, ".spv") == 0) {
                e.name  = base.substr(0, base.size() - 4);
                e.words = read_words(path);
            } else {
                e.name = base;
                if (glslang.empty()) glslang = tool("GLSLANG", "glslangValidator");
                const fs::path spv_out = scratch.path() / (base + ".spv");
                run(glslang, { "-V", path, "-o", spv_out.string() }, "glslangValidator on " + base);
                e.words = read_words(spv_out);
            }

            const std::string stage = stage_of(e.name);
            e.version = stage == "comp" ? args.compute_version : args.gl_version;
            const fs::path spv = scratch.path() / (e.name + ".in.spv");
            write_words(spv, e.words);
            e.symbol   = symbol_for(e.name);
            e.glsl     = clean(e.name, translate(cross, spv, e.name, e.version));
            e.bindings = reflect(cross, spv, e.name);
            if (!args.bundle.empty()) reflect_layout(cross, spv, e.name, e.blocks, e.inputs);
            entries.push_back(std::move(e));
        }
    }

    if (!args.bundle.empty()) {
        std::error_code ec;
        fs::create_directories(args.bundle, ec);
        for (const Entry& e : entries) write_bundle(fs::path(args.bundle) / (e.name + ".fzsh"), e);
        std::cout << "wrote " << entries.size() << " shader bundles to " << display(args.bundle) << "\n";
        if (args.output.empty()) return;
    }

    std::string guard = args.guard;

    if (guard.empty()) {
        for (char c : fs::path(args.output).filename().string()) guard += is_alnum(c) ? char(std::toupper(static_cast<unsigned char>(c))) : '_';
    }

    std::vector<std::string> out = { "#ifndef " + guard, "#define " + guard, "" };
    out.push_back("#include " + args.include);
    out.push_back("#include <cstdint>");
    out.push_back("");
    const std::vector<std::string> names = split_namespace(args.ns);
    open_namespaces(out, names);

    for (const Entry& e : entries) {
        emit_words(out, e.symbol + "Spirv", e.words);
        emit_shader(out, e, e.symbol + "Spirv");
    }

    close_namespaces(out, names);
    out.push_back("");
    out.push_back("#endif // " + guard);
    out.push_back("");
    write_text(args.output, join_lines(out));
    std::cout << "wrote " << entries.size() << " shaders to " << display(args.output) << "\n";
}

void find_renderer_shaders(const std::string& text, std::vector<std::tuple<std::string, std::string, std::vector<std::uint32_t>>>& found) {
    const std::string open = "/* ---- ";
    const std::string decl = "inline constexpr std::uint32_t ";
    std::size_t pos = 0;

    while ((pos = text.find(open, pos)) != std::string::npos) {
        std::size_t p = pos + open.size();
        std::size_t q = p;
        while (q < text.size() && (is_alnum(text[q]) || text[q] == '_' || text[q] == '.')) ++q;

        if (q == p || text.compare(q, 6, " ----\n") != 0) {
            pos = p;
            continue;
        }

        const std::string name = text.substr(p, q - p);
        std::size_t close = q + 6;
        bool matched = false;

        while ((close = text.find("*/", close)) != std::string::npos) {
            std::size_t d = close + 2;
            while (d < text.size() && is_space(text[d])) ++d;

            if (text.compare(d, decl.size(), decl) == 0) {
                std::size_t s = d + decl.size();
                std::size_t se = s;
                while (se < text.size() && (is_alnum(text[se]) || text[se] == '_')) ++se;

                if (se > s + 1 && text[s] == 'k' && text.compare(se, 6, "[] = {") == 0) {
                    const std::size_t body_start = se + 6;
                    const std::size_t body_end = text.find("};", body_start);
                    if (body_end == std::string::npos) break;
                    std::vector<std::uint32_t> words;
                    const std::string body = text.substr(body_start, body_end - body_start);
                    static const std::regex hex_re("0x[0-9a-fA-F]+");
                    for (auto it = std::sregex_iterator(body.begin(), body.end(), hex_re); it != std::sregex_iterator(); ++it)
                        words.push_back(static_cast<std::uint32_t>(std::stoul(it->str(), nullptr, 16)));
                    found.emplace_back(name, text.substr(s, se - s), std::move(words));
                    pos = body_end + 2;
                    matched = true;
                    break;
                }
            }

            close += 2;
        }

        if (!matched) pos = q;
    }
}

void renderer_mode(const Options& args) {
    const fs::path root = args.source_dir;
    const fs::path impl = root / "Source" / "Windows" / "Renderer Impl";
    const fs::path output = impl / "gl_shaders.hpp";
    const std::string cross = tool("SPIRV_CROSS", "spirv-cross");
    std::vector<Entry> entries;

    {
        ScratchDir scratch;

        for (const char* file : { "gpu_shaders.hpp", "gpu_shaders_3d.hpp" }) {
            std::vector<std::tuple<std::string, std::string, std::vector<std::uint32_t>>> found;
            find_renderer_shaders(read_text(impl / file), found);

            for (auto& f : found) {
                Entry e;
                e.name    = std::get<0>(f);
                e.symbol  = std::get<1>(f);
                e.words   = std::move(std::get<2>(f));
                e.version = renderer_version(e.name);
                const fs::path spv = scratch.path() / (e.name + ".spv");
                write_words(spv, e.words);
                e.glsl     = clean(e.name, translate(cross, spv, e.name, e.version));
                e.bindings = reflect(cross, spv, e.name);
                entries.push_back(std::move(e));
            }
        }
    }

    if (entries.empty()) fail("no shaders found");
    std::vector<std::string> out = { "#ifndef FIZMO_GL_SHADERS_HPP", "#define FIZMO_GL_SHADERS_HPP", "" };
    out.push_back("#include \"../../GPU/types.hpp\"");
    out.push_back("#include \"gpu_shaders.hpp\"");
    out.push_back("#include \"gpu_shaders_3d.hpp\"");
    out.push_back("#include <cstdint>");
    out.push_back("");
    const std::vector<std::string> names = { "fizmo", "windows", "detail", "glsl" };
    open_namespaces(out, names);
    for (const Entry& e : entries) emit_shader(out, e, std::string(kSpirvNamespace) + "::" + e.symbol);
    close_namespaces(out, names);
    out.push_back("");
    out.push_back("#endif // FIZMO_GL_SHADERS_HPP");
    out.push_back("");
    write_text(output, join_lines(out));
    std::cout << "wrote " << entries.size() << " shaders to " << display(output) << "\n";

    const fs::path extra = root / "tools" / "shaders";
    std::error_code ec;
    if (!fs::is_directory(extra, ec)) return;
    Options extra_args;

    for (const auto& entry : fs::directory_iterator(extra, ec)) {
        const std::string n = entry.path().filename().string();
        if (is_stage(extension_of(n))) extra_args.shaders.push_back(entry.path().string());
    }

    if (extra_args.shaders.empty()) return;
    std::sort(extra_args.shaders.begin(), extra_args.shaders.end());
    extra_args.output          = (impl / "gpu_shaders_extra.hpp").string();
    extra_args.ns              = kExtraNamespace;
    extra_args.include         = "\"../../GPU/types.hpp\"";
    extra_args.gl_version      = kVersion2D;
    extra_args.compute_version = kVersionCompute;
    extra_args.guard           = "FIZMO_GPU_SHADERS_EXTRA_HPP";
    user_mode(extra_args);
}

void usage(std::ostream& os) {
    os << "usage: fizmo-shaders [-h] [-o OUTPUT] [-n NAMESPACE] [--include INCLUDE] [--gl-version N] [--compute-version N] [--bundle DIR] [--source-dir DIR] [shaders ...]\n"
          "\n"
          "Builds fizmo::gpu shader headers (SPIR-V + GLSL + binding tables).\n"
          "\n"
          "  shaders              GLSL (.vert/.frag/.comp) or SPIR-V (.vert.spv/...) files; none rebuilds the renderer's shaders\n"
          "  -o, --output         header to write\n"
          "  -n, --namespace      namespace for the generated code, e.g. my::shaders (default: shaders)\n"
          "  --include            include line that provides fizmo::gpu (default: <fizmo/includes.hpp>)\n"
          "  --gl-version         GLSL version for vertex and fragment shaders (default: 410)\n"
          "  --compute-version    GLSL version for compute shaders (default: 430)\n"
          "  --bundle             also write a runtime .fzsh bundle per shader into this directory\n"
          "  --source-dir         fizmo source folder used when rebuilding the renderer's shaders\n";
}

int parse_int(const std::string& flag, const std::string& value) {
    try {
        std::size_t used = 0;
        const int v = std::stoi(value, &used);
        if (used == value.size()) return v;
    } catch (...) {
    }
    fail("argument " + flag + ": invalid int value: '" + value + "'");
}

}

int main(int argc, char** argv) {
    try {
        Options args;

        for (int i = 1; i < argc; ++i) {
            const std::string a = argv[i];
            auto value = [&]() -> std::string {
                if (i + 1 >= argc) fail("argument " + a + ": expected one argument");
                return argv[++i];
            };

            if (a == "-h" || a == "--help")                  { usage(std::cout); return 0; }
            else if (a == "-o" || a == "--output")           args.output = value();
            else if (a == "-n" || a == "--namespace")        args.ns = value();
            else if (a == "--include")                       args.include = value();
            else if (a == "--gl-version")                    args.gl_version = parse_int(a, value());
            else if (a == "--compute-version")               args.compute_version = parse_int(a, value());
            else if (a == "--bundle")                        args.bundle = value();
            else if (a == "--source-dir")                    args.source_dir = value();
            else if (a.size() > 1 && a[0] == '-')            fail("unrecognized argument: " + a);
            else                                             args.shaders.push_back(a);
        }

        if (args.shaders.empty()) {
            renderer_mode(args);
            return 0;
        }

        if (args.output.empty() && args.bundle.empty()) fail("--output or --bundle is required when shaders are given");
        if (args.include.empty() || (args.include[0] != '<' && args.include[0] != '"')) args.include = "\"" + args.include + "\"";
        user_mode(args);
        return 0;
    } catch (const Failure& f) {
        std::cerr << "fizmo-shaders: " << f.message << "\n";
        return 1;
    }
}
