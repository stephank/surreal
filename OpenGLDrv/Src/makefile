#=============================================================================
# Unreal OpenGLDrv makefile for Linux.
#
# Revision history:
# 	* Created by Stéphan Kochen (based on SDLDrv).
# 
#=============================================================================

# Unreal directory. (Required by makefile-header.)
UNREAL_DIR	= ../..

# Include global definitions.
include $(UNREAL_DIR)/makefile-header

#-----------------------------------------------------------------------------
# Directories.
#-----------------------------------------------------------------------------

SRC_DIRS	= $(OPENGLDRV_SRC)
BUILD_DIR	= $(OPENGLDRV_BUILD)

#-----------------------------------------------------------------------------
# Compiler/linker options.
#-----------------------------------------------------------------------------

CXX		= $(STD_CXX)
CXXFLAGS	= $(STD_CXXFLAGS) $(SDL_CFLAGS) -Werror -fPIC -I$(OPENGLDRV_INC) \
			-I$(ENGINE_INC) -I$(CORE_INC) \
			-DGPackage=GPackageOpenGLDrv
LDFLAGS		= $(STD_LDFLAGS) -shared -Wl,-soname,$(OPENGLDRV_SONAME)
LIBS		= $(STD_LIBS) $(CORE) $(SDL_LIBS)

#-----------------------------------------------------------------------------
# Files.
#-----------------------------------------------------------------------------

OBJS =			\
	OpenGL.o	\
	OpenGLDrv.o

OUT = $(OPENGLDRV)

#-----------------------------------------------------------------------------
# Rules.
#-----------------------------------------------------------------------------

# Use general rules.
include $(UNREAL_DIR)/makefile-general

#-----------------------------------------------------------------------------
# The End.
#-----------------------------------------------------------------------------
