################################################################################
#
# libretro-hatari
#
################################################################################

LIBRETRO_HATARI_VERSION = 92e2874e664be40e1b51b97e0ff925a1aa917f22
LIBRETRO_HATARI_SITE = $(call github,libretro,hatari,$(LIBRETRO_HATARI_VERSION))
LIBRETRO_HATARI_LICENSE = GPL-2.0
LIBRETRO_HATARI_DEPENDENCIES = zlib

define LIBRETRO_HATARI_BUILD_CMDS
	$(TARGET_MAKE_ENV) CFLAGS="$(TARGET_CFLAGS)" CXXFLAGS="$(TARGET_CXXFLAGS)" LDFLAGS="$(TARGET_LDFLAGS)" \
		$(MAKE) -C $(@D) platform=unix -f Makefile.libretro \
		CC="$(TARGET_CC)" CXX="$(TARGET_CXX)" AR="$(TARGET_AR)" LD="$(TARGET_CXX)"
endef

define LIBRETRO_HATARI_INSTALL_TARGET_CMDS
	$(INSTALL) -D -m 0644 $(@D)/hatari_libretro.so $(TARGET_DIR)/usr/lib/libretro/hatari_libretro.so
endef

$(eval $(generic-package))
