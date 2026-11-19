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
- **Külön REU-build**, **Magic Desk** crt (`plume-xx-2.2e.crt`, `crt-md.s`, `mkcrt.py ... md`). Az EasyFlash I/O2 RAM-ja eltakarja a REU-t, ezért az EasyFlash-crt-k változatlanok (20 000, RTF nélkül).
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
- **Élesítve a C64U-n:** `/SD/PLUME/plume-hu-2.2e.crt`; regény: `/SD/documents/semmi-egyben.d81` (1581, egy SEQ fájl "semmi", 246 KB). Drive A típusa 1581 kell hozzá; a kép `readwrite` módban csatolandó (a `readonly` miatt "write protect"). SD-n lévő képbe a mentés tartósan visszaíródik. A Kernal ROM = `JiffyDOS_C64.bin` futásidőben állítva — az F1 menüben flash-be kell menteni.
- **Hibák, javítva:** (1) `write_rtf` 16 bites `g/tot` → csak a hossz mod 65536 bájtot mentette (a regényből ~1500 sor); most `dpos`. (2) cc65 `?:` 8 bitre csonkít, ha az egyik ág `unsigned char` → a `\'85 \'91 \'92 \'97` (cp1252 0x80–0x9F) betöltése `&`/szóköz lett; most nincs ternáris. Igazolva: a 246 KB-os regény betöltés+RTF mentés után mind a 146 586 betű egyezik az eredetivel.
- **Új:** `disk_busy` makró (lemez I/O alatt sprite ki, JiffyDOS gyors marad; 25 B/build); `slide_back` bekezdésenként (`para_start(win_base - 1)`); `tools/c64u/Loadfromc64u.sh` / `Savetoc64u.sh` (kép le/fel FTP-vel + `c1541`); `MAXDOC_TEST_N` (teszt ablakméret, alap 4000; RTF-hookos teszthez 3000).
- **Szabad bájt (BSS vége → `$BE00`):** non-REU fr 86, en 231, dk 168, hu 13 (a hu build csak ~13 B!); REU fr 422, en 600, dk 537, hu 382.
- **Tesztelés tanulság:** a `filename` statikus, címe változik; a teszt-hookot (0x9E) ideiglenesen a névvel kell fordítani. Az `rtf_wtest.py`, `find_test.py`, `ptest.py`, `reuverify.py` még a régi `$03ED` állapotcímeket használja.
- **Nyitott:** valódi egérrel a végső görgetősáv; JiffyDOS-sebességű mentés/betöltés időmérése a végleges buildben; Word/LibreOffice RTF-ek, `\ansicpg1250`; nagy fájl betöltése (UCI `$DF1C` vagy REU-kép előtöltés — nem vizsgálva); dist/ nincs frissítve.

