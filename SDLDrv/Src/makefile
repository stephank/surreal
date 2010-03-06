#=============================================================================
# Unreal SDLDrv makefile for Linux.
#
# Revision history:
# 	* Created by Daniel Vogel (based on XDrv).
# 
#=============================================================================

# Unreal directory. (Required by makefile-header.)
UNREAL_DIR	= ../..

# Include global definitions.
include $(UNREAL_DIR)/makefile-header

#-----------------------------------------------------------------------------
# Directories.
#-----------------------------------------------------------------------------

SRC_DIRS	= $(SDLDRV_SRC)
BUILD_DIR	= $(SDLDRV_BUILD)

#-----------------------------------------------------------------------------
# Compiler/linker options.
#-----------------------------------------------------------------------------

CXX		= $(STD_CXX)
CXXFLAGS	= $(STD_CXXFLAGS) $(SDL_CFLAGS) -Werror -fPIC -I$(SDLDRV_INC) \
			-I$(ENGINE_INC) -I$(CORE_INC) \
			-DGPackage=GPackageSDLDrv
LDFLAGS		= $(STD_LDFLAGS) -shared -Wl,-soname,$(SDLDRV_SONAME)
LIBS		= $(STD_LIBS) $(CORE) $(SDL_LIBS)

#-----------------------------------------------------------------------------
# Files.
#-----------------------------------------------------------------------------

OBJS =			\
	SDLClient.o	\
	SDLDrv.o		\
	SDLViewport.o

OUT = $(SDLDRV)

#-----------------------------------------------------------------------------
# Rules.
#-----------------------------------------------------------------------------

# Use general rules.
include $(UNREAL_DIR)/makefile-general

#-----------------------------------------------------------------------------
# The End.
#-----------------------------------------------------------------------------
