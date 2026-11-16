#!/usr/bin/env python3
"""1 MHz-es meres: a C64U CPU Speed beallitasat 1-re allitja, lefuttatja a REUTEST builden a
gorgetest, szamolja a billentyunkenti kesleltetest, a vegen mindenkepp visszaallitja 64-re."""
import statistics, subprocess, sys, time, urllib.request, urllib.parse

IP = "192.168.0.158"
S = sys.argv[1]
CFG = f"http://{IP}/v1/configs/U64%20Specific%20Settings/CPU%20Speed"


def rd(a, n):
    return urllib.request.urlopen(f"http://{IP}/v1/machine:readmem?address={a:04X}&length={n}", timeout=15).read()


def wr(a, d):
    urllib.request.urlopen(urllib.request.Request(
        f"http://{IP}/v1/machine:writemem?address={a:04X}&data={bytes(d).hex()}", method="PUT"), timeout=15).read()


def speed(v):
    urllib.request.urlopen(urllib.request.Request(f"{CFG}?value={urllib.parse.quote(v)}", method="PUT"), timeout=15).read()
    return urllib.request.urlopen(CFG, timeout=15).read().decode().split('"current" : ')[1].split(",")[0]


def lat(key):
    while rd(0xC6, 1)[0]:
        time.sleep(0.01)
    wr(0x0277, [key])
    wr(0xC6, [1])
    t = time.time()
    while rd(0xC6, 1)[0]:
        pass
    wr(0x02A7, [2])
    while rd(0x02A7, 1)[0] != 3:
        pass
    dt = time.time() - t
    wr(0x02A7, [0])
    return dt


def wb():
    s = rd(0x03EF, 2)
    return s[0] | s[1] << 8


try:
    print("sebesseg ->", speed(" 1"), flush=True)
    req = urllib.request.Request(f"http://{IP}/v1/runners:run_crt", data=open(f"{S}/rt.crt", "rb").read(), method="POST")
    urllib.request.urlopen(req, timeout=30).read()
    t0 = time.time()
    # keszen all, ha a mailbox valaszol (a generalas es a tordeles lefutott)
    while True:
        try:
            wr(0x02A7, [2])
            time.sleep(0.5)
            if rd(0x02A7, 1)[0] == 3:
                wr(0x02A7, [0])
                break
        except Exception:
            pass
        time.sleep(1)
    print(f"inditas + dokumentum generalasa + tordeles: {time.time() - t0:.1f} s", flush=True)
    for name, key, n in (("LEFELE", 0x11, 330), ("FELFELE", 0x91, 330)):
        slide, plain = [], []
        prev = wb()
        for _ in range(n):
            dt = lat(key)
            cur = wb()
            (slide if cur != prev else plain).append(dt)
            prev = cur
        f = lambda l: (f"n={len(l)} atlag={statistics.mean(l):.2f}s max={max(l):.2f}s" if l else "n=0")
        print(f"{name}: csusztatassal: {f(slide)} | csusztatas nelkul: {f(plain)}", flush=True)
finally:
    print("sebesseg visszaallitva ->", speed("64"), flush=True)
