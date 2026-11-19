; GENERE par tools/mkhunkeys.py : ne pas modifier a la main.
; Clavier HUN : tables de decodage du KERNAL (vecteur $028F), voir le generateur.
        .setcpu "6502"
        .export _hun_set
        .rodata
hun_plain:
        .byte $14, $0D, $1D, $88, $85, $86, $87, $11
        .byte $33, $57, $41, $34, $5A, $53, $45, $01
        .byte $35, $52, $44, $36, $43, $46, $54, $58
        .byte $37, $59, $47, $38, $42, $48, $55, $56
        .byte $39, $49, $4A, $E4, $4D, $4B, $4F, $4E
        .byte $E7, $50, $4C, $E3, $2E, $E0, $E5, $2C
        .byte $09, $E6, $E8, $13, $01, $E1, $E2, $2F
        .byte $31, $30, $04, $32, $20, $02, $51, $03
        .byte $FF
hun_shift:
        .byte $94, $8D, $9D, $8C, $89, $8A, $8B, $91
        .byte $2B, $D7, $C1, $21, $DA, $D3, $C5, $01
        .byte $25, $D2, $C4, $2F, $C3, $C6, $D4, $D8
        .byte $3D, $D9, $C7, $28, $C2, $C8, $D5, $D6
        .byte $29, $C9, $CA, $EE, $CD, $CB, $CF, $CE
        .byte $F1, $D0, $CC, $ED, $3E, $EA, $EF, $3C
        .byte $2D, $F0, $F2, $93, $01, $EB, $EC, $3F
        .byte $27, $3F, $04, $22, $A0, $02, $D1, $83
        .byte $FF
hun_cbm:
        .byte $94, $8D, $9D, $8C, $89, $8A, $8B, $91
        .byte $23, $B3, $B0, $24, $AD, $AE, $B1, $01
        .byte $98, $B2, $AC, $26, $BC, $BB, $A3, $BD
        .byte $9A, $B7, $A5, $9B, $BF, $B4, $B8, $BE
        .byte $29, $A2, $B5, $30, $A7, $A1, $B9, $AA
        .byte $A6, $AF, $B6, $DC, $3E, $3A, $40, $3C
        .byte $A8, $2A, $3B, $93, $01, $3D, $DE, $3F
        .byte $81, $5F, $04, $95, $FA, $02, $AB, $83
        .byte $FF
hun_ctrl:
        .byte $FF, $FF, $FF, $FF, $FF, $FF, $FF, $FF
        .byte $1C, $17, $01, $9F, $1A, $13, $05, $FF
        .byte $9C, $12, $04, $1E, $03, $06, $14, $18
        .byte $1F, $19, $07, $9E, $02, $08, $15, $16
        .byte $12, $09, $0A, $FF, $0D, $0B, $0F, $0E
        .byte $FF, $10, $0C, $FF, $FF, $1B, $00, $FF
        .byte $FF, $FF, $1D, $FF, $FF, $1F, $1E, $FF
        .byte $90, $92, $FF, $05, $FF, $FF, $11, $FF
        .byte $FF
hun_ptab:
        .word hun_plain, hun_shift, hun_cbm, hun_ctrl

        .code
; Meme choix de table que le KERNAL ($EB48), puis retour a sa routine de decodage ($EAE0).
hun_decode:
        lda $028D
        asl
        cmp #8
        bcc @s
        lda #6
@s:     tax
        lda hun_ptab,x
        sta $F5
        lda hun_ptab+1,x
        sta $F6
        jmp $EAE0

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
@off:   lda #$48
        sta $028F
        lda #$EB
        sta $0290
@done:  plp
        rts
