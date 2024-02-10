# $FreeBSD

SUBDIR= libconfig liblogging libutils libsocket libstate libcommand \
	libprocwatch libtranslate vmstated vmstatedctl

tags:
	/bin/rm -f TAGS
	find . -name '*.c' -print -or -name '*.h' -print | xargs etags --append

install:
	make -C vmstated -DDESTDIR=${DESTDIR} -DPREFIX=${PREFIX} install
	make -C vmstatedctl -DDESTDIR=${DESTDIR} -DPREFIX=${PREFIX} install

.include <bsd.subdir.mk>
