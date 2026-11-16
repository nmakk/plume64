"""Test du REU 24 bits : reu24_test.py <scratch> <N paragraphes>
REUTEST build avec -DTESTPARAS=<N>UL (-DNORTF) ; le document de test fait 45*N octets.
Verifie, apres des sauts (ascenseur), des deplacements (touches) et des recherches lointaines,
que la fenetre contient exactement la tranche attendue du document."""
import re, sys, time, urllib.request
IP = "192.168.0.158"; S = sys.argv[1]; N = int(sys.argv[2])
def req(m, u, data=None): return urllib.request.urlopen(urllib.request.Request(f"http://{IP}/v1/{u}", data=data, method=m), timeout=60).read()
rd = lambda a, n: req("GET", f"machine:readmem?address={a:04X}&length={n}")
wr = lambda a, d: req("PUT", f"machine:writemem?address={a:04X}&data={bytes(d).hex()}")
t = open(f"{S}/rt.map").read().split("Exports list by value")[1]
sym = lambda n: int(re.search(r"\b" + n + r"\s+([0-9A-F]{6}) ", t).group(1), 16)
TX, DL, CU, AN = sym("_text"), sym("_doclen"), sym("_cursor"), sym("_anchor")
u16 = lambda b, i=0: b[i] | b[i + 1] << 8
u24 = lambda b, i=0: b[i] | b[i + 1] << 8 | b[i + 2] << 16
TOTAL = 45 * N
def para(j):
    p = b"p%06d " % j
    for k in range(9): p += b"w%02d " % ((j * 7 + k * 3) % 100)
    return p + b"\r"
def expected(a, n):
    out = b""
    j = a // 45 + 1
    skip = a % 45
    while len(out) < n + skip:
        out += para(j); j += 1
    return out[skip:skip + n]
def state():
    st = rd(0x02C0, 11)
    wb, sl, lb, nl = u24(st, 0), u24(st, 3), u24(st, 6), u16(st, 9)
    dl = u16(rd(DL, 2)); cu = u16(rd(CU, 2))
    win = rd(TX, dl)
    return wb, sl, lb, nl, dl, cu, win
def check(tag):
    wb, sl, lb, nl, dl, cu, win = state()
    ok_total = wb + dl + sl == TOTAL
    ok_win = bytes(win) == expected(wb, dl)
    print(f"{tag:28s} win_base={wb:8d} doclen={dl:5d} suf={sl:8d} cursor_g={wb+cu:8d} "
          f"total {'OK' if ok_total else 'FAUX'} contenu {'OK' if ok_win else 'FAUX'}", flush=True)
    return ok_total and ok_win
def press(k, wait=3):
    wr(0x0277, [k]); wr(0xC6, [1]); time.sleep(wait)

req("PUT", "machine:reset"); time.sleep(3)
wr(0x02AB, [0, 0])
req("POST", "runners:run_crt", open(f"{S}/rt.crt", "rb").read())
t0 = time.time()
time.sleep(10)
while rd(0x02AB, 2) == b"\0\0" and time.time() - t0 < 300: time.sleep(3)
print("demarrage %.0f s" % (time.time() - t0), flush=True)
time.sleep(2)
ok = check("depart")
for y in (100, 191, 60, 16, 150, 40, 175):
    wr(0x02A8, [0x38, 0x01, y]); wr(0x02A7, [1])
    time.sleep(6)
    ok &= check(f"ascenseur y={y}")
for i in range(4):
    press(0x90, 4); ok &= check(f"100 lignes bas #{i+1}")
for i in range(4):
    press(0x05, 4); ok &= check(f"100 lignes haut #{i+1}")
for needle, j in (("p099000 ", 99000), ("p000500 ", 500), ("p050000 ", 50000), ("p099999 ", 99999), ("p000001 ", 1)):
    if j > N: continue
    wr(0x0375, needle.encode() + b"\0")
    press(0x8C, 60)
    wb, sl, lb, nl, dl, cu, win = state()
    an = u16(rd(AN, 2))
    sel = bytes(win[an:cu]) if cu > an and an != 0xFFFF else b""
    g = wb + an
    good = (sel == needle.encode() and g == 45 * (j - 1))
    print(f"recherche {needle}: sel={sel!r} global={g} attendu={45*(j-1)} {'OK' if good else 'FAUX'}", flush=True)
    ok &= good and check("  fenetre apres recherche")
print("EREDMENY", "MINDEN RENDBEN" if ok else "HIBA")
