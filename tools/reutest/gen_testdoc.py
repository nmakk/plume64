#!/usr/bin/env python3
"""Tesztdokumentum PLM2-ben: gen_testdoc.py kimenet.bin [bajtszam]  -> kiirja a hosszt es az ellenorzoosszeget.
Az osszeg ugyanaz, mint a lemezolvaso PRG-ben: s = rol16(s) ^ bajt, a teljes fajlra (fejlec is)."""
import sys

target = int(sys.argv[2]) if len(sys.argv) > 2 else 40000
words = ("alma korte szilva barack cseresznye meggy narancs citrom kiwi banan "
         "szolo dio mogyoro mandula gesztenye").split()
body = bytearray()
n = 0
while len(body) < target:
    n += 1
    para = bytearray(("p%04d " % n).encode())
    for k in range(9):
        w = words[(n * 7 + k * 3) % len(words)].encode()
        if n % 7 == 0 and k == 4:
            para += b"\x11" + w + b"\x10"          # felkover szo
        elif n % 11 == 0 and k == 2:
            para += b"\x12" + w + b"\x10"          # dolt szo
        else:
            para += w
        para += b" "
    para += bytes([13])
    body += para
body = bytes(body[:target - 1]) + bytes([13])
data = b"PLM2" + bytes([len(body) & 255, len(body) >> 8]) + body
open(sys.argv[1], "wb").write(data)


def checksum(d):
    s = 0
    for b in d:
        s = (((s << 1) | (s >> 15)) & 0xFFFF) ^ b
    return s


print("body", len(body), "file", len(data), "sum %04X" % checksum(data))
