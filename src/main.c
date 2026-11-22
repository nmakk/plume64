/* Plume : traitement de texte graphique pour Commodore 64.
   Souris 1351 (port 1) ou joystick (port 1 ou 2), police proportionnelle,
   impression MPS-803.
   (c) 2026 LABBEJ27. */
#include <cbm.h>
#include <c64.h>
#include <mouse.h>
#include <string.h>
#include <peekpoke.h>
#include "charmap.h"
#include "strings.h"

/* --- gfx.s --- */
extern unsigned gx, gbase;
extern unsigned char gch, gstyle;
void gfx_init(void);
void draw_glyph(void);
unsigned char __fastcall__ glyph_width(unsigned char code);
void __fastcall__ bm_fill(unsigned addr, unsigned len, unsigned char value);
void __fastcall__ bm_or(unsigned addr, unsigned char value);
void __fastcall__ bm_xormask(unsigned addr, unsigned char mask);
void __fastcall__ bm_move(unsigned dst, unsigned src, unsigned len);
unsigned next_line_asm(void);
extern unsigned nl_pos;
extern unsigned char nl_style;
unsigned next_line_buf(void);
extern unsigned nl_base, nl_len;
extern const unsigned char font_width[];
extern unsigned char fsmall;                    /* 1 : kis betukeszlet (gfx.s) */
#define small_font fsmall
#ifdef REU
void __fastcall__ font_load(unsigned char small);
static void set_small(unsigned char on);
#endif
extern unsigned cur_addr, cur_n;
extern unsigned char cur_sh, cur_st;
extern unsigned char *cur_ptr;
void cur_sample(void);
extern unsigned char cur_row;
extern unsigned cur_px;
void cur_place(void);
void cursor_off(void);
void nmi_off(void);
extern unsigned char nmi_hit;
void blink_cursor(void);
extern unsigned char col[5], theme_no;
void set_theme(void);
void next_theme(void);
static unsigned char dev = 8;                   /* a lemez egysegszama (8..11) */
#ifdef REU
void __fastcall__ led_theme(unsigned char t);
#endif

#if defined(REU) && !defined(NORTF)
#define RTF                     /* import / export RTF (build REU) */
#endif

#define BOLD      0x01
#define ITALIC    0x02
#define UNDERLINE 0x04

/* Le texte contient des marqueurs de style invisibles ($10 + style) aux seuls
   endroits ou le style change : environ 1 octet par caractere. */
#define IS_MARK(c) (((c) & 0xF8) == 0x10)
/* Largeur d'un caractere lue directement dans la table de la police
   (un octet par code a partir de 32). Gras : +1 pixel. */
#define CW(c, st)  ((font_width - 32)[c] + ((st) & BOLD))
#define MARK(s)    (0x10 | (s))

#define SCREEN    0xE000u
#define COLORS    ((unsigned char *)0xCC00)
#define ROWC(r)    (SCREEN + (r) * 320u)                 /* constante */
static unsigned rowbase(unsigned char r) { return SCREEN + (unsigned)r * 320u; }
#define ROWBASE(r) rowbase((unsigned char)(r))
#define TEXT_X0   8
#define TEXT_W    304
#define TEXT_ROW0 2
#define VISIBLE   22
#define FOOT_ROW  24
#ifdef REU
#ifndef TESTPARAS
#define TESTPARAS 1395UL
#endif
#ifdef MAXDOC_TEST
#ifndef MAXDOC_TEST_N
#define MAXDOC_TEST_N 4000u
#endif
#define MAXDOC    MAXDOC_TEST_N         /* build de test : la place manque avec les crochets de test */
#else
#define MAXDOC    3250u                 /* fenetre en RAM ; le document entier est dans le REU */
#endif
#else
#define MAXDOC    20000u
#endif
#ifdef REU
#define MAXLINES  520                   /* $C69E-tol az SD-modul puffere, $C700-tol a kodja (src/sd.s), $CBC0-nal a 2. sprite */
#else
#define MAXLINES  950
#endif
#define CR        13
#define NOSEL     0xFFFFu
#define PRINT_CARDS_ 60

/* Memoire hors du programme :
   $C080 : debuts de ligne ; $D000 (sous les E/S, lus et ecrits E/S masquees) :
   ancienne mise en page pendant un recalcul, puis presse-papiers. */
#define lstart    ((unsigned *)0xC080)                        /* 2 octets par ligne */
#define lstyle    ((unsigned char *)(0xC080 + 2 * (MAXLINES + 2)))  /* style au debut de chaque ligne */
#define oldstart  ((unsigned *)0xD000)
#define oldstyle  ((unsigned char *)(0xD000 + 2 * (MAXLINES + 2)))
#define CLIP      ((unsigned char *)(0xD000 + 3 * (MAXLINES + 2)))
#define CLIPMAX   760
#ifdef REU
typedef char sd_area_check[1 - 2 * (0xC080 + 3 * (MAXLINES + 2) > 0xC69E)];
#endif   /* a sorok tablazata nem erhet az SD-modulig */
typedef unsigned long dpos;             /* position dans le document (24 bits utiles) */
/* Noms de la boite Ouvrir : sous les E/S, apres le presse-papiers. */
#define names     ((char (*)[28])(0xD000 + 3 * (MAXLINES + 2) + CLIPMAX))
#define rawn      ((char (*)[64])((char *)names + 13 * 28))   /* az SD-bejegyzesek nyers neve (a meretlekerdezeshez) */
/* $0334-$03FB, tampon cassette inutilise : petites chaines. */
#define diskmsg   ((char *)0x0334)              /* 41 octets */
#define cmdname   ((char *)0x035D)              /* 24 */
#define findtext  ((unsigned char *)0x02D0)     /* 24 : hors du tampon cassette, que le code des leds ecrase */
#define filename  ((char *)0x038D)              /* 17, PETSCII */
#define statusbuf ((char *)0x039E)              /* 40 : barre d'etat */
#define inputbuf  ((char *)0x03C6)              /* 40 : saisie, jusqu'a $03ED */

/* Boite aux lettres de test (VICE ou API du C64 Ultimate) :
   1 = clic en x, y ; 4 = clic avec extension de la selection ;
   2 = gele le programme ROM masquee (repond 3) pour que l'API lise l'ecran
   en $E000, jusqu'a ce que l'outil remette 0. */
#define MAILBOX   ((volatile unsigned char *)0x02A7)

/* Couleurs : fond (4 bits bas) et encre (4 bits hauts). */
#define COL_PAGE   col[0]
#define COL_BAR    col[1]
#define COL_MARGIN col[2]
/* Jeux de couleurs, CTRL+fleche gauche passe au suivant : page, barres, marges, bord,
   fleche. Le curseur prend l'encre (bloc) et le fond (lettre) de la page. */
#if defined(KBD_HU) && defined(REU)
#define HUN                             /* clavier HUN : disposition hongroise sur les touches du C64 (src/hunkey.s) */
void __fastcall__ hun_set(unsigned char on);
static unsigned char hun_on;
static unsigned char overwrite;                 /* mode ecrasement (Shift + INST/DEL) */
static void hun_toggle(void);
#endif
static unsigned char sound_on = 1;              /* clic de touche */
static void key_click(void);
#define NTHEMES 4
const unsigned char themes[NTHEMES][5] = {
    { 0x01, 0x0F, 0x0C, COLOR_GRAY1, COLOR_BLACK },     /* noir sur blanc */
    { 0xD0, 0x0D, 0x0B, COLOR_BLACK, COLOR_WHITE },     /* vert sur noir */
    { 0xE6, 0x6E, 0x06, COLOR_LIGHTBLUE, COLOR_WHITE }, /* bleu Commodore */
    { 0x70, 0x07, 0x0B, COLOR_BLACK, COLOR_WHITE },     /* jaune sur noir */
};
#ifdef REU
static const unsigned char accent[NTHEMES] = { COLOR_PURPLE, COLOR_PURPLE, COLOR_RED, COLOR_RED };   /* az E betu tintaja a temakhoz */
static const unsigned char dark_tone[NTHEMES] = { 11, 5, 0, 9 };      /* sotet el / arnyek / csuszka szine temankent */
#endif

unsigned char text[MAXDOC];               /* exporte pour next_line_asm */
/* Tampon d'impression : une ligne du mode Texte, ou la sortie d'une passe du
   mode Graphique. Libre autrement. */
#ifdef REU
#define outbuf ((unsigned char *)0x0540)       /* $0540-$0722 : ecran texte inutilise ($0400 : tampon des rangees) */
#else
static unsigned char outbuf[PRINT_CARDS_ * 8 + 3];
#endif
unsigned doclen;
static unsigned nlines;
static unsigned relayout_last;
static unsigned relayout_from;          /* premiere ligne dont le contenu change */
static unsigned top;
unsigned cursor;                          /* exportes pour gfx.s */
unsigned anchor = NOSEL;
static unsigned char style;
static unsigned char dirty;
#ifdef REU
static dpos cliplen;                    /* presse-papiers dans le REU (ou, sans REU, dans CLIP) */
static dpos c_base, c_cap = CLIPMAX;
static unsigned char clip_st, clip_end, clip_m0;       /* style au debut, a la fin ; le premier octet est-il un marqueur */
static dpos agl;                        /* position globale de l'ancre de la selection (anchor n'est que sa part visible) */
static unsigned char bdel(void);
static void copy_selection(void);
static void cut_selection(void);
static void paste(void);
static void select_all(void);
#else
static unsigned cliplen;
#endif
static unsigned char key;
unsigned wrap_width = TEXT_W;
static unsigned char next_style;             /* style a la fin de next_line() */

#ifdef REU
/* Edition REU : le document entier vit dans le REU (jusqu'a 16 Mo), text[] en
   est une fenetre. Adresses REU lineaires de 24 bits : le debut du document
   ([0, win_base)) est range en bas, a l'adresse = position ; la suite apres la
   fenetre (suf_len octets) en haut, finissant en suf_end. Au-dela de suf_end :
   64 Ko de bloc-notes (impression). reu_move coupe les transferts aux limites
   de banque. Les positions dans la fenetre restent sur 16 bits. */

extern unsigned reu_c64, reu_len;
extern unsigned char reu_addr[3];
extern void reu_stash(void), reu_fetch(void);
extern unsigned char reu_detect(void);
void __fastcall__ ovl_load(unsigned char n);   /* charge l'overlay n (banque de la cartouche) */
#define ROOM     400                    /* place libre minimale dans la fenetre */
#define FILL     (MAXDOC - ROOM - 200)  /* remplissage apres un glissement */
#define STEP     768                    /* taille mini du morceau qui quitte la fenetre */
static unsigned char reu_on;
static dpos win_base;                   /* position dans le document de text[0] */
/* Sauvegarde rapide : journal des modifications (dans le REU) -> fichier .bck. Le code est dans l'overlay 3. */
static unsigned char op_ok;                     /* le chargement / l'enregistrement qui vient de finir a reussi */
static unsigned char jr_ok;                     /* un journal valide existe (le document a un fichier de base) */
static dpos jr_len, jr_last;                    /* longueur du journal, debut du dernier enregistrement (valide si jr_n) */
static unsigned jr_n;                           /* nombre d'enregistrements (leurs debuts : pile L) */
static dpos jr_base, r_base, l_base, d_base, u_cap;   /* zones du REU : journal, octets supprimes, pile L, pile refaire ; taille du journal */
static unsigned l_max, d_max;
static dpos jr_lp;                              /* dernier enregistrement : position, octets supprimes / inseres */
static unsigned jr_ld, jr_li;
static void jr_log(unsigned a, unsigned del, unsigned n);
static void jr_rem(unsigned a, unsigned n);
static void cursor_moved(void);
static void handle_key(unsigned char k);
static void handle_click(void);
static void kbd_menu(unsigned char m);
static dpos jb, rl;                         /* debut du journal depuis le dernier enregistrement ; octets supprimes gardes (annuler) */
static unsigned char jr_off, bck_skip;
static unsigned char jr_blk, jr_lf;             /* operation en bloc (2 : premiere partie, 1 : suite) ; drapeaux du dernier enregistrement (0x80 bloc, 0x40 suite) */
static unsigned redo_n;                    /* annuler en cours : on ne journalise pas */
static void quick_save(void);
static void bck_saved(void);
static void bck_loaded(void);
/* SD / USB du C64 Ultimate : le fichier se lit et s'ecrit par l'interface de commande (UCI), via le
   petit module de src/sd.s, copie en $CC00 au demarrage. sd_on : le document est sur la SD. */
unsigned char __fastcall__ sd_open(unsigned char mode);
unsigned char sd_probe(void), sd_close(void), sd_read(void), sd_write(void), sd_dirbeg(void), sd_dirnext(void);
unsigned char sd_delete(void), sd_size(void), sd_loadreu(void), sd_savereu(void), sd_getpath(void), sd_abort(void), sd_chdir(void);
#define SD_PATH ((char *)0xCB00)                /* repertoire (absolu) */
#define SD_NAME ((char *)0xCB40)                /* nom du fichier, octets bruts */
#define SD_PTR  (*(unsigned *)0xCB80)
#define SD_LEN  (*(unsigned *)0xCB82)
#define SD_SIZE (*(unsigned long *)0xCB84)
#define SD_ATT  (*(unsigned char *)0xCB88)
#define SD_REU  (*(unsigned long *)0xCB8A)
#define SD_RLEN (*(unsigned long *)0xCB8E)
#define SD_ENT  ((unsigned char *)0xC6AC)       /* entree de repertoire : attribut, nom */
#define SD_SV   ((char *)0xCB92)                /* extension sauvee pendant que le nom est celui du .bck */
#define SD_PE   (*(unsigned char *)0xCB9A)
static char sd_title[28];                       /* az SD-n levo dokumentum neve a cimsorhoz (megjelenitesre kesz) */
#define sd_cand ((char *)outbuf + 128)         /* a valasztoban kijelolt SD-nev, 64 bajt (a dokumentum neve csak jovahagyas utan valtozik) */
static char dlabel[4] = "D 8";
static unsigned char sd_on, sd_ok, sd_bad, sd_pk, sd_ca, at12, pk_first;
static void sd_conv(char *d, const unsigned char *s, unsigned char n);       /* sd_ca : a kivalasztott bejegyzes attributuma */
static unsigned char quiet;                     /* pas de dessin (relecture du journal) */
static dpos suf_len;                    /* octets apres la fenetre, dans le REU */
static dpos suf_end;                    /* adresse REU de la fin du suffixe */
static unsigned char win_style;         /* style au debut de la fenetre */
static dpos lines_before;               /* lignes avant la fenetre (estimation apres un saut) */
static unsigned bpl_est = 40;           /* octets par ligne, moyenne lissee des fenetres vues */
#ifdef REU
/* Pontos sorszam: a hatterben (a program tetlen) a teljes dokumentumot soronkent vegigtordeljuk, es 2 KB-onkent egy
   ellenorzo pontot (hely, addigi sorok, stilus) taroljuk a REU-ban. Ugrasnal innen indulva a sorszam pontos. */
static unsigned ix_n, ix_max;                   /* ervenyes ellenorzo pontok, kapacitas (0 : nincs index) */
static dpos ix_base, ix_pos, ix_line, ix_tot;   /* a REU teruletete, a futo allapot (hely, sorok), az osszes sor (0 : meg nem ismert) */
static unsigned char ix_sty, ix_st, ix_go;      /* futo stilus, az allapot ervenyes, van tennivalo */
static void ix_tick(void);
static void doc_read(unsigned char *dst, dpos g, unsigned n);
static void ix_reset(void) { ix_n = ix_st = 0; ix_go = 1; ix_tot = 0; }
static void ix_inv(void)                        /* a dokumentum megvaltozott az ablakban */
{
    unsigned k = (unsigned)(win_base >> 11);                    /* az ablak elejetol (az a-t figyelmen kivul hagyva : biztonsagosan keves pontot tart meg) */
    if (ix_n > k) ix_n = k;
    ix_st = 0;
    ix_go = 1;
    ix_tot = 0;
}
#endif
static unsigned char lines_exact = 1;   /* lines_before est exact (pas de saut depuis le debut du document) */
#define fbuf outbuf                     /* lecture du document : 256 octets au plus */
static dpos first_mark;                 /* au plus la position du premier marqueur de style du document */
static dpos sc_lo1, sc_hi;              /* cache de style_at */
static unsigned char sc_st, sc_ok;
#endif

/* JiffyDOS passe au protocole lent des qu'un sprite est actif : on les coupe
   pendant les transferts de fichiers. */
#ifndef REU
#define fo_w() cbm_open(2, dev, 2, cmdname)
#define fw(b, n) cbm_write(2, b, n)
#define fc() cbm_close(2)
#define fr(b, n) cbm_read(2, b, n)
#endif
#ifdef REU
static void disk_busy(unsigned char on);        /* IEC : a nyil elrejtese (JiffyDOS) ; SD / C64U : a nyil homokorra valt, amig var */
#else
#define disk_busy(on) POKE(0xD015, (on) ? 0 : 1)
#endif

static void io_hide(void) { asm("sei"); POKE(0x01, 0x34); }
static void io_show(void) { POKE(0x01, 0x36); asm("cli"); }

/* ------------------------------------------------------------------ */
/* Dessin de base                                                      */

/* Tampon d'une rangee ($0400, ecran texte inutilise) : les lignes y sont dessinees puis copiees
   d'un coup a l'ecran, pour qu'elles ne clignotent pas quand on tape. draw_off != 0 : dessin dans le tampon. */
#ifdef REU
#define LINEBUF   0x0400u
static unsigned draw_off;
#endif

static unsigned put_text(unsigned x, unsigned char row, const char *s, unsigned char st)
{
    gbase = ROWBASE(row);
#ifdef REU
    if (draw_off) gbase = draw_off;
#endif
    gstyle = st;
    while (*s) {
        gch = (unsigned char)*s++;
        gx = x;
        draw_glyph();
        x += glyph_width(gch);
    }
    return x;
}

static unsigned text_width(const char *s, unsigned char st)
{
    unsigned w = 0;
    gstyle = st;
    while (*s) w += glyph_width((unsigned char)*s++);
    return w;
}

static void clear_cells(unsigned char row, unsigned x, unsigned w)
{
    bm_fill(ROWBASE(row) + x, w, 0);
}

static void color_cells(unsigned char row, unsigned char col, unsigned char n, unsigned char c)
{
    memset(COLORS + row * 40 + col, c, n);
}

static void hline(unsigned char row, unsigned x, unsigned w, unsigned char y)
{
    unsigned i;
    for (i = 0; i < w; i += 8) bm_or(ROWBASE(row) + x + i + y, 0xFF);
}

static void vline(unsigned char row, unsigned x, unsigned char mask)
{
    unsigned char y;
    for (y = 0; y < 8; ++y) bm_or(ROWBASE(row) + x + y, mask);
}

static void xor_span(unsigned char row, unsigned x0, unsigned x1)
{
    unsigned card, base = ROWBASE(row);
    unsigned char a, b, mask;
#ifdef REU
    if (draw_off) base = draw_off;
#endif
    for (card = x0 & 0xFFF8u; card < x1; card += 8) {
        a = card < x0 ? (unsigned char)(x0 - card) : 0;
        b = card + 8 > x1 ? (unsigned char)(x1 - card) : 8;
        mask = (unsigned char)((0xFF >> a) & (0xFF << (8 - b)));
        bm_xormask(base + card, mask);
    }
}


static void petscii_to_text(char *dst, const char *src)
{
    unsigned char c;
    while ((c = (unsigned char)*src++) != 0) {
        if (c >= 0x41 && c <= 0x5A) c += 0x20;
        else if (c >= 0xC1 && c <= 0xDA) c -= 0x80;
        *dst++ = c;
    }
    *dst = 0;
}

static char *utoa_simple(char *b, unsigned n)
{
    char tmp[6];
    unsigned char k = 0;
    do { tmp[k++] = (char)('0' + n % 10); n /= 10; } while (n);
    while (k) *b++ = tmp[--k];
    *b = 0;
    return b;
}

#ifdef REU
static char *ultoa_simple(char *b, dpos n)
{
    char tmp[9];
    unsigned char k = 0;
    do { tmp[k++] = (char)('0' + (unsigned char)(n % 10)); n /= 10; } while (n);
    while (k) *b++ = tmp[--k];
    *b = 0;
    return b;
}
#endif

/* ------------------------------------------------------------------ */
/* Mise en page                                                        */

/* Debut de la ligne suivante, a partir de p dans le style st (assembleur,
   gfx.s). next_style recoit le style en vigueur a cette position. */
static unsigned next_line(unsigned p, unsigned char st)
{
    unsigned n;
    nl_pos = p;
    nl_style = st;
    n = next_line_asm();
    next_style = nl_style;
    return n;
}

#ifdef REU
/* Tord a partir de la ligne i, qui commence en s dans le style st. */
static void layout_from(unsigned i, unsigned s, unsigned char st)
{
    unsigned n;
    nlines = i;
    for (;;) {
        lstyle[nlines] = st;
        lstart[nlines++] = s;
        if (nlines >= MAXLINES) break;
        n = next_line(s, st);
        st = next_style;
        if (n >= doclen) {
            if (doclen && text[doclen - 1] == CR && s != doclen) {
                lstyle[nlines] = st;
                lstart[nlines++] = doclen;
            }
            break;
        }
        s = n;
    }
    lstart[nlines] = doclen;
}

static void layout_all(void)
{
    layout_from(0, 0, win_style);
}

#else
static void layout_all(void)
{
    unsigned s = 0, n;
    unsigned char st = 0;
    nlines = 0;
    for (;;) {
        lstyle[nlines] = st;
        lstart[nlines++] = s;
        if (nlines >= MAXLINES) break;
        n = next_line(s, st);
        st = next_style;
        if (n >= doclen) {
            if (doclen && text[doclen - 1] == CR && s != doclen) {
                lstyle[nlines] = st;
                lstart[nlines++] = doclen;
            }
            break;
        }
        s = n;
    }
    lstart[nlines] = doclen;
}
#endif

static unsigned line_of(unsigned p)
{
    unsigned lo = 0, hi = nlines - 1, mid;
    while (lo < hi) {
        mid = (lo + hi + 1) / 2;
        if (lstart[mid] <= p) lo = mid; else hi = mid - 1;
    }
    return lo;
}

/* Style en vigueur pour un caractere tape en p (marqueurs avant p compris). */
static unsigned char style_before(unsigned p)
{
    unsigned i = line_of(p), q;
    unsigned char st = lstyle[i];
    for (q = lstart[i]; q < p; ++q) if (IS_MARK(text[q])) st = text[q] & 7;
    return st;
}

/* Recalcule les lignes apres le remplacement de [p, old_end) par un texte qui
   finit en new_end. S'arrete des que l'ancienne mise en page est retrouvee. */
static unsigned relayout(unsigned p, unsigned old_end, unsigned new_end, unsigned char *all)
{
    unsigned first, i, j, s, n, oldn = nlines;
    int delta = (int)(new_end - p) - (int)(old_end - p);
    unsigned char st;
    io_hide();
    memcpy(oldstart, lstart, (nlines + 1) * sizeof(unsigned));
    memcpy(oldstyle, lstyle, nlines + 1);
    for (j = 0; j <= oldn; ++j) {
        if (oldstart[j] > p) {
            if (oldstart[j] <= old_end) oldstart[j] = p;
            else oldstart[j] += delta;
        }
    }
    first = line_of(p);
    if (first) --first;
    i = first;
    j = first + 1;
    s = lstart[first];
    st = lstyle[first];
    *all = 0;
    relayout_from = first;
    for (;;) {
        if (i + 1 >= MAXLINES) { nlines = i + 1; break; }
        n = next_line(s, st);
        st = next_style;
        if (n >= doclen) {
            if (doclen && text[doclen - 1] == CR && s != doclen) { lstart[++i] = doclen; lstyle[i] = st; }
            nlines = i + 1;
            break;
        }
        while (j < oldn && oldstart[j] < n) ++j;
        if (j < oldn && oldstart[j] == n && n > new_end && oldstyle[j] == st) {
            memcpy(&lstart[i + 1], &oldstart[j], (oldn - j) * sizeof(unsigned));
            memcpy(&lstyle[i + 1], &oldstyle[j], oldn - j);
            nlines = i + 1 + (oldn - j);
            break;
        }
        if (i == first && n == oldstart[first + 1]) relayout_from = first + 1;
        lstart[++i] = n;
        lstyle[i] = st;
        s = n;
    }
    io_show();
    relayout_last = i;
    lstart[nlines] = doclen;
    if (nlines != oldn) *all = 1;
    return first;
}

/* ------------------------------------------------------------------ */
/* Affichage du document                                               */

static unsigned sel_lo(void) { return anchor == NOSEL ? cursor : (anchor < cursor ? anchor : cursor); }
static unsigned sel_hi(void) { return anchor == NOSEL ? cursor : (anchor < cursor ? cursor : anchor); }
#ifdef REU
/* L'ancre est gardee en position globale (agl) ; anchor est sa part visible dans la fenetre. */
static void anchor_fix(void)
{
    if (anchor == NOSEL) return;
    anchor = agl < win_base ? 0 : agl > win_base + doclen ? doclen : (unsigned)(agl - win_base);
}
static dpos sel_lo_g(void) { dpos c = win_base + cursor; return anchor == NOSEL || c < agl ? c : agl; }
static dpos sel_hi_g(void) { dpos c = win_base + cursor; return anchor == NOSEL || c > agl ? c : agl; }
static unsigned char sel_in_window(void) { return anchor == NOSEL || (agl >= win_base && agl <= win_base + doclen); }
#else
#define anchor_fix()
#endif

static void draw_line(unsigned i)
{
    unsigned char row, st, c;
    unsigned p, end, x = TEXT_X0, lo = sel_lo(), hi = sel_hi(), sx = 0, ex = 0;
    if (i < top || i >= top + VISIBLE) return;
    row = (unsigned char)(TEXT_ROW0 + i - top);
#ifdef REU
    draw_off = LINEBUF;
    bm_fill(LINEBUF, 320, 0);
    gbase = LINEBUF;
#else
    clear_cells(row, 0, 320);                   /* marges comprises : efface les ombres des menus */
    gbase = ROWBASE(row);
#endif
#ifdef REU
    if (i >= nlines) goto show;
#else
    if (i >= nlines) return;
#endif
    end = lstart[i + 1];
    st = lstyle[i];
    for (p = lstart[i]; p < end; ++p) {
        if (p == lo) sx = x;
        if (p == hi) ex = x;
        c = text[p];
        if (IS_MARK(c)) { st = c & 7; continue; }
        if (c == CR) { if (p >= lo && p < hi) x += 4; break; }
        gch = c;
        gstyle = st;
        gx = x;
        draw_glyph();
        x += CW(c, st);
    }
    if (lo < hi && lo < end && hi > lstart[i]) {
        if (lo < lstart[i]) sx = TEXT_X0;
        if (hi >= end || (p < hi && ex == 0)) ex = x;
        if (ex > sx) xor_span(row, sx, ex);
    }
#ifdef REU
show:
    draw_off = 0;
    bm_move(ROWBASE(row), LINEBUF, 312);        /* la colonne 39 est l'ascenseur */
#endif
}

static void draw_lines(unsigned from, unsigned to)
{
    unsigned i;
    if (from < top) from = top;
    for (i = from; i <= to && i < top + VISIBLE; ++i) draw_line(i);
}

#ifdef REU
static void draw_sbar(void);
static void sbar_update(void);
#endif

static void draw_text_area(void)
{
    draw_lines(top, top + VISIBLE - 1);
#ifdef REU
    draw_sbar();                                /* draw_line efface la colonne de l'ascenseur */
#endif
}

static unsigned cursor_x(void)
{
    unsigned i = line_of(cursor), p, x = 0;
    unsigned char st = lstyle[i], c;
    for (p = lstart[i]; p < cursor; ++p) {
        c = text[p];
        if (IS_MARK(c)) st = c & 7; else x += CW(c, st);
    }
    return x;
}

/* Curseur : bloc inverse a la Commodore sur le caractere qui suit la position
   d'insertion. Sprite 1 (blanc) : points du caractere relus dans le bitmap ;
   sprite 2 (noir, bloc 47 en $CBC0, libre apres lstyle) : fond plein, dessous. */
#define CUR_BLOCK  ((unsigned char *)0xCBC0)
#ifdef REU
/* Decale le texte d'une rangee, rangee par rangee : la colonne 39 (ascenseur) ne bouge pas. */
static void scroll_text(unsigned a, int step)
{
    unsigned char r;
    unsigned b;
    for (r = VISIBLE - 1; r; --r) { b = a + step; bm_move(a, b, 312); a = b; }
}
#endif
static void place_cursor(void)
{
    unsigned i = line_of(cursor);

    if (i + 1 == top) {                         /* une ligne plus haut : decale l'image */
#ifdef REU
        scroll_text(ROWC(TEXT_ROW0 + VISIBLE - 1), -320);
#else
        bm_move(ROWC(TEXT_ROW0 + 1), ROWC(TEXT_ROW0), (VISIBLE - 1) * 320u);
#endif
        top = i;
        draw_line(i);
#ifdef REU
        sbar_update();
#endif
    } else if (i == top + VISIBLE) {            /* une ligne plus bas */
#ifdef REU
        scroll_text(ROWC(TEXT_ROW0), 320);
#else
        bm_move(ROWC(TEXT_ROW0), ROWC(TEXT_ROW0 + 1), (VISIBLE - 1) * 320u);
#endif
        ++top;
        draw_line(i);
#ifdef REU
        sbar_update();
#endif
    } else if (i < top) { top = i; draw_text_area(); }
    else if (i >= top + VISIBLE) { top = i - VISIBLE + 1; draw_text_area(); }
    cur_row = (unsigned char)(TEXT_ROW0 + i - top);
    cur_px = TEXT_X0 + cursor_x();
    cur_place();                                /* forme relue a l'allumage */
}

/* Copie du caractere sous le curseur (largeur cherchee dans gfx.s). */
void cursor_sample(void)
{
    cur_st = style_before(cursor);
    cur_ptr = text + cursor;
    cur_n = doclen - cursor;
    cur_sample();
}

/* ------------------------------------------------------------------ */
/* Barre d'etat                                                        */

#define STYLE_X 262

/* Barre de titre : nom du document et boutons de style, redessinee seulement
   quand elle change. Barre du bas : ligne et place libre. */
static unsigned char shown_dirty = 255, shown_style = 255;

