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
        php                     ; a REU a C64 buszan olvas : a bitmap ($E000-) felett a KERNAL ROM latszana, ezert a DMA idejere a ROM-ok el vannak rejtve ($01 = $35)
        sei
        lda $01
        pha
        lda #$35
        sta $01
        lda #CMD_STASH
        sta REU_CMD
        pla
        sta $01
        plp
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

; Az SD-fajlkezelo (src/sd.s) belepesei a $C700-on (led_theme(7) masolja oda indulaskor).
        .export _sd_probe, _sd_open, _sd_close, _sd_read, _sd_write, _sd_dirbeg, _sd_dirnext
        .export _sd_delete, _sd_chdir, _sd_getpath, _sd_size, _sd_loadreu, _sd_savereu, _sd_abort
_sd_probe   = $C700
_sd_open    = $C703
_sd_close   = $C706
_sd_read    = $C709
_sd_write   = $C70C
_sd_dirbeg  = $C70F
_sd_dirnext = $C712
_sd_delete  = $C718
_sd_chdir   = $C71B
_sd_getpath = $C71E
_sd_abort   = $C721
_sd_size    = $C724
_sd_loadreu = $C727
_sd_savereu = $C72A

; led_theme(theme) : couleur des leds selon le theme. Le code (src/led.s) est dans la
; banque 62 de la cartouche Magic Desk. ROML ($8000) masque la RAM pendant la copie,
; et ce code peut etre au-dessus de $8000 : la copie se fait donc depuis un petit
; chargeur recopie dans le tampon cassette ($0334) ; il copie 4 pages en $0400
; (ecran texte inutilise), coupe la cartouche et saute dans le code.
        .export _led_theme
        .code
_led_theme:
        pha
        ldx #ld_end - ld - 1
@i:     lda ld_img,x
        sta $0334,x
        dex
        bpl @i
        pla
        ldx #62
        cmp #8
        bcc @b
        ldx #63                 ; A >= 8 : a nyomtato-bank (src/led.s -D PR)
@b:     stx ld_bank+1
        jmp $0334
ld_img:
        .org $0334
ld:     pha
        php
        sei
        lda $01
        pha
        lda #$37
        sta $01
ld_bank: lda #62
        sta $DE00
        ldx #0
@c:     lda $8000,x
        sta $0400,x
        lda $8100,x
        sta $0500,x
        lda $8200,x
        sta $0600,x
        inx
        bne @c
@d:     lda $8300,x            ; a 4. oldalnak csak az elso $A0 bajtja ($07A0-$07F7 a csere-szoveg : nem irjuk at)
        sta $0700,x
        inx
        cpx #$A0
        bne @d
        lda #$80
        sta $DE00
        pla
        sta $01
        plp
        pla
        jmp $0400
ld_end:
        .reloc

; void __fastcall__ font_load(unsigned char small) : la police (font_width + font_bits, 1422 octets) est
; recopiee de la banque 59 (normale) ou 60 (petite) sur la table du programme. Meme technique qu'ovl_load.
        .import _font_width
        .export _font_load
FONT_BANK0 = 59
_font_load:
        clc
        adc #FONT_BANK0
        ldx #fnt_end - fnt - 1
        pha
@i:     lda fnt_img,x
        sta $0334,x
        dex
        bpl @i
        pla
        jmp $0334
fnt_img:
        .org $0334
fnt:    sta OVL_TMP
        php
        sei
        lda $01
        pha
        lda #$37
        sta $01
        lda OVL_TMP
        sta $DE00
        lda #$00
        sta $FB
        lda #$80
        sta $FC
        lda #<_font_width
        sta $FD
        lda #>_font_width
        sta $FE
        ldx #5
        ldy #0
@p:     lda ($FB),y
        sta ($FD),y
        iny
        bne @p
        inc $FC
        inc $FE
        dex
        bne @p
@q:     lda ($FB),y             ; les 142 octets restants (1422 = 5 * 256 + 142)
        sta ($FD),y
        iny
        cpy #142
        bne @q
        lda #$80
        sta $DE00
        pla
        sta $01
        plp
        rts
fnt_end:
        .reloc

; Overlays du programme : l'overlay n (1...) est dans la banque OVL_BANK0 + n de la
; cartouche, a l'adresse $8000 ; il est recopie dans la zone __OVERLAYSTART__ (taille
; __OVERLAYSIZE__, voir plume-reu.cfg). Meme technique que le code des leds : un petit
; chargeur dans le tampon cassette, car ROML ($8000) n'est visible qu'avec la cartouche active.
; void __fastcall__ ovl_load(unsigned char n)
        .import __OVERLAYSTART__, __OVERLAYSIZE__
        .export _ovl_load, _ovl_cur
OVL_BANK0 = 48
OVL_TMP   = $03BA

        .bss
_ovl_cur: .res 1                ; overlay present (0 : aucun)
ovl_cur = _ovl_cur
        .code
_ovl_load:
        cmp ovl_cur
        beq @ret
        sta ovl_cur
        clc
        adc #OVL_BANK0
        cmp #OVL_BANK0 + 11     ; les overlays 11... sont dans les banques 40... (49-58 : 1-10)
        bcc :+
        sbc #19
:       ldx #ovl_end - ovl - 1
        pha                     ; banque
@i:     lda ovl_img,x
        sta $0334,x
        dex
        bpl @i
        pla
        jmp $0334
@ret:   rts
ovl_img:
        .org $0334
ovl:    sta OVL_TMP
        php
        sei
        lda $01
        pha
        lda #$37
        sta $01
        lda OVL_TMP
        sta $DE00
        lda #$00
        sta $FB
        lda #$80
        sta $FC
        lda #<__OVERLAYSTART__
        sta $FD
        lda #>__OVERLAYSTART__
        sta $FE
        ldx #>__OVERLAYSIZE__
        ldy #0
@p:     lda ($FB),y
        sta ($FD),y
        iny
        bne @p
        inc $FC
        inc $FE
        dex
        bne @p
        lda #$80
        sta $DE00
        pla
        sta $01
        plp
        rts
ovl_end:
        .reloc
