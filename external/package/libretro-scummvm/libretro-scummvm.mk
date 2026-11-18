################################################################################
#
# libretro-scummvm
#
################################################################################

LIBRETRO_SCUMMVM_VERSION = c3f4936d1ba2af178816cc9bb06087025e4a2bea
LIBRETRO_SCUMMVM_SITE = $(call github,libretro,scummvm,$(LIBRETRO_SCUMMVM_VERSION))
LIBRETRO_SCUMMVM_LICENSE = GPL-3.0
LIBRETRO_SCUMMVM_DEPENDENCIES = zlib freetype flac libvorbis faad2 fluidsynth host-zip

# O Makefile do libretro baixa duas dependencias (libretro-deps e libretro-common) nos commits fixos que ele mesmo
# declara em dependencies.mk. Sem a "nuvem" do ScummVM (nada de conta online); bibliotecas de som e fonte do sistema.
LIBRETRO_SCUMMVM_MAKE_OPTS = platform=unix USE_CLOUD=0 \
	USE_SYSTEM_fluidsynth=1 USE_SYSTEM_FLAC=1 USE_SYSTEM_vorbis=1 USE_SYSTEM_z=1 USE_SYSTEM_faad=1 USE_SYSTEM_freetype=1

# o .tar.gz do GitHub traz as pastas dos submodulos vazias; o script do ScummVM quebra com pasta vazia que nao e git
define LIBRETRO_SCUMMVM_LIMPA_SUBMODULOS
	rmdir $(@D)/backends/platform/libretro/deps/libretro-deps $(@D)/backends/platform/libretro/deps/libretro-common 2>/dev/null || true
endef
LIBRETRO_SCUMMVM_POST_EXTRACT_HOOKS += LIBRETRO_SCUMMVM_LIMPA_SUBMODULOS

define LIBRETRO_SCUMMVM_BUILD_CMDS
	rm -rf $(@D)/backends/platform/libretro/deps/libretro-deps $(@D)/backends/platform/libretro/deps/libretro-common
	# o TARGET_MAKE_ENV do Buildroot traz GIT_DIR=., e com ele o git do script de submodulos criava repositorios
	# quebrados: o "env -u" depois dele tira so aqui
	$(TARGET_MAKE_ENV) env -u GIT_DIR CFLAGS="$(TARGET_CFLAGS)" CXXFLAGS="$(TARGET_CXXFLAGS) -DHAVE_POSIX_MEMALIGN=1" LDFLAGS="$(TARGET_LDFLAGS)" \
		$(MAKE) -C $(@D)/backends/platform/libretro all $(LIBRETRO_SCUMMVM_MAKE_OPTS) \
		CC="$(TARGET_CC)" CXX="$(TARGET_CXX)" AR="$(TARGET_AR) cru" RANLIB="$(TARGET_RANLIB)" LD="$(TARGET_CXX)"
endef

# scummvm.zip traz os temas, as fontes e o teclado virtual; vai aberto na pasta de BIOS do sistema
define LIBRETRO_SCUMMVM_INSTALL_TARGET_CMDS
	$(INSTALL) -D -m 0644 $(@D)/backends/platform/libretro/scummvm_libretro.so $(TARGET_DIR)/usr/lib/libretro/scummvm_libretro.so
	mkdir -p $(TARGET_DIR)/usr/share/retroarch/system
	rm -rf $(TARGET_DIR)/usr/share/retroarch/system/scummvm
	cd $(TARGET_DIR)/usr/share/retroarch/system && unzip -oq $(@D)/backends/platform/libretro/scummvm.zip
endef

$(eval $(generic-package))
