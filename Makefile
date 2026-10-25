# Top-level build.
#
#   make host     host tools (previewer, tests) with gcc     -> build/host/
#   make test     run the host test suite
#   make preview  render preview PNGs of every theme         -> build/preview/<theme>/
#   make win16    RETHEME.DLL / RETHEME.EXE / SETUP.EXE with Open Watcom -> build/win16/
#   make release  ZIP, floppy image and checksums           -> build/release/
#
# Open Watcom: set WATCOM (default /opt/watcom); see docs/BUILDING.md.

CC      ?= gcc
CFLAGS  ?= -O2 -g
CFLAGS  += -std=c99 -pedantic -Wdeclaration-after-statement -Wall -Wextra -Wno-long-long -Werror -Iengine/include -Ihost

CORE_SRC = engine/theme/util.c engine/theme/parts.c engine/theme/ini.c engine/theme/theme.c \
           engine/render/render.c engine/render/glyphs.c engine/geom/nclayout.c
HOST_SRC = host/backend_sw.c host/platform_host.c host/font_data.c host/scenes.c
HDRS     = $(wildcard engine/include/*.h host/*.h)

B = build/host
THEMES = $(patsubst themes/%/THEME.INI,%,$(wildcard themes/*/THEME.INI))

.PHONY: all host test preview win16 release clean update-golden

all: host

host: $(B)/preview $(B)/rttest

$(B)/preview: $(CORE_SRC) $(HOST_SRC) host/preview.c $(HDRS)
	@mkdir -p $(B)
	$(CC) $(CFLAGS) -o $@ $(CORE_SRC) $(HOST_SRC) host/preview.c

$(B)/rttest: $(CORE_SRC) $(HOST_SRC) $(wildcard host/tests/*.c) $(HDRS)
	@mkdir -p $(B)
	$(CC) $(CFLAGS) -o $@ $(CORE_SRC) $(HOST_SRC) $(wildcard host/tests/*.c)

test: $(B)/rttest
	$(B)/rttest

# Rewrites the golden images from the current renderer. Review the diff!
update-golden: $(B)/rttest
	$(B)/rttest --update-golden

preview: $(B)/preview
	@for t in $(THEMES); do mkdir -p build/preview/$$t; $(B)/preview themes/$$t build/preview/$$t || exit 1; done

win16:
	$(MAKE) -f win16.mk

# needs python3 with Pillow and mtools (see docs/BUILDING.md)
release: preview win16
	python3 tools/release/mkrelease.py

clean:
	rm -rf build
