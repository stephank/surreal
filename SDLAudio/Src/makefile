#=============================================================================
# Unreal SDL Audio makefile for Linux.
#
# Revision history:
# 	* Created by Jeremy Muhlich, based on GenericAudio makefile
#============================================================================= 

# Unreal directory. (Required by makefile-header.)
UNREAL_DIR	= ../..

# Include global definitions.
include $(UNREAL_DIR)/makefile-header

#-----------------------------------------------------------------------------
# Directories.
#-----------------------------------------------------------------------------

SRC_DIRS	= $(SDLAUDIO_SRC)
BUILD_DIR	= $(SDLAUDIO_BUILD)

#-----------------------------------------------------------------------------
# Compiler/linker options.
#-----------------------------------------------------------------------------

CXX		= $(STD_CXX)
CXXFLAGS	= $(STD_CXXFLAGS) -Werror -fPIC -I$(SDLAUDIO_INC) -I$(ENGINE_INC) \
			-I$(CORE_INC) -I/usr/local/include -I/usr/local/include/SDL \
			-DGPackage=GPackageSDLAudio
LDFLAGS		= $(STD_LDFLAGS) -shared -Wl,-soname,$(SDLAUDIO_SONAME) \
			-L$(SDLAUDIO_SRC)
LIBS		= $(STD_LIBS) $(CORE) $(SDL_LIBS) -lSDL_mixer

#-----------------------------------------------------------------------------
# Files.
#-----------------------------------------------------------------------------

OBJS =				\
	SDLAudio.o			\
	SDLAudioSubsystem.o	\
	SDLAudioLibrary.o	\
	AudioCoreSDL.o

OUT = $(SDLAUDIO)

#-----------------------------------------------------------------------------
# Rules.
#-----------------------------------------------------------------------------

# Use general rules.
include $(UNREAL_DIR)/makefile-general

#-----------------------------------------------------------------------------
# The End.
#-----------------------------------------------------------------------------
