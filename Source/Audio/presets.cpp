#include "presets.hpp"
#include "dynamics.hpp"
#include "filters.hpp"
#include "format.hpp"
#include "modulation.hpp"
#include "noise.hpp"
#include "pitch.hpp"
#include "time_effects.hpp"
#include "voice.hpp"

#include <algorithm>
#include <clocale>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <mutex>
#include <sstream>
#include <utility>

namespace fizmo {
namespace audio {

namespace {

const std::size_t k_max_depth = 256;
const std::size_t k_max_effect_depth = 64;
const char* const k_format_name = "fizmo-audio-preset";
const int k_format_version = 1;

char locale_point() {
    const std::lconv* lc = std::localeconv();
    if (lc && lc->decimal_point && lc->decimal_point[0]) return lc->decimal_point[0];
    return '.';
}

std::string format_number(double v, int precision) {
    char buf[64];
    std::snprintf(buf, sizeof(buf), "%.*g", precision, v);
    const char point = locale_point();
    std::string s(buf);
    if (point != '.') std::replace(s.begin(), s.end(), point, '.');
    return s;
}

double parse_number_text(const std::string& text) {
    std::string s = text;
    const char point = locale_point();
    if (point != '.') std::replace(s.begin(), s.end(), '.', point);
    return std::strtod(s.c_str(), nullptr);
}

std::string number_text(double v, bool single) {
    if (!(v == v) || std::isinf(v)) return "null";
    if (v == 0.0) return std::signbit(v) ? "-0" : "0";
    if (single) {
        const float f = static_cast<float>(v);
        for (int p = 1; p <= 9; ++p) {
            const std::string s = format_number(static_cast<double>(f), p);
            if (static_cast<float>(parse_number_text(s)) == f) return s;
        }
        return format_number(static_cast<double>(f), 9);
    }
    for (int p = 1; p <= 17; ++p) {
        const std::string s = format_number(v, p);
        if (parse_number_text(s) == v) return s;
    }
    return format_number(v, 17);
}

void escape_string(const std::string& s, std::string& out) {
    out += '"';
    for (unsigned char c : s) {
        switch (c) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\b': out += "\\b"; break;
            case '\f': out += "\\f"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if (c < 0x20) {
                    char buf[8];
                    std::snprintf(buf, sizeof(buf), "\\u%04x", static_cast<unsigned>(c));
                    out += buf;
                } else {
                    out += static_cast<char>(c);
                }
        }
    }
    out += '"';
}

bool is_scalar(const JsonValue& v) {
    return !v.is_array() && !v.is_object();
}

void write_value(const JsonValue& v, std::string& out, bool pretty, std::size_t depth) {
    const std::string pad = pretty ? std::string((depth + 1) * 2, ' ') : std::string();
    const std::string close_pad = pretty ? std::string(depth * 2, ' ') : std::string();
    switch (v.kind()) {
        case JsonValue::Kind::null: out += "null"; break;
        case JsonValue::Kind::boolean: out += v.as_bool() ? "true" : "false"; break;
        case JsonValue::Kind::number: out += number_text(v.as_number(), v.is_single_precision()); break;
        case JsonValue::Kind::string: escape_string(v.as_string(), out); break;
        case JsonValue::Kind::array: {
            if (v.size() == 0) {
                out += "[]";
                break;
            }
            bool flat = true;
            for (std::size_t i = 0; i < v.size(); ++i) flat = flat && is_scalar(v.at(i));
            out += '[';
            for (std::size_t i = 0; i < v.size(); ++i) {
                if (i) out += pretty && flat ? ", " : ",";
                if (pretty && !flat) {
                    out += '\n';
                    out += pad;
                }
                write_value(v.at(i), out, pretty, depth + 1);
            }
            if (pretty && !flat) {
                out += '\n';
                out += close_pad;
            }
            out += ']';
            break;
        }
        case JsonValue::Kind::object: {
            if (v.size() == 0) {
                out += "{}";
                break;
            }
            out += '{';
            for (std::size_t i = 0; i < v.size(); ++i) {
                if (i) out += ',';
                if (pretty) {
                    out += '\n';
                    out += pad;
                }
                escape_string(v.key(i), out);
                out += pretty ? ": " : ":";
                write_value(v.at(i), out, pretty, depth + 1);
            }
            if (pretty) {
                out += '\n';
                out += close_pad;
            }
            out += '}';
            break;
        }
    }
}

class Parser {
public:
    explicit Parser(const std::string& text) : m_text(text) {}

    bool parse(JsonValue& out, std::string* error) {
        if (m_text.size() >= 3 && static_cast<unsigned char>(m_text[0]) == 0xEF && static_cast<unsigned char>(m_text[1]) == 0xBB && static_cast<unsigned char>(m_text[2]) == 0xBF) m_pos = 3;
        skip();
        if (!value(out, 0)) {
            if (error) *error = m_error;
            return false;
        }
        skip();
        if (m_pos != m_text.size()) {
            fail("unexpected trailing characters");
            if (error) *error = m_error;
            return false;
        }
        return true;
    }

private:
    bool fail(const char* what) {
        if (m_error.empty()) {
            std::size_t line = 1;
            std::size_t col = 1;
            for (std::size_t i = 0; i < m_pos && i < m_text.size(); ++i) {
                if (m_text[i] == '\n') {
                    ++line;
                    col = 1;
                } else {
                    ++col;
                }
            }
            m_error = std::string("JSON error at line ") + std::to_string(line) + ", column " + std::to_string(col) + ": " + what;
        }
        return false;
    }

    void skip() {
        while (m_pos < m_text.size()) {
            const char c = m_text[m_pos];
            if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
                ++m_pos;
            } else {
                break;
            }
        }
    }

    bool literal(const char* word) {
        const std::size_t n = std::strlen(word);
        if (m_text.compare(m_pos, n, word) != 0) return fail("invalid literal");
        m_pos += n;
        return true;
    }

