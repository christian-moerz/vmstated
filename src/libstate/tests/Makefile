
CFLAGS+=	-I.. -L.. -L/usr/local/lib
LDADD+=		-lstate${PIE_SUFFIX} -lpthread -latf-c
PIE_SUFFIX=	_pie
STRIP=

ATF_TESTS_C=	test_state_handler

.include <bsd.test.mk>
