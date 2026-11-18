################################################################################
#
# libretro-opera
#
################################################################################

LIBRETRO_OPERA_VERSION = 636a8dd6340bce7073168ddefd29841fc5e2d213
LIBRETRO_OPERA_SITE = $(call github,libretro,opera-libretro,$(LIBRETRO_OPERA_VERSION))
LIBRETRO_OPERA_LICENSE = LGPL (com notas)

define LIBRETRO_OPERA_BUILD_CMDS
	$(TARGET_MAKE_ENV) CFLAGS="$(TARGET_CFLAGS)" CXXFLAGS="$(TARGET_CXXFLAGS)" LDFLAGS="$(TARGET_LDFLAGS)" \
		$(MAKE) -C $(@D) platform=unix \
		CC="$(TARGET_CC)" CXX="$(TARGET_CXX)" AR="$(TARGET_AR)" LD="$(TARGET_CXX)"
endef

define LIBRETRO_OPERA_INSTALL_TARGET_CMDS
	$(INSTALL) -D -m 0644 $(@D)/opera_libretro.so $(TARGET_DIR)/usr/lib/libretro/opera_libretro.so
endef

$(eval $(generic-package))
