################################################################################
#
# retroarch
#
################################################################################

RETROARCH_VERSION = v1.22.2
RETROARCH_SITE = $(call github,libretro,RetroArch,$(RETROARCH_VERSION))
RETROARCH_LICENSE = GPL-3.0+
RETROARCH_DEPENDENCIES = zlib alsa-lib sdl2 libdrm mesa3d freetype udev host-pkgconf

# So o necessario para os jogos: KMS + OpenGL pela Mesa, som ALSA, controle pelo SDL2 e udev,
# menu RGUI (embutido, sem pacote de icones). Sem X11, Wayland, Qt, ffmpeg, Discord ou atualizador.
RETROARCH_CONF_OPTS = \
	--disable-x11 --disable-wayland --disable-qt --disable-ffmpeg --disable-discord \
	--disable-pulse --disable-jack --disable-oss --disable-sdl --disable-cg --disable-vulkan \
	--disable-opengles --disable-update_cores --disable-update_assets --disable-online_updater \
	--disable-networkgamepad --disable-materialui --disable-ozone --disable-xmb \
	--enable-kms --enable-egl --enable-opengl --enable-alsa --enable-udev --enable-sdl2 \
	--enable-zlib --enable-freetype --enable-threads --enable-rgui

define RETROARCH_CONFIGURE_CMDS
	(cd $(@D); rm -rf config.cache; \
		$(TARGET_CONFIGURE_OPTS) \
		CFLAGS="$(TARGET_CFLAGS)" \
		LDFLAGS="$(TARGET_LDFLAGS)" \
		CROSS_COMPILE="$(TARGET_CROSS)" \
		PKG_CONF_PATH="$(PKG_CONFIG_HOST_BINARY)" \
		./configure --prefix=/usr $(RETROARCH_CONF_OPTS))
endef

define RETROARCH_BUILD_CMDS
	$(TARGET_MAKE_ENV) $(MAKE) -C $(@D) CC="$(TARGET_CC)" CXX="$(TARGET_CXX)" LD="$(TARGET_CXX)" \
		AR="$(TARGET_AR)" STRIP="$(TARGET_STRIP)"
endef

define RETROARCH_INSTALL_TARGET_CMDS
	$(INSTALL) -D -m 0755 $(@D)/retroarch $(TARGET_DIR)/usr/bin/retroarch
endef

$(eval $(generic-package))
