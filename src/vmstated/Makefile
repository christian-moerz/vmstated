
PIE_SUFFIX=	_pie
PROG=		vmstated
SRCS=		config_generator.c vmstated_main.c subscriber.c
LDADD= 		-lprocwatch${PIE_SUFFIX} -lcommand${PIE_SUFFIX} \
		-lsocket${PIE_SUFFIX} -lstate${PIE_SUFFIX} \
		-lutils${PIE_SUFFIX} -llogging${PIE_SUFFIX} \
		-ltranslate${PIE_SUFFIX} -lconfig${PIE_SUFFIX} \
		-lnv -lprivateucl -lpthread
DPADD=		../libprocwatch/libprocwatch${PIE_SUFFIX}
MAN=		vmstated.8

CFLAGS+=	-I.. -L../libprocwatch -L../libsocket -L../libcommand -L../libstate \
		-L../libutils -L../liblogging -L../libtranslate -L../libconfig
.ifdef(DEBUG)
CFLAGS+=	-DDEBUG=${DEBUG}
.endif
LDARGS+=	-L../libprocwatch -L../libsocket -L../libcommand -L../libstate \
		-L../libtranslate -L../libconfig

install:
	/bin/mkdir -p ${DESTDIR}${PREFIX}/sbin
	/bin/mkdir -p ${DESTDIR}${PREFIX}/share/man/man8
	/usr/bin/install -m 750 vmstated ${DESTDIR}${PREFIX}/sbin/vmstated
	/usr/bin/install -m 444 vmstated.8.gz ${DESTDIR}${PREFIX}/share/man/man8
	/usr/bin/strip ${DESTDIR}${PREFIX}/sbin/vmstated

.include <bsd.prog.mk>
