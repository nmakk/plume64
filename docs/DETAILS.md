# PlumE REU in detail

The parts of the README that are not needed to get started.

## Editions

- **English** and **French** use the C64 keyboard as it is.
- **Danish** (`dk`): `:` `;` `@` type æ ø å, and with `Shift` Æ Ø Å; `Shift` + `.` and `Shift` + `,` give `:` and `;`; `C=` + `@` gives `@`. `<`, `>`, `[` and `]` are not available in this edition. Every edition displays and prints æ ø å Æ Ø Å (the printer's Text mode prints them as a, o, a).
- **Hungarian** (`hu`), created by **mkckOner**, who drew the letters and wrote the keyboard code, starts with the **HUN** keyboard: the Hungarian layout on the C64 keys (letters stay QWERTY). **`C=` + `Space`** (or *? → HUN / C64 keys*) switches to the **C64** keyboard (the `C=` layer) and back; the status line shows `HUN` or `C64`.

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
| `£` | `€` | `-` |
| `,` `.` `/` | `,` `.` `/` | `<` `>` `?` |
| `CLR/HOME` | start of line; already there: end of line | delete forward (repeats like `INST/DEL`) |
| `INST/DEL` | delete backwards | toggle insert / overwrite |

Under `C=` each key gives its original C64 symbol: `C=` + `1` … `9` gives `!` `"` `#` `$` `%` `&` `'` `(` `)`, `C=` + `Ü` `Ó` `Ű` gives `+` `-` `=`, `C=` + `Ö` gives `0`, `C=` + `↑` gives `^`, `C=` + `£` gives `€`, `C=` + `←` gives `←`, `C=` + `É` `Á` `Ú` `Ő` gives `:` `;` `*` `@`. `C=` + `Shift` + `É` / `Á` gives `[` / `]`, and `C=` + `Shift` + `-` gives `_`. `CTRL` stays where it is, `RUN/STOP` is Esc. `Shift` + `CLR/HOME` deletes forward on the C64 keyboard too. The Tab (four spaces) is `CTRL`+`I`, also in the Edit menu.

## Colours in detail

The screen is a hires bitmap in which each 8x8-pixel cell has one ink colour, and PlumE's text is proportional, so two letters of different colours can fall into the same cell. PlumE makes the best of it:

- **Whole words are always right.** When a word of another colour follows a space and would share a cell with the end of the previous word, the space grows to the next cell edge (0 to 7 pixels, usually 1 or 2, and only where the colours differ). It is still a single space in the text; printing does not stretch it.
- **Punctuation** (`. , ; : ! ? ' " ) -`) right after a word takes the word's colour.
- **Inside a word** the colours can bleed: if the colour changes in the middle of a word, or letters are coloured one by one, neighbouring letters share cells and the letter covering most of the cell wins it (the later one if equal): a coloured letter never disappears, but it tints its neighbours. This is the hardware's limit, not a bug.
- **RVS** (reverse video) fills the cells with the colour and draws the letters in the paper colour; the bar is continuous over a phrase and ends where the last letter ends. Inside RVS only bold is kept (no italic or underline). If a selection contains RVS text, the RVS part blinks (so you can tell it from a plain RVS word) and the rest stays selected.
A close-up of the rules (the space before *Thank you,* grew to the next cell edge, the comma and the `!` took their word's colour, and the single coloured letters of *colourful* bleed into each other):

![Colours in detail](images/plumereu-colors-detail.png)

- **Each theme has its own colour table** so every colour stays visible; the default colour is the theme's text colour. *Style → Plain* removes style, colour and RVS.
- **Colour printing** (C64 Ultimate): if the document has colours or RVS, *Print → Graphics* and *Text* print in colour on the Ultimate's virtual printer. PlumE sets the printer to *PNG COLOR* itself (graphics also use the *Epson FX-80* emulation) and puts it back to *PNG B&W* / *Commodore MPS* before the next plain print. Colours go to the printer's inks (red = orange, purple = magenta, blue = violet, white prints nothing), RVS is a bar with the letters cut out. Graphics keep the font. Then flush the page in the Ultimate's printer menu. A real printer never gets these codes.
- **Files**: PLM and RTF keep the colours (RTF: `\colortbl`, `\cfN`, `\highlightN` for RVS; other colours of an imported RTF become the nearest of the eight). Older versions cannot show a PLM file that uses colours correctly.

## Good to know

- **FTP folders (`/ftp/...`) on a C64 Ultimate**: the Ultimate caches every file it reads from an FTP server and never refreshes the copy. PlumE removes the cached copy before it opens a file, so you always get the current file; if the file image is still incomplete it says so (*Incomplete read: stale FTP cache?*) instead of loading garbage.

- **Disk access**: Plume hides the sprites during IEC disk access, otherwise JiffyDOS falls back to slow serial transfer. Do not read the C64 Ultimate's memory over the REST API during disk access.
- **RTF code pages**: `\ansicpg1250` (Hungarian ő ű as `\'f5` `\'fb`) and the usual cp1252 escapes are read correctly. Not handled: other code pages and RTF text colours. Loading a very large file over IEC is very slow; use the C64U buttons.
- **Styling a selection**: *Bold* / *Italic* / *Underline* / *Plain* work on a selection that fits into the editing window in RAM (about 2500 characters); for a larger one Plume says so and the style can be applied in parts. Copy, cut, paste, delete and find work on any size. Long runs of one style are no problem for editing or saving.
- **Original Plume files**: PLM1 and PLM2 documents of the original Plume open directly; PLM3 (over 64 KB) cannot be read by the original.
- From a Mac, [`tools/c64u/Loadfromc64u.sh`](../tools/c64u/Loadfromc64u.sh) and [`Savetoc64u.sh`](../tools/c64u/Savetoc64u.sh) fetch and store files in a disk image on a C64 Ultimate (FTP + `c1541`).

## Quick save in detail

**Why it exists.** Saving a whole document means writing every byte of it again. For a large document that takes long: over the IEC serial bus of a plain C64 a 250 KB file needs minutes, and even on the C64 Ultimate or a disk image the time grows with the size of the document, although only a few words may have changed. Quick save (`F1`) writes only what changed, so it takes a moment whatever the size of the document, and the original file stays untouched until the next full *Save*.

**How it works.**
1. **The journal.** Every edit is recorded in the REU as it happens: a 7-byte header (position, number of deleted bytes, number of inserted bytes) followed by the inserted text. Letters typed one after another and repeated backspaces extend the last record instead of making new ones. The deleted text is kept in a second REU area; the same journal drives Undo and Redo.
2. **F1 writes `NAME.bck` next to the file** (same drive, SD card or USB; on a disk the name is at most 12 characters plus `.bck`). The file starts with a 13-byte header: `PBK3`, the length of the file it belongs to, a 16-bit signature of that file (its length and three samples of 128 bytes: start, middle and end) and the length of the journal. Then comes the journal since the last full save. If nothing has changed since then, `F1` only removes an old `.bck` and marks the document as saved.
3. **Opening the file** looks for `NAME.bck`. If its length and signature match the file, the edits are replayed silently and *Quick save restored.* is shown; everything is back as you left it, and Undo works across the restored edits. If they do not match (the file was changed by another program or replaced), Plume asks whether to apply the `.bck` anyway; nothing is ever applied or thrown away without asking.
4. **Leaving the document** (*New* or *Open*) after a quick save asks `Only quick saved (.bck). Save the whole file now?`: *Save* writes the whole file first, *No save* goes on and the changes stay in the `.bck`.
5. **A full *Save* or *Save as*** writes the whole file, makes it the new base of the journal and deletes the `.bck`.
6. **Two ways to get the plain file**: *File → Reload file* opens the file without its `.bck`; *File → Restore from .bck* opens it with the `.bck`.

**Good to know.**
- The `.bck` is only valid for the file it was made for. When you copy a document to another place, copy the `.bck` with it, or do a full *Save* first.
- The journal holds about 256 KB of edits and 40,000 records (16 KB and 2,500 on a REU under 2 MB). When it is full, `F1` does a full save instead and starts a new journal.
- The `.bck` files are hidden in the file requester, so they cannot be opened by mistake; a `*` at the left of a file name tells that it has a `.bck` (on the SD card by trying to open `NAME.bck`, on a disk by a filtered directory read of `$0:NAME.BCK`).
- On a C64 Ultimate the keyboard LED lights while there are unsaved changes and goes out after `F1` or *Save*.
