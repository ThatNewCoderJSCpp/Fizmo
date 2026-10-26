import math
import sys
from pathlib import Path

import chemicals
from chemicals import heat_capacity as hc
from chemicals import vapor_pressure as vp
from chemicals import phase_change as pc
from chemicals import lennard_jones as lj
lj._load_LJ_data()

OUT = Path(sys.argv[1]) if len(sys.argv) > 1 else Path(__file__).resolve().parents[2] / "Source" / "Chemistry" / "Data"
R = 8.31446261815324
T0 = 298.15

SUBSTANCES = [
    ("hydrogen", "H2", "1333-74-0", "g"), ("helium", "He", "7440-59-7", "g"), ("neon", "Ne", "7440-01-9", "g"),
    ("argon", "Ar", "7440-37-1", "g"), ("krypton", "Kr", "7439-90-9", "g"), ("xenon", "Xe", "7440-63-3", "g"),
    ("radon", "Rn", "10043-92-2", "g"), ("nitrogen", "N2", "7727-37-9", "g"), ("oxygen", "O2", "7782-44-7", "g"),
    ("ozone", "O3", "10028-15-6", "g"), ("fluorine", "F2", "7782-41-4", "g"), ("chlorine", "Cl2", "7782-50-5", "g"),
    ("bromine", "Br2", "7726-95-6", "l"), ("iodine", "I2", "7553-56-2", "s"), ("water", "H2O", "7732-18-5", "l"),
    ("hydrogen peroxide", "H2O2", "7722-84-1", "l"), ("carbon monoxide", "CO", "630-08-0", "g"),
    ("carbon dioxide", "CO2", "124-38-9", "g"), ("methane", "CH4", "74-82-8", "g"), ("ethane", "C2H6", "74-84-0", "g"),
    ("propane", "C3H8", "74-98-6", "g"), ("n-butane", "C4H10", "106-97-8", "g"), ("isobutane", "C4H10", "75-28-5", "g"),
    ("n-pentane", "C5H12", "109-66-0", "l"), ("n-hexane", "C6H14", "110-54-3", "l"), ("n-heptane", "C7H16", "142-82-5", "l"),
    ("n-octane", "C8H18", "111-65-9", "l"), ("isooctane", "C8H18", "540-84-1", "l"), ("n-decane", "C10H22", "124-18-5", "l"),
    ("ethylene", "C2H4", "74-85-1", "g"), ("propylene", "C3H6", "115-07-1", "g"), ("acetylene", "C2H2", "74-86-2", "g"),
    ("1,3-butadiene", "C4H6", "106-99-0", "g"), ("cyclohexane", "C6H12", "110-82-7", "l"), ("benzene", "C6H6", "71-43-2", "l"),
    ("toluene", "C7H8", "108-88-3", "l"), ("o-xylene", "C8H10", "95-47-6", "l"), ("p-xylene", "C8H10", "106-42-3", "l"),
    ("styrene", "C8H8", "100-42-5", "l"), ("naphthalene", "C10H8", "91-20-3", "s"), ("methanol", "CH3OH", "67-56-1", "l"),
    ("ethanol", "C2H5OH", "64-17-5", "l"), ("1-propanol", "C3H7OH", "71-23-8", "l"), ("isopropanol", "C3H7OH", "67-63-0", "l"),
    ("1-butanol", "C4H9OH", "71-36-3", "l"), ("ethylene glycol", "C2H6O2", "107-21-1", "l"), ("glycerol", "C3H8O3", "56-81-5", "l"),
    ("acetone", "C3H6O", "67-64-1", "l"), ("formaldehyde", "CH2O", "50-00-0", "g"), ("acetaldehyde", "C2H4O", "75-07-0", "l"),
    ("formic acid", "HCOOH", "64-18-6", "l"), ("acetic acid", "CH3COOH", "64-19-7", "l"), ("diethyl ether", "C4H10O", "60-29-7", "l"),
    ("dimethyl ether", "C2H6O", "115-10-6", "g"), ("ethyl acetate", "C4H8O2", "141-78-6", "l"), ("phenol", "C6H5OH", "108-95-2", "s"),
    ("aniline", "C6H5NH2", "62-53-3", "l"), ("pyridine", "C5H5N", "110-86-1", "l"), ("dimethyl sulfoxide", "C2H6OS", "67-68-5", "l"),
    ("acetonitrile", "CH3CN", "75-05-8", "l"), ("tetrahydrofuran", "C4H8O", "109-99-9", "l"), ("ammonia", "NH3", "7664-41-7", "g"),
    ("hydrazine", "N2H4", "302-01-2", "l"), ("nitric oxide", "NO", "10102-43-9", "g"), ("nitrogen dioxide", "NO2", "10102-44-0", "g"),
    ("dinitrogen tetroxide", "N2O4", "10544-72-6", "g"), ("nitrous oxide", "N2O", "10024-97-2", "g"), ("nitric acid", "HNO3", "7697-37-2", "l"),
    ("hydrogen cyanide", "HCN", "74-90-8", "l"), ("hydrogen sulfide", "H2S", "7783-06-4", "g"), ("sulfur dioxide", "SO2", "7446-09-5", "g"),
    ("sulfur trioxide", "SO3", "7446-11-9", "g"), ("sulfuric acid", "H2SO4", "7664-93-9", "l"), ("carbon disulfide", "CS2", "75-15-0", "l"),
    ("carbonyl sulfide", "COS", "463-58-1", "g"), ("hydrogen fluoride", "HF", "7664-39-3", "g"), ("hydrogen chloride", "HCl", "7647-01-0", "g"),
    ("hydrogen bromide", "HBr", "10035-10-6", "g"), ("hydrogen iodide", "HI", "10034-85-2", "g"), ("sulfur hexafluoride", "SF6", "2551-62-4", "g"),
    ("carbon tetrafluoride", "CF4", "75-73-0", "g"), ("carbon tetrachloride", "CCl4", "56-23-5", "l"), ("chloroform", "CHCl3", "67-66-3", "l"),
    ("dichloromethane", "CH2Cl2", "75-09-2", "l"), ("chloromethane", "CH3Cl", "74-87-3", "g"), ("1,1,1,2-tetrafluoroethane", "C2H2F4", "811-97-2", "g"),
    ("dichlorodifluoromethane", "CCl2F2", "75-71-8", "g"), ("chlorodifluoromethane", "CHClF2", "75-45-6", "g"), ("silane", "SiH4", "7803-62-5", "g"),
    ("phosphine", "PH3", "7803-51-2", "g"), ("urea", "CO(NH2)2", "57-13-6", "s"), ("glucose", "C6H12O6", "50-99-7", "s"),
    ("sucrose", "C12H22O11", "57-50-1", "s"), ("sodium chloride", "NaCl", "7647-14-5", "s"), ("potassium chloride", "KCl", "7447-40-7", "s"),
    ("calcium carbonate", "CaCO3", "471-34-1", "s"), ("calcium oxide", "CaO", "1305-78-8", "s"), ("calcium hydroxide", "Ca(OH)2", "1305-62-0", "s"),
    ("magnesium oxide", "MgO", "1309-48-4", "s"), ("aluminium oxide", "Al2O3", "1344-28-1", "s"), ("silicon dioxide", "SiO2", "14808-60-7", "s"),
    ("iron(III) oxide", "Fe2O3", "1309-37-1", "s"), ("iron(II,III) oxide", "Fe3O4", "1317-61-9", "s"), ("iron(II) oxide", "FeO", "1345-25-1", "s"),
    ("copper(II) oxide", "CuO", "1317-38-0", "s"), ("zinc oxide", "ZnO", "1314-13-2", "s"), ("sodium hydroxide", "NaOH", "1310-73-2", "s"),
    ("potassium hydroxide", "KOH", "1310-58-3", "s"), ("ammonium chloride", "NH4Cl", "12125-02-9", "s"), ("ammonium nitrate", "NH4NO3", "6484-52-2", "s"),
    ("sodium bicarbonate", "NaHCO3", "144-55-8", "s"), ("sodium carbonate", "Na2CO3", "497-19-8", "s"), ("titanium dioxide", "TiO2", "13463-67-7", "s"),
    ("lithium fluoride", "LiF", "7789-24-4", "s"), ("graphite", "C", "7440-44-0", "s"), ("diamond", "C", "7782-40-3", "s"),
    ("sulfur", "S", "7704-34-9", "s"), ("white phosphorus", "P", "12185-10-3", "s"), ("silicon", "Si", "7440-21-3", "s"),
    ("sodium", "Na", "7440-23-5", "s"), ("potassium", "K", "7440-09-7", "s"), ("lithium", "Li", "7439-93-2", "s"),
    ("calcium", "Ca", "7440-70-2", "s"), ("magnesium", "Mg", "7439-95-4", "s"), ("aluminium", "Al", "7429-90-5", "s"),
    ("iron", "Fe", "7439-89-6", "s"), ("copper", "Cu", "7440-50-8", "s"), ("zinc", "Zn", "7440-66-6", "s"),
    ("silver", "Ag", "7440-22-4", "s"), ("gold", "Au", "7440-57-5", "s"), ("mercury", "Hg", "7439-97-6", "l"),
    ("lead", "Pb", "7439-92-1", "s"), ("titanium", "Ti", "7440-32-6", "s"), ("nickel", "Ni", "7440-02-0", "s"),
    ("tin", "Sn", "7440-31-5", "s"), ("uranium", "U", "7440-61-1", "s"),
]

