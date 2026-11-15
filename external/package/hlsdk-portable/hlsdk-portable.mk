################################################################################
#
# hlsdk-portable
#
################################################################################

HLSDK_PORTABLE_VERSION = 89e83b92d561f5ceb11594c2f15e2e1519bc6c36
HLSDK_PORTABLE_SITE = https://github.com/FWGS/hlsdk-portable.git
HLSDK_PORTABLE_SITE_METHOD = git
HLSDK_PORTABLE_GIT_SUBMODULES = YES   # freevgui e um submodulo
HLSDK_PORTABLE_LICENSE = Half-Life 1 SDK License
HLSDK_PORTABLE_LICENSE_FILES = LICENSE
HLSDK_PORTABLE_CONF_OPTS = -T release --disable-werror $(if $(BR2_x86_64),--64bits)

# o tos-jogar copia estas bibliotecas para a pasta valve do usuario (as da Steam sao de 32 bits do Windows/Linux)
define HLSDK_PORTABLE_INSTALL_TARGET_CMDS
	mkdir -p $(TARGET_DIR)/usr/lib/xash3d/hlsdk/cl_dlls $(TARGET_DIR)/usr/lib/xash3d/hlsdk/dlls
	find $(@D)/build -name 'client*.so' -exec cp {} $(TARGET_DIR)/usr/lib/xash3d/hlsdk/cl_dlls/ \;
	find $(@D)/build -name 'hl*.so' -exec cp {} $(TARGET_DIR)/usr/lib/xash3d/hlsdk/dlls/ \;
	$(INSTALL) -D -m 0644 $(@D)/LICENSE $(TARGET_DIR)/usr/lib/xash3d/hlsdk/LICENSE
endef

$(eval $(waf-package))
