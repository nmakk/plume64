#!/usr/bin/env python3
"""Assemble la cartouche de Plume : mkcrt.py banque0.bin programme.prg sortie.crt NOM [md]

Sans option : EasyFlash (banque 0 = ROML + ROMH). Avec "md" : Magic Desk, pour
l'edition REU (la RAM d'E/S2 de l'EasyFlash masque les registres du REU)."""
import struct, sys

BANK = 0x2000
boot, prg_path, out, name = sys.argv[1:5]
magic_desk = len(sys.argv) > 5 and sys.argv[5] == "md"
rom = open(boot, "rb").read()
if len(rom) != (BANK if magic_desk else 2 * BANK):
    sys.exit(f"banque 0 : taille inattendue ({len(rom)})")
prg = open(prg_path, "rb").read()
if prg[:2] != b"\x01\x08":
    sys.exit("le programme doit se charger en $0801")
body = prg[2:]
if 0x0801 + len(body) > 0xC000:
    sys.exit("programme trop grand")

# type materiel 32 = EasyFlash (EXROM 1, GAME 0) ; 19 = Magic Desk (EXROM 0, GAME 1)
hw, exrom, game, chip_type = (19, 0, 1, 0) if magic_desk else (32, 1, 0, 2)
header = (b"C64 CARTRIDGE   " + struct.pack(">IHHBB", 0x40, 0x0100, hw, exrom, game) + bytes(6)
          + name.upper().encode()[:32].ljust(32, b"\0"))


def chip(bank, load, data):
    return b"CHIP" + struct.pack(">IHHHH", 0x10 + BANK, chip_type, bank, load, BANK) + data.ljust(BANK, b"\xFF")


crt = bytearray(header)
crt += chip(0, 0x8000, rom[:BANK])
if not magic_desk:
    crt += chip(0, 0xA000, rom[BANK:])
for n, i in enumerate(range(0, len(body), BANK)):
    crt += chip(1 + n, 0x8000, body[i:i + BANK])
if magic_desk and len(sys.argv) > 6:             # recouvrement des leds (src/led.s), banque 62
    crt += chip(62, 0x8000, open(sys.argv[6], "rb").read())
open(out, "wb").write(crt)
print(f"{out} : {1 + (len(body) + BANK - 1) // BANK} banques, programme de {len(body)} octets")