static void draw_title(void)
{
    static char letter[2];
    char *buf = statusbuf;
    unsigned x;
    unsigned char k;
    shown_dirty = dirty;
    shown_style = style;
    clear_cells(1, 0, 320);
#ifdef REU
    if (sd_on && filename[0]) strcpy(buf, sd_title);
    else
#endif
    petscii_to_text(buf, filename);
    if (!filename[0]) strcpy(buf, S_UNTITLED);
    x = put_text(6, 1, buf, BOLD);
    if (dirty) put_text(x + 2, 1, "*", BOLD);
    for (k = 0; k < 3; ++k) {
        x = STYLE_X + k * 18;
        letter[0] = S_STYLEKEYS[k];
        put_text(x + 4, 1, letter, (unsigned char)(1 << k));
        xor_span(1, x, x + 1);
        xor_span(1, x + 15, x + 16);
        if (style & (1 << k)) xor_span(1, x + 1, x + 15);
    }
}

#ifdef REU
/* Ascenseur dans la colonne de cartes 39 (x 312..319), rangees de texte : un
   rail fin et un curseur de 16 points dont la position suit le document entier. */
#define SBAR_X  312
#define SBAR_H  (VISIBLE * 8)
/* Etendue de l'ascenseur : le document moins la derniere page (estimee), car
   la derniere position du haut de l'ecran est 22 lignes avant la fin. */
static dpos sbar_range(void)
{
    dpos tot = win_base + doclen + suf_len;
    unsigned last = (doclen / nlines) * VISIBLE;
    return tot > last ? tot - last : 1;
}

/* Curseur de 16 points dont le haut est a ty (0..SBAR_H-16). */
static void draw_thumb(unsigned ty)
{
    unsigned char r, i, *p;
    unsigned y;
    for (r = 0; r < VISIBLE; ++r) {
        p = (unsigned char *)ROWBASE(TEXT_ROW0 + r) + SBAR_X;
        y = r * 8;
        for (i = 0; i < 8; ++i, ++y) p[i] = (y >= ty && y < ty + 16) ? 0xFF : 0x00;
    }
}

/* Tant que tout le document est dans la fenetre, l'ascenseur compte en lignes
   (exact) ; sinon en octets. */
#define SBAR_LINES (!win_base && !suf_len)

/* Position du curseur pour le haut de l'ecran actuel. */
static unsigned sbar_ty(void)
{
    dpos R, pos;
    if (SBAR_LINES) { R = nlines > VISIBLE ? nlines - VISIBLE : 1; pos = top; }
    else { R = sbar_range(); pos = win_base + lstart[top]; }
    if (pos > R) pos = R;
    while (R > 400) { R >>= 1; pos >>= 1; }
    return (unsigned)pos * (SBAR_H - 16) / (unsigned)R;
}

static unsigned shown_ty = 0xFFFF;

static void draw_sbar(void)
{
    unsigned ty = sbar_ty();
    shown_ty = ty;
#ifdef REUTEST
    *(unsigned *)0x02B6 = ty;
#endif
    draw_thumb(ty);
}
#endif

#ifdef REU
static void sbar_update(void)
{
    if (sbar_ty() != shown_ty) draw_sbar();
}
#endif

static void draw_status(void)
{
    char *buf = statusbuf, *b;
    if (dirty != shown_dirty || style != shown_style) draw_title();
#ifdef REU
    draw_off = LINEBUF;
    bm_fill(LINEBUF, 320, 0);
#else
    clear_cells(FOOT_ROW, 0, 320);
#endif
    b = buf;
#ifdef REU
    strcpy(b, small_font ? S_LINE " " : "L ");     /* nagy betuvel csak "L" (hogy ne logjon bele a tobbi kijelzesbe) */
    b += strlen(b);
    if (!lines_exact) *b++ = '~';                /* apres un saut, le numero est une estimation */
    b = ultoa_simple(b, lines_before + line_of(cursor) + 1);
    if (ix_tot) {                                /* az osszes sor: ha az index mar kesz */
        *b++ = '/';
        ultoa_simple(b, ix_tot);
    }
#else
    strcpy(b, S_LINE " ");
    b = utoa_simple(b + strlen(b), line_of(cursor) + 1);
    *b++ = '/';
    utoa_simple(b, nlines);
#endif
    put_text(6, FOOT_ROW, buf, 0);
#ifdef HUN
    put_text(150, FOOT_ROW, hun_on ? "HUN" : "C64", BOLD);
    put_text(130, FOOT_ROW, overwrite ? "O" : "I", BOLD);
#endif
#ifdef REU
    {
        dpos nc = win_base + doclen + suf_len;                              /* kb. karakterszam (a stilusjelek is benne vannak) */
        strcpy(ultoa_simple(buf, nc), nc > 1 ? S_CHARS : S_CHAR);          /* 0 es 1: char, utana chars */
    }
#else
    strcpy(buf, S_FREE " ");
    utoa_simple(buf + strlen(buf), MAXDOC - doclen);
#endif
    put_text(314 - text_width(buf, 0), FOOT_ROW, buf, 0);
#ifdef REU
    draw_off = 0;
    bm_move(ROWBASE(FOOT_ROW), LINEBUF, 320);
    sbar_update();
#endif
}

#ifdef REU
/* Barre de progression dans la ligne d'etat pendant les longs transferts de fichiers (chargement, enregistrement). */
#define PG_X0 112                       /* cadre : x = 112 ... 311, remplissage en dedans */
#define PG_W  198
static unsigned pg_step, pg_units, pg_acc, pg_x;     /* pg_step = 0 : pas de barre */

static void pg_fill(unsigned x0, unsigned x1)
{
    unsigned char c0 = (unsigned char)(x0 >> 3);
    color_cells(FOOT_ROW, c0, (unsigned char)(((x1 - 1) >> 3) - c0 + 1), accent[theme_no]);    /* kitoltes : a cellak papirja a hangsulyszin */
}
static void pg_begin(const char *label, dpos total)
{
    pg_units = pg_acc = pg_x = 0;
    pg_step = total ? (unsigned)((total >> 8) / PG_W) + 1 : 0;      /* total en unites de 256 octets */
    if (!pg_step) return;
    draw_off = 0;
    clear_cells(FOOT_ROW, 0, 320);
    put_text(6, FOOT_ROW, label, 0);
    color_cells(FOOT_ROW, PG_X0 >> 3, 25, dark_tone[theme_no]);         /* a palya : sotet cellak (papir = sotet) */
}

static void pg_add(unsigned k)
{
    unsigned x;
    if (!pg_step) return;
    pg_acc += k;
    if (pg_acc < 256) return;
    pg_units += pg_acc >> 8;
    pg_acc &= 255;
    x = pg_units / pg_step;
    if (x > PG_W) x = PG_W;
    if (x > pg_x) { pg_fill(PG_X0 + 1 + pg_x, PG_X0 + 1 + x); pg_x = x; }
}

static void pg_end(void)
{
    if (!pg_step) return;
    pg_step = 0;
    color_cells(FOOT_ROW, 0, 40, COL_BAR);
    draw_status();
}
#endif

/* ------------------------------------------------------------------ */
/* Menus et panneaux                                                   */

#define NMENUS 5
static const char *const menu_title[NMENUS] = { S_MENU0, S_MENU1, S_MENU2, S_MENU3, S_MENU4 };
#define MENUW 12
#ifdef REU
#pragma rodata-name (push, "OVERLAY10")
#endif
static const char *const menu_items[NMENUS][MENUW] = {
#ifdef RTF
    { S_NEW, S_OPEN, S_RELOAD, S_RELOADB, S_QSAVE, S_SAVE, S_SAVEAS, 0 },
#else
    { S_NEW, S_OPEN, S_SAVE, S_SAVEAS, 0 },
#endif
#ifdef REU
    { S_CUT, S_COPY, S_PASTE, S_SELALL, S_FIND, S_FINDNEXT, S_UNDO, S_REDO, S_DELFWD, S_LINEEND, S_TABK, 0 },
#else
    { S_CUT, S_COPY, S_PASTE, S_SELALL, S_FIND, S_FINDNEXT, 0 },
#endif
#ifdef REU
    { S_PLAIN, S_BOLD, S_ITALIC, S_UNDERLINE, S_COLORS, S_SMALL, 0 },
#else
    { S_PLAIN, S_BOLD, S_ITALIC, S_UNDERLINE, 0 },
#endif
    { S_PGRAPH, S_PTEXT, S_PWIDE, 0 },
#ifdef HUN
    { S_ABOUT, S_KBD, S_CLICK, S_OVERWR, 0 },
#elif defined(REU)
    { S_ABOUT, S_CLICK, 0 },
#else
    { S_ABOUT, 0 },
#endif
};
/* A billentyuk : nagybetus nevek '+'-szal elvalasztva, a menu sapkakent rajzolja ('<' : balra nyil). */
static const char *const menu_keys[NMENUS][MENUW] = {
#ifdef RTF
    { "", "", "", "", "F1", "", "", 0 },
#else
    { "", "", "F1", "", 0 },
#endif
#ifdef REU
    { "F6", "F2", "F4", "", "", "F8", "RESTORE", "SHIFT+RESTORE", "SHIFT+HOME", "HOME", "CTRL+I", 0 },
#else
    { "F6", "F2", "F4", "", "", "F8", 0 },
#endif
#ifdef REU
    { "", "F3", "F5", "F7", "CTRL+<", "", 0 },
#else
    { "", "F3", "F5", "F7", 0 },
#endif
    { "", "", "", 0 },
#ifdef HUN
    { "", "C=+SPACE", "CTRL+9", "SHIFT+DEL", 0 },
#elif defined(REU)
    { "", "CTRL+9", 0 },
#else
    { "", 0 },
#endif
};
#ifdef REU
#pragma rodata-name (pop)
#endif
static unsigned menu_x[NMENUS + 1];
static unsigned char menu_kb, menu_next;        /* billentyuzetes menu : nyitas, a kovetkezo menu (1-tol) */

static void draw_menubar(void)
{
    unsigned char m;
    unsigned x = 8;
    clear_cells(0, 0, 320);
    for (m = 0; m < NMENUS; ++m) {
        menu_x[m] = x;
        put_text(x + 4, 0, menu_title[m], 0);
        x += (text_width(menu_title[m], 0) + 16) & 0xFFF8u;
    }
    menu_x[NMENUS] = x;
#ifdef REU
    put_text(304 - text_width("Plum", BOLD), 0, "Plum", BOLD);
    put_text(304, 0, "E", BOLD);                        /* az E a sajat cellajaban van : szine temankent kulon (page_colors) */
#else
    put_text(320 - 8 - text_width("Plume", BOLD), 0, "Plume", BOLD);
#endif
}

static void page_colors(void)
{
    unsigned char r;
#ifdef REU
    color_cells(0, 0, 40, (unsigned char)((COL_BAR & 0xF0) | 1));                     /* menusor : vilagos savos tonus */
    color_cells(0, 38, 1, (unsigned char)((accent[theme_no] << 4) | 1));               /* a PlumE "E" betuje */
#else
    color_cells(0, 0, 40, COL_BAR);
#endif
    color_cells(1, 0, 40, COL_BAR);
    color_cells(FOOT_ROW, 0, 40, COL_BAR);
    for (r = TEXT_ROW0; r < FOOT_ROW; ++r) {
        color_cells(r, 0, 1, COL_MARGIN);
        color_cells(r, 1, 38, COL_PAGE);
#ifdef REU
        color_cells(r, 39, 1, (unsigned char)((dark_tone[theme_no] << 4) | (COL_BAR & 15)));   /* gorgetosav : hatterszinu palya, sotet csuszka */
#else
        color_cells(r, 39, 1, COL_MARGIN);
#endif
    }
}

static void redraw_all(void)
{
    shown_dirty = 255;
    page_colors();
    draw_menubar();
    draw_status();
    draw_text_area();
    place_cursor();
}

#ifdef REU
/* Arnyek-cella: megtartja a hatterszinet (papir), tintaja a sotet ; a bitmap-ja v. */
/* Egyszinu vekony keretu doboz: row..row+rows-1, x, w ; fillc a belso cellak szine, a keret cellai a sotet tintat kapjak a belso papirjan. */
static void box(unsigned char row, unsigned char rows, unsigned x, unsigned w, unsigned char fillc)
{
    unsigned char c = (unsigned char)(x >> 3), n = (unsigned char)(w >> 3), r, e = (unsigned char)(row + rows);
    unsigned char dk = (unsigned char)((dark_tone[theme_no] << 4) | (fillc & 15));
    for (r = row; r < e; ++r) {
        color_cells(r, c, n, fillc);
        clear_cells(r, x, w);
        vline(r, x, 0x80);
        vline(r, x + w - 8, 0x01);
        color_cells(r, c, 1, dk);
        color_cells(r, (unsigned char)(c + n - 1), 1, dk);
    }
    color_cells(row, c, n, dk);
    color_cells((unsigned char)(e - 1), c, n, dk);
    hline(row, x, w, 0);
    hline((unsigned char)(e - 1), x, w, 7);
}
#define outline_box(row, rows, x, w) box(row, rows, x, w, COL_PAGE)
static void shade(unsigned char r, unsigned char col, unsigned char v)
{
    unsigned char *a = COLORS + r * 40 + col;
    *a = (unsigned char)((dark_tone[theme_no] << 4) | (*a & 15));
    bm_fill(ROWBASE(r) + ((unsigned)col << 3), 8, v);
}
/* Ablak: egyszinu, zart vekony keret + vekony (2 kepponttos) arnyek jobbra es lent. */
static void panel(unsigned char row, unsigned char rows, unsigned x, unsigned w)
{
    unsigned char r, k, c = (unsigned char)(x >> 3), n = (unsigned char)(w >> 3), e = (unsigned char)(row + rows);
    cursor_off();
    box(row, rows, x, w, COL_BAR);
    for (r = row; r < e; ++r) shade(r, (unsigned char)(c + n), r == row ? 0 : 0xC0);
    for (k = 0; k <= n; ++k) shade(e, (unsigned char)(c + k), 0);          /* alsó arnyek: a keret alatti sor cellainak elso ket kepponttsora */
    hline(e, x, w, 0);
    hline(e, x, w, 1);
    bm_or(ROWBASE(e) + x + w, 0xC0);                                      /* a sarok-cellaban csak 2 kepponttnyi */
    bm_or(ROWBASE(e) + x + w + 1, 0xC0);
}
#else
static void panel(unsigned char row, unsigned char rows, unsigned x, unsigned w)
{
    unsigned char r, e = (unsigned char)(row + rows);
    unsigned a;
    cursor_off();
    for (r = row; r < e; ++r) {
        color_cells(r, (unsigned char)(x / 8), (unsigned char)(w / 8 + 1), COL_PAGE);
        clear_cells(r, x, w);
        vline(r, x, 0x80);
        vline(r, x + w - 8, 0x01);
        bm_fill(ROWBASE(r) + x + w, 8, r == row ? 0 : 0xC0);
    }
    hline(row, x, w, 0);
    hline((unsigned char)(e - 1), x, w, 7);
    if (e < 25) {
        color_cells(e, (unsigned char)(x / 8), (unsigned char)(w / 8 + 1), COL_PAGE);
        a = ROWBASE(e) + x;
        bm_fill(a + 8, w, 0);
        hline(e, x + 8, w, 0);
        hline(e, x + 8, w, 1);
        bm_fill(a + w, 2, 0xC0);                  /* l'ombre du bas s'arrete avec celle de droite */
    }
}

#endif

/* ------------------------------------------------------------------ */
/* Souris et clavier                                                   */

static struct mouse_info mi;
static unsigned mouse_x;
static unsigned char mouse_y, clicked, extend, held, last_buttons;
/* Joystick : le pointeur avance d'un pixel par image, puis accelere. */
unsigned char joy_read(void);
extern unsigned char joy_p1;
#ifndef REU
#define KB_TOUCH()
#endif
static unsigned char joy_speed, joy_tick, joy_hold, hold_a2, wheel, wheel_last, wheel_t, wheel_n;
#ifdef REU
static unsigned char kb_idle, led_kb;         /* billentyuzet-LED : csend (kepek), az LED allapota */
#define KB_TOUCH() (kb_idle = 100)
static unsigned char click_hold, click_n;     /* double / triple clic : images restantes, nombre de clics */
static unsigned click_p;
#endif

static void poll_mouse(void)
{
    unsigned char j, s;
    clicked = 0;
    extend = 0;
    if (MAILBOX[0] == 2) {
        asm("sei");
        POKE(0x01, 0x35);
        MAILBOX[0] = 3;
        while (MAILBOX[0] == 3) { }
        POKE(0x01, 0x36);
        asm("cli");
    }
    if (MAILBOX[0] == 1 || MAILBOX[0] == 4) {
        mouse_x = MAILBOX[1] | (MAILBOX[2] << 8);
        mouse_y = MAILBOX[3];
        extend = MAILBOX[0] == 4;
        MAILBOX[0] = 0;
        clicked = 1;
        held = 0;
        return;
    }
    mouse_info(&mi);
    mouse_x = mi.pos.x;
    mouse_y = (unsigned char)mi.pos.y;
    j = joy_read();
    if (PEEK(0xA2) != hold_a2) { hold_a2 = PEEK(0xA2); if (joy_hold) --joy_hold;
#ifdef REU
        if (click_hold) --click_hold;
        if (kb_idle) --kb_idle;
#endif
    }
    if (joy_p1) joy_hold = 6;
#ifdef DBGKEY
    if (joy_p1) { if (joy_p1 == 4 || joy_p1 == 8) ++*(unsigned char *)0x02B9; else ++*(unsigned char *)0x02BA; }
#endif
    /* Molette de la souris USB (mode Mouse + Wheel du C64U) : impulsions gauche (haut) / droite (bas)
       sur le port 1, avec du bruit 0x1F ; elles ne bougent pas le pointeur. */
    if (joy_p1 == 4 || joy_p1 == 8 || joy_p1 == 0x1F) {
        /* une impulsion (bit 2 ou 3, avec du bruit 0x1F) = une ligne ; le nombre d'impulsions par cran
           se regle sur le C64U : Mouse Wheel Sensitivity */
        if (joy_p1 != 0x1F && joy_p1 != wheel_last) {
            wheel_last = joy_p1;
            wheel = joy_p1;
            s = PEEK(0xA2) - wheel_t;               /* acceleration : crans rapproches = plus de lignes */
            wheel_t = PEEK(0xA2);
            wheel_n = s < 3 ? 8 : s < 6 ? 4 : s < 12 ? 2 : 1;
        }
        j = 0;
    } else wheel_last = 0;
    if (!(j & 0x0F)) joy_speed = 0;
    else if (PEEK(0xA2) != joy_tick) {
        joy_tick = PEEK(0xA2);
        if (joy_speed < 48) ++joy_speed;
        s = 1 + (joy_speed >> 4);         /* 1 a 4 pixels par image */
        if (j & 4) mouse_x = mouse_x > s ? mouse_x - s : 0;
        if (j & 8) { mouse_x += s; if (mouse_x > 319) mouse_x = 319; }
        if (j & 1) mouse_y = mouse_y > s ? mouse_y - s : 0;
        if (j & 2) { mouse_y += s; if (mouse_y > 199) mouse_y = 199; }
        mouse_move(mouse_x, mouse_y);
    }
    held = (j & 0x10) || (mi.buttons & MOUSE_BTN_LEFT);
    if (held && !last_buttons) clicked = 1;
    last_buttons = held;
}


/* Un joystick branche au port 1 se lit sur les memes fils que le clavier :
   le KERNAL y voit des touches fantomes (DEL, RETURN...). On les jette tant
   que ce joystick bouge, et un instant apres. */
#ifdef KBD_HU
static unsigned char hu_key(void);
static unsigned char hu_last;
#endif

static unsigned char read_key(void)
{
#ifdef KBD_HU
    unsigned char k;
#endif
    if (joy_hold) {
#ifdef DBGKEY
        if (PEEK(198)) ++*(unsigned char *)0x02B8;     /* touche perdue : tampon purge */
#endif
        POKE(198, 0);
        return 0;
    }
#ifdef KBD_HU
    /* Tant que C= est enfoncee, on jette ce que le KERNAL a pu mettre dans
       son tampon : sa couche C= ne fait pas Shift, et notre lecture de la
       matrice pendant sa scrutation peut lui donner des touches fantomes. */
    k = hu_key();
    if (k || hu_last != 0xFF) {         /* C= tenue, ou touche lue */
        POKE(198, 0);
        return k;
    }
#endif
    return cbm_k_getin();
}

/* ------------------------------------------------------------------ */
/* Boites de dialogue                                                  */

#ifdef REU
#define MAXFILES 5                       /* a valasztoban 5 bejegyzes fer egy oldalra (fejlec-sor + 5) */
#else
#define MAXFILES 13
#endif
#define DLG_ROW  7
#define DLG_X    40
#define DLG_W    240
#define BTN_W    72

static void dialog(unsigned char rows, const char *title)
{
#ifdef REU
    panel(DLG_ROW, rows, DLG_X, DLG_W);
    if (title) put_text(DLG_X + 16, DLG_ROW + 1, title, BOLD);
#else
    unsigned char y;
    unsigned w;
    panel(DLG_ROW, rows, DLG_X, DLG_W);
    for (y = 2; y < 7; y += 2) hline(DLG_ROW, DLG_X + 8, DLG_W - 16, y);
    if (title) {
        w = (text_width(title, BOLD) + 16) & 0xFFF8u;
        clear_cells(DLG_ROW, DLG_X + (DLG_W - w) / 2, w);
        put_text(DLG_X + (DLG_W - w) / 2 + 4, DLG_ROW, title, BOLD);
    }
#endif
}

#ifdef REU
/* Gomb (3 sor: row-1, row, row+1): vilagos felso/bal es sotet also/jobb vekony vonal, atlos (45 fokos) sarok jobb felul es bal alul. A lenyomott gomb
   ugyanazt a hatterszint kapja, csak a vonalai fordulnak meg. A cellak ket szinuek: a szoveges sor szeli cellai a vonal szinet viselik. */
static void diag_cell(unsigned char r, unsigned x)
{
    unsigned char k, *p = (unsigned char *)ROWBASE(r) + x;
    for (k = 0; k < 8; ++k) p[k] = (unsigned char)(0x80 >> k);
}
static void button_w(unsigned x, unsigned char row, unsigned w, const char *label, unsigned char pressed)
{
    unsigned char c = (unsigned char)(x >> 3), n = (unsigned char)(w >> 3), i, t, b, f = COL_BAR & 15, st = w == BTN_W ? BOLD : 0;
    t = pressed ? dark_tone[theme_no] : 1;
    b = pressed ? 1 : dark_tone[theme_no];
    for (i = 0; i < 3; ++i) clear_cells((unsigned char)(row - 1 + i), x, w);
    color_cells((unsigned char)(row - 1), c, n, (unsigned char)((t << 4) | f));
    color_cells((unsigned char)(row + 1), c, n, (unsigned char)((b << 4) | f));
    color_cells(row, c, n, COL_BAR);
    color_cells(row, c, 1, (unsigned char)((t << 4) | f));
    color_cells(row, (unsigned char)(c + n - 1), 1, (unsigned char)((b << 4) | f));
    hline((unsigned char)(row - 1), x, w - 8, 0);                         /* felso vonal es bal el */
    vline((unsigned char)(row - 1), x, 0x80);
    diag_cell((unsigned char)(row - 1), x + w - 8);                       /* jobb felso sarok */
    vline(row, x, 0x80);                                                  /* szoveges sor: bal / jobb el */
    vline(row, x + w - 8, 0x01);
    diag_cell((unsigned char)(row + 1), x);                               /* bal also sarok */
    hline((unsigned char)(row + 1), x + 8, w - 8, 7);                     /* also vonal es jobb el */
    vline((unsigned char)(row + 1), x + w - 8, 0x01);
    put_text(x + (w - text_width(label, st)) / 2 + pressed, row, label, st);
}
static void button(unsigned x, unsigned char row, const char *label) { button_w(x, row, BTN_W, label, 0); }
#else
static void button(unsigned x, unsigned char row, const char *label)
{
    unsigned a = ROWBASE(row) + x;
    hline((unsigned char)(row - 1), x + 8, BTN_W - 16, 6);
    hline((unsigned char)(row + 1), x + 8, BTN_W - 16, 1);
    vline(row, x, 0x10);
    vline(row, x + BTN_W - 8, 0x08);
    bm_or(a - 320 + 7, 0x0F);
    bm_or(a - 320 + BTN_W - 8 + 7, 0xF0);
    bm_or(a + 320, 0x0F);
    bm_or(a + 320 + BTN_W - 8, 0xF0);
    put_text(x + (BTN_W - text_width(label, BOLD)) / 2, row, label, BOLD);
}

#endif

static unsigned char on_button(unsigned x, unsigned char row)
{
    return clicked && mouse_x >= x && mouse_x < x + BTN_W
        && mouse_y / 8 + 1 >= row && mouse_y / 8 <= row + 1;
}

#define BTN_OK_X     (DLG_X + 24)
#define BTN_CANCEL_X (DLG_X + DLG_W - 24 - BTN_W)

static void dialog_close(void)
{
    redraw_all();
}

static unsigned char message(const char *title, const char *line1, const char *line2, unsigned char cancel)
{
#ifdef REU
#define MSG_B 7                                 /* a gombok kozepso sora */
    dialog(10, title);
    if (line1) put_text(DLG_X + 16, DLG_ROW + 3, line1, 0);
    if (line2) put_text(DLG_X + 16, DLG_ROW + 4, line2, 0);
#else
#define MSG_B 5
    dialog(8, title);
    if (line1) put_text(DLG_X + 16, DLG_ROW + 2, line1, 0);
    if (line2) put_text(DLG_X + 16, DLG_ROW + 3, line2, 0);
#endif
    button(BTN_OK_X, DLG_ROW + MSG_B, cancel == 2 ? S_SAVEB : S_OK);                       /* 2 : Save / No save a OK / Cancel helyett */
    if (cancel) button(BTN_CANCEL_X, DLG_ROW + MSG_B, cancel == 2 ? S_NOSAVEB : S_CANCEL);
    for (;;) {
        poll_mouse();
        key = read_key();
        if (key == CR || (key == 3 && !cancel) || on_button(BTN_OK_X, DLG_ROW + MSG_B)) { dialog_close(); return 1; }
        if (cancel && (key == 3 || on_button(BTN_CANCEL_X, DLG_ROW + MSG_B))) { dialog_close(); return 0; }
    }
}

static unsigned char translate(unsigned char k);
#ifdef REU
static unsigned char ask_text(const char *title, const char *label, char *buf, unsigned char max, unsigned char petscii);
#endif

#ifndef REU
#ifdef REU
#define AT_B 9                                  /* a gombok kozepso sora */
#else
#define AT_B 5
#endif
static unsigned char ask_text(const char *title, const char *label, char *buf, unsigned char max,
                              unsigned char petscii)
{
    char *shown = inputbuf;
    unsigned char len = (unsigned char)strlen(buf), c, first = 1;
    for (;;) {
        if (first) {
#ifdef REU
            dialog(12, title);
            put_text(DLG_X + 16, DLG_ROW + 3, label, 0);
            outline_box(DLG_ROW + 4, 3, DLG_X + 16, DLG_W - 32);
#else
            dialog(8, title);
            put_text(DLG_X + 16, DLG_ROW + 2, label, 0);
#endif
            button(BTN_OK_X, DLG_ROW + AT_B, S_OK);
            button(BTN_CANCEL_X, DLG_ROW + AT_B, S_CANCEL);
            first = 0;
        }
#ifdef REU
        clear_cells(DLG_ROW + 5, DLG_X + 24, DLG_W - 48);
#else
        clear_cells(DLG_ROW + 3, DLG_X + 8, DLG_W - 16);
        hline(DLG_ROW + 3, DLG_X + 16, DLG_W - 32, 7);
#endif
        if (petscii) petscii_to_text(shown, buf); else strcpy(shown, buf);
        strcat(shown, "|");
#ifdef REU
        put_text(DLG_X + 24, DLG_ROW + 5, shown, BOLD);
#else
        put_text(DLG_X + 16, DLG_ROW + 3, shown, BOLD);
#endif
        do {
            poll_mouse();
            key = read_key();
            if (on_button(BTN_OK_X, DLG_ROW + AT_B)) key = CR;
            if (on_button(BTN_CANCEL_X, DLG_ROW + AT_B)) key = 3;
        } while (!key);
        if (key == CR && len) { dialog_close(); return 1; }
        if (key == 3) { dialog_close(); return 0; }
        if (key == 0x14) { if (len) buf[--len] = 0; continue; }
        if (len >= max) continue;
#ifdef REU
        if (at12 && len >= 12 && key != '.' && !strchr(buf, '.')) continue;   /* lemezen kiterjesztes nelkul 12 jegy, a pont meg jon */
#endif
        if (petscii) {
            if (((key >= 0x20 && key <= 0x5A) || (key >= 0xC1 && key <= 0xDA))
                && key != ',' && key != ':' && key != '*' && key != '?' && key != '"' && key != '=') {
                buf[len++] = key;
            }
        } else if ((c = translate(key)) != 0) {
            buf[len++] = c;
        }
        buf[len] = 0;
    }
}
#endif

/* ------------------------------------------------------------------ */
/* Modifications du texte                                              */

static void refresh_after(unsigned first, unsigned char all)
{
#ifdef REU
    if (quiet) return;
#endif
    if (all) draw_lines(first, top + VISIBLE - 1);
    else draw_lines(relayout_from, relayout_last);
    place_cursor();
    draw_status();
}

/* Remplace [a, b) par n octets de src (src sous les E/S si from_io).
   Renvoie 0 si le document deborderait. */
static unsigned char replace(unsigned a, unsigned b, const unsigned char *src, unsigned n,
                             unsigned char from_io)
{
    unsigned char all;
    unsigned first;
    if (doclen - (b - a) + n > MAXDOC) { message(S_FULL, 0, 0, 0); return 0; }
#ifdef REU
    if (win_base + suf_len + (doclen - (b - a) + n) > suf_end) { message(S_FULL, 0, 0, 0); return 0; }
#endif
#ifdef REU
    jr_rem(a, b - a);
#endif
    memmove(&text[a + n], &text[b], doclen - b);
    if (from_io) io_hide();
    memcpy(&text[a], src, n);
    if (from_io) io_show();
    doclen = doclen - (b - a) + n;
    dirty = 1;
#ifdef REU
    ix_inv();
    jr_log(a, b - a, n);
#endif
    first = relayout(a, b, a + n, &all);
    refresh_after(first, all);
    return 1;
}

