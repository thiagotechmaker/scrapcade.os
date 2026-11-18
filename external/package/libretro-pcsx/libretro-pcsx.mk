################################################################################
#
# libretro-pcsx
#
################################################################################

LIBRETRO_PCSX_VERSION = 7b0e8f8fadfbce3d170d195b9d9eecaabfa08c8e
LIBRETRO_PCSX_SITE = $(call github,libretro,pcsx_rearmed,$(LIBRETRO_PCSX_VERSION))
LIBRETRO_PCSX_LICENSE = GPL-2.0
LIBRETRO_PCSX_DEPENDENCIES =

# sem o leitor de CD fisico: nesse commit ele chama dir_list_new e compat_strcasestr, que o Makefile nao compila, e
# com o "-z now" do sistema o nucleo inteiro nao abria (os jogos de PlayStation vem de arquivo: .cue, .chd, .pbp)

define LIBRETRO_PCSX_BUILD_CMDS
	$(TARGET_MAKE_ENV) CFLAGS="$(TARGET_CFLAGS)" CXXFLAGS="$(TARGET_CXXFLAGS)" LDFLAGS="$(TARGET_LDFLAGS)" \
		$(MAKE) -C $(@D)/. -f Makefile.libretro platform=unix HAVE_PHYSICAL_CDROM=0 \
		CC="$(TARGET_CC)" CXX="$(TARGET_CXX)" AR="$(TARGET_AR)" LD="$(TARGET_CC)" 
endef

define LIBRETRO_PCSX_INSTALL_TARGET_CMDS
	$(INSTALL) -D -m 0644 $(@D)/./pcsx_rearmed_libretro.so $(TARGET_DIR)/usr/lib/libretro/pcsx_rearmed_libretro.so
endef

$(eval $(generic-package))
