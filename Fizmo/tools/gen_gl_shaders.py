import argparse
import json
import os
import re
import struct
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
IMPL = os.path.join(HERE, "..", "Source", "Windows", "Renderer Impl")
SOURCES = [os.path.join(IMPL, "gpu_shaders.hpp"), os.path.join(IMPL, "gpu_shaders_3d.hpp")]
OUTPUT = os.path.join(IMPL, "gl_shaders.hpp")
EXTRA_SOURCES = os.path.join(HERE, "shaders")
EXTRA_OUTPUT = os.path.join(IMPL, "gpu_shaders_extra.hpp")
EXTRA_NAMESPACE = "fizmo::windows::detail::extra_glsl"
SPIRV_NAMESPACE = "::fizmo::windows::detail::gfx"
CHUNK = 12000
VERSION_2D = 330
VERSION_3D = 410
VERSION_COMPUTE = 430
SHADERS_2D = {"draw2d.vert", "draw2d.frag", "present.vert", "present.frag", "upscale.frag"}
STAGES = {"vert": "Vertex", "frag": "Fragment", "comp": "Compute"}

PATTERN = re.compile(r"/\* ---- ([\w.]+) ----\n.*?\*/\s*inline constexpr std::uint32_t (k\w+)\[\] = \{(.*?)\};", re.S)

DEPTH_FIXUP = "#ifdef FIZMO_GL_DEPTH_FIXUP\n    gl_Position.z = 2.0 * gl_Position.z - gl_Position.w;\n#endif\n"


def tool(env, default):
    path = os.environ.get(env, default)
    try:
        subprocess.run([path, "--help"], capture_output=True)
    except FileNotFoundError:
        sys.exit("%s was not found; install it or set %s to its path" % (default, env))
    return path


def stage_of(name):
    ext = name.split(".")[-1]
    if ext not in STAGES:
        sys.exit("cannot tell the shader stage of %s (expected .vert, .frag or .comp)" % name)
    return ext


def renderer_version(name):
    if name.endswith(".comp"):
        return VERSION_COMPUTE
    return VERSION_2D if name in SHADERS_2D else VERSION_3D


def run(args, what):
    result = subprocess.run(args, capture_output=True, text=True)
    if result.returncode != 0:
        sys.exit("%s failed:\n%s%s" % (what, result.stdout, result.stderr))
    return result.stdout


def write_words(path, words):
    with open(path, "wb") as f:
        f.write(struct.pack("<%dI" % len(words), *words))


