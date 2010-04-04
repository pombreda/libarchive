# $FreeBSD$

PROG=	shar
SRCS=	shar.c tree.c

WARNS?=	6

DPADD=	${LIBTRANSFORM}
LDADD=	-ltransform

LINKS=	${BINDIR}/shar
MLINKS=	shar.1

.include <bsd.prog.mk>
