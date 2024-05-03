VERSION=0.10

SUBDIR= src

localport:
	mkdir vmstated-${VERSION}
	tar cvf - src rc doc/examples | tar -C vmstated-${VERSION} -xf -
	cp Makefile vmstated-${VERSION}/
	tar czvf christian-moerz-vmstated-${VERSION}_GH0.tar.gz vmstated-${VERSION}
	doas /bin/mv christian-moerz-vmstated-${VERSION}_GH0.tar.gz /usr/ports/distfiles
	/bin/rm -fr vmstated-${VERSION}

install:
	make -DDESTDIR=${DESTDIR} -DPREFIX=${PREFIX} -C doc/examples install
	make -DDESTDIR=${DESTDIR} -DPREFIX=${PREFIX} -C src install
	make -DDESTDIR=${DESTDIR} -DPREFIX=${PREFIX} -C rc install

.include <bsd.subdir.mk>