/* Position canonique : avant les marqueurs qui suivent un caractere. */
static unsigned canonical(unsigned p)
{
    while (p && IS_MARK(text[p - 1])) --p;
    return p;
}

/* ------------------------------------------------------------------ */
/* Disque                                                              */


#ifdef REU
static void reu_move(unsigned char fetch, void *mem, dpos ra, unsigned n)
{
    unsigned k;
    while (n) {
        k = (unsigned)(0x10000UL - (ra & 0xFFFFUL));        /* 0 : banque entiere */
        if (!k || k > n) k = n;
        reu_c64 = (unsigned)mem;
        reu_addr[0] = (unsigned char)ra;
        reu_addr[1] = (unsigned char)(ra >> 8);
        reu_addr[2] = (unsigned char)(ra >> 16);
        reu_len = k;
        if (fetch) reu_fetch(); else reu_stash();
        mem = (char *)mem + k;
        ra += k;
        n -= k;
    }
}

/* Journal de la sauvegarde rapide : dans le REU, apres les 4 Ko de bloc-notes de l'impression.
   Enregistrement : position (3), octets supprimes (2), octets inseres (2), puis les octets inseres.
   Les frappes qui se suivent prolongent le dernier enregistrement. */
#define JR_BASE jr_base
#define R_BASE  r_base                  /* octets supprimes (pour annuler) */

static void jr_hdr(void)
{
    static unsigned char h[7];
    h[0] = (unsigned char)jr_lp; h[1] = (unsigned char)(jr_lp >> 8); h[2] = (unsigned char)(jr_lp >> 16);
    h[3] = (unsigned char)jr_ld; h[4] = ((unsigned char)(jr_ld >> 8) & 0x3F) | jr_lf;
    h[5] = (unsigned char)jr_li; h[6] = (unsigned char)(jr_li >> 8);
    reu_move(0, h, JR_BASE + jr_last, 7);
}

static void jr_clear(void)
{
    jr_len = jb = rl = 0;
    jr_n = redo_n = 0;
}

/* Avant une modification : les octets supprimes vont dans la banque R (annuler). */
static void jr_rem(unsigned a, unsigned n)
{
    if (n && !jr_off && rl + n <= u_cap) reu_move(0, text + a, R_BASE + rl, n);
}

static void jr_log(unsigned a, unsigned del, unsigned n)
{
    dpos g = win_base + a;
    unsigned char f = 0;
    if (jr_off || (!del && !n)) return;
    redo_n = 0;
    if (jr_blk) {                               /* operation en bloc (coller, supprimer une grande selection) */
        f = 0x80;
        if (jr_blk == 1) {
            if (jr_n && (jr_lf & 0x80)) {       /* la suite prolonge le dernier enregistrement, s'il est contigu */
                if (!n && !jr_li && g == jr_lp && jr_ld + del <= 0x3FFFu && rl + del <= u_cap) {
                    jr_ld += del;
                    rl += del;
                    jr_hdr();
                    return;
                }
                if (!del && n && jr_li + n <= 0xF000u && g == jr_lp + jr_li && jr_len + n <= u_cap) {
                    reu_move(0, text + a, JR_BASE + jr_len, n);
                    jr_len += n;
                    jr_li += n;
                    jr_hdr();
                    return;
                }
            }
            f |= 0x40;
        }
        jr_blk = 1;
    } else if (jr_n && !jr_ld && !(jr_lf & 0x80)) {
        if (!del && n && jr_li < 0xF000u && g == jr_lp + jr_li) {       /* frappe : on prolonge */
            if (jr_len + n > u_cap) { jr_ok = 0; jr_clear(); return; }
            reu_move(0, text + a, JR_BASE + jr_len, n);
            jr_len += n;
            jr_li += n;
            jr_hdr();
            return;
        }
        if (del == 1 && !n && jr_li && g + 1 == jr_lp + jr_li) {        /* retour arriere sur du texte tape */
            --jr_len;
            --jr_li;
            jr_hdr();
            return;
        }
    }
    if (jr_len + 7u + n > u_cap || rl + del > u_cap || jr_n >= l_max) { jr_ok = 0; jr_clear(); return; }
    jr_last = jr_len;
    reu_move(0, &jr_last, l_base + 3UL * jr_n, 3);              /* pile L : debut de chaque enregistrement */
    ++jr_n;
    jr_lp = g;
    jr_ld = del;
    jr_li = n;
    jr_lf = f;
    jr_hdr();
    if (n) reu_move(0, text + a, JR_BASE + jr_len + 7, n);
    jr_len += 7 + n;
    rl += del;
}

/* Taille du REU : la banque b renvoie sur la banque 0 quand b est la taille. Appele une fois, au demarrage : overlay 4. */
#pragma code-name (push, "OVERLAY9")
#pragma rodata-name (push, "OVERLAY9")
static void reu_probe_size_(void)
{
    static unsigned char probe, got;
    unsigned char b;
    probe = 0x11;
    reu_move(0, &probe, 0UL, 1);
    for (b = 1; b; b <<= 1) {
        probe = 0x22;
        reu_move(0, &probe, (dpos)b << 16, 1);
        reu_move(1, &got, 0UL, 1);
        if (got == 0x22) break;
    }
    suf_end = b ? (dpos)b << 16 : 0x1000000UL;          /* taille du REU */
    /* Zone d'annulation en haut du REU : bloc-notes (4 Ko), journal, octets supprimes, pile L, pile refaire.
       REU de 2 Mo ou plus : 256 Ko de journal ; plus petit : 32 Ko. */
    if (suf_end >= 0x200000UL) { u_cap = 0x40000UL; l_max = 40000u; d_max = 4096u; suf_end -= 0xF0000UL; }
    else { u_cap = 0x4000UL; l_max = 2500u; d_max = 256u; suf_end -= 0x20000UL; }
    c_cap = u_cap;
    jr_base = suf_end + 0x1000UL;
    r_base = jr_base + u_cap;
    l_base = r_base + u_cap;
    d_base = l_base + 3UL * l_max;
    c_base = d_base + d_max;                    /* presse-papiers */
    ix_base = c_base + c_cap;                   /* sorszam-index (a fenntartott resz vegen) */
    ix_max = suf_end >= 0x200000UL && u_cap > 0x4000UL ? (unsigned)((suf_end + 0xF0000UL - ix_base) >> 3) : 0;
    if (!ix_max) ix_go = 0;
}
#pragma rodata-name (pop)
#pragma code-name (pop)
static void reu_probe_size(void) { ovl_load(9); reu_probe_size_(); }

/* Entrees-sorties de fichier (canal 2 du disque, ou la SD) : sd_on choisit. */
static unsigned char sdk(unsigned char e) { if (e) sd_bad = e; return e; }
static unsigned char fo_r(const char *name)     /* ouvre en lecture ; name : nom PETSCII (disque) */
{
    if (sd_on) return sdk(sd_open(1));
    strcpy(cmdname, name);
    strcat(cmdname, ",S,R");
    return cbm_open(2, dev, 2, cmdname);
}
static unsigned char fo_w(void)                 /* ouvre en ecriture (cmdname prepare par fit_check) */
{
    if (sd_on) return sdk(sd_open(0x0A));            /* FA_WRITE | FA_CREATE_ALWAYS : felulirja (0x04, FA_CREATE_NEW, letezo fajlnal hibat ad) */
    return cbm_open(2, dev, 2, cmdname);
}
static int fr(void *b, unsigned n)
{
    if (!sd_on) return cbm_read(2, b, n);
    SD_PTR = (unsigned)b;
    SD_LEN = n;
    sdk(sd_read());
    return SD_LEN;
}
static void fw(const void *b, unsigned n)
{
    if (!sd_on) { cbm_write(2, b, n); return; }
    SD_PTR = (unsigned)b;
    SD_LEN = n;
    sdk(sd_write());
}
static void fc(void) { if (sd_on) sdk(sd_close()); else cbm_close(2); }
/* Van-e formazas (stilusjel) a dokumentumban ? A first_mark csak az ablakbol mar kilepett / betoltott reszt tartja nyilvan, ezert az ablak szoveget is atnezzuk. */
static unsigned char has_fmt(void)
{
    unsigned i;
    if (first_mark != 0xFFFFFFFFUL) return 1;
    for (i = 0; i < doclen; ++i) if (IS_MARK(text[i])) return 1;
    return 0;
}


/* Ecrit sur le canal 2 n octets du REU a partir de ra, par morceaux de 256. */
static void write_reu(dpos ra, dpos n)
{
    unsigned k;
    while (n) {
        k = n > 256 ? 256 : (unsigned)n;
        reu_move(1, outbuf, ra, k);
        fw(outbuf, k);
        pg_add(k);
        ra += k;
        n -= k;
    }
}
#endif

static unsigned char disk_status(void)
{
    unsigned char n;
    int r;
    diskmsg[0] = 0;
#ifdef REU
    if (sd_on) { n = sd_bad; sd_bad = 0; strcpy(diskmsg, "SD ERROR"); return n; }
#endif
    if (cbm_open(15, dev, 15, "")) return 99;
    r = cbm_read(15, diskmsg, 40);
    cbm_close(15);
    if (r <= 0) return 99;
    n = (unsigned char)r;
    while (n && (diskmsg[n - 1] == CR || diskmsg[n - 1] == '\n')) --n;
    diskmsg[n] = 0;
    return (unsigned char)((diskmsg[0] - '0') * 10 + diskmsg[1] - '0');
}

static void show_disk_error(void)
{
    char *shown = (char *)outbuf;             /* libre hors impression */
    petscii_to_text(shown, diskmsg);
    message(S_DISKERR, shown, 0, 0);
}

#ifdef REU
/* Repertoire : noms des fichiers SEQ (sous les E/S), blocs libres, taille du
   fichier "find" s'il existe deja. Blocs libres inconnus (pas de disque) : 0xFFFF. */
#ifdef RTF
static unsigned char doc_fmt;                   /* format du document : 0 Plume (.plm), 1 RTF, 2 texte */
static unsigned char rtxt, rtf_txt;             /* lecture / ecriture en texte seul (rtxt : load_rtf, rtf_txt : rtf_body) */
static unsigned char sd_sel, fmt_sel, pk_btn, pk_save, pk_fm;   /* fichier : resultat du choix ; boutons du selecteur */
/* Le code RTF vit dans l'overlay 1 (une banque de la cartouche) : ces points d'entree le chargent d'abord. */
static void rtf_open(void);
static void txt_open(void);
static unsigned char rtf_write(char *name);
static unsigned char txt_write(char *name);
#endif
/* A Plume sajat .bck kisero fajljat a valasztoban nem mutatjuk (PETSCII es ASCII nevre is jo: a kis- es nagybetu azonos). */
static unsigned char is_bck(const char *n)
{
    unsigned char l = (unsigned char)strlen(n);
    return l > 4 && n[l - 4] == '.' && (n[l - 3] & 0x5F) == 0x42 && (n[l - 2] & 0x5F) == 0x43 && (n[l - 1] & 0x5F) == 0x4B;
}
static unsigned blk_free, blk_old, fsize[MAXFILES];
static unsigned char nfiles, fmore;
static unsigned fpage;                          /* a lista elejen atugrott bejegyzesek szama (gorgo: 1-gyel, nyil: egy oldallal) */

/* Page fpage de MAXFILES noms ; fmore : il y en a d'autres apres. */
static void scan_dir(const char *find)
{
    struct cbm_dirent e;
    unsigned char r;
    unsigned tot = 0, skip = fpage;
    nfiles = fmore = 0;
    blk_free = 0xFFFF;
    blk_old = 0;
    if (cbm_opendir(1, dev)) return;
    for (;;) {
        r = cbm_readdir(1, &e);
        if (r == 2) { blk_free = e.size; break; }
        if (r) break;
        if (e.type != CBM_T_SEQ) continue;
        if (find && !strcmp(e.name, find)) blk_old = e.size;
        if (pk_btn && is_bck(e.name)) continue;    /* csak a Plume valasztojaban (Open / Save as) */
        if (tot >= skip && nfiles < MAXFILES) { fsize[nfiles] = e.size; io_hide(); strcpy(names[nfiles++], e.name); io_show(); }
        ++tot;
    }
    cbm_closedir(1);
    fmore = tot > skip + nfiles;
}

#pragma code-name (push, "OVERLAY7")
#pragma rodata-name (push, "OVERLAY7")
#ifdef REU
#pragma rodata-name (pop)
#pragma code-name (pop)
#pragma code-name (push, "OVERLAY9")
#pragma rodata-name (push, "OVERLAY9")
/* A (pos, line, st) allapotot egy darab szoveggel (max. 480 bajt a dokumentumbol) elorevisszuk teljes sorokkal:
   amig pos >= limit, vagy a darab el nem fogy (a darab vegen 300 bajtnyi biztonsagi sav marad, hogy a sorok egeszben
   legyenek benne). rec: ellenorzo pontok rogzitese 2 KB-onkent. */
static void ix_adv(dpos *pp, dpos *pl, unsigned char *ps, dpos limit, unsigned char rec)
{
    dpos pos = *pp, line = *pl, N = win_base + doclen + suf_len, e[2];
    unsigned n, p = 0;
    unsigned char st = *ps;
    if (pos >= N) { *pp = limit; return; }
    n = N - pos > 480 ? 480 : (unsigned)(N - pos);
    doc_read(outbuf, pos, n);
    nl_base = (unsigned)outbuf;
    nl_len = n;
    for (;;) {
        if (pos + p >= limit) break;
        if (n - p < 300 && pos + n < N) break;
        if (rec && ix_n < ix_max && pos + p >= ((dpos)ix_n << 11)) {
            e[0] = pos + p;
            e[1] = line | ((dpos)st << 24);
            reu_move(0, e, ix_base + ((dpos)ix_n << 3), 8);
            ++ix_n;
        }
        nl_pos = p;
        nl_style = st;
        p = next_line_buf();
        st = nl_style;
        ++line;
    }
    *pp = pos + p;
    *pl = line;
    *ps = st;
}

/* Egy ellenorzo pont (k. bejegyzes) betoltese. */
static void ix_get(unsigned k, dpos *pos, dpos *line, unsigned char *st)
{
    dpos e[2];
    reu_move(1, e, ix_base + ((dpos)k << 3), 8);
    *pos = e[0];
    *line = e[1] & 0xFFFFFFUL;
    *st = (unsigned char)(e[1] >> 24);
}

/* Hatterben, ures jaratban: (1) az ablak elotti sorok szamat pontossa teszi, ha az index mar eleri az ablakot;
   (2) tovabbepiti az indexet a dokumentum vegeig. Egy hivas kb. 200 karaktert dolgoz fel. */
static void ix_tick_(void)
{
    dpos pos, line, N = win_base + doclen + suf_len;
    unsigned char st;
    unsigned k = (unsigned)(win_base >> 11);
    if (!ix_max) { ix_go = 0; return; }
    if (!lines_exact && k < ix_n) {                     /* az index eleri az ablak elejet : a sorszam pontos lehet */
        pos = 0; line = 0; st = 0;
        if (k) {
            ix_get(k, &pos, &line, &st);
            if (pos > win_base) ix_get(k - 1, &pos, &line, &st);
        }
        while (pos < win_base) ix_adv(&pos, &line, &st, win_base, 0);
        if (pos == win_base) { lines_before = line; lines_exact = 1; shown_dirty = 255; draw_status(); }
        else ix_go = 0;                                 /* (nem lehetseges) nem probaljuk ujra */
        return;
    }
    if (!ix_st) {
        ix_pos = ix_line = 0; ix_sty = 0;
        if (ix_n) ix_get(ix_n - 1, &ix_pos, &ix_line, &ix_sty);
        ix_st = 1;
    }
    if (ix_pos >= N) {                                  /* az index kesz : az osszes sor szama */
        outbuf[0] = CR;
        if (N) doc_read(outbuf, N - 1, 1);              /* (ures dokumentum : 1 sor) */
        ix_tot = ix_line + (outbuf[0] == CR);           /* a vegi CR utan meg egy ures sor van */
        shown_dirty = 255;
        draw_status();
        ix_go = 0;
        return;
    }
    if (ix_n >= ix_max) { ix_go = 0; return; }
    ix_adv(&ix_pos, &ix_line, &ix_sty, N, 1);
}
#pragma rodata-name (pop)
#pragma code-name (pop)
#pragma code-name (push, "OVERLAY7")
#pragma rodata-name (push, "OVERLAY7")
#endif
static unsigned char sd_scan_(unsigned char cap);
static unsigned char fgrey(unsigned char i);
static void lp_tail(char *d);
static unsigned char lp_isroot(void);
/* Liste des fichiers en deux colonnes + blocs libres. Renvoie l'index choisi,
   254 pour "Nouveau..." (si newbtn), 255 si annule. */
#define RQ_X 16                         /* fajlvalaszto: x, szelesseg (288 kepont), 3. sortol a 20 soros keret */
#define RQ_W 288
static const unsigned char arrow_up[8] = { 0x18, 0x3C, 0x7E, 0x18, 0x18, 0x18, 0x18, 0x18 };

static void arrow_draw(unsigned char row, unsigned char up)
{
    unsigned char k;
    for (k = 0; k < 8; ++k) bm_or(ROWBASE(row) + RQ_X + RQ_W - 24 + k, arrow_up[up ? k : 7 - k]);
}

/* A lista kijelolt sora : elkulonulo (a lap szineinek megfordito) hatter. */
static void fhl(unsigned char i, unsigned char on)
{
    color_cells((unsigned char)(12 + i), 4, 31, on ? COL_BAR : COL_PAGE);
}

static unsigned char pick_file_(const char *title, unsigned char newbtn)
{
    unsigned char i, sel, cur = 0;
    char *shown = inputbuf, *b = (char *)outbuf;
    {
        sel = 255;
        outline_box(10, 8, 24, 272);            /* a lista: fejlec-sor (utvonal / szabad blokk) + a bejegyzesek */
        if (sd_pk) lp_tail(b);
        else if (blk_free == 0xFFFF) { strcpy(b, S_NODISK); utoa_simple(b + sizeof(S_NODISK) - 1, dev); }
        else strcpy(utoa_simple(b, blk_free), S_BLKFREE);
        put_text(32, 11, b, BOLD);
        for (i = 0; i < nfiles; ++i) {          /* meret jobbra igazitva (DIR / blokk / K), aztan a nev */
            if (fsize[i] == 0xFFFF) strcpy(b, "DIR");
            else if (fsize[i] == 0xFFFD) strcpy(b, "?");
            else { b = utoa_simple(b, fsize[i] & 0x3FFF); if (sd_pk) *b++ = "KMG"[fsize[i] >> 14]; *b = 0; b = (char *)outbuf; }
            put_text(64 - text_width(b, 0), (unsigned char)(12 + i), b, 0);
            io_hide();
            strcpy(shown, names[i]);
            io_show();
            petscii_to_text(shown, shown);
            put_text(72, (unsigned char)(12 + i), shown, 0);
            if (fgrey(i)) color_cells((unsigned char)(12 + i), 4, 31, (unsigned char)((dark_tone[theme_no] << 4) | (COL_PAGE & 15)));   /* szurke: nem valaszthato */
        }
        if (fpage) arrow_draw(12, 1);
        if (fmore) arrow_draw(16, 0);
        if (nfiles && !fgrey(0)) fhl(0, 1);
        while (sel == 255) {
            poll_mouse();
            key = read_key();
            if (key == 3 || on_button(RQ_X + RQ_W - 24 - BTN_W, 20)) break;
            if (key == 0x11 || key == 0x91) {                   /* nyilak : a kijeloles lep (a lista vegen lapoz) */
                i = key == 0x11 ? cur + 1 : cur - 1;
                if (i < nfiles) {
                    if (!fgrey(i)) { fhl(cur, 0); cur = i; fhl(i, 1); }
                    key = 0;
                }
            }
            if ((key == ' ' || (key == CR && !newbtn)) && nfiles && !fgrey(cur)) sel = cur;
            if (newbtn && (key == CR || on_button(RQ_X + 24, 20))) sel = 254;
            if (pk_btn && clicked && mouse_y / 8 >= 6 && mouse_y / 8 <= 8) {
                if (sd_ok && mouse_x >= 24 && mouse_x < 72) sel = 249;
                else if (sd_ok && mouse_x >= 80 && mouse_x < 128) sel = 248;
                else if (pk_save && mouse_x >= 136 && mouse_x < 184) sel = 245;
                else if (pk_save && mouse_x >= 192 && mouse_x < 240) sel = 246;
                else if (pk_save && mouse_x >= 248 && mouse_x < 296) sel = 247;
            }
            if (clicked && mouse_y / 8 >= 12 && mouse_y / 8 <= 16) {
                i = (unsigned char)(mouse_y / 8 - 12);
                if (mouse_x >= RQ_X + RQ_W - 32 && (i == 0 || i == MAXFILES - 1) && (fpage || fmore)) {
                    key = i ? 0x11 : 0x91;      /* fleche cliquee : page suivante / precedente */
                } else if (i < nfiles && !fgrey(i)) sel = i;
            }
            if ((key == 0x11 && fmore) || (key == 0x91 && fpage)) { sel = 253; }
            if (wheel) {                        /* egergorgo: a lista egy bejegyzesenkent (gyors forgatasra tobbel) mozog */
                if (wheel == 4) fpage = fpage > wheel_n ? fpage - wheel_n : 0;
                else if (fmore) fpage += wheel_n;
                wheel = wheel_n = 0;
                POKE(198, 0);                   /* a gorgo impulzusai billentyunek latszanak : a puffer urites */
                joy_hold = 6;
                sel = 252;
            }
        }
        if (sel < nfiles) { fhl(cur, 0); fhl(sel, 1); }     /* a valasztott sor kiemelve marad, amig betolt */
        if (sel == 253) fpage = key == 0x11 ? fpage + MAXFILES : (fpage > MAXFILES ? fpage - MAXFILES : 0);
        return sel;                             /* 252 / 253 : a hivo ujraolvassa a listat */
    }
}

/* ---- Fajlvalaszto a SD-kartyan (UCI) ---- */
/* SD-nev (ASCII / UTF-8) -> PETSCII megjelenitesre, legfeljebb n jel (d: n+1 bajt) ; ha hosszabb, kozepen "~"-vel rovidul (a kiterjesztes lathato marad). */
#define sd_tmp ((char *)outbuf + 192)           /* atmeneti nev, 64 bajt */
static void sd_conv(char *d, const unsigned char *s, unsigned char n)
{
    unsigned char c, i = 0, h, t;
    while ((c = *s++) != 0 && i < 63) {
        if (c >= 0x80 && c < 0xC0) continue;            /* UTF-8 folytatobajt */
        if (c >= 'a' && c <= 'z') c -= 0x20;
        else if (c >= 'A' && c <= 'Z') c += 0x80;
        else if (c >= 0xC0) c = '?';
        sd_tmp[i] = (char)c;                            /* (nem sd_tmp[i++] : a cc65 az allando cimu tombnel a novelest az indexeles elott vegzi) */
        ++i;
    }
    sd_tmp[i] = 0;
    if (i <= n) { strcpy(d, sd_tmp); return; }
    h = (unsigned char)((n - 1) / 2);
    t = (unsigned char)(n - 1 - h);
    memcpy(d, sd_tmp, h);
    d[h] = '~';
    strcpy(d + h + 1, sd_tmp + i - t);
}

/* A 15 M-nel nagyobb SD-fajl nem fer a dokumentumba: szurke, nem valaszthato. */
static unsigned char fgrey(unsigned char i)
{
    unsigned f = fsize[i];
    return sd_pk && f != 0xFFFF && ((f >> 14) >= 2 || ((f >> 14) == 1 && (f & 0x3FFF) > 15));
}

/* A fpage. oldal: names[] / fsize[] (0xFFFF: konyvtar). cap: ennek az indexnek a nyers neve SD_NAME-be, attributuma SD_ATT-be.
   Vissza: 0, ha a konyvtar nem olvashato. */
static unsigned char sd_scan_(unsigned char cap)
{
    unsigned tot = 0, skip = fpage;
    unsigned char r, hf = 0, ab = 0, root;
    nfiles = 0;
    fmore = 0;
    r = sd_dirbeg();
    if (r > 1) return 0;                        /* 1 : ures konyvtar (01,DIRECTORY EMPTY) */
    root = lp_isroot();
    if (!root) {                                /* ".." : fel */
        if (!skip) {
            io_hide();
            strcpy(names[0], "..");
            io_show();
            fsize[0] = 0xFFFF;
            if (cap == 0) { strcpy(sd_cand, ".."); sd_ca = 0x10; }
            nfiles = 1;
        }
        ++tot;
    }
    while (!r && !sd_dirnext()) {
        if (!stricmp((char *)SD_ENT + 1, "ftp")) hf = 1;
        if (is_bck((char *)SD_ENT + 1)) continue;
        if (tot >= skip && nfiles < MAXFILES) {
            io_hide();
            sd_conv(names[nfiles], SD_ENT + 1, 24);
            strncpy(rawn[nfiles], (char *)SD_ENT + 1, 63);
            rawn[nfiles][63] = 0;
            io_show();
            fsize[nfiles] = (SD_ENT[0] & 0x10) ? 0xFFFF : 0;
            if (strlen((char *)SD_ENT + 1) > 63) fsize[nfiles] = 0xFFFD;      /* tul hosszu nev : nem nyithato meg (szurke) */
            if (nfiles == cap) { strncpy(sd_cand, (char *)SD_ENT + 1, 63); sd_cand[63] = 0; sd_ca = SD_ENT[0]; }
            ++nfiles;
        }
        ++tot;
        if (tot > skip + MAXFILES) { fmore = ab = 1; break; }        /* van meg az oldal utan : a tobbit nem olvassuk */
        if (cap != 255 && nfiles > cap) { ab = 1; break; }           /* a kijelolt bejegyzes megvan */
    }
    if (ab) sd_abort();
    if (root && !hf && !ab) {                   /* a gyokerben az /ftp/ (bemountolt FTP-szerverek) nincs a listaban, de belepni lehet */
        if (tot >= skip && nfiles < MAXFILES) {
            io_hide();
            strcpy(names[nfiles], "ftp");
            io_show();
            fsize[nfiles] = 0xFFFF;
            if (nfiles == cap) { strcpy(sd_cand, "ftp"); sd_ca = 0x10; }
            ++nfiles;
        }
        ++tot;
    }
    if (!ab) fmore = tot > skip + nfiles;
    return 1;
}

/* ---- A hosszu SD-utvonal (legfeljebb 255 karakter) az I/O alatti RAM-ban (LPATH). Az SD_PATH (64 bajt) a tukre, ha elfer benne
   (a modul ilyenkor minden muvelet elott magatol belep a mappaba) ; ha nem, mappankent lepunk be (relativ CHANGE_DIR),
   es az Ultimate munkamappajara tamaszkodunk (SD_PATH ures). ---- */
#define LPATH    ((char *)0xDBC2)
#define LPATH_SV ((char *)0xDCC2)
static void lp_sync(void)
{
    unsigned char c, i, j;
    io_hide();
    if (strlen(LPATH) <= 63) { strcpy(SD_PATH, LPATH); io_show(); return; }
    io_show();
    SD_PATH[0] = '/';
    SD_PATH[1] = 0;
    sd_chdir();
    for (i = 1;;) {
        io_hide();
        for (j = 0; (c = (unsigned char)LPATH[i]) != 0 && c != '/'; ++i) { SD_PATH[j] = (char)c; ++j; }     /* (nem SD_PATH[j++] : cc65 hiba) */
        SD_PATH[j] = 0;
        if (c) ++i;
        io_show();
        if (!j) break;
        sd_chdir();
        if (!c) break;
    }
    SD_PATH[0] = 0;
}
static unsigned char lp_isroot(void)
{
    unsigned char r;
    io_hide();
    r = LPATH[1] == 0;
    io_show();
    return r;
}
static void lp_root(void)
{
    io_hide();
    strcpy(LPATH, "/");
    io_show();
    lp_sync();
}
/* "mappa" belepes (sd_cand) / ".." kilepes */
static void sd_enter(void)
{
    char *e;
    io_hide();
    if (!strcmp(sd_cand, "..")) {
        e = strrchr(LPATH, '/');
        if (e == LPATH) LPATH[1] = 0; else if (e) *e = 0;
    } else {
        e = LPATH + strlen(LPATH);
        if ((unsigned)(e - LPATH) + strlen(sd_cand) > 253) { io_show(); return; }
        if (LPATH[1]) *e++ = '/';
        strcpy(e, sd_cand);
    }
    io_show();
    lp_sync();
}
/* A fejlec: az utvonal vege (hosszu utvonalnal ~-vel kezdve). */
static void lp_tail(char *d)
{
    unsigned n;
    io_hide();
    n = strlen(LPATH);
    if (n <= 40) strcpy(d, LPATH);
    else { d[0] = '~'; strcpy(d + 1, LPATH + n - 39); }
    io_show();
}
/* A rezidens kod (choose, rq_list, indulas) szamara : 0 torles, 1 mentes, 2 visszaallitas, 3 gyoker ha meg nincs, 4 gyoker, 5 gyoker-e. */
static unsigned char lp_op_(unsigned char op)
{
    unsigned char r = 0;
    io_hide();
    if (op == 0) LPATH[0] = 0;
    else if (op == 1) memcpy(LPATH_SV, LPATH, 256);
    else if (op == 2) memcpy(LPATH, LPATH_SV, 256);
    else if (op == 5) r = LPATH[1] == 0;
    else if (op == 3 && LPATH[0]) op = 0;
    io_show();
    if (op == 2) lp_sync();
    else if (op == 3 || op == 4) lp_root();
    return r;
}

/* Fajlvalaszto, lemez es SD egyutt. save: formatum-kapcsolok es "Uj..." gomb.
   Vissza: 0 megszakitva (az SD utvonal visszaall), 1 meglevo fajl, 2 uj nev. Eredmeny: sd_sel (hely), fmt_sel (formatum),
   filename (megjeleno nev, PETSCII), SD-n SD_NAME (nyers nev). */
