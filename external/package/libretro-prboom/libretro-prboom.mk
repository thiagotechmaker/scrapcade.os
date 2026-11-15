################################################################################
#
# libretro-prboom
#
################################################################################

LIBRETRO_PRBOOM_VERSION = 4849268e7342494193f2bbf2d0128a751cf58da7
LIBRETRO_PRBOOM_SITE = $(call github,libretro,libretro-prboom,$(LIBRETRO_PRBOOM_VERSION))
LIBRETRO_PRBOOM_LICENSE = GPL-2.0+

define LIBRETRO_PRBOOM_BUILD_CMDS
	$(TARGET_MAKE_ENV) CFLAGS="$(TARGET_CFLAGS)" LDFLAGS="$(TARGET_LDFLAGS)" \
		$(MAKE) -C $(@D) -f Makefile platform=unix CC="$(TARGET_CC)" AR="$(TARGET_AR)"
endef

# prboom.wad: dados do proprio PrBoom (menus, fontes), copiados para a pasta de BIOS na primeira vez
define LIBRETRO_PRBOOM_INSTALL_TARGET_CMDS
	$(INSTALL) -D -m 0644 $(@D)/prboom_libretro.so $(TARGET_DIR)/usr/lib/libretro/prboom_libretro.so
	$(INSTALL) -D -m 0644 $(@D)/prboom.wad $(TARGET_DIR)/usr/share/tos/prboom.wad
endef

$(eval $(generic-package))
