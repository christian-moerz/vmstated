
CFLAGS+=	-I.. -L.. -L../../libutils -L/usr/local/lib -g -O0
LDADD+=		-lconfig${PIE_SUFFIX} -lutils${PIE_SUFFIX} -latf-c
PIE_SUFFIX=	_pie
STRIP=

ATF_TESTS_C=	test_config_block test_config_check test_config_core \
		test_string_list

.include <bsd.test.mk>