/* A fajlvalaszto lepesei (a vezerlest a programban levo choose() vegzi, mert a valaszto es a jovahagyas kulon overlayekben van). */
static unsigned char rq_scan_(void)
{
    if (sd_pk) return sd_scan_(255);
    scan_dir(0);
    return 1;
}
/* Egy bejegyzes kivalasztasa. Vissza: 1 fajl (a megjeleno nev az outbuf+64-ben), 2 mappa (belepett), 3 tul hosszu nev. */
static unsigned char rq_sel_(unsigned char sel)
{
    char *buf = (char *)outbuf + 64;
    if (sd_pk) {
        sd_scan_(sel);
        if (sd_ca & 0x10) { sd_enter(); fpage = 0; return 2; }
        sd_conv(buf, (unsigned char *)sd_cand, 16);
    } else {
        io_hide();
        strcpy(buf, names[sel]);
        io_show();
    }
    return 1;
}
static void rq_title_(void)
{
    sd_conv(sd_title, (unsigned char *)SD_NAME, 26);
    petscii_to_text(sd_title, sd_title);
}
#pragma rodata-name (pop)
#pragma code-name (pop)
static void rq_sizes_(void);
static unsigned char file_commit_(const char *title, unsigned char save, unsigned char newname);
/* Lista kirajzolasa utani ujraolvasas (gorgetes / lapozas utan is): beolvas, a tulgorgetest visszaigazitja, az SD-fajlok meretet lekerdezi. */
static unsigned char lp(unsigned char op) { ovl_load(7); return lp_op_(op); }
static void rq_list(void)
{
    if (sd_pk) disk_busy(1);                    /* a C64U lassu lehet : homokora a nyil helyett */
    for (;;) {
        ovl_load(7);
        if (!rq_scan_()) {                      /* az SD-n a konyvtar nem olvashato : a gyokerre, vegul a lemezre esunk vissza */
            if (!lp(5)) lp(4);
            else { disk_busy(0); message(S_DISKERR, S_NOSD, 0, 0); sd_pk = 0; }
            pk_first = 1;
            continue;
        }
        if (nfiles < MAXFILES && fpage) {       /* tulgorgetve : az utolso teljes oldal */
            fpage = fpage + nfiles >= MAXFILES ? fpage + nfiles - MAXFILES : 0;
            continue;
        }
        break;
    }
    if (sd_pk) { ovl_load(4); rq_sizes_(); disk_busy(0); }
}
/* A lemezes torles-valaszto (free_space): csak lemez, meretlekeres nelkul. */
static void ix_tick(void) { ovl_load(9); ix_tick_(); }
static void pick_frame_(const char *title, unsigned char newbtn);
static unsigned char pick_run(const char *title, unsigned char newbtn)
{
    if (pk_first) { ovl_load(4); pick_frame_(title, newbtn); pk_first = 0; }
    ovl_load(7);
    return pick_file_(title, newbtn);
}
static unsigned char pick_file(const char *title, unsigned char newbtn)
{
    unsigned char r;
    pk_first = 1;
    for (;;) {
        r = pick_run(title, newbtn);
        if (r != 252 && r != 253) return r;
        scan_dir(0);
    }
}
/* A fajlvalaszto (Open / Save as): az allapotgep. Vissza: 0 megszakitva, 1 meglevo fajl, 2 uj nev. */
static unsigned char choose(const char *title, unsigned char save)
{
    unsigned char r, nw;
    char *sv = (char *)outbuf + 256;
    lp(1);                                      /* az utvonal mentese (megszakitasnal visszaall) */
    sv[64] = (char)dev;                         /* megszakitasnal az egyseg visszaall */
    sd_pk = sd_ok && (sd_on || !filename[0]);   /* a dokumentum helye ; uj dokumentum : az SD, ha van */
    pk_fm = filename[0] ? doc_fmt : 1;          /* a dokumentum formatuma ; meg nincs : RTF */
    pk_btn = 1;
    pk_save = save;
    if (sd_pk) lp(3);                           /* elso hasznalat: a gyoker (SD, USB0, ...) ; utana az utoljara hasznalt mappa */
    fpage = 0;
    pk_first = 1;
    for (;;) {
        dlabel[1] = dev >= 10 ? '1' : ' ';              /* "D 8" .. "D11" */
        dlabel[2] = (char)('0' + dev % 10);
        rq_list();
        r = pick_run(title, save);
        if (r == 252 || r == 253) continue;     /* gorgo / lapozas : ujraolvasas, csak a lista rajzolodik ujra */
        if (r == 255) break;
        pk_first = 1;
        if (r >= 245 && r <= 249) {             /* kapcsolo */
            if (r == 249 && !sd_pk) {            /* a lenyomott Disk ujra : a kovetkezo mukodo egyseg (8..11) */
                sv[65] = (char)dev;
                for (nw = 0; nw < 3; ++nw) {
                    dev = dev < 11 ? dev + 1 : 8;
                    r = cbm_opendir(1, dev) ? 1 : 0;
                    cbm_closedir(1);                           /* a sikertelen megnyitas utan is : a logikai fajl ne maradjon nyitva */
                    if (!r) break;
                }
                if (nw == 3) dev = (unsigned char)sv[65];      /* nincs masik egyseg : marad */
                r = 249;
            }
            if (r >= 248) { sd_pk = (r == 248); if (sd_pk) lp(3); }
            else pk_fm = r - 245;
            fpage = 0;
            pk_first = 2;                       /* csak a gombsor es a lista rajzolodik ujra */
            continue;
        }
        nw = r == 254;
        if (nw) {                               /* uj nev */
            ((char *)outbuf)[64] = 0;
            at12 = !sd_pk;
            r = ask_text(title, S_FILENAME, (char *)outbuf + 64, sd_pk ? 24 : 16, 1);
            at12 = 0;
            if (!r) continue;                   /* megszakitva : vissza a listahoz */
        } else {
            ovl_load(7);
            r = rq_sel_(r);
            if (r == 2) { pk_first = 0; continue; }                            /* mappa : belepes (csak a lista rajzolodik ujra) */
            if (r == 3) { message(title, S_SDLONG, 0, 0); continue; }
        }
        ovl_load(9);
        if (!file_commit_(title, save, nw)) continue;
        if (sd_pk) { ovl_load(7); rq_title_(); }
        pk_btn = sd_pk = 0;
        return nw ? 2 : 1;
    }
    lp(2);
    dev = (unsigned char)sv[64];
    pk_btn = sd_pk = 0;
    return 0;
}


/* Pas assez de place : on laisse effacer un fichier du disque, ou changer de disque (Annuler dans la
   liste), puis fit_check recompte. Sauver ne doit jamais devenir impossible. */
static void free_space(void)
{
    unsigned char sel;
    char *n = (char *)outbuf + 64;
    fpage = 0;
    scan_dir(0);
    if (nfiles) {
        sel = pick_file(S_DELPICK, 0);
        if (sel < nfiles) {
            io_hide();
            strcpy(n, names[sel]);
            io_show();
            petscii_to_text((char *)outbuf + 100, n);
            if (message(S_DELPICK, (char *)outbuf + 100, S_DELQ, 1)) {
                strcpy(cmdname, "S0:");
                strcat(cmdname, n);
                cbm_open(15, dev, 15, cmdname);
                cbm_close(15);
            }
        }
    }
    redraw_all();
}

/* Verifie qu'un fichier de "bytes" octets tient sur le disque et prepare cmdname.
   Assez de place : "@0:" (remplace sans risque). Sinon, si l'ancien fichier du meme
   nom libere assez, apres confirmation il est efface d'abord. Sinon on propose d'effacer
   un autre fichier ou de changer de disque. */
static unsigned char fit_check(dpos bytes, const char *name)
{
    dpos nb = (bytes + 253) / 254;
    unsigned need = nb > 60000UL ? 60000u : (unsigned)nb;
    char *b;
    if (sd_on) return 1;
    for (;;) {
        scan_dir(name);
        strcpy(cmdname, "@0:");
        if (need <= blk_free) break;
        if (blk_old && need <= blk_free + blk_old) {
            if (!message(S_SAVEAS, S_REPL1, S_REPL2, 1)) return 0;
            strcpy(cmdname, "S0:");
            strcat(cmdname, name);
            cbm_open(15, dev, 15, cmdname);
            cbm_close(15);
            strcpy(cmdname, "0:");
            break;
        }
        b = (char *)outbuf;
        strcpy(b, S_NEEDFREE);
        b = utoa_simple(b + strlen(b), need);
        *b++ = '/';
        utoa_simple(b, blk_free);
        if (!message(S_SAVEAS, S_NOROOM, (char *)outbuf, 1)) return 0;
        free_space();
    }
    strcat(cmdname, name);
    strcat(cmdname, ",S,W");
    return 1;
}
#else
static unsigned char ask_filename(void)
{
    char *buf = (char *)outbuf + 64;          /* libre hors impression */
    strcpy(buf, filename);
    if (!ask_text(S_SAVEAS, S_FILENAME, buf, 16, 1)) return 0;
    strcpy(filename, buf);
    return 1;
}
#endif

/* Format PLM2 : "PLM2", longueur (2 octets), texte avec marqueurs. */
#ifdef REU
/* save_doc et load_named sont dans l'overlay 6 (RAM) : les versions residentes le chargent, puis font les
   appels qui chargent un autre overlay (RTF, bck_saved) une fois revenues. */
#pragma code-name (push, "OVERLAY6")
#pragma rodata-name (push, "OVERLAY6")
static unsigned char save_doc_(void)
#define SD_R(v) return v
#else
static void save_doc(void)
#define SD_R(v) return
#endif
{
#ifdef REU
    unsigned char head[8], hl = 6;
#define SAVELEN (doclen + win_base + suf_len)   /* tout le document, pas seulement la fenetre */
#define HEADLEN hl
#else
    unsigned char head[6];
#define SAVELEN doclen
#define HEADLEN 6
#endif
#ifdef RTF
    if (doc_fmt && filename[0]) SD_R(doc_fmt == 1 ? 1 : 3);   /* RTF / texte : meme format, meme endroit */
#endif
#ifndef REU
    if (!filename[0] && !ask_filename()) SD_R(0);
#endif
    memcpy(head, "PLM2", 4);
    head[4] = (unsigned char)SAVELEN;
    head[5] = (unsigned char)(SAVELEN >> 8);
#ifdef REU
    if (SAVELEN > 65535UL) {                    /* PLM3 : longueur sur 4 octets */
        memcpy(head, "PLM3", 4);
        head[6] = (unsigned char)(SAVELEN >> 16);
        head[7] = (unsigned char)(SAVELEN >> 24);
        hl = 8;
    }
    if (!fit_check(SAVELEN + hl, filename)) SD_R(0);
    pg_begin(S_SAVING, SAVELEN);
#else
    strcpy(cmdname, "@0:");
    strcat(cmdname, filename);
    strcat(cmdname, ",S,W");
#endif
    disk_busy(1);
    if (fo_w() == 0) {
        fw(head, HEADLEN);
#ifdef REU
        write_reu(0UL, win_base);
#endif
        fw(text, doclen);
#ifdef REU
        pg_add(doclen);
        write_reu(suf_end - suf_len, suf_len);
#endif
        fc();
    }
    disk_busy(0);
#ifdef REU
    pg_end();
#endif
    if (disk_status() >= 20) { show_disk_error(); SD_R(0); }
    dirty = 0;
    shown_dirty = 255;                          /* le titre (le nom) se redessine toujours */
    draw_status();
    SD_R(2);
}
#ifdef REU
#pragma rodata-name (pop)
#pragma code-name (pop)
#ifdef RTF
static void save_as(void);
static void txt_strip(void);
static unsigned char file_kind(void);          /* 0 PLM, 1 RTF, 2 szoveg, 3 bináris fajlnak tunik */
#endif
static void save_doc(void)
{
    unsigned char r;
#ifdef RTF
    if (!filename[0]) { save_as(); return; }                /* nouveau document : le selecteur (format, disque ou SD) */
#endif
    ovl_load(6);
    r = save_doc_();
    if (r == 1) rtf_write(filename);
    else if (r == 3) {                          /* texte seul : la mise en forme se perd (avertissement), puis disparait de l'ecran */
        r = has_fmt();
        if (r && !message(S_SAVETXT, S_TXTLOSS, S_TXTASK, 1)) return;
        txt_write(filename);
        if (op_ok && r) txt_strip();
    }
    else if (r == 2) bck_saved();
}
#endif

#ifdef REU
#pragma code-name (push, "OVERLAY8")
#pragma rodata-name (push, "OVERLAY8")
#endif
static void new_doc_(void)
{
#ifdef REU
    win_base = suf_len = lines_before = 0; lines_exact = 1; ix_reset();
    win_style = sc_ok = 0;
    first_mark = 0xFFFFFFFFUL;
#endif
    doclen = 0;
    cursor = 0;
    anchor = NOSEL;
    top = 0;
    style = 0;
    dirty = 0;
    filename[0] = 0;
#ifdef REU
    sd_on = 0;
#endif
#ifdef RTF
    doc_fmt = 0;
#endif
#ifdef REU
    jr_ok = 0;
    jr_clear();
#endif
    layout_all();
}
#ifdef REU
#pragma rodata-name (pop)
#pragma code-name (pop)
static void new_doc(void) { ovl_load(8); new_doc_(); }
#else
#define new_doc new_doc_
#endif

#ifndef REU
/* PLM1 (versions 1.0 et 1.1) : texte puis un octet de style par caractere.
   Le texte est lu en fin de tampon, puis fusionne au debut avec ses marqueurs. */
static unsigned char load_plm1(unsigned n)
{
    unsigned r = MAXDOC - n, w = 0, k;
    unsigned char st = 0, *chunk = outbuf, c, i;  /* tampon d'impression, libre ici */
    cbm_read(2, &text[r], n);
    for (k = 0; k < n; k += 64) {
        c = (unsigned char)(n - k > 64 ? 64 : n - k);
        cbm_read(2, chunk, c);
        for (i = 0; i < c; ++i) {
            if ((chunk[i] & 7) != st) {
                st = chunk[i] & 7;
                if (w >= r) return 0;
                text[w++] = MARK(st);
            }
            if (w > r) return 0;
            text[w++] = text[r++];
        }
    }
    doclen = w;
    return 1;
}
#endif

#ifdef REU
/* ------------------------------------------------------------------ */
/* Fenetre glissante                                                   */

/* Le document par positions globales : fenetre (dans text[], ou rangee dans le
   bloc-notes du REU pendant l'impression), debut en bas, suffixe en haut. */
static unsigned char win_in_reu;        /* la fenetre est dans le bloc-notes, pas dans text[] */
static unsigned print_win;

static unsigned win_len(void)
{
    return win_in_reu ? print_win : doclen;
}

static dpos doc_total(void)
{
    return win_base + win_len() + suf_len;
}

/* n octets du document a partir de la position g, vers dst. */
static void doc_read(unsigned char *dst, dpos g, unsigned n)
{
    dpos wend = win_base + win_len();
    unsigned k;
    for (; n; dst += k, g += k, n -= k) {
        k = n;
        if (g < win_base) {
            if (win_base - g < k) k = (unsigned)(win_base - g);
            reu_move(1, dst, g, k);
        } else if (g < wend) {
            if (wend - g < k) k = (unsigned)(wend - g);
            if (win_in_reu) reu_move(1, dst, suf_end + (g - win_base), k);
            else memcpy(dst, text + (unsigned)(g - win_base), k);
        } else {
            reu_move(1, dst, suf_end - suf_len + (g - wend), k);
        }
    }
}

/* Debut du paragraphe contenant la position p : la plus grande position s <= p
   dont l'octet precedent est un CR (ou 0). A defaut en 8 Ko : p lui-meme. */
static dpos para_start(dpos p)
{
    unsigned n, i;
    dpos lim = p > 8192 ? p - 8192 : 0, q = p;
    while (q > lim) {
        n = q - lim > 256 ? 256 : (unsigned)(q - lim);
        doc_read(fbuf, q - n, n);
        for (i = n; i; --i) if (fbuf[i - 1] == CR) return q - n + i;
        q -= n;
    }
    return lim ? p : 0;
}

/* Style en vigueur au debut de la position p : le dernier marqueur avant p.
   Un intervalle [sc_lo1, sc_hi] garde le dernier resultat : sans marqueur entre
   ces bornes le style est le meme. sc_hi redescend a p pour que les
   modifications faites a partir de p ne rendent pas le cache faux. */
static unsigned char style_at(dpos p)
{
    unsigned n, i;
    dpos q = p, lo = 0;
    unsigned char st = 0;
    if (p <= first_mark) return 0;              /* aucun marqueur avant p */
    if (sc_ok && p >= sc_lo1 && p <= sc_hi) { sc_hi = p; return sc_st; }
    while (q) {
        if (sc_ok && q >= sc_lo1 && q <= sc_hi) { st = sc_st; lo = sc_lo1; break; }
        n = q > 256 ? 256 : (unsigned)q;
        doc_read(fbuf, q - n, n);
        for (i = n; i; --i) {
            if (IS_MARK(fbuf[i - 1])) { st = fbuf[i - 1] & 7; lo = q - n + i; q = n; break; }
        }
        q -= n;
    }
    sc_lo1 = lo;
    sc_hi = p;
    sc_st = st;
    sc_ok = 1;
    return st;
}

/* Le texte qui quitte la fenetre ou qu'on y retrouve peut contenir des marqueurs
   nouveaux : first_mark ne doit jamais depasser le premier du document. */
static void note_marks(const unsigned char *q, unsigned n, dpos base)
{
    unsigned i;
    if (base >= first_mark) return;
    for (i = 0; i < n; ++i) if (IS_MARK(q[i])) { first_mark = base + i; return; }
}

/* Le texte a partir de la ligne T (>= 1) quitte la fenetre : il retourne au
   debut du suffixe. Si le texte garde finit par CR, la mise en page a une ligne
   vide finale, comme layout_from. */
static void cut_tail(unsigned T)
{
    unsigned c = lstart[T], extra = doclen - c;
    unsigned char st = lstyle[T];
    note_marks(text + c, extra, win_base + c);
    suf_len += extra;
    reu_move(0, text + c, suf_end - suf_len, extra);
    doclen = c;
    anchor_fix();
    nlines = T;
    if (text[c - 1] == CR) { lstyle[nlines] = st; lstart[nlines++] = c; }
    lstart[nlines] = c;
}

/* layout_from s'arrete a MAXLINES lignes : le reste retourne dans le suffixe. */
static void trim_lines(void)
{
    if (nlines >= MAXLINES) cut_tail(nlines - 1);
}

/* Remplit la fenetre depuis le suffixe jusqu'a FILL octets, en retordant a
   partir de la derniere ligne (elle s'arretait au bord de la fenetre). */
static unsigned fill_lim = FILL;                /* meddig tolti az ablakot a pull_tail (ha egy sort sem lehet eldobni : MAXDOC - ROOM) */
static void pull_tail(void)
{
    unsigned m, i;
    if (doclen >= fill_lim || !suf_len) return;
    m = fill_lim - doclen;
    if (m > suf_len) m = (unsigned)suf_len;
    reu_move(1, text + doclen, suf_end - suf_len, m);
    suf_len -= m;
    doclen += m;
    anchor_fix();
    i = nlines - 1;
    layout_from(i, lstart[i], lstyle[i]);
    trim_lines();
}

/* Les L premieres lignes (L >= 1) sortent de la fenetre par le haut, vers le
   REU. Le curseur reste a l'ecran : il est au moins 25 lignes plus bas. */
static void drop_head(unsigned L)
{
    unsigned c = lstart[L], i;
    note_marks(text, c, win_base);
    reu_move(0, text, win_base, c);
    memmove(text, text + c, doclen - c);
    doclen -= c;
    win_base += c;
    win_style = lstyle[L];
    lines_before += L;
    for (i = L; i < nlines; ++i) lstyle[i - L] = lstyle[i];
    for (i = L; i <= nlines; ++i) lstart[i - L] = lstart[i] - c;
    nlines -= L;
    cursor -= c;
    anchor_fix();
    top = top > L ? top - L : 0;
}

/* Vers la fin du document : le haut sort, la suite du suffixe entre. */
static void slide_fwd(void)
{
    unsigned L, i = nlines - 1, lim = top > 3 ? top - 3 : 0;      /* on ne touche pas aux lignes de l'ecran (3 lignes de marge : une fenetre petite ne doit pas se bloquer) */
    for (L = 1; L < nlines && lstart[L] < STEP; ++L) ;
    if (L > lim) L = lim;
    if (L) drop_head(L); else fill_lim = MAXDOC - ROOM;     /* nincs mit eldobni : az ablak telibb (kis ablak ne akadjon el) */
    pull_tail();
    fill_lim = FILL;
    draw_lines(i - L, nlines);
}

/* Vers le debut : le morceau qui precede la fenetre, a partir d'un debut de
   paragraphe, revient, et le bas sort si la place manque. Si le texte qui suit
   a change depuis, la mise en page de toute la fenetre en tient compte. */
static void slide_back(void)
{
    unsigned len, T = nlines, ts, nP;
    unsigned char all, st;
    dpos s0;
    if (!win_base) return;
    s0 = para_start(win_base - 1);              /* az elozo bekezdes (vagy a ceges eleje) */
    len = (unsigned)(win_base - s0);
    if (!len || len > FILL) return;
    st = style_at(s0);
    if (doclen + len > FILL) {
        while (T && lstart[T] + len > FILL) --T;
        if (T < top + VISIBLE + 2) return;      /* la coupure doit rester sous l'ecran */
        cut_tail(T);
    }
    ts = lstart[top] + len;
    memmove(text + len, text, doclen);
    reu_move(1, text, s0, len);
    win_base = s0;
    doclen += len;
    win_style = st;
    lstyle[0] = win_style;                      /* relayout repart du style de la ligne 0 */
    relayout(0, 0, len, &all);                  /* le morceau est une insertion en 0 */
    nP = line_of(len);
    lines_before = lines_before > nP ? lines_before - nP : 0;
    if (!s0) { lines_before = 0; lines_exact = 1; }      /* retour au debut du document */
    cursor += len;
    anchor_fix();
    top = line_of(ts);
    draw_lines(nP ? nP - 1 : 0, nP + 1);
    trim_lines();
}

/* La fenetre repart de B (debut de paragraphe) : tout le reste est range dans
   le REU, le trou entre le debut et le suffixe se deplace jusqu'a B. */
static void relocate(dpos B)
{
    dpos d;
    unsigned k;
    unsigned char st;
    if (nlines) bpl_est = (bpl_est * 7 + doclen / nlines) / 8;
    if (!bpl_est) bpl_est = 1;
    note_marks(text, doclen, win_base);
    st = style_at(B);
    reu_move(0, text, win_base, doclen);
    win_base += doclen;
    doclen = 0;
    while (win_base < B) {
        d = B - win_base;
        k = d > MAXDOC ? MAXDOC : (unsigned)d;
        reu_move(1, text, suf_end - suf_len, k);
        reu_move(0, text, win_base, k);
        win_base += k;
        suf_len -= k;
    }
    while (win_base > B) {
        d = win_base - B;
        k = d > MAXDOC ? MAXDOC : (unsigned)d;
        win_base -= k;
        reu_move(1, text, win_base, k);
        suf_len += k;
        reu_move(0, text, suf_end - suf_len, k);
    }
    win_style = st;
    lines_before = B / bpl_est;                 /* estimation : le nombre exact de lignes avant B n'est pas connu */
    lines_exact = !B;
    ix_go = 1;
    anchor_fix();
    layout_all();
    pull_tail();
}

#ifdef REUTEST
static unsigned char checks, fails;
static unsigned sumlines(void)
{
    unsigned s = 0, i;
    for (i = 0; i <= nlines; ++i) s = ((s << 1) | (s >> 15)) ^ lstart[i];
    for (i = 0; i < nlines; ++i) s = ((s << 1) | (s >> 15)) ^ lstyle[i];
    return s;
}
/* La mise en page incrementale doit egaler une mise en page complete. */
static void selfcheck(void)
{
    unsigned a = sumlines(), n = nlines;
    layout_all();
    ++checks;
    if (a != sumlines() || n != nlines) ++fails;
    POKE(0x03ED, checks);
    POKE(0x03EE, fails);
}
/* Etat de la fenetre, relisible par l'API ($02C0 : win_base (3), suf_len (3),
   lines_before (3), nlines (2)). */
static void dbg_state(void)
{
    unsigned char *d = (unsigned char *)0x02C0;
    d[0] = (unsigned char)win_base; d[1] = (unsigned char)(win_base >> 8); d[2] = (unsigned char)(win_base >> 16);
    d[3] = (unsigned char)suf_len; d[4] = (unsigned char)(suf_len >> 8); d[5] = (unsigned char)(suf_len >> 16);
    d[6] = (unsigned char)lines_before; d[7] = (unsigned char)(lines_before >> 8); d[8] = (unsigned char)(lines_before >> 16);
    d[9] = (unsigned char)nlines; d[10] = (unsigned char)(nlines >> 8);
}
#endif

/* Apres chaque touche ou clic : garde le curseur loin des bords de la fenetre
   et au moins ROOM octets libres. Le curseur ne bouge pas a l'ecran. */
static void maybe_slide(void)
{
    if (!reu_on) return;
    if (MAXDOC - doclen < ROOM) { if (top >= 12) slide_fwd(); else slide_back(); }
    else if (suf_len && top + VISIBLE + 4 > nlines) slide_fwd();       /* le bas de l'ecran approche la fin de la fenetre */
    else if (win_base && top < 3) slide_back();                         /* le haut de l'ecran approche le debut */
    else {
#ifdef REUTEST
        dbg_state();
#endif
        return;
    }
#ifdef REUTEST
#ifndef NOCHECK
    selfcheck();
#endif
    dbg_state();
#endif
    draw_status();
}

/* Amene la plage globale [lo, hi) dans la fenetre. */
static void goto_global(dpos lo, dpos hi)
{
    if (lo >= win_base && hi <= win_base + doclen) return;
    relocate(para_start(lo > 400 ? lo - 400 : 0));     /* 400 + un paragraphe (<= ~1200) doit tenir dans FILL */
}

/* PLM2 de n octets, trop grand pour la fenetre : tout le fichier va dans le
   suffixe (par morceaux de 256), puis la fenetre se remplit depuis son debut. */
static unsigned char load_big(dpos n)
{
    unsigned k;
    dpos dst = suf_end - n;
    suf_len = n;
    first_mark = 0xFFFFFFFFUL;
    pg_begin(S_LOADING, n);
    for (; n; n -= k, dst += k) {
        k = n > 256 ? 256 : (unsigned)n;
        if ((unsigned)fr(outbuf, k) != k) { pg_end(); return 0; }
        note_marks(outbuf, k, suf_len - n);
        reu_move(0, outbuf, dst, k);
        pg_add(k);
    }
    pg_end();
    doclen = 0;
    layout_all();
    pull_tail();
    return 1;
}
#endif

#ifdef REU
#pragma code-name (push, "OVERLAY6")
#pragma rodata-name (push, "OVERLAY6")
static void load_named_(void)
#else
static void load_named(void)
#endif
{
#ifdef REU
    unsigned char head[8], ok = 1;
    dpos n;
#else
    unsigned char head[6], ok = 1;
    unsigned n;
#endif
#ifdef REU
    disk_busy(1);
    if (fo_r(filename) != 0) { disk_busy(0); disk_status(); show_disk_error(); return; }
#else
    strcpy(cmdname, filename);
    strcat(cmdname, ",S,R");
    disk_busy(1);
    if (cbm_open(2, dev, 2, cmdname) != 0) { disk_busy(0); disk_status(); show_disk_error(); return; }
#endif
#ifdef RTF
    doc_fmt = 0;
#endif
    fr(head, 6);
    n = head[4] | (head[5] << 8);
#ifdef REU
    if (!memcmp(head, "PLM3", 4)) {
        fr(head + 6, 2);
        n |= ((dpos)head[6] << 16) | ((dpos)head[7] << 24);
    }
    win_base = suf_len = lines_before = 0; lines_exact = 1; ix_reset();
    win_style = sc_ok = 0;
    first_mark = 0xFFFFFFFFUL;
    if (reu_on && n > MAXDOC && n <= suf_end && (!memcmp(head, "PLM2", 4) || !memcmp(head, "PLM3", 4))) {
        ok = load_big(n);
    } else
#endif
    if (!memcmp(head, "PLM2", 4) && n <= MAXDOC) {
        fr(text, (unsigned)n);
        doclen = (unsigned)n;
#ifndef REU
    } else if (!memcmp(head, "PLM1", 4) && n <= MAXDOC) {
        ok = load_plm1((unsigned)n);
#endif
    } else {
        ok = 0;
    }
    fc();
    disk_busy(0);
    if (!ok) {
        doclen = 0;
#ifdef REU
        suf_len = 0;
#endif
        message(S_OPEN, S_BADFILE, 0, 0);
    }
    else if (disk_status() >= 20) { show_disk_error(); return; }
#ifdef REU
    op_ok = ok;
#endif
    cursor = 0;
    anchor = NOSEL;
    top = 0;
    dirty = 0;
    layout_all();
    style = style_before(0);
}
#ifdef REU
#pragma rodata-name (pop)
#pragma code-name (pop)
static void load_named(void) { ovl_load(6); load_named_(); }
#endif

#ifdef RTF
/* Format du fichier "filename" (disque ou SD) d'apres son contenu : 0 Plume, 1 RTF, 2 texte */

#pragma code-name (push, "OVERLAY6")
#pragma rodata-name (push, "OVERLAY6")
/* A fajl fajtaja a tartalmabol: 0 PLM, 1 RTF, 2 szoveg, 3 szovegkent nem megjelenitheto (bináris) fajl. */
static unsigned char file_kind_(void)
{
    unsigned char r = 2, c;
    unsigned n, i, ctl = 0;
    if (fo_r(filename) == 0) {
        n = (unsigned)fr(outbuf, 256);
        if (n >= 5 && !memcmp(outbuf, "{\\rtf", 5)) r = 1;
        else if (n >= 5 && !memcmp(outbuf, "PLM", 3)) r = 0;
        else {
            for (i = 0; i < n; ++i) {
                c = outbuf[i];
                if (!c) { r = 3; break; }                         /* NUL : biztosan bináris */
                if (c < 32 && c != 9 && c != 10 && c != 13) ++ctl;
            }
            if (r == 2 && ctl > (n >> 4)) r = 3;                  /* sok vezerlokarakter */
        }
        fc();
        if (r == 3 && !message(S_OPEN, S_BIN1, S_BIN2, 1)) r = 4;      /* bináris : figyelmeztetes, 4 = megszakitva */
    }
    return r;
}
#pragma rodata-name (pop)
#pragma code-name (pop)
static unsigned char file_kind(void) { ovl_load(6); return file_kind_(); }
/* Charge "filename" (disque ou SD) dans le format que son contenu indique. */
static void load_any(void)
{
    unsigned char k = file_kind(), b = k == 3;
    if (k == 4) return;                         /* bináris fajl, a felhasznalo megszakitotta */
    if (b) k = 2;
    if (k && !reu_on) { message(S_OPENRTF, S_NEEDREU, 0, 0); return; }
    if (k == 1) rtf_open();
    else if (k == 2) txt_open();
    else load_named();
    if (b && op_ok) { filename[0] = 0; draw_title(); }     /* nev nelkul : a Save mindig uj nevet kerdez, az eredeti sosem irodik felul */
}
#endif

