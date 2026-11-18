################################################################################
#
# libretro-flycast
#
################################################################################

LIBRETRO_FLYCAST_VERSION = v2.7
LIBRETRO_FLYCAST_SITE = https://github.com/flyinghead/flycast.git
LIBRETRO_FLYCAST_SITE_METHOD = git
# o codigo depende de submodulos (bibliotecas em core/deps): o download pelo git traz todos
LIBRETRO_FLYCAST_GIT_SUBMODULES = YES
LIBRETRO_FLYCAST_LICENSE = GPL-2.0
LIBRETRO_FLYCAST_DEPENDENCIES = mesa3d zlib
LIBRETRO_FLYCAST_CONF_OPTS = -DLIBRETRO=ON -DUSE_VULKAN=OFF -DUSE_GLES=OFF -DUSE_HOST_LIBZIP=OFF -DBUILD_SHARED_LIBS=OFF -DCMAKE_POSITION_INDEPENDENT_CODE=ON

define LIBRETRO_FLYCAST_INSTALL_TARGET_CMDS
	$(INSTALL) -D -m 0644 $$(find $(@D) -name flycast_libretro.so | head -1) \
		$(TARGET_DIR)/usr/lib/libretro/flycast_libretro.so
endef

$(eval $(cmake-package))
