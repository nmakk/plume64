/* Lemezsebesseg-meres: milyen kijelzo-allapotban gyors a JiffyDOS ?
   Eredmeny $C000-tol: 6 meres x [iras jiffy lo,hi][olvasas jiffy lo,hi][SAVE][LOAD], vege: $C0FF = $EE.
   Meresek: 0 kepernyo be, nincs sprite | 1 kepernyo be + sprite | 2 kepernyo ki | 3 kepernyo ki + sprite
   Minden meres 8192 bajtot ir (SEQ, 256-os darabokkal), visszaolvassa; majd PRG-kent SAVE / LOAD. */
#include <cbm.h>
#include <string.h>
#include <peekpoke.h>

#define OUT ((unsigned *)0xC000)
#define BUFADDR 0x4000
static unsigned char buf[256];

static unsigned now(void)
{
    unsigned char hi, lo, hi2;
    do { hi = PEEK(0xA1); lo = PEEK(0xA2); hi2 = PEEK(0xA1); } while (hi != hi2);
    return ((unsigned)hi << 8) | lo;
}

static void mode(unsigned char m)
{
    POKE(0xD015, (m & 1) ? 1 : 0);                       /* sprite 0 */
    POKE(0xD011, (m & 2) ? (PEEK(0xD011) & 0xEF) : (PEEK(0xD011) | 0x10));   /* kepernyo */
}

void main(void)
{
    unsigned char m, i;
    unsigned t0, w, r, sv, ld;
    unsigned char *p = (unsigned char *)BUFADDR;
    memset(buf, 'x', sizeof buf);
    for (m = 0; m < 4; ++m) {
        mode(m);
        cbm_open(15, 8, 15, "s:jt");
        cbm_close(15);
        t0 = now();
        cbm_open(2, 8, 2, "jt,s,w");
        for (i = 0; i < 32; ++i) cbm_write(2, buf, 256);
        cbm_close(2);
        w = now() - t0;
        t0 = now();
        cbm_open(2, 8, 2, "jt,s,r");
        for (i = 0; i < 32; ++i) cbm_read(2, buf, 256);
        cbm_close(2);
        r = now() - t0;
        cbm_open(15, 8, 15, "s:jtp");
        cbm_close(15);
        t0 = now();
        cbm_save("jtp", 8, p, 8192);
        sv = now() - t0;
        t0 = now();
        cbm_load("jtp", 8, p);
        ld = now() - t0;
        mode(0);
        OUT[m * 4 + 0] = w;
        OUT[m * 4 + 1] = r;
        OUT[m * 4 + 2] = sv;
        OUT[m * 4 + 3] = ld;
    }
    POKE(0xC0FF, 0xEE);
    for (;;) ;
}