#ifdef REU
/* Uj dokumentum / masik fajl megnyitasa elott : nem mentett valtozas (dirty), vagy csak a gyorsmentes (.bck) tartja oket. */
static unsigned char leave_ok(const char *title)
{
    if (dirty) return message(title, S_LOSE, 0, 1);
    if (jr_ok && jr_len != jb && message(title, S_QUICK1, S_QUICK2, 2)) save_doc();      /* Save : teljes mentes ; No save : tovabb (a .bck megorzi) */
    return 1;
}

static void open_doc(void)
{
    if (!leave_ok(S_OPEN)) return;
    if (choose(S_PICK, 0) == 1) {
        sd_on = sd_sel;
        op_ok = 0;
        load_any();
        if (op_ok) bck_loaded(); else { sd_on = 0; filename[0] = 0; }
    }
    redraw_all();
}
#else
static void open_doc(void)
{
    struct cbm_dirent e;
    unsigned char count = 0, i, sel = 255, rows;
    char *shown = inputbuf;
    if (dirty && !message(S_OPEN, S_LOSE, 0, 1)) return;
    if (cbm_opendir(1, dev) == 0) {
        while (count < MAXFILES && !cbm_readdir(1, &e)) {
            if (e.type == CBM_T_SEQ) { io_hide(); strcpy(names[count++], e.name); io_show(); }
        }
        cbm_closedir(1);
    }
    if (!count) {
        if (disk_status() >= 20) show_disk_error(); else message(S_OPEN, S_NOFILES, 0, 0);
        return;
    }
    rows = (unsigned char)((count + 1) / 2);
    panel(4, (unsigned char)(rows + 6), DLG_X, DLG_W);
    put_text(DLG_X + 16, 5, S_PICK, BOLD);
    for (i = 0; i < count; ++i) {
        io_hide();
        strcpy(shown, names[i]);
        io_show();
        petscii_to_text(shown, shown);
        put_text(DLG_X + 16 + (i & 1) * 112, (unsigned char)(7 + i / 2), shown, 0);
    }
    button(BTN_CANCEL_X, (unsigned char)(8 + rows), S_CANCEL);
    while (sel == 255) {
        poll_mouse();
        key = read_key();
        if (key == 3 || on_button(BTN_CANCEL_X, (unsigned char)(8 + rows))) break;
        if (clicked && mouse_y / 8 >= 7 && mouse_y / 8 < 7 + rows) {
            i = (unsigned char)((mouse_y / 8 - 7) * 2 + (mouse_x >= DLG_X + 128));
            if (i < count) sel = i;
        }
    }
    if (sel != 255) {
        io_hide();
        strcpy(filename, names[sel]);
        io_show();
        load_named();
    }
    redraw_all();
}
#endif

/* ------------------------------------------------------------------ */
/* Impression sur MPS-803 (bus serie, peripherique 4)                  */
/* Largeur A4 avec 3 cm de marge de chaque cote : 15 cm de texte.      */

#define PRINT_CARDS 60
#define PRINT_LEFT  71
#define PRINT_W     354
#define TEXT_LEFT   12
#define TEXT_COLS   59
#define WIDE_LEFT   6
#define WIDE_COLS   29
#define TEAR_GRAPH  12
#define TEAR_TEXT   8

/* La ligne en cours de dessin (strip) n'est plus lue quand une passe part :
   elle partage donc la place de outbuf. rows, lui, vit en meme temps que
   outbuf (emit_pass), il garde la sienne. */
#define strip outbuf
#ifdef REU
/* Pas de tampon propre : la fin de text[] sert, les morceaux du document sont plus courts. */
#define PMAX (MAXDOC - 14 * PRINT_CARDS)
#define rows ((unsigned char (*)[PRINT_CARDS])(text + PMAX))
#else
static unsigned char rows[14][PRINT_CARDS];
#endif
#ifdef REU
#pragma code-name (push, "OVERLAY2")           /* toute l'impression est dans l'overlay 2 */
#pragma rodata-name (push, "OVERLAY2")
#endif
static unsigned char pending;
static unsigned char print_ok;

static unsigned char stop_pressed(void)
{
    return PEEK(0x91) == 0x7F;
}

#ifdef REUTEST
static unsigned char pr_sink;
static unsigned pr_sum, pr_cnt;
#endif
static void pr_send(const void *data, unsigned n)
{
    if (!print_ok || !n) return;
#ifdef REUTEST
    if (pr_sink) {                      /* test : somme des octets autres que espace et CR */
        const unsigned char *d = data;
        for (; n; --n, ++d) if (*d != ' ' && *d != CR) { pr_sum = ((pr_sum << 1) | (pr_sum >> 15)) ^ *d; ++pr_cnt; }
        *(unsigned *)0x02B0 = pr_sum;
        *(unsigned *)0x02B2 = pr_cnt;
        return;
    }
#endif
    cbm_write(4, data, n);
    if (cbm_k_readst() & 0x80) print_ok = 0;
}

static void pr_byte(unsigned char b)
{
    pr_send(&b, 1);
}

static void emit_pass(void)
{
    unsigned char card, mask, r, b, *o = outbuf;
    *o++ = 8;
    for (card = 0; card < PRINT_CARDS; ++card) {
        for (mask = 0x80; mask; mask >>= 1) {
            b = 0x80;
            for (r = 0; r < 7; ++r) {
                if (rows[r][card] & mask) b |= 1 << r;
            }
            *o++ = b;
        }
    }
    *o++ = CR;
    pr_send(outbuf, (unsigned)(o - outbuf));
    pending -= 7;
    memmove(rows[0], rows[7], pending * PRINT_CARDS);
}

#ifdef REU
/* Le document entier, par morceaux. La fenetre (modifications comprises) est
   rangee en banque 2 le temps de l'impression, text[] sert de tampon : on y
   charge des morceaux du document coupes apres un CR, pour que leur derniere
   ligne soit complete. print_g : position dans le document de text[0]. */
static dpos print_g;

/* Charge le morceau suivant dans text[] ; 0 s'il n'y en a plus. */
static unsigned char print_more(void)
{
    dpos n;
    unsigned k;
    print_g += doclen;
    n = doc_total() - print_g;
    if (!n) return 0;
    k = n > PMAX ? PMAX : (unsigned)n;
    doc_read(text, print_g, k);
    if (k < n) {
        while (k && text[k - 1] != CR) --k;
        if (!k) k = n > PMAX ? PMAX : (unsigned)n;   /* paragraphe plus long que le morceau */
    }
    doclen = k;
    return 1;
}

static void print_begin(void)
{
    print_win = doclen;
    reu_move(0, text, suf_end, doclen);
    win_in_reu = 1;
    print_g = 0;
    doclen = 0;
    print_more();
}

static void print_end(void)
{
    win_in_reu = 0;
    doclen = print_win;
    reu_move(1, text, suf_end, doclen);
}
#endif

static void print_graphics(void)
{
    unsigned p = 0, n, q, x;
    unsigned char r, card, st = 0, c;
    pending = 0;
    wrap_width = PRINT_W;
    while (print_ok && !stop_pressed()) {
        n = next_line(p, st);
        memset(strip, 0, sizeof strip);
        gbase = (unsigned)strip;
        x = PRINT_LEFT;
        for (q = p; q < n; ++q) {
            c = text[q];
            if (IS_MARK(c)) { st = c & 7; continue; }
            if (c == CR) continue;
            gch = c;
            gstyle = st;
            gx = x;
            draw_glyph();
            x += CW(c, st);
        }
        st = next_style;
        for (r = 0; r < 8; ++r) {
            for (card = 0; card < PRINT_CARDS; ++card) rows[pending + r][card] = strip[card * 8 + r];
        }
        pending += 8;
        while (pending >= 7) emit_pass();
#ifdef REU
        if (n >= doclen) {
            if (!print_more()) break;
            p = 0;                      /* le style courant (st) continue */
        } else p = n;
#else
        if (n >= doclen) break;
        p = n;
#endif
    }
    wrap_width = TEXT_W;
    if (pending) {
        memset(rows[pending], 0, (7 - pending) * PRINT_CARDS);
        pending = 7;
        emit_pass();
    }
    pr_byte(8);
    for (r = 0; r < TEAR_GRAPH; ++r) pr_byte(CR);
    pr_byte(15);
}

/* Mode texte de l'imprimante : lettre de base des codes 128 à 180
   (œ Œ -> O ; « » ° € -> espace). */
static const char accent_base[] = "aaaceeeeiioouuuAACEEEEIIOUUU      aoaAOAaAiIoOuUoOuUO";

static unsigned char to_printer(unsigned char c)
{
    if (c >= 182) return "\"\"\"''--*"[c - 182];                /* tipografiai jelek : ASCII kozelites */
    if (c >= 128) {
        c -= 128;
        if (c == 28 || c == 29) return 'O';
        if (c < sizeof accent_base - 1) c = (unsigned char)accent_base[c];
        else return ' ';
    }
    if (c >= 'a' && c <= 'z') return c - 0x20;
    if (c >= 'A' && c <= 'Z') return c + 0x80;
    return c;
}

