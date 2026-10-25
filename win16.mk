# Win16 build with Open Watcom (GNU make syntax). Invoked by "make win16".
#
# RETHEME.DLL: large model, SS != DS (it runs on other tasks' stacks), no
# stack checks (the checker compares against our DGROUP, not the caller's
# stack), 286 code (Windows 3.1 standard mode minimum). All code segments
# are FIXED: hook procedures and patch handlers are entered from USER/GDI.

WATCOM ?= /opt/watcom
export WATCOM
export PATH := $(WATCOM)/binl64:$(WATCOM)/binl:$(PATH)
export INCLUDE := $(WATCOM)/h:$(WATCOM)/h/win

O = build/win16
CORE_SRC = engine/theme/util.c engine/theme/parts.c engine/theme/ini.c engine/theme/theme.c \
           engine/render/render.c engine/render/glyphs.c engine/geom/nclayout.c
W16_SRC  = engine/win16/rtdll.c engine/win16/patch.c engine/win16/hooks.c \
           engine/win16/intercept.c engine/win16/ncpaint.c engine/win16/wstate.c \
           engine/win16/gdi16.c engine/win16/platform16.c engine/win16/log.c \
           engine/win16/controls.c engine/win16/menus.c \
           engine/win16/scroll.c

DLL_OBJ = $(patsubst %.c,$(O)/obj/%.obj,$(notdir $(CORE_SRC) $(W16_SRC)))
CFLAGS_COMMON = -bt=windows -ml -2 -os -s -w4 -we -zq -Iengine/include -Iengine/win16
CFLAGS_DLL = $(CFLAGS_COMMON) -bd -zu -zW
CFLAGS_EXE = $(CFLAGS_COMMON) -zW

vpath %.c engine/theme engine/render engine/geom engine/win16 loader

.PHONY: all
all: $(O)/RETHEME.DLL $(O)/RETHEME.EXE $(O)/GALLERY.EXE $(O)/THEMES.EXE $(O)/SETUP.EXE

$(O)/obj/%.obj: %.c $(wildcard engine/include/*.h engine/win16/*.h)
	@mkdir -p $(O)/obj
	wcc $(CFLAGS_DLL) -fo=$@ $<

$(O)/obj/loader.obj: loader/loader.c
	@mkdir -p $(O)/obj
	wcc $(CFLAGS_EXE) -fo=$@ $<

$(O)/RETHEME.DLL: $(DLL_OBJ) win16/retheme.lnk
	wlink @win16/retheme.lnk name $@ option map=$(O)/RETHEME.MAP $(foreach o,$(DLL_OBJ),file $(o))

$(O)/obj/loader.res: loader/loader.rc loader/retheme.ico
	@mkdir -p $(O)/obj
	wrc -q -r -bt=windows -i=loader -fo=$@ $<

$(O)/RETHEME.EXE: $(O)/obj/loader.obj $(O)/obj/loader.res
	wlink system windows name $@ option modname=RTLOADER, map=$(O)/RETHEME.EXE.MAP option stack=8k, heapsize=2k, resource=$(O)/obj/loader.res file $<

$(O)/obj/gallery.obj: testapp/gallery.c testapp/gallery.h
	@mkdir -p $(O)/obj
	wcc $(CFLAGS_EXE) -fo=$@ $<

$(O)/obj/gallery.res: testapp/gallery.rc testapp/gallery.h
	@mkdir -p $(O)/obj
	wrc -q -r -bt=windows -i=testapp -fo=$@ $<

$(O)/GALLERY.EXE: $(O)/obj/gallery.obj $(O)/obj/gallery.res
	wlink system windows name $@ option stack=8k, heapsize=2k, resource=$(O)/obj/gallery.res file $<

$(O)/obj/themes.obj: switcher/themes.c switcher/themes.h
	@mkdir -p $(O)/obj
	wcc $(CFLAGS_EXE) -fo=$@ $<

$(O)/obj/themes.res: switcher/themes.rc switcher/themes.h loader/retheme.ico
	@mkdir -p $(O)/obj
	wrc -q -r -bt=windows -i=switcher -i=loader -fo=$@ $<

$(O)/THEMES.EXE: $(O)/obj/themes.obj $(O)/obj/themes.res
	wlink system windows name $@ option stack=8k, heapsize=2k, resource=$(O)/obj/themes.res file $<

# SETUP.EXE: Win16Setup (git submodule external/win16setup)
W16SETUP = external/win16setup
$(O)/SETUP.EXE: $(wildcard $(W16SETUP)/src/*)
	@test -f $(W16SETUP)/Makefile || { echo "external/win16setup is empty: git submodule update --init"; exit 1; }
	$(MAKE) -C $(W16SETUP) O=$(abspath $(O))/w16setup WATCOM=$(WATCOM)
	cp $(O)/w16setup/SETUP.EXE $@
