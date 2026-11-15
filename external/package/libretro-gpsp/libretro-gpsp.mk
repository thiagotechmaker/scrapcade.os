################################################################################
#
# libretro-gpsp
#
################################################################################

LIBRETRO_GPSP_VERSION = 5819380c2ffb0900219d700a382ee68c464ebb99
LIBRETRO_GPSP_SITE = $(call github,libretro,gpsp,$(LIBRETRO_GPSP_VERSION))
LIBRETRO_GPSP_LICENSE = GPL-2.0

define LIBRETRO_GPSP_BUILD_CMDS
	$(TARGET_MAKE_ENV) CFLAGS="$(TARGET_CFLAGS)" CXXFLAGS="$(TARGET_CXXFLAGS)" LDFLAGS="$(TARGET_LDFLAGS)" \
		$(MAKE) -C $(@D) -f Makefile platform=unix \
		CC="$(TARGET_CC)" CXX="$(TARGET_CXX)" AR="$(TARGET_AR)" LD="$(TARGET_CC)"
endef

define LIBRETRO_GPSP_INSTALL_TARGET_CMDS
	$(INSTALL) -D -m 0644 $(@D)/gpsp_libretro.so $(TARGET_DIR)/usr/lib/libretro/gpsp_libretro.so
endef

$(eval $(generic-package))
