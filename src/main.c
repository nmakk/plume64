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
extern const unsigned char font_width[];
extern unsigned cur_addr, cur_n;
extern unsigned char cur_sh, cur_st;
extern unsigned char *cur_ptr;
void cur_sample(void);
extern unsigned char cur_row;
extern unsigned cur_px;
void cur_place(void);
void cursor_off(void);
void blink_cursor(void);
extern unsigned char col[5], theme_no;
void set_theme(void);
void next_theme(void);
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
#define MAXLINES  950
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
/* Noms de la boite Ouvrir : sous les E/S, apres le presse-papiers. */
#define names     ((char (*)[17])(0xD000 + 3 * (MAXLINES + 2) + CLIPMAX))
/* $0334-$03FB, tampon cassette inutilise : petites chaines. */
#define diskmsg   ((char *)0x0334)              /* 41 octets */
#define cmdname   ((char *)0x035D)              /* 24 */
#define findtext  ((unsigned char *)0x0375)     /* 24 */
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
/* Jeux de couleurs, CTRL+0 passe au suivant : page, barres, marges, bord,
   fleche. Le curseur prend l'encre (bloc) et le fond (lettre) de la page. */
#define NTHEMES 4
const unsigned char themes[NTHEMES][5] = {
    { 0x01, 0x0F, 0x0C, COLOR_GRAY1, COLOR_BLACK },     /* noir sur blanc */
    { 0xD0, 0x0D, 0x0B, COLOR_BLACK, COLOR_WHITE },     /* vert sur noir */
    { 0xE6, 0x6E, 0x06, COLOR_LIGHTBLUE, COLOR_WHITE }, /* bleu Commodore */
    { 0x70, 0x07, 0x0B, COLOR_BLACK, COLOR_WHITE },     /* jaune sur noir */
};

unsigned char text[MAXDOC];               /* exporte pour next_line_asm */
/* Tampon d'impression : une ligne du mode Texte, ou la sortie d'une passe du
   mode Graphique. Libre autrement. */
static unsigned char outbuf[PRINT_CARDS_ * 8 + 3];
unsigned doclen;
static unsigned nlines;
static unsigned relayout_last;
static unsigned relayout_from;          /* premiere ligne dont le contenu change */
static unsigned top;
unsigned cursor;                          /* exportes pour gfx.s */
unsigned anchor = NOSEL;
static unsigned char style;
static unsigned char dirty;
static unsigned cliplen;
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
typedef unsigned long dpos;
extern unsigned reu_c64, reu_len;
extern unsigned char reu_addr[3];
extern void reu_stash(void), reu_fetch(void);
extern unsigned char reu_detect(void);
#define ROOM     800                    /* place libre minimale dans la fenetre */
#define FILL     (MAXDOC - ROOM - 200)  /* remplissage apres un glissement */
#define STEP     768                    /* taille mini du morceau qui quitte la fenetre */
static unsigned char reu_on;
static dpos win_base;                   /* position dans le document de text[0] */
static dpos suf_len;                    /* octets apres la fenetre, dans le REU */
static dpos suf_end;                    /* adresse REU de la fin du suffixe */
static unsigned char win_style;         /* style au debut de la fenetre */
static dpos lines_before;               /* lignes avant la fenetre (estimation apres un saut) */
#define fbuf outbuf                     /* lecture du document : 256 octets au plus */
static dpos first_mark;                 /* au plus la position du premier marqueur de style du document */
static dpos sc_lo1, sc_hi;              /* cache de style_at */
static unsigned char sc_st, sc_ok;
#endif

/* JiffyDOS passe au protocole lent des qu'un sprite est actif : on les coupe
   pendant les transferts de fichiers. */
#define disk_busy(on) POKE(0xD015, (on) ? 0 : 1)    /* 1 : la fleche ; le curseur se rallume au clignotement */

static void io_hide(void) { asm("sei"); POKE(0x01, 0x34); }
static void io_show(void) { POKE(0x01, 0x36); asm("cli"); }

/* ------------------------------------------------------------------ */
/* Dessin de base                                                      */

