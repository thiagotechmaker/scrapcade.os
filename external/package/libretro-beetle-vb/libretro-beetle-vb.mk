################################################################################
#
# libretro-beetle-vb
#
################################################################################

LIBRETRO_BEETLE_VB_VERSION = 3f53a40bf8aa18777514fd4b220960427e312a3f
LIBRETRO_BEETLE_VB_SITE = $(call github,libretro,beetle-vb-libretro,$(LIBRETRO_BEETLE_VB_VERSION))
LIBRETRO_BEETLE_VB_LICENSE = GPL-2.0

define LIBRETRO_BEETLE_VB_BUILD_CMDS
	$(TARGET_MAKE_ENV) CFLAGS="$(TARGET_CFLAGS)" CXXFLAGS="$(TARGET_CXXFLAGS)" LDFLAGS="$(TARGET_LDFLAGS)" \
		$(MAKE) -C $(@D) platform=unix \
		CC="$(TARGET_CC)" CXX="$(TARGET_CXX)" AR="$(TARGET_AR)" LD="$(TARGET_CXX)"
endef

define LIBRETRO_BEETLE_VB_INSTALL_TARGET_CMDS
	$(INSTALL) -D -m 0644 $(@D)/mednafen_vb_libretro.so $(TARGET_DIR)/usr/lib/libretro/mednafen_vb_libretro.so
endef

$(eval $(generic-package))
