; Ledek szine a tema szerint (csak a REU/Magic Desk kiadas). Ez a kod NEM a
; programban van: a cartridge egy kulon bankjaban (LED_BANK), a Plume gfx.s-beli
; led_theme betoltoje masolja $0400-ra, es onnan fut.
;
; Hogyan: az Ultimate Command Interface HTTP targetje ($06, a 3.15 / C64U 1.2
; firmware-tol) a gep sajat REST API-jat hivja: PUT /v1/configs/<kategoria>/<elem>?value=<ertek>.
; Belepes: A = tema szinei (0..3; a billentyuzet szine, de be/ki nem), 4 = gorgo, 5 = billentyuzet be, 6 = billentyuzet ki. Minden hivas kb. egy masodperc.
        .setcpu "6502"

CTRL  = $DF1C
STAT  = $DF1C
CMD   = $DF1D
RESP  = $DF1E
STATD = $DF1F
cmdbuf = $0334          ; kazettapuffer, 128 bajt
cmdlen = $03B4
firstb = $03B5
gotf   = $03B6
theme  = $03B7
tmo    = $03B8          ; 3 bajt
tmpy   = $03BB
dead   = $03BC          ; 1 : az Ultimate nem valaszolt idoben, a munkamenet hatralevo reszeben nem probalkozunk (a foprogram nullazza indulaskor)

        .segment "CODE"
entry:  cmp #7
        bne :+
        jmp sdcopy              ; A = 7: az SD-fajlkezelo kimasolasa a $C700-ra (src/sd.s)
:       sta theme
        lda $DF1D               ; UCI azonosito: $C9, ha van Ultimate (egyebkent: kilepes)
        cmp #$C9
        beq :+
        rts
:       lda dead
        beq :+
        rts
:
        php
        sei
        lda $FB
        pha
        lda $FC
        pha
        lda $FD
        pha
        lda $FE
        pha
        lda theme
        asl
        tax
        lda tab,x
        sta $FB
        lda tab+1,x
        sta $FC
next:   ldy #0
        lda ($FB),y
        sta $FD
        iny
        lda ($FB),y
        sta $FE
        ora $FD
        bne :+
        jmp done
:       lda #6                  ; FREE_ALL: nem szivarognak a handle-ok
        sta cmdbuf
        lda #$10
        sta cmdbuf+1
        lda #2
        sta cmdlen
        jsr send
        lda #6                  ; HEADER_CREATE, PUT, URL
        sta cmdbuf
        lda #$11
        sta cmdbuf+1
        lda #2
        sta cmdbuf+2
        ldx #3
        ldy #0
@p:     lda prefix,y
        beq @pd
        sta cmdbuf,x
        inx
        iny
        bne @p
@pd:    ldy #0
@u:     lda ($FD),y
        beq @ud
        cmp #6
        bcs @lit
        sty tmpy                ; 1..5: elore rogzitett eleje (tokenek), lasd lent
        asl
        tay
        lda toks-2,y
        sta @tk+1               ; onmodosito: a token cimet beirjuk az olvaso utasitasba
        lda toks-1,y
        sta @tk+2
        ldy #0
@tk:    lda $FFFF,y
        beq @td
        sta cmdbuf,x
        inx
        iny
        bne @tk
@td:    ldy tmpy
        iny
        bne @u
@lit:   sta cmdbuf,x
        inx
        iny
        bne @u
@ud:    stx cmdlen
        jsr send
        lda #6                  ; DO_EXCHANGE_RAW fejlec, nincs torzs
        sta cmdbuf
        lda #$32
        sta cmdbuf+1
        lda firstb
        sta cmdbuf+2
        lda #$FF
        sta cmdbuf+3
        lda #4
        sta cmdlen
        jsr send
        clc
        lda $FB
        adc #2
        sta $FB
        bcs :+
        jmp next
:       inc $FC
        jmp next
done:   pla
        sta $FE
        pla
        sta $FD
        pla
        sta $FC
        pla
        sta $FB
        plp
        rts

sdcopy: php
        sei
        lda $01
        pha
        lda #$37
        sta $01
        lda #61                 ; SD_BANK
        sta $DE00
        ldx #0
@c:     lda $8000,x
        sta $C700,x
        lda $8100,x
        sta $C800,x
        lda $8200,x
        sta $C900,x
        lda $8300,x
        sta $CA00,x
        inx
        bne @c
        lda #$80
        sta $DE00
        pla
        sta $01
        plp
        rts

