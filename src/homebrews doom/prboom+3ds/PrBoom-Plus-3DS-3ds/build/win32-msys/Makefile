#---------------------------------------------------------------------------------
.SUFFIXES:
#---------------------------------------------------------------------------------

#---------------------------------------------------------------------------------
# TARGET is the name of the output
# BUILD is the directory where object files & intermediate files will be placed
# SOURCES is a list of directories containing source code
# INCLUDES is a list of directories containing header files
#---------------------------------------------------------------------------------
TARGET		:=	dist/PrBoom-Plus
BUILD		:=	build

SOURCES		:=	../../src \
				../../src/Win32

INCLUDES	:=	../../src \
				../../src/Win32

#---------------------------------------------------------------------------------
# options for code generation
#---------------------------------------------------------------------------------
CFLAGS	:=	-m32 -mwindows -static -O3 -ffast-math -D_WIN32 -DGL_DOOM -DHAVE_DIRENT_H -DHAVE_LIBZ \
			-DPACKAGE_NAME=\"PrBoom-Plus\" -DPACKAGE_VERSION=\"2.6.2\" -DPACKAGE_TARNAME=\"prboom-plus\" -DPACKAGE_HOMEPAGE=\"https://example.com\" \
			-DPRBOOMDATADIR=\".\" -DDOOMWADDIR=\".\" `pkg-config --static --cflags sdl sdl_mixer zlib`

CXXFLAGS := $(CFLAGS) -fno-rtti -fno-exceptions

LIBS    := `pkg-config --static --libs sdl sdl_mixer zlib` -lxinput1_4 -lmad -lopengl32 -lcomctl32 -lstdc++

#---------------------------------------------------------------------------------
# no real need to edit anything past this point unless you need to add additional
# rules for different file extensions
#---------------------------------------------------------------------------------
VPATH   := $(foreach dir,$(SOURCES),$(CURDIR)/$(dir))

CFILES	 := $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.c)))
CPPFILES := $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.cpp)))

OFILES  := $(addprefix $(BUILD)/, $(CFILES:.c=.o)) $(addprefix $(BUILD)/, $(CPPFILES:.cpp=.o))

INCLUDE := $(foreach dir,$(INCLUDES),-I$(CURDIR)/$(dir))

.PHONY: all clean

#---------------------------------------------------------------------------------
all: $(BUILD) $(TARGET).exe

$(BUILD):
	@mkdir -p $@

#---------------------------------------------------------------------------------
clean:
	@echo clean...
	@rm -fr $(BUILD)

#---------------------------------------------------------------------------------
# main targets
#---------------------------------------------------------------------------------
$(TARGET).exe: $(OFILES)
	@echo linking $(notdir $@)
	@ $(CC) $(CFLAGS) -o $@ $(OFILES) $(LIBS) -Xlinker -Map=$(BUILD)/$(notdir $@.map)

$(BUILD)/%.o: %.c
	@echo $(notdir $<)
	@ $(CC) $(CFLAGS) -c $< -o $@ $(INCLUDE)

$(BUILD)/%.o: %.cpp
	@echo $(notdir $<)
	@ $(CXX) $(CXXFLAGS) -c $< -o $@ $(INCLUDE)
