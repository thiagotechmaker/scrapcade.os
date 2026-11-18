################################################################################
#
# emutos-bin: TOS livre (GPL-2.0) para o Atari ST, imagem oficial de 256 KB em ingles (nao ha portugues)
#
################################################################################

EMUTOS_BIN_VERSION = 1.4
EMUTOS_BIN_SOURCE = emutos-256k-$(EMUTOS_BIN_VERSION).zip
EMUTOS_BIN_SITE = https://downloads.sourceforge.net/project/emutos/emutos/$(EMUTOS_BIN_VERSION)
EMUTOS_BIN_LICENSE = GPL-2.0

define EMUTOS_BIN_EXTRACT_CMDS
	cd $(@D) && unzip -oq $(EMUTOS_BIN_DL_DIR)/$(EMUTOS_BIN_SOURCE)
endef

# o tos-jogar copia para a pasta de BIOS como tos.img quando o usuario nao tem o TOS original
define EMUTOS_BIN_INSTALL_TARGET_CMDS
	$(INSTALL) -D -m 0644 $(@D)/emutos-256k-$(EMUTOS_BIN_VERSION)/etos256us.img $(TARGET_DIR)/usr/share/tos/emutos/tos.img
	$(INSTALL) -D -m 0644 $(@D)/emutos-256k-$(EMUTOS_BIN_VERSION)/doc/license.txt $(TARGET_DIR)/usr/share/tos/emutos/license.txt
endef

$(eval $(generic-package))
