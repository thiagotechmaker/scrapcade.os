################################################################################
#
# liblcf
#
################################################################################

LIBLCF_VERSION = 92c4450a1bc1acb58bd02bbb99b57e5036919cdf
LIBLCF_SITE = $(call github,EasyRPG,liblcf,$(LIBLCF_VERSION))
LIBLCF_LICENSE = MIT
LIBLCF_INSTALL_STAGING = YES
LIBLCF_SUPPORTS_IN_SOURCE_BUILD = NO
LIBLCF_DEPENDENCIES = expat inih
# sem ICU (a biblioteca de dados dela tem 30 MB): a troca de codificacao do texto usa o iconv da glibc
LIBLCF_CONF_OPTS = -DCMAKE_BUILD_TYPE=Release -DLIBLCF_WITH_ICU=OFF -DLIBLCF_ENABLE_TOOLS=OFF -DLIBLCF_ENABLE_TESTS=OFF

$(eval $(cmake-package))
