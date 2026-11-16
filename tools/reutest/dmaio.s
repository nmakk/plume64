; REU-DMA es IEC iras egymas utan. MODE: 0 = nincs DMA, 1 = DMA minden blokk elott, 2 = DMA, de CLRCHN/CKOUT koruleotte.
; Eredmeny: $C000 = $EE ha kesz, $C001 = befejezett blokkok szama
        .import _reu_fetch, _reu_c64, _reu_addr, _reu_len
        .segment "LOADADDR"
        .word $0801
        .segment "HEADER"
        .word nextline
        .word 10
        .byte $9E, "2061", 0
nextline: .word 0
SETNAM = $FFBD
SETLFS = $FFBA
OPEN   = $FFC0
CLOSE  = $FFC3
CKOUT  = $FFC9
CLRCHN = $FFCC
BSOUT  = $FFD2
BUF    = $4000
        .segment "CODE"
start:  lda #0
        sta $C000
        sta $C001
        lda #8
        ldx #<name
        ldy #>name
        jsr SETNAM
        lda #2
        ldx #8
        ldy #2
        jsr SETLFS
        jsr OPEN
        ldx #2
        jsr CKOUT
next:
.if MODE = 0
.else
.if MODE = 2
        jsr CLRCHN
.endif
        lda #<BUF
        sta _reu_c64
        lda #>BUF
        sta _reu_c64+1
        lda #0
        sta _reu_addr
        lda $C001
        sta _reu_addr+1
        lda #1
        sta _reu_addr+2
        lda #0
        sta _reu_len
        lda #1
        sta _reu_len+1
        jsr _reu_fetch
.if MODE = 2
        ldx #2
        jsr CKOUT
.endif
.endif
        lda #0
        sta $FB
wr:     ldy $FB
        lda BUF,y
        jsr BSOUT
        inc $FB
        bne wr
        inc $C001
        lda $C001
        cmp #100
        bne next
        jsr CLRCHN
        lda #2
        jsr CLOSE
        lda #$EE
        sta $C000
        jmp *
        .segment "RODATA"
name:   .byte "@0:T,S,W"
