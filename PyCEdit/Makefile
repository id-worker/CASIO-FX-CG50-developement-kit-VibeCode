.SUFFIXES:

ifeq ($(strip $(FXCGSDK)),)
export FXCGSDK := $(HOME)/PrizmSDK
endif

include $(FXCGSDK)/toolchain/prizm_rules

# Light-only configuration. Reject obsolete theme arguments explicitly.
THEME ?= light
ifneq ($(THEME),light)
$(error PyCEdit is now light-only: use make or make THEME=light)
endif
TARGET := PyCEdit
BASIC_NAME := PyCEdit
INTERNAL_NAME := PYCEDIT
BUILD    := build-light
SOURCES  := src
INCLUDES :=

MKG3AFLAGS := -V 01.15 -n basic:$(BASIC_NAME) -n internal:$(INTERNAL_NAME) \
              -i uns:../unselected.bmp -i sel:../selected.bmp

CFLAGS   = -std=gnu99 -Os -Wall $(MACHDEP) $(INCLUDE) -ffunction-sections -fdata-sections
CXXFLAGS = $(CFLAGS) -fno-exceptions -fno-rtti
LDFLAGS  = $(MACHDEP) -T../prizm.x -Wl,-static -Wl,-gc-sections
GCCLIB   := $(shell $(CC) $(MACHDEP) -print-file-name=libgcc_be.a 2>/dev/null)
ifeq ($(GCCLIB),libgcc_be.a)
LIBS     := -Wl,--start-group -lc -lfxcg -lgcc -Wl,--end-group
else
LIBS     := -Wl,--start-group -lc -lfxcg -lgcc_be -Wl,--end-group
endif

ifneq ($(BUILD),$(notdir $(CURDIR)))

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

export OFILES := $(CPPFILES:.cpp=.o) $(CFILES:.c=.o) $(sFILES:.s=.o) $(SFILES:.S=.o)
export INCLUDE := $(foreach dir,$(INCLUDES),-iquote $(CURDIR)/$(dir)) \
                  -I$(CURDIR)/$(BUILD) -I$(LIBFXCG_INC)
export LIBPATHS := -L$(LIBFXCG_LIB)

.PHONY: all clean

all: $(BUILD)
	@$(MAKE) --no-print-directory -C $(BUILD) -f $(CURDIR)/Makefile

$(BUILD):
	@mkdir -p $@

clean:
	$(call rmdir,$(BUILD))
	$(call rm,$(OUTPUT).bin)
	$(call rm,$(OUTPUT).g3a)

else

DEPENDS := $(OFILES:.o=.d)

$(OUTPUT).g3a: $(OUTPUT).bin ../unselected.bmp ../selected.bmp ../tools/fix_g3a.py
	$(MKG3A) $(MKG3AFLAGS) $< $@
	python3 ../tools/fix_g3a.py $@ --icons ../unselected.bmp ../selected.bmp
$(OUTPUT).bin: $(OFILES)

-include $(DEPENDS)

endif
