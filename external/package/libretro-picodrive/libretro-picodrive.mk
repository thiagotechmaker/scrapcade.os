################################################################################
#
# libretro-picodrive
#
################################################################################

LIBRETRO_PICODRIVE_VERSION = 1890c2932234c9d30f4cd3851d02228baae8f09e
LIBRETRO_PICODRIVE_SITE = https://github.com/libretro/picodrive.git
LIBRETRO_PICODRIVE_SITE_METHOD = git
LIBRETRO_PICODRIVE_GIT_SUBMODULES = YES
LIBRETRO_PICODRIVE_LICENSE = MAME (nao comercial)

define LIBRETRO_PICODRIVE_BUILD_CMDS
	$(TARGET_MAKE_ENV) CFLAGS="$(TARGET_CFLAGS) -Wno-error=incompatible-pointer-types" CXXFLAGS="$(TARGET_CXXFLAGS)" LDFLAGS="$(TARGET_LDFLAGS)" \
		$(MAKE) -C $(@D) -f Makefile.libretro platform=unix \
		CC="$(TARGET_CC)" CXX="$(TARGET_CXX)" AR="$(TARGET_AR)" LD="$(TARGET_CC)"
endef

define LIBRETRO_PICODRIVE_INSTALL_TARGET_CMDS
	$(INSTALL) -D -m 0644 $(@D)/picodrive_libretro.so $(TARGET_DIR)/usr/lib/libretro/picodrive_libretro.so
endef

$(eval $(generic-package))
