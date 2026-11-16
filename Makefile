# Plume : traitement de texte graphique pour Commodore 64.
# make -> build/plume-fr.crt, plume-en.crt, plume-dk.crt et plume-hu.crt (et les .prg)
# dk : menus anglais, clavier danois (æ ø å sur : ; @)
# hu : menus anglais, clavier hongrois (á é í ó ö ő ú ü ű sous C=)
LANGS := fr en dk hu
SRC   := src/main.c src/gfx.s src/font.s
PAGES  = $(shell python3 -I -c "import os,sys;print((os.path.getsize(sys.argv[1])-2+255)//256)" $<)

.PHONY: all clean
all: $(foreach l,$(LANGS),build/plume-$(l).crt build/plume-$(l)-reu.crt)

src/font.s: fonts/unscii-8.hex tools/mkfont.py
	python3 -I tools/mkfont.py $< $@

build/plume-fr.prg: $(SRC) src/strings.h src/charmap.h
	@mkdir -p build/fr
	cl65 -t c64 -O -Or -Cl -o $@ -m build/plume-fr.map --obj-path build/fr -Wl -D,__HIMEM__=0xC000 -Wl -D,__STACKSIZE__=0x200 $(SRC)

build/plume-en.prg: $(SRC) src/strings.h src/charmap.h
	@mkdir -p build/en
	cl65 -t c64 -O -Or -Cl -DLANG_EN -o $@ -m build/plume-en.map --obj-path build/en -Wl -D,__HIMEM__=0xC000 -Wl -D,__STACKSIZE__=0x200 $(SRC)

build/plume-dk.prg: $(SRC) src/strings.h src/charmap.h
	@mkdir -p build/dk
	cl65 -t c64 -O -Or -Cl -DLANG_EN -DKBD_DK -o $@ -m build/plume-dk.map --obj-path build/dk -Wl -D,__HIMEM__=0xC000 -Wl -D,__STACKSIZE__=0x200 $(SRC)

build/plume-hu.prg: $(SRC) src/strings.h src/charmap.h
	@mkdir -p build/hu
	cl65 -t c64 -O -Or -Cl -DLANG_EN -DKBD_HU -o $@ -m build/plume-hu.map --obj-path build/hu -Wl -D,__HIMEM__=0xC000 -Wl -D,__STACKSIZE__=0x200 $(SRC)

# Edition REU : fenetre de 16 Ko en RAM, document entier dans le REU. Cartouche
# Magic Desk (la RAM d'E/S2 de l'EasyFlash masque les registres du REU).
SRC_REU := $(SRC) src/reu.s
define REU_RULE
build/plume-$(1)-reu.prg: $$(SRC_REU) src/strings.h src/charmap.h
	@mkdir -p build/$(1)-reu
	cl65 -t c64 -O -Or -Cl -DREU $(2) -o $$@ -m build/plume-$(1)-reu.map --obj-path build/$(1)-reu -Wl -D,__HIMEM__=0xC000 -Wl -D,__STACKSIZE__=0x200 $$(SRC_REU)
endef
$(eval $(call REU_RULE,fr,))
$(eval $(call REU_RULE,en,-DLANG_EN))
$(eval $(call REU_RULE,dk,-DLANG_EN -DKBD_DK))
$(eval $(call REU_RULE,hu,-DLANG_EN -DKBD_HU))

build/plume-%-reu.crt: build/plume-%-reu.prg crt-md.s crt-md.cfg tools/mkcrt.py
	ca65 -D PRG_PAGES=$(PAGES) -o build/crt-$*-reu.o crt-md.s
	ld65 -C crt-md.cfg -o build/crt-$*-reu.bin build/crt-$*-reu.o
	python3 -I tools/mkcrt.py build/crt-$*-reu.bin $< $@ "PLUME $* REU" md

build/plume-%.crt: build/plume-%.prg crt.s crt.cfg tools/mkcrt.py
	ca65 -D PRG_PAGES=$(PAGES) -o build/crt-$*.o crt.s
	ld65 -C crt.cfg -o build/crt-$*.bin build/crt-$*.o
	python3 -I tools/mkcrt.py build/crt-$*.bin $< $@ "PLUME $*"

clean:
	rm -rf build
