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
  all:$(LIB)
  #TODO Name all opt units here that lib might need (safe to name ones that aren't included).
  OFILES_LIB:=$(filter \
    mid/lib/% \
  ,$(OFILES))
  $(LIB):$(OFILES_LIB);$(PRECMD) $(AR) $@ $^
  #TODO Name all public headers here.
  LIB_HEADERS_SRC:=src/lib/emuhost.h
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
    mid/opt/serial/% \
    mid/opt/fs/% \
  ,$(OFILES))
  $(EXE_ROMASSIST):$(OFILES_ROMASSIST);$(PRECMD) $(LD) -o$@ $^ $(LDPOST)
  run:$(EXE_ROMASSIST);$(EXE_ROMASSIST)
endif

ifneq (,$(strip $(BUILD_MENU)))
  EXE_MENU:=out/romassist-menu$(EXESFX)
  all:$(EXE_MENU)
  OFILES_MENU:=$(filter \
    mid/menu/% \
    mid/lib/% \
  ,$(OFILES))
  $(EXE_MENU):$(OFILES_MENU);$(PRECMD) $(LD) -o$@ $^ $(LDPOST)
  run:$(EXE_MENU)
endif

endif
