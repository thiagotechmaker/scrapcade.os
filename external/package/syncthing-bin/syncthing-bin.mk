################################################################################
#
# syncthing-bin (saves sincronizados entre os aparelhos; binario oficial do projeto)
#
################################################################################

SYNCTHING_BIN_VERSION = 2.1.7
ifeq ($(BR2_x86_64),y)
SYNCTHING_BIN_ARCH = amd64
else
SYNCTHING_BIN_ARCH = 386
endif
SYNCTHING_BIN_SOURCE = syncthing-linux-$(SYNCTHING_BIN_ARCH)-v$(SYNCTHING_BIN_VERSION).tar.gz
SYNCTHING_BIN_SITE = https://github.com/syncthing/syncthing/releases/download/v$(SYNCTHING_BIN_VERSION)
SYNCTHING_BIN_LICENSE = MPL-2.0

define SYNCTHING_BIN_INSTALL_TARGET_CMDS
	$(INSTALL) -D -m 0755 $(@D)/syncthing $(TARGET_DIR)/usr/bin/syncthing
endef

$(eval $(generic-package))
