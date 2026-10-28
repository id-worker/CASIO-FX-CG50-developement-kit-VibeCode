#---------------------------------------------------------------------------------
# Wireworld for the CASIO fx-CG50 / Prizm  (g3a add-in)
#
#   make          -- build Wireworld.g3a
#   make clean    -- remove build artefacts
#
# Environment:
#   FXCGSDK       path to the PrizmSDK/libfxcg checkout (include/, lib/, toolchain/)
#   PATH          must contain sh3eb-elf-gcc and mkg3a
#
#   export FXCGSDK=$HOME/PrizmSDK
#   export PATH=$HOME/PrizmSDK/bin:$PATH     (Windows SDK)
#   export PATH=$PREFIX/bin:$PATH            (self built Linux toolchain)
#---------------------------------------------------------------------------------
.SUFFIXES:

ifeq ($(strip $(FXCGSDK)),)
export FXCGSDK := $(HOME)/PrizmSDK
endif

include $(FXCGSDK)/toolchain/prizm_rules

#---------------------------------------------------------------------------------
TARGET   := Wireworld
BUILD    := build
SOURCES  := src
INCLUDES :=

#---------------------------------------------------------------------------------
MKG3AFLAGS := -n basic:Wireworld -n internal:WIREWORLD -V 1.64 \
              -i uns:../unselected.bmp -i sel:../selected.bmp

CFLAGS   = -Os -Wall $(MACHDEP) $(INCLUDE) -ffunction-sections -fdata-sections
CXXFLAGS = $(CFLAGS) -fno-exceptions

LDFLAGS  = $(MACHDEP) -T$(FXCGSDK)/toolchain/prizm.x -Wl,-static -Wl,-gc-sections

GCCLIB   := $(shell $(CC) $(MACHDEP) -print-file-name=libgcc_be.a 2>/dev/null)
# A missing -print-file-name result is the literal "libgcc_be.a", not a path.
ifeq ($(GCCLIB),libgcc_be.a)
LIBS     := -Wl,--start-group -lc -lfxcg -lgcc -Wl,--end-group
else
LIBS     := -Wl,--start-group -lc -lfxcg -lgcc_be -Wl,--end-group
endif

#---------------------------------------------------------------------------------
ifneq ($(BUILD),$(notdir $(CURDIR)))
#---------------------------------------------------------------------------------

export OUTPUT  := $(CURDIR)/$(TARGET)
export VPATH   := $(foreach dir,$(SOURCES),$(CURDIR)/$(dir))
export DEPSDIR := $(CURDIR)/$(BUILD)

CFILES   := $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.c)))
CPPFILES := $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.cpp)))
sFILES   := $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.s)))
SFILES   := $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.S)))

ifeq ($(strip $(CPPFILES)),)
export LD := $(CC)
else
export LD := $(CXX)
endif

export OFILES := $(CPPFILES:.cpp=.o) $(CFILES:.c=.o) \
                 $(sFILES:.s=.o) $(SFILES:.S=.o)

export INCLUDE := $(foreach dir,$(INCLUDES),-iquote $(CURDIR)/$(dir)) \
                  -I$(CURDIR)/$(BUILD) -I$(LIBFXCG_INC)

export LIBPATHS := -L$(LIBFXCG_LIB)

.PHONY: all clean

all: $(BUILD)
	@make --no-print-directory -C $(BUILD) -f $(CURDIR)/Makefile

$(BUILD):
	@mkdir $@

clean:
	$(call rmdir,$(BUILD))
	$(call rm,$(OUTPUT).bin)
	$(call rm,$(OUTPUT).g3a)

#---------------------------------------------------------------------------------
else

DEPENDS := $(OFILES:.o=.d)

$(OUTPUT).g3a: $(OUTPUT).bin
	$(MKG3A) $(MKG3AFLAGS) $< $@
	python3 $(CURDIR)/../tools/fix_g3a.py $@

$(OUTPUT).bin: $(OFILES)

-include $(DEPENDS)

#---------------------------------------------------------------------------------
endif
#---------------------------------------------------------------------------------
