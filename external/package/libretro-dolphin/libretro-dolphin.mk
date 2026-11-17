################################################################################
#
# libretro-dolphin (GameCube e Wii; so 64 bits)
#
################################################################################

LIBRETRO_DOLPHIN_VERSION = 49d36c0a08a9f7c0798f71bc8bc43f5d122f7552
LIBRETRO_DOLPHIN_SITE = https://github.com/libretro/dolphin.git
LIBRETRO_DOLPHIN_SITE_METHOD = git
LIBRETRO_DOLPHIN_GIT_SUBMODULES = YES
LIBRETRO_DOLPHIN_LICENSE = GPL-2.0+
LIBRETRO_DOLPHIN_DEPENDENCIES = mesa3d zlib vulkan-headers vulkan-loader
LIBRETRO_DOLPHIN_SUPPORTS_IN_SOURCE_BUILD = NO
LIBRETRO_DOLPHIN_CONF_OPTS = -DCMAKE_BUILD_TYPE=Release -DLIBRETRO=ON -DENABLE_X11=OFF -DENABLE_NOGUI=OFF \
	-DENABLE_QT=OFF -DENABLE_TESTS=OFF -DUSE_DISCORD_PRESENCE=OFF -DBUILD_SHARED_LIBS=OFF \
	-DENABLE_LLVM=OFF -DENABLE_EVDEV=OFF -DENABLE_ALSA=OFF -DENABLE_PULSEAUDIO=OFF -DUSE_UPNP=OFF -DENABLE_ANALYTICS=OFF -DCMAKE_POLICY_VERSION_MINIMUM=3.5

define LIBRETRO_DOLPHIN_INSTALL_TARGET_CMDS
	$(INSTALL) -D -m 0644 $$(find $(@D) -name dolphin_libretro.so | head -1) $(TARGET_DIR)/usr/lib/libretro/dolphin_libretro.so
	mkdir -p $(TARGET_DIR)/usr/share/tos/dolphin-emu
	cp -r $(@D)/Data/Sys $(TARGET_DIR)/usr/share/tos/dolphin-emu/
endef

$(eval $(cmake-package))
