import subprocess, sys, time, urllib.request
IP="192.168.0.158"; S=sys.argv[1]
def rd(a,n): return urllib.request.urlopen(f"http://{IP}/v1/machine:readmem?address={a:04X}&length={n}",timeout=15).read()
def wr(a,d): urllib.request.urlopen(urllib.request.Request(f"http://{IP}/v1/machine:writemem?address={a:04X}&data={bytes(d).hex()}",method="PUT"),timeout=15).read()
def press(key):
    while rd(0xC6,1)[0]: time.sleep(0.005)
    wr(0x0277,[key]); wr(0xC6,[1])
    while rd(0xC6,1)[0]: pass
    wr(0x02A7,[2])
    while rd(0x02A7,1)[0]!=3: pass
    wr(0x02A7,[0])
def verify(tag):
    r=subprocess.run(["python3","-I",f"{S}/reuverify.py",f"{S}/rt.map"],capture_output=True,text=True)
    print(tag, r.stdout.strip().replace("\n","\n   "), flush=True); return r.returncode==0
urllib.request.urlopen(urllib.request.Request(f"http://{IP}/v1/runners:run_crt",data=open(f"{S}/rt.crt","rb").read(),method="POST"),timeout=30).read()
time.sleep(25)
ok=True
for name,key,n in (("LEFELE",0x90,int(sys.argv[2])),("FELFELE",0x05,int(sys.argv[3]))):
    for i in range(1,n+1):
        press(key)
        if i%30==0: ok&=verify(f"{name} {i}")
    ok&=verify(f"{name} vege")
print("EREDMENY:", "MINDEN RENDBEN" if ok else "HIBA")