CODATA_CHECK = {
    "7732-18-5": (-285830.0, -237140.0, "l"), "124-38-9": (-393510.0, -394370.0, "g"), "74-82-8": (-74870.0, -50800.0, "g"),
    "7664-41-7": (-45940.0, -16400.0, "g"), "630-08-0": (-110530.0, -137170.0, "g"), "10102-43-9": (91290.0, 87600.0, "g"),
    "7647-14-5": (-411120.0, -384140.0, "s"), "471-34-1": (-1207600.0, -1129100.0, "s"), "7446-09-5": (-296810.0, -300100.0, "g"),
    "64-17-5": (-277600.0, -174800.0, "l"), "71-43-2": (49100.0, 124500.0, "l"),
}

REFERENCE_ELEMENTS = {
    "H": ("1333-74-0", "g", 0.5), "He": ("7440-59-7", "g", 1), "Ne": ("7440-01-9", "g", 1), "Ar": ("7440-37-1", "g", 1),
    "Kr": ("7439-90-9", "g", 1), "Xe": ("7440-63-3", "g", 1), "Rn": ("10043-92-2", "g", 1), "N": ("7727-37-9", "g", 0.5),
    "O": ("7782-44-7", "g", 0.5), "F": ("7782-41-4", "g", 0.5), "Cl": ("7782-50-5", "g", 0.5), "Br": ("7726-95-6", "l", 0.5),
    "I": ("7553-56-2", "s", 0.5), "C": ("7440-44-0", "s", 1), "S": ("7704-34-9", "s", 1), "P": ("12185-10-3", "s", 1),
    "Si": ("7440-21-3", "s", 1), "Na": ("7440-23-5", "s", 1), "K": ("7440-09-7", "s", 1), "Li": ("7439-93-2", "s", 1),
    "Ca": ("7440-70-2", "s", 1), "Mg": ("7439-95-4", "s", 1), "Al": ("7429-90-5", "s", 1), "Fe": ("7439-89-6", "s", 1),
    "Cu": ("7440-50-8", "s", 1), "Zn": ("7440-66-6", "s", 1), "Ag": ("7440-22-4", "s", 1), "Au": ("7440-57-5", "s", 1),
    "Hg": ("7439-97-6", "l", 1), "Pb": ("7439-92-1", "s", 1), "Ti": ("7440-32-6", "s", 1), "Ni": ("7440-02-0", "s", 1),
    "Sn": ("7440-31-5", "s", 1), "U": ("7440-61-1", "s", 1),
}


