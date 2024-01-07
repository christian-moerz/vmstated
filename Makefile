VERSION=0.01

SUBDIR= src

localport:
	mkdir vmstated-${VERSION}
	tar cvf - src rc | tar -C vmstated-${VERSION} -xf -
	cp Makefile vmstated-${VERSION}/
	tar czvf vmstated-${VERSION}.tar.gz vmstated-${VERSION}
	doas /bin/mv vmstated-0.01.tar.gz /usr/ports/distfiles
	/bin/rm -fr vmstated-${VERSION}

install:
	make -DDESTDIR=${DESTDIR} -DPREFIX=${PREFIX} -C src install
	make -DDESTDIR=${DESTDIR} -DPREFIX=${PREFIX} -C rc install

.include <bsd.subdir.mk>
