import math
import re
import sqlite3
import sys
from collections import defaultdict
from pathlib import Path

from mendeleev.db import get_package_dbpath
import radioactivedecay

OUT = Path(sys.argv[1]) if len(sys.argv) > 1 else Path(__file__).resolve().parents[2] / "Source" / "Chemistry" / "Data"

CIAAW_2024 = """
1 H [1.00784,1.00811]
2 He 4.002602(2)
3 Li [6.938,6.997]
4 Be 9.0121831(5)
5 B [10.806,10.821]
6 C [12.0096,12.0116]
7 N [14.00643,14.00728]
8 O [15.99903,15.99977]
9 F 18.998403162(5)
10 Ne 20.1797(6)
11 Na 22.98976928(2)
12 Mg [24.304,24.307]
13 Al 26.9815384(3)
14 Si [28.084,28.086]
15 P 30.973761998(5)
16 S [32.059,32.076]
17 Cl [35.446,35.457]
18 Ar [39.792,39.963]
19 K 39.0983(1)
20 Ca 40.078(4)
21 Sc 44.955907(4)
22 Ti 47.867(1)
23 V 50.9415(1)
24 Cr 51.9961(6)
25 Mn 54.938043(2)
26 Fe 55.845(2)
27 Co 58.933194(3)
28 Ni 58.6934(4)
29 Cu 63.546(3)
30 Zn 65.38(2)
31 Ga 69.723(1)
32 Ge 72.630(8)
33 As 74.921595(6)
34 Se 78.971(8)
35 Br [79.901,79.907]
36 Kr 83.798(2)
37 Rb 85.4678(3)
38 Sr 87.62(1)
39 Y 88.905838(2)
40 Zr 91.222(3)
41 Nb 92.90637(1)
42 Mo 95.95(1)
44 Ru 101.07(2)
45 Rh 102.90549(2)
46 Pd 106.42(1)
47 Ag 107.8682(2)
48 Cd 112.414(4)
49 In 114.818(1)
50 Sn 118.710(7)
51 Sb 121.760(1)
52 Te 127.60(3)
53 I 126.90447(3)
54 Xe 131.293(6)
55 Cs 132.90545196(6)
56 Ba 137.327(7)
57 La 138.90547(7)
58 Ce 140.116(1)
59 Pr 140.90766(1)
60 Nd 144.242(3)
62 Sm 150.36(2)
63 Eu 151.964(1)
64 Gd 157.249(2)
65 Tb 158.925354(7)
66 Dy 162.500(1)
67 Ho 164.930329(5)
68 Er 167.259(3)
69 Tm 168.934219(5)
70 Yb 173.045(10)
71 Lu 174.96669(5)
72 Hf 178.486(6)
73 Ta 180.94788(2)
74 W 183.84(1)
75 Re 186.207(1)
76 Os 190.23(3)
77 Ir 192.217(2)
78 Pt 195.084(9)
79 Au 196.966570(4)
80 Hg 200.592(3)
81 Tl [204.382,204.385]
82 Pb [206.14,207.94]
83 Bi 208.98040(1)
90 Th 232.0377(4)
91 Pa 231.03588(1)
92 U 238.02891(3)
"""

CONVENTIONAL = {1: 1.008, 3: 6.94, 5: 10.81, 6: 12.011, 7: 14.007, 8: 15.999, 12: 24.305, 14: 28.085, 16: 32.06,
                17: 35.45, 18: 39.95, 35: 79.904, 81: 204.38, 82: 207.2}

REFERENCE_OVERRIDE = {107: 270, 114: 290}

TERM_OVERRIDE = {82: (3, 1, False, 0)}

