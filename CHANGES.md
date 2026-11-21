# PlumE REU: changes since Plume 2.2 (fork point `1f142d9`)

This project is **PlumE** (*enhanced* Plume), and its edition for a C64 with a REU is **PlumE REU**, versioned by year and letter (`26a` = the first release of 2026). It is based on Plume 2.2 by LABBEJ27 and is a separate project from the original Plume, whose goal is to stay small and run on an unexpanded C64. The REU edition is a Magic Desk cartridge (`build/plumereu-XX-26a.crt`, tested on a C64 Ultimate with a 16 MB REU) and also adds many fixes. The standard builds get the shared fixes and a smaller stack (0x180). Everything is documented in `README.md`.

The original Plume has since taken over two of the shared fixes (lost keystrokes, flicker-free line and status drawing). Nothing else of the original's later work (2.3 HU "Hide idle pointer", the `dist/*.prg` programs) is merged here.

## 0. New in PlumE REU 26a
- **Name and version**: PlumE REU 26a. The About box shows the name, version, authors (Plume: LABBEJ27, PlumE: Norbert Makk, HU keys: mkckOner) and the font credit; the *E* of the **PlumE** label at the top right has its own colour in each colour theme.
- **C64 Ultimate file system**: Open, Save, Save as and Quick save work on the SD card, USB, Flash and the `/ftp/` entry (mounted FTP servers) through the Ultimate's command interface (UCI, DOS target), with a small module (`src/sd.s`) that lives at `$C700` and is copied from cartridge bank 61 at startup. A 250 KB RTF opens in about 12 seconds instead of minutes over the IEC serial bus. Plume checks at startup that the Ultimate's DOS target answers; if not (old firmware, not a C64 Ultimate) the *C64U* button is not shown and everything works on the disk as before.
- **Three formats**: `.plm`, `.rtf` and `.txt` (UTF-8, LF). Open recognises the format from the content. Save and Quick save keep the document's own format and place. Saving as text warns when the document has formatting and then removes it from the screen and from memory, so the document is exactly the file.
- **One file requester** for Open and Save as, with *Disk* / *C64U* place buttons and *PLM* / *RTF* / *TXT* format buttons (the document's current choice pressed, RTF for a new document). Folders with `..`, the last folder is remembered, long names are shortened in the middle with `~`, the Plume `.bck` files are hidden, a name typed without an extension gets the format's extension (on disk at most 12 characters without one), and replacing an existing file asks first. The *File* menu is shorter: New, Open, Reload, Restore .bck, Quick save, Save, Save as.
- **New look**: flat panels with a thin outline and a shadow, bevelled buttons, a list box with scroll bar, redesigned file requester and About box, a colour accent per colour theme.
- **Small font** (*Style → Small font*, on by default): a hand-drawn proportional font with one-pixel strokes and composed accents (Hungarian, French, Danish), about 60 to 72 characters per line. `tools/mkfont_small.py` builds it, `tools/fontbin.py` makes the normal font's binary; both live in cartridge banks 60 and 59 and are copied over the program's font table (`font_load` in `src/reu.s`) without any change to the drawing code. Bold uses a gap-preserving smear for it, and printing always uses the normal font.
- **Exact line numbers**: a background index (cartridge overlay 9, checkpoints every 2 KB in the REU) lays the document out while idle and never blocks typing; after a jump or an edit the number is marked `~` until the index reaches the window. The status line shows `Line N/total` (just `L` with the normal font). `next_line_asm` got a second entry for any buffer.
- **File requester**: mouse wheel scrolling, only the list is redrawn when scrolling or paging, the directory read stops as soon as the page is complete, sizes in K/M/G, SD files over 15 MB are greyed out, the *Disk* button cycles through IEC units 8 to 11 (`U08` ... `U11`), an hourglass pointer shows while the C64U is being read.
- **Status line**: `char` for 0 and 1, `chars` otherwise; the B/I/U indicator now follows a style applied to a selection at once.
- **Build**: `make` builds only the PlumE REU editions; the original-edition cartridges are built explicitly (`make build/plume-XX.crt`) and no longer get new features. Overlay area 3840 bytes, new overlay 9.
- **Fix**: DIR entries of the file requester could show a number (a stale pointer write after the size lookup).
- **Fix**: the quick save signature end sample was read from a wrong REU address (cc65 truncates nested `?:` on 32-bit values), so a valid `.bck` could be rejected on the next session. New `PBK3` format; `PBK2` files are still read (by length); a `.bck` that does not match asks whether to apply it anyway.
- **LED time-out**: if the Ultimate's HTTP target does not answer in time, Plume stops trying for the rest of the session instead of freezing for seconds at each colour change.
- **Layout**: the line tables are limited to 520 lines per window (was 950) to make room for the SD module; overlays 7 (file requester) and 8 (menus, styles) were added.

## 1. REU edition: big documents
- **Documents in the REU**: 24-bit positions, up to ~15 MB of text; a sliding window in RAM loads automatically while editing (the original limit was 20,000 bytes). PLM3 format for documents over 64 KB.
- **RTF import and export**; the RTF name, the title and F1 save back to the same RTF file.
- **Unified Open**: one Open recognizes the format from the content; saving keeps the file's own format (see section 0 for the three formats).
- **Scrollbar** over the whole document; clicking above/below the thumb pages (repeats while held), dragging jumps anywhere.
- **Find across the whole document** (F8 = next), including distant matches.
- **Clipboard in the REU (256 KB) and document-wide selection**: *Select all* selects everything; cut, copy, paste and delete work on huge selections. A delete beyond the undo capacity asks for confirmation.
- **Chunked printing** for large documents.
- **File requester** (Open, Save as): one column, block sizes, free blocks, paging every 13 files; **capacity check before saving**; **disk-full dialog** (delete a file or swap the disk, so a save cannot silently fail).
- **Progress bar** in the status line during load and save (Plume files, RTF, quick save).
- **Line numbers**: after a jump the number is an estimate and is marked `~` (smoothed bytes-per-line estimate); exact again at the start of the document.
- **Overlay system**: RTF, printing, quick save, undo, clipboard and file I/O live in separate cartridge banks and load into a shared RAM area; the LED code is in bank 62.
- `tools/c64u/Loadfromc64u.sh`, `Savetoc64u.sh` (d81 images to and from a C64 Ultimate), `tools/reutest` (test scripts), `tools/uci_http.s`.

## 2. Editing features
- **Undo / Redo**: CTRL+Z / CTRL+Y, RESTORE / Shift+RESTORE, Edit menu. Step by step (each typed letter is one step; a paste, a cut or a huge delete is one step), back to the start of the session and across saves; 256 KB journal, 256 KB of deleted text, 40,000 records, 4096 redo steps (smaller on REUs under 2 MB). Edits restored from a `.bck` can be undone too.
- **Quick save (F1)**: a `.bck` journal (position, delete and insert records, typing and backspace coalesced), base file verified by a signature (length plus a checksum of 3 samples), restored automatically on open, deleted by a full save, if it does not match the file the user is asked whether to apply it anyway (`PBK3` format; `PBK2` files are still read, by length only). Fixed: the signature's end sample was read from a wrong REU address (cc65 truncates nested `?:` on 32-bit values), so a valid `.bck` could be rejected on the next session.
- **RESTORE keys**: RUN/STOP+RESTORE reloads the file from disk without its `.bck`; Shift+RUN/STOP+RESTORE reloads it with the `.bck` (also in the File menu). RUN/STOP+RESTORE no longer drops to BASIC.
- **Double click** selects a word, **triple click** a sentence.
- **Insert/overwrite mode** (Shift+INST/DEL) with an I/O indicator in the status line.
- **Key click** (SID click on typing, Return and delete), CTRL+9 or the `?` menu.
- **Mouse wheel** with accelerating scroll and a startup wheel-sensitivity setting.
- **Menus** now contain every function that also has a key: Undo/Redo, Revert / Restore .bck, Colors, Key click, Overwrite. RUN/STOP closes every dialog.
- **Inverse block cursor** (blinking block over the character, two sprites) and **four color themes**, switched with CTRL+← (the zero key in the HUN layout) or Style → Colors.

## 3. C64 Ultimate extras
- **LEDs follow the theme** (keyboard and LED strip, via the UCI HTTP target; UCI presence check); F-key-friendly colors (green: Chartreuse + Bright, blue: Indigo + Bright).
- **Keyboard LED shows the save state**: lit while there are unsaved changes, off once saved (synced with an idle delay).
- Sprites off during disk I/O, so JiffyDOS stays fast.

## 4. Hungarian (HUN) keyboard layout
Alongside the original C64 layout: positional Hungarian layout built from KERNAL tables, QWERTY (Y and Z as printed), accents, `,` `.` with Shift giving `<` `>`, `?` on Shift+slash; toggle with C=+Space or the menu, with a status-line indicator; **Tab**; **Home/End** (CLR/HOME = line start, or line end if already at the start); **forward delete** (Shift+CLR/HOME) with key repeat in both layouts. The `C=` layer gives the original C64 key-top symbols (C=+1…9, C=+ü/ó/ű = `+ - =`, C=+ö/↑/£ = `0 ^ £`, C=+← = the C64 arrow, a new glyph), C=+Shift+é/á = `[ ]`, and the underscore is C=+Shift+-.

## 5. Bug fixes
- **Dropped keystrokes**: the 8-bit jiffy difference in `read_key` wrapped around and flushed the key buffer for about 6 frames every ~5 seconds (about 1 key in 50 lost); now a countdown (`joy_hold`).
- **Flicker** of the status line and the typed line: line and status drawing is buffered (LINEBUF).
- **Scrollbar thumb**: no flicker (redrawn only when needed), no smear when scrolling (row-by-row scroll excluding column 39), returns after a menu closes, no longer grows when a line is added at the bottom.
- **Scrolling stall**: the sliding window follows the top and bottom of the screen instead of a fixed 25–30 lines.
- **End of document reachable** after jumping to the scrollbar bottom or after a search; full redraw. **Distant find**: corrected offset and full redraw after the window moves.
- **RTF save length**: the 16-bit length is fixed, so the whole document is saved. **cp1252 0x80–0x9F characters** load correctly (a cc65 `?:` truncation bug).
- **Menus**: an empty cell row above and below the items; the "tail" of the bottom-right window shadow is gone. The **title** is always redrawn after saving.
- **Key buffer flush after scrolling**, so keys no longer keep repeating. **C=+Space toggle** waits for the key release. **Forward delete repeat**: KERNAL repeats code 0x7F every frame (one press deleted five characters); it now uses code 0x94, which repeats like backspace.
- **A loaded file starts clean**: no asterisk, no keyboard light.
- **Find box garbage**: the LED code used the cassette buffer as scratch space and overwrote the search text; it now lives at `$02D0`.
- **Memory**: print buffer moved to unused screen RAM (`$0540`), stack 0x200 → 0x180 (fixed BSS overflows); the PLM1 reader was dropped from the REU builds (standard builds still read it); `ROWBASE` is a shared function and the `WORDCH` macro became functions.
