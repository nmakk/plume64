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
        .byte $FB, $8D, $9D, $8C, $89, $8A, $8B, $91
        .byte $2B, $D7, $C1, $21, $DA, $D3, $C5, $01
        .byte $25, $D2, $C4, $2F, $C3, $C6, $D4, $D8
        .byte $3D, $D9, $C7, $28, $C2, $C8, $D5, $D6
        .byte $29, $C9, $CA, $EE, $CD, $CB, $CF, $CE
        .byte $F1, $D0, $CC, $ED, $3E, $EA, $EF, $3C
        .byte $2D, $F0, $F2, $94, $01, $EB, $EC, $3F
        .byte $27, $3F, $04, $22, $A0, $02, $D1, $83
        .byte $FF
hun_cbm:
        .byte $94, $8D, $9D, $8C, $89, $8A, $8B, $91
        .byte $23, $B3, $B0, $24, $AD, $AE, $B1, $01
        .byte $25, $B2, $AC, $26, $BC, $BB, $A3, $BD
        .byte $27, $B7, $A5, $28, $BF, $B4, $B8, $BE
        .byte $29, $A2, $B5, $30, $A7, $A1, $B9, $AA
        .byte $2B, $AF, $B6, $2D, $3E, $3A, $40, $3C
        .byte $5C, $2A, $3B, $93, $01, $3D, $5E, $3F
        .byte $21, $5F, $04, $22, $FA, $02, $AB, $83
        .byte $FF
hun_ctrl:
        .byte $FF, $FF, $FF, $FF, $FF, $FF, $FF, $FF
        .byte $1C, $17, $01, $9F, $1A, $13, $05, $FF
        .byte $9C, $12, $04, $1E, $03, $06, $14, $18
        .byte $1F, $19, $07, $9E, $02, $08, $15, $16
        .byte $12, $09, $0A, $FF, $0D, $0B, $0F, $0E
        .byte $FF, $10, $0C, $FF, $FF, $1B, $00, $FF
        .byte $FF, $FF, $1D, $FF, $FF, $1F, $1E, $FF
        .byte $90, $06, $FF, $05, $FF, $FF, $11, $FF
        .byte $FF
hun_cs:
        .byte $FF, $FF, $FF, $FF, $FF, $FF, $FF, $FF
        .byte $FF, $FF, $FF, $FF, $FF, $FF, $FF, $FF
        .byte $FF, $FF, $FF, $FF, $FF, $FF, $FF, $FF
        .byte $FF, $FF, $FF, $FF, $FF, $FF, $FF, $FF
        .byte $FF, $FF, $FF, $FF, $FF, $FF, $FF, $FF
        .byte $FF, $FF, $FF, $DC, $FF, $5B, $FF, $FF
        .byte $FF, $FF, $5D, $FF, $FF, $FF, $FF, $FF
        .byte $FF, $FF, $FF, $FF, $FF, $FF, $FF, $FF
        .byte $FF
c64_shift:
        .byte $FB, $8D, $9D, $8C, $89, $8A, $8B, $91
        .byte $23, $D7, $C1, $24, $DA, $D3, $C5, $01
        .byte $25, $D2, $C4, $26, $C3, $C6, $D4, $D8
        .byte $27, $D9, $C7, $28, $C2, $C8, $D5, $D6
        .byte $29, $C9, $CA, $30, $CD, $CB, $CF, $CE
        .byte $DB, $D0, $CC, $DD, $3E, $5B, $BA, $3C
        .byte $A9, $C0, $5D, $94, $01, $3D, $DE, $3F
        .byte $21, $5F, $04, $22, $A0, $02, $D1, $83
        .byte $FF
hun_ptab:
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
