# Plume

**Un traitement de texte graphique pour Commodore 64, écrit de zéro et libre.** Souris ou joystick, menus, gras, italique, souligné, accents français, et impression sur une Commodore MPS-803, le tout sur une cartouche qui démarre directement.

*English version below.*

| Édition française | Menu Édition |
|---|---|
| ![Plume en français](docs/images/plume-fr.png) | ![Menu Édition](docs/images/plume-fr-menu.png) |

## Fonctions

- **Écran graphique** façon GEOS : police proportionnelle, texte affiché tel qu'il sera imprimé.
- **Souris 1351** sur le port 1 (une souris USB sur le C64 Ultimate) **ou joystick** sur le port 1 ou 2, menus déroulants, boîtes de dialogue.
- **Gras, italique, souligné**, à la frappe ou sur une sélection.
- **Accents français**, æ ø å danois et á é í ó ö ő ú ü ű hongrois, à l'écran, dans le texte et sur papier.
- **Sélection à la souris, couper, copier, coller, rechercher.**
- **Documents jusqu'à 20 000 caractères**, enregistrés sur le lecteur 8.
- **Impression sur Commodore MPS-803 / MPS-801** (périphérique 4), marges A4 de 3 cm :
  - *Graphique* : exactement ce qui est à l'écran, accents et styles compris ;
  - *Texte* : les caractères de l'imprimante, plus rapide, sans accents ;
  - *Texte large* : double largeur.
- **Le papier s'arrête après la dernière ligne** pour pouvoir déchirer un court message.

## Utilisation

