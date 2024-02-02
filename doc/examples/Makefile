# $FreeBSD

install:
	/bin/mkdir -p ${DESTDIR}${PREFIX}/etc/vmstated/testvm
	/usr/bin/install -m 640 bhyve.conf ${DESTDIR}${PREFIX}/etc/vmstated/testvm/bhyve.conf.sample
	/usr/bin/install -m 640 config ${DESTDIR}${PREFIX}/etc/vmstated/testvm/config.sample
	/usr/bin/install -m 755 start_network ${DESTDIR}${PREFIX}/etc/vmstated/testvm/start_network.sample
	/usr/bin/install -m 755 stop_network ${DESTDIR}${PREFIX}/etc/vmstated/testvm/stop_network.sample
