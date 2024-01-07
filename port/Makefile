PORTNAME=	vmstated
DISTVERSION=	0.01
CATEGORIES=	sysutils
MASTER_SITES=	https://github.com/christian-moerz/vmstated/releases/download/${DISTVERSION}/

MAINTAINER=	freebsd@ny-central.org
COMMENT=	Manages and runs bhyve virtual machines and infrastructure
WWW=		https://www.github.com/christian-moerz/vmstated

LICENSE=	BSD2CLAUSE

post-patch:
	${REINPLACE_CMD} "s@/usr/local@${PREFIX}@g" "${WRKSRC}/rc/vmstated"

.include <bsd.port.mk>
