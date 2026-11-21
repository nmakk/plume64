; Fajlkezelo a C64 Ultimate UCI "Ultimate DOS" celjan (SD-kartya / USB), a Plume REU kiadasahoz.
; A kod egy cartridge-bankban van (SD_BANK), indulaskor a $C700-ra masolodik (a sorok tablazata utan, a 2. sprite ($CBC0) elott), es onnan ugrotablan hivhato: ez nincs az overlay-teruleten, igy barmelyik overlayből hívható.
; A valtozok a $CB00-tol vannak. Minden belepes: A = hibakod (0 = rendben), X = 0.
        .setcpu "6502"

CTRL  = $DF1C
STAT  = $DF1C
CMD   = $DF1D
RESP  = $DF1E
STATD = $DF1F

V       = $CB00
p_path  = V             ; 64  : a konyvtar utvonala (nullaval zarva, legfeljebb 63 karakter)
p_name  = V+64          ; 64  : fajlnev (nyers bajtok, UTF-8, legfeljebb 63 karakter)
p_ptr   = V+128         ; 2   : adatcim (olvasas / iras)
p_len   = V+130         ; 2   : hossz / tenylegesen atvitt bajtok
p_size  = V+132         ; 4   : fajlmeret (info / size)
p_att   = V+136         ; 1   : konyvtarbejegyzes attributuma
p_err   = V+137         ; 1   : utolso hibakod ($FF: idotullepes)
p_reu   = V+138         ; 4   : REU-cim (loadreu / savereu)
p_rlen  = V+142         ; 4   : hossz (loadreu / savereu)

        .segment "BLOB"
        jmp do_probe            ; +0
        jmp do_open             ; +3   A = 1 (olvasas) vagy $0E (iras, felulirja)
        jmp do_close            ; +6
        jmp do_read             ; +9   p_ptr, p_len
        jmp do_write            ; +12  p_ptr, p_len
        jmp do_dirbeg           ; +15
        jmp do_dirnext          ; +18  A = 0: bejegyzes az ent-ben (ent+0 attr, ent+1 nev), 1: vege
        jmp do_probe            ; +21  (fenntartva)
        jmp do_delete           ; +24  p_name
        jmp do_chdir            ; +27  p_path
        jmp do_getpath          ; +30  -> p_path
        jmp do_abort            ; +33  a folyamatban levo konyvtarolvasas megszakitasa
        jmp do_size             ; +36  a megnyitott fajl merete -> p_size
        jmp do_loadreu          ; +39  megnyitott fajl -> REU (p_reu, p_rlen)
        jmp do_savereu          ; +42  REU -> megnyitott fajl
        jmp do_probe            ; +45  (fenntartva; a hatralevo bejegyzeseket do_dirnext hivogatasa dobja el)

; ---- alacsony szint
w_idle: lda #0
        sta tmo
        sta tmo+1
        lda #$30
        sta tmo+2
@l:     lda STAT
        and #$31
        beq @ok
        inc tmo
        bne @l
        inc tmo+1
        bne @l
        dec tmo+2
        bne @l
        lda #$FF
        sta p_err
@ok:    rts

; vár, amíg az allapot nem "foglalt" ; vissza: A = allapot ($00, $20, $30)
w_nbusy:
        lda #0
        sta tmo
        sta tmo+1
        lda #$30
        sta tmo+2
@l:     lda STAT
        and #$30
        cmp #$10
        bne @ok
        inc tmo
        bne @l
        inc tmo+1
        bne @l
        dec tmo+2
        bne @l
        lda #$FF
        sta p_err
        lda #0
@ok:    rts

; egy valaszcsomag fogadasa: adat -> rxs cime (rxm bajtig), allapotszoveg -> sbuf ; vissza: A = allapot ($30: van meg csomag)
recv_pkt:
        jsr w_nbusy
        sta st
L_dd:   lda STAT
        bpl L_ds
        lda RESP
        ldx dn+1
        cpx rxm+1
        bcc L_st
        bne L_sk
        ldx dn
        cpx rxm
        bcs L_sk
L_st:   sta $FFFF
rxs = *-2
        inc rxs
        bne L_sk
        inc rxs+1
