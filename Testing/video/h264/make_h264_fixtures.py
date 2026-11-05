import sys


def split_nals(data):
    out = []
    i = 0
    n = len(data)
    starts = []
    while i + 3 <= n:
        if data[i] == 0 and data[i + 1] == 0 and data[i + 2] == 1:
            starts.append(i + 3)
            i += 3
        else:
            i += 1
    for k, s in enumerate(starts):
        e = starts[k + 1] - 3 if k + 1 < len(starts) else n
        while e > s and data[e - 1] == 0:
            e -= 1
        out.append(bytes(data[s:e]))
    return out


def unescape(b):
    out = bytearray()
    zeros = 0
    for x in b:
        if zeros >= 2 and x == 3:
            zeros = 0
            continue
        out.append(x)
        zeros = zeros + 1 if x == 0 else 0
    return bytes(out)


def escape(b):
    out = bytearray()
    zeros = 0
    for x in b:
        if zeros >= 2 and x <= 3:
            out.append(3)
            zeros = 0
        out.append(x)
        zeros = zeros + 1 if x == 0 else 0
    return bytes(out)


class R:
    def __init__(self, b):
        self.b = b
        self.p = 0

    def u(self, n):
        v = 0
        for _ in range(n):
            byte = self.b[self.p >> 3] if (self.p >> 3) < len(self.b) else 0
            v = (v << 1) | ((byte >> (7 - (self.p & 7))) & 1)
            self.p += 1
        return v

    def ue(self):
        z = 0
        while self.u(1) == 0:
            z += 1
            if z > 31:
                raise ValueError("bad ue")
        return (1 << z) - 1 + self.u(z)

    def se(self):
        k = self.ue()
        return (k + 1) >> 1 if k & 1 else -(k >> 1)

    def more(self):
        total = len(self.b) * 8
        last = total - 1
        while last >= 0 and not ((self.b[last >> 3] >> (7 - (last & 7))) & 1):
            last -= 1
        return self.p < last


class W:
    def __init__(self):
        self.bits = []

    def u(self, n, v):
        for i in range(n - 1, -1, -1):
            self.bits.append((v >> i) & 1)

    def ue(self, v):
        v += 1
        n = v.bit_length()
        self.u(n - 1, 0)
        self.u(n, v)

    def se(self, v):
        self.ue(2 * v - 1 if v > 0 else -2 * v)

    def copy_bits(self, r, start, end):
        for p in range(start, end):
            self.bits.append((r.b[p >> 3] >> (7 - (p & 7))) & 1)

    def aligned(self):
        return len(self.bits) % 8 == 0

    def trailing(self):
        self.bits.append(1)
        while len(self.bits) % 8:
            self.bits.append(0)

    def bytes(self):
        out = bytearray()
        for i in range(0, len(self.bits), 8):
            v = 0
            chunk = self.bits[i:i + 8]
            chunk += [0] * (8 - len(chunk))
            for b in chunk:
                v = (v << 1) | b
            out.append(v)
        return bytes(out)


def trailing_pos(b):
    last = len(b) * 8 - 1
    while last >= 0 and not ((b[last >> 3] >> (7 - (last & 7))) & 1):
        last -= 1
    return last


HIGH = (100, 110, 122, 244, 44, 83, 86, 118, 128, 138, 139, 134, 135)


def parse_sps(rb):
    r = R(rb)
    s = {}
    s['profile'] = r.u(8)
    s['constraint'] = r.u(8)
    s['level'] = r.u(8)
    s['id'] = r.ue()
    s['cf'] = 1
    s['sep'] = 0
    s['bdl'] = 0
    s['bdc'] = 0
    s['bypass'] = 0
    if s['profile'] in HIGH:
        s['cf'] = r.ue()
        if s['cf'] == 3:
            s['sep'] = r.u(1)
        s['bdl'] = r.ue()
        s['bdc'] = r.ue()
        s['bypass'] = r.u(1)
        if r.u(1):
            raise ValueError("scaling matrices unsupported")
    s['lmfn'] = r.ue()
    s['poc_type'] = r.ue()
    if s['poc_type'] == 0:
        s['lmpl'] = r.ue()
    elif s['poc_type'] == 1:
        raise ValueError("poc type 1 unsupported")
    s['refs'] = r.ue()
    s['gaps'] = r.u(1)
    s['w'] = r.ue()
    s['h'] = r.ue()
    s['fmo'] = r.u(1)
    s['mbaff'] = 0
    if not s['fmo']:
        s['mbaff'] = r.u(1)
    s['d8'] = r.u(1)
    s['crop'] = r.u(1)
    s['cropv'] = (0, 0, 0, 0)
    if s['crop']:
        s['cropv'] = (r.ue(), r.ue(), r.ue(), r.ue())
    s['vui_raw'] = (rb, r.p, trailing_pos(rb))
    return s