ELECTRON_AFFINITY = {
    1: ("0.754195(19)", "m"), 2: ("-0.5(2)", "e"), 3: ("0.618049(22)", "m"), 4: ("-0.5(2)", "e"), 5: ("0.279723(25)", "m"),
    6: ("1.2621226(11)", "m"), 7: ("-0.07", "m"), 8: ("1.46111297(9)", "m"), 9: ("3.4011898(24)", "m"), 10: ("-1.2(2)", "e"),
    11: ("0.547926(25)", "m"), 12: ("-0.4(2)", "e"), 13: ("0.43283(5)", "m"), 14: ("1.3895212(8)", "m"), 15: ("0.746609(11)", "m"),
    16: ("2.0771042(6)", "m"), 17: ("3.612725(28)", "m"), 18: ("-1.0(2)", "e"), 19: ("0.501459(13)", "m"), 20: ("0.02455(10)", "m"),
    21: ("0.179380(23)", "m"), 22: ("0.07554(5)", "m"), 23: ("0.52766(20)", "m"), 24: ("0.675928(27)", "m"), 25: ("-0.5(2)", "e"),
    26: ("0.153236(35)", "m"), 27: ("0.662255(47)", "m"), 28: ("1.15716(12)", "m"), 29: ("1.23578(4)", "m"), 30: ("-0.6(2)", "e"),
    31: ("0.301166(15)", "m"), 32: ("1.2326764(13)", "m"), 33: ("0.804486(3)", "m"), 34: ("2.020667(5)", "m"), 35: ("3.363588(3)", "m"),
    36: ("-1.0(2)", "e"), 37: ("0.485916(21)", "m"), 38: ("0.05206(6)", "m"), 39: ("0.31129(22)", "m"), 40: ("0.43328(9)", "m"),
    41: ("0.91740(7)", "m"), 42: ("0.74723(8)", "m"), 43: ("0.55(20)", "e"), 44: ("1.04627(2)", "m"), 45: ("1.14289(20)", "m"),
    46: ("0.56214(12)", "m"), 47: ("1.30447(3)", "m"), 48: ("-0.7(2)", "e"), 49: ("0.38392(6)", "m"), 50: ("1.112070(2)", "m"),
    51: ("1.047401(19)", "m"), 52: ("1.970875(7)", "m"), 53: ("3.0590465(37)", "m"), 54: ("-0.8(2)", "e"), 55: ("0.4715983(38)", "m"),
    56: ("0.14462(6)", "m"), 57: ("0.557546(20)", "m"), 58: ("0.600160(27)", "m"), 59: ("0.10923(46)", "m"), 60: ("0.09749(33)", "m"),
    61: ("0.129", "c"), 62: ("0.162", "c"), 63: ("0.116(13)", "m"), 64: ("0.212(30)", "m"), 65: ("0.13131(80)", "m"),
    66: ("0.015(3)", "m"), 67: ("0.338", "c"), 68: ("0.312", "c"), 69: ("1.029(22)", "m"), 70: ("-0.02", "e"),
    71: ("0.2388(7)", "m"), 72: ("0.1780(7)", "m"), 73: ("0.328859(23)", "m"), 74: ("0.816500(82)", "m"), 75: ("0.060396(64)", "m"),
    76: ("1.077661(24)", "m"), 77: ("1.564057(12)", "m"), 78: ("2.12510(5)", "m"), 79: ("2.308610(25)", "m"), 80: ("-0.5(2)", "e"),
    81: ("0.320053(19)", "m"), 82: ("0.356721(2)", "m"), 83: ("0.942362(13)", "m"), 84: ("1.40(7)", "c"), 85: ("2.41578(7)", "m"),
    86: ("-0.7(2)", "e"), 87: ("0.486", "e"), 88: ("0.10", "e"), 89: ("0.35", "e"), 90: ("0.60769(6)", "m"),
    91: ("0.55", "e"), 92: ("0.31497(9)", "m"), 93: ("0.48", "e"), 94: ("-0.50", "e"), 95: ("0.10", "e"),
    96: ("0.28", "e"), 97: ("-1.72", "e"), 98: ("-1.01", "e"), 99: ("-0.30", "e"), 100: ("0.35", "e"),
    101: ("0.98", "e"), 102: ("-2.33", "e"), 103: ("-0.31", "e"), 111: ("1.565", "c"), 113: ("0.69", "c"),
    115: ("0.366", "c"), 116: ("0.776", "c"), 117: ("1.719", "c"), 118: ("0.080(6)", "c"),
}

