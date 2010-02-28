#=============================================================================
# Unreal SDLGLDrv makefile for Linux.
#
# Revision history:
# 	* Created by Daniel Vogel (based on XMesaGLDrv) 
#
#=============================================================================

# Unreal directory. (Required by makefile-header.)
UNREAL_DIR	= ../..

# Include global definitions.
include $(UNREAL_DIR)/makefile-header

#-----------------------------------------------------------------------------
# Directories.
#-----------------------------------------------------------------------------

SRC_DIRS	= $(SDLGLDRV_SRC)
BUILD_DIR	= $(SDLGLDRV_BUILD)

#-----------------------------------------------------------------------------
# Compiler/linker options.
#-----------------------------------------------------------------------------

CXX		= $(STD_CXX)
CXXFLAGS	= $(STD_CXXFLAGS) -Werror -fPIC -I$(RENDER_INC) \
			-I$(ENGINE_INC) -I$(CORE_INC) $(XINCLUDES) \
			-DGPackage=GPackageSDLGLDrv
LDFLAGS		= $(STD_LDFLAGS) -shared -Wl,-soname,$(SDLGLDRV_SONAME)
LIBS		= $(STD_LIBS) $(CORE) $(SDL_LIBS)

#-----------------------------------------------------------------------------
# Files.
#-----------------------------------------------------------------------------

OBJS =			\
	SDLGL.o	\
	SDLGLDrv.o

OUT = $(SDLGLDRV)

#-----------------------------------------------------------------------------
# Rules.
#-----------------------------------------------------------------------------

# Use general rules.
include $(UNREAL_DIR)/makefile-general

#-----------------------------------------------------------------------------
# The End.
#-----------------------------------------------------------------------------
