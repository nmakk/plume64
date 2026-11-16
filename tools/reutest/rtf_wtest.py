"""RTF iro-teszt: rtf_wtest.py <scratch> <rtf-konyvtar>
Betolti a small.rtf-et, kiirja OUT neven RTF-ben, majd a diskcheck.prg-vel visszaolvassa (hossz + osszeg)
es osszeveti a hoston szamolt vart kimenettel."""
import re, sys, time, urllib.request
IP="192.168.0.158"; S=sys.argv[1]; D=sys.argv[2]
UC=[0xE0,0xE2,0xE4,0xE7,0xE9,0xE8,0xEA,0xEB,0xEE,0xEF,0xF4,0xF6,0xF9,0xFB,0xFC,
    0xC0,0xC2,0xC7,0xC9,0xC8,0xCA,0xCB,0xCE,0xCF,0xD4,0xD9,0xDB,0xDC,
    0x153,0x152,0xAB,0xBB,0xB0,0x20AC,0xE6,0xF8,0xE5,0xC6,0xD8,0xC5,
    0xE1,0xC1,0xED,0xCD,0xF3,0xD3,0xFA,0xDA,0x151,0x150,0x171,0x170,0xD6]
def expected(doc):
    o=b"{\\rtf1\\ansi\\ansicpg1252\\deff0{\\fonttbl{\\f0 Times New Roman;}}\\f0\\fs24 "
    ws=0
    for c in doc:
        if 0x10<=c<=0x17:
            ns=c&7
            if ns==ws: continue
            if (ns^ws)&1: o+=b"\\b" if ns&1 else b"\\b0"
            if (ns^ws)&2: o+=b"\\i" if ns&2 else b"\\i0"
            if (ns^ws)&4: o+=b"\\ul" if ns&4 else b"\\ulnone"
            o+=b" "; ws=ns
        elif c==13: o+=b"\\par\n"
        elif c>=128: o+=b"\\u%d?"%(UC[c-128] if c-128<53 else ord('?'))
        elif c>=32:
            if c in (92,123,125): o+=b"\\"
            o+=bytes([c])
    return o+b"}"
def checksum(d):
    s=0
    for b in d: s=(((s<<1)|(s>>15))&0xFFFF)^b
    return s
def req(m,u,data=None): return urllib.request.urlopen(urllib.request.Request(f"http://{IP}/v1/{u}",data=data,method=m),timeout=60).read()
rd=lambda a,n: req("GET",f"machine:readmem?address={a:04X}&length={n}")
wr=lambda a,d: req("PUT",f"machine:writemem?address={a:04X}&data={bytes(d).hex()}")
req("PUT","machine:reset"); time.sleep(3)
req("POST","drives/a:mount?type=d64&mode=readwrite",open(f"{D}/t.d64","rb").read())
req("POST","runners:run_crt",open(f"{S}/rt.crt","rb").read())
time.sleep(30)
wr(0x038D,b"RTFTEST\0"); wr(0x0277,[0x9E]); wr(0xC6,[1])
time.sleep(25)
wr(0x0277,[0x9F]); wr(0xC6,[1])
time.sleep(40)                      # iras: nincs memoriaolvasas
req("POST","runners:run_prg",open(f"{S}/diskcheck.prg","rb").read())
time.sleep(20)
r=rd(0xC000,6)
ln=r[0]|r[1]<<8; sm=r[2]|r[3]<<8
exp=expected(open(f"{D}/small.int","rb").read())
print("lemezen: hossz",ln,"osszeg %04X"%sm,"statusz %02X"%r[4],"| vart: hossz",len(exp),"osszeg %04X"%checksum(exp))
print("EREDMENY","OK" if (ln,sm)==(len(exp),checksum(exp)) else "HIBA")