; X = 0: varakozas idle-re, X = 1: adatallapotra
waitstate:
        lda dead
        bne @ok
        lda #0
        sta tmo
        sta tmo+1
        lda #$60
        sta tmo+2
@l:     lda STAT
        and #$30
        cpx #0
        bne @data
        beq @idle
@data:  and #$20
        bne @ok
        beq @cnt
@idle:  cmp #0
        beq @ok
@cnt:   inc tmo
        bne @l
        inc tmo+1
        bne @l
        dec tmo+2
        bne @l
        lda #1                  ; idotullepes: tobbet nem varunk
        sta dead
@ok:    rts

send:   lda dead
        beq :+
        rts
:       ldx #0
        jsr waitstate
        ldx #0
@w:     lda cmdbuf,x
        sta CMD
        inx
        cpx cmdlen
        bne @w
        lda #1
        sta CTRL
        ldx #1
        jsr waitstate
        lda #0
        sta gotf
@d:     lda STAT
        bpl @s
        lda RESP
        ldx gotf
        bne @d
        sta firstb
        inc gotf
        jmp @d
@s:     lda STAT
        and #$40
        beq @e
        lda STATD
        jmp @s
@e:     lda #2
        sta CTRL
        ldx #0
        jmp waitstate

prefix: .byte "127.0.0.1/v1/configs/", 0

.ifdef PR
; Nyomtato-bank (63) : ugyanez a kod, masik tablaval. A = 8 : szines grafikus nyomtatas (Epson + PNG COLOR),
; 9 : szines szoveg (PNG COLOR + Commodore MPS), 10 : vissza (PNG B&W + Commodore MPS).
tk1:    .byte "Printer%20Settings/Output%20type?value=", 0
tk2:    .byte "Printer%20Settings/Emulation?value=", 0
toks:   .word tk1, tk2
vcol:   .byte 1, "PNG%20COLOR", 0
vbw:    .byte 1, "PNG%20B%26W", 0
vepson: .byte 2, "Epson%20FX-80%2FJX-80", 0
vcbm:   .byte 2, "Commodore%20MPS", 0
tab:    .word z0, z0, z0, z0, z0, z0, z0, z0, t8, t9, t10
z0:     .word 0
t8:     .word vepson, vcol, 0
t9:     .word vcol, vcbm, 0
t10:    .word vbw, vcbm, 0
.else
; Az elemek elso bajtja egy token (1..5), ami a kategoria elejet jelenti: igy nem ismetlodnek a hosszu utvonalak.
tk1:    .byte "Keyboard%20Lighting/Fixed%20Color?value=", 0
tk2:    .byte "Keyboard%20Lighting/LedStrip%20Mode?value=", 0
tk3:    .byte "Keyboard%20Lighting/Color%20tint?value=", 0
tk4:    .byte "LED%20Strip%20Settings/Fixed%20Color?value=", 0
tk5:    .byte "LED%20Strip%20Settings/LedStrip%20Mode?value=", 0
toks:   .word tk1, tk2, tk3, tk4, tk5

sfix:   .byte 5, "Fixed%20Color", 0
soff:   .byte 5, "Off", 0
swhite: .byte 4, "White", 0
sroyal: .byte 4, "Royal%20Blue", 0
kfix:   .byte 2, "Fixed%20Color", 0
koff:   .byte 2, "Off", 0
kchart: .byte 1, "Chartreuse", 0
kindigo: .byte 1, "Indigo", 0
kamber: .byte 1, "Amber", 0
kwhite: .byte 1, "White", 0
ktpure: .byte 3, "Pure", 0
ktbright: .byte 3, "Bright", 0
swheel: .byte "U64%20Specific%20Settings/Mouse%20Wheel%20Sensitivity?value=1", 0

; temankent a muveletek listaja (szavak), 0 zarja
tab:    .word t0, t1, t2, t3, t4, t5, t6
t0:     .word sfix, swhite, kwhite, ktpure, 0                ; fekete a feheren (a billentyuzet szine: fehér)
t1:     .word soff, kchart, ktbright, 0                        ; zold a feketen
t2:     .word sfix, sroyal, kindigo, ktbright, 0                ; Commodore-kek
t3:     .word soff, kamber, ktpure, 0                        ; sarga a feketen
t4:     .word swheel, 0                              ; indulaskor: gorgo-erzekenyseg
t5:     .word kfix, 0                                ; billentyuzet be (van mentetlen valtozas)
t6:     .word koff, 0                                ; billentyuzet ki (minden mentve)
.endif
