#!/usr/bin/env python3
"""RTF-tesztfajlok + referencia: rtf_gen.py <kimeneti-konyvtar>
Kiirja: small.rtf (kis, sokfele szintaxis), big.rtf (nagy), es a vart belso kodolast (small.int, big.int)."""
import sys, os
UC = [0xE0,0xE2,0xE4,0xE7,0xE9,0xE8,0xEA,0xEB,0xEE,0xEF,0xF4,0xF6,0xF9,0xFB,0xFC,
      0xC0,0xC2,0xC7,0xC9,0xC8,0xCA,0xCB,0xCE,0xCF,0xD4,0xD9,0xDB,0xDC,
      0x153,0x152,0xAB,0xBB,0xB0,0x20AC,0xE6,0xF8,0xE5,0xC6,0xD8,0xC5,
      0xE1,0xC1,0xED,0xCD,0xF3,0xD3,0xFA,0xDA,0x151,0x150,0x171,0x170,0xD6]
HI = [0x20AC,'?',',','f','"',0x2026,'+','+','^','%','S','<',0x152,'?','Z','?',
      '?',0x2018,0x2019,0x201C,0x201D,'*',0x2013,0x2014,'~','T','s','>',0x153,'?','z','Y']
def code(u):
    if isinstance(u,str): u=ord(u)
    if u<128: return u if u>=32 else 32
    if u in UC: return 128+UC.index(u)
    if u in (0x2018,0x2019): return ord("'")
    if u in (0x201C,0x201D): return ord('"')
    if u in (0x2013,0x2014): return ord('-')
    if u==0xA0: return 32
    return ord('?')
def ref(data):
    """Ugyanazok a szabalyok, mint a main.c-ben (egyszeru, nem allapotgepes)."""
    out=[];cur=0;emit=0;stack=[];i=0;n=len(data);skip=0;uc=1
    first=False
    def put(c):
        nonlocal skip,emit
        if skip: skip-=1; return
        if cur&8: return
        if (cur&7)!=emit:
            emit=cur&7; out.append(0x10|emit)
        out.append(c)
    def uni(u):
        if u==0x2026: put(46);put(46);u=46
        put(code(u))
    SK=["fonttbl","colortb","stylesh","info","pict","header","footer","object","footnot","listtab","listove"]
    while i<n:
        c=data[i];i+=1
        if c==ord('{'): stack.append(cur); first=True
        elif c==ord('}'):
            if stack: cur=stack.pop()
            first=False
        elif c==ord('\\'):
            c=data[i];i+=1
            if chr(c).isalpha():
                w=chr(c)
                while i<n and chr(data[i]).isalpha():
                    if len(w)<7: w+=chr(data[i])
                    i+=1
                neg=False;has=False;num=0
                if i<n and (data[i]==ord('-') or chr(data[i]).isdigit()):
                    has=True
                    if data[i]==ord('-'): neg=True;i+=1
                    while i<n and chr(data[i]).isdigit(): num=num*10+data[i]-48;i+=1
                if i<n and data[i]==ord(' '): i+=1
                if first:
                    first=False
                    if any(w.startswith(k) for k in SK): cur|=8; continue
                def st(bit):
                    nonlocal cur
                    if has and num==0: cur&=~bit
                    else: cur|=bit
                if w=='b': st(1)
                elif w=='i': st(2)
                elif w=='ul': st(4)
                elif w=='ulnone': cur&=~4
                elif w=='plain': cur&=8
                elif w in('par','line'): put(13)
                elif w=='tab': put(32)
                elif w=='u': uni((-num if neg else num)&0xFFFF); skip=uc
                elif w=='uc': uc=num
            elif c==ord("'"):
                h=int(chr(data[i])+chr(data[i+1]),16);i+=2
                uni(h if h<0x80 else (HI[h&31] if h<0xA0 else h))
            elif c==ord('*'):
                if first: cur|=8
                first=False
            elif c==ord('~'): put(32);first=False
            elif c==ord('_'): put(45);first=False
            elif c in (10,13): put(13);first=False
            elif c in (92,123,125): put(c);first=False
            else: first=False
        elif c>=32:
            first=False
            if c<128: put(c)
            else: uni(HI[c&31])
    return bytes(out)
small=(r"{\rtf1\ansi\ansicpg1252\deff0{\fonttbl{\f0\fnil Times New Roman;}{\f1 Arial;}}"
 r"{\colortbl;\red255\green0\blue0;}{\stylesheet{\s0 Normal;}}{\*\generator Test;}{\info{\title x}}"
 "\r\n"
 r"\pard\plain\f0\fs24 Hello \b bold\b0  and \i italic\i0  and \ul under\ulnone  text.\par"+"\r\n"
 r"Accents: caf\'e9 na\u239?ve \u337?\u369? \u8364? \u233?\u224? {\b nested {\i both}} end\par"+"\r\n"
 r"Braces \{ \} backslash \\ nbsp\~x quote \u8220?q\u8221? dash \u8212? dots \u8230? ok\par"+"\r\n"
 r"{\*\unknown skipped text}{\field{\*\fldinst HYPERLINK}{\fldrslt linked}} tab\tab x\line y\par"+"\r\n}")
big=[r"{\rtf1\ansi\deff0{\fonttbl{\f0 Arial;}}\pard "]
for j in range(1,421):
    big.append(r"Para %d \b word%d\b0  \i it\u233?\i0  \ul un\ulnone  tail text of the paragraph here.\par"%(j,j%97)+"\r\n")
big.append("}")
big="".join(big)
d=sys.argv[1]
os.makedirs(d,exist_ok=True)
for name,txt in (("small",small),("big",big)):
    b=txt.encode("latin1")
    open(f"{d}/{name}.rtf","wb").write(b)
    r=ref(b)
    open(f"{d}/{name}.int","wb").write(r)
    print(name,len(b),"->",len(r))
