#=============================================================================
# Unreal Render makefile for Linux.
#
# Revision history:
# 	* Created by Brandon Reinhart 
#=============================================================================

# Unreal directory. (Required by makefile-header.)
UNREAL_DIR	= ../..

# Include global definitions.
include $(UNREAL_DIR)/makefile-header

#-----------------------------------------------------------------------------
# Directories.
#-----------------------------------------------------------------------------

SRC_DIRS	= $(GLIDEDRV_SRC)
BUILD_DIR	= $(GLIDEDRV_BUILD)

#-----------------------------------------------------------------------------
# Compiler/linker options.
#-----------------------------------------------------------------------------

CXX		= $(STD_CXX)
CXXFLAGS	= $(STD_CXXFLAGS) -Werror -fPIC $(GLIDEINC) -I$(RENDER_INC) \
			-I$(ENGINE_INC) -I$(CORE_INC) \
			-DGPackage=GPackageGlideDrv
LDFLAGS		= $(STD_LDFLAGS) -shared -Wl,-soname,$(GLIDEDRV_SONAME)
LIBS		= $(STD_LIBS) $(GLIDELIBS) $(XLIBS) $(CORE) $(RENDER) 

#-----------------------------------------------------------------------------
# Files.
#-----------------------------------------------------------------------------

OBJS =			\
	UnGlide.o

OUT = $(GLIDEDRV)

#-----------------------------------------------------------------------------
# Rules.
#-----------------------------------------------------------------------------

# Use general rules.
include $(UNREAL_DIR)/makefile-general

#-----------------------------------------------------------------------------
# The End.
#-----------------------------------------------------------------------------
