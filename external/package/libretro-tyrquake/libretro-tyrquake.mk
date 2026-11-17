################################################################################
#
# libretro-tyrquake
#
################################################################################

LIBRETRO_TYRQUAKE_VERSION = e57bb11597e8a00380f30f2627d219da960cf69a
LIBRETRO_TYRQUAKE_SITE = $(call github,libretro,tyrquake,$(LIBRETRO_TYRQUAKE_VERSION))
LIBRETRO_TYRQUAKE_LICENSE = GPL-2.0

define LIBRETRO_TYRQUAKE_BUILD_CMDS
	$(TARGET_MAKE_ENV) CFLAGS="$(TARGET_CFLAGS)" CXXFLAGS="$(TARGET_CXXFLAGS)" LDFLAGS="$(TARGET_LDFLAGS)" \
		$(MAKE) -C $(@D) platform=unix \
		CC="$(TARGET_CC)" CXX="$(TARGET_CXX)" AR="$(TARGET_AR)" LD="$(TARGET_CXX)"
endef

define LIBRETRO_TYRQUAKE_INSTALL_TARGET_CMDS
	$(INSTALL) -D -m 0644 $(@D)/tyrquake_libretro.so $(TARGET_DIR)/usr/lib/libretro/tyrquake_libretro.so
endef

$(eval $(generic-package))
