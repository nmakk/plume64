"""Lemezsebesseg-meres futtatasa: jt_run.py <scratch> <t.d64>"""
import sys, time, urllib.request
IP="192.168.0.158"; S=sys.argv[1]; D=sys.argv[2]
def req(m,u,data=None): return urllib.request.urlopen(urllib.request.Request(f"http://{IP}/v1/{u}",data=data,method=m),timeout=60).read()
typ = "d81" if D.endswith(".d81") else "d64"
req("PUT","machine:reset"); time.sleep(3)
req("PUT","configs/Drive%20A%20Settings/Drive%20Type?value="+("1581" if typ=="d81" else "1541")); time.sleep(2)
req("POST",f"drives/a:mount?type={typ}&mode=readwrite",open(D,"rb").read())
req("POST","runners:run_prg",open(f"{S}/jt.prg","rb").read())
time.sleep(int(sys.argv[3]) if len(sys.argv)>3 else 420)          # lemezmuveletek alatt nincs memoriaolvasas
r=req("GET","machine:readmem?address=C000&length=256")
names=["kepernyo be, nincs sprite","kepernyo be + sprite","kepernyo ki","kepernyo ki + sprite"]
for m in range(4):
    w,rd_,sv,ld=[r[m*8+2*i]|r[m*8+2*i+1]<<8 for i in range(4)]
    f=lambda j: "%.1f s (%d B/s)"%(j/60.0, 8192*60/j) if j else "?"
    print(f"{names[m]:28s} iras {f(w)} | olvasas {f(rd_)} | SAVE {f(sv)} | LOAD {f(ld)}")
print("kesz jelzo:", hex(r[0xFF]))