static void print_text(unsigned char wide)
{
    unsigned char limit = wide ? WIDE_COLS : TEXT_COLS, left = wide ? WIDE_LEFT : TEXT_LEFT, n, k;
    unsigned p = 0, word, w, q;
    unsigned char *line = outbuf;              /* tampon graphique, libre en mode texte */
    if (wide) pr_byte(14);
    memset(line, ' ', left);
    n = left;
#ifdef REU
    while (print_ok && !stop_pressed()) {
        if (p >= doclen) {
            if (!print_more()) break;
            p = 0;
            continue;
        }
#else
    while (p < doclen && print_ok && !stop_pressed()) {
#endif
        if (IS_MARK(text[p])) { ++p; continue; }
        if (text[p] == CR) {
            pr_send(line, n); pr_byte(CR); n = left; ++p;
            continue;
        }
        /* longueur visible du mot suivant (espace comprise) */
        w = 0;
        for (word = p; word < doclen && text[word] != CR; ++word) {
            if (IS_MARK(text[word])) continue;
            ++w;
            if (text[word] == ' ') { ++word; break; }
        }
        if (n - left + w > limit && n > left) { pr_send(line, n); pr_byte(CR); n = left; }
        for (q = p; q < word; ++q) {
            if (IS_MARK(text[q])) continue;
            if (n - left == limit) { pr_send(line, n); pr_byte(CR); n = left; }
            line[n++] = to_printer(text[q]);
        }
        p = word;
    }
    if (n > left) { pr_send(line, n); pr_byte(CR); }
    for (k = 0; k < TEAR_TEXT; ++k) pr_byte(CR);
    if (wide) pr_byte(15);
}

static void print_doc(unsigned char mode)
{
    dialog(5, S_MENU3);
    put_text(DLG_X + 16, DLG_ROW + 2, S_PRINTING, 0);
    print_ok = 1;
    cbm_open(4, 4, mode ? 7 : 0, "");
    if (cbm_k_readst() & 0x80) print_ok = 0;
    if (print_ok) {
#ifdef REU
        print_begin();
#endif
        if (mode == 0) print_graphics(); else print_text(mode == 2);
#ifdef REU
        print_end();
#endif
    }
    cbm_close(4);
    dialog_close();
    if (!print_ok) message(S_MENU3, S_NOPRINTER, 0, 0);
}
#ifdef REU
#pragma rodata-name (pop)
#pragma code-name (pop)
#ifdef REU
static void print_run(unsigned char mode)
{
    unsigned char s = small_font;
    if (s) set_small(0);                        /* a nyomtatas a normal betukeszlettel tordel */
    ovl_load(2);
    print_doc(mode);
    if (s) set_small(1);
}
#else
static void print_run(unsigned char mode) { ovl_load(2); print_doc(mode); }
#endif
#else
#define print_run print_doc
#endif

/* ------------------------------------------------------------------ */
/* Edition                                                             */

static void set_selection(unsigned new_anchor, unsigned new_cursor)
{
    unsigned a = sel_lo(), b = sel_hi(), la, lb;
#ifdef REU
    if (new_anchor != NOSEL && new_anchor != anchor) agl = win_base + new_anchor;
#endif
    anchor = new_anchor;
    cursor = new_cursor;
    if (sel_lo() < a) a = sel_lo();
    if (sel_hi() > b) b = sel_hi();
    la = line_of(a);
    lb = line_of(b);
    place_cursor();
    draw_lines(la, lb);
    draw_status();
}

static void clear_selection(void)
{
    if (anchor != NOSEL) set_selection(NOSEL, cursor);
}

static unsigned char has_selection(void)
{
#ifdef REU
    return anchor != NOSEL && agl != win_base + cursor;
#else
    return anchor != NOSEL && anchor != cursor;
#endif
}

/* Supprime [a, b) en gardant le style du texte qui suit. */
static void delete_range(unsigned a, unsigned b)
{
    unsigned char after, mark;
    a = canonical(a);
    if (a >= b) return;
    after = style_before(b);
    mark = MARK(after);
    anchor = NOSEL;
    cursor = a;
    if (after != style_before(a) && b < doclen && !IS_MARK(text[b])) replace(a, b, &mark, 1, 0);
    else replace(a, b, 0, 0, 0);
}

static unsigned char delete_selection(void)
{
    if (!has_selection()) { anchor = NOSEL; return 1; }
#ifdef REU
    if (!sel_in_window()) return bdel();        /* selection plus grande que la fenetre */
#endif
    delete_range(sel_lo(), sel_hi());
    return 1;
}

static void insert_char(unsigned char c)
{
    unsigned char seq[3], n = 0, here;
    unsigned p, b;
    if (!delete_selection()) return;
    p = b = cursor;
#ifdef HUN
    if (overwrite && p < doclen && !IS_MARK(text[p]) && text[p] != CR) b = p + 1;   /* ecrase le caractere suivant */
#endif
    here = style_before(p);
    if (style != here) seq[n++] = MARK(style);
    seq[n++] = c;
    if (style != here && b < doclen && !IS_MARK(text[b])) seq[n++] = MARK(here);
    cursor = p + (style != here ? 2 : 1);
    if (!replace(p, b, seq, n, 0)) cursor = p;
}

static void delete_before(void)
{
    if (has_selection()) { delete_selection(); return; }
    anchor = NOSEL;
    if (cursor) delete_range(cursor - 1, cursor);
}

#ifdef REU
/* Presse-papiers dans le REU (jusqu'a 256 Ko ; sans REU, dans CLIP) et selection de toute la longueur du
   document. Tout ce bloc est dans l'overlay 5 ; les versions residentes le chargent. */
#pragma code-name (push, "OVERLAY5")
#pragma rodata-name (push, "OVERLAY5")
/* Style en vigueur a la position globale p. */
static unsigned char style_g(dpos p)
{
    if (p >= win_base && p <= win_base + doclen) return style_before((unsigned)(p - win_base));
    note_marks(text, doclen, win_base);
    return style_at(p);
}

/* n octets du presse-papiers a partir de off, vers/depuis buf. */
static void clip_io(unsigned char fetch, unsigned char *buf, dpos off, unsigned n)
{
    if (reu_on) reu_move(fetch, buf, c_base + off, n);
    else {
        io_hide();
        if (fetch) memcpy(buf, CLIP + (unsigned)off, n); else memcpy(CLIP + (unsigned)off, buf, n);
        io_show();
    }
}

/* Redessine apres une operation en bloc. */
static void blk_end(unsigned pos)
{
    unsigned k;
    quiet = 0;
    cursor = pos;
    anchor = NOSEL;
    layout_all();
    k = line_of(cursor);
    if (k < top || k >= top + VISIBLE) top = k > 4 ? k - 4 : 0;
    style = style_before(cursor);
    draw_text_area();
    place_cursor();
    draw_status();
}

static unsigned char copy_(void)
{
    static dpos lo, hi, g;
    static unsigned m;
    if (!has_selection()) return 0;
    lo = sel_lo_g();
    hi = sel_hi_g();
    if (hi - lo > c_cap) { message(S_COPY, S_CLIPFULL, 0, 0); return 0; }
    clip_st = style_g(lo);
    clip_end = style_g(hi);
    for (g = lo; g < hi; g += m) {
        m = hi - g > 256 ? 256 : (unsigned)(hi - g);
        doc_read(fbuf, g, m);
        if (g == lo) clip_m0 = IS_MARK(fbuf[0]);
        clip_io(0, fbuf, g - lo, m);
    }
    cliplen = hi - lo;
    return 1;
}

/* Une operation en bloc doit tenir en entier dans le journal : sinon l'historique repart de zero avant, et
   si elle depasse tout le journal, elle ne s'annule pas (jr_off). Renvoie 1 dans ce dernier cas. */
static unsigned char blk_room(dpos need)
{
    if (jr_len + need + 700 > u_cap || rl + need > u_cap || jr_n + 64u > l_max) { jr_ok = 0; jr_clear(); }
    if (need + 700 > u_cap) { jr_off = 1; return 1; }
    return 0;
}

/* Supprime la selection globale, fenetre apres fenetre. Renvoie 0 si on a renonce. */
static unsigned char bdel_(void)
{
    static dpos lo, hi, g, n;
    static unsigned d, ua;
    static unsigned char s0, s1, mark, own, first, noundo;
    lo = sel_lo_g();
    hi = sel_hi_g();
    if (hi - lo > u_cap && !message(S_BIGDEL, S_BIGDEL1, S_BIGDEL2, 1)) return 0;
    own = !jr_blk;
    if (own) { jr_blk = 2; noundo = blk_room(hi - lo); }
    s0 = style_g(lo);
    s1 = style_g(hi);
    quiet = 1;
    g = lo;
    n = hi - lo;
    first = 1;
    while (n) {
        if (g < win_base || g >= win_base + doclen) relocate(para_start(g > 400 ? g - 400 : 0));
        ua = (unsigned)(g - win_base);
        if (g < win_base || ua >= doclen) break;
        if (first) { first = 0; while (ua && IS_MARK(text[ua - 1])) { --ua; --g; ++n; } }
        d = doclen - ua;
        if (d > n) d = (unsigned)n;
        if (!replace(ua, ua + d, 0, 0, 0)) break;
        n -= d;
    }
    if (g < win_base || g > win_base + doclen) relocate(para_start(g > 400 ? g - 400 : 0));
    ua = (unsigned)(g - win_base);
    if (s0 != s1 && ua < doclen && !IS_MARK(text[ua])) { mark = MARK(s1); replace(ua, ua, &mark, 1, 0); }
    anchor = NOSEL;
    cursor = ua;
    if (own) { jr_blk = 0; jr_off = 0; blk_end(ua); }
    return 1;
}

static void cut_(void)
{
    if (copy_()) delete_selection();
}

static void paste_(void)
{
    static dpos pg, ra, left;
    static unsigned m, ua;
    static unsigned char here, f, pre, skip, mark, noundo;
    if (!cliplen) return;
    jr_blk = 2;
    noundo = blk_room(cliplen + (has_selection() ? sel_hi_g() - sel_lo_g() : 0));
    if (!delete_selection()) { jr_blk = 0; jr_off = 0; return; }
    quiet = 1;
    here = style_before(cursor);
    clip_io(1, &f, 0, 1);
    skip = 0;
    pre = 0;
    if (IS_MARK(f)) { if (f == MARK(here)) skip = 1; } else if (clip_st != here) pre = 1;
    pg = win_base + cursor;
    if (pre) { mark = MARK(clip_st); replace(cursor, cursor, &mark, 1, 0); ++pg; }
    ra = skip;
    left = cliplen - skip;
    while (left) {
        m = left > 400 ? 400 : (unsigned)left;
        if (MAXDOC - doclen < ROOM + 400 || pg < win_base || pg > win_base + doclen)
            relocate(para_start(pg > 400 ? pg - 400 : 0));      /* avant de lire le morceau : para_start se sert d'outbuf */
        clip_io(1, outbuf, ra, m);
        ua = (unsigned)(pg - win_base);
        if (pg < win_base || ua > doclen || !replace(ua, ua, outbuf, m, 0)) break;
        pg += m;
        ra += m;
        left -= m;
    }
    ua = (unsigned)(pg - win_base);
    if (clip_end != here && ua < doclen && !IS_MARK(text[ua])) {
        mark = MARK(here);
        replace(ua, ua, &mark, 1, 0);
    }
    jr_blk = 0;
    jr_off = 0;
    blk_end(ua);
}

static void select_all_(void)
{
    static dpos tot;
    unsigned k;
    tot = doc_total();
    if (!tot) return;
    if (reu_on) relocate(para_start(tot > 400 ? tot - 400 : 0));    /* la fin du document dans la fenetre */
    if (win_base + doclen < tot) tot = win_base + doclen;       /* paragraphe trop long : jusqu'au bout de la fenetre */
    agl = 0;
    anchor = 0;
    cursor = (unsigned)(tot - win_base);
    anchor_fix();
    k = line_of(cursor);
    top = k >= VISIBLE ? k - VISIBLE + 1 : 0;
    draw_text_area();
    place_cursor();
    draw_status();
}
#pragma rodata-name (pop)
#pragma code-name (pop)
static unsigned char bdel(void) { ovl_load(5); return bdel_(); }
static void copy_selection(void) { ovl_load(5); copy_(); }
static void cut_selection(void) { ovl_load(5); cut_(); }
static void paste(void) { ovl_load(5); paste_(); }
static void select_all(void) { ovl_load(5); select_all_(); }
#else
/* Presse-papiers : marqueur du style de depart, puis le texte et ses marqueurs. */
static void copy_selection(void)
{
    unsigned a = sel_lo(), n = sel_hi() - a;
    unsigned char st, k;
    if (!has_selection()) return;
    if (n + 1 > CLIPMAX) { message(S_COPY, S_CLIPFULL, 0, 0); return; }
    st = style_before(a);
    k = IS_MARK(text[a]) ? 0 : 1;               /* la selection commence-t-elle par un marqueur ? */
    io_hide();
    CLIP[0] = MARK(st);
    memcpy(CLIP + k, &text[a], n);
    io_show();
    cliplen = n + k;
}

static void cut_selection(void)
{
    unsigned n = sel_hi() - sel_lo();
    if (!has_selection()) return;
    copy_selection();
    if (cliplen >= n) delete_selection();
}

static void paste(void)
{
    unsigned p, k;
    unsigned char here, last = 0, mark, c;
    if (!cliplen) return;
    delete_selection();
    p = cursor;
    here = style_before(p);
    io_hide();
    for (k = 0; k < cliplen; ++k) { c = CLIP[k]; if (IS_MARK(c)) last = c & 7; }
    io_show();
    /* le marqueur de depart est inutile s'il repete le style du point d'insertion */
    k = 0;
    io_hide();
    if (CLIP[0] == MARK(here)) k = 1;
    io_show();
    if (!replace(p, p, CLIP + k, cliplen - k, 1)) return;
    cursor = p + cliplen - k;
    if (last != here && cursor < doclen && !IS_MARK(text[cursor])) {
        mark = MARK(here);
        k = cursor;
        replace(k, k, &mark, 1, 0);
        cursor = k;
    }
}

#endif

/* Style : sur la selection s'il y en a une, sinon pour la frappe. bit = 0 : normal.
   La selection et la suite du texte sont deplacees en haut du tampon, puis la
   selection est reecrite en un seul passage avec ses nouveaux marqueurs. */
#ifdef REU
/* apply_style est dans l'overlay 8 (il libere de la RAM) ; la version residente le charge d'abord. */
#pragma code-name (push, "OVERLAY8")
#pragma rodata-name (push, "OVERLAY8")
#define APPLY_STYLE apply_style_ov
#else
#define APPLY_STYLE apply_style
#endif
static void APPLY_STYLE(unsigned char bit)
{
    unsigned a, b, w, sel_end, need = 0;
    unsigned char all, every = 1, old_st, start_st, emitted, want, c, k, one = 0;
    unsigned char map[8];                       /* ancien style -> nouveau style */
    unsigned char *q, *end, *rd;
    unsigned first;
    if (!has_selection()) {
        if (bit) {                              /* kijeloles nelkul : a kurzor alatti karakter (ha nincs, az elotte levo) valt */
            a = cursor;
            while (a < doclen && IS_MARK(text[a])) ++a;
            if (a < doclen && text[a] != CR) { one = 1; b = a + 1; a = cursor; }       /* a kurzor es a karakter kozti jelolok a kijeloleshez tartoznak */
            else {
                a = cursor;
                while (a && IS_MARK(text[a - 1])) --a;
                if (a && text[a - 1] != CR) { one = 2; b = a; --a; }
            }
        }
        if (!one) {
            if (bit) style ^= bit; else style = 0;
            draw_status();
            return;
        }
    } else {
#ifdef REU
        if (!sel_in_window()) { message(S_BIGSEL, S_BIGSEL1, 0, 0); return; }
#endif
        a = sel_lo();
        b = sel_hi();
    }
    start_st = old_st = style_before(a);
    end = &text[b];
    for (q = &text[a]; q < end; ++q) {
        c = *q;
        if (IS_MARK(c)) old_st = c & 7; else if (!(old_st & bit)) { every = 0; break; }
    }
    for (k = 0; k < 8; ++k) map[k] = !bit ? 0 : every ? (unsigned char)(k & ~bit) : (unsigned char)(k | bit);
    /* marqueurs a ajouter, pour verifier la place avant de toucher au texte */
    emitted = old_st = start_st;
    for (q = &text[a]; q < end; ++q) {
        c = *q;
        if (IS_MARK(c)) { old_st = c & 7; continue; }
        want = map[old_st];
        if (want != emitted) { ++need; emitted = want; }
    }
    if (doclen + need + 1 > MAXDOC) { message(S_FULL, 0, 0, 0); return; }
    /* selection et suite en haut du tampon, puis reecriture en un passage */
#ifdef REU
    jr_rem(a, b - a);
#endif
    rd = &text[MAXDOC - (doclen - a)];
    memmove(rd, &text[a], doclen - a);
    end = rd + (b - a);
    q = &text[a];
    emitted = old_st = start_st;
    for (; rd < end; ++rd) {
        c = *rd;
        if (IS_MARK(c)) { old_st = c & 7; continue; }
        want = map[old_st];
        if (want != emitted) { *q++ = MARK(want); emitted = want; }
        *q++ = c;
    }
    sel_end = (unsigned)(q - text);
    if (emitted != old_st && end < &text[MAXDOC] && !IS_MARK(*end)) *q++ = MARK(old_st);
    w = (unsigned)(q - text);
    memmove(q, end, (unsigned)(&text[MAXDOC] - end));
    doclen = w + (unsigned)(&text[MAXDOC] - end);
    dirty = 1;
#ifdef REU
    ix_inv();
    jr_log(a, b - a, w - a);
#endif
    anchor = a;
#ifdef REU
    agl = win_base + a;
#endif
    cursor = sel_end;
    if (one) { anchor = NOSEL; if (one == 1) cursor = a; }      /* egy karakter : nincs kijeloles, a kurzor marad */
    first = relayout(a, b, w, &all);
    if (!all && relayout_last < line_of(sel_end)) relayout_last = line_of(sel_end);
    refresh_after(first, all);
    style = style_before(cursor + (IS_MARK(text[cursor]) ? 1 : 0));     /* a B / I / U jelzo a kurzor alatti karakter stilusat mutatja */
    draw_status();
}
#ifdef REU
#pragma rodata-name (pop)
#pragma code-name (pop)
static void apply_style(unsigned char bit) { ovl_load(8); apply_style_ov(bit); }
#endif

/* La recherche est dans l'overlay 2 (avec l'impression). */
#ifdef REU
#pragma code-name (push, "OVERLAY2")
#pragma rodata-name (push, "OVERLAY2")
#else
#define find_next_ find_next
#endif
static unsigned char lower(unsigned char c)
{
    return (c >= 'A' && c <= 'Z') ? c + 32 : c;
}

#ifdef REU
/* Recherche dans tout le document (positions globales), par morceaux de 256
   octets avec 64 octets de recouvrement. */
static dpos fms, fme;

static unsigned char find_range(dpos from, dpos to, unsigned char len)
{
    dpos g, tot = doc_total();
    unsigned n, i, q;
    unsigned char k;
    if (to > tot) to = tot;
    for (g = from; g < to; g += 192) {
        n = tot - g > 256 ? 256 : (unsigned)(tot - g);
        doc_read(fbuf, g, n);
        for (i = 0; i < 192 && g + i < to; ++i) {
            if (IS_MARK(fbuf[i])) continue;
            for (k = 0, q = i; k < len && q < n; ++q) {
                if (IS_MARK(fbuf[q])) continue;
                if (lower(fbuf[q]) != lower(findtext[k])) break;
                ++k;
            }
            if (k == len) { fms = g + i; fme = g + q; return 1; }
        }
    }
    return 0;
}

static void find_reu(unsigned char len)
{
    dpos start = win_base + (has_selection() ? sel_hi() : cursor);
    if (!find_range(start, 0xFFFFFFFFUL, len) && !find_range(0, start + 1, len)) {
        message(S_FIND, S_NOTFOUND, 0, 0);
        return;
    }
    if (fms < win_base || fme > win_base + doclen) {
        goto_global(fms, fme);
        anchor = NOSEL;
        cursor = top = 0;
        draw_text_area();                       /* l'ecran montrait l'ancienne fenetre */
    }
    set_selection((unsigned)(fms - win_base), (unsigned)(fme - win_base));
    maybe_slide();
    draw_text_area();
}
#endif

/* Recherche sans tenir compte des majuscules ni des marqueurs de style. */
static void find_next_(void)
{
    unsigned char len = (unsigned char)strlen((char *)findtext), k;
    unsigned start = has_selection() ? sel_hi() : cursor, p, q, tries;
    if (!len) return;
#ifdef REU
    if (reu_on) { find_reu(len); return; }
#endif
    p = start;
    for (tries = 0; tries <= doclen; ++tries, ++p) {
        if (p >= doclen) p = 0;
        if (IS_MARK(text[p])) continue;
        for (k = 0, q = p; k < len && q < doclen; ++q) {
            if (IS_MARK(text[q])) continue;
            if (lower(text[q]) != lower(findtext[k])) break;
            ++k;
        }
        if (k == len) {
            set_selection(p, q);
            return;
        }
    }
    message(S_FIND, S_NOTFOUND, 0, 0);
}
#ifdef REU
#pragma rodata-name (pop)
#pragma code-name (pop)
static void find_next(void) { ovl_load(2); find_next_(); }
#endif

static void find_dialog(void)
{
    if (ask_text(S_FIND, S_FINDWHAT, (char *)findtext, 22, 0)) find_next();
}

#ifdef RTF
/* ------------------------------------------------------------------ */
/* RTF : lecture en flux (nouveau document) et ecriture                */
/* Tout ce bloc (code et constantes) est dans l'overlay 1. */
#pragma code-name (push, "OVERLAY1")
#pragma rodata-name (push, "OVERLAY1")

/* Unicode des codes internes 128 a 189 (voir font_map.txt). */
static const unsigned ucode[62] = {
    0xE0, 0xE2, 0xE4, 0xE7, 0xE9, 0xE8, 0xEA, 0xEB, 0xEE, 0xEF, 0xF4, 0xF6, 0xF9, 0xFB, 0xFC,
    0xC0, 0xC2, 0xC7, 0xC9, 0xC8, 0xCA, 0xCB, 0xCE, 0xCF, 0xD4, 0xD9, 0xDB, 0xDC,
    0x153, 0x152, 0xAB, 0xBB, 0xB0, 0x20AC, 0xE6, 0xF8, 0xE5, 0xC6, 0xD8, 0xC5,
    0xE1, 0xC1, 0xED, 0xCD, 0xF3, 0xD3, 0xFA, 0xDA, 0x151, 0x150, 0x171, 0x170, 0xD6, 0x2190,
    0x201E, 0x201D, 0x201C, 0x2018, 0x2019, 0x2013, 0x2014, 0x2022
};
/* Windows-1252, octets 0x80 a 0x9F. */
static const unsigned cp1252_hi[32] = {
    0x20AC, '?', ',', 'f', 0x201E, 0x2026, '+', '+', '^', '%', 'S', '<', 0x152, '?', 'Z', '?',
    '?', 0x2018, 0x2019, 0x201C, 0x201D, 0x2022, 0x2013, 0x2014, '~', 'T', 's', '>', 0x153, '?', 'z', 'Y'
};

static unsigned char rtf_code(unsigned u)
{
    unsigned char i;
    if (u < 128) return u < 32 ? ' ' : (unsigned char)u;
    for (i = 0; i < sizeof ucode / 2; ++i) if (ucode[i] == u) return 128 + i;
    if (u == 0xA0) return ' ';
    return '?';
}

static unsigned char rcur, remit, rdepth, rfirst, rst, rlen, rneg, rhas, rhex, rskip, ruc, rfull;
static unsigned rnum, rcp;                      /* rcp : \ansicpg (1250 : a Windows-1250 ekezetes betui, ő ű) */
static unsigned char rstack[24], rword[8];

static void r_flush(void)
{
    reu_move(0, text, win_base, doclen);
    win_base += doclen;
    doclen = 0;
}

static dpos rlim, spos;                         /* limite du texte converti (SD : debut du fichier copie dans le REU) ; position de lecture */

static void r_emit(unsigned char c)
{
    if (doclen >= MAXDOC - 1000u) r_flush();
    if (win_base + doclen + 4096 >= rlim) { rfull = 1; return; }
    text[doclen++] = c;
}

/* Un caractere visible : saute les caracteres de repli apres \u, ignore les
   groupes de destination, insere les marqueurs de style. */
static void r_put(unsigned char c)
{
    if (rskip) { --rskip; return; }
    if (rcur & 8) return;
    if ((rcur & 7) != remit) {
        remit = rcur & 7;
        if (win_base + doclen < first_mark) first_mark = win_base + doclen;
        r_emit(MARK(remit));
    }
    r_emit(c);
}

static void r_uni(unsigned u)
{
    if (u == 0x2026) { r_put('.'); r_put('.'); u = '.'; }
    r_put(rtf_code(u));
}

static void r_style(unsigned char bit)
{
    if (rhas && !rnum) rcur &= ~bit; else rcur |= bit;
}

/* Destinations ignorees (debut de leur mot, 7 lettres au plus). */
static const char r_skip[] = "fonttbl colortb stylesh info pict header footer object footnot listtab listove ";

static void r_tab(void)
{
    unsigned char i;
    for (i = 0; i < 4; ++i) r_put(' ');
}

static void r_word(void)
{
    const char *k = r_skip, *e;
    char *w = (char *)rword;
    w[rlen] = 0;
    if (rfirst) {                               /* premier mot du groupe : destination a ignorer ? */
        rfirst = 0;
        for (; *k; k = e + 1) {
            e = strchr(k, ' ');
            if (!strncmp(w, k, (unsigned)(e - k))) { rcur |= 8; return; }
        }
    }
    if (!strcmp(w, "b")) r_style(BOLD);
    else if (!strcmp(w, "i")) r_style(ITALIC);
    else if (!strcmp(w, "ul")) r_style(UNDERLINE);
    else if (!strcmp(w, "ulnone")) rcur &= ~UNDERLINE;
    else if (!strcmp(w, "plain")) rcur &= 8;
    else if (!strcmp(w, "par") || !strcmp(w, "line")) r_put(CR);
    else if (!strcmp(w, "tab")) r_tab();
    else if (!strcmp(w, "u")) { r_uni(rneg ? 0 - rnum : rnum); rskip = ruc; }
    else if (!strcmp(w, "uc")) ruc = (unsigned char)rnum;
    else if (!strcmp(w, "ansicpg")) rcp = rnum;
    else if (!strcmp(w, "endash")) r_uni(0x2013);
    else if (!strcmp(w, "emdash")) r_uni(0x2014);
    else if (!strcmp(w, "bullet")) r_uni(0x2022);
    else if (!strcmp(w, "lquote")) r_uni(0x2018);
    else if (!strcmp(w, "rquote")) r_uni(0x2019);
    else if (!strcmp(w, "ldblquo")) r_uni(0x201C);               /* a szo 7 beture vagott (rword[8]) */
    else if (!strcmp(w, "rdblquo")) r_uni(0x201D);
}

static unsigned char r_hex(unsigned char c)
{
    return c <= '9' ? c - '0' : (unsigned char)((c | 32) - 'a' + 10);
}

#define R_ALPHA(c) (((c) | 32) >= 'a' && ((c) | 32) <= 'z')

/* Texte seul (UTF-8 ou ASCII) : fin de ligne CR, LF ou CR LF -> CR ; tabulation -> quatre espaces. */
static void t_byte(unsigned char c)
{
    if (rhex) {                                 /* octet de suite UTF-8 */
        if ((c & 0xC0) == 0x80) {
            rnum = (rnum << 6) | (c & 0x3F);
            if (--rhex) return;
            if (rnum != 0xFEFF) r_uni(rnum);    /* pas de BOM */
            return;
        }
        rhex = 0;                               /* sequence interrompue : c est traite normalement */
    }
    if (c == 10 && rneg) { rneg = 0; return; }  /* LF apres CR */
    rneg = 0;
    if (c == 13) { rneg = 1; r_put(CR); }
    else if (c == 10) r_put(CR);
    else if (c == 9) r_tab();
    else if (c < 32) return;
    else if (c < 128) r_put(c);
    else if (c >= 0xE0) { rnum = c & 0x0F; rhex = 2; }
    else if (c >= 0xC0) { rnum = c & 0x1F; rhex = 1; }
}

/* Un octet du fichier ; rst : 0 texte, 1 apres \, 2 mot, 3 nombre, 4-5 \'hh */
static void r_byte(unsigned char c)
{
    switch (rst) {
    case 0:
        if (c == '\\') rst = 1;
        else if (c == '{') { if (rdepth < sizeof rstack) rstack[rdepth++] = rcur; rfirst = 1; }
        else if (c == '}') { if (rdepth) rcur = rstack[--rdepth]; rfirst = 0; }
        else if (c >= 32) { rfirst = 0; if (c < 128) r_put(c); else r_uni(cp1252_hi[c & 31]); }
        break;
    case 1:
        rst = 0;
        if (R_ALPHA(c)) { rlen = 1; rword[0] = c; rhas = rneg = 0; rnum = 0; rst = 2; return; }
        if (c == '\'') rst = 4;
        else if (c == '*') { if (rfirst) rcur |= 8; }
        else if (c == '~') r_put(' ');
        else if (c == '_') r_put('-');
        else if (c == '\n' || c == CR) r_put(CR);
        else if (c == '\\' || c == '{' || c == '}') r_put(c);
        rfirst = 0;
        break;
    case 2:
        if (R_ALPHA(c)) { if (rlen < 7) rword[rlen++] = c; break; }
        if (c == '-') { rneg = rhas = 1; rst = 3; break; }
        if (c >= '0' && c <= '9') { rhas = 1; rnum = c - '0'; rst = 3; break; }
        rst = 0; r_word(); if (c != ' ') r_byte(c);
        break;
    case 3:
        if (c >= '0' && c <= '9') { rnum = rnum * 10 + (c - '0'); break; }
        rst = 0; r_word(); if (c != ' ') r_byte(c);
        break;
    case 4:
        rhex = r_hex(c) << 4; rst = 5;
        break;
    default:
        rst = 0;
        rhex |= r_hex(c);
        rnum = rhex;                              /* pas de ?: (cc65 tronque a 8 bits) */
        if (rhex >= 0x80 && rhex < 0xA0) rnum = cp1252_hi[rhex & 31];
        if (rcp == 1250) {                        /* Windows-1250 : csak a magyar ő Ő ű Ű kulonbozik a 1252-tol */
            c = rhex & 0xDF;
            if (c == 0xD5) rnum = 0x150 | ((rhex >> 5) & 1);
            else if (c == 0xDB) rnum = 0x170 | ((rhex >> 5) & 1);
        }
        r_uni(rnum);
    }
}

/* Charge un fichier RTF comme nouveau document : le texte converti s'empile
   dans la banque 0, puis passe en bloc dans le suffixe (comme load_big). */
/* Az RTF / szoveg megnyitasa harom reszben (rtf_open) : a / c resz a 10. overlayben, a b a fo ciklus az 1.-ben. */
static unsigned char rl_ok, rl_first, rl_go;

static void load_rtf_b(void)
{
    unsigned k;
    int n;
    for (;;) {
        if (sd_on) {
            n = SD_SIZE - spos > 256 ? 256 : (int)(SD_SIZE - spos);
            if (n > 0) { reu_move(1, outbuf, rlim + spos, n); spos += n; }
        } else n = cbm_read(2, outbuf, 256);
        if (n <= 0) break;
        if (rl_first) {
            rl_first = 0;
            if (!rtxt && (n < 5 || memcmp(outbuf, "{\\rtf", 5))) { rl_ok = 0; break; }
        }
        if (rtxt) for (k = 0; k < (unsigned)n; ++k) t_byte(outbuf[k]);
        else for (k = 0; k < (unsigned)n; ++k) r_byte(outbuf[k]);
        pg_add((unsigned)n);
        if (rfull) break;
    }
    if (rl_ok) r_flush();
}
#pragma rodata-name (pop)
#pragma code-name (pop)
#pragma code-name (push, "OVERLAY10")
#pragma rodata-name (push, "OVERLAY10")
static void load_rtf_a(void)
{
    rl_go = 0;
    rlim = suf_end;
    if (!sd_on) scan_dir(filename);             /* la taille du fichier (en blocs) pour la barre de progression */
    disk_busy(1);
    if (fo_r(filename) != 0) { disk_busy(0); disk_status(); show_disk_error(); return; }
    if (sd_on) {                                /* SD : tout le fichier d'un coup dans le REU (DMA), traite ensuite */
        sdk(sd_size());
        if (SD_SIZE + 8192UL > suf_end) { fc(); disk_busy(0); message(S_OPENRTF, S_FULL, 0, 0); return; }
        rlim = suf_end - SD_SIZE;
        SD_REU = rlim;
        SD_RLEN = SD_SIZE;
        sdk(sd_loadreu());
        fc();
        spos = 0;
        blk_old = (unsigned)(SD_SIZE >> 8);
    }
    win_base = suf_len = lines_before = 0; lines_exact = 1; ix_reset();
    win_style = sc_ok = doclen = 0;
    first_mark = 0xFFFFFFFFUL;
    pg_begin(S_LOADING, (dpos)blk_old << 8);
    rcur = remit = rdepth = rfirst = rst = rskip = rfull = rhex = rneg = 0;
    ruc = 1;
    rcp = 0;
    rl_ok = rl_first = rl_go = 1;
}

static void load_rtf_c(void)
{
    unsigned k;
    dpos off, total;
    if (!sd_on) cbm_close(2);
    disk_busy(0);
    pg_end();
    op_ok = rl_ok;
    if (rl_ok) {
        total = win_base;
        for (off = total; off; ) {              /* de la fin au debut : les zones peuvent se recouvrir */
            k = off > 256 ? 256 : (unsigned)off;
            off -= k;
            reu_move(1, outbuf, off, k);
            reu_move(0, outbuf, suf_end - total + off, k);
        }
        suf_len = total;
        win_base = doclen = 0;
        doc_fmt = rtxt ? 2 : 1;                 /* garde le nom : F1 enregistre dans le meme format au meme endroit */
    } else {
        message(S_OPENRTF, S_BADRTF, 0, 0);
        filename[0] = 0;
    }
    cursor = 0;
    anchor = NOSEL;
    top = 0;
    dirty = 0;                                  /* a megnyitott fajl modositatlan (nincs csillag, a billentyuzet nem vilagit) */
    layout_all();
    pull_tail();
    style = style_before(0);
    if (rfull) message(S_OPENRTF, S_FULL, 0, 0);
}

#pragma rodata-name (pop)
#pragma code-name (pop)
/* L'ecriture RTF est dans l'overlay 6 (avec save_doc et load_named) : l'overlay 1 est plein. */
#pragma code-name (push, "OVERLAY6")
#pragma rodata-name (push, "OVERLAY6")
/* Unicode des codes internes 128 a 189 (copie de ucode, pour cet overlay). */
static const unsigned ucode_w[62] = {
    0xE0, 0xE2, 0xE4, 0xE7, 0xE9, 0xE8, 0xEA, 0xEB, 0xEE, 0xEF, 0xF4, 0xF6, 0xF9, 0xFB, 0xFC,
    0xC0, 0xC2, 0xC7, 0xC9, 0xC8, 0xCA, 0xCB, 0xCE, 0xCF, 0xD4, 0xD9, 0xDB, 0xDC,
    0x153, 0x152, 0xAB, 0xBB, 0xB0, 0x20AC, 0xE6, 0xF8, 0xE5, 0xC6, 0xD8, 0xC5,
    0xE1, 0xC1, 0xED, 0xCD, 0xF3, 0xD3, 0xFA, 0xDA, 0x151, 0x150, 0x171, 0x170, 0xD6, 0x2190,
    0x201E, 0x201D, 0x201C, 0x2018, 0x2019, 0x2013, 0x2014, 0x2022
};
/* Ecrit tout le document en RTF (octets non ASCII : \uN?). */
static unsigned char *w_str(unsigned char *o, const char *s)
{
    strcpy((char *)o, s);
    return o + strlen(s);
}

static dpos rtf_tot;                            /* essai a blanc : octets comptes */
static unsigned char rtf_dry;
#ifdef REUTEST
static char rtfname[22];
#endif

static void rtf_put(unsigned n)
{
    if (rtf_dry) rtf_tot += n; else { fw(outbuf, n); pg_add(n); }
}

static void rtf_body(void)
{
    unsigned char *wbuf = outbuf + 400;          /* entree : 64 octets en fin de outbuf, sortie en debut */
    unsigned char *o, c, i, ws = 0, ns;
    unsigned u;
    dpos g, tot;
    unsigned k;
    o = outbuf;
    if (!rtf_txt) o = w_str(outbuf, "{\\rtf1\\ansi\\ansicpg1252\\deff0{\\fonttbl{\\f0 Times New Roman;}}\\f0\\fs24 ");
    tot = doc_total();
    for (g = 0; g < tot; g += k) {
        k = tot - g > 64 ? 64 : (unsigned)(tot - g);
        doc_read(wbuf, g, k);
        for (i = 0; i < k; ++i) {
            c = wbuf[i];
            if (o > outbuf + 300) { rtf_put((unsigned)(o - outbuf)); o = outbuf; }
            if (IS_MARK(c)) {
                if (rtf_txt) continue;
                ns = c & 7;
                if (ns == ws) continue;
                if ((ns ^ ws) & BOLD) o = w_str(o, ns & BOLD ? "\\b" : "\\b0");
                if ((ns ^ ws) & ITALIC) o = w_str(o, ns & ITALIC ? "\\i" : "\\i0");
                if ((ns ^ ws) & UNDERLINE) o = w_str(o, ns & UNDERLINE ? "\\ul" : "\\ulnone");
                *o++ = ' ';
                ws = ns;
            } else if (c == CR) {
                o = w_str(o, rtf_txt ? "\n" : "\\par\n");
            } else if (c >= 128) {
                u = c - 128u < sizeof ucode_w / 2 ? ucode_w[c - 128] : '?';
                if (!rtf_txt) {
                    o = w_str(o, "\\u");
                    o = (unsigned char *)utoa_simple((char *)o, u);
                    *o++ = '?';
                } else if (u < 0x80) *o++ = (unsigned char)u;
                else if (u < 0x800) { *o++ = (unsigned char)(0xC0 | (u >> 6)); *o++ = (unsigned char)(0x80 | (u & 0x3F)); }
                else { *o++ = (unsigned char)(0xE0 | (u >> 12)); *o++ = (unsigned char)(0x80 | ((u >> 6) & 0x3F)); *o++ = (unsigned char)(0x80 | (u & 0x3F)); }
            } else if (c >= 32) {
                if (!rtf_txt && (c == '\\' || c == '{' || c == '}')) *o++ = '\\';
                *o++ = c;
            }
        }
    }
    if (!rtf_txt) *o++ = '}';
    rtf_put((unsigned)(o - outbuf));
}

/* name : tampon stable (rtfname) ; un premier passage a blanc donne la taille. */
static unsigned char write_rtf(char *name)
{
    rtf_dry = 1;
    rtf_tot = 0;
    rtf_body();
    rtf_dry = 0;
    if (!fit_check(rtf_tot, name)) return 0;
    pg_begin(S_SAVING, rtf_tot);
    disk_busy(1);
    if (fo_w() == 0) {
        rtf_body();
        fc();
    }
    disk_busy(0);
    pg_end();
    if (disk_status() >= 20) { show_disk_error(); return 0; }
    dirty = 0;
    draw_status();
    op_ok = 1;
    return 1;
}

#pragma rodata-name (pop)
#pragma code-name (pop)

static void rtf_open(void)
{
    ovl_load(10);
    load_rtf_a();
    if (!rl_go) return;
    ovl_load(1);
    load_rtf_b();
    ovl_load(10);
    load_rtf_c();
}
static void txt_open(void) { rtxt = 1; rtf_open(); rtxt = 0; }
static unsigned char rtf_write(char *name)
{
    unsigned char r;
    op_ok = 0;
    ovl_load(6);
    r = write_rtf(name);
    if (op_ok) bck_saved();
    return r;
}
static unsigned char txt_write(char *name)
{
    unsigned char r;
    rtf_txt = 1;
    r = rtf_write(name);
    rtf_txt = 0;
    return r;
}
#endif

#ifdef REU
/* ------------------------------------------------------------------ */
/* Sauvegarde rapide : le fichier NOM.bck contient le journal des modifications depuis le dernier
   enregistrement complet. Au chargement, il est rejoue sur le fichier de base. Overlay 3. */
#pragma code-name (push, "OVERLAY3")
#pragma rodata-name (push, "OVERLAY3")

static char bckname[20];
static dpos base_len;                           /* le document tel qu'il est au fichier de base */
static unsigned base_sig;

/* SD : le nom du fichier devient celui du .bck (on = 1), puis redevient le nom du document (on = 0). */
static void sd_bck(unsigned char on)
{
    char *e;
    if (on) {
        e = strrchr(SD_NAME, '.');
        if (!e) e = SD_NAME + strlen(SD_NAME);
        SD_PE = (unsigned char)(e - SD_NAME);
        strncpy(SD_SV, e, 7);
        SD_SV[7] = 0;
        strcpy(e, ".bck");
    } else strcpy(SD_NAME + SD_PE, SD_SV);
}

static void make_bck(void)
{
    unsigned char i = 0;
    while (filename[i] && filename[i] != '.' && i < 12) { bckname[i] = filename[i]; ++i; }
    bckname[i] = 0;
    strcat(bckname, ".\x42\x43\x4B");                /* ".bck" en PETSCII */
}

/* Signature : trois echantillons de 128 octets (debut, milieu, fin) et la longueur.
   Attention : pas de ?: imbriques sur des dpos (cc65 tronque a 16 bits avec extension de signe :
   l'echantillon de fin lisait une adresse REU hors du document, donc differente a chaque session). */
static unsigned doc_sig(void)
{
    dpos t = doc_total(), g;
    unsigned sig = (unsigned)t;
    unsigned char i, k, j;
    for (j = 0; j < 3; ++j) {
        g = 0;
        if (j == 1) g = t / 2;
        else if (j == 2 && t > 128) g = t - 128;
        k = 128;
        if (t - g < 128) k = (unsigned char)(t - g);
        doc_read(outbuf, g, k);
        for (i = 0; i < k; ++i) sig = ((sig << 1) | (sig >> 15)) ^ outbuf[i];
    }
    return sig;
}

/* Le document est identique a son fichier : le journal repart de zero. */
static void base_set(unsigned char keep)
{
    base_len = doc_total();
    base_sig = doc_sig();
    if (!keep) jr_clear();                      /* enregistrement : le journal reste (annuler), seul son debut avance */
    jb = jr_len;
    jr_ld = 0xFFFF;                             /* pas de prolongation d'un enregistrement d'avant la base */
    jr_ok = filename[0] != 0;
}

/* Efface le .bck (cmdname recoit la commande de suppression). */
static void bck_del(void)
{
    if (sd_on) { sd_bck(1); sd_delete(); sd_bck(0); sd_bad = 0; return; }
    make_bck();
    strcpy(cmdname, "S0:");
    strcat(cmdname, bckname);
    cbm_open(15, dev, 15, cmdname);
    cbm_close(15);
}

/* Apres un enregistrement complet : nouvelle base, et le .bck n'a plus lieu d'etre. */
static void bck_saved_(void)
{
    base_set(1);
    if (jr_ok) bck_del();
}

static void quick_save_(void)
{
    unsigned char *hd = (unsigned char *)inputbuf;      /* a fejlec 13 bajtja a (most szabad) beviteli pufferben */
    unsigned char again = 1;
    if (jr_len == jb) { bck_del(); dirty = 0; shown_dirty = 255; draw_status(); return; }
    make_bck();
    if (!fit_check(13UL + jr_len - jb, bckname)) return;
    memcpy(hd, "PBK3", 4);
    hd[4] = (unsigned char)base_len; hd[5] = (unsigned char)(base_len >> 8);
    hd[6] = (unsigned char)(base_len >> 16); hd[7] = (unsigned char)(base_len >> 24);
    hd[8] = (unsigned char)base_sig; hd[9] = (unsigned char)(base_sig >> 8);
    hd[10] = (unsigned char)(jr_len - jb); hd[11] = (unsigned char)((jr_len - jb) >> 8); hd[12] = (unsigned char)((jr_len - jb) >> 16);
    for (;;) {
        sd_bad = 0;                             /* egy korabbi, mar elintezett SD-hiba ne szamitson bele */
        if (sd_on) sd_bck(1);
        pg_begin(S_SAVING, jr_len - jb);
        disk_busy(1);
        if (fo_w() == 0) {
            fw(hd, 13);
            write_reu(JR_BASE + jb, jr_len - jb);
            fc();
        }
        if (sd_on) sd_bck(0);
        disk_busy(0);
        pg_end();
        if (disk_status() < 20) break;
        if (!sd_on || !again) { show_disk_error(); return; }
        again = 0;                              /* az SD (UCI) atmeneti hibaja : egyszer ujraprobaljuk */
    }
    dirty = 0;
    shown_dirty = 255;
    draw_status();
}

/* Rejoue les enregistrements du .bck (canal 2 ouvert, apres l'en-tete). */
static void bck_replay(dpos total)
{
    static unsigned char h[7];
    static dpos gp;
    static unsigned del, ins, k;
    static dpos done;
    done = 0;
    while (done + 7u <= total && fr(h, 7) == 7) {
        gp = h[0] | ((dpos)h[1] << 8) | ((dpos)h[2] << 16);
        del = h[3] | ((h[4] & 0x3F) << 8);
        ins = h[5] | (h[6] << 8);
        jr_blk = (h[4] & 0x80) ? ((h[4] & 0x40) ? 1 : 2) : 0;       /* les operations en bloc restent des blocs */
        done += 7 + ins;
        if (done > total) break;
        if (gp > win_base + doclen + suf_len) break;
        if (gp + del > win_base + doclen) relocate(para_start(gp > 400 ? gp - 400 : 0));
        if (gp < win_base || gp + del > win_base + doclen) relocate(para_start(gp > 400 ? gp - 400 : 0));
        if (del && !replace((unsigned)(gp - win_base), (unsigned)(gp - win_base) + del, 0, 0, 0)) break;
        while (ins) {
            k = ins > 400 ? 400 : ins;
            if (MAXDOC - doclen < ROOM + 400 || gp < win_base || gp > win_base + doclen)
                relocate(para_start(gp > 400 ? gp - 400 : 0));      /* avant de lire : para_start se sert d'outbuf */
            if (fr(outbuf, k) != (int)k) return;
            if (!replace((unsigned)(gp - win_base), (unsigned)(gp - win_base), outbuf, k, 0)) return;
            gp += k;
            ins -= k;
        }
    }
}

/* Apres le chargement d'un fichier : s'il y a un .bck qui lui correspond, on le rejoue. */
static void bck_loaded_(void)
{
    static unsigned char hd[13];
    static dpos total;
    unsigned char good = 0, ok;
    base_set(0);
    if (!jr_ok) return;
    if (bck_skip) return;                       /* RESTORE+RUN/STOP : le fichier seul */
    make_bck();
    if (sd_on) sd_bck(1);
    disk_busy(1);
    if (fo_r(bckname) == 0) {
        if (fr(hd, 13) == 13 && !memcmp(hd, "PBK", 3) && (hd[3] == '2' || hd[3] == '3')) {
            total = hd[10] | ((dpos)hd[11] << 8) | ((dpos)hd[12] << 16);
            /* PBK2 : sa signature etait faussee (voir doc_sig), seule la longueur compte */
            ok = base_len == (hd[4] | ((dpos)hd[5] << 8) | ((dpos)hd[6] << 16) | ((dpos)hd[7] << 24))
                 && (hd[3] == '2' || base_sig == (hd[8] | (hd[9] << 8)));
            if (!ok) {                          /* jamais de perte silencieuse : on propose de l'appliquer */
                disk_busy(0);
                ok = message(S_OPEN, S_BCKBAD, S_BCKASK, 1);
                disk_busy(1);
            }
            if (ok) {
                quiet = 1;
                bck_replay(total);
                jr_blk = 0;
                quiet = 0;
                good = 1;
            }
        }
        fc();
    }
    if (sd_on) sd_bck(0);
    disk_busy(0);
    disk_status();                              /* efface une erreur "fichier absent" */
    if (good) {
        goto_global(0, 0);
        cursor = top = 0;
        anchor = NOSEL;
        layout_all();
        style = style_before(0);
        dirty = 0;
        message(S_OPEN, S_RESTORED, 0, 0);
    }
}
#pragma rodata-name (pop)
#pragma code-name (pop)

/* ------------------------------------------------------------------ */
/* Annuler / refaire (overlay 4). Annuler defait le dernier enregistrement du journal : les octets
   inseres sont supprimes, les octets supprimes (banque R) reviennent ; une frappe = une lettre par pas.
   Le journal et la banque R gardent ce qui a ete defait : refaire le rejoue (pile de 255 pas au plus
   a la fin de la banque R, vidée par toute nouvelle modification). */
#pragma code-name (push, "OVERLAY4")
#pragma rodata-name (push, "OVERLAY4")
static unsigned char uh[7];
static dpos ug;
static unsigned udel, uins;
static unsigned ua;                             /* position (fenetre) apres la modification */
static dpos ugp;                                /* idem, globale */

static unsigned char uf;                        /* drapeaux de l'enregistrement lu */

static void u_hdr(dpos at)
{
    reu_move(1, uh, JR_BASE + at, 7);
    ug = uh[0] | ((dpos)uh[1] << 8) | ((dpos)uh[2] << 16);
    udel = uh[3] | ((uh[4] & 0x3F) << 8);
    uf = uh[4] & 0xC0;
    uins = uh[5] | (uh[6] << 8);
}

/* Le dernier enregistrement disparait : le precedent devient le dernier (son debut est dans la pile L). */
static void u_pop(void)
{
    rl -= udel;
    jr_len = jr_last;
    if (jr_n) --jr_n;
    if (jr_n) {
        jr_last = 0;
        reu_move(1, &jr_last, l_base + 3UL * (jr_n - 1), 3);
        u_hdr(jr_last);
        jr_lp = ug;
        jr_ld = jr_len <= jb ? 0xFFFF : udel;
        jr_li = uins;
        jr_lf = uf;
    }
    if (jr_len < jb) jr_ok = 0;
}

/* La fenetre doit contenir [g, g + n). */
static unsigned char u_win(dpos g, unsigned n)
{
    if (g < win_base || g + n > win_base + doclen) relocate(para_start(g > 400 ? g - 400 : 0));
    ugp = g;
    ua = (unsigned)(g - win_base);
    return g >= win_base && g + n <= win_base + doclen;     /* un paragraphe trop long peut laisser g hors de la fenetre */
}

/* Supprime n octets a partir de la position globale g, fenetre apres fenetre (n peut depasser la fenetre). */
static void u_del(dpos g, unsigned n)
{
    unsigned d;
    while (n) {
        if (g < win_base || g >= win_base + doclen) relocate(para_start(g > 400 ? g - 400 : 0));
        ua = (unsigned)(g - win_base);
        if (g < win_base || ua >= doclen) break;
        d = doclen - ua;
        if (d > n) d = n;
        if (!replace(ua, ua + d, 0, 0, 0)) break;
        n -= d;
    }
    u_win(g, 0);
}

/* Insere n octets du REU (adresse ra) a la position globale ugp, par morceaux. */
static void u_put(dpos ra, unsigned n)
{
    unsigned k, m;
    for (k = 0; k < n; k += m) {
        m = n - k > 400 ? 400 : n - k;
        if (MAXDOC - doclen < ROOM + 400) {     /* fenetre pleine : elle repart juste avant ugp */
            relocate(para_start(ugp > 400 ? ugp - 400 : 0));
            ua = (unsigned)(ugp - win_base);
        } else if (ugp < win_base || ugp > win_base + doclen) u_win(ugp, 0);
        reu_move(1, outbuf, ra + k, m);         /* apres le deplacement de la fenetre : para_start se sert d'outbuf */
        if (!replace(ua, ua, outbuf, m, 0)) break;
        ua += m;
        ugp += m;
    }
}

static void u_end(void)
{
    unsigned k;
    quiet = jr_off = 0;
    cursor = ua;
    anchor = NOSEL;
    dirty = 1;
    layout_all();
    k = line_of(cursor);
    if (k < top || k >= top + VISIBLE) top = k > 4 ? k - 4 : 0;
    style = style_before(cursor);
    draw_text_area();
}

static void u_none(void)
{
#ifdef LANG_EN
    message(S_UNDO, "Nothing to undo.", 0, 0);
#else
    message(S_UNDO, "Rien " C_AGRAVE " annuler.", 0, 0);
#endif
}

static void u_far(void)
{
    quiet = jr_off = 0;
    layout_all();
    draw_text_area();
#ifdef LANG_EN
    message(S_UNDO, "Too far from the window.", 0, 0);
#else
    message(S_UNDO, "Hors de la fen" C_ECIRC "tre.", 0, 0);
#endif
}

/* Annule un enregistrement entier (journal, banque R). Renvoie 0 si la fenetre ne peut pas l'atteindre. */
static unsigned char u_full(void)
{
    if (!u_win(ug, 0)) return 0;
    u_del(ug, uins);
    u_put(R_BASE + rl - udel, udel);
    u_pop();
    return 1;
}

static void undo_(void)
{
    static unsigned char part, e, cont;
    static unsigned k;
    for (;;) {
        if (!jr_n) { u_none(); return; }
        u_hdr(jr_last);
        if (udel || uins) break;
        u_pop();                                /* enregistrement vide (frappe effacee) : on passe */
    }
    quiet = jr_off = 1;
    part = e = 0;
    if (!udel && uins > 1 && !(uf & 0x80)) {    /* frappe : une lettre par pas (avec son marqueur de style) */
        if (!u_win(ug + uins - 2, 2)) { u_far(); return; }   /* les deux derniers octets de la frappe */
        k = IS_MARK(text[ua]) ? 2 : 1;
        if (k < uins) part = (unsigned char)k;
    }
    if (part) {
        if (replace(ua + 2 - part, ua + 2, 0, 0, 0)) {
            jr_lp = ug; jr_ld = 0; jr_li = uins - part;
            jr_len -= part;
            jr_hdr();
            if (jr_len < jb) jr_ok = 0;
        }
        ua += 2 - part;
        e = 0x80 | part;
    } else {
        do {                                    /* un bloc (coller, grande suppression) = plusieurs enregistrements */
            cont = uf & 0x40;
            if (!u_full()) { u_far(); return; }
            ++e;
        } while (cont && jr_n);
    }
    if (redo_n >= d_max) redo_n = 0;
    reu_move(0, &e, d_base + redo_n, 1);
    ++redo_n;
    u_end();
}

static void redo_(void)
{
    static unsigned char e;
    static unsigned k;
    static dpos g0;
    if (!redo_n) {
#ifdef LANG_EN
        message(S_REDO, "Nothing to redo.", 0, 0);
#else
        message(S_REDO, "Rien " C_AGRAVE " refaire.", 0, 0);
#endif
        return;
    }
    reu_move(1, &e, d_base + redo_n - 1, 1);
    quiet = jr_off = 1;
    if (e & 0x80) {                             /* une lettre de plus dans l'enregistrement de frappe */
        e &= 0x7F;
        u_hdr(jr_last);
        if (!u_win(ug + uins, 0)) { u_far(); return; }
        u_put(JR_BASE + jr_len, e);
        jr_lp = ug; jr_ld = 0; jr_li = uins + e;
        jr_len += e;
        jr_hdr();
    } else {                                    /* les enregistrements defaits reviennent, dans l'ordre */
        while (e--) {
            u_hdr(jr_len);
            g0 = ug;
            k = uins;
            if (!u_win(g0, 0)) { u_far(); return; }
            u_del(g0, udel);
            u_put(JR_BASE + jr_len + 7, k);
            jr_last = jr_len;
            ++jr_n;                             /* son debut est encore dans la pile L */
            jr_lp = g0; jr_ld = udel; jr_li = k; jr_lf = uf;
            jr_len += 7 + k;
            rl += udel;
        }
    }
    --redo_n;
    u_end();
}
#pragma rodata-name (pop)
#pragma code-name (pop)
static void undo(void) { ovl_load(4); undo_(); }
static void redo(void) { ovl_load(4); redo_(); }

static void quick_save(void) { ovl_load(3); quick_save_(); }
static void bck_saved(void) { ovl_load(3); bck_saved_(); }
static void bck_loaded(void) { ovl_load(3); bck_loaded_(); }

#ifdef RTF
/* Texte seul : apres l'ecriture, la mise en forme disparait aussi de l'ecran et de la memoire : on recharge le fichier ecrit. */
static void txt_strip(void)
{
    op_ok = 0;
    load_any();
    if (op_ok) bck_loaded();
    redraw_all();
    op_ok = 1;
}

/* Enregistrer sous : le selecteur donne le nom, l'endroit (disque / SD) et le format (PLM, RTF, TXT). */
static void save_as(void)
{
    unsigned char r, was = sd_on, wf = doc_fmt, f, lose = has_fmt();   /* lose : van formazas */
    r = choose(S_SAVEAS, 1);
    redraw_all();
    if (!r) return;
    f = fmt_sel;
    if (f && !reu_on) { message(S_SAVEAS, S_NEEDREU, 0, 0); return; }
    sd_on = sd_sel;
    op_ok = 0;
    if (f == 1) rtf_write(filename);
    else if (f == 2) txt_write(filename);
    else { doc_fmt = 0; save_doc(); op_ok = !dirty; }
    if (op_ok) {
        doc_fmt = f;
        if (f == 2 && lose) txt_strip();        /* a formazas a kepernyorol es a memoriabol is eltunik */
        shown_dirty = 255;                      /* a cim (az uj nev) azonnal frissul */
        draw_status();
    } else { sd_on = was; doc_fmt = wf; }
}
#endif
/* RESTORE : annuler ; avec Shift : refaire ; avec RUN/STOP : le fichier revient du disque, sans le .bck
   (le .bck reste sur le disque) ; Shift + RUN/STOP : le fichier et son .bck rejoue. */
static void reload_doc(unsigned char with_bck)
{
    if (!filename[0] || !jr_ok) return;
    if (dirty && !message(with_bck ? S_RELOADB : S_RELOAD, S_RELOAD1, S_LOSE, 1)) return;
    op_ok = 0;
    load_any();
    if (op_ok) { bck_skip = !with_bck; bck_loaded(); bck_skip = 0; }
    redraw_all();
}

static void restore_key(void)
{
    handle_key((PEEK(0x028D) & 1) ? 0x19 : 0x1A);       /* RESTORE : annuler ; Shift + RESTORE : refaire */
}

/* F1 : sauvegarde rapide, ou enregistrement complet quand il n'y a pas encore de fichier de base. */
static void f1_save(void)
{
    if (filename[0] && jr_ok) quick_save(); else save_doc();
}
#endif

/* ------------------------------------------------------------------ */
/* Menus                                                               */

/* Couleurs suivantes : ecran et, sur REU, les leds du C64U. */
static void cycle_theme(void)
{
    next_theme();
    redraw_all();
#ifdef REU
    led_theme(theme_no);
#endif
}

#ifdef REU
#pragma code-name (push, "OVERLAY8")
#pragma rodata-name (push, "OVERLAY8")
#endif
#ifdef REU
#pragma rodata-name (pop)
#pragma code-name (pop)
#pragma code-name (push, "OVERLAY9")
#pragma rodata-name (push, "OVERLAY9")
/* ---- A fajlvalaszto jovahagyasi es meret-lepesei (9. overlay) ---- */
#pragma rodata-name (pop)
#pragma code-name (pop)
#pragma code-name (push, "OVERLAY4")
#pragma rodata-name (push, "OVERLAY4")
/* Meretkod az SD_SIZE-bol (16 bites szamitassal, a bajtokbol): a felso 2 bit az egyseg (0 K, 1 M, 2 G), az also 14 bit az ertek (<= 999; 999K felett a kovetkezo egyseg). */
static unsigned size_code(void)
{
    unsigned char *q = (unsigned char *)&SD_SIZE;
    unsigned k = ((unsigned)q[2] << 6) | (q[1] >> 2), m;
    if (q[0] || (q[1] & 3)) ++k;                                    /* felfele kerekites K-ra (16 MB alatt) */
    if (!q[3] && q[2] < 16) {
        if (k < 1000) return k;
        return 0x4000 | ((k + 512) >> 10);
    }
    m = ((unsigned)q[3] << 4) | (q[2] >> 4);      /* MB, 16 MB felett */
    if (m < 1000) return 0x4000 | m;
    return 0x8000 | ((m + 512) >> 10);
}

/* A fajlvalaszto kerete, cime es gombjai (csak elso rajzolaskor) : a 4. overlayben, hogy a 7. overlayben legyen hely. */
static void pick_frame_(const char *title, unsigned char newbtn)
{
    if (pk_first == 1) {                /* 2 : csak a gombsor (hely- vagy formatumvaltas) */
        panel(3, 20, RQ_X, RQ_W);
        put_text(RQ_X + 16, 4, title, BOLD);
        button(RQ_X + RQ_W - 24 - BTN_W, 20, S_CANCEL);
        if (newbtn) button(RQ_X + 24, 20, S_NEWFILE);
    }
    if (pk_btn && sd_ok) {              /* hely: Disk / C64U (kulon gombok, a kivalasztott lenyomott) */
        button_w(24, 7, 48, dlabel, !sd_pk);
        button_w(80, 7, 48, "C64U", sd_pk);
    }
    if (pk_btn && pk_save) {            /* formatum: PLM RTF TXT */
        button_w(136, 7, 48, "PLM", pk_fm == 0);
        button_w(192, 7, 48, "RTF", pk_fm == 1);
        button_w(248, 7, 48, "TXT", pk_fm == 2);
    }
}

static void rq_sizes_(void)                    /* a LATHATO SD-fajlok merete (az UCI a lista olvasasa utan szabad) */
{
    unsigned char r, pc;
    strcpy(sd_tmp, SD_NAME);                    /* a dokumentum sajat neve */
    pc = PEEK(0xCB00);
    POKE(0xCB00, 0);                             /* a konyvtar mar be van allitva : nincs ujabb chdir */
    for (r = 0; r < nfiles; ++r) {
        if (fsize[r]) continue;
        io_hide();
        strcpy(SD_NAME, rawn[r]);
        io_show();
        if (!sd_open(1)) {
            sd_size();
            fsize[r] = size_code();
            sd_close();
        }
    }
    POKE(0xCB00, pc);                           /* (SD_PATH[0] = pc : a cc65 itt egy elavult mutatora irt) */
    strcpy(SD_NAME, sd_tmp);
    sd_bad = 0;
}
#pragma rodata-name (pop)
#pragma code-name (pop)
#pragma code-name (push, "OVERLAY9")
#pragma rodata-name (push, "OVERLAY9")
static const char fext[3][4] = { "PLM", "RTF", "TXT" };
/* Letezik-e az sd_cand nevu fajl az aktualis SD-mappaban ? (a teljes listat vegigolvassa) */
static unsigned char sd_exists(void)
{
    unsigned char f = 0;
    if (sd_dirbeg() == 0)
        while (!sd_dirnext()) if (!stricmp((char *)SD_ENT + 1, sd_cand)) f = 1;
    return f;
}
/* Uj nev kiterjesztese / letezes-ellenorzese (newname), jovahagyasok, a dokumentum uj neve. buf = outbuf+64. Vissza: 1 kesz, 0 a valaszto folytatodik. */
static unsigned char file_commit_(const char *title, unsigned char save, unsigned char newname)
{
    unsigned char i, c, ex = 1;
    char *e, *buf = (char *)outbuf + 64;
    if (newname) {
        e = strrchr(buf, '.');                  /* mindig van kiterjesztes : ha nincs, a formatumae */
        if (!e) { strcat(buf, "."); strcat(buf, fext[pk_fm]); }
        else if (!e[1]) strcat(buf, fext[pk_fm]);
        if (sd_pk) {
            for (i = 0; (c = (unsigned char)buf[i]) != 0; ++i) {
                if (c >= 0xC1 && c <= 0xDA) c -= 0x80;
                else if (c >= 0x41 && c <= 0x5A) c += 0x20;
                else if (c >= 0x80) c = '_';
                sd_cand[i] = (char)c;
            }
            sd_cand[i] = 0;
            ex = sd_exists();
        } else {
            scan_dir(buf);
            ex = blk_old != 0;
        }
    }
    if (save && ex && !message(title, S_EXISTS, S_REPLACE, 1)) return 0;     /* felulirasi megerosites */
    if (save && pk_fm == 2 && has_fmt()                                       /* szoveg: a formazas elvesz */
        && !message(S_SAVETXT, S_TXTLOSS, S_TXTASK, 1)) return 0;
    strncpy(filename, buf, 16);                 /* a dokumentum uj neve (lemezen 16 jegyu ; SD-n a cimsor a SD_NAME-bol rajzol) */
    filename[16] = 0;
    if (sd_pk) strcpy(SD_NAME, sd_cand);
    sd_sel = sd_pk;
    fmt_sel = pk_fm;
    return 1;
}

#pragma rodata-name (pop)
#pragma code-name (pop)
#pragma code-name (push, "OVERLAY9")
#pragma rodata-name (push, "OVERLAY9")
/* Szovegbekeres (a Find es a fajlvalaszto "Uj..." gombja) : a 9. overlayben. */
#ifdef REU
#define AT_B 9                                  /* a gombok kozepso sora */
#else
#define AT_B 5
#endif
static unsigned char ask_text_(const char *title, const char *label, char *buf, unsigned char max,
                              unsigned char petscii)
{
    char *shown = inputbuf;
    unsigned char len = (unsigned char)strlen(buf), c, first = 1;
    for (;;) {
        if (first) {
#ifdef REU
            dialog(12, title);
            put_text(DLG_X + 16, DLG_ROW + 3, label, 0);
            outline_box(DLG_ROW + 4, 3, DLG_X + 16, DLG_W - 32);
#else
            dialog(8, title);
            put_text(DLG_X + 16, DLG_ROW + 2, label, 0);
#endif
            button(BTN_OK_X, DLG_ROW + AT_B, S_OK);
            button(BTN_CANCEL_X, DLG_ROW + AT_B, S_CANCEL);
            first = 0;
        }
#ifdef REU
        clear_cells(DLG_ROW + 5, DLG_X + 24, DLG_W - 48);
#else
        clear_cells(DLG_ROW + 3, DLG_X + 8, DLG_W - 16);
        hline(DLG_ROW + 3, DLG_X + 16, DLG_W - 32, 7);
#endif
        if (petscii) petscii_to_text(shown, buf); else strcpy(shown, buf);
        strcat(shown, "|");
#ifdef REU
        put_text(DLG_X + 24, DLG_ROW + 5, shown, BOLD);
#else
        put_text(DLG_X + 16, DLG_ROW + 3, shown, BOLD);
#endif
        do {
            poll_mouse();
            key = read_key();
            if (on_button(BTN_OK_X, DLG_ROW + AT_B)) key = CR;
            if (on_button(BTN_CANCEL_X, DLG_ROW + AT_B)) key = 3;
        } while (!key);
        if (key == CR && len) { dialog_close(); return 1; }
        if (key == 3) { dialog_close(); return 0; }
        if (key == 0x14) { if (len) buf[--len] = 0; continue; }
        if (len >= max) continue;
#ifdef REU
        if (at12 && len >= 12 && key != '.' && !strchr(buf, '.')) continue;   /* lemezen kiterjesztes nelkul 12 jegy, a pont meg jon */
#endif
        if (petscii) {
            if (((key >= 0x20 && key <= 0x5A) || (key >= 0xC1 && key <= 0xDA))
                && key != ',' && key != ':' && key != '*' && key != '?' && key != '"' && key != '=') {
                buf[len++] = key;
            }
        } else if ((c = translate(key)) != 0) {
            buf[len++] = c;
        }
        buf[len] = 0;
    }
}

#pragma rodata-name (pop)
#pragma code-name (pop)
#pragma code-name (push, "OVERLAY6")
#pragma rodata-name (push, "OVERLAY6")
/* Nev, verzio, keszitok (tobb sor, mint a message()-ben). */
static void about_(void)
{
    unsigned char r = DLG_ROW + 3;
#ifdef KBD_HU
#define AB_L 6
#else
#define AB_L 5
#endif
    dialog(AB_L + 8, S_ABOUT);
    put_text(DLG_X + 16, r++, S_ABOUT1, BOLD);
    put_text(DLG_X + 16, r++, S_ABOUT2, 0);
    put_text(DLG_X + 16, r++, S_ABOUT3, 0);
    put_text(DLG_X + 16, r++, S_ABOUT5, 0);
#ifdef KBD_HU
    put_text(DLG_X + 16, r++, S_ABOUT6, 0);
#endif
    put_text(DLG_X + 16, r, S_ABOUT4, 0);
    button(BTN_OK_X + 56, DLG_ROW + AB_L + 5, S_OK);
    for (;;) {
        poll_mouse();
        key = read_key();
        if (key == CR || key == 3 || on_button(BTN_OK_X + 56, DLG_ROW + AB_L + 5)) break;
    }
    dialog_close();
}
#pragma rodata-name (pop)
#pragma code-name (pop)
#pragma code-name (push, "OVERLAY10")
#pragma rodata-name (push, "OVERLAY10")
#endif
#ifdef REU
/* Billentyusapkak a menu jobb oldalan : fekete, lekerekitett sapka, 3x5 pontos fehér nagybetuk ; a szoveg betutipusatol fuggetlen. */
static const char kg_ch[] = "ACDEFHILMOPRSTU123456789=";
static const unsigned kg_bits[] = {
    0x2BED, 0x3923, 0x6B6E, 0x79A7, 0x79A4, 0x5BED, 0x7497, 0x4927, 0x5FED, 0x2B6A, 0x6BA4, 0x6BAD, 0x388E, 0x7492, 0x5B6F,
    0x2C97, 0x62A7, 0x628E, 0x5BC9, 0x798E, 0x39AA, 0x7292, 0x2AAA, 0x2ACE, 0xE38
};
#define capbuf ((unsigned char *)outbuf + 300)           /* 7 sor, soronkent 5 bajt */
static void setpx(unsigned char r, unsigned char p, unsigned char v)
{
    unsigned char *q = capbuf + r * 5 + (p >> 3), m = (unsigned char)(0x80 >> (p & 7));
    if (v) *q |= m; else *q &= (unsigned char)~m;
}
static void capblit(unsigned x, unsigned char row)
{
    unsigned char r, k;
    for (r = 0; r < 7; ++r)
        for (k = 0; k < 5; ++k)
            if (capbuf[r * 5 + k]) bm_or(ROWBASE(row) + (x & 0xFFF8u) + k * 8 + r, capbuf[r * 5 + k]);
}
static unsigned char kwid(const char *s)
{
    unsigned char w = 0, in = 0, c;
    while ((c = (unsigned char)*s++) != 0) {
        if (c == '+') { w += 6; in = 0; continue; }
        if (!in) { in = 1; w += 3; }
        w += c == '<' ? 6 : 4;
    }
    return w;
}
static void kdraw(unsigned x, unsigned char row, const char *s)
{
    unsigned char c, r, w, o, gx, gw, i;
    unsigned g;
    const char *t;
    while (*s) {
        memset(capbuf, 0, 35);
        o = (unsigned char)(x & 7);
        if (*s == '+') {
            for (i = 1; i < 4; ++i) setpx(3, o + i, 1);
            setpx(2, o + 2, 1);
            setpx(4, o + 2, 1);
            capblit(x, row);
            x += 6;
            ++s;
            continue;
        }
        w = 3;
        for (t = s; *t && *t != '+'; ++t) w += *t == '<' ? 6 : 4;
        for (r = 0; r < 7; ++r)
            for (c = 0; c < w; ++c)
                if (!((r == 0 || r == 6) && (c == 0 || c == w - 1))) setpx(r, o + c, 1);
        gx = o + 2;
        for (; s < t; ++s) {
            if (*s == '<') {
                for (r = 0; r < 5; ++r) for (c = 0; c < 5; ++c) if (((r == 2 ? 0x1F : r == 0 || r == 4 ? 0x04 : 0x08) >> (4 - c)) & 1) setpx(1 + r, gx + c, 0);
                gw = 5;
            } else {
                g = kg_bits[strchr(kg_ch, *s) - kg_ch];
                for (r = 0; r < 5; ++r) for (c = 0; c < 3; ++c) if ((g >> (14 - r * 3 - c)) & 1) setpx(1 + r, gx + c, 0);
                gw = 3;
            }
            gx += gw + 1;
        }
        capblit(x, row);
        x += w;
    }
}
#endif
static unsigned char run_menu_(unsigned char m)
{
    unsigned char n = 0, i, hover = 255, row, choice = 255, kb = menu_kb;
    unsigned x0 = menu_x[m], width = 0, t;
    menu_kb = 0;
    while (menu_items[m][n]) {
#ifdef REU
        t = text_width(menu_items[m][n], 0) + kwid(menu_keys[m][n]) + 16;
#else
        t = text_width(menu_items[m][n], 0) + text_width(menu_keys[m][n], 0) + 16;
#endif
        if (t > width) width = t;
        ++n;
    }
    width = (width + 31) & 0xFFF8u;
    if (x0 + width + 8 > 320) x0 = 312 - width;
    xor_span(0, menu_x[m], menu_x[m + 1]);
    panel(1, n + 2, x0, width);                 /* une rangee vide en haut et en bas */
    for (i = 0; i < n; ++i) {
        row = 2 + i;
        put_text(x0 + 12, row, menu_items[m][i], 0);
#ifdef REU
        kdraw(x0 + width - 8 - kwid(menu_keys[m][i]), row, menu_keys[m][i]);
#else
        put_text(x0 + width - 8 - text_width(menu_keys[m][i], 0), row, menu_keys[m][i], 0);
#endif
        if (m == 2 && ((i && (style & (1 << (i - 1)))) || (!i && !style)) && !has_selection()) {
            put_text(x0 + 4, row, "*", 0);
        }
#ifdef REU
        if (m == 2 && i == 5 && small_font) put_text(x0 + 4, row, "*", 0);
#endif
#ifdef HUN
        if (m == 4 && ((i == 2 && sound_on) || (i == 3 && overwrite))) put_text(x0 + 4, row, "*", 0);
#elif defined(REU)
        if (m == 4 && i == 1 && sound_on) put_text(x0 + 4, row, "*", 0);
#endif
    }
    if (kb) { hover = 0; xor_span(2, x0 + 2, x0 + width - 2); }
    for (;;) {
        poll_mouse();
        key = read_key();
        if (key == 3) break;
        row = mouse_y / 8;
        i = hover;
        if (!kb) i = (row >= 2 && row <= n + 1 && mouse_x >= x0 && mouse_x < x0 + width) ? row - 2 : 255;
        if (key == 0x11) i = hover + 1 >= n ? 0 : hover + 1;                /* 255 + 1 = 0 */
        if (key == 0x91) i = hover == 255 || !hover ? n - 1 : hover - 1;
        if (key == 0x9D || key == 0x1D) {
            t = key == 0x1D ? m + 1 : m + NMENUS - 1;
            menu_next = (t >= NMENUS ? t - NMENUS : t) + 1;
            break;
        }
        if ((key == ' ' || key == CR) && hover != 255) { choice = hover; break; }
        if (i != hover) {
            if (hover != 255) xor_span(hover + 2, x0 + 2, x0 + width - 2);
            if (i != 255) xor_span(i + 2, x0 + 2, x0 + width - 2);
            hover = i;
        }
        if (clicked) { choice = i; break; }
    }
    /* ne redessine que ce que le menu cachait */
    shown_dirty = 255;
    page_colors();
    draw_menubar();
    draw_status();
    draw_lines(top, top + n + 2);
#ifdef REU
    draw_sbar();                                /* le menu a pu couvrir l'ascenseur */
#endif
    place_cursor();
    return choice;
}
#ifdef REU
#pragma rodata-name (pop)
#pragma code-name (pop)
static unsigned char run_menu(unsigned char m) { ovl_load(10); return run_menu_(m); }
static void about(void) { ovl_load(6); about_(); }
static unsigned char ask_text(const char *title, const char *label, char *buf, unsigned char max, unsigned char petscii)
{
    ovl_load(9);
    return ask_text_(title, label, buf, max, petscii);
}
#else
#define run_menu run_menu_
#endif

#ifdef REU
#pragma code-name (push, "OVERLAY3")
#pragma rodata-name (push, "OVERLAY3")
static void set_small_(unsigned char on)
{
    small_font = on;
    ix_reset();                                 /* a sortores megvaltozott : az index ujraepul */
    font_load(on);                              /* a betutabla a cartridge 59. / 60. bankjabol */
    anchor = NOSEL;
    layout_all();
    if (win_base) { lines_before = win_base / bpl_est; lines_exact = 0; }
    top = line_of(cursor);                      /* a kurzor sora legyen a kepernyo tetejen */
    if (top >= 3) top -= 3; else top = 0;
    draw_status();
    redraw_all();
    cursor_moved();
}
#pragma rodata-name (pop)
#pragma code-name (pop)
static void set_small(unsigned char on) { ovl_load(3); set_small_(on); }
#endif

static void do_menu(unsigned char m, unsigned char item)
{
    if (item == 255) return;
    switch (m) {
    case 0:
#ifdef REU
        if (item == 0 && leave_ok(S_NEW)) { new_doc(); redraw_all(); }
#else
        if (item == 0 && (!dirty || message(S_NEW, S_LOSE, 0, 1))) { new_doc(); redraw_all(); }
#endif
        if (item == 1) open_doc();
#ifdef RTF
        if (item == 2) reload_doc(0);           /* le fichier du disque / de la SD, sans le .bck */
        if (item == 3) reload_doc(1);           /* le fichier et son .bck */
        if (item == 4) f1_save();               /* sauvegarde rapide */
        if (item == 5) save_doc();              /* enregistrement complet */
        if (item == 6) save_as();
#else
        if (item == 2) save_doc();
        if (item == 3 && ask_filename()) save_doc();
#endif
        break;
    case 1:
        if (item == 0) cut_selection();
        if (item == 1) copy_selection();
        if (item == 2) paste();
#ifdef REU
        if (item == 3) select_all();
#else
        if (item == 3) set_selection(0, doclen);
#endif
        if (item == 4) find_dialog();
        if (item == 5) find_next();
#ifdef REU
        if (item == 6) { undo(); cursor_moved(); }
        if (item == 7) { redo(); cursor_moved(); }
        if (item == 8) handle_key(0x94);        /* torles elore */
        if (item == 9) handle_key(0x13);        /* Home : sor eleje / vege */
        if (item == 10) handle_key(0x09);       /* Tab : negy szokoz */
#endif
        break;
    case 2:
#ifdef REU
        if (item == 4) { cycle_theme(); break; }
        if (item == 5) { set_small(!small_font); break; }
#endif
        apply_style(item ? (unsigned char)(1 << (item - 1)) : 0);
        break;
    case 3:
        print_run(item);
        break;
    case 4:
#ifdef HUN
        if (item == 1) { hun_toggle(); break; }
        if (item == 2) { sound_on ^= 1; key_click(); break; }
        if (item == 3) { overwrite ^= 1; draw_status(); break; }
#elif defined(REU)
        if (item == 1) { sound_on ^= 1; key_click(); break; }
#endif
#ifdef REU
        about();
#else
        message(S_ABOUT, S_ABOUT1, S_ABOUT3, 0);
#endif
        break;
    }
}

/* ------------------------------------------------------------------ */
/* Clavier et souris dans le texte                                     */

static unsigned pos_at(unsigned i, unsigned x)
{
    unsigned p = lstart[i], end = lstart[i + 1], px = 0;
    unsigned char w, st = lstyle[i], c;
    if (i + 1 >= nlines) end = doclen;
    while (p < end) {
        c = text[p];
        if (IS_MARK(c)) { st = c & 7; ++p; continue; }
        if (c == CR) break;
        w = CW(c, st);
        if (px + w / 2 >= x) break;
        px += w;
        ++p;
    }
    if (p == end && end < doclen && p > lstart[i] && text[p - 1] == ' ') --p;
    return canonical(p);
}

#ifdef REU
#pragma code-name (push, "OVERLAY8")
#pragma rodata-name (push, "OVERLAY8")
#endif
static unsigned pos_at_mouse_(void)
{
    unsigned i = top + (mouse_y / 8 >= TEXT_ROW0 ? mouse_y / 8 - TEXT_ROW0 : 0);
#ifdef REU
    if (mouse_y / 8 < TEXT_ROW0 && held && top) i = top - 1;     /* au-dessus du texte : defile vers le haut */
#endif
    if (i >= nlines) i = nlines - 1;
    return pos_at(i, mouse_x >= TEXT_X0 ? mouse_x - TEXT_X0 : 0);
}
#ifdef REU
#pragma rodata-name (pop)
#pragma code-name (pop)
static unsigned pos_at_mouse(void) { ovl_load(8); return pos_at_mouse_(); }
#else
#define pos_at_mouse pos_at_mouse_
#endif

static void move_vertical(int d)
{
    unsigned i = line_of(cursor), x = cursor_x();
    if (d < 0 && i == 0) return;
    if (d > 0 && i + 1 >= nlines) return;
    cursor = pos_at(i + d, x);
}

static void move_right(void)
{
    unsigned p = cursor;
    while (p < doclen && IS_MARK(text[p])) ++p;
    if (p < doclen) ++p;
    cursor = p;
}

static void move_left(void)
{
    if (cursor) cursor = canonical(cursor - 1);
}

/* Accents sous C=. Edition hongroise : les positions de AltGr dans hu_prog
   (q=ő w=ű e=é y=ü u=ú i=í o=ó p=ö a=á), sinon le clavier français. */
#ifdef KBD_HU
/* Le KERNAL ne sait pas faire Shift + C= : avec les deux touches, sa table
   ne rend rien du tout. Plume lit donc la matrice lui-meme tant que C= est
   enfoncee. Deux octets par touche : ce qu'il faut ecrire dans $DC00 pour ne garder
   que sa rangee, puis son masque de colonne. L'ordre fixe le numero rendu par
   hu_key, d'ou la table des lettres juste apres. */
static const unsigned char hu_keys[] = {
    0xFD, 0x40,     /* e */
    0xFD, 0x02,     /* w */
    0xEF, 0x02,     /* i */
    0xEF, 0x40,     /* o */
    0xDF, 0x02,     /* p */
    0x7F, 0x40,     /* q */
    0xF7, 0x40,     /* u */
    0xF7, 0x02,     /* y */
    0xFD, 0x04,     /* a */
    0xDF, 0x08      /* - (donne _) */
};
#define HU_KEYS (sizeof hu_keys / 2)

/* Numeros de hu_key (0xE0 + numero, puis + HU_KEYS si Shift), puis les lettres
   de la police. Aucun de ces codes ne tombe sur ceux que handle_key lit avant
   translate, ni sur les lettres de la police, dont 0x85-0x8C sont aussi les
   touches de fonction (0x8B = couper, 0x85 = enregistrer...). */
static const unsigned char hu_font[] = {
    132, 178, 170, 172, 139, 176, 174, 142, 168, '_',   /* e u i o o o u u a _ */
    146, 179, 171, 173, 180, 177, 175, 155, 169, '_'    /* E U I O O O U U A _ */
};

static unsigned char hu_last = 0xFF;    /* offset deja vu ; 0xFF = C= relachee */
static unsigned char hu_t, hu_rep;      /* a kurzorbillentyuk ismetlese */

/* Touche de la couche C= enfoncee, sous forme 0xE0 + son numero (+ HU_KEYS si Shift),
   ou 0 si C= est relachee ou si la touche a deja ete comptee. $028D dirait
   l'etat de C=, mais le KERNAL le remet a zero pendant sa propre scrutation :
   on lit la matrice (rangee 7, bit 5). */
static unsigned char hu_key(void)
{
    unsigned char i, c, d, shift;
    asm("sei");
    POKE(0xDC00, 0x7F);                 /* rangee 7 : C=, et q */
    if (PEEK(0xDC01) & 0x20) {          /* C= relachee */
        asm("cli");
        hu_last = 0xFF;
        return 0;
    }
#ifdef HUN
    if (!hun_on && !(PEEK(0xDC01) & 0x10)) {       /* C= + espace : change de clavier */
        POKE(0xDC00, 0x7F);
        asm("cli");
        return 0xFA;
    }
#endif
    POKE(0xDC00, 0xFD);                 /* rangee 1 : Shift gauche, bit 7 */
    c = ~PEEK(0xDC01);
    POKE(0xDC00, 0xBF);                 /* rangee 6 : Shift droit, bit 4 */
    d = ~PEEK(0xDC01);
    shift = ((c & 0x80) | (d & 0x10)) ? HU_KEYS : 0;
#ifdef HUN
    for (i = 0; i < (hun_on ? 0 : 2 * HU_KEYS); i += 2) {       /* magyar kiosztasban a C= betuk nem a hu_key-e */
#else
    for (i = 0; i < 2 * HU_KEYS; i += 2) {
#endif
        POKE(0xDC00, hu_keys[i]);
        if (!(~PEEK(0xDC01) & hu_keys[i + 1])) continue;
        POKE(0xDC00, 0x7F);
        asm("cli");
        if (i == hu_last) return 0;
        hu_last = i;
        return 0xE0 + (i >> 1) + shift;
    }
    POKE(0xDC00, 0xFE);                 /* 0. sor : a kurzorbillentyuk (a C= + kurzor kijelol) */
    c = ~PEEK(0xDC01);
    POKE(0xDC00, 0x7F);
    asm("cli");
    if (c & 0x84) {
        i = (c & 0x80) ? 0xF6 : 0xF4;               /* le, jobbra ; Shiftel fel, balra */
        if (shift) ++i;
        if (i == hu_last) {
            if ((unsigned char)(PEEK(0xA2) - hu_t) < hu_rep) return 0;
            hu_rep = 5;
        } else {
            hu_last = i;
            hu_rep = 25;
        }
        hu_t = PEEK(0xA2);
        return i;
    }
    hu_last = hun_on ? 0xFF : 0xFE;     /* C= tenue, aucune touche de la couche (magyar kiosztasban a KERNAL billentyuit nem dobjuk el) */
    return 0;
}
#else
static const unsigned char cbm_keys[]   = { 0xB1, 0xB2, 0xA3, 0xB0, 0xAB, 0xBC, 0xB8, 0xB7, 0xB9, 0xA2, 0xA1, 0xB6, 0xAF };
static const unsigned char cbm_accent[] = { 132,  133,  134,  128,  129,  131,  140,  141,  138,  136,  137,  135,  156 };
#endif
static const unsigned char ctrl_pun[8] = { 0x90, 0x05, 0x1C, 0x9F, 0x9C, 0x1E, 0x1F, 0x9E };   /* CTRL + 1 ... 8 : a KERNAL szinkodjai */
static unsigned char translate(unsigned char k)
{
    unsigned char j;
#ifndef KBD_HU
    unsigned char i;
#endif
    for (j = 0; j < 8; ++j) if (k == ctrl_pun[j]) return 182 + j;           /* „ ” “ ‘ ’ – — • */
#ifdef KBD_HU
    /* Lettres rendues par hu_key (voir hu_font). */
    if (k >= 0xE0 && k < 0xE0 + 2 * HU_KEYS) return hu_font[k - 0xE0];
#endif
#ifdef KBD_DK
    /* Clavier danois : : ; @ donnent æ ø å, Shift les majuscules ;
       Shift + . et Shift + , donnent : et ; ; C= + @ donne @. */
    switch (k) {
    case 0x3A: return 162;
    case 0x3B: return 163;
    case 0x40: return 164;
    case 0x5B: return 165;
    case 0x5D: return 166;
    case 0xBA: return 167;
    case 0x3E: return ':';
    case 0x3C: return ';';
    case 0xA4: return '@';
    }
#endif
    if (k >= 0x41 && k <= 0x5A) return k + 0x20;
    if (k >= 0xC1 && k <= 0xDA) return k - 0x80;
    if (k >= 0x20 && k <= 0x40) return k;
    if (k == 0x5B || k == 0x5D) return k;
    if (k == 0x5C) return 161;
    if (k == 0xDC) return '_';              /* C= + - : tiret bas */
    if (k == 0x5E) return '^';
#ifdef HUN
    if (k == 0x5F) return 181;              /* C= + <- : fleche gauche du C64 */
#endif
    if (k == 0xA0) return ' ';
#ifndef KBD_HU
    for (i = 0; i < sizeof cbm_keys; ++i) if (k == cbm_keys[i]) return cbm_accent[i];
#endif
    return 0;
}

/* Apres un deplacement, le style de frappe suit le texte. */
static void cursor_moved(void)
{
    unsigned p = cursor;
    while (p < doclen && IS_MARK(text[p])) ++p;   /* a jelzo a kurzor alatti betu stilusat mutatja */
    style = style_before(p);
    place_cursor();
    draw_status();
}

/* Billentyuklik : un clic de bruit (voix 1). L'enveloppe (attaque 0, decroissance
   courte, tenue 0) s'eteint toute seule. */
#define CLICK_NOISE_FREQ  0x58          /* octet haut de la frequence du bruit */
#define CLICK_NOISE_DECAY 0x00          /* 0 = 6 ms, 1 = 24 ms, 2 = 48 ms */
#define CLICK_VOLUME      15

static void key_click(void)
{
    if (!sound_on) return;
    POKE(0xD418, CLICK_VOLUME);
    POKE(0xD401, CLICK_NOISE_FREQ);
    POKE(0xD405, CLICK_NOISE_DECAY);
    POKE(0xD406, 0);
    POKE(0xD404, 0x80);                 /* porte fermee puis ouverte : rearme l'enveloppe */
    POKE(0xD404, 0x81);                 /* bruit */
}

#ifdef REU
/* CLR/HOME : debut de la ligne ; deja au debut : fin de la ligne. */
#ifdef REU
#pragma code-name (push, "OVERLAY8")
#pragma rodata-name (push, "OVERLAY8")
#endif
static void home_end_(void)
{
    unsigned i = line_of(cursor), e;
    if (cursor != lstart[i]) { cursor = lstart[i]; return; }
    e = lstart[i + 1];
    if (i + 1 < nlines && e > lstart[i]) --e;
    cursor = canonical(e);
}
#ifdef REU
#pragma rodata-name (pop)
#pragma code-name (pop)
static void home_end(void) { ovl_load(8); home_end_(); }
#else
#define home_end home_end_
#endif

/* Suppression vers l'avant (Shift + CLR/HOME). */
static void delete_after(void)
{
    unsigned p = cursor;
    if (has_selection()) { delete_selection(); return; }
    anchor = NOSEL;
    while (p < doclen && IS_MARK(text[p])) ++p;
    if (p < doclen) delete_range(cursor, p + 1);
}
#endif
#ifdef HUN
/* C= et espace tenues ensemble ? */
static unsigned char cs_down(void)
{
    unsigned char r;
    asm("sei");
    POKE(0xDC00, 0x7F);
    r = PEEK(0xDC01);
    POKE(0xDC00, 0x7F);
    asm("cli");
    return !(r & 0x10) || !(r & 0x20);
}

static void hun_toggle(void)
{
    hun_on ^= 1;
    hun_set(hun_on);
    draw_status();
    while (cs_down()) poll_mouse();             /* attend le relachement : pas de double bascule */
    POKE(198, 0);                               /* ni de repetition de l'espace */
    joy_hold = 3;
}
#endif

/* C= + kurzor : kijeloles a kurzor mozgatasaval (a 9. overlayben). */
#ifdef REU
#pragma code-name (push, "OVERLAY9")
#pragma rodata-name (push, "OVERLAY9")
#endif
static void selmove_(unsigned char k)
{
    unsigned a = anchor == NOSEL ? cursor : anchor, oc = cursor;
    if (k == 0xF4 || k == 0x1D) move_right();
    else if (k == 0xF5 || k == 0x9D) move_left();
    else move_vertical(k == 0xF6 || k == 0x11 ? 1 : -1);
    /* a regi kijeloles hatarai a regi kurzorral szamolodnak (az ujrarajzolashoz) */
    {
        unsigned nc = cursor;
        cursor = oc;
        set_selection(a, nc);
    }
}
#ifdef REU
#pragma rodata-name (pop)
#pragma code-name (pop)
static void selmove(unsigned char k) { ovl_load(9); selmove_(k); }
#else
#define selmove selmove_
#endif

static void handle_key(unsigned char k)
{
    unsigned char c;
#ifdef KBD_HU
    if (k >= 0xF4 && k <= 0xF7) { selmove(k); cursor_moved(); return; }
#else
    if ((PEEK(0x028D) & 2) && (k == 0x1D || k == 0x9D || k == 0x11 || k == 0x91)) { selmove(k); cursor_moved(); return; }       /* C= tartva */
#endif
    switch (k) {
    case CR:   key_click(); insert_char(CR); return;
    case 0x14: key_click(); delete_before(); return;
    case 0x1D: clear_selection(); move_right(); break;
    case 0x9D: clear_selection(); move_left(); break;
    case 0x11: clear_selection(); move_vertical(1); break;
    case 0x91: clear_selection(); move_vertical(-1); break;
#ifdef REU
    case 0x13: clear_selection(); home_end(); break;
    case 0x09: { unsigned char i; key_click(); for (i = 0; i < 4; ++i) insert_char(' '); return; }
    case 0x94: key_click(); delete_after(); return;     /* Shift + CLR/HOME ou Shift + INST/DEL : suppression vers l'avant (le KERNAL repete 0x94 comme 0x14) */
#else
    case 0x13: clear_selection(); cursor = lstart[line_of(cursor)]; break;
#endif
#ifdef HUN
    case 0xFA: hun_toggle(); return;
    case 0xFB: overwrite ^= 1; draw_status(); return;
#endif
    case 3:    kbd_menu(0); return;                            /* a kijeloles megmarad : a menubol vagas / masolas / beillesztes */
    case 0x85:
#ifdef REU
        f1_save();
#else
        save_doc();
#endif
        return;
    case 0x86: apply_style(BOLD); return;
    case 0x87: apply_style(ITALIC); return;
    case 0x88: apply_style(UNDERLINE); return;
    case 0x89: copy_selection(); return;
    case 0x8A: paste(); return;
    case 0x8B: cut_selection(); return;
    case 0x8C: find_next(); return;
    case 0x06: cycle_theme(); return;                  /* CTRL + fleche gauche (en HUN la touche du zero) : couleurs */
#ifdef REU
    case 0x1A: undo(); break;
    case 0x19: redo(); break;                           /* CTRL+Z, RESTORE : annuler */
#endif
    case 0x12: sound_on ^= 1; key_click(); return;      /* CTRL+9 : clic de touche oui / non */
#ifdef REUTEST
    case 0x8D: print_run(1); return;
    case 0xA0: load_named(); redraw_all(); return;        /* le nom est dans filename (outil de test) */
#ifdef RTF
    case 0x9E: rtf_open(); redraw_all(); return;          /* le nom est dans filename (outil de test) */
    case 0x9F: strcpy(rtfname, "\x4F\x55\x54"); rtf_write(rtfname); return;       /* OUT */
#endif
    case 0x8E: {                         /* impression de test vers une somme */
        pr_sink = 1; pr_sum = pr_cnt = 0; print_ok = 1;
        print_begin(); print_text(0); print_end();
        *(unsigned *)0x02B4 = 0xA55A;    /* fini */
        pr_sink = 0; return; }
    case 0x90: { unsigned char i; clear_selection(); for (i = 0; i < 100; ++i) move_vertical(1); break; }
    case 0x05: { unsigned char i; clear_selection(); for (i = 0; i < 100; ++i) move_vertical(-1); break; }
#endif
    default:
        c = translate(k);
        if (c) { key_click(); insert_char(c); }
        return;
    }
    cursor_moved();
}

#ifdef REU
/* Clic ou glissement dans l'ascenseur : la fenetre va a cet endroit du document. */
/* La fenetre va a la position y (0..SBAR_H-16) de l'ascenseur. */
#pragma code-name (push, "OVERLAY8")
#pragma rodata-name (push, "OVERLAY8")
static void sbar_end_(void)                     /* a csuszka legalja : a dokumentum vege */
{
    unsigned L, n;
        for (n = 0;;) {
            L = doclen;
            while (L && (text[L - 1] == CR || text[L - 1] == ' ')) --L;     /* az utolso szoveges karakter utan */
            if ((L && nlines >= VISIBLE + 8) || !win_base || ++n > 16) break;
            slide_back();
        }
        top = nlines > VISIBLE ? nlines - VISIBLE : 0;   /* az utolso teljes oldal, a kurzor a dokumentum vegen (az also sorban) */
        cursor = doclen;
        if (L && nlines > VISIBLE) {
            n = line_of(L - 1) + 1;             /* sorok az utolso szoveges sorig */
            if (nlines - n >= VISIBLE) {        /* tobb ures sor a vegen, mint a kepernyo : feljebb, az utolso szoveges sor az also 5. */
                top = n + 4 > VISIBLE ? n + 4 - VISIBLE : 0;
                cursor = L;
            }
        }
    draw_text_area();
    place_cursor();
    cursor_moved();
}

static void sbar_goto_(unsigned y)
{
    unsigned L;
    dpos R, g;
    if (SBAR_LINES) {
        R = nlines > VISIBLE ? nlines - VISIBLE : 0;
        L = (unsigned)(R * y / (SBAR_H - 16));
        clear_selection();
    } else {
        R = sbar_range();
        g = y == SBAR_H - 16 ? win_base + doclen + suf_len : R * y / (SBAR_H - 16);     /* legalul : a dokumentum vege (nem becsult) */
        goto_global(g, g);
        anchor = NOSEL;
        L = line_of((unsigned)(g - win_base));
    }
    cursor = lstart[L];
    top = L;
    maybe_slide();                              /* la fenetre peut glisser : d'abord elle, puis l'ecran */
    if (y == SBAR_H - 16 && !suf_len) { sbar_end_(); return; }   /* legalul : a nezetet az sbar_end_ allitja be es rajzolja ki */
    if (!suf_len && nlines > VISIBLE && top + VISIBLE > nlines) {      /* fin du document : derniere page pleine */
        top = nlines - VISIBLE;
        cursor = lstart[top];
    }
    draw_text_area();
    place_cursor();
    cursor_moved();
}

#pragma rodata-name (pop)
#pragma code-name (pop)
static void sbar_goto(unsigned y) { ovl_load(8); sbar_goto_(y); }

/* Clic ou glissement : le curseur suit la souris tout de suite, le texte
   suit quand la souris s'arrete un instant (6 images) et au relachement. */
/* Un oldal : le curseur et l'ecran avancent (ou reculent) de VISIBLE - 1 lignes, en deux temps pour
   que la fenetre ait le temps de glisser. */
#pragma code-name (push, "OVERLAY6")
#pragma rodata-name (push, "OVERLAY6")
static void page_step_(unsigned char down)
{
    unsigned char part, guard;
    unsigned i, ni, n, x;
    clear_selection();
    for (part = 0; part < 3; ++part) {          /* trois fois (VISIBLE - 1) / 3 lignes : la fenetre glisse entre-temps */
        n = (VISIBLE - 1) / 3;
        for (guard = 0; !down && guard < 12; ++guard) {   /* vers le haut : assez de lignes avant le curseur (un paragraphe par glissement) */
            i = line_of(cursor);
            if (i >= n + 6 || !win_base) break;
            slide_back();
        }
        i = line_of(cursor);
        x = cursor_x();
        if (down) {
            ni = i + n < nlines ? i + n : nlines - 1;
            top += ni - i;
        } else {
            ni = i > n ? i - n : 0;
            n = i - ni;
            top = top > n ? top - n : 0;
        }
        cursor = pos_at(ni, x);
        maybe_slide();
    }
    if (!suf_len && top + VISIBLE > nlines) top = nlines > VISIBLE ? nlines - VISIBLE : 0;
    draw_text_area();
    cursor_moved();
}
#pragma rodata-name (pop)
#pragma code-name (pop)
static void page_step(unsigned char down) { ovl_load(6); page_step_(down); }

/* A gorgetosav csuszkaja lenyomva fekete (a kek temaban sotetkek, mert ott alapbol fekete). */
#ifdef REU
#pragma code-name (push, "OVERLAY8")
#pragma rodata-name (push, "OVERLAY8")
#endif
static void sb_ink_(unsigned char pressed)
{
    unsigned char r, d = dark_tone[theme_no];
    if (pressed) d = d ? 0 : COLOR_BLUE;
    for (r = TEXT_ROW0; r < FOOT_ROW; ++r) color_cells(r, 39, 1, (unsigned char)((d << 4) | (COL_BAR & 15)));
}
#ifdef REU
#pragma rodata-name (pop)
#pragma code-name (pop)
static void sb_ink(unsigned char pressed) { ovl_load(8); sb_ink_(pressed); }
#else
#define sb_ink sb_ink_
#endif

static void sbar_drag(void)
{
    unsigned yy = mouse_y > TEXT_ROW0 * 8 ? mouse_y - TEXT_ROW0 * 8 : 0, ty = sbar_ty(), y, shown = 0xFFFF;
    unsigned char off = (unsigned char)(yy - ty), since = PEEK(0xA2), todo = 0, first = 1, down;
    sb_ink(1);
    if (yy < ty || yy >= ty + 16) {             /* clic au-dessus / au-dessous du curseur : page par page */
        down = yy >= ty;
        do {
            page_step(down);
            since = PEEK(0xA2);
            do poll_mouse(); while (held && (unsigned char)(PEEK(0xA2) - since) < (first ? 25 : 7));
            first = 0;
            yy = mouse_y > TEXT_ROW0 * 8 ? mouse_y - TEXT_ROW0 * 8 : 0;
            ty = sbar_ty();
            if (down ? ty + 16 > yy : yy >= ty) break;     /* le curseur a rejoint la souris */
        } while (held);
        sb_ink(0);
        return;
    }
    do {
        yy = mouse_y > TEXT_ROW0 * 8 ? mouse_y - TEXT_ROW0 * 8 : 0;
        y = yy > off ? yy - off : 0;
        if (y > SBAR_H - 16) y = SBAR_H - 16;
        if (y != shown) { shown = y; since = PEEK(0xA2); todo = 1; draw_thumb(y); }
        else if (todo && (unsigned char)(PEEK(0xA2) - since) >= 6) { todo = 0; sbar_goto(y); }
        poll_mouse();
    } while (held);
    sb_ink(0);
    if (todo) sbar_goto(y);
}
#endif

#ifdef REU
/* Double clic : le mot sous le pointeur ; triple clic : la phrase (jusqu'a . ! ? ou fin de paragraphe). */
#pragma code-name (push, "OVERLAY3")
#pragma rodata-name (push, "OVERLAY3")
static unsigned char wordch(unsigned char c)
{
    return (c >= '0' && c <= '9') || ((c | 32) >= 'a' && (c | 32) <= 'z') || (c >= 128 && c < 182);
}

static unsigned char endch(unsigned char c)
{
    return c == '.' || c == '!' || c == '?';
}

static void select_unit_(unsigned p, unsigned char n)
{
    unsigned a = p, b = p;
    unsigned char c;
    if (n == 2) {
        while (a && wordch(text[a - 1])) --a;
        while (b < doclen && wordch(text[b])) ++b;
    } else {
        while (a) {
            c = text[a - 1];
            if (c == CR || (c == ' ' && a > 1 && endch(text[a - 2]))) break;
            --a;
        }
        while (b < doclen && (c = text[b]) != CR) { ++b; if (endch(c)) break; }
    }
    if (a == b) return;
    set_selection(a, b);
    cursor_moved();
}
#pragma rodata-name (pop)
#pragma code-name (pop)
static void select_unit(unsigned p, unsigned char n) { ovl_load(3); select_unit_(p, n); }
#endif

static void kbd_menu(unsigned char m)
{
    menu_kb = 1;
    mouse_x = menu_x[m];
    mouse_y = 0;
    handle_click();
}

static void handle_click(void)
{
    unsigned char row = mouse_y / 8, m, k;
    unsigned p;
    if (row == 0) {
        for (m = 0; m < NMENUS; ++m) {
            if (mouse_x >= menu_x[m] && mouse_x < menu_x[m + 1]) {
                do_menu(m, run_menu(m));
                if (menu_next) { m = menu_next - 1; menu_next = 0; kbd_menu(m); }
                return;
            }
        }
        return;
    }
    if (row == 1) {
        for (k = 0; k < 3; ++k) {
            if (mouse_x >= STYLE_X + k * 18 && mouse_x < STYLE_X + k * 18 + 16) apply_style((unsigned char)(1 << k));
        }
        return;
    }
#ifdef REU
    if (reu_on && mouse_x >= SBAR_X && row < FOOT_ROW) { sbar_drag(); return; }
#endif
    p = pos_at_mouse();
    if (extend) {
        set_selection(anchor == NOSEL ? cursor : anchor, p);
        return;
    }
#ifdef REU
    if (click_hold && p == click_p && click_n < 3) ++click_n; else click_n = 1;
    click_p = p;
    if (click_n > 1) {
        select_unit(p, click_n);
        while (held) poll_mouse();
        click_hold = 20;
        return;
    }
#endif
    if (anchor != NOSEL) set_selection(NOSEL, p);
    else cursor = p;
    cursor_moved();
    anchor = p;
#ifdef REU
    agl = win_base + p;
#endif
    while (held) {
        poll_mouse();
        p = pos_at_mouse();
        if (p != cursor) set_selection(anchor, p);
#ifdef REU
        maybe_slide();
#endif
    }
    if (anchor == cursor) anchor = NOSEL;
#ifdef REU
    click_hold = 20;
#endif
}

/* ------------------------------------------------------------------ */
/* Sprites : fleche (sprite 0) et curseur texte (sprites 1 et 2)       */

static const unsigned char arrow[] = {
    0x80, 0xC0, 0xE0, 0xF0, 0xF8, 0xFC, 0xFE, 0xF8, 0xD8, 0x8C, 0x0C, 0x06, 0x06
};

#ifdef REU
static const unsigned char hourglass[] = {
    0x80, 0xFF, 0xFF, 0x7E, 0x3C, 0x18, 0x18, 0x3C, 0x7E, 0xFF, 0xFF, 0, 0
};

static void disk_busy(unsigned char on)
{
    unsigned char i, *spr = (unsigned char *)0xC000;
    if (sd_on || sd_pk)                         /* az UCI-hoz nem kell elrejteni a spritot : csak a nyil valt */
        for (i = 0; i < sizeof arrow; ++i) spr[i * 3] = on ? hourglass[i] : arrow[i];
    else POKE(0xD015, on ? 0 : 1);
}

#endif

#ifdef REU
#pragma code-name (push, "OVERLAY8")
#pragma rodata-name (push, "OVERLAY8")
#endif
static void sprites_init_(void)
{
    unsigned char i, *spr = (unsigned char *)0xC000;
    memset(spr, 0, 128);
    for (i = 0; i < sizeof arrow; ++i) spr[i * 3] = arrow[i];
    memset(CUR_BLOCK, 0, 64);
    *(unsigned char *)0xCFF8 = 0;
    *(unsigned char *)0xCFF9 = 1;
    *(unsigned char *)0xCFFA = 47;              /* $CBC0, libre apres lstyle */
    VIC.spr_ena = 0x01;
}
#ifdef REU
#pragma rodata-name (pop)
#pragma code-name (pop)
static void sprites_init(void) { ovl_load(8); sprites_init_(); }
#else
#define sprites_init sprites_init_
#endif

void main(void)
{
    POKE(0x0291, 0x80);
    findtext[0] = 0;
#ifdef KBD_HU
    hu_last = 0xFF;                     /* aucune touche de la couche vue */
#endif                            /* tampon cassette : pas remis a zero */
    gfx_init();
#ifdef REU
    fsmall = 1;                                 /* alapertelmezett a kis betukeszlet */
    font_load(1);
#endif
    sprites_init();
    set_theme();
    mouse_install(&mouse_def_callbacks, mouse_static_stddrv);
    mouse_show();
#ifdef REU
    reu_on = reu_detect();
    if (reu_on) {
        reu_probe_size();
        if (suf_end < 0x20000UL) reu_on = 0;        /* il faut au moins 256 Ko */
    }
#endif
    new_doc();
#ifdef REUTEST
    {   /* TESTPARAS paragraphes de 45 octets, "pNNNNNN wNN wNN ... " : verifiable sur l'hote */
        dpos dst = suf_end - 45UL * TESTPARAS;
        unsigned long j;
        unsigned k, w;
        unsigned char *b = outbuf;
        for (j = 1; j <= TESTPARAS; ++j) {
            b[0] = 'p';
            w = (unsigned)(j / 1000);
            b[1] = (unsigned char)('0' + w / 100);
            b[2] = (unsigned char)('0' + (w / 10) % 10);
            b[3] = (unsigned char)('0' + w % 10);
            w = (unsigned)(j % 1000);
            b[4] = (unsigned char)('0' + w / 100);
            b[5] = (unsigned char)('0' + (w / 10) % 10);
            b[6] = (unsigned char)('0' + w % 10);
            b[7] = ' ';
            for (k = 0; k < 9; ++k) {
                w = (unsigned)((j * 7 + k * 3) % 100);
                b[8 + k * 4] = 'w';
                b[9 + k * 4] = (unsigned char)('0' + w / 10);
                b[10 + k * 4] = (unsigned char)('0' + w % 10);
                b[11 + k * 4] = ' ';
            }
            b[44] = CR;
            reu_move(0, b, dst, 45);
            dst += 45;
        }
        suf_len = 45UL * TESTPARAS;
        pull_tail();
    }
#endif
#ifdef REUTEST
    dbg_state();
#endif
    redraw_all();
    nmi_off();                                          /* RESTORE ne doit plus relancer le BASIC */
#ifdef REU
#ifdef HUN
    hun_on = 1;
    hun_set(1);                                         /* clavier HUN au depart */
    draw_status();
#endif
    *(unsigned char *)0x03BC = 0;                       /* led.s : 1 = l'Ultimate n'a pas repondu, plus d'essais */
    led_theme(4);                                       /* molette : sensibilite 1 */
    led_theme(theme_no);                                /* leds : couleur du theme de depart */
    led_theme(6);                                       /* clavier eteint : rien a enregistrer */
    led_theme(7);                                       /* module SD (src/sd.s) en $CC00 */
    sd_ok = sd_probe() && !sd_getpath();                /* Ultimate present ET sa cible DOS repond (ancien firmware : pas de SD) */
    SD_PATH[0] = 0;                                     /* le repertoire de depart se choisit au premier usage */
    lp(0);
#endif
    /* repere pour les outils de test : adresses du texte et de sa longueur */
    *(unsigned *)0x02AB = (unsigned)text;
    *(unsigned *)0x02AD = (unsigned)&doclen;
    for (;;) {
        poll_mouse();
#ifdef REU
        if (nmi_hit) { nmi_hit = 0; restore_key(); }
#endif
        if (wheel) {
            for (; wheel_n; --wheel_n) handle_key(wheel == 4 ? 0x91 : 0x11);
            wheel = 0;
            POKE(198, 0);                       /* les impulsions de la molette se lisent comme des touches : */
            joy_hold = 6;                       /* purge du tampon, puis quelques images sans touches */
#ifdef REU
            maybe_slide();
#endif
        }
        if (clicked) {
            KB_TOUCH();
            handle_click();
#ifdef REU
            maybe_slide();
#endif
        }
        key = read_key();
        if (key) {
            KB_TOUCH();                         /* 2 s csend utan igazodik a billentyuzet-LED */
            handle_key(key);
#ifdef REU
            maybe_slide();
#endif
        }
#ifdef REU
        /* Billentyuzet-LED : vilagit, ha van mentetlen valtozas ; mentes utan azonnal kialszik,
           bekapcsolni csak szunetben kapcsol (egy hivas kb. 1 s). */
        if ((dirty != 0) != led_kb && (!dirty || !kb_idle)) {
            led_kb = dirty != 0;
            led_theme(led_kb ? 5 : 6);
        }
#endif
#ifdef REU
        if (ix_go && kb_idle < 85) ix_tick();   /* szunetben: hatter-indexeles */
#endif
        blink_cursor();
    }
}
