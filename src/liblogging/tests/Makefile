
CFLAGS+=	-I.. -L.. -L../../libprocwatch -L../../libstate \
		-L../../libcommand -L/usr/local/lib -g -O0
LDADD+=		-llogging${PIE_SUFFIX} -lprocwatch${PIE_SUFFIX} -lstate${PIE_SUFFIX} \
		-lcommand${PIE_SUFFIX} -lpthread -latf-c -lprivateucl -lnv
PIE_SUFFIX=	_pie
STRIP=

ATF_TESTS_C=	test_redirect

.include <bsd.test.mk>
