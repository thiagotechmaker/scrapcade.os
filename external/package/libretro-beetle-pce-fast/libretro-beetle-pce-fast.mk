################################################################################
#
# libretro-beetle-pce-fast
#
################################################################################

LIBRETRO_BEETLE_PCE_FAST_VERSION = b211204c7026dff6e86e79b00185512e2421fff8
LIBRETRO_BEETLE_PCE_FAST_SITE = $(call github,libretro,beetle-pce-fast-libretro,$(LIBRETRO_BEETLE_PCE_FAST_VERSION))
LIBRETRO_BEETLE_PCE_FAST_LICENSE = GPL-2.0

define LIBRETRO_BEETLE_PCE_FAST_BUILD_CMDS
	$(TARGET_MAKE_ENV) CFLAGS="$(TARGET_CFLAGS)" CXXFLAGS="$(TARGET_CXXFLAGS)" LDFLAGS="$(TARGET_LDFLAGS)" \
		$(MAKE) -C $(@D) platform=unix \
		CC="$(TARGET_CC)" CXX="$(TARGET_CXX)" AR="$(TARGET_AR)" LD="$(TARGET_CXX)"
endef

define LIBRETRO_BEETLE_PCE_FAST_INSTALL_TARGET_CMDS
	$(INSTALL) -D -m 0644 $(@D)/mednafen_pce_fast_libretro.so $(TARGET_DIR)/usr/lib/libretro/mednafen_pce_fast_libretro.so
endef

$(eval $(generic-package))
