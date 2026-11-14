################################################################################
#
# libretro-bsnes
#
################################################################################

LIBRETRO_BSNES_VERSION = 63ee22f7af8f5d1ada295c2c98fd7bf261fa62a7
LIBRETRO_BSNES_SITE = $(call github,libretro,bsnes,$(LIBRETRO_BSNES_VERSION))
LIBRETRO_BSNES_LICENSE = GPL-3.0
LIBRETRO_BSNES_DEPENDENCIES = 

define LIBRETRO_BSNES_BUILD_CMDS
	$(TARGET_MAKE_ENV) CFLAGS="$(TARGET_CFLAGS)" CXXFLAGS="$(TARGET_CXXFLAGS)" LDFLAGS="$(TARGET_LDFLAGS)" $(MAKE) -C $(@D)/bsnes target=libretro binary=library platform=linux CC="$(TARGET_CC)" CXX="$(TARGET_CXX)" AR="$(TARGET_AR)" compiler="$(TARGET_CXX)"
endef

define LIBRETRO_BSNES_INSTALL_TARGET_CMDS
	$(INSTALL) -D -m 0644 $$(find $(@D) -name bsnes_libretro.so | head -1) $(TARGET_DIR)/usr/lib/libretro/bsnes_libretro.so
endef

$(eval $(generic-package))
