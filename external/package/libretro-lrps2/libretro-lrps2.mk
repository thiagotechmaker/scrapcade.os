################################################################################
#
# libretro-lrps2 (PlayStation 2, nucleo LRPS2 derivado do PCSX2; so 64 bits)
#
################################################################################

LIBRETRO_LRPS2_VERSION = 797ed4f60841b09ecfad28ab01203182cb5a98cb
LIBRETRO_LRPS2_SITE = https://github.com/libretro/ps2.git
LIBRETRO_LRPS2_SITE_METHOD = git
LIBRETRO_LRPS2_GIT_SUBMODULES = YES
LIBRETRO_LRPS2_LICENSE = GPL-3.0
LIBRETRO_LRPS2_DEPENDENCIES = mesa3d zlib vulkan-headers vulkan-loader
LIBRETRO_LRPS2_SUPPORTS_IN_SOURCE_BUILD = NO
LIBRETRO_LRPS2_CONF_OPTS = -DCMAKE_BUILD_TYPE=Release -DUSE_OPENGL=ON -DUSE_VULKAN=ON -DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
	-DCMAKE_LINK_LIBRARY_USING_WHOLE_ARCHIVE_SUPPORTED=TRUE \
	-DCMAKE_LINK_LIBRARY_USING_WHOLE_ARCHIVE="-Wl,--whole-archive <LIBRARY> -Wl,--no-whole-archive"
# o Buildroot chama o sistema de "Buildroot" (e nao "Linux"), e o CMake nao sabe ligar WHOLE_ARCHIVE sozinho

define LIBRETRO_LRPS2_INSTALL_TARGET_CMDS
	$(INSTALL) -D -m 0644 $$(find $(@D) -name pcsx2_libretro.so | head -1) $(TARGET_DIR)/usr/lib/libretro/pcsx2_libretro.so
	mkdir -p $(TARGET_DIR)/usr/share/tos/pcsx2/resources
	cp $(@D)/bin/resources/GameIndex.yaml $(TARGET_DIR)/usr/share/tos/pcsx2/resources/
endef

$(eval $(cmake-package))