def safe(f, *a):
    try:
        v = f(*a)
        if v is None:
            return math.nan
        return float(v)
    except Exception:
        return math.nan


def num(v):
    if v is None or (isinstance(v, float) and math.isnan(v)):
        return "NO_VALUE"
    r = format(float(v), ".12g")
    if "e" not in r and "." not in r:
        r += ".0"
    return r


def cstr(s):
    return '"' + s.replace('"', '\\"') + '"'


def entropy(cas, phase):
    if "S0" + phase in MANUAL.get(cas, {}):
        return MANUAL[cas]["S0" + phase]
    return {"g": safe(chemicals.S0g, cas), "l": safe(chemicals.S0l, cas), "s": safe(chemicals.S0s, cas)}[phase]


def enthalpy(cas, phase):
    if "Hf" + phase in MANUAL.get(cas, {}):
        return MANUAL[cas]["Hf" + phase]
    return {"g": safe(chemicals.Hfg, cas), "l": safe(chemicals.Hfl, cas), "s": safe(chemicals.Hfs, cas)}[phase]


def composition(formula_text):
    import re
    from chemicals.elements import nested_formula_parser
    return nested_formula_parser(formula_text)


def gibbs(cas, formula_text, phase):
    h = MANUAL.get(cas, {}).get("Hf" + phase, enthalpy(cas, phase))
    s = MANUAL.get(cas, {}).get("S0" + phase, entropy(cas, phase))
    if math.isnan(h) or math.isnan(s):
        return math.nan
    total = 0.0
    for sym, n in composition(formula_text).items():
        ref = REFERENCE_ELEMENTS.get(sym)
        if not ref:
            return math.nan
        s_ref = entropy(ref[0], ref[1])
        if math.isnan(s_ref):
            return math.nan
        total += n * s_ref * (ref[2] if ref[2] != 1 else 1.0)
    return h - T0 * (s - total)


