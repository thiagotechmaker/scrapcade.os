################################################################################
#
# libretro-neocd
#
################################################################################

LIBRETRO_NEOCD_VERSION = 331cb2c4aa3d8490ec83dacf7c3650f13d7c3bf5
LIBRETRO_NEOCD_SITE = $(call github,libretro,neocd_libretro,$(LIBRETRO_NEOCD_VERSION))
LIBRETRO_NEOCD_LICENSE = LGPL-3.0

define LIBRETRO_NEOCD_BUILD_CMDS
	$(TARGET_MAKE_ENV) CFLAGS="$(TARGET_CFLAGS)" CXXFLAGS="$(TARGET_CXXFLAGS)" LDFLAGS="$(TARGET_LDFLAGS)" \
		$(MAKE) -C $(@D) platform=unix \
		CC="$(TARGET_CC)" CXX="$(TARGET_CXX)" AR="$(TARGET_AR)" LD="$(TARGET_CXX)"
endef

define LIBRETRO_NEOCD_INSTALL_TARGET_CMDS
	$(INSTALL) -D -m 0644 $(@D)/neocd_libretro.so $(TARGET_DIR)/usr/lib/libretro/neocd_libretro.so
endef

$(eval $(generic-package))
