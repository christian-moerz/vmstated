
CFLAGS+=	-I.. -L.. -L../../libutils -L/usr/local/lib -g -O0
LDADD+=		-lsocket${PIE_SUFFIX} -lutils${PIE_SUFFIX} -lpthread -latf-c -lnv
PIE_SUFFIX=	_pie
STRIP=

ATF_TESTS_C=	test_nvlist test_socket \
		test_socket_connect test_socket_parser

.include <bsd.test.mk>
