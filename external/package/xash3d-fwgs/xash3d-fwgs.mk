################################################################################
#
# xash3d-fwgs
#
################################################################################

XASH3D_FWGS_VERSION = 707596f4905689ec224e993fcaa4d4ad66aa67d6
XASH3D_FWGS_SITE = https://github.com/FWGS/xash3d-fwgs.git
XASH3D_FWGS_SITE_METHOD = git
XASH3D_FWGS_GIT_SUBMODULES = YES
XASH3D_FWGS_LICENSE = GPL-3.0+
XASH3D_FWGS_LICENSE_FILES = COPYING
XASH3D_FWGS_DEPENDENCIES = sdl2 freetype
XASH3D_FWGS_CONF_OPTS = -T release --enable-packaging --disable-werror $(if $(BR2_x86_64),--64bits)

$(eval $(waf-package))
