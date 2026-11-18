################################################################################
#
# libretro-gearcoleco
#
################################################################################

LIBRETRO_GEARCOLECO_VERSION = 288004dd6405d4efcce35973fbbea3bbebd0ad0c
LIBRETRO_GEARCOLECO_SITE = $(call github,drhelius,Gearcoleco,$(LIBRETRO_GEARCOLECO_VERSION))
LIBRETRO_GEARCOLECO_LICENSE = GPL-3.0

define LIBRETRO_GEARCOLECO_BUILD_CMDS
	$(TARGET_MAKE_ENV) CFLAGS="$(TARGET_CFLAGS)" CXXFLAGS="$(TARGET_CXXFLAGS)" LDFLAGS="$(TARGET_LDFLAGS)" \
		$(MAKE) -C $(@D)/platforms/libretro platform=unix \
		CC="$(TARGET_CC)" CXX="$(TARGET_CXX)" AR="$(TARGET_AR)" LD="$(TARGET_CXX)"
endef

define LIBRETRO_GEARCOLECO_INSTALL_TARGET_CMDS
	$(INSTALL) -D -m 0644 $(@D)/platforms/libretro/gearcoleco_libretro.so $(TARGET_DIR)/usr/lib/libretro/gearcoleco_libretro.so
endef

$(eval $(generic-package))
