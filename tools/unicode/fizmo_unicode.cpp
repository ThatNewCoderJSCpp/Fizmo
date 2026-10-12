#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <tuple>
#include <unordered_map>
#include <vector>

namespace {

constexpr std::uint32_t max_cp = 0x110000;

const char* const gc_names[] = {"Cn", "Lu", "Ll", "Lt", "Lm", "Lo", "Mn", "Mc", "Me", "Nd", "Nl", "No", "Pc", "Pd", "Ps",
                                "Pe", "Pi", "Pf", "Po", "Sm", "Sc", "Sk", "So", "Zs", "Zl", "Zp", "Cc", "Cf", "Cs", "Co"};
const char* const gcb_names[] = {"Other", "CR", "LF", "Control", "Extend", "ZWJ", "Regional_Indicator", "Prepend", "SpacingMark",
                                 "L", "V", "T", "LV", "LVT"};
const char* const wb_names[] = {"Other", "CR", "LF", "Newline", "Extend", "ZWJ", "Regional_Indicator", "Format", "Katakana",
                                "Hebrew_Letter", "ALetter", "Single_Quote", "Double_Quote", "MidNumLet", "MidLetter", "MidNum",
                                "Numeric", "ExtendNumLet", "WSegSpace"};
const char* const incb_names[] = {"None", "Consonant", "Extend", "Linker"};

enum : std::uint16_t {
    f_ext_pict           = 1u << 0,
    f_white_space        = 1u << 1,
    f_alphabetic         = 1u << 2,
    f_lowercase          = 1u << 3,
    f_uppercase          = 1u << 4,
    f_cased              = 1u << 5,
    f_case_ignorable     = 1u << 6,
    f_default_ignorable  = 1u << 7,
    f_emoji_presentation = 1u << 8,
    f_special_case       = 1u << 9,
    f_canon_decomp       = 1u << 10,
    f_compat_decomp      = 1u << 11,
    f_emoji              = 1u << 12,
    f_nfc_qc_not_yes     = 1u << 13,
    f_nfkc_qc_not_yes    = 1u << 14,
    f_composes_second    = 1u << 15
};

using Seq = std::vector<std::uint32_t>;

struct Range {
    std::uint32_t lo;
    std::uint32_t hi;
    std::string   value;
};

[[noreturn]] void fail(const std::string& msg) {
    std::fprintf(stderr, "fizmo_unicode: %s\n", msg.c_str());
    std::exit(1);
}

std::string trim(const std::string& s) {
    std::size_t a = 0, b = s.size();
    while (a < b && (s[a] == ' ' || s[a] == '\t' || s[a] == '\r' || s[a] == '\n')) ++a;
    while (b > a && (s[b - 1] == ' ' || s[b - 1] == '\t' || s[b - 1] == '\r' || s[b - 1] == '\n')) --b;
    return s.substr(a, b - a);
}

std::vector<std::vector<std::string>> read_lines(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) fail("cannot open " + path);
    std::vector<std::vector<std::string>> rows;
    std::string raw;
    while (std::getline(in, raw)) {
        const std::size_t hash = raw.find('#');
        const std::string line = trim(hash == std::string::npos ? raw : raw.substr(0, hash));
        if (line.empty()) continue;
        std::vector<std::string> parts;
        std::size_t start = 0;
        for (;;) {
            const std::size_t semi = line.find(';', start);
            parts.push_back(trim(line.substr(start, semi == std::string::npos ? std::string::npos : semi - start)));
            if (semi == std::string::npos) break;
            start = semi + 1;
        }
        rows.push_back(std::move(parts));
    }
    return rows;
}

std::uint32_t hex(const std::string& s) { return static_cast<std::uint32_t>(std::strtoul(s.c_str(), nullptr, 16)); }

Seq hex_list(const std::string& s) {
    Seq out;
    std::istringstream is(s);
    std::string tok;
    while (is >> tok) out.push_back(hex(tok));
    return out;
}

void parse_range(const std::string& text, std::uint32_t& lo, std::uint32_t& hi) {
    const std::size_t dots = text.find("..");
    if (dots == std::string::npos) {
        lo = hi = hex(text);
    } else {
        lo = hex(text.substr(0, dots));
        hi = hex(text.substr(dots + 2));
    }
}

