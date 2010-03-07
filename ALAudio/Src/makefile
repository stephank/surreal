#=============================================================================
# Unreal OpenAL Audio makefile for Linux.
#
# Revision history:
# 	* Created by Stéphan Kochen (based on Audio).
#=============================================================================

# Unreal directory. (Required by makefile-header.)
UNREAL_DIR	= ../..

# Include global definitions.
include $(UNREAL_DIR)/makefile-header

#-----------------------------------------------------------------------------
# Directories.
#-----------------------------------------------------------------------------

SRC_DIRS	= $(ALAUDIO_SRC)
BUILD_DIR	= $(ALAUDIO_BUILD)

#-----------------------------------------------------------------------------
# Compiler/linker options.
#-----------------------------------------------------------------------------

CXX		= $(STD_CXX)
CXXFLAGS	= $(STD_CXXFLAGS) -Werror -fPIC -I$(ENGINE_INC) -I$(CORE_INC) \
			-DGPackage=GPackageALAudio
LDFLAGS		= $(STD_LDFLAGS) -shared -Wl,-soname,$(ALAUDIO_SONAME)
LIBS		= $(STD_LIBS) $(CORE) -lopenal -lalure

#-----------------------------------------------------------------------------
# Files.
#-----------------------------------------------------------------------------

OBJS =				\
	ALAudio.o			\
	ALAudioSubsystem.o

OUT = $(ALAUDIO)

#-----------------------------------------------------------------------------
# Rules.
#-----------------------------------------------------------------------------

# Use general rules.
include $(UNREAL_DIR)/makefile-general

#-----------------------------------------------------------------------------
# The End.
#-----------------------------------------------------------------------------
