################################################################################
#
# libretro-mupen64plus
#
################################################################################

LIBRETRO_MUPEN64PLUS_VERSION = 15bada85906593376e8fd697f1b0b3af3a20f512
LIBRETRO_MUPEN64PLUS_SITE = $(call github,libretro,mupen64plus-libretro-nx,$(LIBRETRO_MUPEN64PLUS_VERSION))
LIBRETRO_MUPEN64PLUS_LICENSE = GPL-2.0
LIBRETRO_MUPEN64PLUS_DEPENDENCIES = mesa3d host-nasm zlib libpng

define LIBRETRO_MUPEN64PLUS_BUILD_CMDS
	$(TARGET_MAKE_ENV) CFLAGS="$(TARGET_CFLAGS)" CXXFLAGS="$(TARGET_CXXFLAGS)" LDFLAGS="$(TARGET_LDFLAGS)" \
		$(MAKE) -C $(@D)/. -f Makefile platform=unix \
		CC="$(TARGET_CC)" CXX="$(TARGET_CXX)" AR="$(TARGET_AR)" LD="$(TARGET_CXX)" 
endef

define LIBRETRO_MUPEN64PLUS_INSTALL_TARGET_CMDS
	$(INSTALL) -D -m 0644 $(@D)/./mupen64plus_next_libretro.so $(TARGET_DIR)/usr/lib/libretro/mupen64plus_next_libretro.so
endef

$(eval $(generic-package))