    bool value(JsonValue& out, std::size_t depth) {
        if (depth > k_max_depth) return fail("nesting too deep");
        if (m_pos >= m_text.size()) return fail("unexpected end of input");
        const char c = m_text[m_pos];
        if (c == '{') return object(out, depth);
        if (c == '[') return array(out, depth);
        if (c == '"') {
            std::string s;
            if (!string(s)) return false;
            out = JsonValue::make_string(std::move(s));
            return true;
        }
        if (c == 't') {
            if (!literal("true")) return false;
            out = JsonValue::make_bool(true);
            return true;
        }
        if (c == 'f') {
            if (!literal("false")) return false;
            out = JsonValue::make_bool(false);
            return true;
        }
        if (c == 'n') {
            if (!literal("null")) return false;
            out = JsonValue();
            return true;
        }
        if (c == '-' || (c >= '0' && c <= '9')) return number(out);
        return fail("unexpected character");
    }

    bool digits() {
        const std::size_t start = m_pos;
        while (m_pos < m_text.size() && m_text[m_pos] >= '0' && m_text[m_pos] <= '9') ++m_pos;
        return m_pos > start;
    }

    bool number(JsonValue& out) {
        const std::size_t start = m_pos;
        if (m_text[m_pos] == '-') ++m_pos;
        if (m_pos < m_text.size() && m_text[m_pos] == '0') {
            ++m_pos;
        } else if (!digits()) {
            return fail("invalid number");
        }
        if (m_pos < m_text.size() && m_text[m_pos] == '.') {
            ++m_pos;
            if (!digits()) return fail("invalid number");
        }
        if (m_pos < m_text.size() && (m_text[m_pos] == 'e' || m_text[m_pos] == 'E')) {
            ++m_pos;
            if (m_pos < m_text.size() && (m_text[m_pos] == '+' || m_text[m_pos] == '-')) ++m_pos;
            if (!digits()) return fail("invalid number");
        }
        out = JsonValue::make_number(parse_number_text(m_text.substr(start, m_pos - start)));
        return true;
    }

    bool hex4(unsigned& v) {
        if (m_pos + 4 > m_text.size()) return fail("bad unicode escape");
        v = 0;
        for (int i = 0; i < 4; ++i) {
            const char c = m_text[m_pos++];
            v <<= 4;
            if (c >= '0' && c <= '9') {
                v |= static_cast<unsigned>(c - '0');
            } else if (c >= 'a' && c <= 'f') {
                v |= static_cast<unsigned>(c - 'a' + 10);
            } else if (c >= 'A' && c <= 'F') {
                v |= static_cast<unsigned>(c - 'A' + 10);
            } else {
                return fail("bad unicode escape");
            }
        }
        return true;
    }

    static void utf8(unsigned cp, std::string& out) {
        if (cp < 0x80) {
            out += static_cast<char>(cp);
        } else if (cp < 0x800) {
            out += static_cast<char>(0xC0 | (cp >> 6));
            out += static_cast<char>(0x80 | (cp & 0x3F));
        } else if (cp < 0x10000) {
            out += static_cast<char>(0xE0 | (cp >> 12));
            out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
            out += static_cast<char>(0x80 | (cp & 0x3F));
        } else {
            out += static_cast<char>(0xF0 | (cp >> 18));
            out += static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
            out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
            out += static_cast<char>(0x80 | (cp & 0x3F));
        }
    }

    bool string(std::string& out) {
        ++m_pos;
        while (true) {
            if (m_pos >= m_text.size()) return fail("unterminated string");
            const unsigned char c = static_cast<unsigned char>(m_text[m_pos++]);
            if (c == '"') return true;
            if (c < 0x20) return fail("control character in string");
            if (c != '\\') {
                out += static_cast<char>(c);
                continue;
            }
            if (m_pos >= m_text.size()) return fail("unterminated string");
            const char e = m_text[m_pos++];
            switch (e) {
                case '"': out += '"'; break;
                case '\\': out += '\\'; break;
                case '/': out += '/'; break;
                case 'b': out += '\b'; break;
                case 'f': out += '\f'; break;
                case 'n': out += '\n'; break;
                case 'r': out += '\r'; break;
                case 't': out += '\t'; break;
                case 'u': {
                    unsigned cp = 0;
                    if (!hex4(cp)) return false;
                    if (cp >= 0xD800 && cp <= 0xDBFF) {
                        unsigned lo = 0;
                        if (m_pos + 2 > m_text.size() || m_text[m_pos] != '\\' || m_text[m_pos + 1] != 'u') return fail("unpaired surrogate");
                        m_pos += 2;
                        if (!hex4(lo)) return false;
                        if (lo < 0xDC00 || lo > 0xDFFF) return fail("unpaired surrogate");
                        cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00);
                    } else if (cp >= 0xDC00 && cp <= 0xDFFF) {
                        return fail("unpaired surrogate");
                    }
                    utf8(cp, out);
                    break;
                }
                default: return fail("invalid escape");
            }
        }
    }

    bool array(JsonValue& out, std::size_t depth) {
        ++m_pos;
        out = JsonValue::make_array();
        skip();
        if (m_pos < m_text.size() && m_text[m_pos] == ']') {
            ++m_pos;
            return true;
        }
        while (true) {
            skip();
            JsonValue item;
            if (!value(item, depth + 1)) return false;
            out.push(std::move(item));
            skip();
            if (m_pos >= m_text.size()) return fail("unterminated array");
            const char c = m_text[m_pos++];
            if (c == ']') return true;
            if (c != ',') return fail("expected ',' or ']'");
        }
    }

    bool object(JsonValue& out, std::size_t depth) {
        ++m_pos;
        out = JsonValue::make_object();
        skip();
        if (m_pos < m_text.size() && m_text[m_pos] == '}') {
            ++m_pos;
            return true;
        }
        while (true) {
            skip();
            if (m_pos >= m_text.size() || m_text[m_pos] != '"') return fail("expected key");
            std::string key;
            if (!string(key)) return false;
            skip();
            if (m_pos >= m_text.size() || m_text[m_pos] != ':') return fail("expected ':'");
            ++m_pos;
            skip();
            JsonValue item;
            if (!value(item, depth + 1)) return false;
            out.set(key, std::move(item));
            skip();
            if (m_pos >= m_text.size()) return fail("unterminated object");
            const char c = m_text[m_pos++];
            if (c == '}') return true;
            if (c != ',') return fail("expected ',' or '}'");
        }
    }

