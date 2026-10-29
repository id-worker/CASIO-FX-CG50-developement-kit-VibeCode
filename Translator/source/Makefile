.SUFFIXES:

ifeq ($(strip $(FXCGSDK)),)
export FXCGSDK := $(HOME)/PrizmSDK
endif

include $(FXCGSDK)/toolchain/prizm_rules

TARGET   := Translator
BUILD    := build
SOURCES  := src
INCLUDES :=

MKG3AFLAGS := -V 01.11 -n basic:Translator -n internal:TRANS \
              -i uns:../unselected.bmp -i sel:../selected.bmp

CFLAGS   = -std=gnu99 -Os -Wall $(MACHDEP) $(INCLUDE) -ffunction-sections \
           -fdata-sections -DMINIZ_NO_STDIO -DMINIZ_NO_TIME -DMINIZ_NO_ZLIB_APIS -DMINIZ_NO_MALLOC \
           -DMINIZ_NO_ARCHIVE_APIS -DMINIZ_NO_ARCHIVE_WRITING_APIS -DNDEBUG
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

$(OUTPUT).g3a: $(OUTPUT).bin
	$(MKG3A) $(MKG3AFLAGS) $< $@
	python3 ../tools/fix_g3a.py $@
$(OUTPUT).bin: $(OFILES)

-include $(DEPENDS)

endif
