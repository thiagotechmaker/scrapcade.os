################################################################################
#
# libretro-ppsspp
#
################################################################################

LIBRETRO_PPSSPP_VERSION = 1.20.4
# o pacote da versao ja traz os submodulos (glslang, SPIRV-Cross...), que o download automatico do GitHub nao tem
LIBRETRO_PPSSPP_SOURCE = ppsspp-$(LIBRETRO_PPSSPP_VERSION).tar.xz
LIBRETRO_PPSSPP_SITE = https://github.com/hrydgard/ppsspp/releases/download/v$(LIBRETRO_PPSSPP_VERSION)
LIBRETRO_PPSSPP_LICENSE = GPL-2.0+
LIBRETRO_PPSSPP_DEPENDENCIES = mesa3d zlib libpng
LIBRETRO_PPSSPP_CONF_OPTS = -DLIBRETRO=ON -DUSE_FFMPEG=OFF -DUSE_DISCORD=OFF -DUSE_MINIUPNPC=OFF -DUSING_X11_VULKAN=OFF -DUSE_WAYLAND_WSI=OFF


define LIBRETRO_PPSSPP_INSTALL_TARGET_CMDS
	$(INSTALL) -D -m 0644 $$(find $(@D) -name ppsspp_libretro.so | head -1) $(TARGET_DIR)/usr/lib/libretro/ppsspp_libretro.so
	mkdir -p $(TARGET_DIR)/usr/share/tos/ppsspp
	cp -r $(@D)/assets/. $(TARGET_DIR)/usr/share/tos/ppsspp/
endef

$(eval $(cmake-package))
