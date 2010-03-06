#=============================================================================
# Unreal dedicated server makefile for Linux.
#
# Revision history:
# 	* Created by Mike Danylchuk
#=============================================================================

# Unreal directory. (Required by makefile-header.)
UNREAL_DIR	= .

# Include global definitions.
include $(UNREAL_DIR)/makefile-header

# No default sub-make arguments.
ARGS =

#-----------------------------------------------------------------------------
# Rules.
#-----------------------------------------------------------------------------

.PHONY : all
ifeq ($(TARGETTYPE),linux)
#all : core engine ipdrv fire render ucc uweb audio
all : $(TMPDIR)/libstdc++.a \
	sdllaunch sdldrv opengldrv ucc
endif

# Need to statically link libstdc++.
# To force this, ask g++ where the static libstdc++ is, and symlink
# it into TMPDIR. Then TMPDIR is included in the linker's search path,
# where it won't find a shared version.
$(TMPDIR)/libstdc++.a :
	mkdir -p $(TMPDIR)
	ln -s `$(STD_CXX) -print-file-name=libstdc++.a` $@

%.so :
	@echo
	@echo "	Source code for '$@' is not included."
	@echo "	Copy the '$@' file from your Unreal Tournament"
	@echo "	installation, and then try running make again."
	@echo
	@exit 1

.PHONY : core
core : $(CORE)

.PHONY : engine
engine : $(ENGINE)

.PHONY : ipdrv
ipdrv : $(IPDRV)

.PHONY : fire
fire : $(FIRE)

.PHONY : editor
editor : $(EDITOR)

.PHONY : ucc
ucc : core engine
	@$(MAKE) $(ARGS) --directory=$(UCC_SRC)

.PHONY : render
render : $(RENDER)

.PHONY : sdllaunch
sdllaunch : core engine
	@$(MAKE) $(ARGS) --directory=$(SDLLAUNCH_SRC)

.PHONY : sdldrv
sdldrv : core engine
	@$(MAKE) $(ARGS) --directory=$(SDLDRV_SRC)

.PHONY : nulldrv
nulldrv : core engine
	@$(MAKE) $(ARGS) --directory=$(NULLDRV_SRC)

.PHONY : opengldrv
opengldrv : core engine render
	@$(MAKE) $(ARGS) --directory=$(OPENGLDRV_SRC)

.PHONY : galaxy
galaxy : $(GALAXY)

.PHONY : audio
audio :
	@$(MAKE) $(ARGS) --directory=$(AUDIO_SRC)

.PHONY : sdlaudio
sdlaudio :
	@$(MAKE) $(ARGS) --directory=$(SDLAUDIO_SRC)

.PHONY : uweb
uweb : $(UWEB)

.PHONY : nullrender
nullrender : $(NULLRENDER)

.PHONY : nullnetdriver
nullnetdriver : $(NULLNETDRIVER)

#-----------------------------------------------------------------------------
# Maintenance.
#-----------------------------------------------------------------------------

# Backup source code.
.PHONY : backup
backup :
	@-mkdir -p $(BACKUP_DIR)
	@tar zcvf $(BACKUP_DIR)/ucc-$(VERSION)-`date +%y%m%d`.tar.gz \
		$(ALL_SRC_DIRS) [Mm]akefile*

# Create tags table.
.PHONY : tags
tags : 
	@etags --c++ $(ALL_TAGS)

# Pass custom targets to module makefiles.
.DEFAULT : 
	@$(MAKE) ARGS=$@ --no-print-directory

#-----------------------------------------------------------------------------
# The End.
#-----------------------------------------------------------------------------
