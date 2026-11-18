; Ledek szine a tema szerint (csak a REU/Magic Desk kiadas). Ez a kod NEM a
; programban van: a cartridge egy kulon bankjaban (LED_BANK), a Plume gfx.s-beli
; led_theme betoltoje masolja $0400-ra, es onnan fut.
;
; Hogyan: az Ultimate Command Interface HTTP targetje ($06, a 3.15 / C64U 1.2
; firmware-tol) a gep sajat REST API-jat hivja: PUT /v1/configs/<kategoria>/<elem>?value=<ertek>.
; Belepes: A = tema (0..3). Minden hivas kb. egy masodperc.
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

        .segment "CODE"
entry:  sta theme
        php
        sei
        lda $FB
        pha
        lda $FC
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
@u:     lda ($FB),y
        beq @ud
        sta cmdbuf,x
        inx
        iny
        bne @u
@ud:    stx cmdlen
        iny                     ; a lezaro 0 is
        tya
        clc
        adc $FB
        sta $FB
        bcc :+
        inc $FC
:       jsr send
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
        jmp next
done:   pla
        sta $FC
        pla
        sta $FB
        plp
        rts

; X = 0: varakozas idle-re, X = 1: adatallapotra
waitstate:
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
@ok:    rts

send:   ldx #0
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

.define SFIX   "LED%20Strip%20Settings/LedStrip%20Mode?value=Fixed%20Color"
.define SOFF   "LED%20Strip%20Settings/LedStrip%20Mode?value=Off"
.define SWHITE "LED%20Strip%20Settings/Fixed%20Color?value=White"
.define SROYAL "LED%20Strip%20Settings/Fixed%20Color?value=Royal%20Blue"
.define KFIX   "Keyboard%20Lighting/LedStrip%20Mode?value=Fixed%20Color"
.define KOFF   "Keyboard%20Lighting/LedStrip%20Mode?value=Off"
.define KGREEN "Keyboard%20Lighting/Fixed%20Color?value=Green"
.define KAZURE "Keyboard%20Lighting/Fixed%20Color?value=Azure"
.define KAMBER "Keyboard%20Lighting/Fixed%20Color?value=Amber"

tab:    .word t0, t1, t2, t3
t0:     .byte SFIX, 0, SWHITE, 0, KOFF, 0, 0                 ; fekete a feheren
t1:     .byte SOFF, 0, KFIX, 0, KGREEN, 0, 0                 ; zold a feketen
t2:     .byte SFIX, 0, SROYAL, 0, KFIX, 0, KAZURE, 0, 0      ; Commodore-kek
t3:     .byte SOFF, 0, KFIX, 0, KAMBER, 0, 0                 ; sarga a feketen
