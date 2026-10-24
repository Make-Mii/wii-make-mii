#---------------------------------------------------------------------------------
# Clear the implicit built in rules
#---------------------------------------------------------------------------------
.SUFFIXES:
#---------------------------------------------------------------------------------
ifeq ($(strip $(DEVKITPPC)),)
$(error "Please set DEVKITPPC in your environment. export DEVKITPPC=<path to>devkitPPC")
endif

include $(DEVKITPPC)/wii_rules

#---------------------------------------------------------------------------------
# TARGET is the name of the output
# BUILD is the directory where object files & intermediate files will be placed
# SOURCES is a list of directories containing source code
# INCLUDES is a list of directories containing extra header files
#---------------------------------------------------------------------------------
TARGET   := $(notdir $(CURDIR))
BUILD    := build
SOURCES  := source source/gfx
DATA     := data data/mii
INCLUDES := include source

#---------------------------------------------------------------------------------
# Library and Portlibs Setup
#---------------------------------------------------------------------------------
PORTLIBS ?= $(DEVKITPRO)/portlibs/wii
LIBOGC   ?= $(DEVKITPRO)/libogc
LIBDIRS  := $(PORTLIBS) $(LIBOGC)

#---------------------------------------------------------------------------------
# Compiler and Linker options
#---------------------------------------------------------------------------------
CFLAGS   := -g -O2 -Wall $(MACHDEP) \
            -IC:/wii_dev/rendermiiexample/source \
            -IC:/wii_dev/rendermiiexample/build \
            -I/opt/devkitpro/libogc/include \
            -I/opt/devkitpro/portlibs/wii/include \
            -I/opt/devkitpro/portlibs/wii/include/freetype2

CXXFLAGS := $(CFLAGS)
LDFLAGS  := -g $(MACHDEP) -Wl,-Map,$(notdir $@).map

#---------------------------------------------------------------------------------
# Any extra libraries to link (in dependency order)
#---------------------------------------------------------------------------------
LIBS := -lmii -lgrrlib -lpngu -lpng -ljpeg -lfreetype -lbrotlidec -lbrotlicommon -lbz2 -lfat -lz -lwiiuse -lbte -logc -lm

#---------------------------------------------------------------------------------
ifneq ($(BUILD),$(notdir $(CURDIR)))
#---------------------------------------------------------------------------------

export OUTPUT   := $(CURDIR)/$(TARGET)

export VPATH    := $(foreach dir,$(SOURCES),$(CURDIR)/$(dir)) \
                   $(foreach dir,$(DATA),$(CURDIR)/$(dir))

export DEPSDIR  := $(CURDIR)/$(BUILD)

# Automatically build list of files
CFILES   := $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.c)))
CPPFILES := $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.cpp)))
sFILES   := $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.s)))
SFILES   := $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.S)))
BINFILES := $(foreach dir,$(DATA),$(notdir $(wildcard $(dir)/*.*)))

ifeq ($(strip $(CPPFILES)),)
    export LD := $(CC)
else
    export LD := $(CXX)
endif

export OFILES := $(addsuffix .o,$(BINFILES)) \
                 $(CPPFILES:.cpp=.o) $(CFILES:.c=.o) \
                 $(sFILES:.s=.o) $(SFILES:.S=.o)

# Include paths exported to build environment
export INCLUDE := $(foreach dir,$(INCLUDES),-I$(CURDIR)/$(dir)) \
                  $(foreach dir,$(LIBDIRS),-I$(dir)/include) \
                  -I$(PORTLIBS)/include/freetype2 \
                  -I$(CURDIR)/$(BUILD) \
                  -I$(LIBOGC)/include

# Library paths exported to build environment
export LIBPATHS := $(foreach dir,$(LIBDIRS),-L$(dir)/lib) \
                   -L$(LIBOGC_LIB)

.PHONY: $(BUILD) clean run

#---------------------------------------------------------------------------------
$(BUILD):
	@[ -d $@ ] || mkdir -p $@
	@make --no-print-directory -C $(BUILD) -f $(CURDIR)/Makefile

#---------------------------------------------------------------------------------
clean:
	@echo clean ...
	@rm -fr $(BUILD) $(OUTPUT).elf $(OUTPUT).dol

#---------------------------------------------------------------------------------
run:
	wiiload $(TARGET).dol

#---------------------------------------------------------------------------------
else

DEPENDS := $(OFILES:.o=.d)

#---------------------------------------------------------------------------------
# Main targets
#---------------------------------------------------------------------------------
$(OUTPUT).dol: $(OUTPUT).elf
$(OUTPUT).elf: $(OFILES)

# Rules for embedding binary files
%.jpg.o : %.jpg
	@echo $(notdir $<)
	@$(bin2o)

%.png.o : %.png
	@echo $(notdir $<)
	@$(bin2o)

%.ttf.o : %.ttf
	@echo $(notdir $<)
	@$(bin2o)

-include $(DEPENDS)

#---------------------------------------------------------------------------------
endif
#---------------------------------------------------------------------------------