## Frissítés (2026-11-16 éjjel, később): fájlválasztó, kapacitás, RTF-név — KÉSZ, commitolva (c001531)
- **Fájlválasztó (REU-build):** `scan_dir(find)` (név + méret blokkban + szabad blokkok + a `find` nevű fájl régi mérete), `pick_file(title, newbtn)` egy oszlop, jobbra igazított blokkszám a név előtt, 13 fájl/oldal (`MAXFILES`, `fpage`, `fmore`), fel/le nyíl (`arrow_draw`, a jobb szélen) vagy kurzor fel/le lapoz; `ask_name(title, dest)` = lista + "New..." gomb. Az Open, Save as, Save as RTF ezt használja.
- **Kapacitás:** `fit_check(bytes, name)` — blokk = (bytes+253)/254. Ha belefér: `@0:` (biztonságos csere). Ha nem, de a régi azonos nevű fájl törlésével igen: megerősítés után `S0:` majd `0:`-val írás. Különben "Blocks needed / free". RTF: számláló előfutás (`rtf_dry`, `rtf_body` kétszer fut).
- **RTF-név:** `rtf_doc` jelző; `load_rtf` megtartja a nevet, F1 (`save_doc`) RTF dokumentumnál `write_rtf(filename)`-t hív; Save as RTF után a név a dokumentumé; Save as… és new_doc/load_named törli a jelzőt.
- **Menü/ablak:** menü: egy üres cellasor fent/lent (`panel(1, n + 2, ...)`, elemek a 2. sortól); a `panel()` alsó árnyéka a jobb árnyék szélességénél ér véget (a "farok" megszűnt).
- **Optimalizálás:** `ROWBASE` közös függvény (`rowbase()`), a `button()` egyszer számolja a sorcímet: ~290 B megtakarítás minden buildben. cc65 tanulság: a `dpos` eltolás (`>>`) drágább, mint a hosszú osztás; plusz helyi változó néha növeli a kódot — mérni kell.
- **Ablak 3300 B** (volt 4400, 3500): a fájlválasztó stb. ~1,4 KB kódot kért. Szabad bájt (BSS vége → `$BE00`): non-REU fr 331, en 476, dk 413, hu 258; REU fr 49, en 254, dk 191, hu 36 (!). A non-REU buildekbe a fájlválasztó/kapacitás most beférne (nincs megcsinálva).
- **Nyitott:** a Save as… (PLM) utáni PLM-mód átváltása nincs külön tesztelve a gépen; a 4,5 MB-os regresszió (`reu24_test.py`) nem futott újra a 3300 B-os ablakkal; a `PLM1` betöltő (~425 B) REU-ban elhagyható helynyerésre.
- **Munkarend:** a felhasználó engedélyezte, hogy a C64U-t a fejlesztés alatt újraindítsam tesztekhez. A tesztlemezek az SD-n `/SD/documents/` alatt jönnek létre és törlődnek; élesben `/SD/PLUME/plume-hu-reu-dir.crt` + `/SD/documents/semmi-egyben.d81`.

## Frissítés (2026-11-16 éjjel, még később): keresés-javítások, mentési napló elemzése
- **Javítva (a regényen, valódi 3300 B-os ablakkal igazolva, még nincs commitolva):** (1) `goto_global` eltolása 1536 -> 400 B: a `FILL` az ablakcsökkentés óta csak 1700, az "eltolás + egy bekezdés" nem fért bele, a találat az ablakon kívülre eshetett. A feltétel: eltolás + leghosszabb bekezdés (~1224 B) <= `FILL`. (2) `find_reu` ablakváltás után nem rajzolta újra a képernyőt, csak a kijelölt sorokat: vegyes régi/új sorok maradtak a képernyőn. Most `draw_text_area()` hívódik az ablakváltás után. Igazolva a regényen 4 távoli kereséssel (lehettek, amennyinek, magadhoz, megoldotta): a kijelölés helyes, a képernyő tiszta.
- **Mentési napló (NEV.BAK, tétlenség után ment): NEM került be.** Minden módosítás a `replace()`-en megy át (+ `apply_style`), ezért naplózható (rekord: win_base, pozíció, törölt hossz, beszúrt bájtok, a REU bloc-notes végén tárolva; kiírás ~5 s tétlenség után; visszaállítás: alapfájl + lejátszás `gap_to(pos)` + `suf_len -= rem` + beszúrás a prefixbe, ablak nélkül). **Méret: ~3,1 KB kód C-ben** (jlog ~700, recover ~1700, jflush ~350, bakcmd ~150), a REU build szabad helye 36-254 B (+~425 B ha a PLM1 olvasó kikerül), nincs szabad RAM ($CC00-$CFFF a videómátrix). Egyszerűbb terv (módosított tartomány: P = első módosítás az elejétől, S = utolsó a végétől, a .bak = fejléc + P + S + a köztes rész) is ~1,3-1,8 KB. Beférne overlay-jel, assembly újraírással, vagy más funkció elhagyásával. A kód nincs a repóban.
- **cc65 méretmérések:** `unsigned long` (dpos) műveletek és `reu_move(..., dpos, ...)` hívások nagyon drágák (~20 B/hívás); a `dpos >>` drágább a `/`-nél. Méretmérés: `cl65 -c ...; od65 --dump-segsize`, függvények kivételével összehasonlítva.

