; A 8-as egyseg konyvtarat ($) a $C100-tol tarolja; $C000/$C001 = hossz, $C002 = EE ha kesz.
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
CHKIN  = $FFC6
CLRCHN = $FFCC
CHRIN  = $FFCF
READST = $FFB7
        .segment "CODE"
start:  lda #0
        sta $C000
        sta $C001
        sta $C002
        lda #<$C100
        sta $FB
        lda #>$C100
        sta $FC
        lda #1
        ldx #<name
        ldy #>name
        jsr SETNAM
        lda #2
        ldx #8
        ldy #0
        jsr SETLFS
        jsr OPEN
        ldx #2
        jsr CHKIN
loop:   jsr CHRIN
        pha
        jsr READST
        sta $FD
        pla
        ldy #0
        sta ($FB),y
        inc $FB
        bne n1
        inc $FC
n1:     inc $C000
        bne n2
        inc $C001
n2:     lda $FD
        bne fin
        lda $C001
        cmp #3
        bcc loop
fin:    jsr CLRCHN
        lda #2
        jsr CLOSE
        lda #$EE
        sta $C002
        jmp *
        .segment "RODATA"
name:   .byte "$"
