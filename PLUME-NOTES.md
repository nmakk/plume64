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

## Frissítés (2026-11-16 éjjel): élesben, RTF-javítások — KÉSZ, commitolva
- **Élesítve a C64U-n:** `/SD/PLUME/plume-hu-reu.crt`; regény: `/SD/documents/semmi-egyben.d81` (1581, egy SEQ fájl "semmi", 246 KB). Drive A típusa 1581 kell hozzá; a kép `readwrite` módban csatolandó (a `readonly` miatt "write protect"). SD-n lévő képbe a mentés tartósan visszaíródik. A Kernal ROM = `JiffyDOS_C64.bin` futásidőben állítva — az F1 menüben flash-be kell menteni.
- **Hibák, javítva:** (1) `write_rtf` 16 bites `g/tot` → csak a hossz mod 65536 bájtot mentette (a regényből ~1500 sor); most `dpos`. (2) cc65 `?:` 8 bitre csonkít, ha az egyik ág `unsigned char` → a `\'85 \'91 \'92 \'97` (cp1252 0x80–0x9F) betöltése `&`/szóköz lett; most nincs ternáris. Igazolva: a 246 KB-os regény betöltés+RTF mentés után mind a 146 586 betű egyezik az eredetivel.
- **Új:** `disk_busy` makró (lemez I/O alatt sprite ki, JiffyDOS gyors marad; 25 B/build); `slide_back` bekezdésenként (`para_start(win_base - 1)`); `tools/c64u/Loadfromc64u.sh` / `Savetoc64u.sh` (kép le/fel FTP-vel + `c1541`); `MAXDOC_TEST_N` (teszt ablakméret, alap 4000; RTF-hookos teszthez 3000).
- **Szabad bájt (BSS vége → `$BE00`):** non-REU fr 86, en 231, dk 168, hu 13 (a hu build csak ~13 B!); REU fr 422, en 600, dk 537, hu 382.
- **Tesztelés tanulság:** a `filename` statikus, címe változik; a teszt-hookot (0x9E) ideiglenesen a névvel kell fordítani. Az `rtf_wtest.py`, `find_test.py`, `ptest.py`, `reuverify.py` még a régi `$03ED` állapotcímeket használja.
- **Nyitott:** valódi egérrel a végső görgetősáv; JiffyDOS-sebességű mentés/betöltés időmérése a végleges buildben; Word/LibreOffice RTF-ek, `\ansicpg1250`; nagy fájl betöltése (UCI `$DF1C` vagy REU-kép előtöltés — nem vizsgálva); dist/ nincs frissítve.
