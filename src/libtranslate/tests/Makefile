
CFLAGS+=	-I.. -L.. -L../../libutils -L../../libprocwatch \
		-L../../libconfig -L../../libcommand -L/usr/local/lib -g -O0
LDADD+=		-lconfig${PIE_SUFFIX} -lcommand${PIE_SUFFIX} -lprocwatch${PIE_SUFFIX} \
		-lutils${PIE_SUFFIX} -ltranslate${PIE_SUFFIX} -latf-c -lprivateucl -lnv
PIE_SUFFIX=	_pie
STRIP=

ATF_TESTS_C=	test_translate_basic

.include <bsd.test.mk>
