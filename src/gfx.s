; Plume : routines graphiques (ecran bitmap et tampon d'impression).
;
; Ecran : banque VIC 3, bitmap en $E000 (sous la ROM KERNAL), couleurs en $CC00.
; Les routines qui lisent le bitmap coupent la ROM KERNAL le temps de l'acces.
        .setcpu "6502"
        .importzp ptr1, ptr2, tmp1, tmp2, tmp3, tmp4
        .import _font_width, _font_bits
        .export _gfx_init, _draw_glyph, _glyph_width, _fsmall
        .export _bm_fill, _bm_or
        .export _gx, _gch, _gstyle, _gbase

STYLE_BOLD      = $01
STYLE_ITALIC    = $02
STYLE_UNDERLINE = $04

        .bss
_gx:     .res 2          ; x en pixels depuis _gbase
_gch:    .res 1          ; code interne du caractere
_gstyle: .res 1
_gbase:  .res 2          ; adresse de la rangee de cartes (ecran : $E000 + ligne * 320)
shift:   .res 1
shvec:   .res 2
width:   .res 1
umask:   .res 1
_fsmall: .res 1          ; 1 : kis betukeszlet (a felkover nem tolti ki az 1 pixeles reseket)
row:     .res 1

        .code
; Bitmap, couleurs noir sur gris clair, banque VIC 3.
_gfx_init:
        lda $DD02
        ora #$03
        sta $DD02
        lda $DD00
        and #$FC                ; banque 3 : $C000-$FFFF
        sta $DD00
        lda #$38                ; ecran $CC00, bitmap $E000
        sta $D018
        lda #$3B                ; mode bitmap, 25 lignes
        sta $D011
        lda #$C8
        sta $D016
        lda #$0F
        sta $D020
        ldx #0
@c:     lda #$0F                ; noir (0) sur gris clair (15)
        sta $CC00,x
        sta $CD00,x
        sta $CE00,x
        sta $CF00,x
        inx
        bne @c
        lda #<$E000
        ldx #>$E000
        jsr setptr
        lda #<8000
        ldx #>8000
        ldy #0
        jmp fill

; Index de police dans A -> ptr2 = glyphe, renvoie la largeur dans A.
glyph_index:
        sec
        sbc #32                 ; table continue : code 32 = index 0
        tax
        lda #0
        sta ptr2+1
        txa
        asl
        rol ptr2+1
        asl
        rol ptr2+1
        asl
        rol ptr2+1
        clc
        adc #<_font_bits
        sta ptr2
        lda ptr2+1
        adc #>_font_bits
        sta ptr2+1
        lda _font_width,x
        rts

; unsigned char glyph_width(unsigned char code) : largeur avec le style courant.
_glyph_width:
        jsr glyph_index
        ldx _gstyle
        cpx #0
        beq @done
        sta tmp1
        txa
        and #STYLE_BOLD
        beq @nb
        inc tmp1
@nb:    lda tmp1
@done:  ldx #0
        rts

; Dessine _gch en (_gx, _gbase) avec _gstyle, en OU sur le contenu.
; Texte sans style : chemin direct. Decalage : saut dans une suite deroulee
; de "lsr a / ror tmp3". Le second octet n'est ecrit que s'il est non nul.
_draw_glyph:
        lda _gch
        jsr glyph_index
        sta width
        lda _gstyle
        beq dg_nostyle
        and #STYLE_BOLD
        beq dg_w
        inc width
dg_w:     lda #0                  ; masque du soulignement : 'width' bits a gauche
        ldx width
dg_m:     sec
        ror
        dex
        bne dg_m
        sta umask
dg_nostyle:
        lda _gx
        and #7
        sta shift
        eor #7                  ; entree = shtab + (7 - decalage) * 3
        sta tmp1
        asl
        adc tmp1
        adc #<shtab
        sta shvec
        lda #>shtab
        adc #0
        sta shvec+1
        lda _gx                 ; ptr1 = _gbase + (_gx / 8) * 8
        and #$F8
        clc
        adc _gbase
        sta ptr1
        lda _gx+1
        adc _gbase+1
        sta ptr1+1
        php
        sei
        lda $01
        pha
        lda #$35
        sta $01
        ldy #0
dg_row:   lda (ptr2),y
        ldx _gstyle
        beq dg_styled
        tax                     ; styles : italique, gras, souligne
        lda _gstyle
        and #STYLE_ITALIC
        beq dg_ni
        cpy #4
        bcs dg_ni
        txa
        lsr
        tax
dg_ni:    lda _gstyle
        and #STYLE_BOLD
        beq dg_nb
        stx tmp1
        txa
        lsr                     ; a sor 1 pixellel jobbra kenve
        ldx _fsmall
        beq dg_nsm
        sta tmp2                ; kis betu: ahol a kenes jobb szomszedja be van allitva, ott res marad
        lda tmp1
        asl
        eor #$FF
        and tmp2
dg_nsm: ora tmp1
        tax
dg_nb:    lda _gstyle
        and #STYLE_UNDERLINE
        beq dg_nu
        cpy #7
        bne dg_nu
        txa
        ora umask
        tax
dg_nu:    txa
dg_styled:
        ldx shift
        beq dg_one
        ldx #0
        stx tmp3
        jmp (shvec)
shdone: ora (ptr1),y
        sta (ptr1),y
        lda tmp3
        beq dg_next
        sty tmp4
        tya
        ora #8
        tay
        lda tmp3
        ora (ptr1),y
        sta (ptr1),y
        ldy tmp4
dg_next:  iny
        cpy #8
        bne dg_row
        pla
        sta $01
        plp
        rts
dg_one:   ora (ptr1),y
        sta (ptr1),y
        jmp dg_next

; 7 decalages d'un bit vers la droite ; le debordement passe dans tmp3.
shtab:  lsr a
        ror tmp3
        lsr a
        ror tmp3
        lsr a
        ror tmp3
        lsr a
        ror tmp3
        lsr a
        ror tmp3
        lsr a
        ror tmp3
        lsr a
        ror tmp3
        jmp shdone

; Parametres communs : adresse (A/X), longueur sur la pile C via les appels ci-dessous.
setptr: sta ptr1
        stx ptr1+1
        rts

; Remplit ptr1 sur (A/X) octets avec Y : pages entieres, puis le reste.
fill:   sta tmp1
        stx tmp2
        tya
        php
        sei
        ldx $01
        stx tmp4
        ldx #$35
        stx $01
        ldx tmp2
        ldy #0
        cpx #0
        beq @rest
@page:  sta (ptr1),y
        iny
        bne @page
        inc ptr1+1
        dex
        bne @page
@rest:  ldy tmp1
        beq @done
@r:     dey
        sta (ptr1),y
        cpy #0
        bne @r
@done:  ldx tmp4
        stx $01
        plp
        rts

        .import popax, popa
; void bm_fill(unsigned addr, unsigned len, unsigned char value)
_bm_fill:
        pha                     ; valeur
        jsr popax               ; longueur
        sta tmp4
        stx ptr2
        jsr popax               ; adresse
        jsr setptr
        pla
        tay
        lda tmp4
        ldx ptr2
        jmp fill

; void bm_xormask(unsigned addr, unsigned char mask) : inverse les points de
; 'mask' sur les 8 octets d'une carte (selection au pixel pres).
        .export _bm_xormask
_bm_xormask:
        sta tmp1
        jsr popax
        jsr setptr
        php
        sei
        lda $01
        pha
        lda #$35
        sta $01
        ldy #7
@x:     lda (ptr1),y
        eor tmp1
        sta (ptr1),y
        dey
        bpl @x
        pla
        sta $01
        plp
        rts

; void bm_or(unsigned addr, unsigned char value) : ajoute des points a un octet.
_bm_or:
        sta tmp1
        jsr popax
        jsr setptr
        php
        sei
        lda $01
        pha
        lda #$35
        sta $01
        ldy #0
        lda (ptr1),y
        ora tmp1
        sta (ptr1),y
        pla
        sta $01
        plp
        rts

; ------------------------------------------------------------------
; unsigned next_line_asm(void) : debut de la ligne suivante.
; Entree : _nl_pos (position), _nl_style (style). Utilise _text, _doclen,
; _wrap_width et la table _font_width. Sortie : position dans A/X,
; style en vigueur a cette position dans _nl_style.
        .import _text, _doclen, _wrap_width
        .importzp ptr3, ptr4, sreg
        .export _next_line_asm, _next_line_buf, _nl_pos, _nl_style, _nl_base, _nl_len

        .bss
_nl_pos:   .res 2
_nl_base:  .res 2           ; a tordelt szoveg kezdete (_text, vagy mas puffer)
_nl_len:   .res 2           ; hossza
_nl_style: .res 1
nl_end:    .res 2           ; fin du texte
nl_space:  .res 2           ; dernier espace (adresse), $FFFF = aucun
nl_spst:   .res 1           ; style au dernier espace
nl_start:  .res 2

        .code
_next_line_asm:
        lda #<_text             ; az ablak szovege
        sta _nl_base
        lda #>_text
        sta _nl_base+1
        lda _doclen
        sta _nl_len
        lda _doclen+1
        sta _nl_len+1
_next_line_buf:                 ; _nl_base / _nl_len altal megadott puffer (a sorszam-index hasznalja)
        clc                     ; ptr1 = base + pos
        lda _nl_base
        adc _nl_pos
        sta ptr1
        sta nl_start
        lda _nl_base+1
        adc _nl_pos+1
        sta ptr1+1
        sta nl_start+1
        clc                     ; nl_end = base + len
        lda _nl_base
        adc _nl_len
        sta nl_end
        lda _nl_base+1
        adc _nl_len+1
        sta nl_end+1
        lda #0
        sta ptr4                ; x = 0
        sta ptr4+1
        lda #$FF
        sta gp_pcol
        sta nl_space+1
        ldy #0
@loop:  lda ptr1                ; fin du texte ?
        cmp nl_end
        lda ptr1+1
        sbc nl_end+1
        bcc @more
        jmp @eof
@more:  lda (ptr1),y
        sta mk_c
        jsr mk_dec
        bcc @notmk
        sta _nl_style           ; marqueur : l'etat complet
        jmp @next
@notmk: ldx mk_c
@char:  cpx #13
        bne @width
        jsr @inc                ; retour a la ligne : la ligne suivante commence apres
        jmp @retptr
@width: stx gp_c                ; couleur : un mot d'une autre couleur commence sur une limite de carte (l'espace s'allonge)
        lda _nl_style
        sta gp_st
        lda ptr4
        sta gp_x
        lda ptr4+1
        sta gp_x+1
        jsr gp_core
        lda gp_x
        sta ptr4
        lda gp_x+1
        sta ptr4+1
        ldx gp_c
        lda _font_width-32,x    ; largeur + gras
        sta tmp1
        lda _nl_style
        and #1
        clc
        adc tmp1
        clc                     ; sreg = x + largeur
        adc ptr4
        sta sreg
        lda ptr4+1
        adc #0
        sta sreg+1
        lda _wrap_width         ; trop large si wrap_width < x + largeur
        cmp sreg
        lda _wrap_width+1
        sbc sreg+1
        bcs @fits
        lda nl_space+1          ; coupure au dernier espace s'il y en a un
        cmp #$FF
        beq @nospace
        lda nl_space
        sta ptr1
        lda nl_space+1
        sta ptr1+1
        lda nl_spst
        sta _nl_style
        jsr @inc
        jmp @retptr
@nospace:
        lda ptr1                ; mot plus long que la ligne : coupe ici
        cmp nl_start
        bne @retptr
        lda ptr1+1
        cmp nl_start+1
        bne @retptr
        jsr @inc                ; au moins un caractere par ligne
        jmp @retptr
@fits:  cpx #' '
        bne @adv
        lda ptr1
        sta nl_space
        lda ptr1+1
        sta nl_space+1
        lda _nl_style
        sta nl_spst
@adv:   lda sreg
        sta ptr4
        lda sreg+1
        sta ptr4+1
@next:  jsr @inc
        jmp @loop
@eof:   lda _nl_len
        ldx _nl_len+1
        rts
@retptr:
        sec                     ; position = ptr1 - base
        lda ptr1
        sbc _nl_base
        pha
        lda ptr1+1
        sbc _nl_base+1
        tax
        pla
        rts
@inc:   inc ptr1
        bne :+
        inc ptr1+1
:       rts

; Couleur : une carte (8 points) n'a qu'une couleur d'encre. Un mot d'une autre couleur
; que le precedent, apres un espace, commence sur la limite de carte suivante si sa
; premiere lettre tomberait dans la carte de la derniere lettre du mot precedent.
; Entree : gp_x (x), gp_c (code), gp_st (etat). Sortie : gp_x (x ajuste). Ne touche pas Y.
        .export _gp_adv, _gp_reset, _nopad, _cur_pad
        .bss
gp_x:    .res 2
gp_c:    .res 1
gp_st:   .res 1
gp_col:  .res 1
gp_pcol: .res 1         ; couleur de la derniere lettre, $FF = aucune
gp_cell: .res 1         ; carte de son dernier point
gp_sp:   .res 1         ; le caractere precedent etait un espace
gp_t:    .res 2
_nopad:  .res 1         ; 1 : pas d'allongement (impression)
_cur_pad: .res 1        ; allongement avant le caractere sous le curseur
        .code
        .rodata
gp_weak: .byte '.', ',', ';', ':', '!', '?', $27, $22, ')', '-'
        .code
gp_core:
        lda _nopad
        beq :+
        rts
:       lda gp_c
        cmp #' '
        bcs :+
        rts                     ; vezerlokod (igazitas-jel) : nulla szelesseg, semmi sem valtozik
:       cmp #' '
        bne :+
        lda #1
        sta gp_sp
        rts
:
        lda gp_st
        lsr
        lsr
        lsr
        sta gp_col
        lda gp_sp
        beq @note
        lda gp_pcol
        cmp #$FF
        beq @note
        cmp gp_col
        beq @note
        lda gp_x                ; carte de x
        lsr
        lsr
        lsr
        ldx gp_x+1
        beq :+
        ora #$20
:       cmp gp_cell
        bne @note
        lda gp_x                ; x = (x | 7) + 1
        ora #7
        clc
        adc #1
        sta gp_x
        bcc @note
        inc gp_x+1
@note:  lda gp_sp               ; irasjel kozvetlenul a betu utan : a betu szinet orokli (a rest a kovetkezo szo elott szamoljuk)
        bne @own
        lda gp_pcol
        cmp #$FF
        beq @own
        ldx #9
:       lda gp_c
        cmp gp_weak,x
        beq @inh
        dex
        bpl :-
        bmi @own
@inh:   lda gp_pcol
        sta gp_col
@own:   lda gp_col
        sta gp_pcol
        ldx gp_c
        lda _font_width-32,x
        sta gp_t
        lda gp_st
        and #STYLE_BOLD
        clc
        adc gp_t
        clc
        adc gp_x                ; dernier point = x + largeur - 1
        sta gp_t
        lda gp_x+1
        adc #0
        sta gp_t+1
        lda gp_t
        bne :+
        dec gp_t+1
:       dec gp_t
        lda gp_t
        lsr
        lsr
        lsr
        ldx gp_t+1
        beq :+
        ora #$20
:       sta gp_cell
        lda #0
        sta gp_sp
        rts

; Bemeneti reteszek : a fajlvalaszto hatterbeli (lassu, FTP-s) lepesei alatt a kattintast (a tuz lenyomasa, bit 4) es az egergorgo impulzusait
; (pontosan 4 = fel, 8 = le a port 1-en, mint a poll_mouse-ban ; a 0x1F zaj kimarad) az IRQ elkapja (CIA1 port 1 : aktiv alacsony), a lepes utan a main.c ujrajatsza.
; _inp_clk : a lenyomas-elek szama (legfeljebb 3) ; _inp_latch bitek : 0x40 gorgo fel, 0x80 gorgo le. cc65 interruptor : carry torolve = az IRQ lancban a tobbi is lefut.
        .export _inp_latch, _inp_clk
        .interruptor _inp_irq
        .bss
_inp_latch: .res 1
_inp_clk:   .res 1
inp_prev:   .res 1
inp_v:      .res 1
        .code
_inp_irq:
        lda $DC01
        eor #$FF
        and #$1F
        sta inp_v
        lda inp_v
        cmp #$1F
        beq @done               ; zaj
        tax
        and #$10
        tay                     ; Y = a tuz most
        eor inp_prev
        and #$10
        beq @nf                 ; a tuz nem valtozott
        tya
        beq @nf                 ; felengedes
        lda _inp_clk
        cmp #3
        bcs @nf
        inc _inp_clk            ; lenyomas-el
@nf:    sty inp_prev
        cpx #4
        bne @n4
        lda _inp_latch
        ora #$40
        sta _inp_latch
@n4:    cpx #8
        bne @done
        lda _inp_latch
        ora #$80
        sta _inp_latch
@done:  clc
        rts

; unsigned gp_adv(unsigned x, unsigned char c, unsigned char st) : x ajuste pour ce caractere.
_gp_adv:
        sta gp_st
        jsr popa
        sta gp_c
        jsr popax
        sta gp_x
        stx gp_x+1
        jsr gp_core
        lda gp_x
        ldx gp_x+1
        rts

_gp_reset:
        lda #$FF
        sta gp_pcol
        rts

; Marqueurs de style, de couleur et d'inverse video (voir main.c). Un marqueur donne tout l'etat :
; bits 0-2 style, 3-5 couleur, 6 = couleur explicite, 7 = inverse (alors seul le gras reste).
;   $10-$17 : style, sans couleur ; $C0-$FF : couleur 0-7 et style ;
;   inverse : 18 codes de rvs_tab (index = (0 sans couleur, 1-8 couleur) * 2 + gras).
        .export _is_mark, _mark_state, _mark_of
        .rodata
rvs_tab: .byte $18, $19, $1A, $1B, $1C, $1D, $1E, $1F, $01, $02, $03, $04, $05, $06, $07, $08, $0E, $0F
        .bss
mk_t:   .res 1
mk_u:   .res 1
mk_c:   .res 1
        .code
; A = octet. Sortie : C = 1 et A = etat si marqueur ; C = 0 sinon. Ne touche pas Y.
mk_dec:
        cmp #$C0
        bcc @lo
        and #$3F
        ora #$40
        sec
        rts
@lo:    cmp #$20
        bcs @no
        sta mk_t
        and #$F8
        cmp #$10
        bne @r
        lda mk_t
        and #7
        sec
        rts
@r:     lda mk_t
        ldx #17
@s:     cmp rvs_tab,x
        beq @f
        dex
        bpl @s
@no:    clc
        rts
@f:     txa
        and #1
        sta mk_t
        txa
        lsr
        beq @d
        sec
        sbc #1
        asl
        asl
        asl
        ora #$40
        ora mk_t
        ora #$80
        sec
        rts
@d:     lda mk_t
        ora #$80
        sec
        rts
_is_mark:
        jsr mk_dec
        lda #0
        rol
        ldx #0
        rts
_mark_state:
        jsr mk_dec
        ldx #0
        rts
_mark_of:
        sta mk_t
        and #$80
        bne @rvs
        lda mk_t
        and #$40
        beq @n
        lda mk_t
        and #$3F
        ora #$C0
        ldx #0
        rts
@n:     lda mk_t
        and #7
        ora #$10
        ldx #0
        rts
@rvs:   lda mk_t
        and #$40
        beq @c0
        lda mk_t
        lsr
        lsr
        lsr
        and #7
        clc
        adc #1
@c0:    asl
        sta mk_u
        lda mk_t
        and #1
        ora mk_u
        tax
        lda rvs_tab,x
        ldx #0
        rts

; void bm_move(unsigned dst, unsigned src, unsigned len) : deplace des octets
; du bitmap (ROM masquee), vers le bas ou vers le haut selon les adresses.
        .export _bm_move

_bm_move:
        sta tmp1                ; longueur
        stx tmp2
        jsr popax               ; source
        sta ptr2
        stx ptr2+1
        jsr popax               ; destination
        sta ptr1
        stx ptr1+1
        php
        sei
        lda $01
        pha
        lda #$35
        sta $01
        lda ptr1+1              ; destination avant la source : copie montante
        cmp ptr2+1
        bcc @up
        bne @down
        lda ptr1
        cmp ptr2
        bcc @up
@down:  clc                     ; copie descendante : on part de la fin
        lda ptr1+1
        adc tmp2
        sta ptr1+1
        clc
        lda ptr2+1
        adc tmp2
        sta ptr2+1
        ldy tmp1
        beq @dpages
@dtail: dey
        lda (ptr2),y
        sta (ptr1),y
        cpy #0
        bne @dtail
@dpages:
        ldx tmp2
        beq @done
@dpage: dec ptr1+1
        dec ptr2+1
        ldy #0
@dbyte: dey
        lda (ptr2),y
        sta (ptr1),y
        cpy #0
        bne @dbyte
        dex
        bne @dpage
        beq @done
@up:    ldy #0
        ldx tmp2
        beq @utail
@upage: lda (ptr2),y
        sta (ptr1),y
        iny
        bne @upage
        inc ptr1+1
        inc ptr2+1
        dex
        bne @upage
@utail: ldx tmp1
        beq @done
@ubyte: lda (ptr2),y
        sta (ptr1),y
        iny
        dex
        bne @ubyte
@done:  pla
        sta $01
        plp
        rts

; unsigned char joy_read(void) : directions et tir des joysticks des ports 1
; et 2 (bits 0 haut, 1 bas, 2 gauche, 3 droite, 4 tir, a 1 si actif). Les
; colonnes du clavier sont desactivees pendant la lecture, pour qu'une touche
; enfoncee ne passe pas pour le joystick du port 1. _joy_p1 garde le port 1
; seul : il partage ses fils avec le clavier.
        .export _joy_read, _joy_p1
        .bss
_joy_p1: .res 1
        .code
.proc _joy_read
        php
        sei
        lda #$FF
        sta $DC00
.ifdef JOYTEST                  ; essais dans VICE : joystick simule en $02B0/$02B1
        lda $02B1               ; (actif a 1)
.else
        lda $DC01               ; port 1
        eor #$FF
.endif
        and #$1F
        sta _joy_p1
.ifdef JOYTEST
        lda $02B0
.else
        lda $DC00               ; port 2
.endif
        ldx #$7F                ; valeur laissee par le KERNAL
        stx $DC00
        plp
.ifndef JOYTEST
        eor #$FF
.endif
        and #$1F
        ora _joy_p1
        ldx #0
        rts
.endproc

; Curseur bloc inverse : recopie le caractere sous le curseur (8 lignes de
; _cur_addr, decalees de _cur_sh points, largeur _cur_w) dans le sprite 1
; ($C040) et un bloc plein de meme largeur dans le sprite 2 ($CBC0).
        .export _cur_sample, _cur_addr, _cur_sh, _cur_st, _cur_ptr, _cur_n
        .bss
_cur_addr: .res 2
_cur_sh:   .res 1
_cur_st:   .res 1       ; style avant le curseur
_cur_ptr:  .res 2       ; text + curseur
_cur_n:    .res 2       ; octets restants (doclen - curseur)
cur_w:     .res 1
cm0:       .res 1
cm1:       .res 1
cv0:       .res 1
cv1:       .res 1
cv2:       .res 1
        .code
_cur_sample:
        lda #7                  ; fin de ligne ou de texte : bloc de 7 points
        sta cur_w
        lda _cur_ptr
        sta ptr1
        lda _cur_ptr+1
        sta ptr1+1
        ldy #0
@w:     lda _cur_n              ; saute les marqueurs de style ($10-$17)
        ora _cur_n+1
        beq @wd
        lda (ptr1),y
        sta mk_c
        jsr mk_dec
        bcc @ch0
        sta _cur_st
        inc ptr1
        bne :+
        inc ptr1+1
:       lda _cur_n
        bne :+
        dec _cur_n+1
:       dec _cur_n
        jmp @w
@ch0:   ldx mk_c
@ch:    cpx #13
        beq @wd
        lda _font_width-32,x    ; largeur, +1 en gras
        sta cur_w
        lda _cur_st
        and #STYLE_BOLD
        clc
        adc cur_w
        adc _cur_pad            ; bloc large : sur l'espace allonge aussi
        sta cur_w
@wd:    lda #0
        sta cm0
        sta cm1
        ldx cur_w
@m:     sec                     ; masque : _cur_w bits a gauche
        ror cm0
        ror cm1
        dex
        bne @m
        lda _cur_addr
        sta ptr1
        lda _cur_addr+1
        sta ptr1+1
        sei
        lda #$34                ; bitmap sous la ROM KERNAL
        sta $01
        ldx #0
        ldy #0
@row:   lda (ptr1),y            ; carte, carte+8, carte+16
        sta cv0
        tya
        ora #8
        tay
        lda (ptr1),y
        sta cv1
        tya
        eor #$18
        tay
        lda (ptr1),y
        sta cv2
        tya
        and #7
        tay
        lda _cur_sh
        sta tmp1
        beq @put
@sh:    asl cv2
        rol cv1
        rol cv0
        dec tmp1
        bne @sh
@put:   lda cv0
        and cm0
        sta $C040,x
        lda cv1
        and cm1
        sta $C041,x
        lda cm0
        sta $CBC0,x
        lda cm1
        sta $CBC1,x
        inx
        inx
        inx
        iny
        cpy #8
        bne @row
        lda #$36
        sta $01
        cli
        rts

; CTRL+0 : jeu de couleurs suivant. _set_theme applique _theme_no.
        .export _next_theme, _set_theme, _col, _theme_no
        .import _themes
        .bss
_col:      .res 5       ; page, barres, marges, bord, fleche
_theme_no: .res 1
        .code
_next_theme:
        ldx _theme_no
        inx
        cpx #4
        bcc :+
        ldx #0
:       stx _theme_no
_set_theme:
        lda _theme_no
        asl a
        asl a
        adc _theme_no           ; * 5
        tax
        ldy #0
@c:     lda _themes,x
        sta _col,y
        inx
        iny
        cpy #5
        bne @c
        lda _col+3
        sta $D020
        lda _col+4
        sta $D027               ; fleche
        lda _col
        sta $D028               ; lettre du curseur : fond de la page
        lsr a
        lsr a
        lsr a
        lsr a
        sta $D029               ; bloc du curseur : encre de la page
        rts

; Position, extinction et clignotement du curseur.
        .export _cur_place, _cursor_off, _blink_cursor, _cur_row, _cur_px
        .import _cursor_sample, _anchor, _cursor
        .bss
_cur_row:  .res 1       ; rangee de cartes
_cur_px:   .res 2       ; x en points
shown:     .res 1
stale:     .res 1       ; forme a relire avant d'allumer
blink_t:   .res 1
        .code
; Place les sprites 1 et 2 en _cur_row/_cur_px et calcule l'adresse lue par
; _cur_sample. Le curseur s'eteint et repart visible au prochain passage.
_cur_place:
        lda _cur_px
        and #7
        sta _cur_sh
        lda _cur_row            ; adresse : $E000 + rangee * 320 + carte
        sta tmp1
        lda #0
        sta tmp2
        ldx #6
@s:     asl tmp1
        rol tmp2
        dex
        bne @s
        lda _cur_px
        and #$F8
        clc
        adc tmp1
        sta _cur_addr
        lda _cur_px+1
        adc tmp2
        adc _cur_row
        adc #$E0
        sta _cur_addr+1
        lda _cur_px             ; x sprite = x + 24
        clc
        adc #24
        sta $D002
        sta $D004
        lda _cur_px+1
        adc #0
        beq @lo
        lda $D010
        ora #$06
        bne @hx
@lo:    lda $D010
        and #$F9
@hx:    sta $D010
        lda _cur_row            ; y sprite = rangee * 8 + 50
        asl a
        asl a
        asl a
        adc #50
        sta $D003
        sta $D005
        lda #1
        sta stale
        lda $A2
        sta blink_t
_cursor_off:
        lda $D015
        and #$F9
        sta $D015
        lda #0
        sta shown
        rts

; Allume pres d'une demi-seconde, eteint autant (clignote aussi pendant une selection : la barre est entre deux caracteres).
_blink_cursor:
        lda #$80                ; a C64 Ultimate II+ (3.14) egyes REU-olvasasai egy bajtot irnak a $C000 cimre (a nyil elso sora) : a nyil es a homokora elso sora is $80
        sta $C000
        lda $A2
        sec
        sbc blink_t
        and #$20
        bne @off
        lda shown
        bne @done
        lda stale
        beq @on
        jsr _cursor_sample
        lda #0
        sta stale
@on:    lda $D015
        ora #$06
        sta $D015
        lda #1
        sta shown
@done:  rts
@off:   lda shown
        bne _cursor_off
        rts

; RESTORE (NMI) : le vecteur du KERNAL ($0318) pointe sur un RTI, RUN/STOP+RESTORE ne relance plus le BASIC.
        .export _nmi_off, _nmi_hit
        .bss
_nmi_hit: .res 1
        .code
_nmi_off:
        sei
        lda #<@rti
        sta $0318
        sta $FFFA               ; a RAM-vektor is (a KERNAL ROM alatt) : a bitterkepet rajzolo $34 / $35 ablakban a RESTORE nem ugorhat a szemetbe
        lda #>@rti
        sta $0319
        sta $FFFB
        cli
        rts
@rti:   inc _nmi_hit            ; RESTORE = annuler (le programme lit le drapeau)
        rti

; void __fastcall__ rvs_tail(unsigned from, unsigned to) : a sorpuffer ($0400) from..to (a cella vegeig) pontjait 1-re allitja
; (RVS cellaban a bit = papir : az RVS-sav a betu veget koveti, nem a cellahatart)
        .export _rvs_tail
_rvs_tail:
        sta ptr2
        stx ptr2+1
        jsr popax
        sta ptr3
        stx ptr3+1
        lda ptr3+1
        beq @ok
        cmp #1
        bne @rt
        lda ptr3
        cmp #$38
        bcs @rt
@ok:    lda ptr3
        and #7
        beq @rt
        sta tmp1
        lda ptr2
        sec
        sbc ptr3
        sta ptr2
        lda ptr2+1
        sbc ptr3+1
        sta ptr2+1
        bcc @rt
        ora ptr2
        beq @rt
        lda ptr2+1
        bne @full
        lda ptr2
        clc
        adc tmp1
        bcs @full
        cmp #8
        bcc @ee
@full:  lda #8
@ee:    sta tmp2
        lda #$FF
        ldx tmp1
        beq @m1
:       lsr
        dex
        bne :-
@m1:    sta tmp3
        lda #8
        sec
        sbc tmp2
        tax
        lda #$FF
        cpx #0
        beq @m2
:       asl
        dex
        bne :-
@m2:    and tmp3
        sta tmp3
        lda ptr3
        and #$F8
        sta ptr1
        lda ptr3+1
        clc
        adc #4
        sta ptr1+1
        ldy #7
:       lda (ptr1),y
        ora tmp3
        sta (ptr1),y
        dey
        bpl :-
@rt:    rts

; void cur_arrow(void) : a sortores / sorveg kurzorban "ujsor" nyil (a blokk 7 pont szeles, a nyil a sprite 1-ben : $C040, a blokk a sprite 2-ben : $CBC0).
        .export _cur_arrow
        .rodata
cur_arrow_bits: .byte $04, $04, $04, $24, $64, $FC, $60, $20
        .code
_cur_arrow:
        ldx #0
        ldy #0
@l:     lda cur_arrow_bits,y
        sta $C040,x
        lda #0
        sta $C041,x
        sta $CBC1,x
        lda #$FE
        sta $CBC0,x
        inx
        inx
        inx
        iny
        cpy #8
        bne @l
        rts
