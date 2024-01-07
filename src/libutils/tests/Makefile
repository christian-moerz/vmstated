
CFLAGS+=	-I.. -L.. -L/usr/local/lib -g -O0
LDADD+=		-lutils${PIE_SUFFIX} -lpthread -latf-c
PIE_SUFFIX=	_pie
STRIP=

ATF_TESTS_C=	test_collect

.include <bsd.test.mk>