EXTRA_PAULING = {36: 3.00, 86: 2.2}
ALLEN_EV_TO_PAULING = 0.169

ALKALI = {3, 11, 19, 37, 55, 87}
ALKALINE = {4, 12, 20, 38, 56, 88}
METALLOID = {5, 14, 32, 33, 51, 52}
HALOGEN = {9, 17, 35, 53, 85, 117}
NOBLE = {2, 10, 18, 36, 54, 86, 118}
REACTIVE = {1, 6, 7, 8, 15, 16, 34}
POST_TRANSITION = {13, 31, 49, 50, 81, 82, 83, 84, 113, 114, 115, 116}
GASES = {1, 2, 7, 8, 9, 10, 17, 18, 36, 54, 86}
LIQUIDS = {35, 80}

L_LETTERS = "SPDFGHIKLMNOQRTUV"

HALF_LIFE_UNIT = {
    "ysec": 1e-24, "zsec": 1e-21, "asec": 1e-18, "psec": 1e-12, "nsec": 1e-9, "usec": 1e-6, "msec": 1e-3, "sec": 1.0,
    "minute": 60.0, "hour": 3600.0, "day": 86400.0,
}
YEAR = 365.2422 * 86400.0
for prefix, scale in (("", 1.0), ("k", 1e3), ("M", 1e6), ("G", 1e9), ("T", 1e12), ("P", 1e15), ("E", 1e18), ("Z", 1e21), ("Y", 1e24)):
    HALF_LIFE_UNIT[prefix + "year"] = YEAR * scale

DECAY_MODES = {
    "B-": "BetaMinus", "B+": "BetaPlus", "EC": "ElectronCapture", "e+": "Positron", "EC+B+": "BetaPlus", "A": "Alpha",
    "p": "Proton", "2p": "TwoProtons", "3p": "ThreeProtons", "n": "Neutron", "2n": "TwoNeutrons", "3n": "ThreeNeutrons",
    "4n": "FourNeutrons", "SF": "SpontaneousFission", "IT": "IsomericTransition", "2B-": "DoubleBetaMinus",
    "2B+": "DoubleBetaPlus", "B-n": "BetaMinusNeutron", "B-2n": "BetaMinusTwoNeutrons", "B-3n": "BetaMinusThreeNeutrons",
    "B-4n": "BetaMinusFourNeutrons", "B-p": "BetaMinusProton", "B-d": "BetaMinusDeuteron", "B-t": "BetaMinusTriton",
    "B-A": "BetaMinusAlpha", "B-SF": "BetaMinusFission", "B+p": "BetaPlusProton", "B+2p": "BetaPlusTwoProtons",
    "B+3p": "BetaPlusThreeProtons", "B+A": "BetaPlusAlpha", "B+pA": "BetaPlusProtonAlpha", "B+SF": "BetaPlusFission",
}
CLUSTER_Z = {"C": 6, "O": 8, "F": 9, "Ne": 10, "Mg": 12, "Si": 14}
RELATIONS = {"=": "Exact", "~": "Approximate", "<": "LessThan", ">": "GreaterThan"}
ICRP_MODE = {"α": "A", "β-": "B-", "β+ & EC": "B+", "EC": "B+", "β+": "B+", "SF": "SF", "IT": "IT"}


def num(v):
    if v is None or (isinstance(v, float) and math.isnan(v)):
        return "NO_VALUE"
    if isinstance(v, float) and math.isinf(v):
        return "INFINITE_TIME" if v > 0 else "-INFINITE_TIME"
    r = format(float(v), ".15g")
    if "e" not in r and "." not in r and "inf" not in r:
        r += ".0"
    return r


def cstr(s):
    return '"' + (s or "").replace("\\", "\\\\").replace('"', '\\"') + '"'


def parse_uncertain(text):
    text = text.replace(" ", "")
    m = re.fullmatch(r"(-?)(\d+)(?:\.(\d+))?(?:\((\d+)\))?", text)
    if not m:
        raise ValueError(text)
    sign, whole, frac, unc = m.groups()
    frac = frac or ""
    value = float(sign + whole + ("." + frac if frac else ""))
    uncertainty = float(unc) * 10.0 ** (-len(frac)) if unc else math.nan
    return value, uncertainty


