# REU-build tesztelőeszközök (nincsenek commitolva)

C64 Ultimate-on (REST API, `192.168.0.158`) futtatva. A C64U alapból 64 MHz-en jár; az 1 MHz-es
méréshez a `measure*.py` átállítja a `CPU Speed`-et és a végén visszaállítja 64-re.

**Fontos:** lemezre írás/olvasás közben NE olvasd a C64U memóriáját a REST-en át (DMA -> az IEC
elakad, a fájl `*SEQ` marad). Polling nélkül várj.

## REUTEST build (a 60 KB-os generált dokumentum a REU-ban, CTRL+1 / CTRL+2 = 100 sor le/fel)
    S=<munkakönyvtár>; mkdir -p $S/obj
    cl65 -t c64 -O -Or -Cl -DREU -DREUTEST -DLANG_EN -o $S/rt.prg -m $S/rt.map --obj-path $S/obj \
      -Wl -D,__HIMEM__=0xC000 -Wl -D,__STACKSIZE__=0x200 src/main.c src/gfx.s src/font.s src/reu.s
    N=$(python3 -I -c "import os;print((os.path.getsize('$S/rt.prg')-2+255)//256)")
    ca65 -D PRG_PAGES=$N -o $S/rt_crt.o crt-md.s && ld65 -C crt-md.cfg -o $S/rt_crt.bin $S/rt_crt.o
    python3 -I tools/mkcrt.py $S/rt_crt.bin $S/rt.prg $S/rt.crt "PLUME TEST" md
(`-DNOCHECK` kikapcsolja a csúszás utáni önellenőrzést — a késleltetés méréséhez.)

## Futtatás
- `python3 -I tools/reutest/drive.py $S 130 140` : végigmegy a dokumentumon le és fel, minden 30. lépés után ellenőriz (`reuverify.py rt.map`).
- `python3 -I tools/reutest/measure_step2.py $S` : 1 MHz-en méri a billentyűnkénti késleltetést (egysoros mozgás, 330 lépés).
- `gen_testdoc.py out.bin 40000` : PLM2 tesztfájl + hossz és ellenőrzőösszeg (`rol16 ^ bajt`); `diskcheck.s` olvassa vissza a lemezről.
- Lemezkép felcsatolása: `curl -X POST --data-binary @test.d64 "http://192.168.0.158/v1/drives/a:mount?type=d64&mode=readwrite"` (`c1541 -format "plumetest,01" d64 test.d64 -write testdoc.bin "big,s"`).
