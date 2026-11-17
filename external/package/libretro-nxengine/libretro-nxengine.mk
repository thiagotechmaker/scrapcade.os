################################################################################
#
# libretro-nxengine
#
################################################################################

LIBRETRO_NXENGINE_VERSION = a1c45fd4d045333f4ea523e556469b545be82b0a
LIBRETRO_NXENGINE_SITE = $(call github,libretro,nxengine-libretro,$(LIBRETRO_NXENGINE_VERSION))
LIBRETRO_NXENGINE_LICENSE = GPL-3.0

define LIBRETRO_NXENGINE_BUILD_CMDS
	$(TARGET_MAKE_ENV) CFLAGS="$(TARGET_CFLAGS)" CXXFLAGS="$(TARGET_CXXFLAGS)" LDFLAGS="$(TARGET_LDFLAGS)" \
		$(MAKE) -C $(@D) platform=unix \
		CC="$(TARGET_CC)" CXX="$(TARGET_CXX)" AR="$(TARGET_AR)" LD="$(TARGET_CXX)"
endef

define LIBRETRO_NXENGINE_INSTALL_TARGET_CMDS
	$(INSTALL) -D -m 0644 $(@D)/nxengine_libretro.so $(TARGET_DIR)/usr/lib/libretro/nxengine_libretro.so
	mkdir -p $(TARGET_DIR)/usr/share/retroarch/system/nxengine
	cp -r $(@D)/datafiles/. $(TARGET_DIR)/usr/share/retroarch/system/nxengine/
endef

$(eval $(generic-package))
