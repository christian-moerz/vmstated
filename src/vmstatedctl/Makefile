
PIE_SUFFIX=	_pie
PROG=		vmstatedctl
SRCS=		vmstatedctl_main.c
LDADD= 		-lcommand${PIE_SUFFIX} -lsocket${PIE_SUFFIX} \
		-lutils${PIE_SUFFIX} \
		-lnv -lpthread
DPADD=		../libprocwatch/libsocket${PIE_SUFFIX}

CFLAGS+=	-I.. -L../libprocwatch -L../libsocket -L../libcommand \
		-L../libstate -L../libutils
.ifdef(DEBUG)
CFLAGS+=	-DDEBUG=${DEBUG}
.endif
LDARGS+=	-L../libprocwatch -L../libsocket -L../libcommand \
		-L../libstate -L../libutils

install:
	/bin/mkdir -p ${DESTDIR}${PREFIX}/bin
	/bin/mkdir -p ${DESTDIR}${MANPREFIX}/man/man1
	/usr/bin/install -o root -m 755 vmstatedctl ${DESTDIR}${PREFIX}/bin/vmstatedctl
	/usr/bin/install -m 444 vmstatedctl.1.gz ${DESTDIR}${PREFIX}/share/man/man1

.include <bsd.prog.mk>