L_sk:   inc dn
        bne L_dd
        inc dn+1
        jmp L_dd
L_ds:   lda STAT
        and #$40
        beq L_dn
        lda STATD
        ldx sn
        cpx #14
        bcs L_ds
        sta sbuf,x
        inc sn
        jmp L_ds
L_dn:   lda st
        rts

; a teljes valasz fogadasa (a csomagok elfogadasaval)
recv:   lda #0
        sta sn
        sta dn
        sta dn+1
L_pk:   jsr recv_pkt
        pha
        lda #2
        sta CTRL
        pla
        cmp #$30
        beq L_pk
        jsr w_idle
        lda sn
        beq L_z
        lda sbuf
        and #$0F
        asl
        sta st
        asl
        asl
        clc
        adc st
        sta st
        lda sbuf+1
        and #$0F
        clc
        adc st
        ldx p_err
        cpx #$FF
        beq L_r
        sta p_err
        rts
L_z:    lda p_err
        cmp #$FF
        beq L_r
        lda #0
        sta p_err
L_r:    rts

; felbehagyott atvitel (pl. megszakitott konyvtarolvasas) eldobasa: UCI abort
ab:     lda STAT
        and #$B1
        beq @r
        lda #4
        sta CTRL
@r:     rts

; parancs kuldese cmdbuf-bol (cmdlen bajt) + fogadas
send:   lda #0
        sta p_err
        jsr ab
        jsr w_idle
        ldx #0
@c:     lda cmdbuf,x
        sta CMD
        inx
        cpx cmdlen
        bne @c
        lda #1
        sta CTRL
        jmp recv

; fogadas az ent pufferbe (legfeljebb 46 bajt)
rx_ent: lda #<ent
        sta rxs
        lda #>ent
        sta rxs+1
        lda #80
        sta rxm
        lda #0
        sta rxm+1
        rts

; cmdbuf: 1, c ; X = 2
cmd2:   sta cmdbuf+1
        lda #1
        sta cmdbuf
        ldx #2
        rts

app_path:
        ldy #0
@l:     lda p_path,y
        beq @d
        sta cmdbuf,x
        inx
        iny
        cpy #64
        bne @l
@d:     rts

app_name:
        ldy #0
@l:     lda p_name,y
        beq @d
        sta cmdbuf,x
        inx
        iny
        cpy #64
        bne @l
@d:     rts

fin:    stx cmdlen
        jsr rx_ent
        jmp send

ret:    lda p_err
        ldx #0
        rts

; ---- belepesek
do_probe:
        lda CMD
        cmp #$C9
        bne @n
        lda #1
        .byte $2C
@n:     lda #0
        ldx #0
        rts

chdir:  lda #$11
        jsr cmd2
        jsr app_path
        jmp fin

do_chdir:
        jsr chdir
        jmp ret

do_open:
        pha
        lda p_path
        beq @np
        jsr chdir
@np:    lda #2
        jsr cmd2
        pla
        sta cmdbuf+2
        inx
        jsr app_name
        jsr fin
        jmp ret

do_close:
        lda #3
        jsr cmd2
        jsr fin
        jmp ret

do_read:
        lda #4
        jsr cmd2
        lda p_len
        sta cmdbuf+2
        lda p_len+1
        sta cmdbuf+3
        ldx #4
        stx cmdlen
        lda p_ptr
        sta rxs
        lda p_ptr+1
        sta rxs+1
        lda p_len
        sta rxm
        lda p_len+1
        sta rxm+1
        jsr send
        lda dn
        sta p_len
        lda dn+1
        sta p_len+1
        jmp ret

; iras: legfeljebb 256 bajtos darabokban (WRITE_DATA: 1,5,0,0 + adat)
do_write:
        lda p_ptr
        sta wsrc
        lda p_ptr+1
        sta wsrc+1
        lda p_len
        sta wrem
        lda p_len+1
        sta wrem+1
        lda #0
        sta p_err
L_ch:   lda wrem
        ora wrem+1
        beq L_done
        lda #0
        ldx wrem+1
        bne L_go                ; 256 vagy tobb hatra: 256 bajt (wn = 0)
        lda wrem