std::map<std::string, std::vector<Range>> load_property_file(const std::string& path) {
    std::map<std::string, std::vector<Range>> result;
    for (const auto& parts : read_lines(path)) {
        if (parts.size() < 2) continue;
        Range r;
        parse_range(parts[0], r.lo, r.hi);
        r.value = parts.size() > 2 ? parts[2] : std::string();
        result[parts[1]].push_back(r);
    }
    return result;
}

template <std::size_t N>
int index_of(const char* const (&names)[N], const std::string& name) {
    for (std::size_t i = 0; i < N; ++i) {
        if (name == names[i]) return static_cast<int>(i);
    }
    fail("unknown property value " + name);
}

struct Decomp {
    bool compat;
    Seq  mapping;
};

struct Data {
    std::vector<std::uint8_t>  gc, ccc, gcb, wb, incb, width;
    std::vector<std::uint16_t> flags;
    std::vector<char>          eaw;
    std::map<std::uint32_t, Decomp>        decomp;
    std::map<std::uint32_t, std::uint32_t> s_upper, s_lower, s_title, s_fold;
    std::map<std::uint32_t, Seq>           f_fold;
    std::map<std::uint32_t, std::array<Seq, 3>> special;
    std::set<std::uint32_t> exclusions;
};

void load_unicode_data(const std::string& path, Data& d) {
    std::uint32_t first = 0;
    for (const auto& parts : read_lines(path)) {
        if (parts.size() < 15) continue;
        const std::uint32_t cp   = hex(parts[0]);
        const std::string&  name = parts[1];
        const std::uint8_t  cat  = static_cast<std::uint8_t>(index_of(gc_names, parts[2]));
        const std::uint8_t  cc   = static_cast<std::uint8_t>(std::atoi(parts[3].c_str()));
        auto ends_with = [&](const char* suffix) {
            const std::string s(suffix);
            return name.size() >= s.size() && name.compare(name.size() - s.size(), s.size(), s) == 0;
        };
        if (ends_with(", First>")) {
            first = cp;
            continue;
        }
        if (ends_with(", Last>")) {
            for (std::uint32_t c = first; c <= cp; ++c) {
                d.gc[c]  = cat;
                d.ccc[c] = cc;
            }
            continue;
        }
        d.gc[cp]  = cat;
        d.ccc[cp] = cc;
        if (!parts[5].empty()) {
            std::string field = parts[5];
            Decomp      dec{false, {}};
            if (field[0] == '<') {
                dec.compat = true;
                field      = field.substr(field.find('>') + 1);
            }
            dec.mapping   = hex_list(field);
            d.decomp[cp] = dec;
        }
        if (!parts[12].empty()) d.s_upper[cp] = hex(parts[12]);
        if (!parts[13].empty()) d.s_lower[cp] = hex(parts[13]);
        if (!parts[14].empty()) d.s_title[cp] = hex(parts[14]);
    }
}

Seq full_decomposition(std::uint32_t cp, const Data& d, bool compat, std::map<std::pair<std::uint32_t, bool>, Seq>& memo) {
    const auto key = std::make_pair(cp, compat);
    const auto it  = memo.find(key);
    if (it != memo.end()) return it->second;
    Seq res;
    if (cp >= 0xAC00 && cp <= 0xD7A3) {
        const std::uint32_t s = cp - 0xAC00;
        res.push_back(0x1100 + s / 588);
        res.push_back(0x1161 + (s % 588) / 28);
        if (s % 28) res.push_back(0x11A7 + s % 28);
    } else {
        const auto e = d.decomp.find(cp);
        if (e == d.decomp.end() || (e->second.compat && !compat)) {
            res.push_back(cp);
        } else {
            for (std::uint32_t c : e->second.mapping) {
                const Seq sub = full_decomposition(c, d, compat, memo);
                res.insert(res.end(), sub.begin(), sub.end());
            }
        }
    }
    memo[key] = res;
    return res;
}

