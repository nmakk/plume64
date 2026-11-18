; UCI Network target teszt. Eredmeny $C000-tol:
; [id] $90 ident-adat $FF statusz $FE  $91 ipaddr-adat $FF statusz $FE
; majd gazdankent: $A0+n, (open) adat $FF st $FE, (write) .., (read) .., (close) .. ; vegen $EE
        .segment "LOADADDR"
        .word $0801
        .segment "HEADER"
        .word nextline
        .word 10
        .byte $9E, "2061", 0
nextline: .word 0

CTRL  = $DF1C
STAT  = $DF1C
CMD   = $DF1D
ID    = $DF1D
RESP  = $DF1E
STATD = $DF1F
OUT   = $C000
ptr   = $FB
src   = $FD

        .segment "BSS"
cmdbuf: .res 250
cmdlen: .res 1
firstb: .res 1
gotf:   .res 1
hnd:    .res 1
idx:    .res 1
tmo:    .res 3

        .segment "RODATA"
tab:    .word urlA, urlB
urlA:   .byte "127.0.0.1/v1/configs/LED%20Strip%20Settings/Color%20tint?value=Pastel", 0
urlB:   .byte "192.168.0.158/v1/configs/LED%20Strip%20Settings/Color%20tint?value=Whisper", 0

        .segment "CODE"
start:
        sei
        lda #<OUT
        sta ptr
        lda #>OUT
        sta ptr+1
        lda ID
        jsr put
        lda #$90                ; HTTP IDENTIFY
        jsr put
        lda #6
        sta cmdbuf
        lda #1
        sta cmdbuf+1
        lda #2
        sta cmdlen
        jsr send
        lda #6                  ; FREE_ALL
        sta cmdbuf
        lda #$10
        sta cmdbuf+1
        lda #2
        sta cmdlen
        jsr send
        lda #0
        sta idx
host:   lda idx
        clc
        adc #$A0
        jsr put
        lda idx                 ; HEADER_CREATE PUT url
        asl
        tax
        lda tab,x
        sta src
        lda tab+1,x
        sta src+1
        lda #6
        sta cmdbuf
        lda #$11
        sta cmdbuf+1
        lda #2
        sta cmdbuf+2
        ldx #3
        jsr copystr
        stx cmdlen
        jsr send
        lda firstb
        sta hnd
        lda #6                  ; DO_EXCHANGE_RAW hnd, $FF
        sta cmdbuf
        lda #$32
        sta cmdbuf+1
        lda hnd
        sta cmdbuf+2
        lda #$FF
        sta cmdbuf+3
        lda #4
        sta cmdlen
        jsr send
        inc idx
        lda idx
        cmp #2
        beq done
        jmp host
done:   lda #$EE
        jsr put
        jmp *

copystr:
        ldy #0
@l:     lda (src),y
        beq @d
        sta cmdbuf,x
        inx
        iny
        bne @l
@d:     rts

delay:  ldx #0
        ldy #0
@l:     inx
        bne @l
        iny
        bne @l
        inc tmo
        lda tmo
        and #$07
        bne @l
        rts

put:    ldy #0
        sta (ptr),y
        inc ptr
        bne :+
        inc ptr+1
:       rts

; X = 0: varakozas idle-re, X = 1: adatallapotra (STATE bit5)
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
        bne @n
        sta firstb
        inc gotf
@n:     jsr put
        jmp @d
@s:     lda #$FF
        jsr put
@s2:    lda STAT
        and #$40
        beq @e
        lda STATD
        jsr put
        jmp @s2
@e:     lda #$FE
        jsr put
        lda #2
        sta CTRL
        ldx #0
        jsr waitstate
        rts
