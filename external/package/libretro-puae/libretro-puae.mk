################################################################################
#
# libretro-puae
#
################################################################################

LIBRETRO_PUAE_VERSION = 96ebfcfc2c66233ad37f6dc99ee991211dc719ad
LIBRETRO_PUAE_SITE = $(call github,libretro,libretro-uae,$(LIBRETRO_PUAE_VERSION))
LIBRETRO_PUAE_LICENSE = GPL-2.0

define LIBRETRO_PUAE_BUILD_CMDS
	$(TARGET_MAKE_ENV) CFLAGS="$(TARGET_CFLAGS)" CXXFLAGS="$(TARGET_CXXFLAGS)" LDFLAGS="$(TARGET_LDFLAGS)" \
		$(MAKE) -C $(@D) platform=unix \
		CC="$(TARGET_CC)" CXX="$(TARGET_CXX)" AR="$(TARGET_AR)" LD="$(TARGET_CXX)"
endef

define LIBRETRO_PUAE_INSTALL_TARGET_CMDS
	$(INSTALL) -D -m 0644 $(@D)/puae_libretro.so $(TARGET_DIR)/usr/lib/libretro/puae_libretro.so
	mkdir -p $(TARGET_DIR)/usr/share/retroarch/system/uae_data
	cp -r $(@D)/sources/uae_data/. $(TARGET_DIR)/usr/share/retroarch/system/uae_data/
endef

$(eval $(generic-package))