std::string detect_version(const std::string& dir) {
    std::ifstream in(dir + "/DerivedCoreProperties.txt");
    std::string line;
    std::getline(in, line);
    const std::size_t dash = line.rfind('-');
    const std::size_t dot  = line.rfind(".txt");
    if (dash == std::string::npos || dot == std::string::npos || dot <= dash) return "unknown";
    return line.substr(dash + 1, dot - dash - 1);
}

std::string hex_str(std::uint32_t v) {
    char buf[16];
    std::snprintf(buf, sizeof(buf), "0x%X", static_cast<unsigned>(v));
    return buf;
}

std::string triple(const Seq& s) {
    std::string out = "{";
    for (std::size_t i = 0; i < 3; ++i) {
        if (i) out += ", ";
        out += hex_str(i < s.size() ? s[i] : 0);
    }
    return out + "}";
}

template <class T, class F>
void write_array(std::ostream& out, const std::vector<T>& values, std::size_t per_line, F fmt) {
    for (std::size_t i = 0; i < values.size(); i += per_line) {
        out << "    ";
        for (std::size_t j = i; j < values.size() && j < i + per_line; ++j) {
            if (j > i) out << ", ";
            out << fmt(values[j]);
        }
        out << ",\n";
    }
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 3) {
        std::fprintf(stderr, "usage: fizmo_unicode <ucd folder> <output header> [version]\n");
        return 2;
    }
    const std::string dir     = argv[1];
    const std::string out     = argv[2];
    const std::string version = argc > 3 ? argv[3] : detect_version(dir);
    auto p = [&](const char* name) { return dir + "/" + name; };

    Data d;
    d.gc.assign(max_cp, 0);
    d.ccc.assign(max_cp, 0);
    d.gcb.assign(max_cp, 0);
    d.wb.assign(max_cp, 0);
    d.incb.assign(max_cp, 0);
    d.width.assign(max_cp, 1);
    d.flags.assign(max_cp, 0);
    d.eaw.assign(max_cp, 'N');

    load_unicode_data(p("UnicodeData.txt"), d);

    auto fill_flag = [&](const std::vector<Range>& ranges, std::uint16_t bit) {
        for (const Range& r : ranges) {
            for (std::uint32_t c = r.lo; c <= r.hi; ++c) d.flags[c] |= bit;
        }
    };

    for (const auto& kv : load_property_file(p("GraphemeBreakProperty.txt"))) {
        const auto v = static_cast<std::uint8_t>(index_of(gcb_names, kv.first));
        for (const Range& r : kv.second) std::fill(d.gcb.begin() + r.lo, d.gcb.begin() + r.hi + 1, v);
    }
    for (const auto& kv : load_property_file(p("WordBreakProperty.txt"))) {
        const auto v = static_cast<std::uint8_t>(index_of(wb_names, kv.first));
        for (const Range& r : kv.second) std::fill(d.wb.begin() + r.lo, d.wb.begin() + r.hi + 1, v);
    }

    auto core = load_property_file(p("DerivedCoreProperties.txt"));
    fill_flag(core["Alphabetic"], f_alphabetic);
    fill_flag(core["Lowercase"], f_lowercase);
    fill_flag(core["Uppercase"], f_uppercase);
    fill_flag(core["Cased"], f_cased);
    fill_flag(core["Case_Ignorable"], f_case_ignorable);
    fill_flag(core["Default_Ignorable_Code_Point"], f_default_ignorable);
    for (const Range& r : core["InCB"]) {
        const auto v = static_cast<std::uint8_t>(index_of(incb_names, r.value));
        std::fill(d.incb.begin() + r.lo, d.incb.begin() + r.hi + 1, v);
    }

    auto props = load_property_file(p("PropList.txt"));
    fill_flag(props["White_Space"], f_white_space);

    auto emoji = load_property_file(p("emoji-data.txt"));
    fill_flag(emoji["Extended_Pictographic"], f_ext_pict);
    fill_flag(emoji["Emoji_Presentation"], f_emoji_presentation);
    fill_flag(emoji["Emoji"], f_emoji);

    for (const auto& parts : read_lines(p("EastAsianWidth.txt"))) {
        if (parts.size() < 2 || parts[1].empty()) continue;
        std::uint32_t lo, hi;
        parse_range(parts[0], lo, hi);
        const char w = parts[1] == "W" ? 'W' : parts[1] == "F" ? 'F' : 'N';
        std::fill(d.eaw.begin() + lo, d.eaw.begin() + hi + 1, w);
    }

    auto norm = load_property_file(p("DerivedNormalizationProps.txt"));
    for (const Range& r : norm["Full_Composition_Exclusion"]) {
        for (std::uint32_t c = r.lo; c <= r.hi; ++c) d.exclusions.insert(c);
    }
    fill_flag(norm["NFC_QC"], f_nfc_qc_not_yes);
    fill_flag(norm["NFKC_QC"], f_nfkc_qc_not_yes);

    for (const auto& parts : read_lines(p("CaseFolding.txt"))) {
        if (parts.size() < 3) continue;
        const std::uint32_t cp      = hex(parts[0]);
        const Seq           mapping = hex_list(parts[2]);
        if (parts[1] == "C" || parts[1] == "S") d.s_fold[cp] = mapping[0];
        if (parts[1] == "C" || parts[1] == "F") d.f_fold[cp] = mapping;
    }

    for (const auto& parts : read_lines(p("SpecialCasing.txt"))) {
        if (parts.size() < 4) continue;
        if (parts.size() > 4 && !parts[4].empty()) continue;
        d.special[hex(parts[0])] = {hex_list(parts[1]), hex_list(parts[2]), hex_list(parts[3])};
    }

    for (std::uint32_t c = 0; c < max_cp; ++c) {
        const std::string g = gc_names[d.gc[c]];
        const std::string b = gcb_names[d.gcb[c]];
        const bool wide     = d.eaw[c] == 'W' || d.eaw[c] == 'F';
        if (wide || (d.flags[c] & f_emoji_presentation)) d.width[c] = 2;
        if (g == "Mn" || g == "Me" || g == "Cc" || g == "Cs" || (g == "Cf" && c != 0x00AD) || (d.flags[c] & f_default_ignorable) ||
            b == "Extend" || b == "ZWJ" || b == "V" || b == "T") {
            d.width[c] = 0;
        }
        if (g == "Cn" && !(d.flags[c] & f_default_ignorable) && !wide) d.width[c] = 1;
    }
    d.width[0] = 0;

    std::map<std::pair<std::uint32_t, bool>, Seq> memo;
    std::map<std::uint32_t, Seq> canon_full, compat_full;
    std::vector<std::uint32_t> decomp_sources;
    for (const auto& kv : d.decomp) decomp_sources.push_back(kv.first);
    for (std::uint32_t c = 0xAC00; c <= 0xD7A3; ++c) decomp_sources.push_back(c);
    for (std::uint32_t cp : decomp_sources) {
        const Seq cf = full_decomposition(cp, d, false, memo);
        const Seq kf = full_decomposition(cp, d, true, memo);
        if (!(cf.size() == 1 && cf[0] == cp)) {
            canon_full[cp] = cf;
            d.flags[cp] |= f_canon_decomp;
        }
        if (!(kf.size() == 1 && kf[0] == cp)) {
            compat_full[cp] = kf;
            d.flags[cp] |= f_compat_decomp;
        }
    }

    std::vector<std::array<std::uint32_t, 3>> compositions;
    for (const auto& kv : d.decomp) {
        if (!kv.second.compat && kv.second.mapping.size() == 2 && !d.exclusions.count(kv.first)) {
            compositions.push_back({kv.second.mapping[0], kv.second.mapping[1], kv.first});
            d.flags[kv.second.mapping[1]] |= f_composes_second;
        }
    }
    for (std::uint32_t c = 0x1161; c < 0x1176; ++c) d.flags[c] |= f_composes_second;
    for (std::uint32_t c = 0x11A8; c < 0x11C3; ++c) d.flags[c] |= f_composes_second;
    std::sort(compositions.begin(), compositions.end());

    auto lookup = [](const std::map<std::uint32_t, std::uint32_t>& m, std::uint32_t c, std::uint32_t def) {
        const auto it = m.find(c);
        return it == m.end() ? def : it->second;
    };

    struct SpecialRow {
        std::uint32_t cp;
        Seq lower, upper, title, fold;
    };
    std::vector<SpecialRow> special_rows;
    using Key = std::tuple<std::int64_t, std::int64_t, std::int64_t, std::int64_t, int, int, int, int, int, int, int>;
    std::map<Key, std::uint16_t> record_ids;
    std::vector<Key>             records;
    std::vector<std::uint16_t>   index(max_cp);

    for (std::uint32_t c = 0; c < max_cp; ++c) {
        const std::uint32_t su = lookup(d.s_upper, c, c);
        const std::uint32_t sl = lookup(d.s_lower, c, c);
        const std::uint32_t st = lookup(d.s_title, c, su);
        const std::uint32_t sf = lookup(d.s_fold, c, c);
        Seq fl{sl}, ft{st}, fu{su}, ff{sf};
        const auto sp = d.special.find(c);
        if (sp != d.special.end()) {
            fl = sp->second[0];
            ft = sp->second[1];
            fu = sp->second[2];
        }
        const auto fo = d.f_fold.find(c);
        if (fo != d.f_fold.end()) ff = fo->second;
        if (fl != Seq{sl} || ft != Seq{st} || fu != Seq{su} || ff != Seq{sf}) {
            d.flags[c] |= f_special_case;
            special_rows.push_back({c, fl, fu, ft, ff});
        }
        const Key key(std::int64_t(su) - c, std::int64_t(sl) - c, std::int64_t(st) - c, std::int64_t(sf) - c, d.gc[c], d.gcb[c], d.wb[c],
                      d.ccc[c], d.flags[c], d.incb[c], d.width[c]);
        const auto it = record_ids.find(key);
        if (it == record_ids.end()) {
            const auto id = static_cast<std::uint16_t>(records.size());
            record_ids.emplace(key, id);
            records.push_back(key);
            index[c] = id;
        } else {
            index[c] = it->second;
        }
    }

    std::size_t                best_size = 0;
    unsigned                   shift     = 0;
    std::vector<std::uint16_t> stage1, stage2;
    for (unsigned sh = 5; sh < 10; ++sh) {
        const std::uint32_t block = 1u << sh;
        std::map<std::vector<std::uint16_t>, std::uint16_t> blocks;
        std::vector<std::uint16_t> s1, s2;
        for (std::uint32_t start = 0; start < max_cp; start += block) {
            std::vector<std::uint16_t> chunk(index.begin() + start, index.begin() + start + block);
            const auto it = blocks.find(chunk);
            if (it == blocks.end()) {
                const auto id = static_cast<std::uint16_t>(blocks.size());
                blocks.emplace(chunk, id);
                s2.insert(s2.end(), chunk.begin(), chunk.end());
                s1.push_back(id);
            } else {
                s1.push_back(it->second);
            }
        }
        const std::size_t size = s1.size() * 2 + s2.size() * 2;
        if (shift == 0 || size < best_size) {
            best_size = size;
            shift     = sh;
            stage1    = std::move(s1);
            stage2    = std::move(s2);
        }
    }

    std::set<std::uint32_t> keys;
    for (const auto& kv : canon_full) keys.insert(kv.first);
    for (const auto& kv : compat_full) keys.insert(kv.first);

    std::vector<std::uint32_t> pool_data;
    std::map<Seq, std::size_t> pool;
    auto intern = [&](const Seq& s) {
        const auto it = pool.find(s);
        if (it != pool.end()) return it->second;
        const std::size_t off = pool_data.size();
        pool_data.insert(pool_data.end(), s.begin(), s.end());
        pool.emplace(s, off);
        return off;
    };

    struct Entry {
        std::uint32_t cp;
        std::size_t   co, cl, ko, kl;
    };
    std::vector<Entry> entries;
    std::size_t        max_len = 0;
    for (std::uint32_t cp : keys) {
        if (cp >= 0xAC00 && cp <= 0xD7A3) continue;
        const auto ci = canon_full.find(cp);
        const auto ki = compat_full.find(cp);
        const Seq  cs = ci == canon_full.end() ? Seq{} : ci->second;
        const Seq  ks = ki == compat_full.end() ? cs : ki->second;
        const std::size_t co = cs.empty() ? 0 : intern(cs);
        const std::size_t ko = ks.empty() ? 0 : intern(ks);
        entries.push_back({cp, co, cs.size(), ko, ks.size()});
        max_len = std::max(max_len, ks.size());
    }
    if (pool_data.size() > 0xFFFF) fail("decomposition data does not fit 16-bit offsets");
    if (records.size() > 0xFFFF || stage2.size() / (1u << shift) > 0xFFFF) fail("tables do not fit 16-bit indices");

    std::ostringstream os;
    os << "#ifndef FIZMO_UNICODE_DATA_HPP\n#define FIZMO_UNICODE_DATA_HPP\n\n";
    os << "#include <cstddef>\n#include <cstdint>\n\n";
    os << "namespace fizmo {\nnamespace unicode {\nnamespace tables {\n\n";
    os << "constexpr char version[] = \"" << version << "\";\n";
    os << "constexpr unsigned stage_shift = " << shift << ";\n";
    os << "constexpr std::size_t max_decomposition_length = " << max_len << ";\n\n";
    os << "constexpr Record records[] = {\n";
    for (const Key& k : records) {
        os << "    {" << std::get<0>(k) << ", " << std::get<1>(k) << ", " << std::get<2>(k) << ", " << std::get<3>(k) << ", "
           << std::get<8>(k) << ", " << std::get<4>(k) << ", " << std::get<5>(k) << ", " << std::get<6>(k) << ", " << std::get<7>(k)
           << ", " << std::get<9>(k) << ", " << std::get<10>(k) << "},\n";
    }
    os << "};\n\n";
    os << "constexpr std::uint16_t stage1[] = {\n";
    write_array(os, stage1, 16, [](std::uint16_t v) { return std::to_string(v); });
    os << "};\n\n";
    os << "constexpr std::uint16_t stage2[] = {\n";
    write_array(os, stage2, 16, [](std::uint16_t v) { return std::to_string(v); });
    os << "};\n\n";
    os << "constexpr DecompositionEntry decompositions[] = {\n";
    for (const Entry& e : entries) os << "    {" << hex_str(e.cp) << ", " << e.co << ", " << e.cl << ", " << e.ko << ", " << e.kl << "},\n";
    os << "};\n";
    os << "constexpr std::size_t decomposition_count = sizeof(decompositions) / sizeof(decompositions[0]);\n\n";
    os << "constexpr char32_t decomposition_data[] = {\n";
    write_array(os, pool_data, 12, [](std::uint32_t v) { return hex_str(v); });
    os << "};\n\n";
    os << "constexpr CompositionEntry compositions[] = {\n";
    for (const auto& c : compositions) os << "    {" << hex_str(c[0]) << ", " << hex_str(c[1]) << ", " << hex_str(c[2]) << "},\n";
    os << "};\n";
    os << "constexpr std::size_t composition_count = sizeof(compositions) / sizeof(compositions[0]);\n\n";
    os << "constexpr SpecialCaseEntry special_cases[] = {\n";
    for (const SpecialRow& r : special_rows) {
        os << "    {" << hex_str(r.cp) << ", " << triple(r.lower) << ", " << triple(r.upper) << ", " << triple(r.title) << ", " << triple(r.fold) << "},\n";
    }
    os << "};\n";
    os << "constexpr std::size_t special_case_count = sizeof(special_cases) / sizeof(special_cases[0]);\n\n";
    os << "} // namespace tables\n} // namespace unicode\n} // namespace fizmo\n\n#endif // FIZMO_UNICODE_DATA_HPP\n";

    const std::string text = os.str();
    {
        std::ifstream existing(out, std::ios::binary);
        if (existing) {
            std::ostringstream cur;
            cur << existing.rdbuf();
            if (cur.str() == text) {
                std::printf("fizmo: Unicode %s tables are up to date\n", version.c_str());
                return 0;
            }
        }
    }
    std::ofstream file(out, std::ios::binary);
    if (!file) fail("cannot write " + out);
    file << text;
    if (!file) fail("failed writing " + out);
    std::printf("fizmo: generated Unicode %s tables (%zu records, %zu decompositions, %zu compositions)\n", version.c_str(), records.size(),
                entries.size(), compositions.size());
    return 0;
}
