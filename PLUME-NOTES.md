# Plume64 – munkajegyzet (átadás Claude Code-nak)

## Eddig elkészült (plume-kurzor-szinek.patch)
- **Inverz blokk-kurzor**: a C64-es stílusú villogó blokk azon a karakteren, amelyik előtt a kurzor áll. A beszúró mód maradt.
  - Sprite 1 (blokk 1, `$C040`): a karakter képe a bitmapből kiolvasva, a lap háttérszínével.
  - Sprite 2 (blokk 47, `$CBC0`, a `lstyle` után szabad): teli blokk a karakter szélességében, a lap tintaszínével.
  - Sor végén / CR-nél a blokk 7 pont széles.
  - A kód a `src/gfx.s`-ben van: `_cur_place`, `_cur_sample`, `_blink_cursor`, `_cursor_off`. A C-oldali `place_cursor()` és `cursor_sample()` csak paramétereket tölt ki.
  - A `panel()` kikapcsolja a kurzort (menük, párbeszédablakok alatt).
- **Színsémák, CTRL+0** (`$92`) léptet: `themes[4][5]` a `main.c`-ben (lap, sávok, margó, keret, nyíl); `_next_theme` / `_set_theme` a `gfx.s`-ben. A `COL_PAGE` / `COL_BAR` / `COL_MARGIN` most a `col[]` tömbből olvas.
- A `cursor` és az `anchor` már nem static (a `gfx.s` olvassa őket).

## Memória
- A határ `__HIMEM__=$C000` és `__STACKSIZE__=$200`, a BSS vége legfeljebb `$BDFF` lehet.
- A változtatás után szabad: hu 37 bájt, fr ~110, dk ~190, en ~255 bájt.
- Ellenőrzés: `make` után `grep "^BSS" build/plume-*.map`.
- A cc65 C-kód nagy, az új funkciókat érdemes assemblyben írni.

## Következő nagy terv: REU, „csúszó ablak”
- A teljes dokumentum a REU-ban van, a `text[]` egy kb. 18–20 KB-os ablak körülötte, bekezdéshatáron vágva.
- Ablakváltás a háttérben, ha a kurzor az ablak széléhez közelít. Ideálisan csak az új részt tördeli újra.
- Mentés és betöltés folyamatosan, REU ↔ lemez; a fájlformátum változatlan. REU nélkül minden marad a régiben (20 000-es határ).
- A keresés és a nyomtatás végigmegy a teljes dokumentumon; a kijelölés egy ablakon belül marad.
- Teszt: VICE `-reu -reusize 512` (a VICE-ot nem sikerült működésre bírni; a C64 Ultimate-on teszteltünk, lásd `tools/reutest/README.md`).

## REU-terv: döntések és állás (2026-11-16)
A kódhely-kérdés eldőlt, a terv módosult (részletek és mérések: Claude memória `reu-build-decisions`):
- **Külön REU-build**, **Magic Desk** crt (`plume-xx-reu.crt`, `crt-md.s`, `mkcrt.py ... md`). Az EasyFlash I/O2 RAM-ja eltakarja a REU-t, ezért az EasyFlash-crt-k változatlanok (20 000, RTF nélkül).
- **Ablak 12 000 bájt**, a dokumentum max. 64 KB (PLM2 16 bites hossz). REU ≥ 256 KB kell.
- **Kész és C64U-n igazolt:** betöltés/mentés a REU-n át (A), csúszás előre/hátra részleges újratördeléssel (B). **Megírva, nincs tesztelve:** nyomtatás darabolása. **Hátra:** keresés a teljes dokumentumon, görgetősáv (jobb szél), RTF (File menü: „Open RTF...” új dokumentumként, „Save as RTF...”), 1 MHz-es csúszás-késleltetés mérése.
- A nem-REU buildek bájtra változatlanok (fr 110, en 255, dk 192, hu 37 szabad); minden módosítás után ellenőrizni.
- Minden **commitolatlan** a munkafában (a kurzor/szín-módosítások is).

## Állás (2026-11-16 este)
- Kész és C64U-n igazolt: keresés a teljes dokumentumon, nyomtatás darabolása, görgetősáv (REU-build), RTF olvasás/írás (File menü: „Open RTF...”, „Save as RTF...”).
- Az ablak **9000 bájt** lett (volt 12000), hogy az RTF elférjen. Szabad hely REU-buildekben: fr 580, en 760, dk 700, hu 540 B. A nem-REU buildek bájtra változatlanok.
- Tesztek: `tools/reutest/` (rtf_gen.py, rtf_test.py, rtf_wtest.py, find_test.py). Nincs tesztelve: valódi Word/LibreOffice RTF, magyar `\ansicpg1250`, a nyomtatóra írás.
- Nincs commit, README és `dist/` nincs frissítve.
