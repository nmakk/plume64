"""RTF olvaso-teszt: rtf_test.py <scratch> <rtf-konyvtar> small|big
A scratch-ban rt.crt/rt.map (REUTEST build), a konyvtarban t.d64 + *.int."""
import re, sys, time, urllib.request
IP="192.168.0.158"; S=sys.argv[1]; D=sys.argv[2]; which=sys.argv[3]
def req(m,u,data=None): return urllib.request.urlopen(urllib.request.Request(f"http://{IP}/v1/{u}",data=data,method=m),timeout=60).read()
rd=lambda a,n: req("GET",f"machine:readmem?address={a:04X}&length={n}")
wr=lambda a,d: req("PUT",f"machine:writemem?address={a:04X}&data={bytes(d).hex()}")
t=open(f"{S}/rt.map").read().split("Exports list by value")[1]
sym=lambda n:int(re.search(r"\b"+n+r"\s+([0-9A-F]{6}) ",t).group(1),16)
TX,DL=sym("_text"),sym("_doclen")
u=lambda b,i=0:b[i]|b[i+1]<<8
req("PUT","machine:reset"); time.sleep(3)
req("POST","drives/a:mount?type=d64&mode=readwrite",open(f"{D}/t.d64","rb").read())
wr(0x02AB,[0,0])
req("POST","runners:run_crt",open(f"{S}/rt.crt","rb").read())
time.sleep(30)
name={"small":b"RTFTEST","big":b"BIGTEST"}[which]
wr(0x038D,name+b"\0")   # filename (PETSCII)
wr(0x0277,[0x9E]); wr(0xC6,[1])
exp=open(f"{D}/{which}.int","rb").read()
# a betoltes alatt nincs memoriaolvasas (DMA vs IEC): fix ido
time.sleep(25 if which=="small" else 150)
st=rd(0x02C0,11); wb=st[0]|st[1]<<8|st[2]<<16; sl=st[3]|st[4]<<8|st[5]<<16; dl=u(rd(DL,2))
win=rd(TX,dl)
total=wb+dl+sl
print("total",total,"expected",len(exp),"win_base",wb,"doclen",dl,"suf",sl)
ok = total==len(exp) and win==exp[wb:wb+dl]
print("ablak egyezik:", win==exp[wb:wb+dl], "| hossz egyezik:", total==len(exp))
if not ok and which=="small": print(win, exp)
print("EREDMENY","OK" if ok else "HIBA")
