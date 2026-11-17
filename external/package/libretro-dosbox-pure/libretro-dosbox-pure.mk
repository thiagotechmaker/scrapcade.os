################################################################################
#
# libretro-dosbox-pure
#
################################################################################

LIBRETRO_DOSBOX_PURE_VERSION = e39497e7c2a32c67c26e6598dd14f6d58c384bfd
LIBRETRO_DOSBOX_PURE_SITE = $(call github,libretro,dosbox-pure,$(LIBRETRO_DOSBOX_PURE_VERSION))
LIBRETRO_DOSBOX_PURE_LICENSE = GPL-2.0

define LIBRETRO_DOSBOX_PURE_BUILD_CMDS
	$(TARGET_MAKE_ENV) $(MAKE) -C $(@D) platform=unix \
		CXX="$(TARGET_CXX)" CC="$(TARGET_CC)" AR="$(TARGET_AR)" MAKE_CPUFLAGS="$(TARGET_CXXFLAGS)"
endef

define LIBRETRO_DOSBOX_PURE_INSTALL_TARGET_CMDS
	$(INSTALL) -D -m 0644 $(@D)/dosbox_pure_libretro.so $(TARGET_DIR)/usr/lib/libretro/dosbox_pure_libretro.so
endef

$(eval $(generic-package))
