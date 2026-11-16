"""Keresés a teljes dokumentumon: find_test.py <scratch>  (REUTEST build, 60 KB generált doksi)."""
import re, sys, time, urllib.request
IP="192.168.0.158"; S=sys.argv[1]
def rd(a,n): return urllib.request.urlopen(f"http://{IP}/v1/machine:readmem?address={a:04X}&length={n}",timeout=15).read()
def wr(a,d): urllib.request.urlopen(urllib.request.Request(f"http://{IP}/v1/machine:writemem?address={a:04X}&data={bytes(d).hex()}",method="PUT"),timeout=15).read()
t=open(f"{S}/rt.map").read().split("Exports list by value")[1]
sym=lambda n:int(re.search(r"\b"+n+r"\s+([0-9A-F]{6}) ",t).group(1),16)
FT=0x0375
AN,CU,TX,DL=[sym(x) for x in("_anchor","_cursor","_text","_doclen")]
def press(key):
    wr(0x0277,[key]); wr(0xC6,[1])
    while rd(0xC6,1)[0]: time.sleep(0.05)
u=lambda b:b[0]|b[1]<<8
urllib.request.urlopen(urllib.request.Request(f"http://{IP}/v1/runners:run_crt",data=open(f"{S}/rt.crt","rb").read(),method="POST"),timeout=30).read()
time.sleep(25)
ok=True
for needle,idx in (("p1300 ",1300),("p0005 ",5),("p0700 ",700),("p1395 ",1395),("p0001 ",1),("p0002 ",2)):
    wr(FT,needle.encode()+b"\0")
    press(0x8C); time.sleep(3)
    st=rd(0x03ED,11); wb=u(st[2:4]); sl=u(st[4:6])
    an=u(rd(AN,2)); cu=u(rd(CU,2)); dl=u(rd(DL,2))
    sel=bytes(rd(TX+an,cu-an)) if cu>an else b""
    g=wb+an
    good = g==43*(idx-1) and sel.decode("latin1")==needle and wb+dl+sl==59985
    ok&=good
    print(needle,"glob",g,"exp",43*(idx-1),"win_base",wb,"doclen",dl,"sel",sel,"OK" if good else "HIBA",flush=True)
print("EREDMENY:","MINDEN RENDBEN" if ok else "HIBA")
