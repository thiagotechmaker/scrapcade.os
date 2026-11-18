################################################################################
#
# libgsf (arquivos estruturados: base do Gnumeric para abrir .xls/.xlsx/.ods)
#
################################################################################

LIBGSF_VERSION_MAJOR = 1.14
LIBGSF_VERSION = $(LIBGSF_VERSION_MAJOR).60
LIBGSF_SOURCE = libgsf-$(LIBGSF_VERSION).tar.xz
LIBGSF_SITE = https://download.gnome.org/sources/libgsf/$(LIBGSF_VERSION_MAJOR)
LIBGSF_LICENSE = LGPL-2.1
LIBGSF_INSTALL_STAGING = YES
LIBGSF_DEPENDENCIES = host-pkgconf host-intltool libglib2 libxml2 zlib $(TARGET_NLS_DEPENDENCIES)
LIBGSF_CONF_OPTS = --disable-introspection --disable-gtk-doc --without-gdk-pixbuf --without-bz2

$(eval $(autotools-package))