static unsigned put_text(unsigned x, unsigned char row, const char *s, unsigned char st)
{
    gbase = ROWBASE(row);
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
    unsigned card;
    unsigned char a, b, mask;
    for (card = x0 & 0xFFF8u; card < x1; card += 8) {
        a = card < x0 ? (unsigned char)(x0 - card) : 0;
        b = card + 8 > x1 ? (unsigned char)(x1 - card) : 8;
        mask = (unsigned char)((0xFF >> a) & (0xFF << (8 - b)));
        bm_xormask(ROWBASE(row) + card, mask);
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

static void draw_line(unsigned i)
{
    unsigned char row, st, c;
    unsigned p, end, x = TEXT_X0, lo = sel_lo(), hi = sel_hi(), sx = 0, ex = 0;
    if (i < top || i >= top + VISIBLE) return;
    row = (unsigned char)(TEXT_ROW0 + i - top);
    clear_cells(row, 0, 320);                   /* marges comprises : efface les ombres des menus */
    if (i >= nlines) return;
    end = lstart[i + 1];
    st = lstyle[i];
    gbase = ROWBASE(row);
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
}

static void draw_lines(unsigned from, unsigned to)
{
    unsigned i;
    if (from < top) from = top;
    for (i = from; i <= to && i < top + VISIBLE; ++i) draw_line(i);
}

#ifdef REU
static void draw_sbar(void);
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
static void place_cursor(void)
{
    unsigned i = line_of(cursor);
    if (i + 1 == top) {                         /* une ligne plus haut : decale l'image */
        bm_move(ROWC(TEXT_ROW0 + 1), ROWC(TEXT_ROW0), (VISIBLE - 1) * 320u);
        top = i;
        draw_line(i);
    } else if (i == top + VISIBLE) {            /* une ligne plus bas */
        bm_move(ROWC(TEXT_ROW0), ROWC(TEXT_ROW0 + 1), (VISIBLE - 1) * 320u);
        ++top;
        draw_line(i);
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
        for (i = 0; i < 8; ++i, ++y) p[i] = (y >= ty && y < ty + 16) ? 0x7E : 0x18;
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

static void draw_sbar(void)
{
    unsigned ty = sbar_ty();
#ifdef REUTEST
    *(unsigned *)0x02B6 = ty;
#endif
    draw_thumb(ty);
}
#endif

static void draw_status(void)
{
    char *buf = statusbuf, *b;
    if (dirty != shown_dirty || style != shown_style) draw_title();
    clear_cells(FOOT_ROW, 0, 320);
    b = buf;
    strcpy(b, S_LINE " ");
#ifdef REU
    b = ultoa_simple(b + strlen(b), lines_before + line_of(cursor) + 1);
    if (!suf_len) {                              /* le total n'est connu qu'en fin de document */
        *b++ = '/';
        ultoa_simple(b, lines_before + nlines);
    }
#else
    b = utoa_simple(b + strlen(b), line_of(cursor) + 1);
    *b++ = '/';
    utoa_simple(b, nlines);
#endif
    put_text(6, FOOT_ROW, buf, 0);
    strcpy(buf, S_FREE " ");
#ifdef REU
    ultoa_simple(buf + strlen(buf), suf_end - doclen - win_base - suf_len);
#else
    utoa_simple(buf + strlen(buf), MAXDOC - doclen);
#endif
    put_text(314 - text_width(buf, 0), FOOT_ROW, buf, 0);
#ifdef REU
    draw_sbar();
#endif
}

/* ------------------------------------------------------------------ */
/* Menus et panneaux                                                   */

#define NMENUS 5
static const char *const menu_title[NMENUS] = { S_MENU0, S_MENU1, S_MENU2, S_MENU3, S_MENU4 };
static const char *const menu_items[NMENUS][7] = {
#ifdef RTF
    { S_NEW, S_OPEN, S_SAVE, S_SAVEAS, S_OPENRTF, S_SAVERTF, 0 },
#else
    { S_NEW, S_OPEN, S_SAVE, S_SAVEAS, 0 },
#endif
    { S_CUT, S_COPY, S_PASTE, S_SELALL, S_FIND, S_FINDNEXT, 0 },
    { S_PLAIN, S_BOLD, S_ITALIC, S_UNDERLINE, 0 },
    { S_PGRAPH, S_PTEXT, S_PWIDE, 0 },
    { S_ABOUT, 0 },
};
static const char *const menu_keys[NMENUS][7] = {
#ifdef RTF
    { "", "", "F1", "", "", "", 0 },
#else
    { "", "", "F1", "", 0 },
#endif
    { "F6", "F2", "F4", "", "", "F8", 0 },
    { "", "F3", "F5", "F7", 0 },
    { "", "", "", 0 },
    { "", 0 },
};
static unsigned menu_x[NMENUS + 1];

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
    put_text(320 - 8 - text_width("Plume", BOLD), 0, "Plume", BOLD);
}

static void page_colors(void)
{
    unsigned char r;
    color_cells(0, 0, 40, COL_BAR);
    color_cells(1, 0, 40, COL_BAR);
    color_cells(FOOT_ROW, 0, 40, COL_BAR);
    for (r = TEXT_ROW0; r < FOOT_ROW; ++r) {
        color_cells(r, 0, 1, COL_MARGIN);
        color_cells(r, 1, 38, COL_PAGE);
        color_cells(r, 39, 1, COL_MARGIN);
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

/* ------------------------------------------------------------------ */
/* Souris et clavier                                                   */

static struct mouse_info mi;
static unsigned mouse_x;
static unsigned char mouse_y, clicked, extend, held, last_buttons;
/* Joystick : le pointeur avance d'un pixel par image, puis accelere. */
unsigned char joy_read(void);
extern unsigned char joy_p1;
static unsigned char joy_speed, joy_tick, joy_p1_time, wheel, wheel_last, wheel_t, wheel_n;

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
    if (joy_p1) joy_p1_time = PEEK(0xA2);
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
    if ((unsigned char)(PEEK(0xA2) - joy_p1_time) < 6) {
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

#define MAXFILES 13
#define DLG_ROW  7
#define DLG_X    40
#define DLG_W    240
#define BTN_W    72

static void dialog(unsigned char rows, const char *title)
{
    unsigned char y;
    unsigned w;
    panel(DLG_ROW, rows, DLG_X, DLG_W);
    for (y = 2; y < 7; y += 2) hline(DLG_ROW, DLG_X + 8, DLG_W - 16, y);
    if (title) {
        w = (text_width(title, BOLD) + 16) & 0xFFF8u;
        clear_cells(DLG_ROW, DLG_X + (DLG_W - w) / 2, w);
        put_text(DLG_X + (DLG_W - w) / 2 + 4, DLG_ROW, title, BOLD);
    }
}

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
    dialog(8, title);
    if (line1) put_text(DLG_X + 16, DLG_ROW + 2, line1, 0);
    if (line2) put_text(DLG_X + 16, DLG_ROW + 3, line2, 0);
    button(BTN_OK_X, DLG_ROW + 5, S_OK);
    if (cancel) button(BTN_CANCEL_X, DLG_ROW + 5, S_CANCEL);
    for (;;) {
        poll_mouse();
        key = read_key();
        if (key == CR || on_button(BTN_OK_X, DLG_ROW + 5)) { dialog_close(); return 1; }
        if (cancel && (key == 3 || on_button(BTN_CANCEL_X, DLG_ROW + 5))) { dialog_close(); return 0; }
    }
}

static unsigned char translate(unsigned char k);

static unsigned char ask_text(const char *title, const char *label, char *buf, unsigned char max,
                              unsigned char petscii)
{
    char *shown = inputbuf;
    unsigned char len = (unsigned char)strlen(buf), c, first = 1;
    for (;;) {
        if (first) {
            dialog(8, title);
            put_text(DLG_X + 16, DLG_ROW + 2, label, 0);
            button(BTN_OK_X, DLG_ROW + 5, S_OK);
            button(BTN_CANCEL_X, DLG_ROW + 5, S_CANCEL);
            first = 0;
        }
        clear_cells(DLG_ROW + 3, DLG_X + 8, DLG_W - 16);
        hline(DLG_ROW + 3, DLG_X + 16, DLG_W - 32, 7);
        if (petscii) petscii_to_text(shown, buf); else strcpy(shown, buf);
        strcat(shown, "|");
        put_text(DLG_X + 16, DLG_ROW + 3, shown, BOLD);
        do {
            poll_mouse();
            key = read_key();
            if (on_button(BTN_OK_X, DLG_ROW + 5)) key = CR;
            if (on_button(BTN_CANCEL_X, DLG_ROW + 5)) key = 3;
        } while (!key);
        if (key == CR && len) { dialog_close(); return 1; }
        if (key == 3) { dialog_close(); return 0; }
        if (key == 0x14) { if (len) buf[--len] = 0; continue; }
        if (len >= max) continue;
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

/* ------------------------------------------------------------------ */
/* Modifications du texte                                              */

static void refresh_after(unsigned first, unsigned char all)
{
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
    memmove(&text[a + n], &text[b], doclen - b);
    if (from_io) io_hide();
    memcpy(&text[a], src, n);
    if (from_io) io_show();
    doclen = doclen - (b - a) + n;
    dirty = 1;
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

/* Taille du REU : la banque b renvoie sur la banque 0 quand b est la taille. */
static void reu_probe_size(void)
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
    suf_end = (b ? (dpos)b << 16 : 0x1000000UL) - 0x10000UL;
}

/* Ecrit sur le canal 2 n octets du REU a partir de ra, par morceaux de 256. */
static void write_reu(dpos ra, dpos n)
{
    unsigned k;
    while (n) {
        k = n > 256 ? 256 : (unsigned)n;
        reu_move(1, outbuf, ra, k);
        cbm_write(2, outbuf, k);
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
    if (cbm_open(15, 8, 15, "")) return 99;
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
static unsigned char rtf_doc;                   /* le document vient d'un fichier RTF / a ete enregistre en RTF */
static unsigned char write_rtf(char *name);
#endif
static unsigned blk_free, blk_old, fsize[MAXFILES];
static unsigned char nfiles, fpage, fmore;

/* Page fpage de MAXFILES noms ; fmore : il y en a d'autres apres. */
static void scan_dir(const char *find)
{
    struct cbm_dirent e;
    unsigned char r, tot = 0, skip = (unsigned char)(fpage * MAXFILES);
    nfiles = 0;
    blk_free = 0xFFFF;
    blk_old = 0;
    if (cbm_opendir(1, 8)) return;
    for (;;) {
        r = cbm_readdir(1, &e);
        if (r == 2) { blk_free = e.size; break; }
        if (r) break;
        if (e.type != CBM_T_SEQ) continue;
        if (find && !strcmp(e.name, find)) blk_old = e.size;
        if (tot >= skip && nfiles < MAXFILES) { fsize[nfiles] = e.size; io_hide(); strcpy(names[nfiles++], e.name); io_show(); }
        ++tot;
    }
    cbm_closedir(1);
    fmore = tot > skip + nfiles;
}

/* Liste des fichiers en deux colonnes + blocs libres. Renvoie l'index choisi,
   254 pour "Nouveau..." (si newbtn), 255 si annule. */
static const unsigned char arrow_up[8] = { 0x18, 0x3C, 0x7E, 0x18, 0x18, 0x18, 0x18, 0x18 };

static void arrow_draw(unsigned char row, unsigned char up)
{
    unsigned char k;
    for (k = 0; k < 8; ++k) bm_or(ROWBASE(row) + DLG_X + DLG_W - 24 + k, arrow_up[up ? k : 7 - k]);
}

static unsigned char pick_file(const char *title, unsigned char newbtn)
{
    unsigned char i, rows, by, sel, last;
    char *shown = inputbuf, *b = (char *)outbuf;
    for (;;) {
        rows = (fpage || fmore) ? MAXFILES : nfiles;
        by = (unsigned char)(9 + rows);
        last = (unsigned char)(6 + rows);
        sel = 255;
        panel(4, (unsigned char)(rows + 7), DLG_X, DLG_W);
        put_text(DLG_X + 16, 5, title, BOLD);
        for (i = 0; i < nfiles; ++i) {          /* blocs (aligne a droite) puis le nom */
            utoa_simple(b, fsize[i]);
            put_text(DLG_X + 48 - text_width(b, 0), (unsigned char)(7 + i), b, 0);
            io_hide();
            strcpy(shown, names[i]);
            io_show();
            petscii_to_text(shown, shown);
            put_text(DLG_X + 60, (unsigned char)(7 + i), shown, 0);
        }
        strcpy(utoa_simple(b, blk_free), S_BLKFREE);
        put_text(DLG_X + 16, (unsigned char)(7 + rows), b, 0);
        button(BTN_CANCEL_X, by, S_CANCEL);
        if (newbtn) button(BTN_OK_X, by, S_NEWFILE);
        if (fpage) arrow_draw(7, 1);
        if (fmore) arrow_draw(last, 0);
        while (sel == 255) {
            poll_mouse();
            key = read_key();
            if (key == 3 || on_button(BTN_CANCEL_X, by)) break;
            if (newbtn && (key == CR || on_button(BTN_OK_X, by))) sel = 254;
            if (clicked && mouse_y / 8 >= 7 && mouse_y / 8 <= last) {
                i = (unsigned char)(mouse_y / 8 - 7);
                if (mouse_x >= DLG_X + DLG_W - 32 && (i == 0 || i == rows - 1) && (fpage || fmore)) {
                    key = i ? 0x11 : 0x91;      /* fleche cliquee : page suivante / precedente */
                } else if (i < nfiles) sel = i;
            }
            if ((key == 0x11 && fmore) || (key == 0x91 && fpage)) { sel = 253; }
        }
        if (sel != 253) return sel;
        fpage = (unsigned char)(fpage + (key == 0x11 ? 1 : -1));
        scan_dir(0);
    }
}

/* Nom de fichier : choix dans la liste (ecrase) ou saisie ("Nouveau..."). */
static unsigned char ask_name(const char *title, char *dest)
{
    char *buf = (char *)outbuf + 64;          /* libre hors impression */
    unsigned char sel = 254;
    fpage = 0;
    scan_dir(0);
    if (nfiles) {
        sel = pick_file(title, 1);
        if (sel == 255) { redraw_all(); return 0; }
        if (sel < 254) {
            io_hide();
            strcpy(dest, names[sel]);
            io_show();
            redraw_all();
            return 1;
        }
    }
    if (sel == 254 && nfiles) redraw_all();
    strcpy(buf, dest);
    if (!ask_text(title, S_FILENAME, buf, 16, 1)) return 0;
    strcpy(dest, buf);
    return 1;
}
#define ask_filename() ask_name(S_SAVEAS, filename)

/* Verifie qu'un fichier de "bytes" octets tient sur le disque et prepare cmdname.
   Assez de place : "@0:" (remplace sans risque). Sinon, si l'ancien fichier du meme
   nom libere assez, apres confirmation il est efface d'abord. */
static unsigned char fit_check(dpos bytes, const char *name)
{
    dpos nb = (bytes + 253) / 254;
    unsigned need = nb > 60000UL ? 60000u : (unsigned)nb;
    char *b = (char *)outbuf;
    scan_dir(name);
    strcpy(cmdname, "@0:");
    if (need > blk_free) {
        if (!blk_old || need > blk_free + blk_old) {
            strcpy(b, S_NEEDFREE);
            b = utoa_simple(b + strlen(b), need);
            *b++ = '/';
            utoa_simple(b, blk_free);
            message(S_SAVEAS, S_NOROOM, (char *)outbuf, 0);
            return 0;
        }
        if (!message(S_SAVEAS, S_REPL1, S_REPL2, 1)) return 0;
        strcpy(cmdname, "S0:");
        strcat(cmdname, name);
        cbm_open(15, 8, 15, cmdname);
        cbm_close(15);
        strcpy(cmdname, "0:");
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
static void save_doc(void)
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
    if (rtf_doc && filename[0]) { write_rtf(filename); return; }   /* un document RTF s'enregistre en RTF */
#endif
    if (!filename[0] && !ask_filename()) return;
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
    if (!fit_check(SAVELEN + hl, filename)) return;
#else
    strcpy(cmdname, "@0:");
    strcat(cmdname, filename);
    strcat(cmdname, ",S,W");
#endif
    disk_busy(1);
    if (cbm_open(2, 8, 2, cmdname) == 0) {
        cbm_write(2, head, HEADLEN);
#ifdef REU
        write_reu(0UL, win_base);
#endif
        cbm_write(2, text, doclen);
#ifdef REU
        write_reu(suf_end - suf_len, suf_len);
#endif
        cbm_close(2);
    }
    disk_busy(0);
    if (disk_status() >= 20) { show_disk_error(); return; }
    dirty = 0;
    shown_dirty = 255;                          /* le titre (le nom) se redessine toujours */
    draw_status();
}

static void new_doc(void)
{
#ifdef REU
    win_base = suf_len = lines_before = 0;
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
#ifdef RTF
    rtf_doc = 0;
#endif
    layout_all();
}

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
static void pull_tail(void)
{
    unsigned m, i;
    if (doclen >= FILL || !suf_len) return;
    m = FILL - doclen;
    if (m > suf_len) m = (unsigned)suf_len;
    reu_move(1, text + doclen, suf_end - suf_len, m);
    suf_len -= m;
    doclen += m;
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
    if (anchor != NOSEL) anchor = anchor > c ? anchor - c : 0;
    top = top > L ? top - L : 0;
}

/* Vers la fin du document : le haut sort, la suite du suffixe entre. */
static void slide_fwd(void)
{
    unsigned L, i = nlines - 1, lim = top > 6 ? top - 6 : 0;      /* on ne touche pas aux lignes de l'ecran */
    for (L = 1; L < nlines && lstart[L] < STEP; ++L) ;
    if (L > lim) L = lim;
    if (L) drop_head(L);
    pull_tail();
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
    cursor += len;
    if (anchor != NOSEL) anchor += len;
    top = line_of(ts);
    draw_lines(nP ? nP - 1 : 0, nP + 1);
    trim_lines();
}

/* La fenetre repart de B (debut de paragraphe) : tout le reste est range dans
   le REU, le trou entre le debut et le suffixe se deplace jusqu'a B. */
static void relocate(dpos B)
{
    dpos d;
    unsigned k, bpl = nlines ? doclen / nlines : 40;
    unsigned char st;
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
    lines_before = bpl ? B / bpl : 0;
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
    else if (suf_len && top + VISIBLE + 8 > nlines) slide_fwd();       /* le bas de l'ecran approche la fin de la fenetre */
    else if (win_base && top < 6) slide_back();                         /* le haut de l'ecran approche le debut */
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
    for (; n; n -= k, dst += k) {
        k = n > 256 ? 256 : (unsigned)n;
        if ((unsigned)cbm_read(2, outbuf, k) != k) return 0;
        note_marks(outbuf, k, suf_len - n);
        reu_move(0, outbuf, dst, k);
    }
    doclen = 0;
    layout_all();
    pull_tail();
    return 1;
}
#endif

static void load_named(void)
{
#ifdef REU
    unsigned char head[8], ok = 1;
    dpos n;
#else
    unsigned char head[6], ok = 1;
    unsigned n;
#endif
    strcpy(cmdname, filename);
    strcat(cmdname, ",S,R");
    disk_busy(1);
    if (cbm_open(2, 8, 2, cmdname) != 0) { disk_busy(0); disk_status(); show_disk_error(); return; }
#ifdef RTF
    rtf_doc = 0;
#endif
    cbm_read(2, head, 6);
    n = head[4] | (head[5] << 8);
#ifdef REU
    if (!memcmp(head, "PLM3", 4)) {
        cbm_read(2, head + 6, 2);
        n |= ((dpos)head[6] << 16) | ((dpos)head[7] << 24);
    }
    win_base = suf_len = lines_before = 0;
    win_style = sc_ok = 0;
    first_mark = 0xFFFFFFFFUL;
    if (reu_on && n > MAXDOC && n <= suf_end && (!memcmp(head, "PLM2", 4) || !memcmp(head, "PLM3", 4))) {
        ok = load_big(n);
    } else
#endif
    if (!memcmp(head, "PLM2", 4) && n <= MAXDOC) {
        cbm_read(2, text, (unsigned)n);
        doclen = (unsigned)n;
#ifndef REU
    } else if (!memcmp(head, "PLM1", 4) && n <= MAXDOC) {
        ok = load_plm1((unsigned)n);
#endif
    } else {
        ok = 0;
    }
    cbm_close(2);
    disk_busy(0);
    if (!ok) {
        doclen = 0;
#ifdef REU
        suf_len = 0;
#endif
        message(S_OPEN, S_BADFILE, 0, 0);
    }
    else if (disk_status() >= 20) { show_disk_error(); return; }
    cursor = 0;
    anchor = NOSEL;
    top = 0;
    dirty = 0;
    layout_all();
    style = style_before(0);
}

#ifdef RTF
static unsigned char rtf_mode;
static void load_rtf(void);
#endif

#ifdef REU
static void open_doc(void)
{
    unsigned char sel;
    if (dirty && !message(S_OPEN, S_LOSE, 0, 1)) return;
    fpage = 0;
    scan_dir(0);
    if (!nfiles) {
        if (disk_status() >= 20) show_disk_error(); else message(S_OPEN, S_NOFILES, 0, 0);
        return;
    }
    sel = pick_file(S_PICK, 0);
    if (sel < 254) {
        io_hide();
        strcpy(filename, names[sel]);
        io_show();
#ifdef RTF
        if (rtf_mode) load_rtf(); else
#endif
        load_named();
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
    if (cbm_opendir(1, 8) == 0) {
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
#ifdef RTF
        if (rtf_mode) load_rtf(); else
#endif
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

/* ------------------------------------------------------------------ */
/* Edition                                                             */

static void set_selection(unsigned new_anchor, unsigned new_cursor)
{
    unsigned a = sel_lo(), b = sel_hi(), la, lb;
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
    return anchor != NOSEL && anchor != cursor;
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

static void delete_selection(void)
{
    if (has_selection()) delete_range(sel_lo(), sel_hi());
    else anchor = NOSEL;
}

static void insert_char(unsigned char c)
{
    unsigned char seq[3], n = 0, here;
    unsigned p;
    delete_selection();
    p = cursor;
    here = style_before(p);
    if (style != here) seq[n++] = MARK(style);
    seq[n++] = c;
    if (style != here && p < doclen && !IS_MARK(text[p])) seq[n++] = MARK(here);
    cursor = p + (style != here ? 2 : 1);
    if (!replace(p, p, seq, n, 0)) cursor = p;
}

static void delete_before(void)
{
    if (has_selection()) { delete_selection(); return; }
    anchor = NOSEL;
    if (cursor) delete_range(cursor - 1, cursor);
}

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

/* Style : sur la selection s'il y en a une, sinon pour la frappe. bit = 0 : normal.
   La selection et la suite du texte sont deplacees en haut du tampon, puis la
   selection est reecrite en un seul passage avec ses nouveaux marqueurs. */
static void apply_style(unsigned char bit)
{
    unsigned a, b, w, sel_end, need = 0;
    unsigned char all, every = 1, old_st, start_st, emitted, want, c, k;
    unsigned char map[8];                       /* ancien style -> nouveau style */
    unsigned char *q, *end, *rd;
    unsigned first;
    if (!has_selection()) {
        if (bit) style ^= bit; else style = 0;
        draw_status();
        return;
    }
    a = sel_lo();
    b = sel_hi();
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
    anchor = a;
    cursor = sel_end;
    first = relayout(a, b, w, &all);
    if (!all && relayout_last < line_of(sel_end)) relayout_last = line_of(sel_end);
    refresh_after(first, all);
}

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
static void find_next(void)
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

static void find_dialog(void)
{
    if (ask_text(S_FIND, S_FINDWHAT, (char *)findtext, 22, 0)) find_next();
}

#ifdef RTF
/* ------------------------------------------------------------------ */
/* RTF : lecture en flux (nouveau document) et ecriture                */

/* Unicode des codes internes 128 a 180 (voir font_map.txt). */
static const unsigned ucode[53] = {
    0xE0, 0xE2, 0xE4, 0xE7, 0xE9, 0xE8, 0xEA, 0xEB, 0xEE, 0xEF, 0xF4, 0xF6, 0xF9, 0xFB, 0xFC,
    0xC0, 0xC2, 0xC7, 0xC9, 0xC8, 0xCA, 0xCB, 0xCE, 0xCF, 0xD4, 0xD9, 0xDB, 0xDC,
    0x153, 0x152, 0xAB, 0xBB, 0xB0, 0x20AC, 0xE6, 0xF8, 0xE5, 0xC6, 0xD8, 0xC5,
    0xE1, 0xC1, 0xED, 0xCD, 0xF3, 0xD3, 0xFA, 0xDA, 0x151, 0x150, 0x171, 0x170, 0xD6
};
/* Windows-1252, octets 0x80 a 0x9F. */
static const unsigned cp1252_hi[32] = {
    0x20AC, '?', ',', 'f', '"', 0x2026, '+', '+', '^', '%', 'S', '<', 0x152, '?', 'Z', '?',
    '?', 0x2018, 0x2019, 0x201C, 0x201D, '*', 0x2013, 0x2014, '~', 'T', 's', '>', 0x153, '?', 'z', 'Y'
};

static unsigned char rtf_code(unsigned u)
{
    unsigned char i;
    if (u < 128) return u < 32 ? ' ' : (unsigned char)u;
    for (i = 0; i < sizeof ucode / 2; ++i) if (ucode[i] == u) return 128 + i;
    if (u == 0x2018 || u == 0x2019) return '\'';
    if (u == 0x201C || u == 0x201D) return '"';
    if (u == 0x2013 || u == 0x2014) return '-';
    if (u == 0xA0) return ' ';
    return '?';
}

static unsigned char rcur, remit, rdepth, rfirst, rst, rlen, rneg, rhas, rhex, rskip, ruc, rfull;
static unsigned rnum;
static unsigned char rstack[24], rword[8];

static void r_flush(void)
{
    reu_move(0, text, win_base, doclen);
    win_base += doclen;
    doclen = 0;
}

static void r_emit(unsigned char c)
{
    if (doclen >= MAXDOC - 1000u) r_flush();
    if (win_base + doclen + 4096 >= suf_end) { rfull = 1; return; }
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
    else if (!strcmp(w, "tab")) r_put(' ');
    else if (!strcmp(w, "u")) { r_uni(rneg ? 0 - rnum : rnum); rskip = ruc; }
    else if (!strcmp(w, "uc")) ruc = (unsigned char)rnum;
}

static unsigned char r_hex(unsigned char c)
{
    return c <= '9' ? c - '0' : (unsigned char)((c | 32) - 'a' + 10);
}

#define R_ALPHA(c) (((c) | 32) >= 'a' && ((c) | 32) <= 'z')

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
        r_uni(rnum);
    }
}

/* Charge un fichier RTF comme nouveau document : le texte converti s'empile
   dans la banque 0, puis passe en bloc dans le suffixe (comme load_big). */
static void load_rtf(void)
{
    unsigned char ok = 1, first = 1;
    unsigned k;
    dpos off, total;
    int n;
    strcpy(cmdname, filename);
    strcat(cmdname, ",S,R");
    disk_busy(1);
    if (cbm_open(2, 8, 2, cmdname) != 0) { disk_busy(0); disk_status(); show_disk_error(); return; }
    win_base = suf_len = lines_before = 0;
    win_style = sc_ok = doclen = 0;
    first_mark = 0xFFFFFFFFUL;
    rcur = remit = rdepth = rfirst = rst = rskip = rfull = 0;
    ruc = 1;
    while ((n = cbm_read(2, outbuf, 256)) > 0) {
        if (first) {
            first = 0;
            if (n < 5 || memcmp(outbuf, "{\\rtf", 5)) { ok = 0; break; }
        }
        for (k = 0; k < (unsigned)n; ++k) r_byte(outbuf[k]);
        if (rfull) break;
    }
    cbm_close(2);
    disk_busy(0);
    if (ok) {
        r_flush();
        total = win_base;
        for (off = total; off; ) {              /* de la fin au debut : les zones peuvent se recouvrir */
            k = off > 256 ? 256 : (unsigned)off;
            off -= k;
            reu_move(1, outbuf, off, k);
            reu_move(0, outbuf, suf_end - total + off, k);
        }
        suf_len = total;
        win_base = doclen = 0;
        rtf_doc = 1;                            /* garde le nom : F1 enregistre en RTF au meme endroit */
    } else {
        message(S_OPENRTF, S_BADRTF, 0, 0);
        filename[0] = 0;
    }
    cursor = 0;
    anchor = NOSEL;
    top = 0;
    dirty = ok;
    layout_all();
    pull_tail();
    style = style_before(0);
    if (rfull) message(S_OPENRTF, S_FULL, 0, 0);
}

/* Ecrit tout le document en RTF (octets non ASCII : \uN?). */
static unsigned char *w_str(unsigned char *o, const char *s)
{
    strcpy((char *)o, s);
    return o + strlen(s);
}

static dpos rtf_tot;                            /* essai a blanc : octets comptes */
static unsigned char rtf_dry;
static char rtfname[22];

static void rtf_put(unsigned n)
{
    if (rtf_dry) rtf_tot += n; else cbm_write(2, outbuf, n);
}

static void rtf_body(void)
{
    unsigned char *wbuf = outbuf + 400;          /* entree : 64 octets en fin de outbuf, sortie en debut */
    unsigned char *o, c, i, ws = 0, ns;
    dpos g, tot;
    unsigned k;
    o = w_str(outbuf, "{\\rtf1\\ansi\\ansicpg1252\\deff0{\\fonttbl{\\f0 Times New Roman;}}\\f0\\fs24 ");
    tot = doc_total();
    for (g = 0; g < tot; g += k) {
        k = tot - g > 64 ? 64 : (unsigned)(tot - g);
        doc_read(wbuf, g, k);
        for (i = 0; i < k; ++i) {
            c = wbuf[i];
            if (o > outbuf + 300) { rtf_put((unsigned)(o - outbuf)); o = outbuf; }
            if (IS_MARK(c)) {
                ns = c & 7;
                if (ns == ws) continue;
                if ((ns ^ ws) & BOLD) o = w_str(o, ns & BOLD ? "\\b" : "\\b0");
                if ((ns ^ ws) & ITALIC) o = w_str(o, ns & ITALIC ? "\\i" : "\\i0");
                if ((ns ^ ws) & UNDERLINE) o = w_str(o, ns & UNDERLINE ? "\\ul" : "\\ulnone");
                *o++ = ' ';
                ws = ns;
            } else if (c == CR) {
                o = w_str(o, "\\par\n");
            } else if (c >= 128) {
                o = w_str(o, "\\u");
                o = (unsigned char *)utoa_simple((char *)o, c - 128u < sizeof ucode / 2 ? ucode[c - 128] : '?');
                *o++ = '?';
            } else if (c >= 32) {
                if (c == '\\' || c == '{' || c == '}') *o++ = '\\';
                *o++ = c;
            }
        }
    }
    *o++ = '}';
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
    disk_busy(1);
    if (cbm_open(2, 8, 2, cmdname) == 0) {
        rtf_body();
        cbm_close(2);
    }
    disk_busy(0);
    if (disk_status() >= 20) { show_disk_error(); return 0; }
    dirty = 0;
    draw_status();
    return 1;
}

static void save_rtf(void)
{
    strcpy(rtfname, filename);
    if (!ask_name(S_SAVERTF, rtfname)) return;
    if (!strchr(rtfname, '.') && strlen(rtfname) <= 12) strcat(rtfname, ".\x52\x54\x46");   /* ".rtf" en PETSCII */
    if (write_rtf(rtfname)) {                   /* le document porte maintenant ce nom, en RTF */
        strcpy(filename, rtfname);
        rtf_doc = 1;
        shown_dirty = 255;
        draw_status();
    }
}
#endif

/* ------------------------------------------------------------------ */
/* Menus                                                               */

static unsigned char run_menu(unsigned char m)
{
    unsigned char n = 0, i, hover = 255, row, choice = 255;
    unsigned x0 = menu_x[m], width = 0, t;
    while (menu_items[m][n]) {
        t = text_width(menu_items[m][n], 0) + text_width(menu_keys[m][n], 0) + 16;
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
        put_text(x0 + width - 8 - text_width(menu_keys[m][i], 0), row, menu_keys[m][i], 0);
        if (m == 2 && ((i && (style & (1 << (i - 1)))) || (!i && !style)) && !has_selection()) {
            put_text(x0 + 4, row, "*", 0);
        }
    }
    for (;;) {
        poll_mouse();
        key = read_key();
        if (key == 3) break;
        row = mouse_y / 8;
        i = (row >= 2 && row <= n + 1 && mouse_x >= x0 && mouse_x < x0 + width) ? row - 2 : 255;
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
    place_cursor();
    return choice;
}

static void do_menu(unsigned char m, unsigned char item)
{
    if (item == 255) return;
    switch (m) {
    case 0:
        if (item == 0 && (!dirty || message(S_NEW, S_LOSE, 0, 1))) { new_doc(); redraw_all(); }
        if (item == 1) open_doc();
        if (item == 2) save_doc();
        if (item == 3 && ask_filename()) {
#ifdef RTF
            rtf_doc = 0;
#endif
            save_doc();
        }
#ifdef RTF
        if (item == 4) {
            if (!reu_on) message(S_OPENRTF, S_NEEDREU, 0, 0);
            else { rtf_mode = 1; open_doc(); rtf_mode = 0; }
        }
        if (item == 5) save_rtf();
#endif
        break;
    case 1:
        if (item == 0) cut_selection();
        if (item == 1) copy_selection();
        if (item == 2) paste();
        if (item == 3) set_selection(0, doclen);
        if (item == 4) find_dialog();
        if (item == 5) find_next();
        break;
    case 2:
        apply_style(item ? (unsigned char)(1 << (item - 1)) : 0);
        break;
    case 3:
        print_doc(item);
        break;
    case 4:
        message(S_ABOUT, S_ABOUT1, S_ABOUT3, 0);
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

static unsigned pos_at_mouse(void)
{
    unsigned i = top + (mouse_y / 8 >= TEXT_ROW0 ? mouse_y / 8 - TEXT_ROW0 : 0);
#ifdef REU
    if (mouse_y / 8 < TEXT_ROW0 && held && top) i = top - 1;     /* au-dessus du texte : defile vers le haut */
#endif
    if (i >= nlines) i = nlines - 1;
    return pos_at(i, mouse_x >= TEXT_X0 ? mouse_x - TEXT_X0 : 0);
}

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
    POKE(0xDC00, 0xFD);                 /* rangee 1 : Shift gauche, bit 7 */
    c = ~PEEK(0xDC01);
    POKE(0xDC00, 0xBF);                 /* rangee 6 : Shift droit, bit 4 */
    d = ~PEEK(0xDC01);
    shift = ((c & 0x80) | (d & 0x10)) ? HU_KEYS : 0;
    for (i = 0; i < 2 * HU_KEYS; i += 2) {
        POKE(0xDC00, hu_keys[i]);
        if (!(~PEEK(0xDC01) & hu_keys[i + 1])) continue;
        POKE(0xDC00, 0x7F);
        asm("cli");
        if (i == hu_last) return 0;
        hu_last = i;
        return 0xE0 + (i >> 1) + shift;
    }
    hu_last = 0xFE;                     /* C= tenue, aucune touche de la couche */
    asm("cli");
    return 0;
}
#else
static const unsigned char cbm_keys[]   = { 0xB1, 0xB2, 0xA3, 0xB0, 0xAB, 0xBC, 0xB8, 0xB7, 0xB9, 0xA2, 0xA1, 0xB6, 0xAF };
static const unsigned char cbm_accent[] = { 132,  133,  134,  128,  129,  131,  140,  141,  138,  136,  137,  135,  156 };
#endif
static unsigned char translate(unsigned char k)
{
#ifndef KBD_HU
    unsigned char i;
#endif
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
    if (k == 0xA0) return ' ';
#ifndef KBD_HU
    for (i = 0; i < sizeof cbm_keys; ++i) if (k == cbm_keys[i]) return cbm_accent[i];
#endif
    return 0;
}

/* Apres un deplacement, le style de frappe suit le texte. */
static void cursor_moved(void)
{
    style = style_before(cursor);
    place_cursor();
    draw_status();
}

/* Billentyuklik : un clic de bruit (voix 1). L'enveloppe (attaque 0, decroissance
   courte, tenue 0) s'eteint toute seule. */
#define CLICK_NOISE_FREQ  0x58          /* octet haut de la frequence du bruit */
#define CLICK_NOISE_DECAY 0x00          /* 0 = 6 ms, 1 = 24 ms, 2 = 48 ms */
#define CLICK_VOLUME      15
static unsigned char sound_on = 1;

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

static void handle_key(unsigned char k)
{
    unsigned char c;
    switch (k) {
    case CR:   key_click(); insert_char(CR); return;
    case 0x14: key_click(); delete_before(); return;
    case 0x1D: clear_selection(); move_right(); break;
    case 0x9D: clear_selection(); move_left(); break;
    case 0x11: clear_selection(); move_vertical(1); break;
    case 0x91: clear_selection(); move_vertical(-1); break;
    case 0x13: clear_selection(); cursor = lstart[line_of(cursor)]; break;
    case 3:    clear_selection(); return;
    case 0x85: save_doc(); return;
    case 0x86: apply_style(BOLD); return;
    case 0x87: apply_style(ITALIC); return;
    case 0x88: apply_style(UNDERLINE); return;
    case 0x89: copy_selection(); return;
    case 0x8A: paste(); return;
    case 0x8B: cut_selection(); return;
    case 0x8C: find_next(); return;
    case 0x92: next_theme(); redraw_all();             /* CTRL+0 : couleurs */
#ifdef REU
        led_theme(theme_no);                            /* et les leds du C64U */
#endif
        return;
    case 0x12: sound_on ^= 1; key_click(); return;      /* CTRL+9 : clic de touche oui / non */
#ifdef REUTEST
    case 0x8D: print_doc(1); return;
    case 0xA0: load_named(); redraw_all(); return;        /* le nom est dans filename (outil de test) */
#ifdef RTF
    case 0x9E: load_rtf(); redraw_all(); return;          /* le nom est dans filename (outil de test) */
    case 0x9F: strcpy(rtfname, "\x4F\x55\x54"); write_rtf(rtfname); return;       /* OUT */
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
static void sbar_goto(unsigned y)
{
    unsigned L;
    dpos R, g;
    if (SBAR_LINES) {
        R = nlines > VISIBLE ? nlines - VISIBLE : 0;
        L = y == SBAR_H - 16 ? (unsigned)R : (unsigned)((R / (SBAR_H - 16)) * y + ((R % (SBAR_H - 16)) * y) / (SBAR_H - 16));
        clear_selection();
    } else {
        R = sbar_range();
        g = y == SBAR_H - 16 ? R : (R / (SBAR_H - 16)) * y;
        goto_global(g, g);
        anchor = NOSEL;
        L = line_of((unsigned)(g - win_base));
    }
    cursor = lstart[L];
    top = L;
    maybe_slide();                              /* la fenetre peut glisser : d'abord elle, puis l'ecran */
    if (!suf_len && nlines > VISIBLE && top + VISIBLE > nlines) {      /* fin du document : derniere page pleine */
        top = nlines - VISIBLE;
        cursor = lstart[top];
    }
    draw_text_area();
    place_cursor();
    cursor_moved();
}

/* Clic ou glissement : le curseur suit la souris tout de suite, le texte
   suit quand la souris s'arrete un instant (6 images) et au relachement. */
static void sbar_drag(void)
{
    unsigned yy = mouse_y > TEXT_ROW0 * 8 ? mouse_y - TEXT_ROW0 * 8 : 0, ty = sbar_ty(), y, shown = 0xFFFF;
    unsigned char off = (yy >= ty && yy < ty + 16) ? (unsigned char)(yy - ty) : 8, since = PEEK(0xA2), todo = 0;
    do {
        yy = mouse_y > TEXT_ROW0 * 8 ? mouse_y - TEXT_ROW0 * 8 : 0;
        y = yy > off ? yy - off : 0;
        if (y > SBAR_H - 16) y = SBAR_H - 16;
        if (y != shown) { shown = y; since = PEEK(0xA2); todo = 1; draw_thumb(y); }
        else if (todo && (unsigned char)(PEEK(0xA2) - since) >= 6) { todo = 0; sbar_goto(y); }
        poll_mouse();
    } while (held);
    if (todo) sbar_goto(y);
}
#endif

static void handle_click(void)
{
    unsigned char row = mouse_y / 8, m, k;
    unsigned p;
    if (row == 0) {
        for (m = 0; m < NMENUS; ++m) {
            if (mouse_x >= menu_x[m] && mouse_x < menu_x[m + 1]) { do_menu(m, run_menu(m)); return; }
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
    if (anchor != NOSEL) set_selection(NOSEL, p);
    else cursor = p;
    cursor_moved();
    anchor = p;
    while (held) {
        poll_mouse();
        p = pos_at_mouse();
        if (p != cursor) set_selection(anchor, p);
#ifdef REU
        maybe_slide();
#endif
    }
    if (anchor == cursor) anchor = NOSEL;
}

/* ------------------------------------------------------------------ */
/* Sprites : fleche (sprite 0) et curseur texte (sprites 1 et 2)       */

static const unsigned char arrow[] = {
    0x80, 0xC0, 0xE0, 0xF0, 0xF8, 0xFC, 0xFE, 0xF8, 0xD8, 0x8C, 0x0C, 0x06, 0x06
};

static void sprites_init(void)
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

void main(void)
{
    POKE(0x0291, 0x80);
    findtext[0] = 0;
#ifdef KBD_HU
    hu_last = 0xFF;                     /* aucune touche de la couche vue */
#endif                            /* tampon cassette : pas remis a zero */
    gfx_init();
    sprites_init();
    set_theme();
    mouse_install(&mouse_def_callbacks, mouse_static_stddrv);
    mouse_show();
#ifdef REU
    reu_on = reu_detect();
    if (reu_on) {
        reu_probe_size();
        if (suf_end < 0x30000UL) reu_on = 0;        /* il faut au moins 256 Ko */
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
    /* repere pour les outils de test : adresses du texte et de sa longueur */
    *(unsigned *)0x02AB = (unsigned)text;
    *(unsigned *)0x02AD = (unsigned)&doclen;
    for (;;) {
        poll_mouse();
        if (wheel) {
            for (; wheel_n; --wheel_n) handle_key(wheel == 4 ? 0x91 : 0x11);
            wheel = 0;
#ifdef REU
            maybe_slide();
#endif
        }
        if (clicked) {
            handle_click();
#ifdef REU
            maybe_slide();
#endif
        }
        key = read_key();
        if (key) {
            handle_key(key);
#ifdef REU
            maybe_slide();
#endif
        }
        blink_cursor();
    }
}