## Frissítés (2026-11-16 éjjel, vége): görgetősáv-ugrás a dokumentum végére
- **Hiba (a felhasználó jelezte: "a file legvége nem töltődött be"):** a regény vége be volt töltve, de a görgetősáv aljára ugrás után az ablak csak `FILL` (1650-1700 B) volt, és a `slide_fwd` nem toldotta meg (a kurzor túl közel a tetejéhez: nem dob el fejet; a `pull_tail` sem tölt, mert az ablak már FILL), így az utolsó ~450 karakter az ablakon kívül maradt, a képernyő vége üres volt.
- **Javítás:** `relocate()` ugrás után `pull_to(MAXDOC - ROOM - 100)`-ig tölt (`pull_tail()` = `pull_to(FILL)`); `sbar_goto`: előbb `maybe_slide()`, utána a "dokumentum vége" igazítás és az újrarajzolás; `find_reu` is újrarajzol. **Ablak 3250 B** (a plusz kód miatt). Szabad (BSS): hu REU 25 B (!), fr 38, en 243, dk 180.
- **Ismert hiányosság:** aljára ugrás után az utolsó 2-3 sor (üres sorok + az utolsó szövegsor) még a státuszsor alatt marad; egy kurzor-le billentyű megmutatja. Az igazítás (`top = nlines - VISIBLE`) a becsült/rövid `nlines` miatt nem pontos. A felhasználó így jónak találta ("nem kell babrálni").

## Frissítés (2026-11-16 éjjel, vége 2): megakadó görgetés — javítva
- **Hiba (a felhasználó jelezte):** kurzor fel/le (és egérrel kijelölés lehúzása) közben a görgetés egy idő után megakadt (pl. a 2902. sornál), a görgetősávval át lehetett lépni. Ok: az ablakcsökkentés után az ablak csak ~35 sor, a `slide_fwd` (`L + 25 > cl`) és a `slide_back` (`T < cl + 25`) feltételei fix 25-30 sort kértek, ami nem teljesülhetett.
- **Javítás:** a csúszás a képernyő tetejéhez/aljához igazodik: `maybe_slide`: előre ha `top + VISIBLE + 8 > nlines`, vissza ha `top < 6` (kényszerített eset: `top >= 12` -> előre, különben vissza); `slide_fwd` legfeljebb `top - 6` sort dob el (a képernyő sorait nem érinti); `slide_back` a farkat csak a képernyő alatt vágja (`T >= top + VISIBLE + 2`). `FILL` = `MAXDOC - ROOM - 200` (2250 B). `EDGE` megszűnt. A relocate újra sima `pull_tail()`.
- **PLM1 olvasó kikerült a REU buildekből** (a sima buildek olvassák); szabad hely: hu REU 576 B, fr ~590, en ~790, dk ~730 B. Igazolva a regényen: 400 sor le + 400 sor fel megszakítás nélkül, távoli keresés, aljára ugrás.