    const std::string& m_text;
    std::size_t m_pos = 0;
    std::string m_error;
};

struct RegistryEntry {
    EffectTypeInfo info;
    EffectFactory factory;
};

template <typename T>
EffectFactory factory_of() {
    return []() -> std::shared_ptr<Effect> { return std::make_shared<T>(); };
}

class Registry {
public:
    Registry() {
        add("gain", factory_of<Gain>(), "Gain", "utility");
        add("chain", factory_of<EffectChain>(), "Effect Chain", "utility");
        add("preset", factory_of<PresetEffect>(), "Preset", "utility");
        add("dc_blocker", factory_of<DcBlocker>(), "DC Blocker", "utility");
        add("filter", factory_of<Filter>(), "Filter", "eq");
        add("parametric_eq", factory_of<ParametricEq>(), "Parametric EQ", "eq");
        add("graphic_eq", factory_of<GraphicEq>(), "Graphic EQ", "eq");
        add("compressor", factory_of<Compressor>(), "Compressor", "dynamics");
        add("limiter", factory_of<Limiter>(), "Limiter", "dynamics");
        add("noise_gate", factory_of<NoiseGate>(), "Noise Gate", "dynamics");
        add("expander", factory_of<Expander>(), "Expander", "dynamics");
        add("de_esser", factory_of<DeEsser>(), "De-esser", "dynamics");
        add("noise_reduction", factory_of<NoiseReduction>(), "Noise Reduction", "noise");
        add("hum_remover", factory_of<HumRemover>(), "Hum Remover", "noise");
        add("click_remover", factory_of<ClickRemover>(), "Click Remover", "noise");
        add("pitch_shifter", factory_of<PitchShifter>(), "Pitch Shifter", "voice");
        add("formant_shifter", factory_of<FormantShifter>(), "Formant Shifter", "voice");
        add("autotune", factory_of<AutoTune>(), "Pitch Correction", "voice");
        add("voice_eq", factory_of<VoiceEQ>(), "Voice EQ", "voice");
        add("ring_modulator", factory_of<RingModulator>(), "Ring Modulator", "voice");
        add("vocoder", factory_of<Vocoder>(), "Vocoder", "voice");
        add("voice_changer", factory_of<VoiceChanger>(), "Voice Changer", "voice");
        add("chorus", factory_of<Chorus>(), "Chorus", "modulation");
        add("flanger", factory_of<Flanger>(), "Flanger", "modulation");
        add("phaser", factory_of<Phaser>(), "Phaser", "modulation");
        add("tremolo", factory_of<Tremolo>(), "Tremolo", "modulation");
        add("vibrato", factory_of<Vibrato>(), "Vibrato", "modulation");
        add("autopan", factory_of<AutoPan>(), "Auto Pan", "modulation");
        add("delay", factory_of<Delay>(), "Delay", "time");
        add("reverb", factory_of<Reverb>(), "Reverb", "time");
        add("distortion", factory_of<Distortion>(), "Distortion", "distortion");
        add("bitcrusher", factory_of<Bitcrusher>(), "Bitcrusher", "distortion");
        for (RegistryEntry& e : m_entries) e.info.builtin = true;
    }

    bool add(const std::string& type, EffectFactory factory, const std::string& display, const std::string& category) {
        if (type.empty() || !factory) return false;
        RegistryEntry entry;
        entry.info.type = type;
        entry.info.display_name = display.empty() ? type : display;
        entry.info.category = category.empty() ? "other" : category;
        entry.factory = std::move(factory);
        std::lock_guard<std::mutex> lock(m_mutex);
        for (RegistryEntry& e : m_entries) {
            if (e.info.type == type) {
                e = std::move(entry);
                return true;
            }
        }
        m_entries.push_back(std::move(entry));
        return true;
    }

    bool remove(const std::string& type) {
        std::lock_guard<std::mutex> lock(m_mutex);
        for (std::size_t i = 0; i < m_entries.size(); ++i) {
            if (m_entries[i].info.type == type) {
                m_entries.erase(m_entries.begin() + static_cast<std::ptrdiff_t>(i));
                return true;
            }
        }
        return false;
    }

    EffectFactory factory(const std::string& type) const {
        std::lock_guard<std::mutex> lock(m_mutex);
        for (const RegistryEntry& e : m_entries) {
            if (e.info.type == type) return e.factory;
        }
        return EffectFactory();
    }

    bool info(const std::string& type, EffectTypeInfo& out) const {
        std::lock_guard<std::mutex> lock(m_mutex);
        for (const RegistryEntry& e : m_entries) {
            if (e.info.type == type) {
                out = e.info;
                return true;
            }
        }
        return false;
    }

    std::vector<EffectTypeInfo> list() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        std::vector<EffectTypeInfo> out;
        out.reserve(m_entries.size());
        for (const RegistryEntry& e : m_entries) out.push_back(e.info);
        return out;
    }

private:
    mutable std::mutex m_mutex;
    std::vector<RegistryEntry> m_entries;
};

Registry& registry() {
    static Registry r;
    return r;
}

void warn(std::vector<std::string>* warnings, const std::string& text) {
    if (warnings) warnings->push_back(text);
}

std::string read_text_file(const std::string& path, bool& ok) {
    std::ifstream in(path, std::ios::binary);
    ok = static_cast<bool>(in);
    if (!ok) return std::string();
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

bool write_text_file(const std::string& path, const std::string& text, std::string* error) {
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out) {
        if (error) *error = "cannot open '" + path + "' for writing";
        return false;
    }
    out.write(text.data(), static_cast<std::streamsize>(text.size()));
    out.flush();
    if (!out) {
        if (error) *error = "failed writing '" + path + "'";
        return false;
    }
    return true;
}

