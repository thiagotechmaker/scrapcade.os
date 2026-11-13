################################################################################
#
# scrapcade
#
################################################################################

SCRAPCADE_VERSION = 0.7
SCRAPCADE_SITE = $(BR2_EXTERNAL_TOS_PATH)/package/scrapcade/src
SCRAPCADE_SITE_METHOD = local
SCRAPCADE_LICENSE = GPL-3.0-or-later
SCRAPCADE_DEPENDENCIES = sdl2 sdl2_ttf sdl2_image zlib libzip libcurl

define SCRAPCADE_BUILD_CMDS
	$(TARGET_CXX) $(TARGET_CXXFLAGS) -std=c++17 -O2 -Wall -D_REENTRANT \
		-I$(STAGING_DIR)/usr/include/SDL2 -o $(@D)/scrapcade $(@D)/scrapcade.cpp \
		$(TARGET_LDFLAGS) -lSDL2_ttf -lSDL2_image -lSDL2 -lcurl -lzip -lz -lpthread
endef

define SCRAPCADE_INSTALL_TARGET_CMDS
	$(INSTALL) -D -m 0755 $(@D)/scrapcade $(TARGET_DIR)/usr/bin/scrapcade
	$(INSTALL) -D -m 0755 $(BR2_EXTERNAL_TOS_PATH)/package/scrapcade/S99scrapcade \
		$(TARGET_DIR)/etc/init.d/S99scrapcade
	mkdir -p $(TARGET_DIR)/usr/share/scrapcade
	cp -r $(BR2_EXTERNAL_TOS_PATH)/package/scrapcade/data/. $(TARGET_DIR)/usr/share/scrapcade/
endef

$(eval $(generic-package))
