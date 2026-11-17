################################################################################
#
# libretro-prosystem
#
################################################################################

LIBRETRO_PROSYSTEM_VERSION = 363b6dfbd3e240762e022c2b4897b4fe55722be3
LIBRETRO_PROSYSTEM_SITE = $(call github,libretro,prosystem-libretro,$(LIBRETRO_PROSYSTEM_VERSION))
LIBRETRO_PROSYSTEM_LICENSE = GPL-2.0

define LIBRETRO_PROSYSTEM_BUILD_CMDS
	$(TARGET_MAKE_ENV) CFLAGS="$(TARGET_CFLAGS)" CXXFLAGS="$(TARGET_CXXFLAGS)" LDFLAGS="$(TARGET_LDFLAGS)" \
		$(MAKE) -C $(@D) platform=unix \
		CC="$(TARGET_CC)" CXX="$(TARGET_CXX)" AR="$(TARGET_AR)" LD="$(TARGET_CXX)"
endef

define LIBRETRO_PROSYSTEM_INSTALL_TARGET_CMDS
	$(INSTALL) -D -m 0644 $(@D)/prosystem_libretro.so $(TARGET_DIR)/usr/lib/libretro/prosystem_libretro.so
endef

$(eval $(generic-package))
