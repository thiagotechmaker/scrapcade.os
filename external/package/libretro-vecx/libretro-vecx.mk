################################################################################
#
# libretro-vecx
#
################################################################################

LIBRETRO_VECX_VERSION = 8f671cc9d737f2890c3ce19e177e2984dcae121f
LIBRETRO_VECX_SITE = $(call github,libretro,libretro-vecx,$(LIBRETRO_VECX_VERSION))
LIBRETRO_VECX_LICENSE = GPL-2.0

LIBRETRO_VECX_DEPENDENCIES = $(if $(BR2_PACKAGE_HAS_LIBGL),libgl)

define LIBRETRO_VECX_BUILD_CMDS
	$(TARGET_MAKE_ENV) CFLAGS="$(TARGET_CFLAGS)" CXXFLAGS="$(TARGET_CXXFLAGS)" LDFLAGS="$(TARGET_LDFLAGS)" \
		$(MAKE) -C $(@D) platform=unix -f Makefile.libretro \
		CC="$(TARGET_CC)" CXX="$(TARGET_CXX)" AR="$(TARGET_AR)" LD="$(TARGET_CXX)"
endef

define LIBRETRO_VECX_INSTALL_TARGET_CMDS
	$(INSTALL) -D -m 0644 $(@D)/vecx_libretro.so $(TARGET_DIR)/usr/lib/libretro/vecx_libretro.so
endef

$(eval $(generic-package))