def ranges_for(cas, phase):
    out = []
    table = {"g": hc.WebBook_Shomate_gases, "l": hc.WebBook_Shomate_liquids, "s": hc.WebBook_Shomate_solids}[phase]
    entry = table.get(cas)
    models = []
    if entry is not None:
        models = entry.models if hasattr(entry, "models") else [entry]
    for m in models:
        a, b, c, d, e = m.coeffs
        out.append((m.Tmin, m.Tmax, a, b, c, d, 0.0, e))
    out.sort()
    if phase == "g" and cas in hc.Cp_data_Poling.index:
        row = hc.Cp_data_Poling.loc[cas]
        coeffs = [row[k] for k in ("a0", "a1", "a2", "a3", "a4")]
        if not any(isinstance(x, float) and math.isnan(x) for x in coeffs):
            tmin, tmax = float(row["Tmin"]), float(row["Tmax"])
            poly = [R * float(x) for x in coeffs]
            if not out:
                out.append((tmin, tmax, poly[0], poly[1], poly[2], poly[3], poly[4], 0.0))
            elif out[0][0] > T0 + 1 and tmin <= T0:
                out.insert(0, (tmin, out[0][0], poly[0], poly[1], poly[2], poly[3], poly[4], 0.0))
    if phase == "l" and not out and cas in hc.Cp_data_Poling.index:
        cpl = hc.Cp_data_Poling.loc[cas]["Cpl"]
        if not (isinstance(cpl, float) and math.isnan(cpl)):
            out.append((T0 - 50.0, T0 + 50.0, float(cpl), 0.0, 0.0, 0.0, 0.0, 0.0))
    if not out:
        key = {"g": "Cpg", "l": "Cpl", "s": "Cps"}[phase]
        value = MANUAL.get(cas, {}).get(key, math.nan)
        if math.isnan(value) and cas in hc.CRC_standard_data.index:
            value = float(hc.CRC_standard_data.loc[cas][key])
        if not math.isnan(value):
            out.append((T0 - 100.0, T0 + 100.0, value, 0.0, 0.0, 0.0, 0.0, 0.0))
    return out


def element_weights():
    import re
    weights = {}
    text = (OUT / "element_data.hpp").read_text()
    for m in re.finditer(r'Element\{ (\d+), "(\w+)", "[^"]*", (?:[^,]*, ){9}([0-9.eE+-]+),', text):
        weights[m.group(2)] = float(m.group(3))
    return weights


MOLECULAR_SOLIDS = {"7553-56-2", "91-20-3", "108-95-2"}

MANUAL = {
    "7440-44-0": {"Hfs": 0.0, "S0s": 5.74, "Cps": 8.517},
    "7782-40-3": {"Hfs": 1895.0, "S0s": 2.377, "Cps": 6.113},
    "50-99-7": {"Hfs": -1273300.0, "S0s": 209.2, "Cps": 218.6},
    "57-50-1": {"Hfs": -2226100.0, "S0s": 360.2, "Cps": 424.3},
    "13463-67-7": {"Hfs": -944000.0, "S0s": 50.62, "Cps": 55.02},
    "12185-10-3": {"Hfs": 0.0, "S0s": 41.09, "Cps": 23.84},
}

REFERENCE_STATES = {ref[0]: ref[1] for ref in REFERENCE_ELEMENTS.values()}


