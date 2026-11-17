################################################################################
#
# retroarch-glsl-shaders (pacote completo de filtros GLSL do RetroArch)
#
################################################################################

RETROARCH_GLSL_SHADERS_VERSION = a35069864b4bf30f8c8e740d57206543fb7e745e
RETROARCH_GLSL_SHADERS_SITE = $(call github,libretro,glsl-shaders,$(RETROARCH_GLSL_SHADERS_VERSION))
RETROARCH_GLSL_SHADERS_LICENSE = varias (GPL, MIT, dominio publico; ver cada shader)

define RETROARCH_GLSL_SHADERS_INSTALL_TARGET_CMDS
	mkdir -p $(TARGET_DIR)/usr/share/retroarch/shaders/glsl
	cd $(@D) && find . -mindepth 1 -maxdepth 1 -type d ! -name '.*' ! -name test -exec cp -r {} $(TARGET_DIR)/usr/share/retroarch/shaders/glsl/ \;
endef

$(eval $(generic-package))