JsonValue effect_json(const Effect& effect, std::size_t depth);

JsonValue chain_effects_json(const EffectChain& chain, std::size_t depth) {
    JsonValue list = JsonValue::make_array();
    if (depth >= k_max_effect_depth) return list;
    for (const std::shared_ptr<Effect>& e : chain.effects()) {
        if (e) list.push(effect_json(*e, depth + 1));
    }
    return list;
}

JsonValue info_json(const PresetInfo& info) {
    JsonValue v = JsonValue::make_object();
    v.set("name", JsonValue::make_string(info.name));
    if (!info.author.empty()) v.set("author", JsonValue::make_string(info.author));
    if (!info.description.empty()) v.set("description", JsonValue::make_string(info.description));
    if (!info.category.empty()) v.set("category", JsonValue::make_string(info.category));
    if (!info.tags.empty()) {
        JsonValue tags = JsonValue::make_array();
        for (const std::string& t : info.tags) tags.push(JsonValue::make_string(t));
        v.set("tags", std::move(tags));
    }
    if (!info.version.empty()) v.set("version", JsonValue::make_string(info.version));
    return v;
}

void read_info(const JsonValue& v, PresetInfo& info) {
    info = PresetInfo();
    if (const JsonValue* x = v.find("name")) info.name = x->as_string(std::string());
    if (const JsonValue* x = v.find("author")) info.author = x->as_string(std::string());
    if (const JsonValue* x = v.find("description")) info.description = x->as_string(std::string());
    if (const JsonValue* x = v.find("category")) info.category = x->as_string(std::string());
    if (const JsonValue* x = v.find("version")) {
        if (x->is_number()) {
            info.version = number_text(x->as_number(), false);
        } else {
            info.version = x->as_string(std::string());
        }
    }
    if (const JsonValue* x = v.find("tags")) {
        if (x->is_array()) {
            for (std::size_t i = 0; i < x->size(); ++i) {
                if (x->at(i).is_string()) info.tags.push_back(x->at(i).as_string());
            }
        } else if (x->is_string()) {
            info.tags.push_back(x->as_string());
        }
    }
    if (const JsonValue* x = v.find("format_version")) info.format_version = static_cast<int>(x->as_number(1.0));
}

JsonValue effect_json(const Effect& effect, std::size_t depth) {
    if (const MissingEffect* m = dynamic_cast<const MissingEffect*>(&effect)) {
        if (m->data().is_object()) {
            JsonValue v = m->data();
            v.set("enabled", JsonValue::make_bool(effect.enabled()));
            return v;
        }
    }
    JsonValue v = JsonValue::make_object();
    v.set("type", JsonValue::make_string(effect.type()));
    v.set("enabled", JsonValue::make_bool(effect.enabled()));
    const std::vector<const Parameter*> params = effect.parameters();
    if (!params.empty()) {
        JsonValue p = JsonValue::make_object();
        for (const Parameter* q : params) p.set(q->name(), JsonValue::make_float(q->get()));
        v.set("parameters", std::move(p));
    }
    const std::string state = effect.save_state();
    if (!state.empty()) {
        JsonValue parsed;
        if (parse_json(state, parsed, nullptr)) {
            v.set("state", std::move(parsed));
        } else {
            v.set("state", JsonValue::make_string(state));
        }
    }
    if (const EffectChain* chain = dynamic_cast<const EffectChain*>(&effect)) {
        v.set("effects", chain_effects_json(*chain, depth));
    }
    if (const PresetEffect* preset = dynamic_cast<const PresetEffect*>(&effect)) {
        const PresetInfo info = preset->info();
        if (!info.name.empty() || !info.author.empty() || !info.description.empty() || !info.category.empty() || !info.tags.empty() || !info.version.empty()) v.set("info", info_json(info));
        std::shared_ptr<EffectChain> inner = preset->chain();
        if (inner && depth < k_max_effect_depth) v.set("chain", effect_json(*inner, depth + 1));
    }
    return v;
}

std::shared_ptr<Effect> effect_parse(const JsonValue& v, std::vector<std::string>* warnings, std::string* error, const std::string& where, std::size_t depth);

std::vector<std::shared_ptr<Effect>> parse_effect_list(const JsonValue& list, std::vector<std::string>* warnings, const std::string& where, std::size_t depth) {
    std::vector<std::shared_ptr<Effect>> out;
    if (!list.is_array()) {
        warn(warnings, where + ": 'effects' is not an array");
        return out;
    }
    for (std::size_t i = 0; i < list.size(); ++i) {
        const std::string at = where + ".effects[" + std::to_string(i) + "]";
        std::string err;
        std::shared_ptr<Effect> e = effect_parse(list.at(i), warnings, &err, at, depth + 1);
        if (e) {
            out.push_back(std::move(e));
        } else {
            warn(warnings, at + ": " + (err.empty() ? std::string("skipped") : err));
        }
    }
    return out;
}

