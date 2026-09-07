# o2/Makefile -- Odyssey 2 build.
#
# Standalone, like intv/Makefile: the root build drives cc65/cmoc-family C
# toolchains, and Macroassembler AS + p2bin is a different toolchain entirely.
# build.sh does the real work (it has to locate AS, which is not packaged for
# most distributions); this is the convenience wrapper.

PRODUCT = 5card
# Every source, or `make` will not notice one changing. The order mirrors
# 5card.a48's own includes: the resident half, then what every bank carries,
# then the one screen each bank holds.
SOURCES = $(PRODUCT).a48 o2fuji.inc vdctext.inc charmap.inc g7000.h \
          fujilib.inc bankjmp.inc fujistub.inc \
          mailbox.inc cycle.inc tail.inc \
          url.inc net.inc lobby.inc entry.inc tableview.inc \
          render.inc game.inc

.PHONY: all run clean charmap

all: build/$(PRODUCT).bin

build/$(PRODUCT).bin: $(SOURCES) build.sh tools/checklayout.py tools/checkmb.py tools/checkp1.py tools/checkresident.py
	./build.sh $(PRODUCT)

# charmap.inc is generated and committed; regenerate it only when the glyph
# table or the row Y changes.
charmap:
	python3 tools/genmap.py > charmap.inc

run: build/$(PRODUCT).bin
	./run.sh

clean:
	rm -rf build