def read_words(path):
    with open(path, "rb") as f:
        data = f.read()
    if len(data) % 4:
        sys.exit("%s is not a SPIR-V binary" % path)
    return list(struct.unpack("<%dI" % (len(data) // 4), data))


def translate(cross, spv, name, version):
    args = [cross, spv, "--version", str(version), "--no-es", "--no-420pack-extension", "--glsl-emit-push-constant-as-ubo"]
    return run(args, "spirv-cross on %s" % name)


def reflect(cross, spv, name):
    data = json.loads(run([cross, spv, "--reflect"], "spirv-cross --reflect on %s" % name))
    types = data.get("types", {})
    out = []

    def count(entry):
        array = entry.get("array", [])
        total = 1
        for n in array:
            total *= max(int(n), 1)
        return total

    for t in data.get("textures", []):
        out.append((t["name"], "Texture", t.get("set", 0), t.get("binding", 0), count(t)))
    for u in data.get("ubos", []):
        out.append((u["name"], "UniformBlock", u.get("set", 0), u.get("binding", 0), count(u)))
    for p in data.get("push_constants", []):
        block = types.get(p["type"], {}).get("name", p["name"])
        out.append((block, "PushBlock", 0, 0, 1))
    for i in data.get("images", []):
        out.append((i["name"], "StorageImage", i.get("set", 0), i.get("binding", 0), count(i)))
    for s in data.get("ssbos", []):
        out.append((s["name"], "StorageBlock", s.get("set", 0), s.get("binding", 0), count(s)))
    if data.get("separate_images") or data.get("separate_samplers"):
        sys.exit("%s uses separate images or samplers; use combined image samplers" % name)
    return out


BASE_TYPES = {"float": 1, "int": 2, "uint": 3, "bool": 4, "double": 5}
BLOCK_KINDS = {"UniformBlock": 0, "PushBlock": 1, "StorageBlock": 2}
STAGE_BITS = {"vert": 1, "frag": 2, "comp": 4}


def parse_type(t):
    m = re.match(r"^(d|i|u|b)?vec([234])$", t)
    if m:
        prefix = {None: "float", "d": "double", "i": "int", "u": "uint", "b": "bool"}[m.group(1)]
        return BASE_TYPES[prefix], int(m.group(2)), 1
    m = re.match(r"^d?mat([234])(?:x([234]))?$", t)
    if m:
        cols = int(m.group(1))
        rows = int(m.group(2)) if m.group(2) else cols
        return BASE_TYPES["double" if t.startswith("d") else "float"], rows, cols
    if t in BASE_TYPES:
        return BASE_TYPES[t], 1, 1
    return 0, 0, 0


def flatten_members(types, type_id, prefix, base_offset, out):
    for member in types.get(type_id, {}).get("members", []):
        name = prefix + member["name"]
        offset = base_offset + member.get("offset", 0)
        array = member.get("array", [])
        count = 1
        for n in array:
            count *= max(int(n), 1) if n else 0
        stride = member.get("array_stride", 0)
        if member["type"] in types:
            if array and count > 0:
                for i in range(count):
                    flatten_members(types, member["type"], "%s[%d]." % (name, i), offset + i * stride, out)
            elif not array:
                flatten_members(types, member["type"], name + ".", offset, out)
            continue
        base, rows, cols = parse_type(member["type"])
        out.append((name, offset, base, rows, cols, count if array else 1, stride, member.get("matrix_stride", 0)))


def reflect_layout(cross, spv, name):
    data = json.loads(run([cross, spv, "--reflect"], "spirv-cross --reflect on %s" % name))
    types = data.get("types", {})
    blocks = []
    for kind, key in (("UniformBlock", "ubos"), ("StorageBlock", "ssbos"), ("PushBlock", "push_constants")):
        for b in data.get(key, []):
            members = []
            flatten_members(types, b["type"], "", 0, members)
            block_name = b["name"] if kind != "PushBlock" else types.get(b["type"], {}).get("name", b["name"])
            size = b.get("block_size")
            if size is None:
                size = max([m[1] + max(m[3], 1) * 4 * max(m[4], 1) for m in members] or [0])
            set_index = b.get("set", 0) if kind != "PushBlock" else 0
            binding = b.get("binding", 0) if kind != "PushBlock" else 0
            blocks.append((block_name, BLOCK_KINDS[kind], set_index, binding, size, members))
    inputs = []
    for i in data.get("inputs", []):
        base, rows, cols = parse_type(i["type"])
        inputs.append((i["name"], i.get("location", 0), base, rows))
    return blocks, inputs


def pack_string(text):
    raw = text.encode("utf-8")
    return struct.pack("<I", len(raw)) + raw


def write_bundle(path, entry):
    kinds = {"Texture": 0, "UniformBlock": 1, "PushBlock": 2, "StorageImage": 3, "StorageBlock": 4}
    out = bytearray(b"FZSH")
    out += struct.pack("<III", 1, STAGE_BITS[stage_of(entry["name"])], entry["version"])
    out += pack_string(entry["name"])
    out += pack_string(entry["glsl"])
    out += struct.pack("<I", len(entry["bindings"]))
    for b in entry["bindings"]:
        out += pack_string(b[0]) + struct.pack("<IIII", kinds[b[1]], b[2], b[3], b[4])
    out += struct.pack("<I", len(entry["blocks"]))
    for blk in entry["blocks"]:
        out += pack_string(blk[0]) + struct.pack("<IIII", blk[1], blk[2], blk[3], blk[4])
        out += struct.pack("<I", len(blk[5]))
        for m in blk[5]:
            out += pack_string(m[0]) + struct.pack("<IIIIIII", *m[1:])
    out += struct.pack("<I", len(entry["inputs"]))
    for i in entry["inputs"]:
        out += pack_string(i[0]) + struct.pack("<III", i[1], i[2], i[3])
    out += struct.pack("<I", len(entry["words"]))
    out += struct.pack("<%dI" % len(entry["words"]), *entry["words"])
    tmp = path + ".tmp"
    with open(tmp, "wb") as f:
        f.write(out)
    os.replace(tmp, path)


def clean(name, text):
    lines = []
    for line in text.splitlines():
        if line.startswith("#version"):
            continue
        if "shading_language_420pack" in line:
            continue
        lines.append(line)
    while lines and lines[0].strip() in ("", "#endif"):
        lines.pop(0)
    body = "\n".join(lines).strip() + "\n"
    body = re.sub(r"\bbinding\s*=\s*\d+\s*,\s*", "", body)
    body = re.sub(r",\s*binding\s*=\s*\d+\s*", "", body)
    body = re.sub(r"layout\(\s*binding\s*=\s*\d+\s*\)\s*", "", body)
    if name.endswith(".vert"):
        end = body.rstrip().rfind("}")
        if end < 0:
            sys.exit("no main body in %s" % name)
        body = body[:end] + DEPTH_FIXUP + body[end:]
    return body


def chunks(text):
    pieces, current = [], ""
    for line in text.splitlines(keepends=True):
        if current and len(current) + len(line) > CHUNK:
            pieces.append(current)
            current = ""
        current += line
    if current:
        pieces.append(current)
    return pieces


def symbol_for(name):
    parts = re.split(r"[^0-9A-Za-z]+", name)
    return "k" + "".join(p[:1].upper() + p[1:] for p in parts if p)


def function_for(name):
    ident = re.sub(r"[^0-9A-Za-z_]", "_", name)
    return ident if not ident[0].isdigit() else "_" + ident


def emit_shader(out, entry, spirv_symbol):
    name, symbol, version, body, bindings = entry["name"], entry["symbol"], entry["version"], entry["glsl"], entry["bindings"]
    out.append("inline constexpr int %sVersion = %d;" % (symbol, version))
    out.append("")
    out.append("inline constexpr const char* %s[] = {" % symbol)
    for piece in chunks(body):
        out.append('R"fizmo_glsl(' + piece + ')fizmo_glsl",')
    out.append("};")
    out.append("")
    out.append("inline constexpr ::fizmo::gpu::GlslBinding %sBindings[] = {" % symbol)
    for b in bindings:
        out.append('    { "%s", ::fizmo::gpu::GlslResource::%s, %d, %d, %d },' % b)
    if not bindings:
        out.append("    { nullptr, ::fizmo::gpu::GlslResource::Texture, 0, 0, 0 },")
    out.append("};")
    out.append("")
    out.append("inline ::fizmo::gpu::ShaderDesc %s() noexcept {" % function_for(name))
    out.append("    ::fizmo::gpu::ShaderDesc d;")
    out.append("    d.stage         = ::fizmo::gpu::ShaderStage::%s;" % STAGES[stage_of(name)])
    out.append("    d.spirv         = { %s, sizeof(%s) / sizeof(std::uint32_t) };" % (spirv_symbol, spirv_symbol))
    out.append("    d.glsl          = %s;" % symbol)
    out.append("    d.glsl_pieces   = sizeof(%s) / sizeof(%s[0]);" % (symbol, symbol))
    out.append("    d.glsl_version  = %sVersion;" % symbol)
    out.append("    d.glsl_bindings = { %sBindings, %d };" % (symbol, len(bindings)))
    out.append('    d.name          = "%s";' % name)
    out.append("    return d;")
    out.append("}")
    out.append("")


def open_namespaces(out, names):
    for n in names:
        out.append("namespace %s {" % n)
    out.append("")


def close_namespaces(out, names):
    for n in reversed(names):
        out.append("} // namespace %s" % n)


def emit_words(out, symbol, words):
    out.append("inline constexpr std::uint32_t %s[] = {" % symbol)
    for i in range(0, len(words), 8):
        out.append("    " + ", ".join("0x%08x" % w for w in words[i:i + 8]) + ",")
    out.append("};")
    out.append("")


def renderer_mode():
    cross = tool("SPIRV_CROSS", "spirv-cross")
    entries = []
    with tempfile.TemporaryDirectory() as scratch:
        for path in SOURCES:
            with open(path) as f:
                text = f.read()
            for match in PATTERN.finditer(text):
                name, symbol, raw = match.groups()
                words = [int(w, 16) for w in re.findall(r"0x[0-9a-fA-F]+", raw)]
                spv = os.path.join(scratch, name + ".spv")
                write_words(spv, words)
                version = renderer_version(name)
                entries.append({
                    "name": name,
                    "symbol": symbol,
                    "version": version,
                    "glsl": clean(name, translate(cross, spv, name, version)),
                    "bindings": reflect(cross, spv, name),
                })
    if not entries:
        sys.exit("no shaders found")
    out = ["#ifndef FIZMO_GL_SHADERS_HPP", "#define FIZMO_GL_SHADERS_HPP", ""]
    out.append('#include "../../GPU/types.hpp"')
    out.append('#include "gpu_shaders.hpp"')
    out.append('#include "gpu_shaders_3d.hpp"')
    out.append("#include <cstdint>")
    out.append("")
    names = ["fizmo", "windows", "detail", "glsl"]
    open_namespaces(out, names)
    for e in entries:
        emit_shader(out, e, "%s::%s" % (SPIRV_NAMESPACE, e["symbol"]))
    close_namespaces(out, names)
    out.append("")
    out.append("#endif // FIZMO_GL_SHADERS_HPP")
    out.append("")
    with open(OUTPUT, "w", newline="\n") as f:
        f.write("\n".join(out))
    print("wrote %d shaders to %s" % (len(entries), os.path.normpath(OUTPUT)))
    extra_mode()


def extra_mode():
    if not os.path.isdir(EXTRA_SOURCES):
        return
    shaders = sorted(os.path.join(EXTRA_SOURCES, n) for n in os.listdir(EXTRA_SOURCES) if n.split(".")[-1] in STAGES)
    if not shaders:
        return
    args = argparse.Namespace(
        shaders=shaders,
        output=EXTRA_OUTPUT,
        namespace=EXTRA_NAMESPACE,
        include='"../../GPU/types.hpp"',
        gl_version=VERSION_2D,
        compute_version=VERSION_COMPUTE,
        guard="FIZMO_GPU_SHADERS_EXTRA_HPP",
    )
    user_mode(args)


def user_mode(args):
    cross = tool("SPIRV_CROSS", "spirv-cross")
    glslang = None
    entries = []
    with tempfile.TemporaryDirectory() as scratch:
        for path in args.shaders:
            base = os.path.basename(path)
            if base.endswith(".spv"):
                name = base[:-4]
                words = read_words(path)
            else:
                name = base
                if glslang is None:
                    glslang = tool("GLSLANG", "glslangValidator")
                spv_out = os.path.join(scratch, base + ".spv")
                run([glslang, "-V", path, "-o", spv_out], "glslangValidator on %s" % base)
                words = read_words(spv_out)
            stage = stage_of(name)
            version = args.compute_version if stage == "comp" else args.gl_version
            spv = os.path.join(scratch, name + ".in.spv")
            write_words(spv, words)
            entry = {
                "name": name,
                "symbol": symbol_for(name),
                "version": version,
                "words": words,
                "glsl": clean(name, translate(cross, spv, name, version)),
                "bindings": reflect(cross, spv, name),
            }
            if getattr(args, "bundle", None):
                entry["blocks"], entry["inputs"] = reflect_layout(cross, spv, name)
            entries.append(entry)
    if getattr(args, "bundle", None):
        os.makedirs(args.bundle, exist_ok=True)
        for e in entries:
            write_bundle(os.path.join(args.bundle, e["name"] + ".fzsh"), e)
        print("wrote %d shader bundles to %s" % (len(entries), os.path.normpath(args.bundle)))
        if not args.output:
            return
    guard = getattr(args, "guard", None) or re.sub(r"[^0-9A-Za-z]", "_", os.path.basename(args.output)).upper()
    out = ["#ifndef %s" % guard, "#define %s" % guard, ""]
    out.append("#include %s" % args.include)
    out.append("#include <cstdint>")
    out.append("")
    names = [n for n in args.namespace.split("::") if n]
    open_namespaces(out, names)
    for e in entries:
        emit_words(out, e["symbol"] + "Spirv", e["words"])
        emit_shader(out, e, e["symbol"] + "Spirv")
    close_namespaces(out, names)
    out.append("")
    out.append("#endif // %s" % guard)
    out.append("")
    with open(args.output, "w", newline="\n") as f:
        f.write("\n".join(out))
    print("wrote %d shaders to %s" % (len(entries), os.path.normpath(args.output)))


def main():
    parser = argparse.ArgumentParser(description="Builds fizmo::gpu shader headers (SPIR-V + GLSL + binding tables).")
    parser.add_argument("shaders", nargs="*", help="GLSL (.vert/.frag/.comp) or SPIR-V (.vert.spv/...) files; none rebuilds the renderer's shaders")
    parser.add_argument("-o", "--output", help="header to write")
    parser.add_argument("-n", "--namespace", default="shaders", help="namespace for the generated code, e.g. my::shaders")
    parser.add_argument("--include", default="<fizmo/includes.hpp>", help="include line that provides fizmo::gpu")
    parser.add_argument("--gl-version", type=int, default=VERSION_3D, help="GLSL version for vertex and fragment shaders")
    parser.add_argument("--compute-version", type=int, default=VERSION_COMPUTE, help="GLSL version for compute shaders")
    parser.add_argument("--bundle", help="also write a runtime .fzsh bundle per shader into this directory (loadable with fizmo::gpu::ShaderBundle)")
    args = parser.parse_args()
    if not args.shaders:
        renderer_mode()
        return
    if not args.output and not args.bundle:
        parser.error("--output or --bundle is required when shaders are given")
    if not args.include.startswith(("<", '"')):
        args.include = '"%s"' % args.include
    user_mode(args)


if __name__ == "__main__":
    main()
