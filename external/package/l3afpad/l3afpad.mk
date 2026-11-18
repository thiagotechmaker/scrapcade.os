################################################################################
#
# l3afpad (editor de texto pequeno, GTK3: substitui o nano no terminal do modo desktop)
#
################################################################################

L3AFPAD_VERSION = 16f22222116b78b7f6a6fd83289937cdaabed624
L3AFPAD_SITE = $(call github,stevenhoneyman,l3afpad,$(L3AFPAD_VERSION))
L3AFPAD_LICENSE = GPL-2.0
L3AFPAD_AUTORECONF = YES
L3AFPAD_DEPENDENCIES = host-pkgconf host-intltool libgtk3 $(TARGET_NLS_DEPENDENCIES)

$(eval $(autotools-package))
