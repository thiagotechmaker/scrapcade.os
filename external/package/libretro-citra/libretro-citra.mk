################################################################################
#
# libretro-citra (Nintendo 3DS; so 64 bits)
#
################################################################################

LIBRETRO_CITRA_VERSION = 4268124a74b55ddba69f29dc9858f222c4bd006d
LIBRETRO_CITRA_SITE = https://github.com/libretro/citra.git
LIBRETRO_CITRA_SITE_METHOD = git
LIBRETRO_CITRA_GIT_SUBMODULES = YES
LIBRETRO_CITRA_LICENSE = GPL-2.0+
LIBRETRO_CITRA_DEPENDENCIES = mesa3d zlib
LIBRETRO_CITRA_SUPPORTS_IN_SOURCE_BUILD = NO
LIBRETRO_CITRA_CONF_OPTS = -DCMAKE_BUILD_TYPE=Release -DENABLE_TESTS=OFF -DENABLE_DEDICATED_ROOM=OFF -DENABLE_SDL2=OFF \
	-DENABLE_QT=OFF -DENABLE_WEB_SERVICE=OFF -DENABLE_SCRIPTING=OFF -DENABLE_OPENAL=OFF -DENABLE_LIBUSB=OFF \
	-DCITRA_ENABLE_BUNDLE_TARGET=OFF -DCITRA_WARNINGS_AS_ERRORS=OFF -DCMAKE_POLICY_VERSION_MINIMUM=3.5

define LIBRETRO_CITRA_INSTALL_TARGET_CMDS
	$(INSTALL) -D -m 0644 $$(find $(@D) -name citra_libretro.so | head -1) $(TARGET_DIR)/usr/lib/libretro/citra_libretro.so
endef

$(eval $(cmake-package))
