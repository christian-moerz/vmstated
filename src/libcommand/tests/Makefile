
CFLAGS+=	-I.. -L.. -L../../libutils -L/usr/local/lib -g -O0
LDADD+=		-lcommand${PIE_SUFFIX} -lutils${PIE_SUFFIX} -latf-c -lnv
PIE_SUFFIX=	_pie
STRIP=

ATF_TESTS_C=	test_parser test_vm_info

.include <bsd.test.mk>
