################################################################################
#
# gnumeric (planilha leve do modo desktop: abre .xls, .xlsx, .ods e .csv)
#
################################################################################

GNUMERIC_VERSION_MAJOR = 1.12
GNUMERIC_VERSION = $(GNUMERIC_VERSION_MAJOR).62
GNUMERIC_SOURCE = gnumeric-$(GNUMERIC_VERSION).tar.xz
GNUMERIC_SITE = https://download.gnome.org/sources/gnumeric/$(GNUMERIC_VERSION_MAJOR)
GNUMERIC_LICENSE = GPL-2.0 ou GPL-3.0
GNUMERIC_DEPENDENCIES = host-pkgconf host-intltool host-bison host-flex goffice libgsf libxml2 libgtk3 pango fribidi \
	$(TARGET_NLS_DEPENDENCIES)
# sem os carregadores de Perl e Python, sem banco de dados (GDA) e sem os formatos raros que pedem bibliotecas a mais
GNUMERIC_CONF_OPTS = --disable-introspection --without-perl --without-python --without-gda --without-psiconv \
	--without-paradox --without-guile --disable-component
# o itstool so serve para o manual (doc/), que nao entra: o Buildroot nao tem itstool e o modo desktop nao tem leitor
# de ajuda. O configure so precisa achar "algum" itstool; a pasta doc sai da compilacao.
GNUMERIC_CONF_ENV = ITSTOOL=/bin/true
define GNUMERIC_SEM_MANUAL
	sed -i 's/ templates doc tools / templates tools /' $(@D)/Makefile.in
endef
GNUMERIC_POST_PATCH_HOOKS += GNUMERIC_SEM_MANUAL

$(eval $(autotools-package))
