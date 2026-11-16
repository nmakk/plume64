; reu.s primitivek tesztje. Eredmeny $C000: [detect][roundtrip 16000 B: 1=OK][eltero bajtok szama lo][hi][EE]
        .import _reu_detect, _reu_stash, _reu_fetch, _reu_c64, _reu_addr, _reu_len
        .segment "LOADADDR"
        .word $0801
        .segment "HEADER"
        .word nextline
        .word 10
        .byte $9E, "2061", 0
nextline: .word 0
BUF = $4000
LEN = 16000
        .segment "CODE"
start:  sei
        lda #0
        sta $C004
        jsr _reu_detect
        sta $C000
        ; minta: BUF[i] = (i lo) xor (i hi) xor $5A
        lda #<BUF
        sta $FB
        lda #>BUF
        sta $FC
        lda #<LEN
        sta $FD
        lda #>LEN
        sta $FE
        ldy #0
        sty $02
        sty $03
fill:   lda $02
        eor $03
        eor #$5A
        sta ($FB),y
        inc $02
        bne :+
        inc $03
:       inc $FB
        bne :+
        inc $FC
:       lda $FD
        bne :+
        dec $FE
:       dec $FD
        lda $FD
        ora $FE
        bne fill
        ; stash 16000 bajt a REU $01F000 cimre (atlepi a bank-hatart)
        lda #<BUF
        sta _reu_c64
        lda #>BUF
        sta _reu_c64+1
        lda #$00
        sta _reu_addr
        lda #$F0
        sta _reu_addr+1
        lda #$01
        sta _reu_addr+2
        lda #<LEN
        sta _reu_len
        lda #>LEN
        sta _reu_len+1
        jsr _reu_stash
        ; torles
        lda #<BUF
        sta $FB
        lda #>BUF
        sta $FC
        ldx #>LEN+1
        ldy #0
        tya
clr:    sta ($FB),y
        iny
        bne clr
        inc $FC
        dex
        bne clr
        ; fetch
        lda #<BUF
        sta _reu_c64
        lda #>BUF
        sta _reu_c64+1
        lda #$00
        sta _reu_addr
        lda #$F0
        sta _reu_addr+1
        lda #$01
        sta _reu_addr+2
        lda #<LEN
        sta _reu_len
        lda #>LEN
        sta _reu_len+1
        jsr _reu_fetch
        ; ellenorzes
        lda #<BUF
        sta $FB
        lda #>BUF
        sta $FC
        lda #<LEN
        sta $FD
        lda #>LEN
        sta $FE
        lda #0
        sta $02
        sta $03
        sta $C002
        sta $C003
        ldy #0
ver:    lda $02
        eor $03
        eor #$5A
        cmp ($FB),y
        beq ok
        inc $C002
        bne ok
        inc $C003
ok:     inc $02
        bne :+
        inc $03
:       inc $FB
        bne :+
        inc $FC
:       lda $FD
        bne :+
        dec $FE
:       dec $FD
        lda $FD
        ora $FE
        bne ver
        lda $C002
        ora $C003
        bne bad
        lda #1
        .byte $2C
bad:    lda #0
        sta $C001
        lda #$EE
        sta $C004
        jmp *
