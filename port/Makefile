PORTNAME=	vmstated
DISTVERSION=	0.10
CATEGORIES=	sysutils

MAINTAINER=	freebsd@ny-central.org
COMMENT=	Manages and runs bhyve virtual machines and infrastructure
WWW=		https://www.github.com/christian-moerz/vmstated

LICENSE=	BSD2CLAUSE

USE_GITHUB=	yes
GH_ACCOUNT=	christian-moerz
GH_PROJECT=	vmstated
GH_TAGNAME=	${DISTVERSION}
ONLY_FOR_ARCHS=	aarch64 amd64 armv7

post-patch:
	${REINPLACE_CMD} "s@PREFIX@${PREFIX}@g" "${WRKSRC}/rc/vmstated"

.include <bsd.port.mk>