1. Copiez [`dist/plume-fr.crt`](dist/plume-fr.crt) (ou [`plume-en.crt`](dist/plume-en.crt)) sur votre carte SD ou votre cartouche flash.
2. Montez une disquette ou un `.d64` sur le lecteur 8 pour vos documents. [`dist/plume.d64`](dist/plume.d64) est un disque vierge.

   Édition REU (Plume 2.2e, cartouche Magic Desk, REU requis) : [`dist/plume-fr-2.2e.crt`](dist/plume-fr-2.2e.crt), [`en`](dist/plume-en-2.2e.crt), [`dk`](dist/plume-dk-2.2e.crt), [`hu`](dist/plume-hu-2.2e.crt) (aussi dans la [release v2.2e](https://github.com/nmakk/plume64/releases/tag/v2.2e)).
3. Lancez la cartouche.

### Clavier

| Touches | Effet |
|---|---|
| `C=` + `E` / `R` / `T` | é / è / ê |
| `C=` + `A` / `Q` | à / â |
| `C=` + `C` | ç |
| `C=` + `U` / `Y` | ù / û |
| `C=` + `O` / `I` / `K` / `L` | ô / î / ï / ë |
| `C=` + `P` | œ |
| `C=` + `-` | `_` (tiret bas) |
| `£` | € |
| `F1` | Enregistrer (éditions REU : sauvegarde rapide) |
| `F3` / `F5` / `F7` | Gras / italique / souligné |
| `F2` / `F4` / `F6` | Copier / coller / couper |
| `F8` | Rechercher le suivant |
| `RUN/STOP` | Annuler une sélection ou une boîte de dialogue |

### Édition danoise

`dist/plume-dk.crt` a les menus en anglais et un clavier danois :

| Touches | Effet |
|---|---|
| `:` / `;` / `@` | æ / ø / å |
| `Shift` + `:` / `;` / `@` | Æ / Ø / Å |
| `Shift` + `.` / `,` | `:` / `;` |
| `C=` + `@` | `@` |

Dans cette édition, `<`, `>`, `[` et `]` ne sont plus disponibles. Toutes les éditions affichent et impriment æ ø å Æ Ø Å ; en mode Texte, l'imprimante les remplace par a, o, a.

### Édition hongroise

L'édition hongroise est l'œuvre de **mkckOner**, qui a dessiné les lettres et écrit le clavier. `dist/plume-hu.crt` a les menus en anglais et un clavier hongrois : la couche `C=` reprend les positions de `AltGr` de la disposition *Hungarian Programmers* (`hu_prog`), sur une base QWERTY.

| Touches | Effet |
|---|---|
| `C=` + `E` / `Y` / `U` / `I` | é / ü / ú / í |
| `C=` + `O` / `P` / `Q` / `W` | ó / ö / ő / ű |
| `C=` + `A` | á |
| `Shift` + `C=` + la même touche | É Ü Ú Í Ó Ö Ő Ű Á |
| `C=` + `-` | `_` |

Sous `C=`, Plume lit la matrice du clavier lui-même : le KERNAL ne sait pas faire `Shift` + `C=`. Toutes les éditions affichent et impriment ces lettres ; en mode Texte, l'imprimante écrit la lettre sans accent (ő donne o).

### Version REU (documents jusqu'à 16 Mo, RTF)

`make` produit aussi `build/plume-fr-2.2e.crt`, `plume-en-2.2e.crt`, `plume-dk-2.2e.crt` et `plume-hu-2.2e.crt` (cartouche Magic Desk). Elles demandent une extension REU d'au moins 256 Ko (sur un C64 Ultimate : REU 16 Mo activée) et gardent tout le document dans le REU : jusqu'à 16 Mo, au lieu de 20 000 caractères. Le texte est affiché par une fenêtre glissante de 3 250 octets.

- **Ouvrir** : *File → Open…* reconnaît seul un fichier RTF (Word, LibreOffice : gras, italique, souligné, accents cp1252 et `\uN?`) ou un fichier Plume. Un document de 250 Ko (un roman) tient sur une disquette `.d81`, pas sur un `.d64`.
- **Enregistrer** : *File → Save* écrit tout le document dans le format du fichier (RTF s'il vient d'un RTF, sinon Plume), *Save as…* au format Plume, *Save as RTF…* en RTF. Les documents de plus de 64 Ko sont enregistrés au format PLM3 (les anciens PLM1/PLM2 restent lisibles ; les éditions sans REU ne lisent pas PLM3).
- **Sélection et presse-papiers sur tout le document** : *Select all* sélectionne vraiment tout ; le presse-papiers est dans le REU (jusqu'à 256 Ko) ; couper, copier, coller et supprimer marchent sur de très grandes sélections, et chacun est un seul pas d'annulation. Au-delà de la capacité d'annulation (256 Ko), la suppression demande une confirmation et n'est plus annulable ; le style (gras…) ne s'applique qu'aux sélections qui tiennent dans la fenêtre.
- **Barre de progression** dans la ligne d'état pendant le chargement et l'enregistrement (fichiers Plume, RTF, sauvegarde rapide).
- **Barre de défilement** : un clic au-dessus ou au-dessous du curseur de la barre avance d'une page ; maintenu, il répète jusqu'à ce que le curseur rejoigne la souris. Le curseur de la barre se tire toujours où l'on veut. Après un saut, le numéro de ligne est une estimation (`~`) ; il redevient exact au début du document.
- **Menus** : *File → Revert to file / Restore .bck* (rechargement, voir `RUN/STOP`+`RESTORE`), *Style → Colors* (écran et leds), *? → Key click* et *Overwrite* (avec `*` quand actifs).
- **Annuler / refaire** (`CTRL`+`Z` / `CTRL`+`Y`, `RESTORE` / `Shift`+`RESTORE`, ou *Edit → Undo / Redo*). `RUN/STOP`+`RESTORE` recharge le fichier du disque sans son `.bck` (le `.bck` reste sur le disque) ; `Shift`+`RUN/STOP`+`RESTORE` recharge le fichier et rejoue son `.bck`. Annuler : défait les modifications pas à pas, jusqu'au début de la session, même après un enregistrement. Chaque lettre tapée est un pas. Les modifications restituées par un `.bck` s'annulent aussi. Le journal et les textes supprimés sont gardés dans le REU : 256 Ko de journal, 256 Ko de textes supprimés, 40 000 enregistrements et 4096 pas de « refaire » (REU de 2 Mo ou plus ; sinon un huitième). Si le journal est plein, l'historique repart de zéro. Le fichier `.bck` a maintenant le format `PBK3` ; les `.bck` `PBK2` sont encore lus (par la longueur). Si un `.bck` ne correspond pas au fichier, Plume demande s'il faut l'appliquer quand même : rien n'est perdu en silence.
- **Sauvegarde rapide** (`F1`, *File → Quick save*) : n'écrit que les modifications faites depuis le dernier enregistrement complet, dans `NOM.bck`, à côté du fichier. Plume tient ce journal dans le REU. Un enregistrement complet (*File → Save*) efface le `.bck`. À l'ouverture, un `.bck` qui correspond au fichier est rejoué automatiquement (« Quick save restored »). Un `.bck` qui ne correspond pas (fichier modifié ailleurs) est ignoré avec un message. Sans fichier de base (document nouveau), `F1` fait un enregistrement complet.
- **Disque plein** : avant d'enregistrer, Plume calcule les blocs nécessaires. Si la copie ne tient pas à côté de l'ancienne mais tient à sa place, il propose d'effacer d'abord l'ancien fichier. Sinon il indique les blocs nécessaires et libres, puis propose d'effacer un autre fichier (avec confirmation) ou de changer de disque (*Cancel* dans la liste) et recompte : on ne peut pas se retrouver sans pouvoir enregistrer.
- **Liste des fichiers** (Open, Save as, Save as RTF) : un fichier par ligne, précédé de sa taille en blocs, avec les blocs libres ; 13 fichiers par page, des flèches haut/bas (ou les touches curseur) tournent les pages. *New…* permet de saisir un nouveau nom, choisir un nom de la liste l'écrase.
- **Souris** : double clic = sélectionne le mot, triple clic = la phrase. La molette (C64 Ultimate, Mouse Mode *Mouse + Wheel* ; Plume règle *Mouse Wheel Sensitivity* sur 1 au démarrage) fait défiler avec accélération.
- **Barre de défilement** à droite, recherche dans tout le document, impression par morceaux.
- **Couleurs** : `CTRL` + `←` passe au jeu de couleurs suivant (sur un C64 Ultimate, les LED du clavier et du boîtier suivent). `CTRL` + `9` active ou coupe le clic de touche.
- **Disque** : pendant les accès disque Plume masque les sprites, sinon JiffyDOS retombe en transfert série lent (environ 3 Ko/s en lecture avec JiffyDOS). Ne lisez pas la mémoire du C64 Ultimate par l'API REST pendant un accès disque.
- Depuis un Mac : [`tools/c64u/Loadfromc64u.sh`](tools/c64u/Loadfromc64u.sh) et [`Savetoc64u.sh`](tools/c64u/Savetoc64u.sh) téléchargent et déposent des fichiers dans une image disque sur un C64 Ultimate (FTP + `c1541`).
- Limites : `\ansicpg1250` (ő ű en `\'f5`) n'est pas traité ; charger un très gros fichier par l'IEC est très lent ; pas de couleurs de texte RTF.
- Technique : le code RTF, l'impression et la sauvegarde rapide sont des *overlays* rangés dans des banques de la cartouche (`plume-reu.cfg`, `ovl_load` dans `src/reu.s`) et copiés dans une zone commune de la RAM quand on en a besoin ; le code des LED est dans la banque 62 (`src/led.s`, cible HTTP de l'UCI, firmware 3.15 / C64U 1.2 ou plus).

#### Clavier HUN (édition hongroise REU)

`plume-hu-2.2e.crt` démarre avec le clavier **HUN** : la disposition hongroise sur les touches du C64 (les lettres restent en QWERTY). **`C=` + `Espace`** (ou *? → HUN / C64 keys*) bascule vers le clavier **C64** (la couche `C=` décrite plus haut) et inversement ; la barre d'état affiche `HUN` ou `C64`.

| Touche (légende C64) | Normal | `Shift` |
|---|---|---|
| `←` | 0 | ? |
| `1` … `9` | 1 … 9 | `'` `"` `+` `!` `%` `/` `=` `(` `)` |
| `0` (Ö) | ö | Ö |
| `+` (Ü) | ü | Ü |
| `-` (Ó) | ó | Ó |
| `↑` (Í) | í | Í |
| `@` (Ő) | ő | Ő |
| `*` (Ú) | ú | Ú |
| `:` (É) | é | É |
| `;` (Á) | á | Á |
| `=` (Ű) | ű | Ű |
| `£` | Tab (4 espaces) | `-` |
| `,` `.` `/` | `,` `.` `/` | `<` `>` `?` |
| `CLR/HOME` | début de ligne ; déjà au début : fin de ligne | suppression vers l'avant (répétée comme `INST/DEL`) |
| `INST/DEL` | suppression arrière | bascule insertion / écrasement |

Sous `C=`, chaque touche donne le signe d'origine du C64 : `C=` + `1` … `9` donne `!` `"` `#` `$` `%` `&` `'` `(` `)`, `C=` + `Ü` `Ó` `Ű` donne `+` `-` `=`, `C=` + `Ö` donne `0`, `C=` + `↑` donne `^`, `C=` + `£` donne `£`, `C=` + `←` donne la flèche `←` du C64, `C=` + `É` `Á` `Ú` `Ő` donne `:` `;` `*` `@`. `C=` + `Shift` + `É` / `Á` donne `[` / `]`, et **`C=` + `Shift` + `-` donne `_`**. `RUN/STOP` + `RESTORE` ne relance plus le BASIC. `CTRL` reste à sa place, `RUN/STOP` est Échap. La barre d'état montre `I` (insertion) ou `O` (écrasement). Le même `Shift` + `CLR/HOME` fait la suppression vers l'avant aussi sur le clavier C64.

### Joystick

Sans souris, par exemple sur un C64 d'origine avec une Kung Fu Flash, un joystick sur le port 2 (ou le port 1) déplace le pointeur. Il avance d'un pixel par image pour viser une lettre, puis accélère si l'on garde le manche poussé. Le bouton de tir sert de clic. Gardez-le enfoncé en bougeant le manche pour sélectionner du texte. Le port 2 est conseillé : le port 1 partage ses fils avec le clavier, donc Plume ignore le clavier quand le joystick du port 1 bouge.

## Matériel testé

| Configuration | État |
|---|---|
| Commodore 64 Ultimate, firmware 1.1.0, à 1 MHz et à 64 MHz | Testé : édition, fichiers, impression sur une vraie MPS-803 |
| VICE 3.10 (EasyFlash, 1541, MPS-803 émulés) | Testé, sert de banc d'essai |
| C64 d'origine avec Kung Fu Flash 2, joysticks | Testé : la cartouche démarre, le joystick pilote le pointeur (version 2.0) |
| Édition danoise (2.1) | Testée dans VICE : clavier danois, æ ø å Æ Ø Å à l'écran ; pas encore sur un vrai C64 danois |
| Édition hongroise (2.2) | Testée dans VICE 3.7.1 et 3.10 : les 18 lettres á é í ó ö ő ú ü ű et leurs majuscules à l'écran et sur la MPS-803 émulée ; pas encore essayée sur un vrai C64 par des utilisateurs hongrois |

## Plume en ligne : écrire sur l'ordinateur, imprimer sur le C64

La page [labbej.eu/plume](https://labbej.eu/plume) est un éditeur de texte en ligne pour Plume. On écrit avec gras, italique et souligné (ou on colle un texte depuis Word, ou on ouvre un fichier `.txt`, Markdown ou `.docx`), puis on télécharge une disquette `.d64` que Plume ouvre, prête à imprimer sur la MPS-803. Accents français, lettres danoises æ ø å et hongroises á í ó ő ú ű sont conservés. Tout se passe dans le navigateur : le texte n'est envoyé nulle part.

## Compiler

Outils : `make`, `python3`, [cc65](https://github.com/cc65/cc65) et [VICE](https://vice-emu.sourceforge.io) (pour `c1541`). Sur macOS :

```sh
brew install cc65 vice
make
```

Les cartouches sont dans `build/plume-fr.crt`, `build/plume-en.crt`, `build/plume-dk.crt` et `build/plume-hu.crt`.

## Comment c'est fait

- **Pointeur** : le pilote 1351 de cc65 pour la souris, plus une lecture directe des deux ports joystick ([`src/gfx.s`](src/gfx.s), `joy_read`).
- **Langages** : C (cc65) pour la logique, assembleur 6502 pour le dessin des caractères, la mise en page et le défilement.
- **Styles** : de petits marqueurs invisibles dans le texte, uniquement là où le style change. Un caractère coûte un octet, d'où les 20 000 caractères.
- **Mémoire** : écran graphique caché sous la ROM, mise en page et presse-papiers rangés sous les circuits d'entrée-sortie. Les 64 Ko sont utilisés au plus près.
- **Police** : [unscii](http://viznut.fi/unscii/) de viznut, dans le domaine public, rendue proportionnelle, accents redessinés.
- **Tests** : [`tools/c64plume.py`](tools/c64plume.py) pilote Plume sur un C64 Ultimate par son API réseau (frappes, clics, relecture du document, capture d'écran) grâce à une boîte aux lettres en `$02A7`.

## Licence

[MIT](LICENSE) (composants tiers : [NOTICE](NOTICE)) : vous pouvez utiliser, modifier et redistribuer Plume librement, en gardant la mention du copyright. La police unscii est dans le domaine public ; la bibliothèque d'exécution de cc65 incluse dans le programme est sous licence zlib.

---

# Plume (English)

**A graphical word processor for the Commodore 64, written from scratch and open source.** Mouse or joystick, menus, bold, italic, underline, accented characters and printing on a Commodore MPS-803, all on a cartridge that boots straight into the editor.

| English edition | Edit menu |
|---|---|
| ![Plume in English](docs/images/plume-en.png) | ![Edit menu](docs/images/plume-en-menu.png) |

## Features

- **Graphical screen** in the spirit of GEOS: proportional font, what you see is what you print.
- **1351 mouse** on port 1 (a USB mouse on the C64 Ultimate) **or joystick** on port 1 or 2, pull-down menus, dialog boxes.
- **Bold, italic, underline**, while typing or on a selection.
- **Accented characters** (French, Danish æ ø å, Hungarian á é í ó ö ő ú ü ű) on screen, in the text and on paper.
- **Mouse selection, cut, copy, paste, find.**
- **Documents up to 20,000 characters**, saved on drive 8.
- **Printing on a Commodore MPS-803 / MPS-801** (device 4), with 3 cm margins on A4:
  - *Graphics*: exactly what is on screen, accents and styles included;
  - *Text*: the printer's own characters, faster, without accents;
  - *Wide text*: double width.
- **The paper stops after the last line**, so short notes can be torn off.

## Usage

1. Copy [`dist/plume-en.crt`](dist/plume-en.crt) (or [`plume-fr.crt`](dist/plume-fr.crt)) to your SD card or flash cartridge.
2. Put a disk or a `.d64` in drive 8 for your documents. [`dist/plume.d64`](dist/plume.d64) is an empty one.

   REU edition (Plume 2.2e, Magic Desk cartridge, REU required): [`dist/plume-fr-2.2e.crt`](dist/plume-fr-2.2e.crt), [`en`](dist/plume-en-2.2e.crt), [`dk`](dist/plume-dk-2.2e.crt), [`hu`](dist/plume-hu-2.2e.crt) (also in the [v2.2e release](https://github.com/nmakk/plume64/releases/tag/v2.2e)).
3. Start the cartridge.

**Danish edition** (`dist/plume-dk.crt`, English menus): `:` `;` `@` type æ ø å, and with `Shift` Æ Ø Å; `Shift` + `.` and `Shift` + `,` give `:` and `;`; `C=` + `@` gives `@`. `<`, `>`, `[` and `]` are not available in this edition. Every edition displays and prints æ ø å Æ Ø Å (the printer's Text mode prints them as a, o, a).

**Hungarian edition** (`dist/plume-hu.crt`, English menus): the `C=` layer follows the `AltGr` positions of the Hungarian Programmers layout (`hu_prog`) on a QWERTY base: `C=`+`E`/`Y`/`U`/`I` give é/ü/ú/í, `C=`+`O`/`P`/`Q`/`W` give ó/ö/ő/ű, `C=`+`A` gives á. Hold `Shift` as well for É Ü Ú Í Ó Ö Ő Ű Á. The Hungarian edition was created by **mkckOner**, who drew the letters and wrote the keyboard code.

**REU edition (documents up to 16 MB, RTF)**: `make` also builds `build/plume-fr-2.2e.crt`, `plume-en-2.2e.crt`, `plume-dk-2.2e.crt` and `plume-hu-2.2e.crt` (Magic Desk cartridge). They need a REU of at least 256 KB (on a C64 Ultimate: enable the 16 MB REU) and keep the whole document in the REU: up to 16 MB instead of 20,000 characters, shown through a sliding 3,250-byte window.

- **Open**: *File → Open…* recognises an RTF file (Word, LibreOffice: bold, italic, underline, cp1252 accents and `\uN?`) or a Plume file by itself. A 250 KB document (a novel) fits a `.d81` disk, not a `.d64`.
- **Save**: *File → Save* writes the whole document in the file's own format (RTF if it came from an RTF file, otherwise Plume), *Save as…* writes the Plume format, *Save as RTF…* writes RTF. Documents over 64 KB are saved as PLM3 (older PLM1/PLM2 stay readable; non-REU editions cannot read PLM3).
- **Selection and clipboard over the whole document**: *Select all* really selects everything; the clipboard lives in the REU (up to 256 KB); cut, copy, paste and delete work on very large selections, and each is a single undo step. Beyond the undo capacity (256 KB) a delete asks for confirmation and can no longer be undone; styles (bold…) only apply to selections that fit in the window.
- **Progress bar** in the status line while loading and saving (Plume files, RTF, quick save).
- **Scrollbar**: clicking above or below the thumb moves one page; held down it repeats until the thumb reaches the mouse. The thumb can still be dragged anywhere. After a jump the line number is an estimate (`~`); it is exact again at the start of the document.
- **Menus**: *File → Revert to file / Restore .bck* (reload, see `RUN/STOP`+`RESTORE`), *Style → Colors* (screen and LEDs), *? → Key click* and *Overwrite* (marked with `*` when on).
- **Undo / Redo** (`CTRL`+`Z` / `CTRL`+`Y`, `RESTORE` / `Shift`+`RESTORE`, or *Edit → Undo / Redo*). `RUN/STOP`+`RESTORE` reloads the file from disk without its `.bck` (the `.bck` stays on disk); `Shift`+`RUN/STOP`+`RESTORE` reloads the file and replays its `.bck`. Undo: undoes edits step by step, back to the start of the session, even across saves. Each typed letter is one step. Edits restored from a `.bck` can be undone too. The journal and the deleted text are kept in the REU: 256 KB of journal, 256 KB of deleted text, 40,000 records and 4096 redo steps (REU of 2 MB or more; otherwise one eighth). When the journal is full the history starts over. The `.bck` file now uses the `PBK3` format; `PBK2` files are still read (by length). If a `.bck` does not match its file, Plume asks whether to apply it anyway, so nothing is lost silently.
- **Quick save** (`F1`, *File → Quick save*): writes only the changes made since the last full save to `NAME.bck` next to the file. Plume keeps this journal in the REU. A full save (*File → Save*) deletes the `.bck`. When you open a file, a matching `.bck` is replayed on it automatically ("Quick save restored"); a `.bck` that does not match (the file was changed elsewhere) is ignored with a message. A new document without a file does a full save on `F1`.
- **Disk full**: before saving, Plume checks that the file fits. If it fits only in place of the old file of the same name, it offers to delete the old one first. Otherwise it shows the blocks needed and free, then lets you delete another file (with confirmation) or swap the disk (*Cancel* in the list) and counts again, so saving can never become impossible.
- **File lists** (Open, Save as, Save as RTF): one file per line with its size in blocks and the blocks free, 13 per page, with up/down arrows (or the cursor up/down keys) to turn pages. *New…* lets you type a new name; picking a listed name overwrites it.
- **Mouse**: double click selects a word, triple click a sentence. The mouse wheel (C64 Ultimate, Mouse Mode *Mouse + Wheel*; at startup Plume sets *Mouse Wheel Sensitivity* to 1) scrolls with acceleration.
- Also a scrollbar, search over the whole document and chunked printing.
- **Colours**: `CTRL` + `←` switches to the next colour theme (on a C64 Ultimate the keyboard and case LEDs follow it). `CTRL` + `9` switches the key-click sound on or off.
- **Disk access**: Plume hides the sprites during disk access, otherwise JiffyDOS falls back to slow serial transfer (about 3 KB/s reading with JiffyDOS). Do not read the C64 Ultimate's memory over the REST API during disk access.
- From a Mac, [`tools/c64u/Loadfromc64u.sh`](tools/c64u/Loadfromc64u.sh) and [`Savetoc64u.sh`](tools/c64u/Savetoc64u.sh) fetch and store files in a disk image on a C64 Ultimate (FTP + `c1541`).
- Not handled: `\ansicpg1250` (ő ű as `\'f5`); RTF text colours; loading a very large file over IEC is very slow.
- Technical: the RTF code, printing and quick save are *overlays* stored in cartridge banks (`plume-reu.cfg`, `ovl_load` in `src/reu.s`) and copied into a shared RAM area when needed; the LED code lives in bank 62 (`src/led.s`, UCI HTTP target, firmware 3.15 / C64U 1.2 or later).

**HUN keyboard (Hungarian REU edition)**: `plume-hu-2.2e.crt` starts with the **HUN** keyboard: the Hungarian layout on the C64 keys (letters stay QWERTY). **`C=` + `Space`** (or *? → HUN / C64 keys*) switches to the **C64** keyboard (the `C=` layer described above) and back; the status line shows `HUN` or `C64`.

| Key (C64 legend) | Plain | `Shift` |
|---|---|---|
| `←` | 0 | ? |
| `1` … `9` | 1 … 9 | `'` `"` `+` `!` `%` `/` `=` `(` `)` |
| `0` (Ö) | ö | Ö |
| `+` (Ü) | ü | Ü |
| `-` (Ó) | ó | Ó |
| `↑` (Í) | í | Í |
| `@` (Ő) | ő | Ő |
| `*` (Ú) | ú | Ú |
| `:` (É) | é | É |
| `;` (Á) | á | Á |
| `=` (Ű) | ű | Ű |
| `£` | Tab (4 spaces) | `-` |
| `,` `.` `/` | `,` `.` `/` | `<` `>` `?` |
| `CLR/HOME` | start of line; already there: end of line | delete forward (repeats like `INST/DEL`) |
| `INST/DEL` | delete backwards | toggle insert / overwrite |

Under `C=`, each key gives its original C64 symbol: `C=` + `1` … `9` gives `!` `"` `#` `$` `%` `&` `'` `(` `)`, `C=` + `Ü` `Ó` `Ű` gives `+` `-` `=`, `C=` + `Ö` gives `0`, `C=` + `↑` gives `^`, `C=` + `£` gives `£`, `C=` + `←` gives the C64 arrow `←`, `C=` + `É` `Á` `Ú` `Ő` gives `:` `;` `*` `@`. `C=` + `Shift` + `É` / `Á` gives `[` / `]`, and **`C=` + `Shift` + `-` gives `_`**. `RUN/STOP` + `RESTORE` no longer drops to BASIC. `CTRL` stays where it is, `RUN/STOP` is Esc. The status line shows `I` (insert) or `O` (overwrite). `Shift` + `CLR/HOME` deletes forward on the C64 keyboard too.

Keys: `F1` save (REU editions: quick save), `F3`/`F5`/`F7` bold/italic/underline, `F2`/`F4`/`F6` copy/paste/cut, `F8` find next, `RUN/STOP` cancels. Accented letters use `C=` plus a letter (see the table above): `C=`+`E` gives é, `C=`+`A` gives à, `C=`+`C` gives ç. In every edition, `C=`+`-` types an underscore `_`.

No mouse? A joystick in port 2 (or port 1) moves the pointer: one pixel per frame for precise aiming, then faster while you hold it. Fire clicks; hold fire while moving to select text. Port 2 is better: port 1 shares its wires with the keyboard, so Plume ignores the keyboard while a port 1 joystick is moving.

## Tested on

A Commodore 64 Ultimate (firmware 1.1.0) at 1 MHz and 64 MHz with a real MPS-803, and VICE 3.10. Also tested on an original C64 with a Kung Fu Flash 2 and joysticks (version 2.0). The Danish edition (2.1) has been tested in VICE so far; feedback from Danish users is welcome. The Hungarian edition (2.2) has also only been tested in VICE so far (screen and emulated MPS-803); feedback from Hungarian users is welcome.

## Plume online: write on your computer, print on the C64

[labbej.eu/plume](https://labbej.eu/plume) is an online text editor for Plume: write with bold, italic and underline (or paste from Word, or open a `.txt`, Markdown or `.docx` file), then download a `.d64` disk that Plume opens, ready to print on the MPS-803. French accents, Danish æ ø å and Hungarian á í ó ő ú ű are kept. Everything happens in your browser: your text is not sent anywhere.

## Building

`make`, `python3`, [cc65](https://github.com/cc65/cc65) and [VICE](https://vice-emu.sourceforge.io) are needed (`brew install cc65 vice` on macOS), then run `make`.

## License

[MIT](LICENSE). The unscii font is in the public domain; the cc65 runtime library linked into the program is under the zlib license.