def write_sps(s, profile=None, keep_vui=False):
    w = W()
    p = s['profile'] if profile is None else profile
    w.u(8, p)
    w.u(8, s['constraint'] & 0xFC)
    w.u(8, s['level'])
    w.ue(s['id'])
    if p in HIGH:
        w.ue(s['cf'])
        if s['cf'] == 3:
            w.u(1, s['sep'])
        w.ue(s['bdl'])
        w.ue(s['bdc'])
        w.u(1, s['bypass'])
        w.u(1, 0)
    w.ue(s['lmfn'])
    w.ue(s['poc_type'])
    if s['poc_type'] == 0:
        w.ue(s['lmpl'])
    w.ue(s['refs'])
    w.u(1, s['gaps'])
    w.ue(s['w'])
    w.ue(s['h'])
    w.u(1, s['fmo'])
    if not s['fmo']:
        w.u(1, s['mbaff'])
    w.u(1, s['d8'])
    w.u(1, s['crop'])
    if s['crop']:
        for v in s['cropv']:
            w.ue(v)
    if keep_vui:
        rb, a, b = s['vui_raw']
        w.copy_bits(R(rb), a, b)
    else:
        w.u(1, 0)
    w.trailing()
    return w.bytes()


def parse_pps(rb):
    r = R(rb)
    p = {}
    p['id'] = r.ue()
    p['sps'] = r.ue()
    p['cabac'] = r.u(1)
    p['bfpo'] = r.u(1)
    if r.ue() != 0:
        raise ValueError("fmo")
    p['nref0'] = r.ue()
    p['nref1'] = r.ue()
    p['wp'] = r.u(1)
    p['wb'] = r.u(2)
    p['qp_pos'] = r.p
    p['qp'] = r.se()
    p['qp_end'] = r.p
    p['qs'] = r.se()
    p['cqo'] = r.se()
    p['dfc'] = r.u(1)
    p['cip'] = r.u(1)
    p['rpc'] = r.u(1)
    p['t8'] = 0
    if r.more():
        p['t8'] = r.u(1)
    return p


def rewrite_pps_qp(rb, delta):
    p = parse_pps(rb)
    r = R(rb)
    w = W()
    w.copy_bits(r, 0, p['qp_pos'])
    w.se(p['qp'] + delta)
    w.copy_bits(r, p['qp_end'], trailing_pos(rb))
    w.trailing()
    return w.bytes()


def parse_slice(rb, nal_type, nal_ref, sps, pps):
    r = R(rb)
    h = {}
    h['first_mb'] = r.ue()
    h['type'] = r.ue()
    st = h['type'] % 5
    h['st'] = st
    h['pps'] = r.ue()
    if sps['sep']:
        h['cpid'] = r.u(2)
    h['frame_num'] = r.u(sps['lmfn'] + 4)
    h['field'] = 0
    h['bottom'] = 0
    if not sps['fmo']:
        h['field'] = r.u(1)
        if h['field']:
            h['bottom'] = r.u(1)
    if nal_type == 5:
        h['idr_id'] = r.ue()
    if sps['poc_type'] == 0:
        h['poc_lsb'] = r.u(sps['lmpl'] + 4)
        if pps['bfpo'] and not h['field']:
            h['dpb'] = r.se()
    if pps['rpc']:
        h['rpc'] = r.ue()
    if st == 1:
        h['dsp'] = r.u(1)
    h['nref'] = [pps['nref0'] + 1, pps['nref1'] + 1]
    h['override'] = 0
    if st in (0, 1, 3):
        h['override'] = r.u(1)
        if h['override']:
            h['nref'][0] = r.ue() + 1
            if st == 1:
                h['nref'][1] = r.ue() + 1
    h['mods'] = [[], []]
    if st not in (2, 4):
        for l in range(2 if st == 1 else 1):
            if r.u(1):
                while True:
                    idc = r.ue()
                    if idc == 3:
                        break
                    h['mods'][l].append((idc, r.ue()))
    h['pwt'] = None
    if (pps['wp'] and st in (0, 3)) or (pps['wb'] == 1 and st == 1):
        pwt = {}
        pwt['ld'] = r.ue()
        chroma = sps['cf'] != 0 and not sps['sep']
        if chroma:
            pwt['cd'] = r.ue()
        pwt['lists'] = []
        for l in range(2 if st == 1 else 1):
            entries = []
            for i in range(h['nref'][l]):
                e = {}
                if r.u(1):
                    e['l'] = (r.se(), r.se())
                if chroma and r.u(1):
                    e['c'] = (r.se(), r.se(), r.se(), r.se())
                entries.append(e)
            pwt['lists'].append(entries)
        h['pwt'] = pwt
    h['mark'] = None
    if nal_ref:
        if nal_type == 5:
            h['mark'] = ('idr', r.u(1), r.u(1))
        else:
            if r.u(1):
                ops = []
                while True:
                    op = r.ue()
                    if op == 0:
                        break
                    args = []
                    if op in (1, 3):
                        args.append(r.ue())
                    if op == 2:
                        args.append(r.ue())
                    if op in (3, 6):
                        args.append(r.ue())
                    if op == 4:
                        args.append(r.ue())
                    ops.append((op, args))
                h['mark'] = ('mmco', ops)
            else:
                h['mark'] = ('sw',)
    if pps['cabac'] and st not in (2, 4):
        h['cabac_init'] = r.ue()
    h['qpd'] = r.se()
    if pps['dfc']:
        h['dfi'] = r.ue()
        if h['dfi'] != 1:
            h['da'] = r.se()
            h['db'] = r.se()
    h['end'] = r.p
    return h, r


