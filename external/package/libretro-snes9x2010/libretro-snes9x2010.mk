################################################################################
#
# libretro-snes9x2010
#
################################################################################

LIBRETRO_SNES9X2010_VERSION = fe690dd321fa5a46b5234a2bde089d2518c62b0e
LIBRETRO_SNES9X2010_SITE = $(call github,libretro,snes9x2010,$(LIBRETRO_SNES9X2010_VERSION))
LIBRETRO_SNES9X2010_LICENSE = Snes9x (nao comercial)

define LIBRETRO_SNES9X2010_BUILD_CMDS
	$(TARGET_MAKE_ENV) CFLAGS="$(TARGET_CFLAGS)" CXXFLAGS="$(TARGET_CXXFLAGS)" LDFLAGS="$(TARGET_LDFLAGS)" \
		$(MAKE) -C $(@D) -f Makefile platform=unix \
		CC="$(TARGET_CC)" CXX="$(TARGET_CXX)" AR="$(TARGET_AR)" LD="$(TARGET_CC)"
endef

define LIBRETRO_SNES9X2010_INSTALL_TARGET_CMDS
	$(INSTALL) -D -m 0644 $(@D)/snes9x2010_libretro.so $(TARGET_DIR)/usr/lib/libretro/snes9x2010_libretro.so
endef

$(eval $(generic-package))
