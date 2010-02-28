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
ifeq ($(TARGETTYPE),psx2)
all : core engine fire nullnetdriver ucc render \
	nulldrv psx2drv psx2render psx2audio ipdrv psx2ilinkdrv psxlaunch
endif
ifeq ($(TARGETTYPE),linux)
#all : core engine ipdrv fire render ucc xdrv xlaunch xmesagldrv glidedrv \
#	uweb audio
all : libstdc++.a xlaunch sdldrv opengldrv
endif

# Need to statically link libstdc++.
# To force this, ask g++ where the static libstdc++ is, and symlink
# it into UNREAL_DIR. Then UNREAL_DIR is included in the linker's
# search path, where it won't find a shared version.
libstdc++.a :
	ln -s `$(STD_CXX) -print-file-name=$@` $(UNREAL_DIR)/$@

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
ucc : core engine nullnetdriver psx2ilinkdrv
	@$(MAKE) $(ARGS) --directory=$(UCC_SRC)

.PHONY : render
render : $(RENDER)

.PHONY : xlaunch
xlaunch : core engine
	@$(MAKE) $(ARGS) --directory=$(XLAUNCH_SRC)

.PHONY : psxlaunch
psxlaunch : $(PSXLAUNCH)

.PHONY : xdrv
xdrv : core engine
	@$(MAKE) $(ARGS) --directory=$(XDRV_SRC)

.PHONY : sdldrv
sdldrv : core engine
	@$(MAKE) $(ARGS) --directory=$(SDLDRV_SRC)

.PHONY : nulldrv
nulldrv : core engine
	@$(MAKE) $(ARGS) --directory=$(NULLDRV_SRC)

.PHONY : psx2drv
psx2drv : $(PSX2DRV)

.PHONY : xmesagldrv
xmesagldrv : core engine render
	@$(MAKE) $(ARGS) --directory=$(XMESAGLDRV_SRC)

.PHONY : glidedrv
glidedrv : core engine render
	@$(MAKE) $(ARGS) --directory=$(GLIDEDRV_SRC)

.PHONY : opengldrv
opengldrv : core engine render
	@$(MAKE) $(ARGS) --directory=$(OPENGLDRV_SRC)

.PHONY : galaxy
galaxy : $(GALAXY)

.PHONY : audio
audio :
	@$(MAKE) $(ARGS) --directory=$(AUDIO_SRC)

.PHONY : uweb
uweb : $(UWEB)

.PHONY : nullrender
nullrender : $(NULLRENDER)

.PHONY : psx2render
psx2render : $(PSX2RENDER)

.PHONY : psx2audio
psx2audio : $(PSX2AUDIO)

.PHONY : nullnetdriver
nullnetdriver : $(NULLNETDRIVER)

.PHONY : psx2ilinkdrv
psx2ilinkdrv : $(PSX2ILINKDRV)

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