def parse_spin(spin):
    if not spin:
        return -1
    s = spin.strip().strip("()[]").split(",")[0].split(" ")[0].strip("()[]~#*")
    if not s:
        return -1
    m = re.fullmatch(r"(\d+)/2", s)
    if m:
        return int(m.group(1))
    m = re.fullmatch(r"(\d+)", s)
    if m:
        return 2 * int(m.group(1))
    return -1


def parse_parity(p):
    if not p:
        return 0
    p = p.strip("()")
    return 1 if p.startswith("+") else (-1 if p.startswith("-") else 0)


def parse_term(level):
    if not level:
        return (0, 0, False, -1)
    m = re.fullmatch(r"(\d+)([A-Z])(\*?)(?:<(\d+)(?:/(\d))?>|(\d+))", level.strip())
    if not m:
        return (0, 0, False, -1)
    mult, L, star, jn, jd, jplain = m.groups()
    if jplain is not None:
        twice_j = 2 * int(jplain)
    elif jd:
        twice_j = int(jn)
    else:
        twice_j = 2 * int(jn)
    return (int(mult), L_LETTERS.index(L), star == "*", twice_j)


CORES = {
    "[He]": "1s2", "[Ne]": "1s2 2s2 2p6", "[Ar]": "1s2 2s2 2p6 3s2 3p6", "[Kr]": "1s2 2s2 2p6 3s2 3p6 3d10 4s2 4p6",
    "[Xe]": "1s2 2s2 2p6 3s2 3p6 3d10 4s2 4p6 4d10 5s2 5p6",
    "[Rn]": "1s2 2s2 2p6 3s2 3p6 3d10 4s2 4p6 4d10 5s2 5p6 4f14 5d10 6s2 6p6",
}
L_OF = {"s": 0, "p": 1, "d": 2, "f": 3, "g": 4}


def config_parts(cfg):
    core = None
    parts = []
    for t in cfg.split():
        if t in CORES:
            core = t
            continue
        m = re.fullmatch(r"(\d)([spdfg])(\d*)", t)
        parts.append((int(m.group(1)), m.group(2), int(m.group(3) or 1)))
    return core, parts


def full_subshells(cfg):
    core, parts = config_parts(cfg)
    occ = {}
    if core:
        for t in CORES[core].split():
            m = re.fullmatch(r"(\d)([spdfg])(\d+)", t)
            occ[(int(m.group(1)), L_OF[m.group(2)])] = int(m.group(3))
    for n, l, e in parts:
        occ[(n, L_OF[l])] = e
    return sorted(occ.items(), key=lambda kv: (kv[0][0], kv[0][1]))


def normalized_config(cfg):
    core, parts = config_parts(cfg)
    text = " ".join(f"{n}{l}{e}" for n, l, e in parts)
    return (core + " " + text).strip() if core else text


