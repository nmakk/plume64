# Changes since Plume 2.2 (fork point `1f142d9`)

This fork adds a REU edition (Magic Desk cartridge, `build/plume-XX-2.2e.crt`) and several fixes. The standard builds only get the shared fixes and a smaller stack (0x180).

## 1. Core features (REU edition)
- **REU-backed documents**: 24-bit positions, up to 16 MB of text; a 3250-byte sliding window in RAM loads automatically while editing (the original limit was 20,000 bytes).
- **PLM3 format** for documents over 64 KB.
- **RTF import and export**; the RTF name, the title and F1 save back to the same RTF file.
- **Scrollbar** over the whole document, including jumping to the end.
- **Find across the whole document** (F8 = next), including distant matches.
- **Chunked printing** for large documents.
- **Inverse block cursor** (blinking block over the character, two sprites).
- **Four color themes**, switched with CTRL+← (the zero key in the HUN layout), in every build.
- **File requester** (Open, Save as, Save as RTF): one column, block sizes, free blocks, paging every 13 files.
- **Capacity check before saving** (counting pre-pass for RTF); if a file only fits after deleting the old file of the same name, it asks and deletes first.
- **Sprites off during disk I/O**, so JiffyDOS stays fast.
- `tools/c64u/Loadfromc64u.sh`, `Savetoc64u.sh` (d81 images to and from a C64 Ultimate), `tools/reutest` (test scripts), `tools/uci_http.s`.

## 2. Later features
- **Key click** (SID click on typing, Return and delete), toggled with CTRL+9.
- **Mouse wheel** with accelerating scroll and a startup wheel-sensitivity setting.
- **LEDs follow the theme** (C64 Ultimate keyboard and LED strip, via the UCI HTTP target; separate LED overlay in cartridge bank 62; UCI presence check).
- **Keyboard LED shows the save state**: lit while there are unsaved changes, off once saved (synced with an idle delay).
- **F-key-friendly colors**: green theme Chartreuse + Bright, blue theme Indigo + Bright, white and yellow Pure.
- **Unified Open**: one Open recognizes RTF and the native format; saving keeps the file's own format, and either can be chosen.
- **Overlay system**: RTF, printing and quick save live in separate cartridge banks and load into a shared RAM area (about 2.5 KB freed).
- **Quick save (F1)**: a `.bck` journal (position, delete and insert records, typing and backspace coalesced), base file verified by a signature (length plus a checksum of 3 samples), restored automatically on open, deleted by a full save, warning if it does not match.
- **Disk-full dialog**: offers to delete a file or swap the disk, so a save cannot silently fail.
- **Double click** selects a word, **triple click** a sentence.
- **Hungarian (HUN) keyboard layout** alongside the C64 layout: positional layout via KERNAL tables, QWERTY (Y and Z as printed), accents, `,` `.` with Shift giving `<` `>`, `?` on Shift+slash, underscore on C=+minus; toggle with C=+Space or the menu, with a status-line indicator; **Tab**; **Home/End** (CLR/HOME = line start, or line end if already at the start); **forward delete** (Shift+CLR/HOME) with key repeat in both layouts.
- **Clipboard in the REU (256 KB) and document-wide selection**: Select all selects everything; cut/copy/paste/delete of huge selections, each one undo step (journal records carry block/continuation flags); delete beyond the undo capacity asks for confirmation; styles only for selections that fit the window.
- **Progress bar** in the status line during load/save (Plume, RTF, quick save).
- **Scrollbar paging** (click above/below the thumb, repeats while held); line number marked `~` when estimated after a jump (smoothed bytes-per-line estimate); menu entries for functions that only had key combos (Revert / Restore .bck, Colors, Key click, Overwrite); the Find box no longer shows garbage (the LED code overwrote its buffer in the cassette buffer; it now lives at $02D0).
- **Undo / Redo** (CTRL+Z / CTRL+Y, RESTORE / Shift+RESTORE, Edit menu); RUN/STOP+RESTORE reloads the file without its `.bck`, Shift+RUN/STOP+RESTORE reloads it with the `.bck`; RUN/STOP closes every dialog: step-by-step, back to the start of the session and across saves; each typed letter is one step; restored `.bck` edits can be undone. Built on the quick-save journal plus a bank of deleted text in the REU. RUN/STOP+RESTORE no longer crashes to BASIC.
- **C= layer in the HUN layout**: C=+1…9 give the C64 key-top symbols, C=+ü/ó/ű give + - =, C=+ö/↑/£ give 0 ^ £, C=+← gives the C64 arrow (new glyph), C=+Shift+é/á give [ ], underscore moved to C=+Shift+-.
- **Insert/overwrite mode** (Shift+INST/DEL) with an I/O indicator in the status line.
- English and French README sections for all of the above.

## 3. Bug fixes
- **Dropped keystrokes**: the 8-bit jiffy difference in `read_key` wrapped around and flushed the key buffer for about 6 frames every ~5 seconds (about 1 key in 50 lost); now a countdown (`joy_hold`).
- **Flicker** of the status line and the typed line: line and status drawing is buffered (LINEBUF).
- **Scrollbar thumb**: no flicker (redrawn only when needed), no smear when scrolling (row-by-row scroll excluding column 39), returns after a menu closes, no longer grows when a line is added at the bottom.
- **Scrolling stall**: the sliding window follows the top and bottom of the screen instead of a fixed 25–30 lines.
- **End of document reachable** after jumping to the scrollbar bottom or after a search; full redraw.
- **Distant find**: corrected offset and full redraw after the window moves.
- **RTF save length**: the 16-bit length is fixed, so the whole document is saved.
- **cp1252 0x80–0x9F characters** load correctly (a cc65 `?:` truncation bug).
- **Menus**: an empty cell row above and below the items; the "tail" of the bottom-right window shadow is gone.
- **Title** is always redrawn after saving, not only when the modified flag changes.
- **Key buffer flush after scrolling**, so keys no longer keep repeating.
- **C=+Space toggle** waits for the key release, so it no longer toggles twice.
- **Forward delete repeat**: KERNAL repeats code 0x7F every frame (one press deleted five characters); it now uses code 0x94, which repeats like backspace.
- **A loaded file starts clean**: no asterisk, no keyboard light.
- **Memory**: print buffer moved to unused screen RAM ($0540), stack 0x200 → 0x180 (fixed BSS overflows); the PLM1 reader was dropped from the REU builds (standard builds still read it); `ROWBASE` is a shared function and the `WORDCH` macro became functions.
