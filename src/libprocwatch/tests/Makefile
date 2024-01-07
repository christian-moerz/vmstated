
CFLAGS+=	-I.. -L.. -L/usr/local/lib -L../../libstate -L../../libutils -L../../libcommand \
		-L../../liblogging
LDADD+=		-lprocwatch${PIE_SUFFIX} -lstate${PIE_SUFFIX} -lutils${PIE_SUFFIX} \
		-llogging${PIE_SUFFIX} -lcommand${PIE_SUFFIX} -lpthread -latf-c \
		-lnv -lprivateucl
PIE_SUFFIX=	_pie
STRIP=

ATF_TESTS_C=	test_bhyve_config test_bhyve_director test_process_def test_process_state

.include <bsd.test.mk>