def main():
    con = sqlite3.connect(get_package_dbpath())
    q = lambda s, *a: con.execute(s, a).fetchall()

    cols = [r[1] for r in q("pragma table_info(elements)")]
    elements = {r[cols.index("atomic_number")]: dict(zip(cols, r)) for r in q("select * from elements")}

    ciaaw = {}
    for line in CIAAW_2024.strip().splitlines():
        z, sym, w = line.split()
        z = int(z)
        if w.startswith("["):
            lo, hi = (float(x) for x in w.strip("[]").split(","))
            ciaaw[z] = ("interval", CONVENTIONAL[z], round((hi - lo) / 2.0, 10), lo, hi)
        else:
            v, u = parse_uncertain(w)
            ciaaw[z] = ("value", v, u, round(v - u, 12), round(v + u, 12))

    iso_cols = [r[1] for r in q("pragma table_info(isotopes)")]
    isotopes = [dict(zip(iso_cols, r)) for r in q("select * from isotopes order by atomic_number, mass_number")]
    decay_rows = defaultdict(list)
    for iid, mode, rel, inten in q("select isotope_id, mode, relation, intensity from isotopedecaymodes order by id"):
        decay_rows[iid].append((mode, rel, inten))

    for iso in isotopes:
        hl = iso["half_life"]
        iso["stable"] = not iso["is_radioactive"] and iso["abundance"] is not None
        if iso["stable"]:
            iso["hl_s"] = math.inf
            iso["hl_unc_s"] = math.nan
        elif hl is None:
            iso["hl_s"] = math.nan
            iso["hl_unc_s"] = math.nan
        else:
            scale = HALF_LIFE_UNIT[iso["half_life_unit"]]
            iso["hl_s"] = hl * scale
            iso["hl_unc_s"] = iso["half_life_uncertainty"] * scale if iso["half_life_uncertainty"] is not None else math.nan

    by_z = defaultdict(list)
    for iso in isotopes:
        by_z[iso["atomic_number"]].append(iso)

    reference = {}
    most_abundant = {}
    for z, lst in by_z.items():
        stable = [i for i in lst if i["stable"]]
        natural = [i for i in lst if i["abundance"]]
        most_abundant[z] = max(natural, key=lambda i: i["abundance"])["mass_number"] if natural else 0
        if natural:
            reference[z] = max(natural, key=lambda i: i["abundance"])
        elif stable:
            reference[z] = stable[0]
        else:
            known = [i for i in lst if not math.isnan(i["hl_s"])]
            reliable = [i for i in known if math.isnan(i["hl_unc_s"]) or i["hl_unc_s"] <= 0.7 * i["hl_s"]]
            reference[z] = max(reliable or known, key=lambda i: (i["hl_s"], -i["mass_number"]))
            override = [i for i in lst if i["mass_number"] == REFERENCE_OVERRIDE.get(z)]
            if override:
                reference[z] = override[0]

    ie_rows = defaultdict(dict)
    for z, charge, e, u in q("select atomic_number, ion_charge, ionization_energy, uncertainty from ionizationenergies"):
        ie_rows[z][charge] = (e, u)
    ground = {z: lvl for z, lvl in q("select atomic_number, ground_level from ionizationenergies where ion_charge = 0")}

    ox_rows = defaultdict(list)
    for z, s, cat in q("select atomic_number, oxidation_state, category from oxidationstates order by atomic_number, oxidation_state"):
        ox_rows[z].append((s, cat == "main"))

    phase = {}
    for z, bp, mp, tc, pc, tt, pt, allo, subl, trans in q(
            "select atomic_number, boiling_point, melting_point, critical_temperature, critical_pressure, triple_point_temperature, triple_point_pressure, allotrope, is_sublimation_point, is_transition from phasetransitions"):
        if z in phase and not trans:
            continue
        phase[z] = dict(bp=bp, mp=mp, tc=tc, pc=pc, tt=tt, pt=pt, allotrope=allo, sublimes=bool(subl))

    subshell_lines = []
    ie_values = []
    ie_unc = []
    ox_lines = []
    element_lines = []
    report = []

    for z in range(1, 119):
        e = elements[z]
        sym = e["symbol"]
        group = e["group_id"] or 0
        block = e["block"]
        if 57 <= z <= 70 or 89 <= z <= 102:
            group, block = 0, "f"
        if z in (71, 103):
            group, block = 3, "d"
        period = e["period"]

        if z in ALKALI:
            category = "AlkaliMetal"
        elif z in ALKALINE:
            category = "AlkalineEarthMetal"
        elif 57 <= z <= 71:
            category = "Lanthanide"
        elif 89 <= z <= 103:
            category = "Actinide"
        elif z in METALLOID:
            category = "Metalloid"
        elif z in HALOGEN:
            category = "Halogen"
        elif z in NOBLE:
            category = "NobleGas"
        elif z in REACTIVE:
            category = "ReactiveNonmetal"
        elif z in POST_TRANSITION:
            category = "PostTransitionMetal"
        else:
            category = "TransitionMetal"

        if category in ("Metalloid",) or z in (85, 117):
            classification = "Metalloid"
        elif category in ("Halogen", "NobleGas", "ReactiveNonmetal"):
            classification = "Nonmetal"
        else:
            classification = "Metal"

        pt = phase.get(z, {})
        if z >= 104:
            state = "Unknown"
        elif z in GASES:
            state = "Gas"
        elif z in LIQUIDS:
            state = "Liquid"
        else:
            state = "Solid"

        lst = by_z[z]
        is_stable = any(i["stable"] for i in lst)
        ref = reference[z]
        if z in ciaaw:
            kind, w, wu, wlo, whi = ciaaw[z]
            has_std = True
        else:
            w, wu = ref["mass"], ref["mass_uncertainty"] or math.nan
            wlo, whi = w - (wu if not math.isnan(wu) else 0.0), w + (wu if not math.isnan(wu) else 0.0)
            has_std = False
        if has_std and e["atomic_weight"] and abs(e["atomic_weight"] - w) > 2e-3:
            report.append(f"weight differs from mendeleev: {sym} {w} vs {e['atomic_weight']}")

        cfg = e["electronic_configuration"]
        subs = full_subshells(cfg)
        assert sum(c for _, c in subs) == z, sym
        sub_off = len(subshell_lines)
        for (n, l), c in subs:
            subshell_lines.append(f"    Subshell{{ {n}, {l}, {c} }},")

        term = TERM_OVERRIDE.get(z) or parse_term(ground.get(z))

        ea_text, ea_kind = ELECTRON_AFFINITY.get(z, (None, None))
        if ea_text is None:
            ea, eau, eaq = math.nan, math.nan, "Unknown"
        else:
            ea, eau = parse_uncertain(ea_text)
            eaq = {"m": "Measured", "e": "Estimated", "c": "Calculated"}[ea_kind]
            old = e["electron_affinity"]
            if old is not None and ea > 0 and abs(old - ea) > 0.02:
                report.append(f"EA changed: {sym} mendeleev {old} -> {ea}")

        pauling = e["en_pauling"] if e["en_pauling"] is not None else EXTRA_PAULING.get(z, math.nan)

        ies = ie_rows.get(z, {})
        ie_off = len(ie_values)
        ie_count = (max(ies) + 1) if ies else 0
        for k in range(ie_count):
            v = ies.get(k)
            ie_values.append(num(v[0]) if v else "NO_VALUE")
            ie_unc.append(num(v[1]) if v and v[1] is not None else "NO_VALUE")

        ox_off = len(ox_lines)
        for s, main_state in ox_rows.get(z, []):
            ox_lines.append(f"    OxidationStateEntry{{ {s}, {'true' if main_state else 'false'} }},")

        iso_off = sum(len(by_z[k]) for k in range(1, z))
        cpk = int(e["cpk_color"].lstrip("#"), 16) if e["cpk_color"] else 0
        year = e["discovery_year"] or 0

        element_lines.append("    Element{ " + ", ".join([
            str(z), cstr(sym), cstr(e["name"]), str(period), str(group), "Block::" + block.upper(),
            "Category::" + category, "Classification::" + classification, "Phase::" + state,
            "true" if is_stable else "false", "true" if has_std else "false", "true" if z >= 104 else "false",
            num(w), num(wu), num(wlo), num(whi), str(ref["mass_number"]), str(most_abundant[z]),
            cstr(normalized_config(cfg)), str(sub_off), str(len(subs)),
            f"TermSymbol{{ {term[0]}, {term[1]}, {'true' if term[2] else 'false'}, {term[3]} }}",
            num(pauling), num(e["en_allen"] * ALLEN_EV_TO_PAULING if e["en_allen"] is not None else None), num(ea), num(eau), "DataQuality::" + eaq,
            str(ie_off), str(ie_count), str(ox_off), str(len(ox_rows.get(z, []))), str(iso_off), str(len(lst)),
            num(e["covalent_radius_pyykko"]), num(e["covalent_radius_pyykko_double"]), num(e["covalent_radius_pyykko_triple"]),
            num(e["covalent_radius_cordero"]), num(e["vdw_radius_alvarez"] or e["vdw_radius_bondi"] or e["vdw_radius"]),
            num(e["metallic_radius"]), num(e["atomic_radius"]), num(e["dipole_polarizability"]),
            num(pt.get("mp")), num(pt.get("bp")), "true" if pt.get("sublimes") else "false",
            num(pt.get("tt")), num(pt.get("pt") * 1e3 if pt.get("pt") is not None else None), num(pt.get("tc")),
            num(pt.get("pc") * 1e6 if pt.get("pc") is not None else None),
            num(e["density"]), num(e["fusion_heat"]), num(e["evaporation_heat"]), num(e["molar_heat_capacity"]),
            num(e["specific_heat_capacity"]), num(e["thermal_conductivity"]), cstr(e["lattice_structure"]),
            num(e["lattice_constant"] * 100.0 if e["lattice_constant"] is not None else None), num(e["abundance_crust"]), num(e["abundance_sea"]), f"0x{cpk:06X}u", str(year),
            num(e["heat_of_formation"]),
        ]) + " },")

    icrp = radioactivedecay.DEFAULTDATA
    clusters = {}
    for mode in {m for v in decay_rows.values() for m, _, _ in v}:
        m = re.fullmatch(r"(\d+)([A-Z][a-z]?)", mode)
        if m:
            clusters[mode] = (CLUSTER_Z[m.group(2)], int(m.group(1)))

    branch_lines = []
    isotope_lines = []
    fixed = []
    for iso in isotopes:
        z = iso["atomic_number"]
        a = iso["mass_number"]
        rows = decay_rows.get(iso["id"], [])
        has_bplus = any(m == "B+" for m, _, _ in rows)
        delayed = None
        decoded = []
        for m, rel, inten in rows:
            sub = False
            mode = m
            if m.startswith("B-") and m not in ("B-",):
                delayed, sub = "B-", True
            elif m.startswith("B+") and m not in ("B+",):
                delayed, sub = "B+", True
            elif m in ("n", "2n", "3n", "4n") and delayed == "B-":
                mode, sub = "B-" + m, True
            elif m in ("p", "2p", "3p") and delayed == "B+":
                mode, sub = "B+" + m, True
            elif m in ("e+", "EC") and has_bplus:
                sub = True
            decoded.append([mode, rel, inten, sub])
        exclusive = [d for d in decoded if not d[3] and d[1] == "=" and d[2] is not None]
        if sum(d[2] for d in exclusive) > 100.5:
            top = max(exclusive, key=lambda d: d[2])
            icrp_fractions = {}
            try:
                nuc = radioactivedecay.Nuclide(f"{elements[z]['symbol']}-{a}")
                for mm, bf in zip(nuc.decay_modes(), nuc.branching_fractions()):
                    key = ICRP_MODE.get(mm)
                    if key:
                        icrp_fractions[key] = icrp_fractions.get(key, 0.0) + bf * 100.0
            except Exception:
                pass
            for d in decoded:
                if d is top or d[3]:
                    continue
                if d[0] in icrp_fractions:
                    d[2] = icrp_fractions[d[0]]
                    d[1] = "="
                else:
                    d[2] = None
                    d[1] = "?"
            fixed.append(f"{elements[z]['symbol']}-{a}")
        boff = len(branch_lines)
        for mode, rel, inten, sub in decoded:
            if mode in clusters:
                cz, ca = clusters[mode]
                enum = "ClusterEmission"
            else:
                cz, ca = 0, 0
                enum = DECAY_MODES[mode]
            relation = RELATIONS.get(rel, "Unknown")
            if inten is None:
                relation = "Unknown"
            branch_lines.append(
                f"    DecayBranch{{ DecayMode::{enum}, {num(inten)}, Relation::{relation}, {'true' if sub else 'false'}, {cz}, {ca} }},")
        isotope_lines.append("    Isotope{ " + ", ".join([
            str(z), str(a), num(iso["mass"]), num(iso["mass_uncertainty"]),
            num(iso["abundance"] / 100.0 if iso["abundance"] is not None else None),
            num(iso["abundance_uncertainty"] / 100.0 if iso["abundance_uncertainty"] is not None else None),
            "true" if iso["stable"] else "false", num(iso["hl_s"]), num(iso["hl_unc_s"]),
            str(parse_spin(iso["spin"])), str(parse_parity(iso["parity"])), num(iso["g_factor"]),
            num(iso["quadrupole_moment"]),
            str(boff), str(len(decoded)), str(iso["discovery_year"] or 0),
        ]) + " },")

    radii = []
    for z, charge, econf, coord, spin, crystal, ionic, origin, best in q(
            "select atomic_number, charge, econf, coordination, spin, crystal_radius, ionic_radius, origin, most_reliable from ionicradii order by atomic_number, charge, coordination"):
        spin_enum = {"HS": "High", "LS": "Low"}.get(spin or "", "Unspecified")
        radii.append(f"    IonicRadius{{ {z}, {charge}, {cstr(coord)}, SpinState::{spin_enum}, {num(crystal)}, {num(ionic)}, {'true' if best else 'false'} }},")

    OUT.mkdir(parents=True, exist_ok=True)
    head = "#ifndef {g}\n#define {g}\n\n#include \"../{inc}\"\n\nnamespace fizmo {{\nnamespace chemistry {{\nnamespace data {{\n\n"
    tail = "\n}} // namespace data\n}} // namespace chemistry\n}} // namespace fizmo\n\n#endif // {g}\n"

    g = "FIZMO_CHEMISTRY_DATA_ELEMENT_DATA_HPP"
    with open(OUT / "element_data.hpp", "w") as f:
        f.write(head.format(g=g, inc="element_types.hpp"))
        f.write("inline constexpr Subshell SUBSHELLS[] = {\n" + "\n".join(subshell_lines) + "\n};\n\n")
        f.write("inline constexpr double IONIZATION_ENERGIES[] = {\n" + "\n".join(f"    {v}," for v in ie_values) + "\n};\n\n")
        f.write("inline constexpr double IONIZATION_ENERGY_UNCERTAINTIES[] = {\n" + "\n".join(f"    {v}," for v in ie_unc) + "\n};\n\n")
        f.write("inline constexpr OxidationStateEntry OXIDATION_STATES[] = {\n" + "\n".join(ox_lines) + "\n};\n\n")
        f.write("inline constexpr Element ELEMENTS[] = {\n" + "\n".join(element_lines) + "\n};\n")
        f.write(tail.format(g=g))

    g = "FIZMO_CHEMISTRY_DATA_ISOTOPE_DATA_HPP"
    with open(OUT / "isotope_data.hpp", "w") as f:
        f.write(head.format(g=g, inc="isotope_types.hpp"))
        f.write("inline constexpr DecayBranch DECAY_BRANCHES[] = {\n" + "\n".join(branch_lines) + "\n};\n\n")
        f.write("inline constexpr Isotope ISOTOPES[] = {\n" + "\n".join(isotope_lines) + "\n};\n")
        f.write(tail.format(g=g))

    g = "FIZMO_CHEMISTRY_DATA_IONIC_RADIUS_DATA_HPP"
    with open(OUT / "ionic_radius_data.hpp", "w") as f:
        f.write(head.format(g=g, inc="element_types.hpp"))
        f.write("inline constexpr IonicRadius IONIC_RADII[] = {\n" + "\n".join(radii) + "\n};\n")
        f.write(tail.format(g=g))

    print("\n".join(report))
    print("decay intensities repaired:", len(fixed), " ".join(fixed))
    print("reference isotopes for elements without a standard atomic weight:")
    print(" ".join(f"{elements[z]['symbol']}-{reference[z]['mass_number']}" for z in range(1, 119) if z not in ciaaw))
    print(len(element_lines), "elements,", len(isotope_lines), "isotopes,", len(branch_lines), "decay branches,", len(ie_values), "ionization energies,", len(radii), "ionic radii")


if __name__ == "__main__":
    main()
