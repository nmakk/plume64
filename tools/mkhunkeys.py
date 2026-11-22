"""Genere src/hunkey.s : les quatre tables de decodage du clavier (normal, Shift, C=, CTRL) du mode HUN.
Base : les tables du KERNAL du C64 (65 touches, ordre de la matrice) ; on y change les touches hongroises.
Codes des lettres hongroises : 0xE0 + indice de hu_font (src/main.c)."""
H = lambda s: bytes.fromhex(s)
PLAIN = H("140d1d8885868711335741345a5345013552443643465458375947384248555639494a304d4b4f4e2b504c2d2e3a402c5c2a3b13013d5e2f315f043220025103ff")
SHIFT = H("948d9d8c898a8b9123d7c124dad3c50125d2c426c3c6d4d827d9c728c2c8d5d629c9ca30cdcbcfcedbd0ccdd3e5bba3ca9c05d93013dde3f215f0422a002d183ff")
CBM   = H("948d9d8c898a8b9196b3b097adaeb10198b2ac99bcbba3bd9ab7a59bbfb4b8be29a2b530a7a1b9aaa6afb6dc3e5ba43ca8df5d93013dde3f815f0495a002ab83ff")
CTRL  = H("ffffffffffffffff1c17019f1a1305ff9c12041e030614181f19079e0208151612090a920d0b0f0eff100cffff1b00ff1cff1dffff1f1eff9006ff05ffff11ffff")
for t in (PLAIN, SHIFT, CBM, CTRL): assert len(t) == 65, len(t)
P, S, C, K = map(bytearray, (PLAIN, SHIFT, CBM, CTRL))

# indices de la matrice (voir le tableau du KERNAL)
LEFTARROW, K0, PLUS, MINUS, POUND, UPARROW = 57, 35, 40, 43, 48, 54
AT, STAR, COLON, SEMI, EQUAL = 46, 49, 45, 50, 53
HOME, SPACE = 51, 60
D1, D2, D3, D4, D5, D6, D7, D8, D9 = 56, 59, 8, 11, 16, 19, 24, 27, 32
# lettres hongroises : minuscule / majuscule
LOW = dict(e=0xE0, ue=0xE1, i=0xE2, o=0xE3, oe=0xE4, o2=0xE5, u=0xE6, ue2=0xE7, a=0xE8)
def hu(k, lo, up):
    P[k], S[k] = lo, up
hu(COLON, 0xE0, 0xEA)       # e acute
hu(SEMI,  0xE8, 0xF2)       # a acute
hu(EQUAL, 0xE1, 0xEB)       # u double acute
hu(AT,    0xE5, 0xEF)       # o double acute
hu(STAR,  0xE6, 0xF0)       # u acute
hu(K0,    0xE4, 0xEE)       # o umlaut
hu(PLUS,  0xE7, 0xF1)       # u umlaut
hu(MINUS, 0xE3, 0xED)       # o acute
hu(UPARROW, 0xE2, 0xEC)     # i acute
# zero a la place de la fleche gauche ; Shift : ?
P[LEFTARROW], S[LEFTARROW] = 0x30, 0x3F
# chiffres, Shift : disposition hongroise
S[D1], S[D3], S[D4], S[D6], S[D7] = 0x27, 0x2B, 0x21, 0x2F, 0x3D
# livre : euro (a KERNAL 0x5C kodja, a main.c translate 161 = euro jelet ad) ; Shift : tiret ; a Tab : CTRL+I
P[POUND], S[POUND] = 0x5C, 0x2D
# CLR/HOME : debut / fin de ligne ; Shift : suppression vers l'avant
# 0x94 (mint a Shift+INST/DEL) : a KERNAL ugyanugy ismetli, mint a backspace-et (0x14) ;
# a 0x7F egy leutesre sok karaktert torolne, a 0x93 pedig nem ismetlodne
S[HOME] = 0x94
S[0] = 0xFB                 # Shift + INST/DEL : insert / overwrite
# C= : symboles deplaces (le tiret bas : C= + Shift + -)
C[COLON], C[SEMI], C[STAR], C[AT] = 0x3A, 0x3B, 0x2A, 0x40
# C= : a C64 billentyuk eredeti jelei (a szamoknal a rajtuk levo Shift-jel), hogy minden elerheto legyen
for k, ch in zip((D1, D2, D3, D4, D5, D6, D7, D8, D9), b"!\"#$%&'()"): C[k] = ch
C[K0], C[PLUS], C[MINUS], C[EQUAL] = 0x30, 0x2B, 0x2D, 0x3D
C[UPARROW], C[POUND] = 0x5E, 0x5C
# Shift + C= : a KERNAL nem kulonbozteti meg, ezert sajat tabla; csak C= + Shift + - ad alahuzast (0xDC)
CS = bytearray([0xFF] * 65)
CS[MINUS] = 0xDC
CS[COLON], CS[SEMI] = 0x5B, 0x5D      # C= + Shift + e acute / a acute : [ ]
C[LEFTARROW] = 0x5F                  # C= + <- : la fleche gauche du C64
C[SPACE] = 0xFA             # change de clavier
# CTRL + fleche gauche = themes (0x06, comme dans le KERNAL) ; la touche "0" (o umlaut) n'a pas de CTRL
K[K0] = 0xFF
K[POUND] = 0xFF

def rows(name, t):
    s = f"{name}:\n"
    for i in range(0, 65, 8):
        s += "        .byte " + ", ".join(f"${b:02X}" for b in t[i:i + 8]) + "\n"
    return s
S64 = bytearray(SHIFT)       # C64 mod : a KERNAL tablaja, csak a Shift+CLR/HOME tér el
S64[HOME] = 0x94
S64[0] = 0xFB
out = """; GENERE par tools/mkhunkeys.py : ne pas modifier a la main.
; Clavier HUN : tables de decodage du KERNAL (vecteur $028F), voir le generateur.
        .setcpu "6502"
        .export _hun_set
        .rodata
""" + rows("hun_plain", P) + rows("hun_shift", S) + rows("hun_cbm", C) + rows("hun_ctrl", K) + rows("hun_cs", CS) + rows("c64_shift", S64) + """hun_ptab:
        .word hun_plain, hun_shift, hun_cbm, hun_ctrl

        .code
; Meme choix de table que le KERNAL ($EB48), puis retour a sa routine de decodage ($EAE0).
hun_decode:
        lda $028D
        cmp #3                  ; Shift + C=
        bne @n
        lda #<hun_cs
        sta $F5
        lda #>hun_cs
        sta $F6
        jmp $EAE0
@n:     asl
        cmp #8
        bcc @s
        lda #6
@s:     tax
        lda hun_ptab,x
        sta $F5
        lda hun_ptab+1,x
        sta $F6
        jmp $EAE0

; C64 mod : a KERNAL sajat tablai, kiveve a Shift+CLR/HOME (ismetlodo elore torles).
c64_decode:
        lda $028D
        cmp #1
        bne @def
        lda #<c64_shift
        sta $F5
        lda #>c64_shift
        sta $F6
        jmp $EAE0
@def:   jmp $EB48

; void __fastcall__ hun_set(unsigned char on) : 1 = clavier HUN, 0 = clavier du KERNAL
_hun_set:
        php
        sei
        tax
        beq @off
        lda #<hun_decode
        sta $028F
        lda #>hun_decode
        sta $0290
        bne @done
@off:   lda #<c64_decode
        sta $028F
        lda #>c64_decode
        sta $0290
@done:  plp
        rts
"""
open("src/hunkey.s", "w").write(out)
print("src/hunkey.s")
