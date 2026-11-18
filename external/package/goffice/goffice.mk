################################################################################
#
# goffice (graficos e formatos de escritorio do Gnumeric). Sem librsvg (que pediria o Rust) e sem lasem/EPS.
#
################################################################################

GOFFICE_VERSION_MAJOR = 0.10
GOFFICE_VERSION = $(GOFFICE_VERSION_MAJOR).62
GOFFICE_SOURCE = goffice-$(GOFFICE_VERSION).tar.xz
GOFFICE_SITE = https://download.gnome.org/sources/goffice/$(GOFFICE_VERSION_MAJOR)
GOFFICE_LICENSE = GPL-2.0 ou GPL-3.0
GOFFICE_INSTALL_STAGING = YES
GOFFICE_DEPENDENCIES = host-pkgconf host-intltool libglib2 libgsf libxml2 libxslt cairo pango libgtk3 \
	$(TARGET_NLS_DEPENDENCIES)
GOFFICE_CONF_OPTS = --disable-introspection --disable-gtk-doc --without-librsvg --without-lasem --without-eps \
	--with-config-backend=keyfile --with-gtk

$(eval $(autotools-package))