void apply_effect_body(Effect& e, const JsonValue& v, std::vector<std::string>* warnings, const std::string& where, std::size_t depth) {
    if (const JsonValue* en = v.find("enabled")) {
        if (en->is_bool()) {
            e.set_enabled(en->as_bool());
        } else if (en->is_number()) {
            e.set_enabled(en->as_number() != 0.0);
        } else {
            warn(warnings, where + ": 'enabled' is not a boolean");
        }
    }
    if (const JsonValue* st = v.find("state")) {
        if (!st->is_null()) {
            const std::string text = st->is_string() ? st->as_string() : write_json(*st, false);
            if (!e.load_state(text)) warn(warnings, where + ": effect '" + e.type() + "' rejected its saved state");
        }
    }
    if (const JsonValue* ps = v.find("parameters")) {
        if (ps->is_object()) {
            for (std::size_t i = 0; i < ps->size(); ++i) {
                const std::string& name = ps->key(i);
                const JsonValue& value = ps->at(i);
                Parameter* p = e.parameter(name);
                if (!p) {
                    warn(warnings, where + ": unknown parameter '" + name + "' for effect '" + e.type() + "' ignored");
                    continue;
                }
                if (value.is_number()) {
                    p->set(value.as_float());
                } else if (value.is_bool()) {
                    p->set(value.as_bool() ? 1.0f : 0.0f);
                } else {
                    warn(warnings, where + ": parameter '" + name + "' is not a number");
                }
            }
        } else {
            warn(warnings, where + ": 'parameters' is not an object");
        }
    }
    if (EffectChain* chain = dynamic_cast<EffectChain*>(&e)) {
        if (const JsonValue* list = v.find("effects")) {
            if (depth >= k_max_effect_depth) {
                warn(warnings, where + ": chains nested too deeply");
            } else {
                chain->assign(parse_effect_list(*list, warnings, where, depth));
            }
        }
    }
    if (PresetEffect* preset = dynamic_cast<PresetEffect*>(&e)) {
        if (const JsonValue* info = v.find("info")) {
            PresetInfo pi;
            if (info->is_object()) read_info(*info, pi);
            preset->set_info(pi);
        }
        if (const JsonValue* inner = v.find("chain")) {
            if (depth >= k_max_effect_depth) {
                warn(warnings, where + ": chains nested too deeply");
            } else if (inner->is_array()) {
                auto chain = std::make_shared<EffectChain>();
                chain->assign(parse_effect_list(*inner, warnings, where + ".chain", depth));
                preset->set_chain(std::move(chain));
            } else {
                std::string err;
                std::shared_ptr<Effect> ce = effect_parse(*inner, warnings, &err, where + ".chain", depth + 1);
                std::shared_ptr<EffectChain> chain = std::dynamic_pointer_cast<EffectChain>(ce);
                if (chain) {
                    preset->set_chain(std::move(chain));
                } else {
                    warn(warnings, where + ": 'chain' is not an effect chain");
                }
            }
        }
    }
}

std::shared_ptr<Effect> effect_parse(const JsonValue& v, std::vector<std::string>* warnings, std::string* error, const std::string& where, std::size_t depth) {
    if (!v.is_object()) {
        if (error) *error = "effect entry is not an object";
        return nullptr;
    }
    const JsonValue* t = v.find("type");
    if (!t || !t->is_string() || t->as_string().empty()) {
        if (error) *error = "effect entry has no 'type'";
        return nullptr;
    }
    const std::string type = t->as_string();
    EffectFactory f = registry().factory(type);
    std::shared_ptr<Effect> e = f ? f() : nullptr;
    if (!e) {
        warn(warnings, where + ": unknown effect type '" + type + "' kept as a pass-through placeholder");
        auto m = std::make_shared<MissingEffect>(type, v);
        if (const JsonValue* en = v.find("enabled")) m->set_enabled(en->is_bool() ? en->as_bool() : en->as_number(1.0) != 0.0);
        return m;
    }
    apply_effect_body(*e, v, warnings, where, depth);
    return e;
}

const JsonValue* preset_chain_node(const JsonValue& root) {
    if (const JsonValue* c = root.find("chain")) return c;
    if (root.find("effects")) return &root;
    return nullptr;
}

} // namespace

JsonValue JsonValue::make_bool(bool value) {
    JsonValue v;
    v.m_kind = Kind::boolean;
    v.m_bool = value;
    return v;
}

JsonValue JsonValue::make_number(double value) {
    JsonValue v;
    v.m_kind = Kind::number;
    v.m_number = value;
    return v;
}

JsonValue JsonValue::make_float(float value) {
    JsonValue v;
    v.m_kind = Kind::number;
    v.m_number = static_cast<double>(value);
    v.m_single = true;
    return v;
}

JsonValue JsonValue::make_string(std::string value) {
    JsonValue v;
    v.m_kind = Kind::string;
    v.m_string = std::move(value);
    return v;
}

JsonValue JsonValue::make_array() {
    JsonValue v;
    v.m_kind = Kind::array;
    return v;
}

JsonValue JsonValue::make_object() {
    JsonValue v;
    v.m_kind = Kind::object;
    return v;
}

bool JsonValue::as_bool(bool fallback) const noexcept {
    if (m_kind == Kind::boolean) return m_bool;
    if (m_kind == Kind::number) return m_number != 0.0;
    return fallback;
}

double JsonValue::as_number(double fallback) const noexcept {
    if (m_kind == Kind::number) return m_number;
    if (m_kind == Kind::boolean) return m_bool ? 1.0 : 0.0;
    return fallback;
}

float JsonValue::as_float(float fallback) const noexcept {
    if (m_kind != Kind::number && m_kind != Kind::boolean) return fallback;
    const double d = as_number();
    if (!(d == d)) return fallback;
    if (d > 3.4028234663852886e38) return 3.4028234663852886e38f;
    if (d < -3.4028234663852886e38) return -3.4028234663852886e38f;
    return static_cast<float>(d);
}

const std::string& JsonValue::as_string() const noexcept {
    static const std::string empty;
    return m_kind == Kind::string ? m_string : empty;
}

std::string JsonValue::as_string(const std::string& fallback) const {
    return m_kind == Kind::string ? m_string : fallback;
}

std::size_t JsonValue::size() const noexcept {
    return (m_kind == Kind::array || m_kind == Kind::object) ? m_items.size() : 0;
}

const JsonValue& JsonValue::at(std::size_t index) const noexcept {
    static const JsonValue null_value;
    return index < m_items.size() ? m_items[index] : null_value;
}

JsonValue& JsonValue::at(std::size_t index) noexcept {
    return m_items[index];
}

const std::string& JsonValue::key(std::size_t index) const noexcept {
    static const std::string empty;
    return index < m_keys.size() ? m_keys[index] : empty;
}

JsonValue& JsonValue::push(JsonValue value) {
    if (m_kind != Kind::array) {
        *this = make_array();
    }
    m_items.push_back(std::move(value));
    return m_items.back();
}

