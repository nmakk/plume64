"""PLM3 : enregistre un document de 45*N octets (> 64 Ko) sous le nom OUT, relit le fichier (longueur,
somme), puis le recharge sur le MEME disque monte (un nouveau montage effacerait OUT).
plm3_test.py <scratch> <rtf-dir (t.d64)> <N>   (REUTEST build avec -DTESTPARAS=<N>UL -DNORTF)"""
import re, sys, time, urllib.request
IP="192.168.0.158"; S=sys.argv[1]; D=sys.argv[2]; N=int(sys.argv[3])
BPS=int(sys.argv[4]) if len(sys.argv)>4 else 380      # feltetelezett bajt/s a varakozasokhoz
def req(m,u,data=None): return urllib.request.urlopen(urllib.request.Request(f"http://{IP}/v1/{u}",data=data,method=m),timeout=60).read()
rd=lambda a,n: req("GET",f"machine:readmem?address={a:04X}&length={n}")
wr=lambda a,d: req("PUT",f"machine:writemem?address={a:04X}&data={bytes(d).hex()}")
t=open(f"{S}/rt.map").read().split("Exports list by value")[1]
sym=lambda n:int(re.search(r"\b"+n+r"\s+([0-9A-F]{6}) ",t).group(1),16)
TX,DL=sym("_text"),sym("_doclen")
def para(j):
    p=b"p%06d "%j
    for k in range(9): p+=b"w%02d "%((j*7+k*3)%100)
    return p+b"\r"
doc=b"".join(para(j) for j in range(1,N+1))
head=b"PLM3"+len(doc).to_bytes(4,"little")
def checksum(d):
    s=0
    for b in d: s=(((s<<1)|(s>>15))&0xFFFF)^b
    return s
def boot(mount):
    req("PUT","machine:reset"); time.sleep(3)
    if mount: req("POST","drives/a:mount?type=d64&mode=readwrite",open(f"{D}/t.d64","rb").read())
    wr(0x02AB,[0,0]); req("POST","runners:run_crt",open(f"{S}/rt.crt","rb").read())
    time.sleep(10)
    while rd(0x02AB,2)==b"\0\0": time.sleep(3)
    time.sleep(2)
boot(True)
wr(0x038D,b"OUT\0"); wr(0x0277,[0x85]); wr(0xC6,[1])
time.sleep(len(doc)/BPS+30)                      # ecriture : pas de lecture memoire
t_dc=time.time(); req("POST","runners:run_prg",open(f"{S}/diskcheck.prg","rb").read()); time.sleep(len(doc)/BPS+30)
r=rd(0xC000,6); ln=r[0]|r[1]<<8; sm=r[2]|r[3]<<8
exp=head+doc
ok1=(ln==len(exp)&0xFFFF and sm==checksum(exp))      # diskcheck compte sur 16 bits
print("fichier: longueur(16b)",ln,"attendu",len(exp)&0xFFFF,"somme %04X"%sm,"attendue %04X"%checksum(exp),"OK" if ok1 else "FAUX",flush=True)
boot(False)
wr(0x038D,b"OUT\0"); wr(0x0277,[0xA0]); wr(0xC6,[1])
time.sleep(len(doc)/BPS+30)
st=rd(0x02C0,11); wb=st[0]|st[1]<<8|st[2]<<16; sl=st[3]|st[4]<<8|st[5]<<16
dl=rd(DL,2); dl=dl[0]|dl[1]<<8
win=rd(TX,dl)
ok2=(dl>0 and wb+dl+sl==len(doc) and bytes(win)==doc[wb:wb+dl])
print("rechargement: total",wb+dl+sl,"attendu",len(doc),"doclen",dl,"fenetre","OK" if ok2 else "FAUX",flush=True)
print("EREDMENY","OK" if ok1 and ok2 else "HIBA")