def write_slice(h, sps, pps, nal_type, nal_ref, r, out_sps, new_frame_num=None, field=None, bottom=None, poc_lsb=None, mods=None, cpid=None):
    w = W()
    w.ue(h['first_mb'])
    w.ue(h['type'])
    w.ue(h['pps'])
    if out_sps['sep']:
        w.u(2, h.get('cpid', 0) if cpid is None else cpid)
    w.u(out_sps['lmfn'] + 4, h['frame_num'] if new_frame_num is None else new_frame_num)
    fld = h['field'] if field is None else field
    if not out_sps['fmo']:
        w.u(1, fld)
        if fld:
            w.u(1, h['bottom'] if bottom is None else bottom)
    if nal_type == 5:
        w.ue(h['idr_id'])
    if out_sps['poc_type'] == 0:
        w.u(out_sps['lmpl'] + 4, h['poc_lsb'] if poc_lsb is None else poc_lsb)
        if pps['bfpo'] and not fld:
            w.se(h.get('dpb', 0))
    if pps['rpc']:
        w.ue(h['rpc'])
    st = h['st']
    if st == 1:
        w.u(1, h['dsp'])
    if st in (0, 1, 3):
        if mods is not None:
            w.u(1, 1)
            w.ue(h['nref'][0] - 1)
            if st == 1:
                w.ue(h['nref'][1] - 1)
        else:
            w.u(1, h['override'])
            if h['override']:
                w.ue(h['nref'][0] - 1)
                if st == 1:
                    w.ue(h['nref'][1] - 1)
    if st not in (2, 4):
        lm = mods if mods is not None else h['mods']
        for l in range(2 if st == 1 else 1):
            if lm[l]:
                w.u(1, 1)
                for idc, v in lm[l]:
                    w.ue(idc)
                    w.ue(v)
                w.ue(3)
            else:
                w.u(1, 0)
    if h['pwt'] is not None:
        pwt = h['pwt']
        w.ue(pwt['ld'])
        chroma = sps['cf'] != 0 and not sps['sep']
        if chroma:
            w.ue(pwt['cd'])
        for entries in pwt['lists']:
            for e in entries:
                if 'l' in e:
                    w.u(1, 1)
                    w.se(e['l'][0])
                    w.se(e['l'][1])
                else:
                    w.u(1, 0)
                if chroma:
                    if 'c' in e:
                        w.u(1, 1)
                        for v in e['c']:
                            w.se(v)
                    else:
                        w.u(1, 0)
    if nal_ref:
        if nal_type == 5:
            w.u(1, h['mark'][1] if h['mark'] and h['mark'][0] == 'idr' else 0)
            w.u(1, h['mark'][2] if h['mark'] and h['mark'][0] == 'idr' else 0)
        elif h['mark'] and h['mark'][0] == 'mmco':
            w.u(1, 1)
            for op, args in h['mark'][1]:
                w.ue(op)
                for a in args:
                    w.ue(a)
            w.ue(0)
        else:
            w.u(1, 0)
    if pps['cabac'] and st not in (2, 4):
        w.ue(h['cabac_init'])
    w.se(h['qpd'])
    if pps['dfc']:
        w.ue(h['dfi'])
        if h['dfi'] != 1:
            w.se(h['da'])
            w.se(h['db'])
    if pps['cabac']:
        while not w.aligned():
            w.u(1, 1)
        start = (h['end'] + 7) & ~7
        w.copy_bits(r, start, len(r.b) * 8)
        return w.bytes()
    end = trailing_pos(r.b)
    w.copy_bits(r, h['end'], end)
    w.trailing()
    return w.bytes()


def nal(t, ref, payload):
    return b'\x00\x00\x00\x01' + bytes([(ref << 5) | t]) + escape(payload)