def main():
    weights = element_weights()
    range_lines = []
    lines = []
    report = []
    for name, formula, cas, state in SUBSTANCES:
        mw = sum(weights[sym] * n for sym, n in composition(formula).items())
        tc = safe(chemicals.Tc, cas)
        pcrit = safe(chemicals.Pc, cas)
        vcrit = safe(chemicals.Vc, cas)
        omega = safe(chemicals.omega, cas)
        if state == "s" and cas not in MOLECULAR_SOLIDS:
            tc = pcrit = vcrit = omega = math.nan
        zc = pcrit * vcrit / (R * tc) if not any(math.isnan(x) for x in (tc, pcrit, vcrit)) else math.nan
        tb = safe(chemicals.Tb, cas)
        tm = safe(chemicals.Tm, cas)
        tt = safe(chemicals.Tt, cas)
        pt = safe(chemicals.Pt, cas)
        dipole = safe(chemicals.dipole_moment, cas)
        hvap_tb = math.nan
        hvap_298 = math.nan
        if cas in pc.Hvap_data_CRC.index:
            row = pc.Hvap_data_CRC.loc[cas]
            hvap_tb = float(row["HvapTb"]) if not math.isnan(row["HvapTb"]) else math.nan
            hvap_298 = float(row["Hvap298"]) if not math.isnan(row["Hvap298"]) else math.nan
        hfus = safe(chemicals.Hfus, cas)
        eps_k = math.nan
        sigma = math.nan
        if cas in lj.LJ_data_Poling.index:
            row = lj.LJ_data_Poling.loc[cas]
            eps_k = float(row["Stockmayer"])
            sigma = float(row["molecular_diameter"])
        antoine = (math.nan,) * 5
        if cas in vp.Psat_data_AntoinePoling.index:
            row = vp.Psat_data_AntoinePoling.loc[cas]
            antoine = (float(row["A"]), float(row["B"]), float(row["C"]), float(row["Tmin"]), float(row["Tmax"]))

        h = {p: enthalpy(cas, p) for p in "gls"}
        s = {p: entropy(cas, p) for p in "gls"}
        if cas in MANUAL:
            h["s"] = MANUAL[cas].get("Hfs", h["s"])
            s["s"] = MANUAL[cas].get("S0s", s["s"])
        if cas in REFERENCE_STATES:
            h[REFERENCE_STATES[cas]] = 0.0
        g = {p: gibbs(cas, formula, p) for p in "gls"}
        if cas in REFERENCE_STATES:
            g[REFERENCE_STATES[cas]] = 0.0
        cp_offsets = []
        for p in "gls":
            rs = ranges_for(cas, p)
            cp_offsets.append((len(range_lines), len(rs)))
            for r in rs:
                range_lines.append("    HeatCapacityRange{ " + ", ".join(num(x) for x in r) + " },")

        if cas in CODATA_CHECK:
            hh, gg, pp = CODATA_CHECK[cas]
            report.append(f"{name:22s} dHf {h[pp]:>12.0f} (CODATA {hh:>10.0f})   dGf {g[pp]:>12.0f} (ref {gg:>10.0f})")

        phase = {"g": "Gas", "l": "Liquid", "s": "Solid"}[state]
        lines.append("    Substance{ " + ", ".join([
            cstr(name), cstr(formula), cstr(cas), num(mw), "Phase::" + phase,
            num(tc), num(pcrit), num(vcrit), num(zc), num(omega), num(tb), num(tm), num(tt), num(pt), num(dipole),
            num(h["g"]), num(h["l"]), num(h["s"]), num(s["g"]), num(s["l"]), num(s["s"]), num(g["g"]), num(g["l"]), num(g["s"]),
            num(hvap_tb), num(hvap_298), num(hfus), num(sigma), num(eps_k),
            num(antoine[0]), num(antoine[1]), num(antoine[2]), num(antoine[3]), num(antoine[4]),
            str(cp_offsets[0][0]), str(cp_offsets[0][1]), str(cp_offsets[1][0]), str(cp_offsets[1][1]), str(cp_offsets[2][0]), str(cp_offsets[2][1]),
        ]) + " },")

    g = "FIZMO_CHEMISTRY_DATA_SUBSTANCE_DATA_HPP"
    with open(OUT / "substance_data.hpp", "w") as f:
        f.write(f"#ifndef {g}\n#define {g}\n\n#include \"../substance_types.hpp\"\n\nnamespace fizmo {{\nnamespace chemistry {{\nnamespace data {{\n\n")
        f.write("inline constexpr HeatCapacityRange HEAT_CAPACITY_RANGES[] = {\n" + "\n".join(range_lines) + "\n};\n\n")
        f.write("inline constexpr Substance SUBSTANCES[] = {\n" + "\n".join(lines) + "\n};\n")
        f.write(f"\n}} // namespace data\n}} // namespace chemistry\n}} // namespace fizmo\n\n#endif // {g}\n")
    print("\n".join(report))
    print(len(lines), "substances,", len(range_lines), "heat capacity ranges")


if __name__ == "__main__":
    main()