const JsonValue* JsonValue::find(const std::string& name) const noexcept {
    if (m_kind != Kind::object) return nullptr;
    for (std::size_t i = 0; i < m_keys.size(); ++i) {
        if (m_keys[i] == name) return &m_items[i];
    }
    return nullptr;
}

JsonValue* JsonValue::find(const std::string& name) noexcept {
    if (m_kind != Kind::object) return nullptr;
    for (std::size_t i = 0; i < m_keys.size(); ++i) {
        if (m_keys[i] == name) return &m_items[i];
    }
    return nullptr;
}

JsonValue& JsonValue::set(const std::string& name, JsonValue value) {
    if (m_kind != Kind::object) *this = make_object();
    for (std::size_t i = 0; i < m_keys.size(); ++i) {
        if (m_keys[i] == name) {
            m_items[i] = std::move(value);
            return m_items[i];
        }
    }
    m_keys.push_back(name);
    m_items.push_back(std::move(value));
    return m_items.back();
}

bool JsonValue::erase(const std::string& name) {
    if (m_kind != Kind::object) return false;
    for (std::size_t i = 0; i < m_keys.size(); ++i) {
        if (m_keys[i] == name) {
            m_keys.erase(m_keys.begin() + static_cast<std::ptrdiff_t>(i));
            m_items.erase(m_items.begin() + static_cast<std::ptrdiff_t>(i));
            return true;
        }
    }
    return false;
}

bool parse_json(const std::string& text, JsonValue& out, std::string* error) {
    Parser p(text);
    JsonValue v;
    if (!p.parse(v, error)) return false;
    out = std::move(v);
    return true;
}

std::string write_json(const JsonValue& value, bool pretty) {
    std::string out;
    write_value(value, out, pretty, 0);
    if (pretty) out += '\n';
    return out;
}

bool register_effect(const std::string& type, EffectFactory factory, const std::string& display_name, const std::string& category) {
    return registry().add(type, std::move(factory), display_name, category);
}

bool unregister_effect(const std::string& type) {
    return registry().remove(type);
}

bool has_effect_type(const std::string& type) {
    return static_cast<bool>(registry().factory(type));
}

std::shared_ptr<Effect> create_effect(const std::string& type) {
    EffectFactory f = registry().factory(type);
    return f ? f() : nullptr;
}

std::vector<EffectTypeInfo> effect_types() {
    return registry().list();
}

std::vector<EffectTypeInfo> effect_types(const std::string& category) {
    std::vector<EffectTypeInfo> all = registry().list();
    std::vector<EffectTypeInfo> out;
    for (EffectTypeInfo& i : all) {
        if (i.category == category) out.push_back(std::move(i));
    }
    return out;
}

bool effect_type_info(const std::string& type, EffectTypeInfo& out) {
    return registry().info(type, out);
}

MissingEffect::MissingEffect(std::string type, JsonValue data) : m_type(std::move(type)), m_data(std::move(data)) {}

void MissingEffect::prepare(std::uint32_t sample_rate, std::size_t channels) {
    set_format(sample_rate, channels);
}

void MissingEffect::process(float*, std::size_t) noexcept {}

void MissingEffect::reset() noexcept {}

JsonValue effect_to_json(const Effect& effect) {
    return effect_json(effect, 0);
}

std::shared_ptr<Effect> effect_from_json(const JsonValue& value, std::vector<std::string>* warnings, std::string* error) {
    return effect_parse(value, warnings, error, "effect", 0);
}

std::string save_effect(const Effect& effect, bool pretty) {
    return write_json(effect_to_json(effect), pretty);
}

std::shared_ptr<Effect> load_effect(const std::string& json, std::string* error, std::vector<std::string>* warnings) {
    JsonValue v;
    if (!parse_json(json, v, error)) return nullptr;
    return effect_from_json(v, warnings, error);
}

std::shared_ptr<Effect> clone_effect(const Effect& effect) {
    return effect_from_json(effect_to_json(effect), nullptr, nullptr);
}

std::string save_preset(const EffectChain& chain, const PresetInfo& info) {
    JsonValue root = JsonValue::make_object();
    root.set("format", JsonValue::make_string(k_format_name));
    root.set("format_version", JsonValue::make_number(k_format_version));
    JsonValue meta = info_json(info);
    for (std::size_t i = 0; i < meta.size(); ++i) root.set(meta.key(i), meta.at(i));
    root.set("chain", effect_to_json(chain));
    return write_json(root, true);
}

std::shared_ptr<EffectChain> load_preset_chain(const std::string& json, PresetInfo* info, std::string* error, std::vector<std::string>* warnings) {
    JsonValue root;
    if (!parse_json(json, root, error)) return nullptr;
    if (!root.is_object()) {
        if (error) *error = "preset is not a JSON object";
        return nullptr;
    }
    if (const JsonValue* f = root.find("format")) {
        if (f->as_string() != k_format_name) {
            if (error) *error = "not a fizmo audio preset (format '" + f->as_string() + "')";
            return nullptr;
        }
    }
    PresetInfo pi;
    read_info(root, pi);
    if (pi.format_version > k_format_version) warn(warnings, "preset format version " + std::to_string(pi.format_version) + " is newer than supported version " + std::to_string(k_format_version) + "; loading what is understood");
    const JsonValue* node = preset_chain_node(root);
    if (!node) {
        if (error) *error = "preset has no 'chain'";
        return nullptr;
    }
    auto chain = std::make_shared<EffectChain>();
    if (node->is_array()) {
        chain->assign(parse_effect_list(*node, warnings, "chain", 0));
    } else if (node->is_object()) {
        if (const JsonValue* t = node->find("type")) {
            if (t->as_string() != "chain") {
                if (error) *error = "preset 'chain' has type '" + t->as_string() + "', expected 'chain'";
                return nullptr;
            }
        }
        apply_effect_body(*chain, *node, warnings, "chain", 0);
    } else {
        if (error) *error = "preset 'chain' is not an object";
        return nullptr;
    }
    if (info) *info = pi;
    return chain;
}

