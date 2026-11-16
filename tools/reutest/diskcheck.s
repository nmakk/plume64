; Lemezolvaso ellenorzo: a 8-as egyseg "OUT" SEQ fajljat vegigolvassa.
; Eredmeny $C000: [hossz lo][hossz hi][osszeg lo][osszeg hi][statusz][EE]
; osszeg: s = rol16(s) ^ bajt. Ha az OPEN nem sikerul: statusz = $FF.
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
OUT    = $C000

        .segment "CODE"
start:  lda #0
        sta OUT
        sta OUT+1
        sta OUT+2
        sta OUT+3
        sta OUT+5
        lda #3
        ldx #<name
        ldy #>name
        jsr SETNAM
        lda #2
        ldx #8
        ldy #2
        jsr SETLFS
        jsr OPEN
        bcs fail
        ldx #2
        jsr CHKIN
        bcs fail
loop:   jsr CHRIN
        pha
        jsr READST
        sta OUT+4
        pla
        ; osszeg
        tax
        asl OUT+2
        rol OUT+3
        bcc nc
        inc OUT+2
nc:     txa
        eor OUT+2
        sta OUT+2
        inc OUT
        bne nl
        inc OUT+1
nl:     lda OUT+4
        beq loop
fin:    jsr CLRCHN
        lda #2
        jsr CLOSE
        lda #$EE
        sta OUT+5
        jmp *
fail:   lda #$FF
        sta OUT+4
        jmp fin
        .segment "RODATA"
name:   .byte "OUT"
