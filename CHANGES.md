# Changes since Plume 2.2 (fork point `1f142d9`)

This fork adds a **REU edition** (Magic Desk cartridge, `build/plume-XX-2.2e.crt`, tested on a C64 Ultimate with a 16 MB REU) and many fixes. The standard builds get the shared fixes and a smaller stack (0x180). Everything is documented in English and French in `README.md`.

Not yet merged: your later commits (2.3 HU "Hide idle pointer", the `dist/*.prg` programs). They conflict with this fork in `src/main.c`, `src/strings.h` and `README.md`; I can rebase on top of them, or split the work into smaller pull requests, whichever you prefer.

## 1. REU edition: big documents
- **Documents in the REU**: 24-bit positions, up to ~15 MB of text; a sliding window in RAM loads automatically while editing (the original limit was 20,000 bytes). PLM3 format for documents over 64 KB.
- **RTF import and export**; the RTF name, the title and F1 save back to the same RTF file.
- **Unified Open**: one Open recognizes RTF and the native format; saving keeps the file's own format, and either can be chosen.
- **Scrollbar** over the whole document; clicking above/below the thumb pages (repeats while held), dragging jumps anywhere.
- **Find across the whole document** (F8 = next), including distant matches.
- **Clipboard in the REU (256 KB) and document-wide selection**: *Select all* selects everything; cut, copy, paste and delete work on huge selections. A delete beyond the undo capacity asks for confirmation.
- **Chunked printing** for large documents.
- **File requester** (Open, Save as, Save as RTF): one column, block sizes, free blocks, paging every 13 files; **capacity check before saving**; **disk-full dialog** (delete a file or swap the disk, so a save cannot silently fail).
- **Progress bar** in the status line during load and save (Plume files, RTF, quick save).
- **Line numbers**: after a jump the number is an estimate and is marked `~` (smoothed bytes-per-line estimate); exact again at the start of the document.
- **Overlay system**: RTF, printing, quick save, undo, clipboard and file I/O live in separate cartridge banks and load into a shared RAM area; the LED code is in bank 62.
- `tools/c64u/Loadfromc64u.sh`, `Savetoc64u.sh` (d81 images to and from a C64 Ultimate), `tools/reutest` (test scripts), `tools/uci_http.s`.

## 2. Editing features
- **Undo / Redo**: CTRL+Z / CTRL+Y, RESTORE / Shift+RESTORE, Edit menu. Step by step (each typed letter is one step; a paste, a cut or a huge delete is one step), back to the start of the session and across saves; 256 KB journal, 256 KB of deleted text, 40,000 records, 4096 redo steps (smaller on REUs under 2 MB). Edits restored from a `.bck` can be undone too.
- **Quick save (F1)**: a `.bck` journal (position, delete and insert records, typing and backspace coalesced), base file verified by a signature (length plus a checksum of 3 samples), restored automatically on open, deleted by a full save, warning if it does not match (`PBK2` format).
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