bool load_preset(const std::string& json, EffectChain& out, PresetInfo* info, std::string* error, std::vector<std::string>* warnings) {
    std::shared_ptr<EffectChain> chain = load_preset_chain(json, info, error, warnings);
    if (!chain) return false;
    out.assign(chain->effects());
    out.set_enabled(chain->enabled());
    return true;
}

bool read_preset_info(const std::string& json, PresetInfo& info, std::string* error) {
    JsonValue root;
    if (!parse_json(json, root, error)) return false;
    if (!root.is_object()) {
        if (error) *error = "preset is not a JSON object";
        return false;
    }
    read_info(root, info);
    return true;
}

bool save_preset_file(const std::string& path, const EffectChain& chain, const PresetInfo& info, std::string* error) {
    return write_text_file(path, save_preset(chain, info), error);
}

bool load_preset_file(const std::string& path, EffectChain& out, PresetInfo* info, std::string* error, std::vector<std::string>* warnings) {
    bool ok = false;
    const std::string text = read_text_file(path, ok);
    if (!ok) {
        if (error) *error = "cannot open '" + path + "'";
        return false;
    }
    return load_preset(text, out, info, error, warnings);
}

std::shared_ptr<EffectChain> builtin_voice_preset(VoicePreset preset) {
    if (static_cast<std::size_t>(preset) >= voice_preset_count()) return nullptr;
    return make_voice_chain(preset);
}

std::shared_ptr<EffectChain> builtin_voice_preset(const std::string& name) {
    for (std::size_t i = 0; i < voice_preset_count(); ++i) {
        if (name == voice_preset_name(static_cast<VoicePreset>(i))) return make_voice_chain(static_cast<VoicePreset>(i));
    }
    return nullptr;
}

std::vector<std::string> builtin_voice_preset_names() {
    std::vector<std::string> out;
    for (std::size_t i = 0; i < voice_preset_count(); ++i) out.push_back(voice_preset_name(static_cast<VoicePreset>(i)));
    return out;
}

std::string builtin_voice_preset_json(VoicePreset preset) {
    std::shared_ptr<EffectChain> chain = builtin_voice_preset(preset);
    if (!chain) return std::string();
    PresetInfo info;
    info.name = voice_preset_name(preset);
    info.author = "fizmo";
    info.category = "voice";
    info.version = "1";
    return save_preset(*chain, info);
}

struct PresetEffect::State {
    static constexpr std::size_t chunk = 256;

    mutable std::mutex mutex;
    std::shared_ptr<EffectChain> current;
    std::size_t current_latency = 0;
    std::shared_ptr<EffectChain> pending;
    std::size_t pending_latency = 0;
    std::atomic<bool> dirty{ false };
    std::vector<std::shared_ptr<EffectChain>> retired;
    PresetInfo info;

    std::shared_ptr<EffectChain> live;
    std::size_t live_latency = 0;
    std::shared_ptr<EffectChain> old;
    std::size_t old_latency = 0;
    bool fading = false;
    bool old_done = false;
    std::size_t fade_pos = 0;
    std::size_t fade_len = 1;

    std::size_t channels = 0;
    std::vector<float> a;
    std::vector<float> b;
    std::vector<float> dry;
    std::size_t dry_mask = 0;
    std::size_t dry_pos = 0;
    float mix = 1.0f;
    float gain = 1.0f;
    float k = 0.01f;
};

PresetEffect::PresetEffect(std::shared_ptr<EffectChain> chain)
    : mix("mix", 1.0f, 0.0f, 1.0f),
      output("output", 0.0f, -48.0f, 24.0f, "dB"),
      crossfade_ms("crossfade_ms", 30.0f, 0.0f, 2000.0f, "ms"),
      m_state(new State()) {
    add_parameter(mix);
    add_parameter(output);
    add_parameter(crossfade_ms);
    m_state->retired.reserve(16);
    if (chain) {
        m_state->current = chain;
        m_state->current_latency = chain->latency_frames();
        m_state->live = chain;
        m_state->live_latency = m_state->current_latency;
    }
}

PresetEffect::~PresetEffect() = default;

void PresetEffect::prepare(std::uint32_t sample_rate, std::size_t channel_count) {
    const std::uint32_t r = sample_rate ? sample_rate : 48000;
    const std::size_t c = channel_count ? channel_count : 1;
    set_format(r, c);
    State& s = *m_state;
    std::lock_guard<std::mutex> lock(s.mutex);
    if (s.pending) {
        s.current = s.pending;
        s.pending.reset();
    }
    s.dirty.store(false);
    s.retired.clear();
    s.old.reset();
    s.fading = false;
    s.old_done = false;
    m_fading.store(false);
    if (s.current) {
        s.current->prepare(r, c);
        s.current_latency = s.current->latency_frames();
    } else {
        s.current_latency = 0;
    }
    s.live = s.current;
    s.live_latency = s.current_latency;
    s.channels = c;
    s.a.assign(State::chunk * c, 0.0f);
    s.b.assign(State::chunk * c, 0.0f);
    const std::size_t need = std::max<std::size_t>(s.live_latency + State::chunk + 1, static_cast<std::size_t>(r));
    const std::size_t dry_frames = next_power_of_two(need);
    s.dry.assign(dry_frames * c, 0.0f);
    s.dry_mask = dry_frames - 1;
    s.dry_pos = 0;
    s.k = 1.0f - std::exp(-1.0f / (0.01f * static_cast<float>(r)));
    s.mix = mix.get();
    s.gain = db_to_gain(output.get());
    if (s.live) s.live->reset();
}