def convert_paff(src, bff=0):
    nals = split_nals(src)
    s = pps = None
    out = bytearray()
    out_sps = None
    dpb = []
    i = -1
    maxrefs = None
    for n in nals:
        t = n[0] & 31
        ref = (n[0] >> 5) & 3
        rb = unescape(n[1:])
        if t == 7:
            s = parse_sps(rb)
            maxrefs = max(1, s['refs'])
            out_sps = dict(s)
            out_sps['fmo'] = 0
            out_sps['mbaff'] = 0
            out_sps['d8'] = 1
            out_sps['poc_type'] = 0
            out_sps['lmpl'] = 6
            out_sps['refs'] = min(16, 2 * maxrefs + 2)
            out_sps['crop'] = 0
            out += nal(7, 3, write_sps(out_sps))
            continue
        if t == 8:
            pps = parse_pps(rb)
            out += nal(8, 3, rb)
            continue
        if t not in (1, 5):
            continue
        h, r = parse_slice(rb, t, ref, s, pps)
        if h['mark'] and h['mark'][0] == 'mmco':
            raise ValueError("mmco not supported for field conversion")
        if h['first_mb'] == 0:
            if i >= 0 and prev_ref:
                dpb.append((i, prev_fn))
                if len(dpb) > maxrefs:
                    dpb.pop(0)
            i += 1
            if t == 5:
                dpb = []
            prev_ref = ref
            prev_fn = h['frame_num']
        maxfn = 1 << (s['lmfn'] + 4)
        k = i // 2
        par = (i % 2) ^ bff
        mods = None
        if h['st'] == 0:
            cur_fn = h['frame_num']
            def fnw(fn):
                return fn if fn <= cur_fn else fn - maxfn
            lst = sorted(dpb, key=lambda e: -fnw(e[1]))
            nref = h['nref'][0]
            lst = lst + [None] * (nref + 1)
            pred = cur_fn
            idx = 0
            for idc, v in h['mods'][0]:
                if idc == 0:
                    nw = pred - (v + 1)
                    if nw < 0:
                        nw += maxfn
                else:
                    nw = pred + (v + 1)
                    if nw >= maxfn:
                        nw -= maxfn
                pred = nw
                pn = nw - maxfn if nw > cur_fn else nw
                target = None
                for e in dpb:
                    if fnw(e[1]) == pn:
                        target = e
                rest = [x for x in lst[idx:] if x is not target]
                lst = lst[:idx] + [target] + rest
                idx += 1
            final = lst[:nref]
            if any(x is None for x in final):
                raise ValueError("missing ref in list")
            fmax = 2 * maxfn
            kc = k % maxfn
            pred = 2 * kc + 1
            fm = []
            for e in final:
                j = e[0]
                kj = (j // 2) % maxfn
                pj = (j % 2) ^ bff
                wj = kj if kj <= kc else kj - maxfn
                pn = 2 * wj + (1 if pj == par else 0)
                if pn < pred:
                    fm.append((0, pred - pn - 1))
                elif pn > pred:
                    fm.append((1, pn - pred - 1))
                else:
                    fm.append((0, fmax - 1))
                pred = pn
            mods = [fm, []]
        new_t = 5 if (t == 5 and par == 0) else 1
        hh = dict(h)
        if new_t == 1 and t == 5:
            hh['mark'] = ('sw',)
        if new_t == 5 and 'idr_id' not in hh:
            hh['idr_id'] = 0
        data = write_slice(hh, s, pps, new_t, ref, r, out_sps, new_frame_num=k % maxfn, field=1, bottom=par, poc_lsb=(2 * i) % (1 << (out_sps['lmpl'] + 4)), mods=mods)
        out += nal(new_t, ref, data)
    return bytes(out)


def convert_depth(src, depth, profile=244):
    nals = split_nals(src)
    out = bytearray()
    for n in nals:
        t = n[0] & 31
        ref = (n[0] >> 5) & 3
        rb = unescape(n[1:])
        if t == 7:
            s = parse_sps(rb)
            s['bdl'] = depth - 8
            s['bdc'] = depth - 8 if s['cf'] != 0 else s['bdc']
            if s['cf'] == 0:
                s['bdc'] = depth - 8
            out += nal(7, ref, write_sps(s, profile, True))
        elif t == 8:
            out += nal(8, ref, rewrite_pps_qp(rb, -6 * (depth - 8)))
        elif t in (1, 5):
            out += b'\x00\x00\x00\x01' + n
    return bytes(out)


def convert_planes(srcs):
    lists = [split_nals(s) for s in srcs]
    sps = None
    pps = None
    out = bytearray()
    for n in lists[0]:
        t = n[0] & 31
        if t == 7:
            sps = parse_sps(unescape(n[1:]))
            o = dict(sps)
            o['cf'] = 3
            o['sep'] = 1
            o['bdc'] = o['bdl']
            out += nal(7, 3, write_sps(o, 244, True))
            out_sps = o
        elif t == 8:
            pps = parse_pps(unescape(n[1:]))
            out += nal(8, 3, unescape(n[1:]))
            break
    for l in lists[1:]:
        for n in l:
            if (n[0] & 31) == 8:
                if unescape(n[1:]) != unescape([x for x in lists[0] if (x[0] & 31) == 8][0][1:]):
                    raise ValueError("pps differs")
    slices = []
    for nl in lists:
        pics = []
        for n in nl:
            t = n[0] & 31
            if t in (1, 5):
                rb = unescape(n[1:])
                h, r = parse_slice(rb, t, (n[0] >> 5) & 3, sps, pps)
                if h['first_mb'] == 0:
                    pics.append([])
                pics[-1].append((n, h, r))
        slices.append(pics)
    npics = min(len(p) for p in slices)
    for pi in range(npics):
        hdr0 = slices[0][pi][0][1]
        for c in range(3):
            for n, h, r in slices[c][pi]:
                if h['frame_num'] != hdr0['frame_num'] or h.get('poc_lsb') != hdr0.get('poc_lsb') or (n[0] & 31) != (slices[0][pi][0][0][0] & 31):
                    raise ValueError("plane structure differs")
                t = n[0] & 31
                ref = (n[0] >> 5) & 3
                data = write_slice(h, sps, pps, t, ref, r, out_sps, cpid=c)
                out += nal(t, ref, data)
    return bytes(out)


def convert_paff_b(src):
    nals = split_nals(src)
    s = pps = None
    out = bytearray()
    out_sps = None
    refs = []
    j = -1
    pics = []
    for n in nals:
        t = n[0] & 31
        ref = (n[0] >> 5) & 3
        rb = unescape(n[1:])
        if t == 7:
            s = parse_sps(rb)
            maxrefs = max(1, s['refs'])
            out_sps = dict(s)
            out_sps['fmo'] = 0
            out_sps['mbaff'] = 0
            out_sps['d8'] = 1
            out_sps['refs'] = min(16, 2 * maxrefs + 2)
            out_sps['crop'] = 0
            if s['poc_type'] != 0:
                raise ValueError("need poc type 0")
            out += nal(7, 3, write_sps(out_sps))
            continue
        if t == 8:
            pps = parse_pps(rb)
            out += nal(8, 3, rb)
            continue
        if t not in (1, 5):
            continue
        h, r = parse_slice(rb, t, ref, s, pps)
        if h['mark'] and h['mark'][0] == 'mmco':
            raise ValueError("mmco")
        maxfn = 1 << (s['lmfn'] + 4)
        maxlsb = 1 << (s['lmpl'] + 4)
        if h['first_mb'] == 0:
            if j >= 0 and pics[j]['ref']:
                refs.append(pics[j])
                if len(refs) > maxrefs:
                    refs.pop(0)
            j += 1
            if t == 5:
                refs = []
            pair = j // 2
            par = j % 2
            pics.append({'j': j, 'fn': h['frame_num'], 'poc': h['poc_lsb'], 'ref': ref, 'pair': pair, 'par': par})
        cur = pics[j]
        pair = cur['pair']
        par = cur['par']
        kc = pair % maxfn
        cur_fn = h['frame_num']
        def fnw(fn):
            return fn if fn <= cur_fn else fn - maxfn
        mods = None
        st = h['st']
        if st in (0, 1):
            lists = []
            if st == 0:
                init = [sorted(refs, key=lambda e: -fnw(e['fn']))]
            else:
                cp = cur['poc']
                before = sorted([e for e in refs if e['poc'] < cp], key=lambda e: -e['poc'])
                after = sorted([e for e in refs if e['poc'] > cp], key=lambda e: e['poc'])
                l0 = before + after
                l1 = after + before
                if len(l1) > 1 and l0 == l1:
                    l1[0], l1[1] = l1[1], l1[0]
                init = [l0, l1]
            for l in range(len(init)):
                lst = list(init[l]) + [None] * (h['nref'][l] + 1)
                pred = cur_fn
                idx = 0
                for idc, v in h['mods'][l]:
                    if idc == 0:
                        nw = pred - (v + 1)
                        if nw < 0:
                            nw += maxfn
                    else:
                        nw = pred + (v + 1)
                        if nw >= maxfn:
                            nw -= maxfn
                    pred = nw
                    pn = nw - maxfn if nw > cur_fn else nw
                    target = None
                    for e in refs:
                        if fnw(e['fn']) == pn:
                            target = e
                    rest = [x for x in lst[idx:] if x is not target]
                    lst = lst[:idx] + [target] + rest
                    idx += 1
                final = lst[:h['nref'][l]]
                if any(x is None for x in final):
                    raise ValueError("missing ref")
                lists.append(final)
            fmax = 2 * maxfn
            mods = [[], []]
            for l, final in enumerate(lists):
                pred = 2 * kc + 1
                for e in final:
                    kj = e['pair'] % maxfn
                    wj = kj if kj <= kc else kj - maxfn
                    pn = 2 * wj + (1 if e['par'] == par else 0)
                    if pn < pred:
                        mods[l].append((0, pred - pn - 1))
                    elif pn > pred:
                        mods[l].append((1, pn - pred - 1))
                    else:
                        mods[l].append((0, fmax - 1))
                    pred = pn
        new_t = 5 if (t == 5 and j == 0) else 1
        hh = dict(h)
        if new_t == 1 and t == 5:
            hh['mark'] = ('sw',)
        oref = ref
        if not ref:
            oref = 1
            hh['mark'] = ('sw',)
        data = write_slice(hh, s, pps, new_t, oref, r, out_sps, new_frame_num=kc, field=1, bottom=par, poc_lsb=cur['poc'] % maxlsb, mods=mods)
        out += nal(new_t, oref, data)
    return bytes(out)




import hashlib
import os
import subprocess
import tempfile

PSRC = "testsrc2=size=176x144:rate=25,rotate=angle=t*0.8:fillcolor=gray"
SSRC = "testsrc2=size=128x96:rate=25,rotate=angle=t*0.8:fillcolor=gray"
ISRC = "testsrc2=size=176x144:rate=50,rotate=angle=t*1.5:fillcolor=gray,interlace=scan=tff"
ISRC_B = "testsrc2=size=176x144:rate=50,rotate=angle=t*1.5:fillcolor=gray,interlace=scan=bff"
ISRC_S = "testsrc2=size=128x96:rate=50,rotate=angle=t*1.5:fillcolor=gray,interlace=scan=tff"
FSRC = "testsrc2=size=160x128:rate=50,rotate=angle=t*1.5:fillcolor=gray,interlace=scan={o},setfield={o},separatefields"
FORMATS = {
    'yuv420p': 'yuv420p', 'yuv420p10le': 'yuv420p10', 'yuv420p12le': 'yuv420p12', 'yuv420p14le': 'yuv420p14',
    'yuv422p': 'yuv422p', 'yuv422p10le': 'yuv422p10', 'yuv422p12le': 'yuv422p12', 'yuv422p14le': 'yuv422p14',
    'yuv444p': 'yuv444p', 'yuv444p10le': 'yuv444p10', 'yuv444p12le': 'yuv444p12', 'yuv444p14le': 'yuv444p14',
    'gray': 'gray', 'gray10le': 'gray10', 'gray12le': 'gray12', 'gray14le': 'gray14',
}


def run(cmd):
    subprocess.run(cmd, check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)


def x264(src, frames, pix, params, out, extra=()):
    cmd = ['ffmpeg', '-hide_banner', '-loglevel', 'error', '-y', '-f', 'lavfi', '-i', src, '-frames:v', str(frames), '-c:v', 'libx264', '-pix_fmt', pix]
    cmd += list(extra)
    if params:
        cmd += ['-x264-params', params]
    cmd += ['-f', 'h264', out]
    run(cmd)


def probe(path):
    o = subprocess.check_output(['ffprobe', '-v', 'error', '-select_streams', 'v', '-show_entries', 'stream=width,height,pix_fmt', '-of', 'csv=p=0', path]).decode().strip().split(',')
    return int(o[0]), int(o[1]), o[2]


def bps_of(pix):
    return 2 if any(t in pix for t in ('10', '12', '14', '9')) else 1


def native_luma(path):
    w, h, pix = probe(path)
    raw = subprocess.check_output(['ffmpeg', '-hide_banner', '-loglevel', 'error', '-i', path, '-fps_mode', 'passthrough', '-f', 'rawvideo', '-'])
    b = bps_of(pix)
    fs = w * h * b if pix.startswith('gray') else (w * h * b * 3) // 2
    return [raw[i * fs:i * fs + w * h * b] for i in range(len(raw) // fs)], w, h


def frames_of(path, pix):
    w, h, _ = probe(path)
    if pix.startswith('gray'):
        planes, w, h = native_luma(path)
        return planes, w, h
    raw = subprocess.check_output(['ffmpeg', '-hide_banner', '-loglevel', 'error', '-i', path, '-fps_mode', 'passthrough', '-f', 'rawvideo', '-pix_fmt', pix, '-'])
    b = bps_of(pix)
    if pix.startswith('yuv420'):
        fs = w * h * b * 3 // 2
    elif pix.startswith('yuv422'):
        fs = w * h * b * 2
    else:
        fs = w * h * b * 3
    return [raw[i * fs:(i + 1) * fs] for i in range(len(raw) // fs)], w, h


class Builder:
    def __init__(self, outdir):
        self.outdir = outdir
        self.tmp = tempfile.mkdtemp()
        self.lines = []

    def t(self, name):
        return os.path.join(self.tmp, name)

    def add(self, name, data, pix, interlaced, structure, frames=None, w=None, h=None):
        path = os.path.join(self.outdir, name)
        open(path, 'wb').write(data)
        if frames is None:
            frames, w, h = frames_of(path, pix)
        m = hashlib.md5()
        for f in frames:
            m.update(f)
        self.lines.append('stream %s %s %d %d %d %d %s %s' % (name, FORMATS[pix], w, h, len(frames), interlaced, structure, m.hexdigest()))

    def container(self, name, source):
        path = os.path.join(self.outdir, name)
        mp4 = self.t(name + '.mp4')
        run(['ffmpeg', '-hide_banner', '-loglevel', 'error', '-y', '-fflags', '+genpts', '-r', '25', '-i', os.path.join(self.outdir, source), '-c', 'copy', mp4])
        run(['ffmpeg', '-hide_banner', '-loglevel', 'error', '-y', '-i', mp4, '-c', 'copy', path])
        self.lines.append('container %s %s' % (name, source))

    def plain(self, name, src, frames, pix, params, extra=(), interlaced=0, structure='frame'):
        x264(src, frames, pix, params, self.t(name), extra)
        self.add(name, open(self.t(name), 'rb').read(), pix, interlaced, structure)

    def depth(self, name, src, frames, pix_in, depth, pix_out, params, extra=(), interlaced=0, structure='frame'):
        x264(src, frames, pix_in, params, self.t(name + '.8'), ('-coder', '0') + tuple(extra))
        self.add(name, convert_depth(open(self.t(name + '.8'), 'rb').read(), depth), pix_out, interlaced, structure)

    def paff(self, name, frames, pix, params, bff=0, depth=0, bfields=False):
        order = 'bff' if bff else 'tff'
        x264(FSRC.format(o=order), frames, pix, params, self.t(name + '.prog'), ('-coder', '0'))
        src = open(self.t(name + '.prog'), 'rb').read()
        data = convert_paff_b(src) if bfields else convert_paff(src, bff)
        out_pix = pix
        if depth:
            data = convert_depth(data, depth)
            out_pix = {'yuv420p': 'yuv420p', 'yuv422p': 'yuv422p'}[pix] + str(depth) + 'le'
        self.add(name, data, out_pix, 1, 'field_pair')

    def planes(self, name, frames, params):
        srcs = []
        decoded = []
        for p in 'yuv':
            x264(PSRC + ',format=yuv444p,extractplanes=' + p, frames, 'gray', params, self.t(name + '.' + p), ('-coder', '0'))
            srcs.append(open(self.t(name + '.' + p), 'rb').read())
            planes, w, h = native_luma(self.t(name + '.' + p))
            decoded.append(planes)
        n = min(len(d) for d in decoded)
        frames_out = [decoded[0][i] + decoded[1][i] + decoded[2][i] for i in range(n)]
        self.add(name, convert_planes(srcs), 'yuv444p', 0, 'frame', frames_out, w, h)


def main(outdir):
    b = Builder(outdir)
    i = ('-flags', '+ildct+ilme')
    b.plain('hi10_cabac.h264', PSRC, 8, 'yuv420p10le', 'bframes=3:weightp=2:ref=3:direct=temporal')
    b.plain('hi10_cavlc.h264', PSRC, 8, 'yuv420p10le', 'bframes=2:weightp=2', ('-coder', '0'))
    b.plain('hi422_cabac.h264', PSRC, 8, 'yuv422p', 'bframes=2:ref=2')
    b.plain('hi422_10_cavlc.h264', PSRC, 8, 'yuv422p10le', 'bframes=2:weightp=2:cqm=jvt', ('-coder', '0'))
    b.plain('hi444_cabac.h264', PSRC, 8, 'yuv444p', 'bframes=2:cqm=jvt:ref=2')
    b.plain('hi444_10_cavlc.h264', PSRC, 8, 'yuv444p10le', 'bframes=2:chroma-qp-offset=4', ('-coder', '0'))
    b.plain('lossless444_cabac.h264', SSRC, 5, 'yuv444p', 'bframes=1', ('-qp', '0'))
    b.plain('lossless444_10_cavlc.h264', SSRC, 5, 'yuv444p10le', 'bframes=1', ('-qp', '0', '-coder', '0'))
    b.plain('lossless420_cavlc.h264', SSRC, 5, 'yuv420p', 'bframes=1', ('-qp', '0', '-coder', '0'))
    b.plain('gray10_cabac.h264', PSRC, 8, 'gray10le', 'bframes=2:weightp=2')
    b.depth('depth12_420.h264', PSRC, 8, 'yuv420p', 12, 'yuv420p12le', 'bframes=2:weightp=2:ref=2')
    b.depth('depth14_444.h264', PSRC, 8, 'yuv444p', 14, 'yuv444p14le', 'bframes=2:weightp=2')
    b.depth('depth12_422.h264', PSRC, 8, 'yuv422p', 12, 'yuv422p12le', 'bframes=2')
    b.depth('depth14_gray.h264', PSRC, 8, 'gray', 14, 'gray14le', 'bframes=2')
    b.depth('depth14_lossless444.h264', SSRC, 4, 'yuv444p', 14, 'yuv444p14le', 'bframes=0', ('-qp', '0'))
    b.planes('separate_planes.h264', 8, 'bframes=2:b-adapt=0:ref=2:keyint=100:scenecut=0:weightp=2:direct=temporal')
    b.plain('mbaff_cabac.h264', ISRC, 8, 'yuv420p', 'tff=1:bframes=3:b-pyramid=normal:weightp=2:ref=3', i, 1, 'mbaff_frame')
    b.plain('mbaff_cavlc_temporal.h264', ISRC, 8, 'yuv420p', 'tff=1:bframes=2:direct=temporal:ref=2', i + ('-coder', '0'), 1, 'mbaff_frame')
    b.plain('mbaff_bff.h264', ISRC_B, 8, 'yuv420p', 'bff=1:bframes=2', i, 1, 'mbaff_frame')
    b.plain('mbaff_422_10.h264', ISRC, 8, 'yuv422p10le', 'tff=1:bframes=2:weightp=2', i, 1, 'mbaff_frame')
    b.plain('mbaff_lossless444.h264', ISRC_S, 4, 'yuv444p', 'tff=1:bframes=1', i + ('-qp', '0'), 1, 'mbaff_frame')
    b.plain('mbaff_444_10_cavlc.h264', ISRC, 6, 'yuv444p10le', 'tff=1:bframes=2', i + ('-coder', '0'), 1, 'mbaff_frame')
    b.plain('mbaff_gray.h264', ISRC, 6, 'gray', 'tff=1:bframes=2', i, 1, 'mbaff_frame')
    b.plain('mbaff_slices_cavlc.h264', ISRC, 6, 'yuv420p', 'tff=1:slices=3:bframes=1', i + ('-coder', '0'), 1, 'mbaff_frame')
    b.plain('fake_interlaced.h264', PSRC, 6, 'yuv420p', 'fake-interlaced=1:pic-struct=1:tff=1:bframes=2', (), 1, 'mbaff_frame')
    b.depth('depth12_mbaff.h264', ISRC, 6, 'yuv420p', 12, 'yuv420p12le', 'tff=1:bframes=2', i, 1, 'mbaff_frame')
    b.paff('paff_p.h264', 16, 'yuv420p', 'bframes=0:ref=3:keyint=1000:scenecut=0:weightp=2')
    b.paff('paff_intra.h264', 12, 'yuv420p', 'bframes=0:keyint=1')
    b.paff('paff_422_10.h264', 12, 'yuv422p10le', 'bframes=0:ref=2:keyint=1000:scenecut=0')
    b.paff('paff_444.h264', 12, 'yuv444p', 'bframes=0:ref=2:keyint=1000:scenecut=0')
    b.paff('paff_gray.h264', 12, 'gray', 'bframes=0:ref=2:keyint=1000:scenecut=0')
    b.paff('paff_bff.h264', 12, 'yuv420p', 'bframes=0:ref=3:keyint=1000:scenecut=0', bff=1)
    b.paff('paff_bfields.h264', 16, 'yuv420p', 'bframes=1:b-adapt=0:b-pyramid=none:ref=3:keyint=1000:scenecut=0:weightp=0:weightb=1:direct=none', bfields=True)
    b.paff('paff_12.h264', 12, 'yuv420p', 'bframes=0:ref=2:keyint=1000:scenecut=0', depth=12)
    b.container('mbaff_422_10.mp4', 'mbaff_422_10.h264')
    b.container('hi444_10_cavlc.mkv', 'hi444_10_cavlc.h264')
    b.container('paff_p.mp4', 'paff_p.h264')
    b.container('mbaff_cabac.mp4', 'mbaff_cabac.h264')
    b.container('depth14_444.mkv', 'depth14_444.h264')
    open(os.path.join(outdir, 'h264_ext_manifest.txt'), 'w').write('\n'.join(b.lines) + '\n')


if __name__ == '__main__':
    main(sys.argv[1] if len(sys.argv) > 1 else os.path.dirname(os.path.abspath(__file__)))
