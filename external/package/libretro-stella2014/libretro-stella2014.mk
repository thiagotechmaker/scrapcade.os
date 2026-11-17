################################################################################
#
# libretro-stella2014
#
################################################################################

LIBRETRO_STELLA2014_VERSION = 4a7da82595d27b8df7af1ecb467a64b642a41bc9
LIBRETRO_STELLA2014_SITE = $(call github,libretro,stella2014-libretro,$(LIBRETRO_STELLA2014_VERSION))
LIBRETRO_STELLA2014_LICENSE = GPL-2.0

define LIBRETRO_STELLA2014_BUILD_CMDS
	$(TARGET_MAKE_ENV) CFLAGS="$(TARGET_CFLAGS)" CXXFLAGS="$(TARGET_CXXFLAGS)" LDFLAGS="$(TARGET_LDFLAGS)" \
		$(MAKE) -C $(@D) platform=unix \
		CC="$(TARGET_CC)" CXX="$(TARGET_CXX)" AR="$(TARGET_AR)" LD="$(TARGET_CXX)"
endef

define LIBRETRO_STELLA2014_INSTALL_TARGET_CMDS
	$(INSTALL) -D -m 0644 $(@D)/stella2014_libretro.so $(TARGET_DIR)/usr/lib/libretro/stella2014_libretro.so
endef

$(eval $(generic-package))
