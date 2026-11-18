################################################################################
#
# fribidi (texto da direita para a esquerda; o Gnumeric exige)
#
################################################################################

FRIBIDI_VERSION = 1.0.18
FRIBIDI_SOURCE = fribidi-$(FRIBIDI_VERSION).tar.xz
FRIBIDI_SITE = https://github.com/fribidi/fribidi/releases/download/v$(FRIBIDI_VERSION)
FRIBIDI_LICENSE = LGPL-2.1+
FRIBIDI_INSTALL_STAGING = YES
FRIBIDI_CONF_OPTS = -Ddocs=false -Dtests=false -Dbin=false

$(eval $(meson-package))
