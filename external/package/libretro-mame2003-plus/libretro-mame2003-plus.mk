################################################################################
#
# libretro-mame2003-plus
#
################################################################################

LIBRETRO_MAME2003_PLUS_VERSION = d6bf36f65f36853120d7e97cdf4fa3720de66728
LIBRETRO_MAME2003_PLUS_SITE = $(call github,libretro,mame2003-plus-libretro,$(LIBRETRO_MAME2003_PLUS_VERSION))
LIBRETRO_MAME2003_PLUS_LICENSE = MAME (nao comercial)

define LIBRETRO_MAME2003_PLUS_BUILD_CMDS
	$(TARGET_MAKE_ENV) CFLAGS="$(TARGET_CFLAGS)" CXXFLAGS="$(TARGET_CXXFLAGS)" LDFLAGS="$(TARGET_LDFLAGS)" \
		$(MAKE) -C $(@D) platform=unix ARCH= NATIVE_CC="$(TARGET_CC)" \
		CC="$(TARGET_CC)" CXX="$(TARGET_CXX)" AR="$(TARGET_AR)" LD="$(TARGET_CXX)"
endef

define LIBRETRO_MAME2003_PLUS_INSTALL_TARGET_CMDS
	$(INSTALL) -D -m 0644 $(@D)/mame2003_plus_libretro.so $(TARGET_DIR)/usr/lib/libretro/mame2003_plus_libretro.so
	mkdir -p $(TARGET_DIR)/usr/share/retroarch/system/mame2003-plus
	cp $(@D)/metadata/hiscore.dat $(TARGET_DIR)/usr/share/retroarch/system/mame2003-plus/
endef

$(eval $(generic-package))
