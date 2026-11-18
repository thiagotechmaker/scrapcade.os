################################################################################
#
# libretro-bluemsx
#
################################################################################

LIBRETRO_BLUEMSX_VERSION = 0f32f52c48d3e772bfdf0379756f81f00b4e08bc
LIBRETRO_BLUEMSX_SITE = $(call github,libretro,blueMSX-libretro,$(LIBRETRO_BLUEMSX_VERSION))
LIBRETRO_BLUEMSX_LICENSE = GPL-2.0

define LIBRETRO_BLUEMSX_BUILD_CMDS
	$(TARGET_MAKE_ENV) CFLAGS="$(TARGET_CFLAGS)" CXXFLAGS="$(TARGET_CXXFLAGS)" LDFLAGS="$(TARGET_LDFLAGS)" \
		$(MAKE) -C $(@D) platform=unix -f Makefile.libretro \
		CC="$(TARGET_CC)" CXX="$(TARGET_CXX)" AR="$(TARGET_AR)" LD="$(TARGET_CXX)"
endef

define LIBRETRO_BLUEMSX_INSTALL_TARGET_CMDS
	$(INSTALL) -D -m 0644 $(@D)/bluemsx_libretro.so $(TARGET_DIR)/usr/lib/libretro/bluemsx_libretro.so
	mkdir -p $(TARGET_DIR)/usr/share/retroarch/system
	cp -r $(@D)/system/bluemsx/Databases $(@D)/system/bluemsx/Machines $(TARGET_DIR)/usr/share/retroarch/system/
endef

$(eval $(generic-package))