## Frissítés (2026-11-18): Plume 2.2e — egérgörgő, ledek, billentyűhang
- **Verzió:** a REU buildek „Plume 2.2e” (enhanced); a crt-k neve `build/plume-XX-2.2e.crt`. A non-REU maradt 2.2.
- **Egérgörgő (C64U, Mouse + Wheel):** a firmware a kereket a port 1-en bal/jobb (0x04/0x08) impulzusként küldi, 0x1F zajjal; egy kattanás annyi impulzus, amennyi a *Mouse Wheel Sensitivity* (1–16, alap 8). A Plume indításkor 1-re állítja. Egy impulzus = 1 sor, gyors forgatásnál 2/4/8 (a kattanások közti idő alapján: 3/6/12 jiffy). Az impulzusok a billentyűmátrixban is látszanak: görgetés után `POKE(198,0)` + `joy_p1_time` (különben 1 MHz-en hamis betűk, É).
- **Ledek a témához (CTRL+0, és indításkor az alap témához):** a Programmatic LED mód C64-oldali interfésze nem publikus (changelog, doksi, kézikönyv: nincs). Helyette az **UCI HTTP target ($06, 3.15 / C64U 1.2)** a gép saját REST API-ját hívja (`PUT /v1/configs/<kat>/<elem>?value=`), 127.0.0.1-gyel és a saját IP-vel is megy. A kód (`src/led.s`) a Magic Desk crt **62. bankjában** van; `led_theme()` (`src/reu.s`) egy kis betöltővel $0334-re másol, az $0400–$07FF-re másolja és ugrik. A betöltő nem lehet a $8000 felett (ROML elfedi a RAM-ot), és nem kerülhet a STARTUP szegmensbe ($080D = belépési pont). UCI nélkül (`$DF1D != $C9`) azonnal visszatér. Témák: 1 fehér/ki, 2 ki/zöld, 3 Royal Blue/Azure, 4 ki/Amber (ház/billentyűzet). Egy hívás kb. 1 s.
- **Billentyűhang:** CTRL+9 kapcsolja (SID zaj, hu REU).
- **Nem tesztelt:** nem-Ultimate gép, Ultimate-II+ és nem Starlight C64U (a LED/Mouse kategóriák ott hiányozhatnak; a hiba ártalmatlan).


## Frissítés (2026-11-19): egységes Open

- A File menü „Open…” sora a fájl első 5 bájtja alapján felismeri az RTF-et és a saját (PLM) formátumot; az „Open RTF…” sor megszűnt. F1 abban a formátumban ment, amelyben a fájl van; „Save as…” saját formátumban, „Save as RTF…” RTF-ben.
- A nyomtatási puffer (`outbuf`) a $0540-es képernyő-RAM-ba költözött, a verem 0x180 (REU-buildek hely miatt). Recovery pont: tag `recovery-2.2e-pre-overlay`.

## Frissítés (2026-11-19): overlayek, gyorsmentés, dupla/tripla kattintás

- **Overlayek:** `plume-reu.cfg` (c64.cfg + közös RAM-terület `__OVERLAYSTART__`, 4 KB). Az 1. overlay az RTF, a 2. a nyomtatás, a 3. a gyorsmentés; a cartridge 49., 50., 51. bankjában, `ovl_load(n)` (src/reu.s) másolja a közös területre (a $0334-es betöltőn át, ROML csak aktív cartridge mellett látszik). Overlayből másik overlayt betölteni TILOS (felülírná magát); a belépési pontok (rtf_open, rtf_save, rtf_write, quick_save, bck_saved, bck_loaded, print_run) rezidens burkolók. Ha az overlay-n belüli kód rezidens függvényt hív, az sem tölthet be másik overlayt.
- **Gyorsmentés:** a `replace()` és az `apply_style()` naplózza a szerkesztést (`jr_log`) a REU utolsó bankjába (suf_end + $1000-től, 61000 bájtig). F1 = `NÉV.bck` (PBK1 + alaphossz + aláírás + naplóhossz + napló); a "Save" a teljes mentés és törli a .bck-t; betöltéskor `bck_loaded_` rejátssza. Lemezhely: `fit_check` + `free_space` (törlés vagy lemezcsere felkínálása).
- **Mérési tanulság:** lemezművelet közben (különösen nagy fájl törlésekor) NE olvasd a C64U memóriáját a REST-en át: a DMA elrontja az IEC-et, a gép lefagy / szemetet mutat.
- **Dupla/tripla kattintás:** `select_unit` (REU), 20 képkockás időablak (`click_hold`).
