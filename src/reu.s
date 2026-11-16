; Plume : acces au REU (DMA). Edition REU seulement.
;
; Registres $DF00-$DF0A. Les parametres sont dans les variables exportees :
;   reu_c64  adresse en RAM C64 (2 octets)
;   reu_addr adresse REU (3 octets : poids faible, moyen, banque)
;   reu_len  longueur (2 octets ; 0 = 65536)
; Les E/S doivent etre visibles ($01 bits 0-2) pendant l'appel.
        .setcpu "6502"
        .export _reu_detect, _reu_stash, _reu_fetch
        .export _reu_c64, _reu_addr, _reu_len

REU_CMD  = $DF01
REU_C64  = $DF02        ; +0 poids faible, +1 poids fort
REU_ADDR = $DF04        ; +0, +1, +2 (banque)
REU_LEN  = $DF07        ; +0, +1
REU_IRQ  = $DF09
REU_CTRL = $DF0A
CMD_STASH = $90         ; execute, pas de declenchement par $FF00, C64 -> REU
CMD_FETCH = $91         ; REU -> C64

        .bss
_reu_c64:  .res 2
_reu_addr: .res 3
_reu_len:  .res 2
probe:     .res 2

        .code
setup:
        lda _reu_c64
        sta REU_C64
        lda _reu_c64+1
        sta REU_C64+1
        lda _reu_addr
        sta REU_ADDR
        lda _reu_addr+1
        sta REU_ADDR+1
        lda _reu_addr+2
        sta REU_ADDR+2
        lda _reu_len
        sta REU_LEN
        lda _reu_len+1
        sta REU_LEN+1
        lda #0
        sta REU_IRQ
        sta REU_CTRL            ; les deux adresses s'incrementent
        rts

; void reu_stash(void) : RAM C64 -> REU
_reu_stash:
        jsr setup
        lda #CMD_STASH
        sta REU_CMD
        rts

; void reu_fetch(void) : REU -> RAM C64
_reu_fetch:
        jsr setup
        lda #CMD_FETCH
        sta REU_CMD
        rts

; unsigned char reu_detect(void) : 1 si un REU repond. Epreuve reelle : deux
; octets partent dans le REU (adresse 0) et reviennent ; une cartouche a RAM
; en $DF00 (EasyFlash) ou un bus libre ne ferait pas revenir le motif.
_reu_detect:
        lda #<probe
        sta _reu_c64
        lda #>probe
        sta _reu_c64+1
        lda #0
        sta _reu_addr
        sta _reu_addr+1
        sta _reu_addr+2
        sta _reu_len+1
        lda #2
        sta _reu_len
        lda #$A5
        sta probe
        lda #$5A
        sta probe+1
        jsr _reu_stash
        lda #0
        sta probe
        sta probe+1
        jsr _reu_fetch
        ldx #0
        lda probe
        cmp #$A5
        bne no
        lda probe+1
        cmp #$5A
        bne no
        lda #1
        rts
no:     lda #0
        rts
