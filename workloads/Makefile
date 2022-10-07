# This file copies the specified assets to $(OBJ_BASE)/assets.
# There, they can be found during the creation of the ROM.

PKGDIR		= ..
L4DIR		?= $(PKGDIR)/../..

# Required for $(SRC_DIR).
include $(L4DIR)/mk/Makeconf

# Effectively sets the destination subdirectory in the assets folder.
# Only "dtb" would have some special meaning.
ASSET_TYPE = ycsb
# The list of assets to copy.
INSTALL_TARGET = workloada.spec workloadb.spec workloadc.spec workloadd.spec \
                 workloade.spec workloadf.spec

# Unfortunately, it seems like we need to create the copy rule ourselves.
$(INSTALL_TARGET): % : $(SRC_DIR)/%
	@$(CP) $< $@

# Include the assets rules.
include $(L4DIR)/mk/assets.mk
