all:
.SILENT:
.SECONDARY:
PRECMD=echo "  $(@F)" ; mkdir -p $(@D) ;

ifneq (,$(strip $(filter clean,$(MAKECMDGOALS))))

clean:;rm -rf mid out

else

include etc/config.mk
etc/config.mk:|etc/config.mk.default;cp etc/config.mk.default $@ ; echo "Generated $@. Please edit then make again." ; exit 1

# Discover source files and filter optional units.
SRCFILES:=$(shell find src -type f)
SRCFILES:=$(filter-out src/opt/%,$(SRCFILES)) $(filter $(foreach U,$(OPT_ENABLE),src/opt/$U/%),$(SRCFILES))

CFILES:=$(filter %.c %.m %.cxx %.s,$(SRCFILES))
OFILES:=$(patsubst src/%,mid/%.o,$(basename $(CFILES)))
mid/%.o:src/%.c;$(PRECMD) $(CC) -o$@ $<
mid/%.o:src/%.cxx;$(PRECMD) $(CXX) -o$@ $<
mid/%.o:src/%.m;$(PRECMD) $(OBJC) -o$@ $<
mid/%.o:src/%.s;$(PRECMD) $(AS) -o$@ $<
-include $(OFILES:.o=.d)

ifneq (,$(strip $(BUILD_LIB)))
  LIB:=out/libemuhost.a
  LIB_CONFIG_SCRIPT:=out/emuhost-config
  all:$(LIB) $(LIB_CONFIG_SCRIPT)
  OFILES_LIB:=$(filter \
    mid/lib/% \
    mid/opt/fs/% \
    mid/opt/serial/% \
    mid/opt/fakews/% \
    mid/opt/png/% \
    mid/opt/glx/% \
    mid/opt/drm/% \
    mid/opt/pulse/% \
    mid/opt/alsa/% \
    mid/opt/evdev/% \
    mid/opt/macwm/% \
    mid/opt/macaudio/% \
    mid/opt/machid/% \
    mid/opt/mswm/% \
    mid/opt/msaudio/% \
    mid/opt/mshid/% \
  ,$(OFILES))
  $(LIB):$(OFILES_LIB);$(PRECMD) $(AR) $@ $^
  $(LIB_CONFIG_SCRIPT):etc/tool/genehcfg.sh; \
    CFLAGS="$(LIB_CFLAGS)" \
    LDFLAGS="$(LIB_LDFLAGS)" \
    LIBS="$(LDPOST)" \
    EHLIB="$(abspath $(LIB))" \
    INC="$(abspath out/include)" \
    $< $@
  #TODO Name all public headers here.
  LIB_HEADERS_SRC:=src/lib/emuhost.h src/lib/eh_inmgr.h
  LIB_HEADERS_DST:=$(patsubst src/lib/%,out/include/%,$(LIB_HEADERS_SRC))
  all:$(LIB_HEADERS_DST)
  out/include/%:src/lib/%;$(PRECMD) cp $< $@
endif

ifneq (,$(strip $(BUILD_WEB)))
  #TODO What do we do to build the web app?
endif

ifneq (,$(strip $(BUILD_ROMASSIST)))
  EXE_ROMASSIST:=out/romassist$(EXESFX)
  all:$(EXE_ROMASSIST)
  OFILES_ROMASSIST:=$(filter \
    mid/romassist/% \
    mid/opt/db/% \
    mid/opt/http/% \
    mid/opt/serial/% \
    mid/opt/fs/% \
    mid/opt/png/% \
  ,$(OFILES))
  $(EXE_ROMASSIST):$(OFILES_ROMASSIST);$(PRECMD) $(LD) -o$@ $^ $(LDPOST)
  ifeq (,$(strip $(BUILD_MENU)))
    run:run-bg
    run-bg:$(EXE_ROMASSIST);$(EXE_ROMASSIST) --dbroot=$(PWD)/data --htdocs=$(PWD)/src/www --no-update
  else
    run:$(EXE_ROMASSIST);$(EXE_ROMASSIST) --dbroot=$(PWD)/data --htdocs=$(PWD)/src/www --menu=$(PWD)/out/romassist-menu$(EXESFX) --poweroff=0 --no-update --public-port=8080
    run-bg:$(EXE_ROMASSIST);$(EXE_ROMASSIST) --dbroot=$(PWD)/data --htdocs=$(PWD)/src/www --no-update
    run-kiosk:$(EXE_ROMASSIST) out/romassist-menu$(EXESFX);$(EXE_ROMASSIST) --dbroot=$(PWD)/data --htdocs=$(PWD)/src/www --menu='$(PWD)/out/romassist-menu$(EXESFX) --kiosk --allow-quit-button=0' --no-update --public-port=8080 
  endif
endif
serve:run-bg

ifneq (,$(strip $(BUILD_MENU)))
  EXE_MENU:=out/romassist-menu$(EXESFX)
  all:$(EXE_MENU)
  OFILES_MENU:=$(OFILES_LIB) $(filter \
    mid/menu/% \
    mid/opt/gui/% \
    mid/opt/cheapsynth/% \
  ,$(OFILES))
  $(EXE_MENU):$(OFILES_MENU);$(PRECMD) $(LD) -o$@ $^ $(LDPOST)
  run:$(EXE_MENU)
endif

# Some helpers to build the emulators dependent on Emuhost...

EMULATORS:=akfceu aksnes9x akgambatte akz26 akprosys
emulators:$(addprefix emu-,$(EMULATORS))
emu-%:$(LIB) $(LIB_HEADERS_DST);rm -f ../$*/out/$* ; make -C../$*

#nes:emu-akfceu run
nes:emu-akfceu;make -C../akfceu run

endif
