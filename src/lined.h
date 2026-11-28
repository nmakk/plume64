/* Egysoros szerkeszto : kurzor (balra / jobbra / Home / Backspace / Shift+Del), beszuras a kurzornal, villogo kurzor.
   Az includolo megadja : EDN(n) az elotag, ED_ST az allapot cime (6 bajt : kurzor, ablak eleje, kurzor lathato, ido, x). Minden hasznalo sajat peldany. */
#define ED_P   (*(unsigned char *)(ED_ST))
#define ED_W   (*(unsigned char *)(ED_ST + 1))
#define ED_CS  (*(unsigned char *)(ED_ST + 2))
#define ED_T   (*(unsigned char *)(ED_ST + 3))
#define ED_CX  (*(unsigned *)(ED_ST + 4))
/* Egy billentyu : 1, ha valtozott a szoveg vagy a kurzor ; ch a beszurando jel (0 : nincs). */
static unsigned char EDN(edit)(char *buf, unsigned char max, unsigned char key, unsigned char ch)
{
    unsigned char n = (unsigned char)strlen(buf), p = ED_P;
    if (p > n) p = n;
    if (key == 0x1D) { if (p < n) ++p; }
    else if (key == 0x9D) { if (p) --p; }
    else if (key == 0x13) p = p ? 0 : n;
    else if (key == 0x14) { if (!p) return 0; memmove(buf + p - 1, buf + p, n - p + 1); --p; }
    else if (key == 0x94) { if (p >= n) return 0; memmove(buf + p, buf + p + 1, n - p); }
    else if (ch && n < max) { memmove(buf + p + 1, buf + p, n - p + 1); buf[p++] = (char)ch; }
    else return 0;
    ED_P = p;
    return 1;
}
/* A mezo kirajzolasa (x, w : kepponttban ; pet : a puffer PETSCII) ; act : a kurzor is megjelenik */
static void EDN(show)(const char *buf, unsigned char row, unsigned x, unsigned w, unsigned char pet, unsigned char act)
{
    char *t = (char *)outbuf + 120;
    unsigned char n, p = 0, i, ws = 0;
    unsigned tw = 0, cx = 0;
    if (pet) petscii_to_text(t, buf); else strcpy(t, buf);
    n = (unsigned char)strlen(t);
    gstyle = BOLD;
    if (act) {
        p = ED_P;
        if (p > n) p = n;
        ED_P = p;
        ws = ED_W;
        if (ws > p) ws = p;
        for (;;) {                                      /* a kurzor latszodjon : az ablak eleje addig csuszik, amig a kurzorig befer */
            for (cx = 0, i = ws; i < p; ++i) cx += glyph_width((unsigned char)t[i]);
            if (cx + 8 <= w || ws >= p) break;
            ++ws;
        }
        ED_W = ws;
    }
    clear_cells(row, x, w);
    for (i = ws; i < n; ++i) { tw += glyph_width((unsigned char)t[i]); if (tw + 4 > w) break; }
    t[i] = 0;
    put_text(x + 2, row, t + ws, BOLD);
    if (act) {
        ED_CX = x + 2 + cx;
        ED_CS = 1;
        ED_T = PEEK(0xA2);
        xor_span(row, ED_CX, ED_CX + 2);
    }
}
/* A varakozo ciklusban : a kurzor villog (kb. fel masodpercenkent) */
static void EDN(blink)(unsigned char row)
{
    if ((unsigned char)(PEEK(0xA2) - ED_T) >= 30) {
        xor_span(row, ED_CX, ED_CX + 2);
        ED_CS ^= 1;
        ED_T = PEEK(0xA2);
    }
}
#undef ED_P
#undef ED_W
#undef ED_CS
#undef ED_T
#undef ED_CX
