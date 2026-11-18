################################################################################
#
# libretro-virtualjaguar
#
################################################################################

LIBRETRO_VIRTUALJAGUAR_VERSION = 59f7f9cc594ae6c6982c2bd8fc0dbf36873f9869
LIBRETRO_VIRTUALJAGUAR_SITE = $(call github,libretro,virtualjaguar-libretro,$(LIBRETRO_VIRTUALJAGUAR_VERSION))
LIBRETRO_VIRTUALJAGUAR_LICENSE = GPL-3.0

define LIBRETRO_VIRTUALJAGUAR_BUILD_CMDS
	$(TARGET_MAKE_ENV) CFLAGS="$(TARGET_CFLAGS)" CXXFLAGS="$(TARGET_CXXFLAGS)" LDFLAGS="$(TARGET_LDFLAGS)" \
		$(MAKE) -C $(@D) platform=unix \
		CC="$(TARGET_CC)" CXX="$(TARGET_CXX)" AR="$(TARGET_AR)" LD="$(TARGET_CXX)"
endef

define LIBRETRO_VIRTUALJAGUAR_INSTALL_TARGET_CMDS
	$(INSTALL) -D -m 0644 $(@D)/virtualjaguar_libretro.so $(TARGET_DIR)/usr/lib/libretro/virtualjaguar_libretro.so
endef

$(eval $(generic-package))
