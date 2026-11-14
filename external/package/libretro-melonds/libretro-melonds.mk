################################################################################
#
# libretro-melonds
#
################################################################################

LIBRETRO_MELONDS_VERSION = 66b5d2634cd0a79030562811e6e05f5532f800ba
LIBRETRO_MELONDS_SITE = $(call github,libretro,melonDS,$(LIBRETRO_MELONDS_VERSION))
LIBRETRO_MELONDS_LICENSE = GPL-3.0
LIBRETRO_MELONDS_DEPENDENCIES = mesa3d

define LIBRETRO_MELONDS_BUILD_CMDS
	$(TARGET_MAKE_ENV) CFLAGS="$(TARGET_CFLAGS)" CXXFLAGS="$(TARGET_CXXFLAGS)" LDFLAGS="$(TARGET_LDFLAGS)" $(MAKE) -C $(@D) platform=unix CC="$(TARGET_CC)" CXX="$(TARGET_CXX)" AR="$(TARGET_AR)" LD="$(TARGET_CXX)"
endef

define LIBRETRO_MELONDS_INSTALL_TARGET_CMDS
	$(INSTALL) -D -m 0644 $(@D)/melonds_libretro.so $(TARGET_DIR)/usr/lib/libretro/melonds_libretro.so
endef

$(eval $(generic-package))
