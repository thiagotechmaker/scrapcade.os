################################################################################
#
# libretro-freeintv
#
################################################################################

LIBRETRO_FREEINTV_VERSION = 428915baf2bfc032fc03e645f4f8f9c6c3144979
LIBRETRO_FREEINTV_SITE = $(call github,libretro,FreeIntv,$(LIBRETRO_FREEINTV_VERSION))
LIBRETRO_FREEINTV_LICENSE = GPL-3.0

define LIBRETRO_FREEINTV_BUILD_CMDS
	$(TARGET_MAKE_ENV) CFLAGS="$(TARGET_CFLAGS)" CXXFLAGS="$(TARGET_CXXFLAGS)" LDFLAGS="$(TARGET_LDFLAGS)" \
		$(MAKE) -C $(@D) platform=unix \
		CC="$(TARGET_CC)" CXX="$(TARGET_CXX)" AR="$(TARGET_AR)" LD="$(TARGET_CXX)"
endef

define LIBRETRO_FREEINTV_INSTALL_TARGET_CMDS
	$(INSTALL) -D -m 0644 $(@D)/freeintv_libretro.so $(TARGET_DIR)/usr/lib/libretro/freeintv_libretro.so
endef

$(eval $(generic-package))
