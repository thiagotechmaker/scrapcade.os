################################################################################
#
# scrapcade
#
################################################################################

SCRAPCADE_VERSION = 0.1
SCRAPCADE_SITE = $(BR2_EXTERNAL_TOS_PATH)/package/scrapcade/src
SCRAPCADE_SITE_METHOD = local
SCRAPCADE_LICENSE = GPL-3.0-or-later

define SCRAPCADE_BUILD_CMDS
	$(TARGET_CC) $(TARGET_CFLAGS) -O2 -Wall -o $(@D)/scrapcade $(@D)/scrapcade.c -lm
endef

define SCRAPCADE_INSTALL_TARGET_CMDS
	$(INSTALL) -D -m 0755 $(@D)/scrapcade $(TARGET_DIR)/usr/bin/scrapcade
	$(INSTALL) -D -m 0755 $(BR2_EXTERNAL_TOS_PATH)/package/scrapcade/S99scrapcade \
		$(TARGET_DIR)/etc/init.d/S99scrapcade
endef

$(eval $(generic-package))
