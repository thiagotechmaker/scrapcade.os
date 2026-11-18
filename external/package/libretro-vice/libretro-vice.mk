################################################################################
#
# libretro-vice
#
################################################################################

LIBRETRO_VICE_VERSION = c8c242db75a559246d6d51017e6dd4ecd75d6a9f
LIBRETRO_VICE_SITE = $(call github,libretro,vice-libretro,$(LIBRETRO_VICE_VERSION))
LIBRETRO_VICE_LICENSE = GPL-2.0

define LIBRETRO_VICE_BUILD_CMDS
	$(TARGET_MAKE_ENV) CFLAGS="$(TARGET_CFLAGS)" CXXFLAGS="$(TARGET_CXXFLAGS)" LDFLAGS="$(TARGET_LDFLAGS)" \
		$(MAKE) -C $(@D) platform=unix EMUTYPE=x64 \
		CC="$(TARGET_CC)" CXX="$(TARGET_CXX)" AR="$(TARGET_AR)" LD="$(TARGET_CXX)"
endef

define LIBRETRO_VICE_INSTALL_TARGET_CMDS
	$(INSTALL) -D -m 0644 $(@D)/vice_x64_libretro.so $(TARGET_DIR)/usr/lib/libretro/vice_x64_libretro.so
endef

$(eval $(generic-package))
