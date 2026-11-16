#!/usr/bin/env python3
"""A REUTEST build allapotanak ellenorzese: reuverify.py rt.map
Kiolvassa az ablak allapotat ($03ED..) es a text[] tartalmat, osszeveti a generalt dokumentum szeletevel."""
import re, sys, urllib.request

IP = "192.168.0.158"


def rd(a, n):
    return urllib.request.urlopen(f"http://{IP}/v1/machine:readmem?address={a:04X}&length={n}", timeout=10).read()


def sym(mapfile, name):
    t = open(mapfile).read()
    m = re.search(r"\b" + name + r"\s+([0-9A-F]{6}) ", t.split("Exports list by value")[1])
    return int(m.group(1), 16)


doc = bytearray()
for j in range(1, 1396):
    p = bytearray(b"p%04d " % j)
    for k in range(9):
        p += b"w%02d " % ((j * 7 + k * 3) % 100)
    p.append(13)
    doc += p
assert len(doc) == 59985

mp = sys.argv[1]
text, doclen_a, cursor_a = sym(mp, "_text"), sym(mp, "_doclen"), sym(mp, "_cursor")
st = rd(0x03ED, 11)
u = lambda b, i: b[i] | b[i + 1] << 8
checks, fails = st[0], st[1]
win_base, suf_len, lines_before, ncuts, nlines = u(st, 2), u(st, 4), u(st, 6), st[8], u(st, 9)
doclen = u(rd(doclen_a, 2), 0)
cursor = u(rd(cursor_a, 2), 0)
win = rd(text, doclen)
exp = doc[win_base:win_base + doclen]
ok_content = bytes(win) == bytes(exp)
ok_total = win_base + doclen + suf_len == 59985
print(f"checks={checks} fails={fails} win_base={win_base} doclen={doclen} suf_len={suf_len} "
      f"lines_before={lines_before} ncuts={ncuts} nlines={nlines} cursor={cursor}")
print(f"  tartalom egyezik: {ok_content}   osszhossz 59985: {ok_total}   "
      f"kurzor sora (globalis, 1 sor = 43 B): {(win_base + cursor) // 43 + 1}   lines_before+sor ~ {lines_before}")
if not ok_content:
    for i, (a, b) in enumerate(zip(win, exp)):
        if a != b:
            print("  elso eltérés:", i, "kapott", bytes(win[i:i + 20]), "vart", bytes(exp[i:i + 20]))
            break
sys.exit(0 if ok_content and ok_total and fails == 0 else 1)
