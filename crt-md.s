; Cartouche Magic Desk de Plume (edition REU).
;
; Pourquoi pas EasyFlash : la RAM d'E/S2 de l'EasyFlash ($DF00-$DFFF) masque les
; registres du REU, meme cartouche coupee (mesure sur C64 Ultimate). Magic Desk
; n'a pas de RAM en $DF00 : le REU reste accessible.
;
; Banque 0 : ROML porte le demarrage CBM80. Banques 1 et suivantes (ROML, 8 Ko
; chacune) : le programme, sans son adresse de chargement. Il est recopie en
; $0801 puis lance a son point d'entree. Bank = $DE00 (bits 0-5), bit 7 = cartouche coupee.
; PRG_PAGES vient de la ligne de commande.
        .setcpu "6502"

MD_BANK    = $DE00
MD_OFF     = $80
FIRST_BANK = 1
LOAD       = $0801
ENTRY      = $080D        ; apres la ligne BASIC "SYS 2061" de cc65
RAM_CODE   = $0334        ; tampon cassette, libre
SRC        = $FB
DST        = $FD
PAGE       = $02

        .segment "ROML"
        .word cold, cold
        .byte $C3, $C2, $CD, $38, $30   ; signature CBM80
cold:   sei
        stx $D016
        jsr $FDA3               ; IOINIT
        jsr $FD50               ; RAMTAS
        jsr $FD15               ; RESTOR
        jsr $FF5B               ; CINT
        ldx #loader_end - loader - 1
@c:     lda loader_image,x
        sta RAM_CODE,x
        dex
        bpl @c
        jmp RAM_CODE

loader_image:
        .org RAM_CODE
loader:
        lda #0
        sta PAGE
        lda #<LOAD
        sta DST
        lda #>LOAD
        sta DST+1
@page:  lda PAGE                ; banque = 1 + page / 32, adresse = $8000 + (page % 32) * 256
        lsr
        lsr
        lsr
        lsr
        lsr
        clc
        adc #FIRST_BANK
        sta MD_BANK
        lda PAGE
        and #$1F
        ora #$80
        sta SRC+1
        lda #0
        sta SRC
        ldy #0
@byte:  lda (SRC),y
        sta (DST),y
        iny
        bne @byte
        inc DST+1
        inc PAGE
        lda PAGE
        cmp #PRG_PAGES
        bne @page
        lda #MD_OFF             ; la cartouche disparait
        sta MD_BANK
        lda #$37
        sta $01
        ldx #$FF
        txs
        cli
        jmp ENTRY
loader_end:
.assert loader_end <= $03FC, error, "chargeur trop grand"
        .reloc