L_go:   sta wn
        jsr w_idle
        lda #1
        sta CMD
        lda #5
        sta CMD
        lda #0
        sta CMD
        sta CMD
        tay
L_by:   lda $FFFF,y
wsrc = *-2
        sta CMD
        iny
        cpy wn
        bne L_by
        lda #1
        sta CTRL
        jsr rx_ent
        lda #0
        sta rxm
        jsr recv
        lda p_err
        bne L_done
        lda wn
        bne L_pt
        inc wsrc+1
        dec wrem+1
        jmp L_ch
L_pt:   clc
        adc wsrc
        sta wsrc
        bcc L_nc
        inc wsrc+1
L_nc:   lda #0
        sta wrem
        jmp L_ch
L_done: jmp ret

do_dirbeg:
        lda p_path
        beq @np
        jsr chdir
@np:    lda #$13
        jsr cmd2
        jsr fin
        lda p_err
        bne @e
        lda #$14                ; READ_DIR: minden bejegyzes kulon csomag
        jsr cmd2
        stx cmdlen
        lda #0
        sta p_err
        jsr w_idle
        ldx #0
@c:     lda cmdbuf,x
        sta CMD
        inx
        cpx cmdlen
        bne @c
        lda #1
        sta CTRL
        lda #$80
        sta dst
@e:     jmp ret

do_dirnext:
        lda dst
        cmp #$80
        beq @read
        cmp #$40
        beq @end
        lda #2                  ; az elozo csomag elfogadasa
        sta CTRL
        lda dst
        cmp #$30
        beq @read
        jsr w_idle
        lda #$40
        sta dst
@end:   lda #1
        ldx #0
        rts
@read:  jsr rx_ent
        lda #0
        sta sn
        sta dn
        sta dn+1
        jsr recv_pkt
        sta dst
        lda dn
        beq @none
        ldx dn
        cpx #81                 ; a hosszabb nevet levagjuk (a hivo strlen >= 64 alapjan felismeri)
        bcc @cl
        ldx #80
@cl:    lda #0
        sta ent,x
        lda ent
        sta p_att
        lda #0
        ldx #0
        rts
@none:  lda #2
        sta CTRL
        jsr w_idle
        lda #$40
        sta dst
        jmp @end

do_size:
        lda #7
        jsr cmd2
        jsr fin
info_done:
        lda dn
        ora dn+1
        bne @ok
        lda #$FE
        sta p_err
@ok:    ldx #3
@c:     lda ent,x
        sta p_size,x
        dex
        bpl @c
        jmp ret

; megnyitott fajl <-> REU (24 bites cim es hossz, a negyedik bajt 0)
do_loadreu:
        lda #$21
        .byte $2C
do_savereu:
        lda #$22
        jsr cmd2
        ldy #0
@c:     lda p_reu,y
        sta cmdbuf+2,y
        lda p_rlen,y
        sta cmdbuf+6,y
        iny
        cpy #4
        bne @c
        ldx #10
        jsr fin
        jmp ret

do_delete:
        lda #9
        jsr cmd2
        jsr app_name
        jsr fin
        jmp ret

do_abort:
        lda #4                  ; UCI abort: a meg ki nem olvasott csomagok eldobasa
        sta CTRL
        jsr w_idle
        lda #$40
        sta dst
        lda #0
        ldx #0
        rts

do_getpath:
        lda #$12
        jsr cmd2
        stx cmdlen
        lda #<p_path
        sta rxs
        lda #>p_path
        sta rxs+1
        lda #63
        sta rxm
        lda #0
        sta rxm+1
        jsr send
        ldx dn
        lda #0
        sta p_path,x
        jmp ret

; ---- adatok (nincsenek a fajlban)
        .segment "BSSA"
tmo:    .res 3
sn:     .res 1
dn:     .res 2
st:     .res 1
rxm:    .res 2
cmdlen: .res 1
wrem:   .res 2
wn:     .res 2
wc:     .res 2
dst:    .res 1
        .segment "BSSB"
sbuf:   .res 14
        .export ent
ent:    .res 84
cmdbuf = ent            ; a parancs elkuldese utan a puffer a valasz fogadasara szolgal
