PORTNAME=	vmstated
DISTVERSION=	0.02
CATEGORIES=	sysutils

MAINTAINER=	freebsd@ny-central.org
COMMENT=	Manages and runs bhyve virtual machines and infrastructure
WWW=		https://www.github.com/christian-moerz/vmstated

LICENSE=	BSD2CLAUSE

USE_GITHUB=	yes
GH_ACCOUNT=	christian-moerz
GH_PROJECT=	vmstated
GH_TAGNAME=	${DISTVERSION}

post-patch:
	${REINPLACE_CMD} "s@/usr/local@${PREFIX}@g" "${WRKSRC}/rc/vmstated"

.include <bsd.port.mk>
