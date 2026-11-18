################################################################################
#
# libretro-easyrpg
#
################################################################################

LIBRETRO_EASYRPG_VERSION = 78328fa29f465315291e161130e6682f69410370
LIBRETRO_EASYRPG_SITE = $(call github,EasyRPG,Player,$(LIBRETRO_EASYRPG_VERSION))
LIBRETRO_EASYRPG_LICENSE = GPL-3.0
LIBRETRO_EASYRPG_SUPPORTS_IN_SOURCE_BUILD = NO
# o submodulo libretro-common nao vem no .tar.gz do GitHub: baixa no commit que o Player fixa
LIBRETRO_EASYRPG_COMMON_VERSION = 20a43ba79fe6b4ec094b3b20b7bc88f4cfe916fa
LIBRETRO_EASYRPG_EXTRA_DOWNLOADS = https://github.com/libretro/libretro-common/archive/$(LIBRETRO_EASYRPG_COMMON_VERSION).tar.gz
define LIBRETRO_EASYRPG_EXTRACT_COMMON
	mkdir -p $(@D)/builds/libretro/libretro-common
	tar -xzf $(LIBRETRO_EASYRPG_DL_DIR)/$(LIBRETRO_EASYRPG_COMMON_VERSION).tar.gz --strip-components=1 \
		-C $(@D)/builds/libretro/libretro-common
endef
LIBRETRO_EASYRPG_POST_EXTRACT_HOOKS += LIBRETRO_EASYRPG_EXTRACT_COMMON
LIBRETRO_EASYRPG_DEPENDENCIES = liblcf fmt pixman speexdsp mpg123 libsndfile libvorbis opusfile libpng zlib
# como no Lakka: so o nucleo libretro, com o som pelas bibliotecas que o sistema ja tem (MIDI pelo sintetizador interno)
LIBRETRO_EASYRPG_CONF_OPTS = -DPLAYER_TARGET_PLATFORM=libretro -DBUILD_SHARED_LIBS=ON -DCMAKE_BUILD_TYPE=Release \
	-DPLAYER_WITH_FREETYPE=OFF -DPLAYER_WITH_LHASA=OFF -DPLAYER_WITH_NLOHMANN_JSON=OFF -DPLAYER_WITH_NATIVE_MIDI=OFF \
	-DPLAYER_WITH_WILDMIDI=OFF -DPLAYER_WITH_FLUIDSYNTH=OFF -DPLAYER_WITH_FLUIDLITE=OFF -DPLAYER_WITH_XMP=OFF \
	-DPLAYER_WITH_SAMPLERATE=OFF

define LIBRETRO_EASYRPG_INSTALL_TARGET_CMDS
	$(INSTALL) -D -m 0644 $$(find $(@D) -name easyrpg_libretro.so | head -1) $(TARGET_DIR)/usr/lib/libretro/easyrpg_libretro.so
endef

$(eval $(cmake-package))