void PresetEffect::process(float* io, std::size_t frames) noexcept {
    State& s = *m_state;
    const std::size_t c = s.channels;
    if (c == 0 || !io) return;
    if (!s.fading && s.old && s.old_done) {
        if (s.mutex.try_lock()) {
            if (s.retired.size() < s.retired.capacity()) {
                s.retired.push_back(std::move(s.old));
                s.old.reset();
                s.old_done = false;
            }
            s.mutex.unlock();
        }
    }
    if (!s.fading && !s.old && s.dirty.load(std::memory_order_acquire) && s.mutex.try_lock()) {
        if (s.pending) {
            s.old = std::move(s.live);
            s.old_latency = s.live_latency;
            s.live = std::move(s.pending);
            s.live_latency = s.pending_latency;
            s.pending.reset();
            const float ms = crossfade_ms.get();
            s.fade_len = std::max<std::size_t>(1, static_cast<std::size_t>(ms * 0.001f * static_cast<float>(sample_rate())));
            s.fade_pos = 0;
            s.fading = ms > 0.0f;
            s.old_done = !s.fading;
            m_fading.store(s.fading, std::memory_order_relaxed);
        }
        s.dirty.store(false, std::memory_order_relaxed);
        s.mutex.unlock();
    }
    const float mix_t = mix.get();
    const float gain_t = db_to_gain(output.get());
    const std::size_t latency = std::min(s.live_latency, s.dry_mask);
    for (std::size_t off = 0; off < frames; off += State::chunk) {
        const std::size_t n = std::min(State::chunk, frames - off);
        float* x = io + off * c;
        for (std::size_t i = 0; i < n * c; ++i) {
            const float v = x[i];
            s.a[i] = v == v ? v : 0.0f;
        }
        if (s.fading) std::memcpy(s.b.data(), s.a.data(), n * c * sizeof(float));
        if (s.live && s.live->enabled()) s.live->process(s.a.data(), n);
        if (s.fading && s.old && s.old->enabled()) s.old->process(s.b.data(), n);
        for (std::size_t f = 0; f < n; ++f) {
            float* p = x + f * c;
            const std::size_t dw = s.dry_pos & s.dry_mask;
            const std::size_t dr = (s.dry_pos - latency) & s.dry_mask;
            float wa = 1.0f;
            float wb = 0.0f;
            if (s.fading) {
                const float t = std::min(1.0f, static_cast<float>(s.fade_pos) / static_cast<float>(s.fade_len));
                wa = std::sin(t * 1.5707963267948966f);
                wb = std::cos(t * 1.5707963267948966f);
                ++s.fade_pos;
            }
            s.mix += (mix_t - s.mix) * s.k;
            s.gain += (gain_t - s.gain) * s.k;
            for (std::size_t ch = 0; ch < c; ++ch) {
                const float in = s.a[f * c + ch];
                const float raw = p[ch] == p[ch] ? p[ch] : 0.0f;
                s.dry[dw * c + ch] = raw;
                float wet = in * wa;
                if (s.fading) wet += s.b[f * c + ch] * wb;
                const float out = (s.dry[dr * c + ch] * (1.0f - s.mix) + wet * s.mix) * s.gain;
                p[ch] = out == out ? out : 0.0f;
            }
            ++s.dry_pos;
        }
        if (s.fading && s.fade_pos >= s.fade_len) {
            s.fading = false;
            s.old_done = true;
            m_fading.store(false, std::memory_order_relaxed);
        }
    }
}

void PresetEffect::reset() noexcept {
    State& s = *m_state;
    if (s.live) s.live->reset();
    if (s.old) s.old->reset();
    std::fill(s.dry.begin(), s.dry.end(), 0.0f);
    s.dry_pos = 0;
    s.mix = mix.get();
    s.gain = db_to_gain(output.get());
}

std::size_t PresetEffect::latency_frames() const noexcept {
    std::lock_guard<std::mutex> lock(m_state->mutex);
    return m_state->pending ? m_state->pending_latency : m_state->current_latency;
}

std::size_t PresetEffect::tail_frames() const noexcept {
    std::shared_ptr<EffectChain> c = chain();
    return c ? c->tail_frames() : 0;
}

void PresetEffect::set_chain(std::shared_ptr<EffectChain> chain) {
    State& s = *m_state;
    if (chain && sample_rate()) {
        chain->prepare(sample_rate(), channels());
        chain->reset();
    }
    const std::size_t latency = chain ? chain->latency_frames() : 0;
    std::lock_guard<std::mutex> lock(s.mutex);
    s.retired.clear();
    s.current = chain;
    s.current_latency = latency;
    if (!sample_rate()) {
        s.live = chain;
        s.live_latency = latency;
        s.pending.reset();
        s.dirty.store(false);
        return;
    }
    s.pending = std::move(chain);
    s.pending_latency = latency;
    s.dirty.store(true, std::memory_order_release);
}

std::shared_ptr<EffectChain> PresetEffect::chain() const {
    std::lock_guard<std::mutex> lock(m_state->mutex);
    return m_state->current;
}

bool PresetEffect::load(const std::string& json, std::string* error, std::vector<std::string>* warnings) {
    PresetInfo info;
    std::shared_ptr<EffectChain> c = load_preset_chain(json, &info, error, warnings);
    if (!c) return false;
    set_info(info);
    set_chain(std::move(c));
    return true;
}

bool PresetEffect::load_file(const std::string& path, std::string* error, std::vector<std::string>* warnings) {
    bool ok = false;
    const std::string text = read_text_file(path, ok);
    if (!ok) {
        if (error) *error = "cannot open '" + path + "'";
        return false;
    }
    return load(text, error, warnings);
}

std::string PresetEffect::save() const {
    std::shared_ptr<EffectChain> c = chain();
    EffectChain empty;
    return save_preset(c ? *c : empty, info());
}

bool PresetEffect::save_file(const std::string& path, std::string* error) const {
    return write_text_file(path, save(), error);
}

PresetInfo PresetEffect::info() const {
    std::lock_guard<std::mutex> lock(m_state->mutex);
    return m_state->info;
}

void PresetEffect::set_info(const PresetInfo& info) {
    std::lock_guard<std::mutex> lock(m_state->mutex);
    m_state->info = info;
}

} // namespace audio
} // namespace fizmo
